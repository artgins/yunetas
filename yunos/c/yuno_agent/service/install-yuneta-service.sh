#!/bin/sh
#######################################################################
# Install the service: the systemd units, or the SysV script elsewhere
#######################################################################
set -eu

if [ -d /run/systemd/system ] && command -v systemctl >/dev/null 2>&1; then
    systemctl daemon-reload || true
    systemctl enable yuneta_agent.service yuneta_agent22.service || true
    for _a in yuneta_agent yuneta_agent22; do
        if [ "$(systemctl show -p MainPID --value "${_a}.service")" = "0" ] && pgrep -x "${_a}" >/dev/null 2>&1; then
            su -s /bin/sh -c "/yuneta/agent/${_a} --config-file=/yuneta/agent/${_a}.json --stop" yuneta || true
            sleep 2
        fi
        systemctl restart "${_a}.service" || break
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
