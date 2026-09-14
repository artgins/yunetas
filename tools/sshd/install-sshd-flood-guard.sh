#!/usr/bin/env bash
#
#   install-sshd-flood-guard.sh
#
#   Keep sshd reachable under an SSH connection flood.
#
#   A password botnet that cannot log in (password login off) still fills
#   sshd's slots for unauthenticated connections, and past 10 of them the stock
#   sshd drops new connections AT RANDOM: the operator's and the deploy tools'
#   included. On 2026-09-13 that was up to 1,464 drops an hour on one node, and
#   none once LoginGraceTime and MaxStartups were raised.
#
#   The yuneta-agent package does NOT run this. How sshd behaves is the policy
#   of whoever operates the node: run it on a node we operate, and on a node of
#   another company only with its operator's agreement.
#
#   It does not stop brute force -- password login off does, and this script
#   warns when it is on. It relieves the symptom; the fix for port 22 open to
#   the world is a source allowlist, and in the end a sealed node with no
#   inbound SSH (yunos/c/yuno_agent/NODE_SEALING.md).
#
#   What it does: writes /etc/ssh/sshd_config.d/10-yuneta-ssh-flood.conf,
#   validates with `sshd -t` (the previous state comes back if sshd rejects the
#   file), reloads -- never restarts -- sshd, and then reads `sshd -T` to
#   confirm the values are the ones sshd runs with.
#
#   Usage (as root, or through sudo):
#       install-sshd-flood-guard.sh            install or update the drop-in
#       install-sshd-flood-guard.sh --check    show what sshd runs with, change nothing
#       install-sshd-flood-guard.sh --remove   remove the drop-in
#
set -euo pipefail

CONF=/etc/ssh/sshd_config.d/10-yuneta-ssh-flood.conf
SSHD=/usr/sbin/sshd

#
#   Helpers
#
usage() {
    cat <<'EOF'
Usage (as root, or through sudo):
    install-sshd-flood-guard.sh            install or update the drop-in
    install-sshd-flood-guard.sh --check    show what sshd runs with, change nothing
    install-sshd-flood-guard.sh --remove   remove the drop-in
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

reload_sshd() {
    local unit
    if ! unit="$(sshd_unit)"; then
        warn "no ssh or sshd systemd unit found: sshd not reloaded"
        return 0
    fi
    if systemctl is-active --quiet "${unit}.service"; then
        systemctl reload "${unit}.service"
        echo "sshd reloaded (${unit}.service)."
    else
        echo "${unit}.service is not running: the values apply when it starts."
    fi
}

write_conf() {
    cat > "$1" <<'EOF'
#
#   Yuneta: keep sshd reachable under an SSH connection flood.
#
#   Written by tools/sshd/install-sshd-flood-guard.sh (yunetas), by hand: the
#   yuneta-agent package does not touch sshd. Password logins are not the
#   target -- they should already be off, and the script warns when they are
#   not. A botnet that cannot log in still fills sshd's MaxStartups slots with
#   connections that never authenticate, and past 10 of them the stock sshd
#   drops new connections AT RANDOM: the operator's and the deploy tools'
#   included. Measured 2026-09-13: up to 1,464 drops an hour on one node;
#   none with the first two lines below.
#
#   LoginGraceTime        a connection that has not authenticated in 20 s
#                         is closed (stock: 120 s), so its slot is freed
#                         sooner.
#   MaxStartups           start dropping at 50 unauthenticated connections,
#                         not 10, and refuse them all at 200.
#   PerSourceMaxStartups  at most 10 of them from one address (stock: no
#                         limit), so a single source cannot take the slots
#                         on its own. A botnet spreads over many addresses:
#                         this stops the few that hammer, not the flood.
#                         Needs OpenSSH 8.5 (Debian 12, Rocky 9 and later).
#
#   sshd keeps the FIRST value it reads, and sshd_config includes this
#   directory at its top, so the low number wins over later drop-ins and over
#   sshd_config itself. To change a value on a node, add a LOWER number (e.g.
#   05-local.conf): running the script again rewrites this file.
#
#   This relieves the symptom. The exposure is port 22 open to the world; the
#   fix for that is a source allowlist, and in the end a sealed node with no
#   inbound SSH (yunos/c/yuno_agent/NODE_SEALING.md).
#
LoginGraceTime 20
MaxStartups 50:30:200
PerSourceMaxStartups 10
EOF
}

#
#   Read `sshd -T`: what sshd will run with, whatever file said it
#
show_effective() {
    local effective key key_lc value have
    effective="$("$SSHD" -T 2>/dev/null)" || true
    if [ -z "$effective" ]; then
        warn "sshd -T printed nothing: the values sshd runs with could not be read"
        return 0
    fi

    echo "sshd runs with:"
    for key in logingracetime maxstartups persourcemaxstartups passwordauthentication kbdinteractiveauthentication; do
        have="$(printf '%s\n' "$effective" | awk -v k="$key" '$1 == k {print $2; exit}')"
        printf '    %-30s %s\n' "$key" "${have:-(unset)}"
    done

    if [ -f "$CONF" ]; then
        while read -r key value; do
            key_lc="$(printf '%s' "$key" | tr 'A-Z' 'a-z')"
            have="$(printf '%s\n' "$effective" | awk -v k="$key_lc" '$1 == k {print $2; exit}')"
            if [ "$have" != "$value" ]; then
                warn "sshd runs with $key ${have:-(unset)}, not the $value of $CONF: a drop-in read before it wins"
            fi
        done < <(grep -E '^[A-Za-z]' "$CONF")
    fi

    for key in passwordauthentication kbdinteractiveauthentication; do
        if printf '%s\n' "$effective" | grep -qx "$key yes"; then
            warn "sshd accepts password logins ($key yes): a botnet can try passwords. Turn it off once key login is confirmed to work"
        fi
    done
}

#
#   Commands
#
cmd_install() {
    local tmp prev=""

    if [ ! -d /etc/ssh/sshd_config.d ]; then
        die "/etc/ssh/sshd_config.d does not exist: this sshd reads no drop-ins"
    fi
    if ! grep -qiE '^[[:space:]]*Include[[:space:]]+/etc/ssh/sshd_config\.d/' /etc/ssh/sshd_config; then
        die "/etc/ssh/sshd_config does not Include /etc/ssh/sshd_config.d/: a drop-in there would be ignored"
    fi
    if ! sshd_check; then
        die "sshd -t fails BEFORE any change: fix the node's sshd configuration first"
    fi

    # Not *.conf, so sshd's Include never reads it half-written.
    tmp="$(mktemp /etc/ssh/sshd_config.d/.yuneta-ssh-flood.XXXXXX)"
    write_conf "$tmp"
    chmod 0600 "$tmp"

    if [ -f "$CONF" ] && cmp -s "$tmp" "$CONF"; then
        rm -f "$tmp"
        echo "$CONF is already up to date."
        show_effective
        return 0
    fi

    if [ -f "$CONF" ]; then
        prev="$(mktemp /etc/ssh/sshd_config.d/.yuneta-ssh-flood-prev.XXXXXX)"
        cp -p "$CONF" "$prev"
    fi
    mv -f "$tmp" "$CONF"

    if ! sshd_check; then
        if [ -n "$prev" ]; then
            mv -f "$prev" "$CONF"
        else
            rm -f "$CONF"
        fi
        die "sshd rejects the new $CONF (OpenSSH older than 8.5?): previous state restored, sshd not reloaded"
    fi
    if [ -n "$prev" ]; then
        rm -f "$prev"
    fi

    echo "Installed $CONF."
    reload_sshd
    show_effective
}

cmd_remove() {
    if [ ! -f "$CONF" ]; then
        echo "$CONF is not installed: nothing to remove."
        return 0
    fi
    rm -f "$CONF"
    if ! sshd_check; then
        die "sshd -t fails without $CONF: the node's own sshd configuration is broken, sshd not reloaded"
    fi
    echo "Removed $CONF."
    reload_sshd
    show_effective
}

cmd_check() {
    if ! sshd_check; then
        warn "sshd -t fails: sshd would not start with this configuration"
    fi
    if [ -f "$CONF" ]; then
        echo "$CONF is installed."
    else
        echo "$CONF is not installed."
    fi
    show_effective
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
