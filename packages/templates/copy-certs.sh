#!/usr/bin/env bash
#######################################################################
# copy-certs.sh -- copy the Let's Encrypt certificates of this node from
# /etc/letsencrypt/live/<name>/ into /yuneta/store/certs/<name>.{crt,chain}
# and /yuneta/store/certs/private/<name>.key
#
# Run as root: the certbot deploy hook runs it, and the agent's cert-sync
# runs it every 15 minutes (cert_sync_copy_cmd, `sudo -n`). Root only
# READS the certificates (privkey.pem is root's). Everything written in
# /yuneta/store/certs is written by the user yuneta (setpriv), which owns
# that tree: root never writes there.
#
# The names: the lines of /yuneta/store/certs/certs.list (blank lines and
# lines starting with # are skipped), or, without that file, every
# directory of /etc/letsencrypt/live. A name is [A-Za-z0-9._-], does not
# start with a dot and holds no "..": any other name is skipped with a
# line on stderr, and the others go on. Each pem must resolve to a file
# under /etc/letsencrypt (the live/ links point into archive/).
#
# An unchanged certificate is not written again: install -C keeps its
# mtime, and the agent's cert-sync reloads the yunos only when the size or
# the mtime of a file changes.
#
# A destination directory or file that is a symbolic link is refused, with
# a line on stderr.
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
#   Run a command as the owner of /yuneta
#
as_owner() {
    "$SETPRIV" --reuid="$OWNER" --regid="$OWNER" --init-groups -- \
        env HOME="$(getent passwd "$OWNER" | cut -d: -f6)" "$@"
}

valid_name() {
    case "$1" in
        ""|.*|*..*|*[!A-Za-z0-9._-]*)
            return 1
            ;;
    esac
    return 0
}

#
#   The real file of a pem, under /etc/letsencrypt, or fail
#
real_pem() {
    local real
    if ! real="$(readlink -f -- "$1")" || [ ! -f "$real" ]; then
        return 1
    fi
    case "$real" in
        "$(readlink -f -- "$LE_DIR")"/*)
            printf '%s' "$real"
            return 0
            ;;
    esac
    return 1
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
as_owner install -d -m 0755 "$DEST_BASE"
as_owner install -d -m 0700 "$DEST_PRIV"

#
#   The names: certs.list (read by the owner, whose file it is) or the
#   live directories
#
CERT_NAMES=()
if as_owner test -s "$CERTS_FILE"; then
    while IFS= read -r line; do
        line="${line%$'\r'}"
        case "$line" in
            ''|\#*)
                continue
                ;;
        esac
        CERT_NAMES+=("$line")
    done < <(as_owner cat "$CERTS_FILE")
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

#
#   Root copies each pem to a directory of its own (the owner can enter it,
#   not list it), owned by the owner, and the owner installs it in its tree
#
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
chmod 0711 "$STAGE"

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

    # install -C skips the copy (and the mtime bump) when the content, the
    # owner and the mode are the same. Its source must be a regular file:
    # it never skips for a link (letsencrypt's live/*.pem are links into
    # ../archive), so the stage holds real files of the owner.
    install -o "$OWNER" -g "$(id -gn "$OWNER")" -m 0600 "$FULLCHAIN_R" "${STAGE}/${CERT}.crt"
    install -o "$OWNER" -g "$(id -gn "$OWNER")" -m 0600 "$CHAIN_R" "${STAGE}/${CERT}.chain"
    install -o "$OWNER" -g "$(id -gn "$OWNER")" -m 0600 "$PRIVKEY_R" "${STAGE}/${CERT}.key"
    as_owner install -C -m 0644 "${STAGE}/${CERT}.crt" "${DEST_BASE}/${CERT}.crt"
    as_owner install -C -m 0644 "${STAGE}/${CERT}.chain" "${DEST_BASE}/${CERT}.chain"
    as_owner install -C -m 0600 "${STAGE}/${CERT}.key" "${DEST_PRIV}/${CERT}.key"
    rm -f "${STAGE}/${CERT}.crt" "${STAGE}/${CERT}.chain" "${STAGE}/${CERT}.key"
    echo "Copied ${CERT} -> ${DEST_BASE}/{${CERT}.crt, ${CERT}.chain, private/${CERT}.key}"
done

# A validity summary, as the owner
if [ -x "$VALIDITY" ]; then
    as_owner "$VALIDITY" "$DEST_BASE" || true
fi

exit 0
