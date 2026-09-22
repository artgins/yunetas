#!/usr/bin/env bash
#
#   install-probe-ban-escalation.sh
#
#   Make the bans of the yuneta-nginx-probe jail grow for an address that
#   comes back: 1 day, then 2, 4, and a week from then on.
#
#   Why. The scanners the jail catches return. Measured on a node in
#   September 2026: one address banned on the 4th and again on the 21st,
#   another on the 19th and again on the 21st. A fixed 1-day ban only makes
#   them wait a day. And the ban lands at the END of a scan anyway -- a whole
#   scan is 200-300 requests in two to five seconds -- so what a ban buys is
#   the time until the next one.
#
#   Why bantime.increment and not the stock recidive jail. recidive reads
#   fail2ban.log, and fail2ban.log rotates weekly: an address that returns
#   after the rotation looks like a first offence. Both returns above cross
#   one. bantime.increment counts the earlier bans in fail2ban's own
#   database, which does not rotate -- as long as the database keeps them,
#   and the stock dbpurgeage is ONE day. So this raises it to 30 days, and
#   without that the escalation would never see a second ban.
#
#   The cap is a week on purpose: the jail also catches us. A hardening check
#   run from the office by curl asks for /.env and /.git like a scanner does,
#   and an address of ours that fell in a few times would be locked out of
#   every site of the node for the longest ban. A week is a cost that can be
#   waited out; unban with
#       fail2ban-client set yuneta-nginx-probe unbanip <ip>
#
#   Only the probe jail escalates, and only on its own bans
#   (bantime.overalljails = false): an sshd ban does not lengthen a web one.
#
#   The yuneta-agent package does NOT run this: dbpurgeage is a setting of
#   the whole fail2ban server, the policy of whoever operates the node. Run
#   it on a node we operate, and on a node of another company only with its
#   operator's agreement.
#
#   A configuration fail2ban cannot load makes the WHOLE server exit, taking
#   every jail with it, so `fail2ban-client -t` runs before any reload and
#   the previous state comes back if the new files are the reason it fails.
#
#   Usage (as root, or through sudo):
#       install-probe-ban-escalation.sh            install
#       install-probe-ban-escalation.sh --check    show the settings, change nothing
#       install-probe-ban-escalation.sh --remove   back to the fixed 1-day ban
#
set -euo pipefail

JAIL_NAME=yuneta-nginx-probe
JAIL_FILE=/etc/fail2ban/jail.d/zz-yuneta-nginx-escalation.conf
SERVER_FILE=/etc/fail2ban/fail2ban.d/yuneta-ban-history.conf

#
#   Helpers
#
usage() {
    cat <<'EOF'
Usage (as root, or through sudo):
    install-probe-ban-escalation.sh            install
    install-probe-ban-escalation.sh --check    show the settings, change nothing
    install-probe-ban-escalation.sh --remove   back to the fixed 1-day ban
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

need_fail2ban() {
    if ! command -v fail2ban-client >/dev/null 2>&1; then
        die "fail2ban is not installed (dnf install fail2ban, or apt install fail2ban)"
    fi
}

need_jail() {
    if ! fail2ban-client status "$JAIL_NAME" >/dev/null 2>&1; then
        die "the jail $JAIL_NAME is not running: enable it first (see /etc/fail2ban/jail.d/yuneta-nginx.conf)"
    fi
}

reload_fail2ban() {
    if systemctl is-active --quiet fail2ban; then
        systemctl reload fail2ban
        echo "fail2ban reloaded."
    else
        echo "fail2ban is not running: the settings apply when it starts."
    fi
}

write_jail() {
    cat > "$1" <<'EOF'
#
#   Yuneta: the bans of yuneta-nginx-probe grow for an address that comes
#   back -- 1 day, 2, 4, then a week.
#
#   Written by tools/fail2ban/install-probe-ban-escalation.sh (yunetas), by
#   hand. It needs the ban history that
#   /etc/fail2ban/fail2ban.d/yuneta-ban-history.conf keeps.
#
[yuneta-nginx-probe]
bantime.increment    = true
bantime.maxtime      = 1w
bantime.overalljails = false
EOF
}

write_server() {
    cat > "$1" <<'EOF'
#
#   Yuneta: keep 30 days of bans in fail2ban's database.
#
#   The stock purge age is one day, and bantime.increment finds an earlier
#   ban of an address in the database: with one day of history it never
#   finds one, and no ban ever grows.
#
#   Written by tools/fail2ban/install-probe-ban-escalation.sh (yunetas), by
#   hand.
#
[DEFAULT]
dbpurgeage = 30d
EOF
}

show_settings() {
    echo "dbpurgeage:           $(fail2ban-client get dbpurgeage 2>/dev/null | tail -1 | sed 's/^[`| -]*//')"
    echo "bantime:              $(fail2ban-client get "$JAIL_NAME" bantime 2>/dev/null)"
    echo "bantime.increment:    $(fail2ban-client get "$JAIL_NAME" bantime.increment 2>/dev/null)"
    echo "bantime.maxtime:      $(fail2ban-client get "$JAIL_NAME" bantime.maxtime 2>/dev/null)"
    echo "bantime.overalljails: $(fail2ban-client get "$JAIL_NAME" bantime.overalljails 2>/dev/null)"
}

#
#   Put $2 at $1 atomically, keeping what was there in $3 (empty: nothing).
#   Written first to a name fail2ban never reads (neither *.conf nor
#   *.local), so it is never read half-written.
#
install_file() {
    local target="$1" writer="$2" tmp

    tmp="$(mktemp "$(dirname "$target")/.yuneta-escalation.XXXXXX")"
    "$writer" "$tmp"
    chmod 0644 "$tmp"
    if [ -f "$target" ] && cmp -s "$tmp" "$target"; then
        rm -f "$tmp"
        return 1
    fi
    mv -f "$tmp" "$target"
    return 0
}

#
#   Commands
#
cmd_install() {
    local out changed=0 prev_jail="" prev_server=""

    need_fail2ban
    need_jail
    if ! out="$(fail2ban-client -t 2>&1)"; then
        echo "$out" >&2
        die "fail2ban-client -t fails BEFORE any change: fix the node's fail2ban configuration first"
    fi

    if [ -f "$JAIL_FILE" ]; then
        prev_jail="$(mktemp /etc/fail2ban/jail.d/.yuneta-escalation-prev.XXXXXX)"
        cp -p "$JAIL_FILE" "$prev_jail"
    fi
    if [ -f "$SERVER_FILE" ]; then
        prev_server="$(mktemp /etc/fail2ban/fail2ban.d/.yuneta-escalation-prev.XXXXXX)"
        cp -p "$SERVER_FILE" "$prev_server"
    fi

    mkdir -p /etc/fail2ban/fail2ban.d
    if install_file "$SERVER_FILE" write_server; then
        changed=1
    fi
    if install_file "$JAIL_FILE" write_jail; then
        changed=1
    fi

    if [ "$changed" -eq 0 ]; then
        rm -f "$prev_jail" "$prev_server"
        echo "Already installed and up to date."
        show_settings
        return 0
    fi

    if ! out="$(fail2ban-client -t 2>&1)"; then
        if [ -n "$prev_jail" ]; then
            mv -f "$prev_jail" "$JAIL_FILE"
        else
            rm -f "$JAIL_FILE"
        fi
        if [ -n "$prev_server" ]; then
            mv -f "$prev_server" "$SERVER_FILE"
        else
            rm -f "$SERVER_FILE"
        fi
        echo "$out" >&2
        die "fail2ban rejects the new settings: previous state restored, fail2ban not reloaded"
    fi
    rm -f "$prev_jail" "$prev_server"

    echo "Installed $SERVER_FILE and $JAIL_FILE."
    reload_fail2ban
    show_settings
}

cmd_remove() {
    local out

    need_fail2ban
    if [ ! -f "$JAIL_FILE" ] && [ ! -f "$SERVER_FILE" ]; then
        echo "Not installed: nothing to remove."
        return 0
    fi
    rm -f "$JAIL_FILE" "$SERVER_FILE"
    if ! out="$(fail2ban-client -t 2>&1)"; then
        echo "$out" >&2
        die "fail2ban-client -t fails without the escalation files: the node's own fail2ban configuration is broken, not reloaded"
    fi
    echo "Removed $JAIL_FILE and $SERVER_FILE."
    reload_fail2ban
    show_settings
}

cmd_check() {
    need_fail2ban
    for f in "$SERVER_FILE" "$JAIL_FILE"; do
        if [ -f "$f" ]; then
            echo "$f is installed."
        else
            echo "$f is not installed."
        fi
    done
    show_settings
}

#
#   Main
#
case "${1:-}" in
    "")
        need_root "$@"
        cmd_install
        ;;
    --check)
        need_root "$@"
        cmd_check
        ;;
    --remove)
        need_root "$@"
        cmd_remove
        ;;
    -h|--help)
        usage
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac
