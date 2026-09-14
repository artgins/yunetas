#!/usr/bin/env bash
#
#   stop-sshd.sh
#
#   Stop sshd for a while, and arm a timer that starts it again.
#
#   A closed port 22 is the one thing a connection flood cannot get round, and
#   on a node reachable only by SSH it is also the way to lose the node. So:
#
#   - the configuration is checked first (`sshd -t`): a timer that starts an
#     sshd that does not come up is no way back;
#   - the timer is armed BEFORE sshd is stopped, and if it cannot be armed
#     nothing is stopped.
#
#   Sessions already open survive: Debian's ssh.service and RHEL's
#   sshd.service stop only the listener (KillMode=process), not the sessions
#   it started. The script warns when the unit says otherwise.
#
#   While sshd is down, the node is reachable only through what is not SSH:
#   the provider's console, and the yuneta agent's control plane.
#
#   The timer is a transient systemd unit, yuneta-sshd-autostart.timer:
#   `systemctl list-timers yuneta-sshd-autostart.timer` says when it fires.
#   start-sshd.sh starts sshd before it does, and cancels it.
#
#   Usage (as root, or through sudo):
#       stop-sshd.sh                 stop now, start again in 60 minutes
#       stop-sshd.sh MINUTES         stop now, start again in MINUTES minutes
#       stop-sshd.sh --no-restart    stop now and arm nothing (only with console access)
#
set -euo pipefail

SSHD=/usr/sbin/sshd
TIMER_UNIT=yuneta-sshd-autostart
DEFAULT_MINUTES=60

#
#   Helpers
#
usage() {
    cat <<'EOF'
Usage (as root, or through sudo):
    stop-sshd.sh                 stop now, start again in 60 minutes
    stop-sshd.sh MINUTES         stop now, start again in MINUTES minutes
    stop-sshd.sh --no-restart    stop now and arm nothing (only with console access)
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

warn() {
    echo "WARNING: $*" >&2
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
    dir="$(sed -n 's/^Missing privilege separation directory: //p' <<<"$out")"
    if [ -n "$dir" ]; then
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
minutes="$DEFAULT_MINUTES"
restart=1
case "${1:-}" in
    "")
        ;;
    --no-restart)
        restart=0
        ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        if [[ "$1" =~ ^[1-9][0-9]*$ ]]; then
            minutes="$1"
        else
            usage >&2
            exit 2
        fi
        ;;
esac

need_root "$@"

#
#   Checks: nothing is stopped unless it can come back
#
if ! unit="$(sshd_unit)"; then
    die "no ssh or sshd systemd unit found"
fi
if ! sshd_check; then
    die "sshd -t fails: an sshd stopped now would not come back; fix the configuration first"
fi

units=("${unit}.service")
if systemctl is-active --quiet "${unit}.socket"; then
    units=("${unit}.socket" "${unit}.service")
fi

kill_mode="$(systemctl show -p KillMode --value "${unit}.service")"
if [ "$kill_mode" != "process" ]; then
    warn "${unit}.service has KillMode=${kill_mode}: stopping it may close the sessions already open, this one included"
fi

#
#   Arm the way back, then stop
#
if [ "$restart" -eq 1 ]; then
    systemctl stop "${TIMER_UNIT}.timer" >/dev/null 2>&1 || true
    systemctl reset-failed "${TIMER_UNIT}.timer" "${TIMER_UNIT}.service" >/dev/null 2>&1 || true
    if ! systemd-run --quiet --unit="$TIMER_UNIT" \
            --on-active="${minutes}min" --timer-property=AccuracySec=1s \
            "$(command -v systemctl)" start "${units[@]}"; then
        die "could not arm ${TIMER_UNIT}.timer: sshd NOT stopped"
    fi
fi

systemctl stop "${units[@]}"
echo "sshd stopped (${units[*]})."

if [ "$restart" -eq 1 ]; then
    echo "It starts again at $(date -d "+${minutes} min" '+%F %T %Z') (${TIMER_UNIT}.timer)."
    echo "To start it sooner: start-sshd.sh, from the provider's console."
else
    warn "nothing will start sshd again: start-sshd.sh, from the provider's console"
fi
