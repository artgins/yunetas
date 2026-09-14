#!/usr/bin/env bash
#
#   install-fail2ban-sshd-jail.sh
#
#   Make fail2ban watch sshd.
#
#   Debian enables fail2ban's sshd jail in its own jail.d/defaults-debian.conf.
#   RHEL/Rocky ship fail2ban with no jail enabled, so a Rocky node can have
#   fail2ban running and nothing watching sshd. When the distribution already
#   runs an sshd jail, this script says so and changes nothing.
#
#   backend = systemd, not the stock `auto`: sshd logs to the journal, and
#   Fedora's paths-fedora.conf says so itself (sshd_backend = systemd). It
#   needs python3-systemd, which fail2ban-server requires on EL9.
#
#   It does not stop a botnet -- thousands of addresses, a few tries each;
#   that is what install-sshd-flood-guard.sh is for. It stops the single
#   address that hammers.
#
#   The yuneta-agent package does NOT run this: banning addresses from sshd is
#   the policy of whoever operates the node. Run it on a node we operate, and
#   on a node of another company only with its operator's agreement.
#
#   A jail fail2ban cannot configure makes the WHOLE server exit, taking every
#   other jail with it, so `fail2ban-client -t` runs before any reload and the
#   previous state comes back if the new jail is the reason it fails.
#
#   Usage (as root, or through sudo):
#       install-fail2ban-sshd-jail.sh            install the jail
#       install-fail2ban-sshd-jail.sh --check    show the sshd jail, change nothing
#       install-fail2ban-sshd-jail.sh --remove   remove the jail
#
set -euo pipefail

JAIL=/etc/fail2ban/jail.d/yuneta-sshd.conf

#
#   Helpers
#
usage() {
    cat <<'EOF'
Usage (as root, or through sudo):
    install-fail2ban-sshd-jail.sh            install the jail
    install-fail2ban-sshd-jail.sh --check    show the sshd jail, change nothing
    install-fail2ban-sshd-jail.sh --remove   remove the jail
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

reload_fail2ban() {
    if systemctl is-active --quiet fail2ban; then
        systemctl reload fail2ban
        echo "fail2ban reloaded."
    else
        echo "fail2ban is not running: the jail applies when it starts (systemctl enable --now fail2ban)."
    fi
}

write_jail() {
    cat > "$1" <<'EOF'
#
#   Yuneta: watch sshd.
#
#   Written by tools/sshd/install-fail2ban-sshd-jail.sh (yunetas), by hand:
#   the yuneta-agent package does not touch sshd.
#
#   backend = systemd: sshd logs to the journal. It needs python3-systemd.
#
#   It does not stop a botnet -- thousands of addresses, a few tries each;
#   that is what /etc/ssh/sshd_config.d/10-yuneta-ssh-flood.conf is for. It
#   stops the single address that hammers.
#
[sshd]
enabled = true
backend = systemd
EOF
}

show_jail() {
    if fail2ban-client status sshd 2>/dev/null; then
        return 0
    fi
    echo "fail2ban does not watch sshd (or fail2ban is not running)."
}

#
#   Commands
#
cmd_install() {
    local tmp prev="" out

    need_fail2ban
    if [ ! -f "$JAIL" ] && fail2ban-client status sshd >/dev/null 2>&1; then
        echo "fail2ban already watches sshd with a jail of the distribution: nothing to do."
        show_jail
        return 0
    fi
    if ! out="$(fail2ban-client -t 2>&1)"; then
        echo "$out" >&2
        die "fail2ban-client -t fails BEFORE any change: fix the node's fail2ban configuration first"
    fi

    # Not *.conf nor *.local, so fail2ban never reads it half-written.
    tmp="$(mktemp /etc/fail2ban/jail.d/.yuneta-sshd.XXXXXX)"
    write_jail "$tmp"
    chmod 0644 "$tmp"

    if [ -f "$JAIL" ] && cmp -s "$tmp" "$JAIL"; then
        rm -f "$tmp"
        echo "$JAIL is already up to date."
        show_jail
        return 0
    fi

    if [ -f "$JAIL" ]; then
        prev="$(mktemp /etc/fail2ban/jail.d/.yuneta-sshd-prev.XXXXXX)"
        cp -p "$JAIL" "$prev"
    fi
    mv -f "$tmp" "$JAIL"

    if ! out="$(fail2ban-client -t 2>&1)"; then
        if [ -n "$prev" ]; then
            mv -f "$prev" "$JAIL"
        else
            rm -f "$JAIL"
        fi
        echo "$out" >&2
        die "fail2ban rejects the new $JAIL (python3-systemd missing?): previous state restored, fail2ban not reloaded"
    fi
    if [ -n "$prev" ]; then
        rm -f "$prev"
    fi

    echo "Installed $JAIL."
    reload_fail2ban
    show_jail
}

cmd_remove() {
    local out

    need_fail2ban
    if [ ! -f "$JAIL" ]; then
        echo "$JAIL is not installed: nothing to remove."
        return 0
    fi
    rm -f "$JAIL"
    if ! out="$(fail2ban-client -t 2>&1)"; then
        echo "$out" >&2
        die "fail2ban-client -t fails without $JAIL: the node's own fail2ban configuration is broken, not reloaded"
    fi
    echo "Removed $JAIL."
    reload_fail2ban
}

cmd_check() {
    need_fail2ban
    if [ -f "$JAIL" ]; then
        echo "$JAIL is installed."
    else
        echo "$JAIL is not installed."
    fi
    show_jail
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
