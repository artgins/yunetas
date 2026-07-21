#!/usr/bin/env bash
#######################################################################
#               make-yuneta-agent-deb.sh
#######################################################################
# Build a .deb for Yuneta's Agent with SysV integration
#
# Usage:
#     ./make-yuneta-agent-deb.sh <project> <version> <release> <arch>
#
# Layout (relative to this script, which must live in yunetas/packages/):
#   - builds under: ./build/deb/<arch>/<project-version-release-arch>/
#   - outputs to:   ./dist/<project-version-release-arch>.deb
#
# Major features:
#   - Ships Yuneta agent binaries + runtime tree under /yuneta
#   - SysV init script at /etc/init.d/yuneta_agent
#   - Certbot deploy hook to copy-renewed certs + reload selected web server + restart Yuneta
#   - Helpers for installing dev deps and certbot (snap)
#   - Creates user 'yuneta' with login at /home/yuneta (if needed)
#   - Makes /var/log/syslog available via rsyslog
#
# Notes:
#   - Script must live in yunetas/packages/
#   - Builds under ./build/ and places the final .deb in ./dist
#   - Indentation is 4 spaces everywhere (as requested)
#   - The ssh keys that you put in `authorized_keys/authorized_keys` file,
#     they will be installed by the .deb in the user yuneta.
#   - The selection of the webserver can be done with the file `webserver/webserver`
#     (contents: "nginx" or "openresty"), default nginx
#######################################################################

set -euo pipefail
umask 022

# Resolve the script's real directory (yunetas/packages) to anchor relative paths.
# Works regardless of launch location (cron, make, symlink, different CWD).
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
BIN_DIR="/yuneta/bin"

echo "[i] Building package tree at: ${WORKDIR}"

#-----------------------------------------------------#
#   Get yunetas base path:
#   Resolve YUNETAS_BASE:
#       1) $YUNETAS_BASE if valid dir,
#       2) /yuneta/development/yunetas,
#       else fail.
#-----------------------------------------------------#
# If env is set but invalid, warn and ignore
if [[ -n "${YUNETAS_BASE:-}" && ! -d "$YUNETAS_BASE" ]]; then
    echo "Warning: YUNETAS_BASE is set to '$YUNETAS_BASE' but is not a directory. Falling back..." >&2
    unset YUNETAS_BASE
fi

if [[ -z "${YUNETAS_BASE:-}" ]]; then
    d="/yuneta/development/yunetas"
    if [[ -d "$d" ]]; then
        YUNETAS_BASE="$d"
    fi
fi

# Hard fail if still unset
if [[ -z "${YUNETAS_BASE:-}" ]]; then
    echo "Error: Could not determine YUNETAS_BASE. Set the env var or ensure /yuneta/development/yunetas exists." >&2
    exit 1
fi

echo "Using YUNETAS_BASE: $YUNETAS_BASE"

# Fresh workspace
rm -rf "${WORKDIR}"
mkdir -p "${WORKDIR}/DEBIAN"
mkdir -p "${WORKDIR}/etc/profile.d"
mkdir -p "${WORKDIR}/etc/sudoers.d"
mkdir -p "${WORKDIR}/etc/init.d"                           # ship the init script
mkdir -p "${WORKDIR}/etc/letsencrypt/renewal-hooks/deploy" # certbot deploy hook
mkdir -p "${WORKDIR}/yuneta/agent/service"                 # helper scripts live here
mkdir -p "${WORKDIR}/yuneta/agent"
mkdir -p "${WORKDIR}/yuneta/bin"
mkdir -p "${WORKDIR}/yuneta/gui"
mkdir -p "${WORKDIR}/yuneta/realms"
mkdir -p "${WORKDIR}/yuneta/repos"
mkdir -p "${WORKDIR}/yuneta/store/certs/private"           # private will be 0700 in postinst
mkdir -p "${WORKDIR}/yuneta/store/queues/gate_msgs2"
mkdir -p "${WORKDIR}/yuneta/share"
mkdir -p "${WORKDIR}/yuneta/development/yunetas/outputs"
mkdir -p "${WORKDIR}/yuneta/development/yunetas/outputs_ext"
mkdir -p "${WORKDIR}/yuneta/development/yunetas/tools/cmake"
mkdir -p "${WORKDIR}/yuneta/development/projects"
mkdir -p "${WORKDIR}/etc/yuneta"
mkdir -p "${WORKDIR}/var/crash"
mkdir -p "${WORKDIR}/etc/sysctl.d"
mkdir -p "${WORKDIR}/etc/security/limits.d"

# --- Single-file utilities to include /yuneta/bin ---
BINARIES=(
    keycloak_pkey_to_jwks
    list_queue_msgs2
    tr2keys
    tr2list
    tr2migrate
    watchfs
    ybatch
    ycli
    ycommand
    ylist
    yshutdown
    ystats
    ytests

    fs_watcher
    inotify
    yclone-gclass
    yclone-project
    ymake-skeleton
    yscapec
    ytestconfig
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

copy_tree "/yuneta/bin/ncurses"                 "${WORKDIR}/yuneta/bin"
copy_tree "/yuneta/bin/nginx"                   "${WORKDIR}/yuneta/bin"
copy_tree "/yuneta/bin/openresty"               "${WORKDIR}/yuneta/bin"
copy_tree "/yuneta/bin/skeletons"               "${WORKDIR}/yuneta/bin"
# Sparse SDK tree: same YUNETAS_BASE path as a full source checkout, so
# outputs/ lives at /yuneta/development/yunetas/outputs on EVERY node.
copy_tree "${YUNETAS_BASE}/outputs_ext"         "${WORKDIR}/yuneta/development/yunetas"
copy_tree "${YUNETAS_BASE}/outputs"             "${WORKDIR}/yuneta/development/yunetas"
copy_tree "${YUNETAS_BASE}/tools"               "${WORKDIR}/yuneta/development/yunetas"

#
#   The glibc stamp must ship: it is what stops a node from compiling its own
#   yunos against these archives under a different glibc, which links cleanly
#   and then corrupts the heap at run time. Written by the gobj-c build (see
#   tools/cmake/libc_guard.cmake); missing means the payload was not built
#   from a current SDK.
#
STAMP="${WORKDIR}/yuneta/development/yunetas/outputs/lib/yuneta_libc.stamp"
if [ ! -s "${STAMP}" ]; then
    echo "[-] Missing glibc stamp: ${STAMP}" >&2
    echo "    Rebuild the SDK (yunetas clean && yunetas build) before packaging." >&2
    exit 2
fi
echo "[i] Payload archives built against glibc $(cat "${STAMP}")"
install -D -m 0644 "${YUNETAS_BASE}/.config"    "${WORKDIR}/yuneta/development/yunetas/.config"

# Strip lab load-generators from the payload: stress_* have no place on a
# production node. perf_* stay on purpose — fully static benchmarks, handy
# to measure the target machine.
rm -f "${WORKDIR}"/yuneta/development/yunetas/outputs/yunos/stress_* \
      "${WORKDIR}"/yuneta/development/yunetas/outputs/bin/stress_*

rm -f "${WORKDIR}"/yuneta/bin/nginx/logs/* 2>/dev/null || true
rm -f "${WORKDIR}"/yuneta/bin/openresty/nginx/logs/* 2>/dev/null || true


# --- Optional: bundle SSH public key(s) for user 'yuneta' ---
# Two ways to provide keys (env var wins):
#   - YUNETA_AUTHORIZED_KEYS=/path/to/authorized_keys  (preferred)
#   - ${SCRIPT_DIR}/authorized_keys/authorized_keys     (legacy, kept
#     for backwards-compat with project-level builds)
# The public framework .deb sets neither: no SSH keys bundled.
YUNETA_AUTH_KEYS_FILE="${YUNETA_AUTHORIZED_KEYS:-${SCRIPT_DIR}/authorized_keys/authorized_keys}"
if [ -f "${YUNETA_AUTH_KEYS_FILE}" ]; then
    echo "[i] Bundling authorized_keys from ${YUNETA_AUTH_KEYS_FILE}"
    install -D -m 0644 "${YUNETA_AUTH_KEYS_FILE}" "${WORKDIR}/etc/yuneta/authorized_keys"
fi

# --- Optional: select web server (nginx|openresty) via external file ---
# Reads ${SCRIPT_DIR}/webserver/webserver if present (contents: "nginx" or "openresty").
WEB_SELECTOR_FILE="${SCRIPT_DIR}/webserver/webserver"
WEB_CHOICE="nginx"
if [ -f "${WEB_SELECTOR_FILE}" ]; then
    WEB_CHOICE="$(tr '[:upper:]' '[:lower:]' < "${WEB_SELECTOR_FILE}" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
    case "${WEB_CHOICE}" in
        nginx|openresty) : ;;
        *) echo "[!] Invalid webserver in ${WEB_SELECTOR_FILE}: '${WEB_CHOICE}', defaulting to nginx"; WEB_CHOICE="nginx" ;;
    esac
fi
echo "[i] Web server selection: ${WEB_CHOICE}"
install -D -m 0644 /dev/null "${WORKDIR}/etc/yuneta/webserver"
printf '%s\n' "${WEB_CHOICE}" > "${WORKDIR}/etc/yuneta/webserver"

# --- Copy yuneta_agent binaries (required) and create default config samples ---
# Binaries are pre-built into /yuneta/agent/ by `yunetas build`.
# Config samples are generic templates committed to the repo (no JWTs,
# no project-specific authz roles or jwks, owner placeholder is "owner").
# Project-level packaging that needs a populated config can either
# overlay /yuneta/agent/*.json on top of the .deb postinst, or wrap
# this script with its own templates.
AGENT_SRC_1="/yuneta/agent/yuneta_agent"
AGENT_SRC_2="/yuneta/agent/yuneta_agent22"
AGENT_SRC_4="/yuneta/agent/yuneta_agent44"
AGENT_JSON_1="${YUNETAS_BASE}/packages/templates/yuneta_agent.json.sample"
AGENT_JSON_2="${YUNETAS_BASE}/packages/templates/yuneta_agent22.json.sample"

if [ ! -x "${AGENT_SRC_1}" ]; then
    echo "[-] Missing or non-executable: ${AGENT_SRC_1}" >&2
    exit 2
fi
if [ ! -x "${AGENT_SRC_2}" ]; then
    echo "[-] Missing or non-executable: ${AGENT_SRC_2}" >&2
    exit 2
fi
if [ ! -x "${AGENT_SRC_4}" ]; then
    echo "[-] Missing or non-executable: ${AGENT_SRC_4}" >&2
    exit 2
fi

install -D -m 0755 "${AGENT_SRC_1}" "${WORKDIR}/yuneta/agent/yuneta_agent"
install -D -m 0755 "${AGENT_SRC_2}" "${WORKDIR}/yuneta/agent/yuneta_agent22"
install -D -m 0755 "${AGENT_SRC_4}" "${WORKDIR}/yuneta/agent/yuneta_agent44"

# Create default JSON samples (real files are created in postinst only if missing)
install -D -m 0644 "${AGENT_JSON_1}" "${WORKDIR}/yuneta/agent/yuneta_agent.json.sample"
install -D -m 0644 "${AGENT_JSON_2}" "${WORKDIR}/yuneta/agent/yuneta_agent22.json.sample"

# --- Copy agent certs directory (if present) ---
# Result: ${WORKDIR}/yuneta/agent/certs/...
copy_tree "/yuneta/agent/certs" "${WORKDIR}/yuneta/agent"

# --- Certs validator (installed into /yuneta/bin, scans current dir by default) ---
cat > "${WORKDIR}/yuneta/bin/check-certs-validity.sh" <<'EOF'
#!/usr/bin/env bash
#######################################################################
# check-certs-validity.sh
# Recursively list *.crt and show subject, dates, and expiry status.
#
# Usage:
#     check-certs-validity.sh                # scans . (current directory)
#     check-certs-validity.sh <path> [...]   # each path can be a dir or a file
#######################################################################
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

# --- Profile snippet (env, limits for shells, and aliases) ---
cat > "${WORKDIR}/etc/profile.d/yuneta.sh" <<'EOF'
#!/bin/sh
# Yuneta environment
export YUNETA_DIR=/yuneta
export PATH="/yuneta/bin:/usr/sbin:/sbin:/home/yuneta/.local/bin:$PATH"

# Raise core dump, open-files and locked-memory limits for interactive shells
# (Init/service scripts also raise limits before launching daemons).
# memlock is needed by any yuno started from a shell, ycommand included: its
# yev_loop pins io_uring ring memory against RLIMIT_MEMLOCK.
ulimit -c unlimited 2>/dev/null || true
ulimit -n unlimited 2>/dev/null || true
ulimit -Hn unlimited 2>/dev/null || true
ulimit -l unlimited 2>/dev/null || true

# Handy aliases, plus the yuno binaries and agent tools on PATH.
# /yuneta/development/yunetas is the single SDK base on every node: full
# source checkout on dev nodes, sparse SDK (outputs/, outputs_ext/, tools/,
# .config — no sources) staged by this package on runtime nodes.
alias y='cd /yuneta/development/yunetas'
alias salidas='cd /yuneta/development/yunetas/outputs'
alias outputs='cd /yuneta/development/yunetas/outputs'
export PATH="/yuneta/development/yunetas/outputs/yunos:/yuneta/development/yunetas/tools/agent:$PATH"

alias logs='cd /yuneta/realms/agent/logcenter/logs'
alias ll='ls -la'

EOF
chmod 0644 "${WORKDIR}/etc/profile.d/yuneta.sh"

# --- core_pattern, re-asserted after apport has had its turn ---
#
# On Ubuntu, apport.service starts AFTER systemd-sysctl.service and writes
# /proc/sys/kernel/core_pattern with its own pipe, so the value from
# /etc/sysctl.d/99-yuneta-core.conf survives the install (postinst runs
# `sysctl --system`) and is lost at the next boot. Since apport discards cores
# of binaries that did not come from a distro package, yuno cores then vanish
# with no diagnostic — the same shape as the /var/crash tug-of-war below: a
# one-shot setting losing to a later actor.
#
# Re-assert rather than fight: apport keeps reporting crashes for everything
# else, we just take core_pattern back afterwards. Ordering-only dependencies,
# so this is harmless on Debian, where apport does not exist.
# /usr/lib, not /lib: Ubuntu and Debian are merged-usr (/lib -> usr/lib), and
# this is where the unit actually lands — verified on the node, where systemd
# resolved the enable symlink to /usr/lib/systemd/system/. Shipping to /lib
# would work through the symlink but is the deprecated path.
mkdir -p "${WORKDIR}/usr/lib/systemd/system"
cat > "${WORKDIR}/usr/lib/systemd/system/yuneta-core-pattern.service" <<'EOF'
[Unit]
Description=Yuneta: re-apply kernel.core_pattern after apport
Documentation=https://doc.yuneta.io/entry-point
After=systemd-sysctl.service apport.service
ConditionPathExists=/etc/sysctl.d/99-yuneta-core.conf

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/sbin/sysctl -p /etc/sysctl.d/99-yuneta-core.conf

[Install]
WantedBy=multi-user.target
EOF
chmod 0644 "${WORKDIR}/usr/lib/systemd/system/yuneta-core-pattern.service"

# --- Core dump directory ownership, re-asserted at every boot ---
#
# /var/crash is not necessarily ours alone (on RHEL/Rocky kdump's kexec-tools
# owns it too, as root:root 0755), and a one-shot chmod/chown in postinst holds
# only until the next package touches it — after which cores silently stop
# being written. systemd-tmpfiles re-applies this on every boot.
mkdir -p "${WORKDIR}/usr/lib/tmpfiles.d"
cat > "${WORKDIR}/usr/lib/tmpfiles.d/yuneta-crash.conf" <<'EOF'
# Yuneta: core dumps land in /var/crash (see kernel.core_pattern in
# /etc/sysctl.d/99-yuneta-core.conf). Re-assert group write for 'yuneta' here —
# applied at boot and by `systemd-tmpfiles --create`.
d /var/crash 0775 root yuneta -
EOF
chmod 0644 "${WORKDIR}/usr/lib/tmpfiles.d/yuneta-crash.conf"

# --- Kernel tuning and Core dumps to /var/crash ---
cat > "${WORKDIR}/etc/sysctl.d/99-yuneta-core.conf" <<'EOF'
# Yuneta: TCP server tuning
# yuneta: write core files to /var/crash
# Install this file in /etc/sysctl.d/ and execute `sudo sysctl --system`
#

net.core.somaxconn = 65535
# net.ipv4.tcp_max_syn_backlog = 65535
# net.ipv4.tcp_syncookies = 1
# net.ipv4.tcp_synack_retries = 2

kernel.core_uses_pid = 0
kernel.core_pattern = /var/crash/core.%e
# Uncomment to allow dumps of setuid binaries (usually not needed)
# fs.suid_dumpable = 2

fs.file-max = 4000000
fs.nr_open  = 4000000

# Each tranger2 rt_disk follower (a non-master reader of a topic) uses one
# inotify instance; the 128 default is too low for a node running many yunos.
fs.inotify.max_user_instances = 4096
fs.inotify.max_user_watches = 524288

# Defensive cushion above the 16384 kernel default: on IN_Q_OVERFLOW fs_watcher
# aborts the yuno (ydaemon relaunches it with a clean reload), so a bigger
# per-instance queue makes that bounce less likely under CREATE/DELETE bursts.
fs.inotify.max_queued_events = 65536

EOF
chmod 0644 "${WORKDIR}/etc/sysctl.d/99-yuneta-core.conf"

# limits drop-in (prefer over editing /etc/security/limits.conf)
cat > "${WORKDIR}/etc/security/limits.d/99-yuneta-core.conf" <<'EOF'
# Yuneta: file limits tuning
# Install this file in /etc/security/limits.d
# and add to files /etc/pam.d/common-session and /etc/pam.d/common-session-noninteractive
# the line "session required pam_limits.so"

yuneta  soft    core    unlimited
yuneta  hard    core    unlimited
yuneta  soft    nofile  unlimited
yuneta  hard    nofile  unlimited

# memlock: io_uring rings are pinned memory charged against RLIMIT_MEMLOCK,
# and the budget is per USER, shared by every yuno running as yuneta.
# A yuno with io_uring_entries=32768 pins ~3.2 MB (SQEs 32768*64 + CQEs
# 65536*16 + SQ array), so the usual 8 MB default only admits two of them:
# the third yuno onwards dies at startup with ENOMEM in yev_loop_create(),
# and the ydaemon watcher relaunches it forever. Seen on a fresh node with
# 7 GB of free RAM — it is a limit, never a shortage of memory.
yuneta  soft    memlock unlimited
yuneta  hard    memlock unlimited
EOF
chmod 0644 "${WORKDIR}/etc/security/limits.d/99-yuneta-core.conf"

# --- SysV init script (adds hard runtime limits before starting) ---
cat > "${WORKDIR}/etc/init.d/yuneta_agent" <<'EOF'
#!/bin/sh
### BEGIN INIT INFO
# Provides:          yuneta_agent
# Required-Start:    $local_fs $remote_fs $network $time $syslog $named
# Required-Stop:     $local_fs $remote_fs $network $time $syslog $named
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Yuneta Agent service
# Description:       Start/stop Yuneta Agent yunos and selected web server (nginx or openresty)
### END INIT INFO

# Keep it simple: Yuneta handles its own daemon/watchdog. No start-stop-daemon here.
set -eu

YUNETA_DIR="/yuneta"
RUN_AS="yuneta"

AGENT1_BIN="${YUNETA_DIR}/agent/yuneta_agent"
AGENT1_CFG="${YUNETA_DIR}/agent/yuneta_agent.json"

AGENT2_BIN="${YUNETA_DIR}/agent/yuneta_agent22"
AGENT2_CFG="${YUNETA_DIR}/agent/yuneta_agent22.json"

if [ -r /lib/lsb/init-functions ]; then
    . /lib/lsb/init-functions
else
    log_daemon_msg() { echo "$@"; }
    log_end_msg() { [ "$1" -eq 0 ] && echo "OK" || echo "FAIL ($1)"; }
fi

_set_limits() {
    # Core dumps
    ulimit -c unlimited || true
    # io_uring rings are pinned memory shared per user (see the memlock note in
    # /etc/security/limits.d/99-yuneta-core.conf). pam_limits does not reach
    # every boot path that starts this script, so raise it here as well.
    ulimit -l unlimited 2>/dev/null || true
    # Try to raise the hard limit, then set the soft limit
    TARGET=200000
    HARD="$(ulimit -Hn 2>/dev/null || echo 0)"
    case "$HARD" in
        unlimited) : ;;
        *) ulimit -Hn "$TARGET" 2>/dev/null || true ;;
    esac
    ulimit -n "$TARGET" 2>/dev/null || ulimit -n 65535 2>/dev/null || true
}

_run_as_yuneta() {
    su -s /bin/sh - "$RUN_AS" -c "$*"
}

start_yunos() {
    RC1=1
    RC2=1
    _set_limits

    if [ -x "$AGENT1_BIN" ]; then
        log_daemon_msg "Starting yuneta_agent"
        _run_as_yuneta "exec /yuneta/agent/yuneta_agent --config-file=/yuneta/agent/yuneta_agent.json --start" && RC1=0 || RC1=$?
        log_end_msg $RC1
        logger -t yuneta_agent_init "start yuneta_agent rc=$RC1"
    else
        echo "WARN: $AGENT1_BIN not found or not executable" >&2
        logger -t yuneta_agent_init "start yuneta_agent skipped: not executable"
    fi

    if [ -x "$AGENT2_BIN" ]; then
        log_daemon_msg "Starting yuneta_agent22"
        _run_as_yuneta "exec /yuneta/agent/yuneta_agent22 --config-file=/yuneta/agent/yuneta_agent22.json --start" && RC2=0 || RC2=$?
        log_end_msg $RC2
        logger -t yuneta_agent_init "start yuneta_agent22 rc=$RC2"
    else
        echo "WARN: $AGENT2_BIN not found or not executable" >&2
        logger -t yuneta_agent_init "start yuneta_agent22 skipped: not executable"
    fi

    if [ "$RC1" -eq 0 ] || [ "$RC2" -eq 0 ]; then
        return 0
    fi
    return 1
}

# ---------------- Web server selection + control ----------------
# Default nginx unless /etc/yuneta/webserver says otherwise (openresty)
WEBCONF="/etc/yuneta/webserver"
WEBTYPE="nginx"
if [ -r "$WEBCONF" ]; then
    WEBTYPE="$(tr '[:upper:]' '[:lower:]' < "$WEBCONF" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
fi

NGINX_BIN="${YUNETA_DIR}/bin/nginx/sbin/nginx"
OPENRESTY_BIN="${YUNETA_DIR}/bin/openresty/bin/openresty"

start_web() {
    case "$WEBTYPE" in
        openresty)
            if [ -x "$OPENRESTY_BIN" ]; then
                log_daemon_msg "Starting OpenResty"
                "$OPENRESTY_BIN" || true
                log_end_msg 0
            else
                echo "WARN: OpenResty binary not found: $OPENRESTY_BIN" >&2
            fi
            ;;
        nginx|*)
            if [ -x "$NGINX_BIN" ]; then
                log_daemon_msg "Starting Nginx"
                "$NGINX_BIN" || true
                log_end_msg 0
            else
                echo "WARN: Nginx binary not found: $NGINX_BIN" >&2
            fi
            ;;
    esac
}

stop_web() {
    case "$WEBTYPE" in
        openresty)
            if pidof openresty >/dev/null 2>&1; then
                log_daemon_msg "Stopping OpenResty"
                "$OPENRESTY_BIN" -s quit || true
                log_end_msg 0
            fi
            ;;
        nginx|*)
            if pidof nginx >/dev/null 2>&1; then
                log_daemon_msg "Stopping Nginx"
                "$NGINX_BIN" -s quit || true
                log_end_msg 0
            fi
            ;;
    esac
}

status_web() {
    case "$WEBTYPE" in
        openresty)
            if pidof openresty >/dev/null 2>&1; then
                echo "openresty: running;"
                return 0
            fi
            ;;
        nginx|*)
            if pidof nginx >/dev/null 2>&1; then
                echo "nginx: running;"
                return 0
            fi
            ;;
    esac
    echo "web: not running;"
    return 3
}
# ----------------------------------------------------------------

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
        if pidof -x yuneta_agent >/dev/null 2>&1; then MSG="$MSG yuneta_agent: running;"; S=0; else MSG="$MSG yuneta_agent: not running;"; fi
        if pidof -x yuneta_agent22 >/dev/null 2>&1; then MSG="$MSG yuneta_agent22: running;"; S=0; else MSG="$MSG yuneta_agent22: not running;"; fi
    fi

    echo "$MSG"
    return $S
}

stop_yunos() {
    RC1=0
    RC2=0

    if [ -x "$AGENT1_BIN" ]; then
        log_daemon_msg "Stopping yuneta_agent"
        _run_as_yuneta "exec /yuneta/agent/yuneta_agent --config-file=/yuneta/agent/yuneta_agent.json --stop" && RC1=0 || RC1=$?
        log_end_msg $RC1
        logger -t yuneta_agent_init "stop yuneta_agent rc=$RC1"
    else
        logger -t yuneta_agent_init "stop yuneta_agent skipped: not executable"
    fi

    return 0
}

case "$1" in
    start)
        start_yunos
        start_web
        ;;
    stop)
        stop_web
        stop_yunos
        ;;
    restart|force-reload)
        stop_web || true
        stop_yunos || true
        start_yunos
        start_web
        ;;
    status)
        status_yunos
        status_web
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|force-reload|status}"
        exit 2
        ;;
esac

exit 0
EOF
chmod 0755 "${WORKDIR}/etc/init.d/yuneta_agent"

# Mirror init script into service dir with perms (handy helpers)
install -m 0755 "${WORKDIR}/etc/init.d/yuneta_agent" "${WORKDIR}/yuneta/agent/service/yuneta_agent"

# --- control file ---
cat > "${WORKDIR}/DEBIAN/control" <<EOF
Package: ${PROJECT}
Version: ${VERSION}-${RELEASE}
Section: utils
Priority: optional
Architecture: ${ARCHITECTURE}
Homepage: https://yuneta.io
Maintainer: ArtGins S.L. <support@artgins.com>
Depends: adduser, lsb-base, rsync, locales, rsyslog, init-system-helpers, gdb
Recommends: curl, vim, sudo, tree, pipx, fail2ban, net-tools, locate
Suggests: git, mercurial, make, cmake, ninja-build, gcc, clang, g++, python3-dev, python3-pip, python3-setuptools, python3-tk, python3-wheel, python3-venv, libjansson-dev, libpcre2-dev, liburing-dev, libcurl4-openssl-dev, zlib1g-dev, libssl-dev, perl, dos2unix, postgresql-server-dev-all, libpq-dev, kconfig-frontends, telnet, patch, gettext, snapd
Description: Yuneta's Agent
 Yuneta Agent binaries, runtime directories, SysV service, certbot hooks and helpers.
EOF

# --- conffiles (mark config files; dpkg removes these only on purge) ---
cat > "${WORKDIR}/DEBIAN/conffiles" <<'EOF'
/etc/profile.d/yuneta.sh
/etc/sudoers.d/90-yuneta
/etc/init.d/yuneta_agent
/etc/letsencrypt/renewal-hooks/deploy/reload-certs
EOF

if [ -f "${WORKDIR}/etc/yuneta/authorized_keys" ]; then
    echo "/etc/yuneta/authorized_keys" >> "${WORKDIR}/DEBIAN/conffiles"
fi
if [ -f "${WORKDIR}/etc/yuneta/webserver" ]; then
    echo "/etc/yuneta/webserver" >> "${WORKDIR}/DEBIAN/conffiles"
fi

# --- helper scripts (installed under /yuneta/agent/service) ---
cat > "${WORKDIR}/yuneta/agent/service/install-yuneta-service.sh" <<'EOF'
#!/bin/sh
#######################################################################
# Install SysV service: place init script and enable/start it
#######################################################################
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
#######################################################################
# Disable SysV service: remove runlevel symlinks (leave conffile in place)
#######################################################################
set -eu
if command -v invoke-rc.d >/dev/null 2>&1; then
    invoke-rc.d yuneta_agent stop || true
else
    /etc/init.d/yuneta_agent stop || true
fi
if command -v systemctl >/dev/null 2>&1; then
    systemctl disable --now yuneta-core-pattern.service >/dev/null 2>&1 || true
fi
if [ -x /usr/sbin/update-rc.d ]; then
    /usr/sbin/update-rc.d -f yuneta_agent remove >/dev/null 2>&1 || true
fi
exit 0
EOF
chmod 0755 "${WORKDIR}/yuneta/agent/service/remove-yuneta-service.sh"

# --- Certbot installer (ops helper) ---
# Same path on both families (`install-certbot.sh`), so operators and install.sh
# have one name to know; only the mechanism differs (snap here, EPEL dnf on
# RHEL). `install-certbot-snap.sh` stays as a symlink — it is the name shipped
# up to 7.8.5 and it appears in operators' notes.
cat > "${WORKDIR}/yuneta/bin/install-certbot.sh" <<'EOF'
#!/usr/bin/env bash
#######################################################################
# Install certbot from Snap and make it visible at /usr/bin/certbot
#######################################################################
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

# Ensure snapd is present (Debian/Ubuntu)
if ! command -v snap >/dev/null 2>&1; then
    if command -v apt-get >/dev/null 2>&1; then
        apt-get update -y || true
        apt-get install -y --no-install-recommends snapd
        # Ensure snapd services are up before using snap
        systemctl enable --now snapd snapd.apparmor 2>/dev/null || true
    else
        echo "apt-get not found; cannot install snapd automatically." >&2
        exit 1
    fi
fi

# Wait for snapd to finish seeding before asking it for anything.
#
# This used to be `sleep 3` plus `snap wait ... 2>/dev/null || true`: the one
# safeguard against the race, silenced. When snapd cannot seed, every later
# command fails with something that looks unrelated -- "context canceled" from
# a store POST -- and the real cause never reaches the operator. Bounded,
# because `snap wait` has no timeout of its own and a broken snapd would
# otherwise hang the whole installer.
echo "[i] Waiting for snapd to finish seeding…"
if ! timeout 180 snap wait system seed.loaded; then
    echo "[!] snapd did not finish seeding within 180s; continuing anyway." >&2
fi

# Install core + certbot, retrying. snapd restarts itself right after being
# installed, and a store request in flight when that happens dies with
# "context canceled". These operations are idempotent, so retrying is safe.
snap_try() {
    _n=0
    while :; do
        if snap "$@"; then
            return 0
        fi
        _n=$((_n + 1))
        if [ "$_n" -ge 3 ]; then
            return 1
        fi
        echo "[i] 'snap $1' failed (attempt ${_n}/3); retrying in 5s…" >&2
        sleep 5
    done
}

# core is not fatal on its own: certbot pulls what it needs. It is still
# reported, because it failing is the first sign that snapd is unusable.
snap_try install core || echo "[!] 'snap install core' failed; see the error above." >&2
snap_try refresh core || true

if ! snap_try install --classic certbot; then
    cat >&2 <<'CERTBOT_FAIL'

[!] certbot could NOT be installed from snap. The error above is the real one:
      "context canceled"        -> snapd aborted the request: it restarts just
                                   after being installed, or it never seeded.
      dial tcp / no such host   -> this machine cannot reach api.snapcraft.io.
      deadline exceeded         -> it can reach it, but something drops it.

    Diagnose before retrying:
      uname -r                     # an OVH kernel may lack what snapd needs
      snap debug sandbox-features  # snapd says what the kernel denies it
      snap changes
      journalctl -u snapd -n 80

    Debian also packages certbot (`apt install certbot`), but READ THIS FIRST:
    the distro package has NO dns-ovh plugin. Debian ships cloudflare, desec,
    google, infomaniak, rfc2136, route53 and standalone only, so a node whose
    certificates use `authenticator = dns-ovh` CANNOT renew them with it. It
    also takes over /usr/bin/certbot and adds a second renewal timer next to
    snap's, both pointed at the same /etc/letsencrypt.
CERTBOT_FAIL
    exit 1
fi

# Make certbot visible even if /snap/bin isn't in PATH
if [ -x /snap/bin/certbot ]; then
    if [ ! -e /usr/bin/certbot ]; then
        ln -s /snap/bin/certbot /usr/bin/certbot
    elif [ ! -L /usr/bin/certbot ]; then
        echo "[!] /usr/bin/certbot is a real file, not a symlink to the snap:" >&2
        echo "    another certbot (most likely the apt package) owns it. Two" >&2
        echo "    certbots renewing the same /etc/letsencrypt is a hazard, and" >&2
        echo "    the apt one cannot use dns-ovh. Keep one of them." >&2
    fi
fi

# Ensure deploy-hook directory exists (also created by the .deb)
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

  # Webroot (recommended if your Yuneta nginx/openresty serves HTTP):
  # 1) In your web server block, add:
  #    location /.well-known/acme-challenge/ {
  #        root /yuneta/share/certbot-webroot;
  #        default_type text/plain;
  #    }
  #    (reload web server)
  # 2) Create the webroot dir:
  #    mkdir -p /yuneta/share/certbot-webroot
  # 3) Issue:
  #    /usr/bin/certbot certonly --webroot -w /yuneta/share/certbot-webroot -d example.com

Your .deb already installs a deploy hook at:
/etc/letsencrypt/renewal-hooks/deploy/reload-certs
It copies renewed certs to /yuneta/store/certs/, reloads the selected web server, and restarts Yuneta.

DNS-01 with a provider plugin (dns-ovh, dns-cloudflare, dns-route53, ...):
only plain certbot is installed above, the plugins are separate snaps, and
the FIRST of these two commands is the one nobody guesses -- without it the
install fails with a message about trusting the plugin author, which snap
prints inside a box that is easy to miss:

  snap set certbot trust-plugin-with-root=ok
  snap install certbot-dns-<provider>

That acknowledges the plugin runs as root. Verify with `certbot plugins`;
the plugin must appear there, and `snap connections certbot` must show
certbot:plugin joined to the plugin snap.

Note this is where Debian and RHEL genuinely differ: the RHEL side gets its
plugins as ordinary rpms (`dnf install python3-certbot-dns-<provider>`),
because EPEL packages them. The Debian archive does not package all of them
-- dns-ovh, for one, exists nowhere but the snap.
HINT
EOF
chmod 0755 "${WORKDIR}/yuneta/bin/install-certbot.sh"
ln -sf install-certbot.sh "${WORKDIR}/yuneta/bin/install-certbot-snap.sh"

# --- Let's Encrypt: deploy hook to copy + reload ---
# Each step runs independently: individual failures are logged but do NOT
# abort the hook (set +e) so that one broken yuno does not block the rest.
# The hook touches /var/lib/yuneta/last-deploy-hook-run on success so an
# internal auto-sync monitor can detect hooks that never fired.
cat > "${WORKDIR}/etc/letsencrypt/renewal-hooks/deploy/reload-certs" <<'EOF'
#!/bin/sh
#######################################################################
# Triggered by certbot after successful renewal.
# 1) Copies fresh certs to /yuneta/store/certs
# 2) Reloads selected web server (nginx or openresty)
# 3) Hot-reloads TLS certificates in every running yuno via ycommand
#    (no process restart, active TLS connections are preserved).
#######################################################################
set -u    # fail on unset vars, but NOT on individual command failures

LOG_DIR="/var/log/yuneta"
mkdir -p "$LOG_DIR"

STATE_DIR="/var/lib/yuneta"
mkdir -p "$STATE_DIR"

ts() { date '+%Y-%m-%dT%H:%M:%S%z'; }
log() { printf '[%s] %s\n' "$(ts)" "$*" >>"$LOG_DIR/deploy-hook.log"; }

log "deploy hook started (RENEWED_LINEAGE=${RENEWED_LINEAGE:-?} RENEWED_DOMAINS=${RENEWED_DOMAINS:-?})"

# 1) Copy fresh certs into /yuneta/store/certs (synchronous, requires root)
if [ -x /yuneta/store/certs/copy-certs.sh ]; then
    if /yuneta/store/certs/copy-certs.sh >>"$LOG_DIR/copy-certs.log" 2>&1; then
        log "copy-certs.sh OK"
    else
        log "copy-certs.sh FAILED (rc=$?)"
    fi
else
    log "copy-certs.sh NOT FOUND — skipping"
fi

# 2) Reload web server to pick updated certs
WEBCONF="/etc/yuneta/webserver"
WEBTYPE="nginx"
if [ -r "$WEBCONF" ]; then
    WEBTYPE="$(tr '[:upper:]' '[:lower:]' < "$WEBCONF" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
fi

case "$WEBTYPE" in
    openresty)
        if [ -x /yuneta/bin/openresty/bin/openresty ]; then
            if /yuneta/bin/openresty/bin/openresty -s reload >>"$LOG_DIR/deploy-hook.log" 2>&1; then
                log "openresty reload OK"
            else
                log "openresty reload FAILED (rc=$?)"
            fi
        else
            log "openresty binary NOT FOUND"
        fi
        ;;
    nginx|*)
        if [ -x /yuneta/bin/nginx/sbin/nginx ]; then
            if /yuneta/bin/nginx/sbin/nginx -s reload >>"$LOG_DIR/deploy-hook.log" 2>&1; then
                log "nginx reload OK"
            else
                log "nginx reload FAILED (rc=$?)"
            fi
        else
            log "nginx binary NOT FOUND"
        fi
        ;;
esac

# 3) Hot-reload TLS certs in every running yuno via ycommand
#    ycommand must run as the yuneta user so it can authenticate with its
#    own token. Using sudo -u also picks up yuneta's PATH & HOME.
if [ -x /yuneta/bin/ycommand ]; then
    if sudo -u yuneta -H /yuneta/bin/ycommand \
            -c 'command-yuno command=reload-certs service=__yuno__' \
            >>"$LOG_DIR/deploy-hook.log" 2>&1; then
        log "ycommand reload-certs OK"
    else
        rc=$?
        log "ycommand reload-certs FAILED (rc=$rc) — falling back to restart-yuneta"
        if [ -x /yuneta/bin/restart-yuneta ]; then
            nohup /yuneta/bin/restart-yuneta >>"$LOG_DIR/restart-yuneta.log" 2>&1 &
            log "restart-yuneta launched in background"
        else
            log "restart-yuneta NOT FOUND — certs copied but yunos still using old certs"
        fi
    fi
else
    log "ycommand NOT FOUND — falling back to restart-yuneta"
    if [ -x /yuneta/bin/restart-yuneta ]; then
        nohup /yuneta/bin/restart-yuneta >>"$LOG_DIR/restart-yuneta.log" 2>&1 &
    fi
fi

# Record that the hook ran (auto-sync monitor looks at this timestamp)
date '+%s' > "$STATE_DIR/last-deploy-hook-run" 2>/dev/null || true
log "deploy hook finished"

exit 0
EOF
chmod 0755 "${WORKDIR}/etc/letsencrypt/renewal-hooks/deploy/reload-certs"

# --- Restart helper: prefer SysV restart when root; fallback otherwise ---
cat > "${WORKDIR}/yuneta/bin/restart-yuneta" <<'EOF'
#!/usr/bin/env bash
#######################################################################
# Restart Yuneta stack. If root, prefer service restart; otherwise
# fallback to yshutdown + agent start.
#######################################################################
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

# --- Helper: check queues of messages ---
cat > "${WORKDIR}/yuneta/bin/colas2.sh" <<'EOF'
#!/usr/bin/env bash
#######################################################################
# colas2.sh
# Scan two-level queue dirs and call list_queue_msgs2 on each.
#
# Usage:
#   colas2.sh                       # scan default base
#   colas2.sh <base_dir> [args...]  # scan <base_dir> and pass [args...] to list_queue_msgs2
#######################################################################
set -euo pipefail

LIST_BIN="/yuneta/bin/list_queue_msgs2"

# Determine base dir and the rest of args to forward
BASE="/yuneta/store/queues/gate_msgs2"
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

# Iterate two levels: <BASE>/<QUEUE>/<QUEUE2>/
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

# --- Dev stack installer (optional helper) ---
cat > "${WORKDIR}/yuneta/bin/install-yuneta-dev-deps.sh" <<'EOF'
#!/usr/bin/env bash
#######################################################################
# Install recommended developer dependencies for building new Yunos
#######################################################################
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

# One apt update up-front
if command -v apt-get >/dev/null 2>&1; then
    apt-get update -y || true
else
    echo "apt-get not found; this helper supports Debian/Ubuntu systems only." >&2
    exit 1
fi

# Packages requested (installed individually to tolerate missing ones)
PKGS=(
    git mercurial make cmake ninja-build
    gcc clang g++
    python3-dev python3-pip python3-setuptools
    python3-tk python3-wheel python3-venv
    libjansson-dev libpcre2-dev liburing-dev libcurl4-openssl-dev
    zlib1g-dev libssl-dev
    perl dos2unix tree curl wget
    postgresql-server-dev-all libpq-dev
    kconfig-frontends telnet pipx
    patch gettext fail2ban rsync
)

# Extra commonly-needed build meta packages
PKGS+=(build-essential pkg-config ca-certificates linux-libc-dev)

echo "[i] Installing dev packages (no recommends)…"
FAILED=()
for p in "${PKGS[@]}"; do
    echo " -> $p"
    if ! apt-get install -y --no-install-recommends "$p"; then
        echo "[!] failed: $p (continuing)"
        FAILED+=("$p")
    fi
done

# Refresh CA bundle if available (helps TLS in builds/fetches)
if command -v update-ca-certificates >/dev/null 2>&1; then
    update-ca-certificates || true
fi

# pipx CLIs (kconfiglib for menuconfig, yunetas build CLI). This script runs
# as root, but the CLIs must land in the OPERATOR's ~/.local/bin — the staged
# profile.d/yuneta.sh already puts /home/yuneta/.local/bin on PATH. Install
# for 'yuneta' when it exists, else for the sudo caller, else for root.
if command -v pipx >/dev/null 2>&1; then
    pipx_user="root"
    if id yuneta >/dev/null 2>&1; then
        pipx_user="yuneta"
    elif [ -n "${SUDO_USER:-}" ] && [ "${SUDO_USER}" != "root" ]; then
        pipx_user="${SUDO_USER}"
    fi
    run_pipx() {
        if [ "${pipx_user}" = "root" ]; then
            pipx "$@"
        else
            runuser -l "${pipx_user}" -c "pipx $*"
        fi
    }
    echo "[i] Installing kconfiglib via pipx (user: ${pipx_user})…"
    run_pipx install --include-deps kconfiglib || echo "[!] pipx install kconfiglib failed (continuing)"
    echo "[i] Installing yunetas via pipx (user: ${pipx_user})…"
    run_pipx install --include-deps yunetas || echo "[!] pipx install yunetas failed (continuing)"
else
    echo "[!] pipx not installed; kconfiglib and yunetas not installed." >&2
fi

if [ "${#FAILED[@]}" -eq 0 ]; then
    echo "[✓] Dev environment ready — all ${#PKGS[@]} packages installed."
else
    echo "[!] ${#FAILED[@]} package(s) NOT installed: ${FAILED[*]}" >&2
fi
EOF
chmod 0755 "${WORKDIR}/yuneta/bin/install-yuneta-dev-deps.sh"

# --- Copy-certs helper: root-only, supports certs.list or auto-discovery ---
cat > "${WORKDIR}/yuneta/store/certs/copy-certs.sh" <<'EOF'
#!/usr/bin/env bash
#######################################################################
# Copy Let's Encrypt certs from /etc/letsencrypt/live/* into
# /yuneta/store/certs (and private keys into /yuneta/store/certs/private)
#
# - Must run as root (to read privkey.pem)
# - Uses /yuneta/store/certs/certs.list if present, else auto-discovers all
#######################################################################
set -euo pipefail

# Change to the directory where this script is located
SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" >/dev/null 2>&1 && pwd -P)"
cd "${SCRIPT_DIR}"

# Must run as root (privkey.pem is root-only in /etc/letsencrypt/live)
if [ "${EUID:-$(id -u)}" -ne 0 ]; then
    echo "This script must run as root (to read /etc/letsencrypt/live/*/privkey.pem)" >&2
    exit 1
fi

DEST_BASE="/yuneta/store/certs"
DEST_PRIV="${DEST_BASE}/private"
CERTS_FILE="${DEST_BASE}/certs.list"

# Ensure destinations exist with safe permissions
install -d -m 0755 "${DEST_BASE}"
install -d -m 0700 "${DEST_PRIV}"

# Build list of certificate names: prefer certs.list, else auto-discover
CERT_NAMES=""
if [ -s "${CERTS_FILE}" ]; then
    # read non-empty, non-comment lines
    while IFS= read -r line; do
        case "$line" in
            ''|\#*) continue ;;
            *) CERT_NAMES="${CERT_NAMES} ${line}" ;;
        esac
    done < "${CERTS_FILE}"
else
    # auto-detect all directories with live certs
    if [ -d /etc/letsencrypt/live ]; then
        for d in /etc/letsencrypt/live/*; do
            [ -d "$d" ] || continue
            name="$(basename "$d")"
            CERT_NAMES="${CERT_NAMES} ${name}"
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

    # install -C compares and skips the copy (incl. mtime bump) when the
    # content is already identical. This is critical for the agent's
    # cert-sync timer: every spurious copy bumps the .crt mtime and the
    # agent (which snapshots size:mtime) broadcasts a reload-certs to every
    # yuno on every 15-min tick. Two coreutils footguns make -C re-copy
    # unconditionally unless handled:
    #
    #   1) Symlink source: letsencrypt's live/*.pem are symlinks into
    #      ../archive. `install --compare` lstat()s the source and NEVER
    #      skips when it is not a regular file. Resolve with readlink -f so
    #      -C compares the real file content.
    #   2) Owner mismatch: this script runs as root, so set the final owner
    #      with -o/-g *inside* install. A trailing `chown yuneta` would leave
    #      the dest owned by yuneta while -C assumes a root-owned target, so
    #      the owner mismatch would force a re-copy on the next run too.
    FULLCHAIN_R="$(readlink -f "${FULLCHAIN}")"
    CHAIN_R="$(readlink -f "${CHAIN}")"
    PRIVKEY_R="$(readlink -f "${PRIVKEY}")"
    install -C -o yuneta -g yuneta -m 0644 "${FULLCHAIN_R}" "${DEST_BASE}/${CERT}.crt"
    install -C -o yuneta -g yuneta -m 0644 "${CHAIN_R}"     "${DEST_BASE}/${CERT}.chain"
    install -C -o yuneta -g yuneta -m 0600 "${PRIVKEY_R}"   "${DEST_PRIV}/${CERT}.key"
    echo "Copied ${CERT} -> ${DEST_BASE}/{${CERT}.crt, ${CERT}.chain, private/${CERT}.key}"
done

# Quick validity summary if tool is present
if [ -x /yuneta/bin/check-certs-validity.sh ]; then
    /yuneta/bin/check-certs-validity.sh "${DEST_BASE}" || true
fi

exit 0
EOF
chmod 0755 "${WORKDIR}/yuneta/store/certs/copy-certs.sh"

# --- Provide a commented template certs.list (admin can edit later) ---
# By default use auto-discover
#cat > "${WORKDIR}/yuneta/store/certs/certs.list" <<'EOF'
## One certificate name per line (matching directory name under /etc/letsencrypt/live)
## Example:
## example.com
## api.example.com
#EOF
#chmod 0644 "${WORKDIR}/yuneta/store/certs/certs.list"

# --- Make yuneta sudo (NOPASSWD) ---
cat > "${WORKDIR}/etc/sudoers.d/90-yuneta" <<'EOF'
# User rules for yuneta
yuneta ALL=(ALL) NOPASSWD:ALL
EOF
chmod 0440 "${WORKDIR}/etc/sudoers.d/90-yuneta"

# --- postinst (create login user, add to wide groups, locales, syslog, SysV enable) ---
cat > "${WORKDIR}/DEBIAN/postinst" <<'EOF'
#!/bin/sh
#######################################################################
# postinst
# - Ensure 'yuneta' login user with home /home/yuneta (bash shell)
# - Add 'yuneta' to wide set of system-access groups (dialout, tty, netdev, etc.)
# - Create agent configs if missing (do not overwrite)
# - Install authorized_keys
# - Ensure locales (en_US.UTF-8, es_ES.UTF-8)
# - Ensure rsyslog (/var/log/syslog)
# - Enable/start SysV service (idempotent)
#######################################################################
set -eu

ACTION="${1:-}"  # <-- capture dpkg action BEFORE any manipulation

info() { echo "[postinst] $*"; }
warn() { echo "[postinst] WARNING: $*" >&2; }

# Create 'yuneta' login user if missing
if ! id -u yuneta >/dev/null 2>&1; then
    info "Creating login user 'yuneta' (non-system)…"
    adduser --home /home/yuneta --shell /bin/bash --gecos "Yuneta User" --disabled-password yuneta || true
else
    UID_NOW="$(id -u yuneta || echo 0)"
    if [ "$UID_NOW" -lt 1000 ]; then
        warn "'yuneta' exists with system UID ($UID_NOW < 1000). Keeping UID but granting full groups + sudo."
    fi
fi

# Ensure home exists and owned by user
install -d -o yuneta -g yuneta -m 0755 /home/yuneta || true

# Add 'yuneta' to a broad, practical set of system-access groups
DEFAULT_GROUPS="adm,sudo,tty,dialout,cdrom,audio,video,plugdev,netdev,render,input,gpio,i2c,spi,uucp,wireshark,bluetooth,scanner,lp,lpadmin,sambashare,docker,libvirt,kvm,lxd"
GROUPS="${YUNETA_GROUPS:-$DEFAULT_GROUPS}"
CREATE_MISSING="${YUNETA_CREATE_MISSING_GROUPS:-0}"

# SAFE iteration: do NOT use `set --` (it would clobber $1)
OLDIFS="$IFS"
IFS=','; set -f
for grp in $GROUPS; do
    [ -z "$grp" ] && continue
    if getent group "$grp" >/dev/null 2>&1; then
        usermod -aG "$grp" yuneta || true
        # info "Added 'yuneta' to group: $grp"
    else
        if [ "$CREATE_MISSING" = "1" ]; then
            info "Creating missing group: $grp"
            groupadd "$grp" || true
            usermod -aG "$grp" yuneta || true
            info "Added 'yuneta' to newly created group: $grp"
        fi
    fi
done
set +f; IFS="$OLDIFS"

# Ensure agent configs exist without overwriting existing ones.
# The default node_owner is "none": a standalone node with no
# controlcenter. The agent only dials the controlcenter when
# node_owner != "none" (see c_agent.c mt_start), so "none" keeps a
# fresh box quiet instead of looping on getaddrinfo() for a URL that
# does not resolve. Operators WITH a controlcenter set it as:
#   YUNETA_OWNER=mycompany sudo apt install ./yuneta-agent.deb
# which substitutes "none" with their owner. The .json files are
# conffiles and are never overwritten on upgrade.
if [ ! -e /yuneta/agent/yuneta_agent.json ]; then
    if [ -e /yuneta/agent/yuneta_agent.json.sample ]; then
        install -o yuneta -g yuneta -m 0644 -T \
            /yuneta/agent/yuneta_agent.json.sample /yuneta/agent/yuneta_agent.json
    else
        printf '{}\n' > /yuneta/agent/yuneta_agent.json
        chown yuneta:yuneta /yuneta/agent/yuneta_agent.json
        chmod 0644 /yuneta/agent/yuneta_agent.json
    fi
    if [ -n "${YUNETA_OWNER:-}" ]; then
        sed -i "s|\"node_owner\": \"none\"|\"node_owner\": \"${YUNETA_OWNER}\"|" \
            /yuneta/agent/yuneta_agent.json
        info "node_owner set to '${YUNETA_OWNER}' in yuneta_agent.json"
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
    if [ -n "${YUNETA_OWNER:-}" ]; then
        sed -i "s|\"node_owner\": \"none\"|\"node_owner\": \"${YUNETA_OWNER}\"|" \
            /yuneta/agent/yuneta_agent22.json
    fi
fi

# Locales: ensure en_US.UTF-8 and es_ES.UTF-8
if command -v locale-gen >/dev/null 2>&1; then
    sed -i 's/^[# ]*en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen || true
    sed -i 's/^[# ]*es_ES.UTF-8 UTF-8/es_ES.UTF-8 UTF-8/' /etc/locale.gen || true
    grep -q '^en_US.UTF-8 UTF-8' /etc/locale.gen || echo 'en_US.UTF-8 UTF-8' >> /etc/locale.gen
    grep -q '^es_ES.UTF-8 UTF-8' /etc/locale.gen || echo 'es_ES.UTF-8 UTF-8' >> /etc/locale.gen
    locale-gen || true
    update-locale LANG=en_US.UTF-8 LANGUAGE="en_US:es_ES" || true
fi

# Ownership/perms for Yuneta dirs
if [ -d /yuneta ]; then
    chown -R yuneta:yuneta /yuneta || true
fi
if [ -d /yuneta/store/certs ]; then
    chmod 0755 /yuneta/store/certs || true
fi
if [ -d /yuneta/store/certs/private ]; then
    chmod 0700 /yuneta/store/certs/private || true
fi

if [ -d /var/crash ]; then
    chmod 0775 /var/crash || true
    chown root:yuneta /var/crash || true
fi
# Re-assert it now, and let systemd-tmpfiles do it again on every boot.
if command -v systemd-tmpfiles >/dev/null 2>&1; then
    systemd-tmpfiles --create /usr/lib/tmpfiles.d/yuneta-crash.conf >/dev/null 2>&1 || true
fi

# --- apport: turn it off, it has no mission on a Yuneta node ---
#
# apport is Ubuntu's crash TELEMETRY client: it captures crashes, turns them
# into deduplicated reports and ships them to errors.ubuntu.com so Canonical
# can find bugs in the packages Ubuntu ships. Its beneficiary is the
# distribution, not this machine. It grabs kernel.core_pattern to do that, and
# it deliberately DISCARDS crashes of binaries outside its packaging allowlist
# (/bin /boot /etc /initrd /lib /sbin /opt /usr /var) — /yuneta is not on it,
# so every yuno core is dropped, silently, with no report and no file.
#
# On a dedicated Yuneta node that trade is all cost: there is no desktop, no
# one to approve an upload, and its telemetry helps nobody here. Ubuntu Server
# images usually ship it disabled for the same reason.
if [ -f /etc/default/apport ]; then
    sed -i 's/^enabled=1/enabled=0/' /etc/default/apport || true
fi
if command -v systemctl >/dev/null 2>&1; then
    systemctl disable --now apport.service >/dev/null 2>&1 || true
fi

# WARNING: order matters. `apport --stop` does NOT restore our value — it writes
# the bare word "core", which makes the kernel drop cores into the crashing
# process's CWD. So our sysctl has to be re-applied AFTER apport is stopped,
# never before.
if command -v systemctl >/dev/null 2>&1; then
    systemctl daemon-reload >/dev/null 2>&1 || true
    # Kept as a backstop: an apport upgrade can re-enable itself, and this is
    # the third setting today that a later actor silently took over.
    systemctl enable yuneta-core-pattern.service >/dev/null 2>&1 || true
    systemctl start yuneta-core-pattern.service >/dev/null 2>&1 || true
else
    sysctl -p /etc/sysctl.d/99-yuneta-core.conf >/dev/null 2>&1 || true
fi

if [ "$(cat /proc/sys/kernel/core_pattern 2>/dev/null)" != "/var/crash/core.%e" ]; then
    logger -t yuneta_agent_deb \
        "WARNING: kernel.core_pattern is not /var/crash/core.%e — yuno core dumps will not land there"
fi

# Apply kernel settings and reload systemd units
if command -v sysctl >/dev/null 2>&1; then
    sysctl --system >/dev/null 2>&1 || true
fi

# SSH authorized_keys for 'yuneta' (merge, idempotent)
if [ -s /etc/yuneta/authorized_keys ]; then
    umask 077
    install -d -o yuneta -g yuneta -m 0700 /home/yuneta/.ssh || true
    install -m 0600 "/etc/yuneta/authorized_keys" "/home/yuneta/.ssh/authorized_keys"
    chown yuneta:yuneta "/home/yuneta/.ssh/authorized_keys"
fi

# Ensure classic /var/log/syslog via rsyslog
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

# --- Enable rc symlinks always; start only on configure/reconfigure ---

# Ensure the init script is present (idempotent)
if [ ! -x /etc/init.d/yuneta_agent ] && [ -x /yuneta/agent/service/yuneta_agent ]; then
    install -m 0755 /yuneta/agent/service/yuneta_agent /etc/init.d/yuneta_agent
    info "Installed /etc/init.d/yuneta_agent"
fi

# Create /etc/rc*.d/ symlinks (idempotent; harmless if they already exist)
if [ -x /usr/sbin/update-rc.d ] && [ -e /etc/init.d/yuneta_agent ]; then
    /usr/sbin/update-rc.d yuneta_agent defaults || info "update-rc.d returned $?"
fi

# Start only on configure / reconfigure
case "$ACTION" in
    configure|reconfigure)
        info "Starting service…"
        if command -v invoke-rc.d >/dev/null 2>&1; then
            invoke-rc.d yuneta_agent start || true
        else
            /etc/init.d/yuneta_agent start || true
        fi
        ;;
    *)
        : # not starting on other actions (e.g. triggered/abort*)
        ;;
esac

# Ensure pam_limits.so is enabled in PAM session files
for pam_file in /etc/pam.d/common-session /etc/pam.d/common-session-noninteractive; do
    if [ -f "$pam_file" ]; then
        if ! grep -q '^session[[:space:]]\+required[[:space:]]\+pam_limits\.so' "$pam_file"; then
            echo "Adding 'session required pam_limits.so' to $pam_file"
            echo 'session required pam_limits.so' >> "$pam_file"
        fi
    fi
done

# --- friendly reminder for developers ---
printf "\n[Yuneta] To install developer dependencies, run:\n" >&2
printf "    sudo /yuneta/bin/install-yuneta-dev-deps.sh\n" >&2
printf "    sudo /yuneta/bin/install-certbot.sh\n\n" >&2

logger -t yuneta_agent_deb "Reminder: run /yuneta/bin/install-yuneta-dev-deps.sh"
logger -t yuneta_agent_deb "Reminder: run /yuneta/bin/install-certbot.sh"

# --- Kernel tuning applied live; reboot recommended, NEVER forced ---
# The sysctl settings (core dumps, fd limits) were already applied above with
# `sysctl --system`, so a reboot is NOT required to run. Forcing one here would
# kill an unattended `curl | sh` run mid-flight — exactly what happened when the
# auto-reboot fired before the developer toolchain could install. So we only
# leave the standard reboot-required hint and recommend it; we never reboot.
# (The .rpm %post follows the same no-auto-reboot policy.)
mkdir -p /run || true
printf "reboot recommended by yuneta-agent installer (verify boot-time startup)\n" \
    >/run/reboot-required 2>/dev/null || true
echo "Yuneta Agent installed. Kernel tuning applied live (no reboot needed)."
echo "A reboot is recommended (not required) to verify the agent starts at boot:"
echo "    sudo reboot"
# --- /no forced reboot ---

exit 0

EOF
chmod 0755 "${WORKDIR}/DEBIAN/postinst"

# --- prerm (stop before upgrade/remove) ---
cat > "${WORKDIR}/DEBIAN/prerm" <<'EOF'
#!/bin/sh
#######################################################################
# prerm
# Stop the service prior to upgrade/remove (links removed in postrm)
#######################################################################
set -eu
if command -v invoke-rc.d >/dev/null 2>&1; then
    invoke-rc.d yuneta_agent stop || true
else
    [ -x /etc/init.d/yuneta_agent ] && /etc/init.d/yuneta_agent stop || true
fi
exit 0
EOF
chmod 0755 "${WORKDIR}/DEBIAN/prerm"

# --- postrm (remove runlevel links on remove; delete init script only on purge) ---
cat > "${WORKDIR}/DEBIAN/postrm" <<'EOF'
#!/bin/sh
#######################################################################
# postrm
# - On remove: delete SysV runlevel symlinks (keep /etc/init.d/yuneta_agent)
# - On purge: also delete /etc/init.d/yuneta_agent (conffile) if it remains
#######################################################################
set -eu
case "${1:-}" in
    remove)
        if [ -x /yuneta/agent/service/remove-yuneta-service.sh ]; then
            /yuneta/agent/service/remove-yuneta-service.sh || true
        else
            if [ -x /usr/sbin/update-rc.d ]; then
                /usr/sbin/update-rc.d -f yuneta_agent remove >/dev/null 2>&1 || true
            fi
        fi
        ;;
    purge)
        # Remove runlevel symlinks (again, just in case)
        if [ -x /usr/sbin/update-rc.d ]; then
            /usr/sbin/update-rc.d -f yuneta_agent remove >/dev/null 2>&1 || true
        fi
        # Delete the init script itself only on purge
        if [ -e /etc/init.d/yuneta_agent ]; then
            rm -f /etc/init.d/yuneta_agent || true
        fi
        ;;
esac
exit 0
EOF
chmod 0755 "${WORKDIR}/DEBIAN/postrm"

# ---------- Installed-Size (KiB, payload only) ----------
# `dpkg-deb --build` does NOT compute this field — only dpkg-gencontrol does,
# and we write DEBIAN/control by hand. Without it apt reports the package as
# taking 0 bytes ("After this operation, 49.0 MB will be used" for a 294 MB
# .deb) and its disk-space check silently passes on a box that cannot hold the
# payload. Insert it BEFORE Description: that field is multi-line, so anything
# appended after it is parsed as part of the description.
SIZE_ALL_KB=$(du -sk "${WORKDIR}" | cut -f1)
SIZE_CTRL_KB=$(du -sk "${WORKDIR}/DEBIAN" | cut -f1)
INSTALLED_SIZE_KB=$((SIZE_ALL_KB - SIZE_CTRL_KB))
sed -i "/^Description:/i Installed-Size: ${INSTALLED_SIZE_KB}" "${WORKDIR}/DEBIAN/control"
echo "[i] Installed-Size: ${INSTALLED_SIZE_KB} KiB"

# ---------- Normalize permissions before building ----------
# Ensure all dirs are 0755 and clear any inherited setgid/sticky bits
find "${WORKDIR}" -type d -exec chmod 0755 {} + -exec chmod g-s {} + -exec chmod -t {} +

# DEBIAN/ must be exactly 0755 (no setgid/sticky)
chmod 0755 "${WORKDIR}/DEBIAN" || true
chmod g-s  "${WORKDIR}/DEBIAN" || true
chmod -t   "${WORKDIR}/DEBIAN" || true

# All DEBIAN control files default to 0644 (maintainer scripts are made +x above)
find "${WORKDIR}/DEBIAN" -type f -print0 | xargs -0 chmod 0644
chmod 0755 "${WORKDIR}/DEBIAN/postinst" "${WORKDIR}/DEBIAN/prerm" "${WORKDIR}/DEBIAN/postrm"

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
