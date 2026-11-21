#!/usr/bin/env bash
#######################################################################
# copy-certs.sh -- copy the Let's Encrypt certificates of this node from
# /etc/letsencrypt/live/<name>/ into /yuneta/store/certs/<name>.{crt,chain}
# and /yuneta/store/certs/private/<name>.key
#
# Run as root: the certbot deploy hook runs it, and the agent's cert-sync
# runs it every 15 minutes (cert_sync_copy_cmd, `sudo -n`). Root only
# READS the certificates (privkey.pem is root's): it opens each one and
# hands it on stdin to the user yuneta (setpriv --reset-env, in a session
# of its own: setsid), who compares it with its copy (cmp) and, only when
# it differs, writes it atomically in the destination directory (a temp
# file there, then mv). Root never writes in /yuneta/store/certs, no key is
# written anywhere else, and an unchanged certificate is not written at
# all: its mtime stays, and the agent's cert-sync reloads the yunos only
# when the size or the mtime of a file changes.
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
# symbolic link skips its certificate. Otherwise exit 0.
# "Copied <name>" is said only when something of <name> changed, and
# check-certs-validity.sh (as yuneta) runs only then.
#
# THE LIMIT: this script is in /yuneta, which yuneta owns, and root runs it
# (see the page "Files outside /yuneta", the root features). Removing sudo
# from yuneta does not make yuneta a privilege boundary while that is so.
#
# For tests only: run by a user that is not root, with
# YUNETA_COPY_CERTS_TEST_ROOT set, every path is under that directory, the
# owner is that user, and YUNETA_COPY_CERTS_TEST_SETPRIV names the setpriv
# to call. As root both are ignored.
#
# Copyright (c) 2024-2026, ArtGins.
#######################################################################
set -euo pipefail

if [ "$(id -u)" -eq 0 ]; then
    ROOT=""
    OWNER="yuneta"
    SETPRIV="setpriv"
elif [ -n "${YUNETA_COPY_CERTS_TEST_ROOT:-}" ]; then
    ROOT="$YUNETA_COPY_CERTS_TEST_ROOT"
    OWNER="$(id -un)"
    SETPRIV="${YUNETA_COPY_CERTS_TEST_SETPRIV:-setpriv}"
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
#   The real file of a pem, under /etc/letsencrypt, or fail
#
real_pem() {
    local real
    if ! real="$(readlink -f -- "$1")" || [ ! -f "$real" ]; then
        return 1
    fi
    case "$real" in
        "$LE_REAL"/*)
            printf '%s' "$real"
            return 0
            ;;
    esac
    return 1
}

#
#   Copy src (opened by root) to dest as the owner, with mode, only when
#   its content differs: 0 written, 1 unchanged, 2 failed
#
copy_file() {    # src dest mode
    local src="$1" dest="$2" mode="$3"
    if as_owner cmp -s - "$dest" < "$src"; then
        if [ "$(stat -c %a -- "$dest")" != "$mode" ]; then
            if ! as_owner chmod "$mode" "$dest"; then
                echo "FAILED to chmod $mode $dest" >&2
                return 2
            fi
        fi
        return 1
    fi
    if ! as_owner sh -c '
            umask 077
            t="$(mktemp "$1.XXXXXX")" || exit 1
            if cat > "$t" && chmod "$2" "$t" && mv -f "$t" "$1"; then
                exit 0
            fi
            rm -f "$t"
            exit 1
        ' copy-certs "$dest" "$mode" < "$src"; then
        echo "FAILED to write $dest" >&2
        return 2
    fi
    return 0
}

#
#   The destination directories: made by the owner, never a link
#
for d in "$DEST_BASE" "$DEST_PRIV"; do
    if [ -L "$d" ]; then
        echo "Refused: $d is a symbolic link: no certificate copied" >&2
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
        echo "FAILED: $OWNER cannot read $CERTS_FILE: no certificate copied" >&2
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
    echo "No certificates to copy (certs.list empty and no live certs found)." >&2
    exit 0
fi

CHANGED=0
FAILED=0
for CERT in "${CERT_NAMES[@]}"; do
    if ! valid_name "$CERT"; then
        echo "Skip '$(printf '%s' "$CERT" | tr -c '[:graph:]' '?')': not a valid name ([A-Za-z0-9._-], no leading dot, no ..)" >&2
        continue
    fi
    SRC_DIR="${LIVE_DIR}/${CERT}"
    if ! FULLCHAIN_R="$(real_pem "${SRC_DIR}/fullchain.pem")" ||
            ! CHAIN_R="$(real_pem "${SRC_DIR}/chain.pem")" ||
            ! PRIVKEY_R="$(real_pem "${SRC_DIR}/privkey.pem")"; then
        echo "Skip ${CERT}: missing files under ${SRC_DIR}, or a file that is not under ${LE_DIR}" >&2
        continue
    fi
    skip=0
    for f in "${DEST_BASE}/${CERT}.crt" "${DEST_BASE}/${CERT}.chain" "${DEST_PRIV}/${CERT}.key"; do
        if [ -L "$f" ]; then
            echo "Skip ${CERT}: $f is a symbolic link" >&2
            skip=1
        fi
    done
    if [ "$skip" -ne 0 ]; then
        continue
    fi

    cert_changed=0
    for spec in "${FULLCHAIN_R}|${DEST_BASE}/${CERT}.crt|644" \
                "${CHAIN_R}|${DEST_BASE}/${CERT}.chain|644" \
                "${PRIVKEY_R}|${DEST_PRIV}/${CERT}.key|600"; do
        IFS='|' read -r src dest mode <<< "$spec"
        rc=0
        copy_file "$src" "$dest" "$mode" || rc=$?
        if [ "$rc" -eq 0 ]; then
            cert_changed=1
        elif [ "$rc" -eq 2 ]; then
            FAILED=1
        fi
    done
    if [ "$cert_changed" -ne 0 ]; then
        CHANGED=1
        echo "Copied ${CERT} -> ${DEST_BASE}/{${CERT}.crt, ${CERT}.chain, private/${CERT}.key}"
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
