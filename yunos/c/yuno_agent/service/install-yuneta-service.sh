#!/bin/sh
#######################################################################
# Install the service: the systemd units, or the SysV script elsewhere
#######################################################################
set -eu

# An /etc/init.d/yuneta_agent of an earlier agent is replaced by this one's,
# the old one kept beside it, not executable (up to 7.30.16 it was kept, and
# a node without systemd started the agents without the node uuid and the
# agent's pair). Compared with sha256sum (coreutils): a minimal RHEL has no
# cmp (diffutils)
if [ -e /etc/init.d/yuneta_agent ] \
        && [ "$(sha256sum < /yuneta/agent/service/yuneta_agent)" != "$(sha256sum < /etc/init.d/yuneta_agent)" ]; then
    _old="/etc/init.d/yuneta_agent.$(date +%Y%m%d%H%M%S).bak"
    install -m 0644 /etc/init.d/yuneta_agent "$_old"
    install -m 0755 /yuneta/agent/service/yuneta_agent /etc/init.d/yuneta_agent
    echo "/etc/init.d/yuneta_agent replaced by this agent's, the old one kept as $_old"
fi

# The units only where they are, as the SysV script decides: up to 7.30.16 a
# systemd without them stopped the agents and left them down
UNITS=0
if [ -d /run/systemd/system ] && command -v systemctl >/dev/null 2>&1; then
    for _d in /usr/lib/systemd/system /lib/systemd/system /etc/systemd/system; do
        if [ -f "$_d/yuneta_agent.service" ]; then
            UNITS=1
        fi
    done
fi

if [ "$UNITS" = "1" ]; then
    systemctl daemon-reload || true
    systemctl enable yuneta_agent.service yuneta_agent22.service || true
    for _a in yuneta_agent yuneta_agent22; do
        # An agent running outside its unit, found as its own --stop finds
        # it, by the base name of its argv[0]: its executable may be
        # replaced, renamed, behind a symlink, or unreadable for a root
        # without CAP_SYS_PTRACE
        _outside=0
        for _p in $(pgrep -x "$_a" 2>/dev/null); do
            _argv0="$(tr '\0' '\n' 2>/dev/null < "/proc/$_p/cmdline" | head -n 1)"
            if [ "${_argv0##*/}" = "${_a}" ]; then
                _outside=1
            fi
        done
        if [ "$(systemctl show -p MainPID --value "${_a}.service")" = "0" ] && [ "$_outside" = "1" ]; then
            su -s /bin/sh -c "/yuneta/agent/${_a} --config-file=/yuneta/agent/${_a}.json --stop" yuneta || true
            sleep 2
        fi
        if ! systemctl restart "${_a}.service"; then
            echo "ERROR: systemctl restart ${_a}.service failed: systemctl status ${_a}" >&2
            exit 1      # agent22 left as it is: never both down
        fi
    done
    exit 0
fi

if [ ! -x "/etc/init.d/yuneta_agent" ]; then
    install -m 0755 /yuneta/agent/service/yuneta_agent /etc/init.d/yuneta_agent
fi

# Enable SysV service and start it
if [ -x /usr/sbin/update-rc.d ]; then
    /usr/sbin/update-rc.d yuneta_agent defaults >/dev/null 2>&1 || true
fi
if command -v invoke-rc.d >/dev/null 2>&1; then
    invoke-rc.d yuneta_agent start || true
else
    /etc/init.d/yuneta_agent start || true
fi
exit 0
