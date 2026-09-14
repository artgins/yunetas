#!/usr/bin/env bash
#
#   start-sshd.sh
#
#   Start sshd, and cancel the timer that stop-sshd.sh armed to start it.
#
#   sshd is down, so this runs from where SSH is not needed: the provider's
#   console. The configuration is checked first (`sshd -t`), and the errors
#   are shown: a node with a broken sshd configuration is fixed from that same
#   console, not found out about later.
#
#   Usage (as root, or through sudo):
#       start-sshd.sh
#
set -euo pipefail

SSHD=/usr/sbin/sshd
TIMER_UNIT=yuneta-sshd-autostart

#
#   Helpers
#
usage() {
    cat <<'EOF'
Usage (as root, or through sudo):
    start-sshd.sh
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

sshd_unit() {
    local u
    for u in ssh sshd; do
        if systemctl cat "${u}.service" >/dev/null 2>&1; then
            echo "$u"
            return 0
        fi
    done
    return 1
}

# Debian's privilege separation directory, /run/sshd, is created by
# ssh.service and removed when it stops, so with sshd down `sshd -t` fails
# for that alone. Create it as Debian's own init script does, then retry.
sshd_check() {
    local out dir
    if out="$("$SSHD" -t 2>&1)"; then
        return 0
    fi
    # sshd ends its stderr lines with \r\n: the \r is not part of the path.
    dir="$(sed -n 's/^Missing privilege separation directory: //p' <<<"$out" | tr -d '\r')"
    if [[ "$dir" =~ ^/[A-Za-z0-9._/-]+$ ]]; then
        install -d -m 0755 "$dir"
        if out="$("$SSHD" -t 2>&1)"; then
            return 0
        fi
    fi
    echo "$out" >&2
    return 1
}

#
#   Arguments
#
case "${1:-}" in
    "")
        ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

need_root "$@"

#
#   Start
#
if ! unit="$(sshd_unit)"; then
    die "no ssh or sshd systemd unit found"
fi
if ! sshd_check; then
    die "sshd -t fails: sshd would not start; fix the configuration first"
fi

units=("${unit}.service")
if systemctl is-enabled --quiet "${unit}.socket" 2>/dev/null; then
    units=("${unit}.socket" "${unit}.service")
fi

systemctl start "${units[@]}"
if ! systemctl is-active --quiet "${units[0]}"; then
    die "${units[0]} did not come up: journalctl -u ${units[0]}"
fi
echo "sshd running (${units[*]})."

if systemctl is-active --quiet "${TIMER_UNIT}.timer"; then
    systemctl stop "${TIMER_UNIT}.timer"
    echo "Cancelled ${TIMER_UNIT}.timer."
fi
