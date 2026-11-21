#!/usr/bin/env bash
#######################################################################
# copy-certs.sh -- copy the Let's Encrypt certificates of this node from
# /etc/letsencrypt/live/<name>/ into /yuneta/store/certs/<name>.{crt,chain}
# and /yuneta/store/certs/private/<name>.key
#
# Run as root: the certbot deploy hook runs it, and the agent's cert-sync
# runs it every 15 minutes (cert_sync_copy_cmd, `sudo -n`). Root only
# READS the certificates (privkey.pem is root's): it opens the three pems
# of a certificate and hands them, as open files, to the user yuneta
# (setpriv --reset-env, in a session of its own: setsid). yuneta compares
# each one with its copy (cmp), writes the ones that differ to temporary
# files beside them, and only when all are written moves them into place,
# the key first and the crt last: a failure leaves the old set whole, never
# an old crt with a new key. Root never writes in /yuneta/store/certs, no
# key is written anywhere else, and an unchanged certificate is not written
# at all: its mtime stays, and the agent's cert-sync reloads the yunos only
# when the size or the mtime of a file changes.
#
# What it says goes to its output and to the journal (logger -t
# copy-certs): the agent's cert-sync runs it with its output on /dev/null,
# so `journalctl -t copy-certs` is where a Skip or a FAILED of the tick is.
#
# The names: the lines of /yuneta/store/certs/certs.list (read by yuneta;
# blanks around a line are trimmed, a line may hold several names
# separated by blanks, blank lines and lines starting with # are skipped),
# or, without that file (or with an empty one), every directory of
# /etc/letsencrypt/live. A name is [A-Za-z0-9._-], does not start with a
# dot and holds no "..": any other name is skipped with a line on stderr,
# and the others go on. Each pem must resolve to a file under
# /etc/letsencrypt (the live/ links point into archive/).
#
# Exit 1, and nothing copied, when certs.list cannot be read by yuneta, or
# when /yuneta/store/certs or private/ is a symbolic link; exit 1 too when
# a copy failed (the others are done). A destination file that is a
# symbolic link skips its certificate. Otherwise exit 0. A certificate
# whose copy fails keeps its old files, whole.
# "Copied <name>" is said only when something of <name> changed, and
# check-certs-validity.sh (as yuneta) runs only then.
#
# THE LIMIT: this script is in /yuneta, which yuneta owns, and root runs it
# (see the page "Files outside /yuneta", the root features). Removing sudo
# from yuneta does not make yuneta a privilege boundary while that is so.
#
# For tests only: run by a user that is not root, with
# YUNETA_COPY_CERTS_TEST_ROOT set, every path is under that directory, the
# owner is that user, YUNETA_COPY_CERTS_TEST_SETPRIV names the setpriv to
# call, and YUNETA_COPY_CERTS_TEST_LOGGER the logger (none by default). As
# root they are ignored.
#
# Copyright (c) 2024-2026, ArtGins.
#######################################################################
set -euo pipefail

if [ "$(id -u)" -eq 0 ]; then
    ROOT=""
    OWNER="yuneta"
    SETPRIV="setpriv"
    LOGGER="logger"
elif [ -n "${YUNETA_COPY_CERTS_TEST_ROOT:-}" ]; then
    ROOT="$YUNETA_COPY_CERTS_TEST_ROOT"
    OWNER="$(id -un)"
    SETPRIV="${YUNETA_COPY_CERTS_TEST_SETPRIV:-setpriv}"
    LOGGER="${YUNETA_COPY_CERTS_TEST_LOGGER:-true}"
else
    echo "This script must run as root (to read /etc/letsencrypt/live/*/privkey.pem)" >&2
    exit 1
fi

LE_DIR="${ROOT}/etc/letsencrypt"
LIVE_DIR="${LE_DIR}/live"
DEST_BASE="${ROOT}/yuneta/store/certs"
DEST_PRIV="${DEST_BASE}/private"
CERTS_FILE="${DEST_BASE}/certs.list"
VALIDITY="${ROOT}/yuneta/bin/check-certs-validity.sh"

#
#   What the script says goes to its output AND to the journal (logger -t
#   copy-certs): from the agent's cert-sync its output is /dev/null, and
#   from the hook it is /var/log/yuneta/copy-certs.log
#
say() {    # priority message
    if [ "$1" = "info" ]; then
        echo "$2"
    else
        echo "$2" >&2
    fi
    "$LOGGER" -t copy-certs -p "user.$1" -- "$2" 2>/dev/null || true
}

#
#   Run a command as the owner of /yuneta: its own environment (HOME, USER,
#   PATH of its passwd entry), and no controlling terminal of root's
#
as_owner() {
    setsid -w "$SETPRIV" --reuid="$OWNER" --regid="$OWNER" --init-groups --reset-env -- "$@"
}

valid_name() {
    case "$1" in
        ""|.*|*..*|*[!A-Za-z0-9._-]*)
            return 1
            ;;
    esac
    return 0
}

LE_REAL="$(readlink -f -- "$LE_DIR")"

#
#   The part of the owner, for one certificate: its three pems come opened
#   by root, twice each (fds 3-5 to compare, 6-8 to copy: a reopen of
#   /dev/fd/N would check the permissions again). Each file that differs is
#   written to a temporary file in its directory; only when all of them are
#   written are they moved into place, the key first and the crt last: the
#   crt is what the agent's cert-sync watches, so when its change is seen
#   the key is already the new one, and a failure leaves the old set whole
#   instead of an old crt with a new key. An unchanged file is not written
#   (its mtime stays); its mode is set again when it moved.
#   Exit 10: something written; 11: nothing to write; anything else failed.
#
OWNER_PART='
set -u
dests=("$1" "$2" "$3")
modes=("$4" "$5" "$6")
cfds=(3 4 5)
wfds=(6 7 8)
tmps=("" "" "")
same=()
drop() {
    local t
    for t in "${tmps[@]}"; do
        if [ -n "$t" ]; then
            rm -f -- "$t"
        fi
    done
}
for i in 0 1 2; do
    if cmp -s - "${dests[i]}" <&"${cfds[i]}"; then
        same+=("$i")
        continue
    fi
    if ! t="$(mktemp "${dests[i]}.XXXXXX")"; then
        echo "FAILED to make a temporary file beside ${dests[i]}" >&2
        drop
        exit 2
    fi
    tmps[i]="$t"
    if ! cat <&"${wfds[i]}" > "$t" || ! chmod "${modes[i]}" "$t"; then
        echo "FAILED to write ${dests[i]}: nothing of this certificate changed" >&2
        drop
        exit 2
    fi
done
if [ "${#same[@]}" -gt 0 ]; then
    paths=()
    for i in "${same[@]}"; do
        paths+=("${dests[i]}")
    done
    mapfile -t now < <(stat -c %a -- "${paths[@]}")
    k=0
    for i in "${same[@]}"; do
        if [ "${now[k]:-}" != "${modes[i]}" ] && ! chmod "${modes[i]}" "${dests[i]}"; then
            echo "FAILED to chmod ${modes[i]} ${dests[i]}" >&2
            drop
            exit 2
        fi
        k=$((k + 1))
    done
fi
written=0
for i in 2 1 0; do
    if [ -n "${tmps[i]}" ]; then
        if ! mv -f -- "${tmps[i]}" "${dests[i]}"; then
            echo "FAILED to move ${tmps[i]} to ${dests[i]}" >&2
            tmps[i]=""
            drop
            exit 2
        fi
        tmps[i]=""
        written=1
    fi
done
if [ "$written" -ne 0 ]; then
    exit 10
fi
exit 11
'

#
#   The destination directories: made by the owner, never a link
#
for d in "$DEST_BASE" "$DEST_PRIV"; do
    if [ -L "$d" ]; then
        say err "Refused: $d is a symbolic link: no certificate copied"
        exit 1
    fi
done
if [ ! -d "$DEST_BASE" ]; then
    as_owner install -d -m 0755 "$DEST_BASE"
fi
if [ ! -d "$DEST_PRIV" ]; then
    as_owner install -d -m 0700 "$DEST_PRIV"
fi

#
#   The names: certs.list (read by the owner, whose file it is) or the
#   live directories
#
CERT_NAMES=()
if [ -s "$CERTS_FILE" ]; then
    if ! list="$(as_owner cat "$CERTS_FILE")"; then
        say err "FAILED: $OWNER cannot read $CERTS_FILE: no certificate copied"
        exit 1
    fi
    while IFS= read -r line; do
        line="${line%$'\r'}"
        read -r -a words <<< "$line"
        if [ "${#words[@]}" -eq 0 ] || [ "${words[0]:0:1}" = "#" ]; then
            continue
        fi
        CERT_NAMES+=("${words[@]}")
    done <<< "$list"
elif [ -d "$LIVE_DIR" ]; then
    for d in "$LIVE_DIR"/*; do
        if [ -d "$d" ]; then
            CERT_NAMES+=("$(basename "$d")")
        fi
    done
fi

if [ "${#CERT_NAMES[@]}" -eq 0 ]; then
    say notice "No certificates to copy (certs.list empty and no live certs found)."
    exit 0
fi

CHANGED=0
FAILED=0
for CERT in "${CERT_NAMES[@]}"; do
    if ! valid_name "$CERT"; then
        say warning "Skip '$(printf '%s' "$CERT" | tr -c '[:graph:]' '?')': not a valid name ([A-Za-z0-9._-], no leading dot, no ..)"
        continue
    fi
    SRC_DIR="${LIVE_DIR}/${CERT}"
    # one readlink for the three: a pem that is missing gives no line
    mapfile -t REAL < <(readlink -e -- "${SRC_DIR}/fullchain.pem" "${SRC_DIR}/chain.pem" "${SRC_DIR}/privkey.pem" 2>/dev/null)
    ok=0
    if [ "${#REAL[@]}" -eq 3 ]; then
        ok=1
        for r in "${REAL[@]}"; do
            case "$r" in
                "$LE_REAL"/*)
                    ;;
                *)
                    ok=0
                    ;;
            esac
            if [ ! -f "$r" ]; then
                ok=0
            fi
        done
    fi
    if [ "$ok" -ne 1 ]; then
        say warning "Skip ${CERT}: missing files under ${SRC_DIR}, or a file that is not under ${LE_DIR}"
        continue
    fi
    D_CRT="${DEST_BASE}/${CERT}.crt"
    D_CHAIN="${DEST_BASE}/${CERT}.chain"
    D_KEY="${DEST_PRIV}/${CERT}.key"
    skip=0
    for f in "$D_CRT" "$D_CHAIN" "$D_KEY"; do
        if [ -L "$f" ]; then
            say warning "Skip ${CERT}: $f is a symbolic link"
            skip=1
        fi
    done
    if [ "$skip" -ne 0 ]; then
        continue
    fi

    rc=0
    out="$(as_owner bash -c "$OWNER_PART" copy-certs "$D_CRT" "$D_CHAIN" "$D_KEY" 644 644 600 \
        3<"${REAL[0]}" 4<"${REAL[1]}" 5<"${REAL[2]}" \
        6<"${REAL[0]}" 7<"${REAL[1]}" 8<"${REAL[2]}" 2>&1)" || rc=$?
    if [ "$rc" -eq 10 ]; then
        CHANGED=1
        say info "Copied ${CERT} -> ${DEST_BASE}/{${CERT}.crt, ${CERT}.chain, private/${CERT}.key}"
    elif [ "$rc" -ne 11 ]; then
        FAILED=1
        say err "FAILED to copy ${CERT} (rc=$rc): ${out:-no message}"
    fi
done

# A validity summary, as the owner, only when something changed
if [ "$CHANGED" -ne 0 ] && [ -x "$VALIDITY" ]; then
    as_owner "$VALIDITY" "$DEST_BASE" || true
fi

if [ "$FAILED" -ne 0 ]; then
    exit 1
fi
exit 0
