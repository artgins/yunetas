#!/usr/bin/env bash
#######################################################################
#               make-yuneta-agent-deb.sh
#######################################################################
# Build a .deb for Yuneta’s Agent with SysV integration + cert hooks
#
# Usage:
#   ./make-yuneta-agent-deb.sh <project> <version> <release> <arch>
#
# Layout (relative to this script, which must live in yunetas/packages/):
#   - builds under: ./build/deb/<arch>/<project-version-release-arch>/
#   - outputs to:   ./dist/<project-version-release-arch>.deb
#
# Major features:
#   - Ships Yuneta agent binaries + runtime tree under /yuneta
#   - SysV init script at /etc/init.d/yuneta_agent
#   - Certbot deploy hook to copy-renewed certs + reload nginx + restart Yuneta
#   - Helpers for installing dev deps and certbot (snap)
#   - Creates user 'yuneta' with login at /home/yuneta (if needed)
#   - Makes /var/log/syslog available via rsyslog
#
# Notes:
#   - /etc/init.d/yuneta_agent is NOT a conffile, so it’s removed on uninstall.
#   - Agent JSON configs are created on first install only (preserve if exist).
#######################################################################

set -euo pipefail
umask 022

# Resolve the script's real directory (yunetas/packages) to anchor relative paths.
# HACK: Works regardless of launch location (cron, make, symlink, different CWD).
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd -P)"

if [ "${1:-}" = "--help" ] || [ "$#" -lt 4 ]; then
    echo "Usage: $0 <project> <version> <release> <arch>"
    exit 1
fi

PROJECT="$1"; shift
VERSION="$1"; shift
RELEASE="$1"; shift
ARCHITECTURE="$1"; shift

PACKAGE="${PROJECT}-${VERSION}-${RELEASE}-${ARCHITECTURE}"
BASE_DEST="${SCRIPT_DIR}/build/deb/${ARCHITECTURE}"
WORKDIR="${BASE_DEST}/${PACKAGE}"
OUT_DIR="${SCRIPT_DIR}/dist"
BIN_DIR="/yuneta/bin/"

echo "[i] Building package tree at: ${WORKDIR}"

# Fresh workspace
rm -rf "${WORKDIR}"
mkdir -p "${WORKDIR}/DEBIAN"
mkdir -p "${WORKDIR}/etc/profile.d"
mkdir -p "${WORKDIR}/etc/sudoers.d"
mkdir -p "${WORKDIR}/etc/init.d"                          # ship the init script
mkdir -p "${WORKDIR}/etc/letsencrypt/renewal-hooks/deploy" # certbot deploy hook

# Yuneta tree (runtime + dev convenience dirs)
mkdir -p "${WORKDIR}/yuneta/agent/service"
mkdir -p "${WORKDIR}/yuneta/agent"
mkdir -p "${WORKDIR}/yuneta/bin"
mkdir -p "${WORKDIR}/yuneta/gui"
mkdir -p "${WORKDIR}/yuneta/realms"
mkdir -p "${WORKDIR}/yuneta/repos"
mkdir -p "${WORKDIR}/yuneta/share"
mkdir -p "${WORKDIR}/yuneta/store/queues/gate_msgs2"
mkdir -p "${WORKDIR}/yuneta/store/certs/private"          # private will be 0700 in postinst

# Dev-oriented dirs (optional, handy when building yunos locally)
mkdir -p "${WORKDIR}/yuneta/development/projects"
mkdir -p "${WORKDIR}/yuneta/development/outputs"
mkdir -p "${WORKDIR}/yuneta/development/outputs/include"
mkdir -p "${WORKDIR}/yuneta/development/outputs/libs"
mkdir -p "${WORKDIR}/yuneta/development/outputs/yunos"
mkdir -p "${WORKDIR}/yuneta/development/outputs_ext"

# --- Single-file utilities to include (must exist in BIN_DIR) ---
BINARIES=(
    fs_watcher
    inotify
    keycloak_pkey_to_jwks
    list_queue_msgs2
    tr2keys
    tr2list
    tr2migrate
    watchfs
    ybatch
    ycli
    yclone-gclass
    yclone-project
    ycommand
    ylist
    ymake-skeleton
    yscapec
    yshutdown
    ystats
    ytestconfig
    ytests
    yuno-skeleton
)

# Copy single-file utilities with correct perms; verify existence
for BINARY in "${BINARIES[@]}"; do
    SRC="${BIN_DIR%/}/${BINARY}"
    if [ ! -x "${SRC}" ]; then
        echo "[-] Missing or non-executable binary: ${SRC}" >&2
        exit 2
    fi
    install -D -m 0755 "${SRC}" "${WORKDIR}/yuneta/bin/${BINARY}"
done

# --- Copy bundled trees (if present), dereferencing symlinks ---
copy_tree() {
    local src="$1" dst_base="$2"
    if [ -d "$src" ]; then
        echo "[i] Copying $(basename "$src") -> ${dst_base}"
        cp -a --dereference "$src" "$dst_base"/
    else
        echo "[!] Skipping missing dir: $src"
    fi
}

copy_tree "/yuneta/bin/ncurses"   "${WORKDIR}/yuneta/bin"
copy_tree "/yuneta/bin/nginx"     "${WORKDIR}/yuneta/bin"
copy_tree "/yuneta/bin/openresty" "${WORKDIR}/yuneta/bin"
copy_tree "/yuneta/ssl3"          "${WORKDIR}/yuneta/bin"
copy_tree "/yuneta/share"         "${WORKDIR}/yuneta/share"

# Optional dev artifacts snapshot
copy_tree "/yuneta/development/outputs_ext"     "${WORKDIR}/yuneta/development/outputs_ext"
copy_tree "/yuneta/development/outputs/include" "${WORKDIR}/yuneta/development/outputs/include"
copy_tree "/yuneta/development/outputs/libs"    "${WORKDIR}/yuneta/development/outputs/libs"
copy_tree "/yuneta/development/outputs/yunos"   "${WORKDIR}/yuneta/development/outputs/yunos"

# --- Copy yuneta_agent binaries (required) and create default configs (samples) ---
AGENT_SRC_1="/yuneta/agent/yuneta_agent"
AGENT_SRC_2="/yuneta/agent/yuneta_agent22"

if [ ! -x "${AGENT_SRC_1}" ]; then
    echo "[-] Missing or non-executable: ${AGENT_SRC_1}" >&2
    exit 2
fi
if [ ! -x "${AGENT_SRC_2}" ]; then
    echo "[-] Missing or non-executable: ${AGENT_SRC_2}" >&2
    exit 2
fi

install -D -m 0755 "${AGENT_SRC_1}" "${WORKDIR}/yuneta/agent/yuneta_agent"
install -D -m 0755 "${AGENT_SRC_2}" "${WORKDIR}/yuneta/agent/yuneta_agent22"

# Create default JSON samples (real files created in postinst ONLY IF MISSING)
printf '{}\n' > "${WORKDIR}/yuneta/agent/yuneta_agent.json.sample"
printf '{}\n' > "${WORKDIR}/yuneta/agent/yuneta_agent22.json.sample"
chmod 0644 "${WORKDIR}/yuneta/agent/yuneta_agent.json.sample" \
           "${WORKDIR}/yuneta/agent/yuneta_agent22.json.sample"

# --- Copy agent certs directory (if present) ---
copy_tree "/yuneta/agent/certs" "${WORKDIR}/yuneta/agent"

# --- Certs validator (installed into /yuneta/bin; scans current dir by default) ---
cat > "${WORKDIR}/yuneta/bin/check-certs-validity.sh" <<'EOF'
#!/usr/bin/env bash
#
# Recursively list *.crt and show subject, validity dates, and expiry status.
#
# Usage:
#   check-certs-validity.sh                # scans . (current directory)
#   check-certs-validity.sh <path> [...]   # each path can be a dir or a file
#
set -euo pipefail

OPENSSL="$(command -v openssl || true)"
if [ -z "${OPENSSL}" ]; then
    echo "openssl not found in PATH" >&2
    exit 127
fi

# If no args, scan current dir; else scan provided paths
if [ "$#" -eq 0 ]; then
    set -- "."
fi

process_file() {
    file="$1"
    echo "==============> ${file}"
    if ! "${OPENSSL}" x509 -noout -in "${file}" -subject -dates 2>/dev/null; then
        echo "not a valid X.509 certificate" >&2
        return 0
    fi
    if "${OPENSSL}" x509 -in "${file}" -noout -checkend 0 >/dev/null 2>&1; then
        echo "status: valid (not expired)"
    else
        echo "status: EXPIRED"
    fi
}

for path in "$@"; do
    if [ -d "${path}" ]; then
        # Scan directory recursively for *.crt (robust to spaces/newlines)
        find "${path}" -type f -name '*.crt' -print0 | \
        while IFS= read -r -d '' crt; do
            process_file "${crt}"
        done
    elif [ -f "${path}" ]; then
        process_file "${path}"
    else
        echo "skip: path not found: ${path}" >&2
    fi
done
EOF
chmod 0755 "${WORKDIR}/yuneta/bin/check-certs-validity.sh"

# --- Profile snippet (system-wide shell PATH for Yuneta) ---
cat > "${WORKDIR}/etc/profile.d/yuneta.sh" <<'EOF'
# Yuneta environment
export YUNETA_DIR=/yuneta
export PATH="/yuneta/bin:$PATH"
EOF
chmod 0644 "${WORKDIR}/etc/profile.d/yuneta.sh"

# --- SysV init script (minimal; Yuneta manages its own daemons) ---
cat > "${WORKDIR}/etc/init.d/yuneta_agent" <<'EOF'
#!/bin/sh
### BEGIN INIT INFO
# Provides:          yuneta_agent
# Required-Start:    $local_fs $remote_fs $network $time $syslog $named
# Required-Stop:     $local_fs $remote_fs $network $time $syslog $named
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Yuneta Agent service
# Description:       Start/stop Yuneta Agent yunos and default web server (nginx)
### END INIT INFO

# Keep it simple: Yuneta handles its own daemon/watchdog. No start-stop-daemon here.
set -eu

YUNETA_DIR="/yuneta"
RUN_AS="yuneta"

AGENT1_BIN="${YUNETA_DIR}/agent/yuneta_agent"
AGENT1_CFG="${YUNETA_DIR}/agent/yuneta_agent.json"

AGENT2_BIN="${YUNETA_DIR}/agent/yuneta_agent22"
AGENT2_CFG="${YUNETA_DIR}/agent/yuneta_agent22.json"

# Default web server: Nginx
NGINX_BIN="${YUNETA_DIR}/bin/nginx/sbin/nginx"

# OpenResty is shipped but DISABLED by default; uncomment to enable:
# OPENRESTY_BIN="${YUNETA_DIR}/bin/openresty/bin/openresty"

if [ -r /lib/lsb/init-functions ]; then
    . /lib/lsb/init-functions
else
    log_daemon_msg() { echo "$@"; }
    log_end_msg()    { [ "$1" -eq 0 ] && echo "OK" || echo "FAIL ($1)"; }
fi

_run_as_yuneta() {
    # Use a login shell so /home/yuneta environment is loaded.
    su - "$RUN_AS" -s /bin/sh -c "$*"
}

start_yunos() {
    RC=0
    if [ -x "$AGENT1_BIN" ]; then
        log_daemon_msg "Starting yuneta_agent"
        _run_as_yuneta "exec \"$AGENT1_BIN\" --start --config-file=\"$AGENT1_CFG\"" || RC=$?
        log_end_msg $RC
    else
        echo "WARN: $AGENT1_BIN not found or not executable" >&2
    fi

    if [ -x "$AGENT2_BIN" ]; then
        log_daemon_msg "Starting yuneta_agent22"
        _run_as_yuneta "exec \"$AGENT2_BIN\" --start --config-file=\"$AGENT2_CFG\"" || true
        log_end_msg 0
    else
        echo "WARN: $AGENT2_BIN not found or not executable" >&2
    fi
    return 0
}

start_web() {
    # Default: start Nginx if present
    if [ -x "$NGINX_BIN" ]; then
        log_daemon_msg "Starting Nginx"
        "$NGINX_BIN" || true
        log_end_msg 0
    fi

    # To use OpenResty instead of Nginx, uncomment these lines:
    # if [ -n "$OPENRESTY_BIN" ] && [ -x "$OPENRESTY_BIN" ]; then
    #     log_daemon_msg "Starting OpenResty"
    #     "$OPENRESTY_BIN" || true
    #     log_end_msg 0
    # fi
}

stop_yunos() {
    YSHUT="${YUNETA_DIR}/bin/yshutdown"
    if [ -x "$YSHUT" ]; then
        log_daemon_msg "Stopping Yuneta (yshutdown)"
        "$YSHUT" || true
        log_end_msg 0
    else
        echo "WARN: $YSHUT not found" >&2
    fi
}

stop_web() {
    # Stop Nginx if running
    if pidof nginx >/dev/null 2>&1; then
        log_daemon_msg "Stopping Nginx"
        "$NGINX_BIN" -s quit || true
        log_end_msg 0
    fi

    # To stop OpenResty if enabled, uncomment:
    # if pidof openresty >/dev/null 2>&1; then
    #     log_daemon_msg "Stopping OpenResty"
    #     "$OPENRESTY_BIN" -s quit || true
    #     log_end_msg 0
    # fi
}

status_yunos() {
    S=3
    MSG=""

    if command -v pgrep >/dev/null 2>&1; then
        if pgrep -u "$RUN_AS" -f "/agent/yuneta_agent( |$)" >/dev/null; then
            MSG="$MSG yuneta_agent: running;"; S=0
        else
            MSG="$MSG yuneta_agent: not running;"
        fi
        if pgrep -u "$RUN_AS" -f "/agent/yuneta_agent22( |$)" >/dev/null; then
            MSG="$MSG yuneta_agent22: running;"; S=0
        else
            MSG="$MSG yuneta_agent22: not running;"
        fi
    else
        if pidof -x yuneta_agent   >/dev/null 2>&1; then MSG="$MSG yuneta_agent: running;";   S=0; else MSG="$MSG yuneta_agent: not running;";   fi
        if pidof -x yuneta_agent22 >/dev/null 2>&1; then MSG="$MSG yuneta_agent22: running;"; S=0; else MSG="$MSG yuneta_agent22: not running;"; fi
    fi

    echo "$MSG"
    return $S
}

status_web() {
    if pidof nginx >/dev/null 2>&1; then
        echo "nginx: running;"
        return 0
    fi
    # If OpenResty is enabled, you can also report it:
    # if pidof openresty >/dev/null 2>&1; then
    #     echo "openresty: running;"
    #     return 0
    # fi
    echo "web: not running;"
    return 3
}

case "${1:-}" in
    start)            start_yunos; start_web ;;
    stop)             stop_web;    stop_yunos ;;
    restart|force-reload)
                      stop_web || true
                      stop_yunos || true
                      start_yunos
                      start_web
                      ;;
    status)           status_yunos; status_web ;;
    *)                echo "Usage: $0 {start|stop|restart|force-reload|status}"; exit 2 ;;
esac

exit 0
EOF
chmod 0755 "${WORKDIR}/etc/init.d/yuneta_agent"

# Mirror init script into service dir with perms
install -m 0755 "${WORKDIR}/etc/init.d/yuneta_agent" "${WORKDIR}/yuneta/agent/service/yuneta_agent"

# --- control file (runtime deps + friendly suggests) ---
cat > "${WORKDIR}/DEBIAN/control" <<EOF
Package: ${PROJECT}
Version: ${VERSION}-${RELEASE}
Section: utils
Priority: optional
Architecture: ${ARCHITECTURE}
Homepage: https://yuneta.io
Maintainer: ArtGins S.L. <support@artgins.com>
Depends: adduser, lsb-base, rsync, locales, rsyslog
Recommends: curl, vim, sudo
Suggests: git, mercurial, make, cmake, ninja-build, gcc, musl, musl-dev, musl-tools, clang, g++,
 python3-dev, python3-pip, python3-setuptools, python3-tk, python3-wheel, python3-venv,
 libjansson-dev, libpcre2-dev, liburing-dev, libcurl4-openssl-dev, libpcre3-dev, zlib1g-dev, libssl-dev,
 perl, dos2unix, tree, postgresql-server-dev-all, libpq-dev, kconfig-frontends, telnet, pipx, patch, gettext, fail2ban, snapd
Description: Yunetas Agent
 Yunetas Agent binaries and runtime directories.
EOF

# --- conffiles ---
cat > "${WORKDIR}/DEBIAN/conffiles" <<'EOF'
/etc/profile.d/yuneta.sh
/etc/init.d/yuneta_agent
/etc/letsencrypt/renewal-hooks/deploy/reload-certs
EOF

# --- helper scripts (installed under /yuneta/agent/service) ---
cat > "${WORKDIR}/yuneta/agent/service/install-yuneta-service.sh" <<'EOF'
#!/bin/sh
#
# Enable and start the SysV service. Idempotent and quiet on errors.
#
set -eu

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
EOF
chmod 0755 "${WORKDIR}/yuneta/agent/service/install-yuneta-service.sh"

cat > "${WORKDIR}/yuneta/agent/service/remove-yuneta-service.sh" <<'EOF'
#!/bin/sh
# Stop SysV service and remove init symlinks.
# Does NOT delete /etc/init.d/yuneta_agent unless called with --purge.
set -eu

PURGE=0
[ "${1:-}" = "--purge" ] && PURGE=1

# Stop service if present
if command -v invoke-rc.d >/dev/null 2>&1; then
    invoke-rc.d yuneta_agent stop || true
elif [ -x /etc/init.d/yuneta_agent ]; then
    /etc/init.d/yuneta_agent stop || true
fi

# Remove rc.d symlinks (safe even if already gone)
if [ -x /usr/sbin/update-rc.d ]; then
    /usr/sbin/update-rc.d -f yuneta_agent remove >/dev/null 2>&1 || true
fi

# Only on purge, delete the init file itself (dpkg will also remove conffiles)
if [ "$PURGE" -eq 1 ]; then
    rm -f /etc/init.d/yuneta_agent || true
fi

exit 0
EOF
chmod 0755 "${WORKDIR}/yuneta/agent/service/remove-yuneta-service.sh"

# --- Certbot via snap installer (ops helper; safe to rerun) ---
cat > "${WORKDIR}/yuneta/bin/install-certbot-snap.sh" <<'EOF'
#!/usr/bin/env bash
#
# Install/enable snapd and Certbot (snap package) the official way.
#
# Makes /usr/bin/certbot -> /snap/bin/certbot symlink if needed.
#
set -euo pipefail

need_root() {
    if [ "${EUID:-$(id -u)}" -ne 0 ]; then
        if command -v sudo >/dev/null 2>&1; then
            exec sudo -E bash "$0" "$@"
        fi
        echo "This script must run as root (or install sudo)." >&2
        exit 1
    fi
}
need_root "$@"

export DEBIAN_FRONTEND=noninteractive

# 1) Ensure snapd is present (Debian/Ubuntu)
if ! command -v snap >/dev/null 2>&1; then
    if command -v apt-get >/dev/null 2>&1; then
        apt-get update -y || true
        apt-get install -y --no-install-recommends snapd
        # Ensure snapd services are up before using snap
        systemctl enable --now snapd snapd.apparmor 2>/dev/null || true
        # Wait for initial snap seed on some systems
        snap wait system seed.loaded 2>/dev/null || true
    else
        echo "apt-get not found; cannot install snapd automatically." >&2
        exit 1
    fi
fi

# 2) Install/refresh core and certbot (official method)
snap install core || true
snap refresh core || true
snap install --classic certbot

# 3) Make certbot visible even if /snap/bin isn't in PATH
if [ -x /snap/bin/certbot ] && [ ! -e /usr/bin/certbot ]; then
    ln -s /snap/bin/certbot /usr/bin/certbot
fi

# 4) Ensure your deploy-hook directory exists (your .deb also creates it)
install -d -m 0755 /etc/letsencrypt/renewal-hooks/deploy

echo
echo "[i] Certbot version:"
/usr/bin/certbot --version || true

echo
echo "[i] Certbot renewal timers (snap-managed):"
systemctl list-timers | grep -E 'certbot|snap' || true

cat <<'HINT'

Next steps (examples):

  # Standalone (temporarily free port 80)
  # service yuneta_agent stop && /usr/bin/certbot certonly --standalone -d example.com && service yuneta_agent start

  # Webroot (recommended if your Yuneta nginx serves HTTP):
  # 1) In your Yuneta nginx server block, add:
  #    location /.well-known/acme-challenge/ {
  #        root /yuneta/share/certbot-webroot;
  #        default_type text/plain;
  #    }
  #    (reload nginx)
  # 2) Create the webroot dir:
  #    mkdir -p /yuneta/share/certbot-webroot
  # 3) Issue:
  #    /usr/bin/certbot certonly --webroot -w /yuneta/share/certbot-webroot -d example.com

Your .deb already installs a deploy hook at:
/etc/letsencrypt/renewal-hooks/deploy/reload-certs
It copies renewed certs to /yuneta/store/certs/, reloads nginx, and restarts Yuneta.
HINT
EOF
chmod 0755 "${WORKDIR}/yuneta/bin/install-certbot-snap.sh"

# --- Let's Encrypt: deploy hook to copy + reload + restart ---
cat > "${WORKDIR}/etc/letsencrypt/renewal-hooks/deploy/reload-certs" <<'EOF'
#!/bin/sh
#
# Certbot deploy hook: run after a successful renewal
#
# What this does:
#   1) Copies renewed certs from /etc/letsencrypt/live/* to /yuneta/store/certs/
#   2) Reloads Nginx to pick the new certs (OpenResty optional)
#   3) Restarts the Yuneta stack (background, non-blocking)
#
set -eu

LOG_DIR="/var/log/yuneta"
mkdir -p "$LOG_DIR"

# 1) Copy fresh certs into /yuneta/store/certs (synchronous, requires root)
if [ -x /yuneta/store/certs/copy-certs.sh ]; then
    /yuneta/store/certs/copy-certs.sh >>"$LOG_DIR/copy-certs.log" 2>&1
else
    echo "Warning: /yuneta/store/certs/copy-certs.sh not found or not executable." >&2
fi

# 2) Reload web servers to pick updated certs
if [ -x /yuneta/bin/nginx/sbin/nginx ]; then
    /yuneta/bin/nginx/sbin/nginx -s reload || true
else
    echo "Warning: nginx binary not found or not executable." >&2
fi

# Optional: OpenResty reload
# if [ -x /yuneta/bin/openresty/bin/openresty ]; then
#     /yuneta/bin/openresty/bin/openresty -s reload || true
# fi

# 3) Restart Yuneta stack (background, non-blocking)
if [ -x /yuneta/bin/restart-yuneta ]; then
    nohup /yuneta/bin/restart-yuneta >>"$LOG_DIR/restart-yuneta.log" 2>&1 &
else
    echo "Warning: /yuneta/bin/restart-yuneta not found or not executable." >&2
fi

exit 0
EOF
chmod 0755 "${WORKDIR}/etc/letsencrypt/renewal-hooks/deploy/reload-certs"

# --- Restart helper: prefer SysV restart when root; fallback otherwise ---
cat > "${WORKDIR}/yuneta/bin/restart-yuneta" <<'EOF'
#!/usr/bin/env bash
#
# Restart Yuneta safely after cert updates or maintenance.
#
# - If root: prefer SysV service restart (invoke-rc.d/init.d)
# - Else:    graceful yshutdown + start via yuneta_agent
#
set -euo pipefail

# If root, prefer SysV service restart; else fallback to yshutdown + start
if [ "${EUID:-$(id -u)}" -eq 0 ]; then
    if command -v invoke-rc.d >/dev/null 2>&1; then
        invoke-rc.d yuneta_agent restart || true
        exit 0
    fi
    if [ -x /etc/init.d/yuneta_agent ]; then
        /etc/init.d/yuneta_agent restart || true
        exit 0
    fi
fi

# Fallback: do a gentle restart under current user (works for user 'yuneta')
if [ -x /yuneta/bin/yshutdown ]; then
    /yuneta/bin/yshutdown || true
    sleep 1
fi
if [ -x /yuneta/agent/yuneta_agent ]; then
    /yuneta/agent/yuneta_agent --start --config-file=/yuneta/agent/yuneta_agent.json || true
fi

# Optional web reloads (usually handled by the certbot hook already)
# [ -x /yuneta/bin/nginx/sbin/nginx ] && /yuneta/bin/nginx/sbin/nginx -s reload || true
# [ -x /yuneta/bin/openresty/bin/openresty ] && /yuneta/bin/openresty/bin/openresty -s reload || true
EOF
chmod 0755 "${WORKDIR}/yuneta/bin/restart-yuneta"

# --- Helper: check queues of messages (two-level walker) ---
cat > "${WORKDIR}/yuneta/bin/colas2.sh" <<'EOF'
#!/usr/bin/env bash
#
# Walk Yuneta gate message queues and list messages with `list_queue_msgs2`.
#
# Usage:
#   colas2.sh                       # scan default base /yuneta/store/queues/gate_msgs2
#   colas2.sh <base_dir> [args...]  # scan <base_dir> and pass [args...] to list_queue_msgs2
#
set -euo pipefail

LIST_BIN="/yuneta/bin/list_queue_msgs2"
BASE="/yuneta/store/queues/gate_msgs2"

# Allow an alternate base dir as first argument
if [ "$#" -gt 0 ] && [ -d "$1" ]; then
    BASE="$1"
    shift
fi

FWD_ARGS=("$@")

if [ ! -d "$BASE" ]; then
    echo "Directory not found: $BASE" >&2
    exit 1
fi

# Ensure we don't iterate literal patterns when no matches
shopt -s nullglob

found_any=false
for qdir in "$BASE"/*/; do
    qdir="${qdir%/}"
    for q2dir in "$qdir"/*/; do
        q2dir="${q2dir%/}"
        echo "===> ${LIST_BIN} \"$q2dir\" ${FWD_ARGS[*]:-}"
        if [ -x "$LIST_BIN" ]; then
            "$LIST_BIN" "$q2dir" "${FWD_ARGS[@]:-}"
        else
            echo "ERROR: $LIST_BIN not found or not executable." >&2
            exit 2
        fi
        found_any=true
    done
done

if ! $found_any; then
    echo "No queue subdirectories found under: $BASE"
fi
EOF
chmod 0755 "${WORKDIR}/yuneta/bin/colas2.sh"

# --- Dev stack installer (optional helper; installs many build deps) ---
cat > "${WORKDIR}/yuneta/bin/install-yuneta-dev-deps.sh" <<'EOF'
#!/usr/bin/env bash
#
# Install common Yuneta dev dependencies via apt (no recommends) and pipx.
#
set -euo pipefail

need_root() {
    if [ "${EUID:-$(id -u)}" -ne 0 ]; then
        if command -v sudo >/dev/null 2>&1; then
            exec sudo -E bash "$0" "$@"
        fi
        echo "This script must run as root (or install sudo)." >&2
        exit 1
    fi
}
need_root "$@"

export DEBIAN_FRONTEND=noninteractive

if command -v apt-get >/dev/null 2>&1; then
    apt-get update -y || true
else
    echo "apt-get not found; this helper supports Debian/Ubuntu systems only." >&2
    exit 1
fi

PKGS=(
    git mercurial make cmake ninja-build
    gcc musl musl-dev musl-tools clang g++
    python3-dev python3-pip python3-setuptools
    python3-tk python3-wheel python3-venv
    libjansson-dev libpcre2-dev liburing-dev libcurl4-openssl-dev
    libpcre3-dev zlib1g-dev libssl-dev
    perl dos2unix tree curl
    postgresql-server-dev-all libpq-dev
    kconfig-frontends telnet pipx
    patch gettext fail2ban rsync
    build-essential pkg-config ca-certificates
)

echo "[i] Installing development packages (no recommends)…"
for p in "${PKGS[@]}"; do
    if apt-cache show "$p" >/dev/null 2>&1; then
        echo " -> $p"
        apt-get install -y --no-install-recommends "$p" || echo "[!] Failed: $p (continuing)"
    else
        echo "[!] Skipping not-found package: $p"
    fi
done

# Refresh CA bundle if available (helps TLS in builds/fetches)
if command -v update-ca-certificates >/dev/null 2>&1; then
    update-ca-certificates || true
fi

# kconfiglib via pipx
if command -v pipx >/dev/null 2>&1; then
    echo "[i] Installing kconfiglib via pipx…"
    pipx install --include-deps kconfiglib || echo "[!] pipx install kconfiglib failed (continuing)"
else
    echo "[!] pipx not installed; kconfiglib not installed." >&2
fi

echo "[✓] Dev environment setup attempt complete."
EOF
chmod 0755 "${WORKDIR}/yuneta/bin/install-yuneta-dev-deps.sh"

# --- Copy-certs helper: root-only, supports certs.list or auto-discovery ---
cat > "${WORKDIR}/yuneta/store/certs/copy-certs.sh" <<'EOF'
#!/usr/bin/env bash
#
# Copy Let’s Encrypt live certs into Yuneta’s store.
#
# Sources:
#   /etc/letsencrypt/live/<name>/{fullchain.pem,chain.pem,privkey.pem}
#
# Destinations:
#   /yuneta/store/certs/<name>.crt
#   /yuneta/store/certs/<name>.chain
#   /yuneta/store/certs/private/<name>.key   (0600)
#
# Selection:
#   - If /yuneta/store/certs/certs.list exists and has entries, copy only those.
#   - Else, auto-discover all directories under /etc/letsencrypt/live.
#
# Permissions:
#   - Requires root to read privkey.pem.
#   - Sets ownership to yuneta:yuneta for copied files (best effort).
#
set -euo pipefail

# Must run as root (privkey.pem is root-only in /etc/letsencrypt/live)
if [ "${EUID:-$(id -u)}" -ne 0 ]; then
    echo "This script must run as root (to read /etc/letsencrypt/live/*/privkey.pem)" >&2
    exit 1
fi

DEST_BASE="/yuneta/store/certs"
DEST_PRIV="${DEST_BASE}/private"
CERTS_FILE="${DEST_BASE}/certs.list"

install -d -m 0755 "${DEST_BASE}"
install -d -m 0700 "${DEST_PRIV}"

# Build list of certificate names: prefer certs.list, else auto-discover
CERT_NAMES=""
if [ -s "${CERTS_FILE}" ]; then
    while IFS= read -r line; do
        case "$line" in ''|\#*) continue ;; *) CERT_NAMES="${CERT_NAMES} ${line}" ;; esac
    done < "${CERTS_FILE}"
else
    if [ -d /etc/letsencrypt/live ]; then
        for d in /etc/letsencrypt/live/*; do
            [ -d "$d" ] || continue
            CERT_NAMES="${CERT_NAMES} $(basename "$d")"
        done
    fi
fi

if [ -z "${CERT_NAMES}" ]; then
    echo "No certificates to copy (certs.list empty and no live certs found)." >&2
    exit 0
fi

for CERT in ${CERT_NAMES}; do
    SRC_DIR="/etc/letsencrypt/live/${CERT}"
    FULLCHAIN="${SRC_DIR}/fullchain.pem"
    CHAIN="${SRC_DIR}/chain.pem"
    PRIVKEY="${SRC_DIR}/privkey.pem"

    if [ ! -r "${FULLCHAIN}" ] || [ ! -r "${CHAIN}" ] || [ ! -r "${PRIVKEY}" ]; then
        echo "Skip ${CERT}: missing files under ${SRC_DIR}" >&2
        continue
    fi

    install -m 0644 "${FULLCHAIN}" "${DEST_BASE}/${CERT}.crt"
    install -m 0644 "${CHAIN}"     "${DEST_BASE}/${CERT}.chain"
    install -m 0600 "${PRIVKEY}"   "${DEST_PRIV}/${CERT}.key"

    chown yuneta:yuneta "${DEST_BASE}/${CERT}.crt" "${DEST_BASE}/${CERT}.chain" "${DEST_PRIV}/${CERT}.key" || true
    echo "Copied ${CERT} -> ${DEST_BASE}/{${CERT}.crt, ${CERT}.chain, private/${CERT}.key}"
done

# Optional: quick validity summary if tool is present
if [ -x /yuneta/bin/check-certs-validity.sh ]; then
    /yuneta/bin/check-certs-validity.sh "${DEST_BASE}" || true
fi

exit 0
EOF
chmod 0755 "${WORKDIR}/yuneta/store/certs/copy-certs.sh"

# --- Provide a commented template certs.list (admin can edit later) ---
cat > "${WORKDIR}/yuneta/store/certs/certs.list" <<'EOF'
# One certificate name per line (matching directory name under /etc/letsencrypt/live)
# Example:
# example.com
# api.example.com
EOF
chmod 0644 "${WORKDIR}/yuneta/store/certs/certs.list"

# --- Make yuneta sudo (NOPASSWD, convenience for ops) ---
cat > "${WORKDIR}/etc/sudoers.d/90-yuneta" <<'EOF'
# User rules for yuneta (adjust if you prefer finer-grained commands)
yuneta ALL=(ALL) NOPASSWD:ALL
EOF
chmod 0440 "${WORKDIR}/etc/sudoers.d/90-yuneta"

# --- postinst (create login user, locales, syslog, SysV enable on first install) ---
cat > "${WORKDIR}/DEBIAN/postinst" <<'EOF'
#!/bin/sh
#
# Configure system after package files are unpacked.
#
set -eu

# ---- Create/normalize 'yuneta' login user (home /home/yuneta, shell /bin/bash)
if id -u yuneta >/dev/null 2>&1; then
    # Ensure login shell and home exist/match expectations
    CUR_SHELL="$(getent passwd yuneta | awk -F: '{print $7}')"
    [ -n "$CUR_SHELL" ] || CUR_SHELL="/bin/false"
    if [ "$CUR_SHELL" = "/usr/sbin/nologin" ] || [ "$CUR_SHELL" = "/bin/false" ]; then
        usermod -s /bin/bash yuneta || true
    fi
    if [ ! -d /home/yuneta ]; then
        mkdir -p /home/yuneta
        chown -R yuneta:yuneta /home/yuneta || true
        chmod 0755 /home/yuneta || true
    fi
    # If home is different, migrate it
    CUR_HOME="$(getent passwd yuneta | awk -F: '{print $6}')"
    if [ "$CUR_HOME" != "/home/yuneta" ]; then
        usermod -d /home/yuneta -m yuneta || true
    fi
else
    # Create a normal login user (no preset password)
    adduser --disabled-password --gecos "" \
            --home /home/yuneta --shell /bin/bash yuneta || true
fi

# ---- Ensure agent configs exist without overwriting existing ones
if [ ! -e /yuneta/agent/yuneta_agent.json ]; then
    if [ -e /yuneta/agent/yuneta_agent.json.sample ]; then
        install -o yuneta -g yuneta -m 0644 -T \
            /yuneta/agent/yuneta_agent.json.sample /yuneta/agent/yuneta_agent.json
    else
        printf '{}\n' > /yuneta/agent/yuneta_agent.json
        chown yuneta:yuneta /yuneta/agent/yuneta_agent.json
        chmod 0644 /yuneta/agent/yuneta_agent.json
    fi
fi

if [ ! -e /yuneta/agent/yuneta_agent22.json ]; then
    if [ -e /yuneta/agent/yuneta_agent22.json.sample ]; then
        install -o yuneta -g yuneta -m 0644 -T \
            /yuneta/agent/yuneta_agent22.json.sample /yuneta/agent/yuneta_agent22.json
    else
        printf '{}\n' > /yuneta/agent/yuneta_agent22.json
        chown yuneta:yuneta /yuneta/agent/yuneta_agent22.json
        chmod 0644 /yuneta/agent/yuneta_agent22.json
    fi
fi

# ---- Locales: ensure en_US.UTF-8 and es_ES.UTF-8
if command -v locale-gen >/dev/null 2>&1; then
    sed -i 's/^[# ]*en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen || true
    sed -i 's/^[# ]*es_ES.UTF-8 UTF-8/es_ES.UTF-8 UTF-8/' /etc/locale.gen || true
    grep -q '^en_US.UTF-8 UTF-8' /etc/locale.gen || echo 'en_US.UTF-8 UTF-8' >> /etc/locale.gen
    grep -q '^es_ES.UTF-8 UTF-8' /etc/locale.gen || echo 'es_ES.UTF-8 UTF-8' >> /etc/locale.gen
    locale-gen || true
    update-locale LANG=en_US.UTF-8 LANGUAGE="en_US:es_ES" || true
fi

# ---- Ownership/perms for Yuneta dirs
if [ -d /yuneta ]; then
    chown -R yuneta:yuneta /yuneta || true
fi
# Keep /yuneta/store/certs readable and lock private/
if [ -d /yuneta/store/certs ]; then
    chmod 0755 /yuneta/store/certs || true
fi
if [ -d /yuneta/store/certs/private ]; then
    chmod 0700 /yuneta/store/certs/private || true
fi

# ---- Ensure classic /var/log/syslog via rsyslog
if command -v systemctl >/dev/null 2>&1; then
    systemctl enable --now rsyslog || true
else
    if [ -x /etc/init.d/rsyslog ]; then
        if command -v update-rc.d >/dev/null 2>&1; then
            update-rc.d rsyslog defaults >/dev/null 2>&1 || true
        fi
        /etc/init.d/rsyslog start || true
    fi
fi

# $1 is "configure"; $2 is previous version on upgrade (empty on first install)
if [ "${1:-}" = "configure" ] && [ -z "${2:-}" ]; then
    # SysV integration (prefer helper script)
    if [ -x /yuneta/agent/service/install-yuneta-service.sh ]; then
        /yuneta/agent/service/install-yuneta-service.sh || true
    else
        if [ -x /usr/sbin/update-rc.d ]; then
            /usr/sbin/update-rc.d yuneta_agent defaults >/dev/null 2>&1 || true
        fi
        if command -v invoke-rc.d >/dev/null 2>&1; then
            invoke-rc.d yuneta_agent start || true
        else
            /etc/init.d/yuneta_agent start || true
        fi
    fi
fi
exit 0
EOF
chmod 0755 "${WORKDIR}/DEBIAN/postinst"

# --- prerm (stop before upgrade/remove) ---
cat > "${WORKDIR}/DEBIAN/prerm" <<'EOF'
#!/bin/sh
#
# Stop the service prior to upgrade/remove (best-effort).
#
set -eu

if command -v invoke-rc.d >/dev/null 2>&1; then
    invoke-rc.d yuneta_agent stop || true
else
    [ -x /etc/init.d/yuneta_agent ] && /etc/init.d/yuneta_agent stop || true
fi

exit 0
EOF
chmod 0755 "${WORKDIR}/DEBIAN/prerm"

# --- postrm (remove init symlinks AND the init file on remove/purge) ---
cat > "${WORKDIR}/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -eu

case "${1:-}" in
    remove)
        # Remove rc symlinks only; keep the init script (conffile)
        if [ -x /yuneta/agent/service/remove-yuneta-service.sh ]; then
            /yuneta/agent/service/remove-yuneta-service.sh || true
        else
            if [ -x /usr/sbin/update-rc.d ]; then
                /usr/sbin/update-rc.d -f yuneta_agent remove >/dev/null 2>&1 || true
            fi
            # Do NOT rm /etc/init.d/yuneta_agent here
        fi
        ;;
    purge)
        # Full cleanup; dpkg will remove the conffile, but ensure service is stopped and symlinks gone
        if [ -x /yuneta/agent/service/remove-yuneta-service.sh ]; then
            /yuneta/agent/service/remove-yuneta-service.sh --purge || true
        else
            if command -v invoke-rc.d >/dev/null 2>&1; then
                invoke-rc.d yuneta_agent stop || true
            elif [ -x /etc/init.d/yuneta_agent ]; then
                /etc/init.d/yuneta_agent stop || true
            fi
            if [ -x /usr/sbin/update-rc.d ]; then
                /usr/sbin/update-rc.d -f yuneta_agent remove >/dev/null 2>&1 || true
            fi
            # If dpkg hasn’t removed the conffile yet, remove it quietly
            rm -f /etc/init.d/yuneta_agent || true
        fi
        ;;
esac

exit 0
EOF
chmod 0755 "${WORKDIR}/DEBIAN/postrm"

# ---------- Normalize permissions before building ----------
# Ensure all dirs are 0755 and clear any inherited setgid/sticky bits
find "${WORKDIR}" -type d -exec chmod 0755 {} + -exec chmod g-s {} + -exec chmod -t {} +

# DEBIAN/ must be exactly 0755 (no setgid/sticky)
chmod 0755 "${WORKDIR}/DEBIAN" || true
chmod g-s  "${WORKDIR}/DEBIAN" || true
chmod -t   "${WORKDIR}/DEBIAN" || true

# All DEBIAN control files default to 0644
find "${WORKDIR}/DEBIAN" -type f -print0 | xargs -0 chmod 0644

# Maintainer scripts must be executable
chmod 0755 "${WORKDIR}/DEBIAN/postinst" \
            "${WORKDIR}/DEBIAN/prerm" \
            "${WORKDIR}/DEBIAN/postrm"

# init.d script must be executable
chmod 0755 "${WORKDIR}/etc/init.d/yuneta_agent"

# ---------- Build .deb (root ownership inside package) ----------
mkdir -p "${BASE_DEST}" "${OUT_DIR}"
OUT_DEB_WORK="${BASE_DEST}/${PACKAGE}.deb"
echo "[i] Building ${OUT_DEB_WORK}"
dpkg-deb --build --root-owner-group "${WORKDIR}" "${OUT_DEB_WORK}"

# ---------- Place final artifact in ./dist ----------
FINAL_DEB="${OUT_DIR}/${PACKAGE}.deb"
mv -f "${OUT_DEB_WORK}" "${FINAL_DEB}"

echo "[✓] Final package:"
echo "    ${FINAL_DEB}"
echo
echo "Install with:"
echo "    sudo apt -y install '${FINAL_DEB}'"
