#!/usr/bin/env bash
#
#   make-fail2ban-log-readable.sh
#
#   Let the yuneta user read /var/log/fail2ban.log, the way Debian already
#   does: root:adm 0640, with yuneta in the adm group.
#
#   Why. webstats reads fail2ban.log to say, next to each top offender of
#   its daily mail, whether the jail banned it -- a jail that bans nobody
#   looks exactly as healthy as one that works, and this is where it shows.
#   Debian ships the log root:adm 0640 and yuneta is in adm, so there is
#   nothing to do. RHEL/Rocky ship it root:root 0600 and rotate it without a
#   `create` line, so fail2ban recreates it 0600 after every rotation.
#
#   What it changes, only where needed:
#     - the live log and its uncompressed rotations: group adm, mode 0640;
#     - /etc/logrotate.d/fail2ban: `create 0640 root adm`, so a rotation
#       keeps it that way;
#     - the yuneta user: added to adm.
#
#   A process keeps the groups it started with, so after adding yuneta to adm
#   restart yuneta_agent (it does not stop the yunos) and then the webstats
#   yuno, or the yuno still cannot read the file.
#
#   The yuneta-agent package does NOT run this: who reads the system logs is
#   the policy of whoever operates the node.
#
#   Usage (as root, or through sudo):
#       make-fail2ban-log-readable.sh            apply
#       make-fail2ban-log-readable.sh --check    show the state, change nothing
#
set -euo pipefail

LOG=/var/log/fail2ban.log
LOGROTATE=/etc/logrotate.d/fail2ban
GROUP=adm
USER_NAME=yuneta

#
#   Helpers
#
usage() {
    cat <<'EOF'
Usage (as root, or through sudo):
    make-fail2ban-log-readable.sh            apply
    make-fail2ban-log-readable.sh --check    show the state, change nothing
EOF
}

need_root() {
    if [ "${EUID:-$(id -u)}" -ne 0 ]; then
        if command -v sudo >/dev/null 2>&1; then
            exec sudo -E bash "$0" "$@"
        fi
        echo "This script must run as root (or install sudo)." >&2
        exit 1
    fi
}

die() {
    echo "ERROR: $*" >&2
    exit 1
}

show_state() {
    ls -la "$LOG"* 2>/dev/null || echo "$LOG does not exist."
    echo "user $USER_NAME: $(id "$USER_NAME" 2>/dev/null || echo 'does not exist')"
    if [ -f "$LOGROTATE" ]; then
        echo "$LOGROTATE create line: $(grep -E '^\s*create' "$LOGROTATE" || echo 'none')"
    else
        echo "$LOGROTATE does not exist."
    fi
    if su -s /bin/sh -c "head -c 0 $LOG" "$USER_NAME" 2>/dev/null; then
        echo "A fresh login of $USER_NAME CAN read $LOG."
    else
        echo "A fresh login of $USER_NAME can NOT read $LOG."
    fi
}

#
#   Commands
#
cmd_apply() {
    local f

    [ -f "$LOG" ] || die "$LOG does not exist: is fail2ban running with logtarget = $LOG?"
    getent group "$GROUP" >/dev/null || die "the group $GROUP does not exist"
    id "$USER_NAME" >/dev/null 2>&1 || die "the user $USER_NAME does not exist"

    for f in "$LOG" "$LOG".[0-9] "$LOG"-[0-9]*; do
        if [ -f "$f" ] && [[ "$f" != *.gz ]]; then
            chgrp "$GROUP" "$f"
            chmod 0640 "$f"
        fi
    done
    echo "$LOG and its uncompressed rotations: group $GROUP, mode 0640."

    if [ -f "$LOGROTATE" ]; then
        if grep -qE '^\s*create' "$LOGROTATE"; then
            echo "$LOGROTATE already has a create line, left as it is:"
            grep -E '^\s*create' "$LOGROTATE"
        else
            cp -p "$LOGROTATE" "$LOGROTATE.yuneta-prev"
            sed -i "0,/{/s//{\n    create 0640 root $GROUP/" "$LOGROTATE"
            if ! logrotate -d "$LOGROTATE" >/dev/null 2>&1; then
                mv -f "$LOGROTATE.yuneta-prev" "$LOGROTATE"
                die "logrotate rejects the edited $LOGROTATE: previous file restored"
            fi
            rm -f "$LOGROTATE.yuneta-prev"
            echo "$LOGROTATE: added 'create 0640 root $GROUP'."
        fi
    else
        echo "WARNING: $LOGROTATE does not exist; a rotation may recreate $LOG unreadable." >&2
    fi

    if id -nG "$USER_NAME" | tr ' ' '\n' | grep -qx "$GROUP"; then
        echo "$USER_NAME is already in $GROUP."
    else
        usermod -aG "$GROUP" "$USER_NAME"
        echo "$USER_NAME added to $GROUP. Restart yuneta_agent and then the webstats yuno:"
        echo "processes keep the groups they started with."
    fi

    show_state
}

#
#   Main
#
case "${1:-}" in
    "")
        need_root "$@"
        cmd_apply
        ;;
    --check)
        need_root "$@"
        show_state
        ;;
    -h|--help)
        usage
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac
