#!/bin/sh
# Install SysV service and start it — no systemd calls, so no polkit prompts.
set -eu

# Require root (you said you always sudo)
if [ "$(id -u)" -ne 0 ]; then
    echo "Run as root: sudo /yuneta/agent/service/install-yuneta-service.sh" >&2
    exit 1
fi

# Ensure init script in place
if [ ! -x /etc/init.d/yuneta_agent ]; then
    /bin/install -m 0755 /yuneta/agent/service/yuneta_agent /etc/init.d/yuneta_agent
fi

# Create rc?.d links with predictable priorities (S98/K02)
if [ -x /usr/sbin/update-rc.d ]; then
    /usr/sbin/update-rc.d yuneta_agent defaults 98 02 >/dev/null 2>&1 || true
fi

# Start via SysV
if [ -x /usr/sbin/invoke-rc.d ]; then
    /usr/sbin/invoke-rc.d yuneta_agent start || true
else
    /etc/init.d/yuneta_agent start || true
fi

exit 0
