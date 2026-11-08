#!/bin/sh
#######################################################################
# Disable the service: the systemd units (both agents: the package goes),
# and the SysV runlevel symlinks (leave the conffile in place)
#######################################################################
set -eu
if [ -d /run/systemd/system ] && command -v systemctl >/dev/null 2>&1; then
    systemctl disable --now yuneta_agent.service yuneta_agent22.service >/dev/null 2>&1 || true
fi
if command -v invoke-rc.d >/dev/null 2>&1; then
    invoke-rc.d yuneta_agent stop || true
else
    /etc/init.d/yuneta_agent stop || true
fi
if command -v systemctl >/dev/null 2>&1; then
    systemctl disable --now yuneta-core-pattern.service >/dev/null 2>&1 || true
    systemctl disable --now yuneta-webserver.service >/dev/null 2>&1 || true
fi
if [ -x /usr/sbin/update-rc.d ]; then
    /usr/sbin/update-rc.d -f yuneta_agent remove >/dev/null 2>&1 || true
fi
exit 0
