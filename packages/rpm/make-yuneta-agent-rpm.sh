#!/usr/bin/env bash
#######################################################################
#               make-yuneta-agent-rpm.sh
#######################################################################
# Build an .rpm for Yuneta's Agent with SysV integration (RHEL/Rocky/Alma).
#
# This is the RPM counterpart of ../make-yuneta-agent-deb.sh. It stages
# the SAME /yuneta runtime tree and the same generated helpers/configs,
# then packages it with rpmbuild instead of dpkg-deb. Differences are
# RHEL-specific:
#   - control file        -> RPM .spec (Requires/Recommends)
#   - postinst/prerm/postrm -> %post / %preun / %postun scriptlets
#   - adduser             -> useradd ; 'sudo' group -> 'wheel'
#   - locale-gen          -> glibc-langpack-* + /etc/locale.conf
#   - update-rc.d         -> chkconfig
#   - certbot via snap    -> certbot via EPEL (install-certbot.sh)
#   - dev deps via apt    -> dev deps via dnf (install-yuneta-dev-deps.sh)
#   - ships kernel.io_uring_disabled=0 (RHEL 9 disables io_uring; Yuneta's
#     yev_loop needs it) in /etc/sysctl.d/99-yuneta-core.conf
#
# Usage:
#     ./make-yuneta-agent-rpm.sh <project> <version> <release> <arch>
#
# Layout (relative to this script, which must live in yunetas/packages/rpm/):
#   - stages under: ./build/rpm/<arch>/stage/
#   - rpmbuild dir: ./build/rpm/<arch>/rpmbuild/
#   - outputs to:   ./dist/<project>-<version>-<release>.<dist>.<arch>.rpm
#
# Notes:
#   - All yunetas binaries must be pre-built (`yunetas build`).
#   - Builds only package whatever is in /yuneta/bin and /yuneta/agent —
#     the <arch> argument is a label (same contract as the .deb script);
#     for a real cross-arch package those dirs must hold cross-built bins.
#   - The .rpm is NOT installed by this script. Inspect with
#     `rpm -qlp`, `rpm -qp --scripts`, `rpmlint`.
#######################################################################

set -euo pipefail
umask 022

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd -P)"

if [ "${1:-}" = "--help" ] || [ "$#" -lt 4 ]; then
    echo "Usage: $0 <project> <version> <release> <arch>"
    exit 1
fi

PROJECT="$1"; shift
RAW_VERSION="$1"; shift
RELEASE="$1"; shift
ARCHITECTURE="$1"; shift

# RPM Version must not contain '-': map it to '~' (pre-release ordering).
VERSION="${RAW_VERSION//-/\~}"

BIN_DIR="/yuneta/bin"
BASE_DEST="${SCRIPT_DIR}/build/rpm/${ARCHITECTURE}"
STAGE="${BASE_DEST}/stage"
RPMTOP="${BASE_DEST}/rpmbuild"
SPEC="${BASE_DEST}/yuneta-agent.spec"
OUT_DIR="${SCRIPT_DIR}/dist"

echo "[i] Building RPM stage tree at: ${STAGE}"

#-----------------------------------------------------#
#   Resolve YUNETAS_BASE
#-----------------------------------------------------#
if [[ -n "${YUNETAS_BASE:-}" && ! -d "$YUNETAS_BASE" ]]; then
    echo "Warning: YUNETAS_BASE='$YUNETAS_BASE' is not a directory. Falling back..." >&2
    unset YUNETAS_BASE
fi
if [[ -z "${YUNETAS_BASE:-}" ]]; then
    if [[ -d "/yuneta/development/yunetas" ]]; then
        YUNETAS_BASE="/yuneta/development/yunetas"
    fi
fi
if [[ -z "${YUNETAS_BASE:-}" ]]; then
    echo "Error: Could not determine YUNETAS_BASE. Set the env var or ensure /yuneta/development/yunetas exists." >&2
    exit 1
fi
echo "Using YUNETAS_BASE: $YUNETAS_BASE"

#-----------------------------------------------------#
#   Fresh stage tree (mirror of the .deb layout, no DEBIAN/)
#-----------------------------------------------------#
rm -rf "${BASE_DEST}"
mkdir -p "${STAGE}/etc/profile.d"
mkdir -p "${STAGE}/etc/sudoers.d"
mkdir -p "${STAGE}/etc/init.d"
mkdir -p "${STAGE}/etc/letsencrypt/renewal-hooks/deploy"
mkdir -p "${STAGE}/yuneta/agent/service"
mkdir -p "${STAGE}/yuneta/agent"
mkdir -p "${STAGE}/yuneta/bin"
mkdir -p "${STAGE}/yuneta/gui"
mkdir -p "${STAGE}/yuneta/realms"
mkdir -p "${STAGE}/yuneta/repos"
mkdir -p "${STAGE}/yuneta/store/certs/private"
mkdir -p "${STAGE}/yuneta/store/queues/gate_msgs2"
mkdir -p "${STAGE}/yuneta/share"
mkdir -p "${STAGE}/yuneta/development/yunetas/outputs"
mkdir -p "${STAGE}/yuneta/development/yunetas/outputs_ext"
mkdir -p "${STAGE}/yuneta/development/yunetas/tools/cmake"
mkdir -p "${STAGE}/yuneta/development/projects"
mkdir -p "${STAGE}/etc/yuneta"
mkdir -p "${STAGE}/var/crash"
mkdir -p "${STAGE}/etc/sysctl.d"
mkdir -p "${STAGE}/etc/security/limits.d"

# --- Single-file utilities into /yuneta/bin ---
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
for BINARY in "${BINARIES[@]}"; do
    SRC="${BIN_DIR%/}/${BINARY}"
    if [ ! -x "${SRC}" ]; then
        echo "[-] Missing or non-executable binary: ${SRC}" >&2
        exit 2
    fi
    install -D -m 0755 "${SRC}" "${STAGE}/yuneta/bin/${BINARY}"
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

copy_tree "/yuneta/bin/ncurses"                 "${STAGE}/yuneta/bin"
copy_tree "/yuneta/bin/nginx"                   "${STAGE}/yuneta/bin"
copy_tree "/yuneta/bin/openresty"               "${STAGE}/yuneta/bin"
copy_tree "/yuneta/bin/skeletons"               "${STAGE}/yuneta/bin"
# Sparse SDK tree: same YUNETAS_BASE path as a full source checkout, so
# outputs/ lives at /yuneta/development/yunetas/outputs on EVERY node.
copy_tree "${YUNETAS_BASE}/outputs_ext"         "${STAGE}/yuneta/development/yunetas"
copy_tree "${YUNETAS_BASE}/outputs"             "${STAGE}/yuneta/development/yunetas"
copy_tree "${YUNETAS_BASE}/tools"               "${STAGE}/yuneta/development/yunetas"
install -D -m 0644 "${YUNETAS_BASE}/.config"    "${STAGE}/yuneta/development/yunetas/.config"

# Strip lab load-generators from the payload: stress_* have no place on a
# production node. perf_* stay on purpose — fully static benchmarks, handy
# to measure the target machine.
rm -f "${STAGE}"/yuneta/development/yunetas/outputs/yunos/stress_* \
      "${STAGE}"/yuneta/development/yunetas/outputs/bin/stress_*

rm -f "${STAGE}"/yuneta/bin/nginx/logs/* 2>/dev/null || true
rm -f "${STAGE}"/yuneta/bin/openresty/nginx/logs/* 2>/dev/null || true

# --- Optional: bundle SSH public key(s) for user 'yuneta' ---
YUNETA_AUTH_KEYS_FILE="${YUNETA_AUTHORIZED_KEYS:-${SCRIPT_DIR}/authorized_keys/authorized_keys}"
BUNDLED_AUTH_KEYS=0
if [ -f "${YUNETA_AUTH_KEYS_FILE}" ]; then
    echo "[i] Bundling authorized_keys from ${YUNETA_AUTH_KEYS_FILE}"
    install -D -m 0644 "${YUNETA_AUTH_KEYS_FILE}" "${STAGE}/etc/yuneta/authorized_keys"
    BUNDLED_AUTH_KEYS=1
fi

# --- Optional: select web server (nginx|openresty) via external file ---
WEB_SELECTOR_FILE="${SCRIPT_DIR}/webserver/webserver"
WEB_CHOICE="nginx"
if [ -f "${WEB_SELECTOR_FILE}" ]; then
    WEB_CHOICE="$(tr '[:upper:]' '[:lower:]' < "${WEB_SELECTOR_FILE}" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
    case "${WEB_CHOICE}" in
        nginx|openresty) : ;;
        *) echo "[!] Invalid webserver '${WEB_CHOICE}', defaulting to nginx"; WEB_CHOICE="nginx" ;;
    esac
fi
echo "[i] Web server selection: ${WEB_CHOICE}"
printf '%s\n' "${WEB_CHOICE}" > "${STAGE}/etc/yuneta/webserver"
chmod 0644 "${STAGE}/etc/yuneta/webserver"

# --- Copy yuneta_agent binaries (required) and config samples ---
AGENT_SRC_1="/yuneta/agent/yuneta_agent"
AGENT_SRC_2="/yuneta/agent/yuneta_agent22"
AGENT_SRC_4="/yuneta/agent/yuneta_agent44"
AGENT_JSON_1="${YUNETAS_BASE}/packages/templates/yuneta_agent.json.sample"
AGENT_JSON_2="${YUNETAS_BASE}/packages/templates/yuneta_agent22.json.sample"

for f in "${AGENT_SRC_1}" "${AGENT_SRC_2}" "${AGENT_SRC_4}"; do
    if [ ! -x "${f}" ]; then
        echo "[-] Missing or non-executable: ${f}" >&2
        exit 2
    fi
done

install -D -m 0755 "${AGENT_SRC_1}" "${STAGE}/yuneta/agent/yuneta_agent"
install -D -m 0755 "${AGENT_SRC_2}" "${STAGE}/yuneta/agent/yuneta_agent22"
install -D -m 0755 "${AGENT_SRC_4}" "${STAGE}/yuneta/agent/yuneta_agent44"
install -D -m 0644 "${AGENT_JSON_1}" "${STAGE}/yuneta/agent/yuneta_agent.json.sample"
install -D -m 0644 "${AGENT_JSON_2}" "${STAGE}/yuneta/agent/yuneta_agent22.json.sample"

copy_tree "/yuneta/agent/certs" "${STAGE}/yuneta/agent"

#-----------------------------------------------------#
#   Generated runtime files (verbatim from the .deb)
#-----------------------------------------------------#

# --- Certs validator ---
cat > "${STAGE}/yuneta/bin/check-certs-validity.sh" <<'EOF'
#!/usr/bin/env bash
#######################################################################
# check-certs-validity.sh
# Recursively list *.crt and show subject, dates, and expiry status.
#######################################################################
set -euo pipefail

OPENSSL="$(command -v openssl || true)"
if [ -z "${OPENSSL}" ]; then
    echo "openssl not found in PATH" >&2
    exit 127
fi

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
chmod 0755 "${STAGE}/yuneta/bin/check-certs-validity.sh"

# --- Profile snippet (env, limits for shells, aliases) ---
cat > "${STAGE}/etc/profile.d/yuneta.sh" <<'EOF'
#!/bin/sh
# Yuneta environment
export YUNETA_DIR=/yuneta
export PATH="/yuneta/bin:/usr/sbin:/sbin:/home/yuneta/.local/bin:$PATH"

ulimit -c unlimited 2>/dev/null || true
ulimit -n unlimited 2>/dev/null || true
ulimit -Hn unlimited 2>/dev/null || true

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
chmod 0644 "${STAGE}/etc/profile.d/yuneta.sh"

# --- Kernel tuning + core dumps + io_uring (RHEL needs it ON) ---
cat > "${STAGE}/etc/sysctl.d/99-yuneta-core.conf" <<'EOF'
# Yuneta: TCP server tuning
# yuneta: write core files to /var/crash
# Install this file in /etc/sysctl.d/ and execute `sudo sysctl --system`
#

net.core.somaxconn = 65535

kernel.core_uses_pid = 0
kernel.core_pattern = /var/crash/core.%e

fs.file-max = 4000000
fs.nr_open  = 4000000

# Each tranger2 rt_disk follower (a non-master reader of a topic) uses one
# inotify instance; the 128 default is too low for a node running many yunos.
fs.inotify.max_user_instances = 4096
fs.inotify.max_user_watches = 524288

# Defensive cushion above the 16384 kernel default: fs_watcher resyncs on
# IN_Q_OVERFLOW, but a bigger per-instance queue makes the overflow (and the
# full-tree rescan it triggers) less likely under CREATE/DELETE bursts.
fs.inotify.max_queued_events = 65536

# Yuneta's event loop (yev_loop) is io_uring-based. RHEL/Rocky/Alma 9 ship
# kernel.io_uring_disabled=2 (fully disabled, a hardening default), which
# makes every yuno abort at startup. Re-enable it for all processes.
kernel.io_uring_disabled = 0

EOF
chmod 0644 "${STAGE}/etc/sysctl.d/99-yuneta-core.conf"

# --- limits drop-in ---
cat > "${STAGE}/etc/security/limits.d/99-yuneta-core.conf" <<'EOF'
# Yuneta: file limits tuning
# Install this file in /etc/security/limits.d
# pam_limits.so is enabled by default in RHEL's /etc/pam.d/system-auth

yuneta  soft    core    unlimited
yuneta  hard    core    unlimited
yuneta  soft    nofile  unlimited
yuneta  hard    nofile  unlimited
EOF
chmod 0644 "${STAGE}/etc/security/limits.d/99-yuneta-core.conf"

# --- SysV init script (with chkconfig header for RHEL) ---
cat > "${STAGE}/etc/init.d/yuneta_agent" <<'EOF'
#!/bin/sh
# chkconfig: 2345 90 10
# description: Yuneta Agent service (yunos + selected web server)
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

# We only use log_daemon_msg / log_end_msg, and define them ourselves.
# Do NOT source /etc/init.d/functions: on RHEL it references unset vars
# (e.g. SYSTEMCTL_SKIP_REDIRECT) and, under this script's `set -u`, sourcing
# it aborts the whole init script with "unbound variable" BEFORE the agent
# is ever launched — which is exactly why the service "failed" while the
# binary itself was fine. Debian's /lib/lsb/init-functions is set -u-clean,
# so use it when present (guarded just in case it pulls in functions).
log_daemon_msg() { echo "$@"; }
log_end_msg() { [ "$1" -eq 0 ] && echo "OK" || echo "FAIL ($1)"; }
if [ -r /lib/lsb/init-functions ]; then
    set +u
    . /lib/lsb/init-functions
    set -u
fi

_set_limits() {
    ulimit -c unlimited || true
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
                "$OPENRESTY_BIN" && rc=0 || rc=$?
                log_end_msg "$rc"
            else
                echo "WARN: OpenResty binary not found: $OPENRESTY_BIN" >&2
            fi
            ;;
        nginx|*)
            if [ -x "$NGINX_BIN" ]; then
                log_daemon_msg "Starting Nginx"
                "$NGINX_BIN" && rc=0 || rc=$?
                log_end_msg "$rc"
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
chmod 0755 "${STAGE}/etc/init.d/yuneta_agent"
install -m 0755 "${STAGE}/etc/init.d/yuneta_agent" "${STAGE}/yuneta/agent/service/yuneta_agent"

# --- service helpers (chkconfig-based) ---
cat > "${STAGE}/yuneta/agent/service/install-yuneta-service.sh" <<'EOF'
#!/bin/sh
#######################################################################
# Install SysV service: place init script and enable/start it (RHEL)
#######################################################################
set -eu

if [ ! -x "/etc/init.d/yuneta_agent" ]; then
    install -m 0755 /yuneta/agent/service/yuneta_agent /etc/init.d/yuneta_agent
fi

if command -v chkconfig >/dev/null 2>&1; then
    chkconfig --add yuneta_agent >/dev/null 2>&1 || true
    chkconfig yuneta_agent on    >/dev/null 2>&1 || true
fi

if command -v service >/dev/null 2>&1; then
    service yuneta_agent start || true
else
    /etc/init.d/yuneta_agent start || true
fi
exit 0
EOF
chmod 0755 "${STAGE}/yuneta/agent/service/install-yuneta-service.sh"

cat > "${STAGE}/yuneta/agent/service/remove-yuneta-service.sh" <<'EOF'
#!/bin/sh
#######################################################################
# Disable SysV service: stop and remove from chkconfig (RHEL)
#######################################################################
set -eu
if command -v service >/dev/null 2>&1; then
    service yuneta_agent stop || true
else
    /etc/init.d/yuneta_agent stop || true
fi
if command -v chkconfig >/dev/null 2>&1; then
    chkconfig --del yuneta_agent >/dev/null 2>&1 || true
fi
exit 0
EOF
chmod 0755 "${STAGE}/yuneta/agent/service/remove-yuneta-service.sh"

# --- Certbot installer (EPEL, RHEL/Rocky) ---
cat > "${STAGE}/yuneta/bin/install-certbot.sh" <<'EOF'
#!/usr/bin/env bash
#######################################################################
# Install certbot on RHEL/Rocky/Alma from EPEL.
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

if ! command -v dnf >/dev/null 2>&1; then
    echo "dnf not found; this helper supports RHEL/Rocky/Alma/Fedora only." >&2
    exit 1
fi

# certbot lives in EPEL on EL9
dnf -y install epel-release || true
dnf -y install certbot

install -d -m 0755 /etc/letsencrypt/renewal-hooks/deploy

echo
echo "[i] Certbot version:"
certbot --version || true

cat <<'HINT'

Your .rpm already installs a deploy hook at:
/etc/letsencrypt/renewal-hooks/deploy/reload-certs
It copies renewed certs to /yuneta/store/certs/, reloads the selected web
server, and hot-reloads TLS in every running yuno.

certbot's systemd renewal timer:
  systemctl enable --now certbot-renew.timer
HINT
EOF
chmod 0755 "${STAGE}/yuneta/bin/install-certbot.sh"

# --- Let's Encrypt deploy hook (verbatim from the .deb) ---
cat > "${STAGE}/etc/letsencrypt/renewal-hooks/deploy/reload-certs" <<'EOF'
#!/bin/sh
#######################################################################
# Triggered by certbot after successful renewal.
# 1) Copies fresh certs to /yuneta/store/certs
# 2) Reloads selected web server (nginx or openresty)
# 3) Hot-reloads TLS certificates in every running yuno via ycommand
#######################################################################
set -u

LOG_DIR="/var/log/yuneta"
mkdir -p "$LOG_DIR"

STATE_DIR="/var/lib/yuneta"
mkdir -p "$STATE_DIR"

ts() { date '+%Y-%m-%dT%H:%M:%S%z'; }
log() { printf '[%s] %s\n' "$(ts)" "$*" >>"$LOG_DIR/deploy-hook.log"; }

log "deploy hook started (RENEWED_LINEAGE=${RENEWED_LINEAGE:-?} RENEWED_DOMAINS=${RENEWED_DOMAINS:-?})"

if [ -x /yuneta/store/certs/copy-certs.sh ]; then
    if /yuneta/store/certs/copy-certs.sh >>"$LOG_DIR/copy-certs.log" 2>&1; then
        log "copy-certs.sh OK"
    else
        log "copy-certs.sh FAILED (rc=$?)"
    fi
else
    log "copy-certs.sh NOT FOUND — skipping"
fi

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

date '+%s' > "$STATE_DIR/last-deploy-hook-run" 2>/dev/null || true
log "deploy hook finished"

exit 0
EOF
chmod 0755 "${STAGE}/etc/letsencrypt/renewal-hooks/deploy/reload-certs"

# --- Restart helper (RHEL: service / init.d) ---
cat > "${STAGE}/yuneta/bin/restart-yuneta" <<'EOF'
#!/usr/bin/env bash
#######################################################################
# Restart Yuneta stack. If root, prefer service restart; otherwise
# fallback to yshutdown + agent start.
#######################################################################
set -euo pipefail

if [ "${EUID:-$(id -u)}" -eq 0 ]; then
    if command -v service >/dev/null 2>&1; then
        service yuneta_agent restart || true
        exit 0
    fi
    if [ -x /etc/init.d/yuneta_agent ]; then
        /etc/init.d/yuneta_agent restart || true
        exit 0
    fi
fi

if [ -x /yuneta/bin/yshutdown ]; then
    /yuneta/bin/yshutdown || true
    sleep 1
fi
if [ -x /yuneta/agent/yuneta_agent ]; then
    /yuneta/agent/yuneta_agent --start --config-file=/yuneta/agent/yuneta_agent.json || true
fi
EOF
chmod 0755 "${STAGE}/yuneta/bin/restart-yuneta"

# --- Helper: check queues of messages (verbatim) ---
cat > "${STAGE}/yuneta/bin/colas2.sh" <<'EOF'
#!/usr/bin/env bash
#######################################################################
# colas2.sh
# Scan two-level queue dirs and call list_queue_msgs2 on each.
#######################################################################
set -euo pipefail

LIST_BIN="/yuneta/bin/list_queue_msgs2"

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
chmod 0755 "${STAGE}/yuneta/bin/colas2.sh"

# --- Dev stack installer (RHEL/dnf) ---
cat > "${STAGE}/yuneta/bin/install-yuneta-dev-deps.sh" <<'EOF'
#!/usr/bin/env bash
#######################################################################
# Install recommended developer dependencies for building new Yunos
# on RHEL/Rocky/Alma/Fedora (dnf + EPEL + CRB).
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

if ! command -v dnf >/dev/null 2>&1; then
    echo "dnf not found; this helper supports RHEL/Rocky/Alma/Fedora only." >&2
    exit 1
fi

. /etc/os-release 2>/dev/null || true
if [ "${ID:-}" != "fedora" ]; then
    dnf -y install epel-release || true
    if command -v crb >/dev/null 2>&1; then
        crb enable || true
    else
        dnf config-manager --set-enabled crb 2>/dev/null \
            || dnf config-manager --set-enabled powertools 2>/dev/null || true
    fi
fi

PKGS=(
    git mercurial make cmake ninja-build
    gcc clang gcc-c++
    python3-devel python3-pip python3-setuptools
    python3-tkinter python3-wheel
    jansson-devel pcre2-devel liburing-devel
    pcre-devel zlib-devel openssl-devel
    perl dos2unix tree wget
    libpq-devel
    telnet pipx
    patch gettext fail2ban rsync
    pkgconf-pkg-config ca-certificates glibc-devel kernel-headers
    glibc-static libstdc++-static libxcrypt-static
)

echo "[i] Installing dev packages…"
# Resilient: --setopt=strict=0 installs every package that exists in the
# enabled repos and skips only the unfindable ones. dnf's DEFAULT is an
# all-or-nothing transaction: one missing package (e.g. a CRB-only -devel
# when CRB never enabled) aborts the WHOLE set, leaving nothing installed —
# and the old `|| echo continuing` hid it behind a green "[✓] complete".
# NOTE: use --setopt=strict=0, NOT --skip-unavailable: the latter is dnf5
# (Fedora) only and RHEL 9's dnf4 rejects it ("unrecognized arguments"),
# which would itself abort the whole install.
dnf -y --setopt=strict=0 install "${PKGS[@]}" \
    || echo "[!] dnf returned an error; see the per-package report below"

# Honest report: re-check each requested package so a silently-skipped one is
# visible instead of masked.
MISSING=()
for pkg in "${PKGS[@]}"; do
    if ! rpm -q "$pkg" >/dev/null 2>&1; then
        MISSING+=("$pkg")
    fi
done

if command -v update-ca-trust >/dev/null 2>&1; then
    update-ca-trust || true
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

if [ "${#MISSING[@]}" -eq 0 ]; then
    echo "[✓] Dev environment ready — all ${#PKGS[@]} packages installed."
else
    echo "[!] ${#MISSING[@]} package(s) NOT installed: ${MISSING[*]}" >&2
    echo "[!] A missing -devel/-static usually means EPEL or CRB is not" >&2
    echo "[!] enabled. Enable them and re-run this script:" >&2
    echo "[!]     sudo dnf install -y epel-release" >&2
    echo "[!]     sudo crb enable   # or: sudo dnf config-manager --set-enabled crb" >&2
    echo "[!] Then: sudo $0" >&2
fi
EOF
chmod 0755 "${STAGE}/yuneta/bin/install-yuneta-dev-deps.sh"

# --- Copy-certs helper (verbatim from the .deb) ---
cat > "${STAGE}/yuneta/store/certs/copy-certs.sh" <<'EOF'
#!/usr/bin/env bash
#######################################################################
# Copy Let's Encrypt certs from /etc/letsencrypt/live/* into
# /yuneta/store/certs (and private keys into /yuneta/store/certs/private)
#######################################################################
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" >/dev/null 2>&1 && pwd -P)"
cd "${SCRIPT_DIR}"

if [ "${EUID:-$(id -u)}" -ne 0 ]; then
    echo "This script must run as root (to read /etc/letsencrypt/live/*/privkey.pem)" >&2
    exit 1
fi

DEST_BASE="/yuneta/store/certs"
DEST_PRIV="${DEST_BASE}/private"
CERTS_FILE="${DEST_BASE}/certs.list"

install -d -m 0755 "${DEST_BASE}"
install -d -m 0700 "${DEST_PRIV}"

CERT_NAMES=""
if [ -s "${CERTS_FILE}" ]; then
    while IFS= read -r line; do
        case "$line" in
            ''|\#*) continue ;;
            *) CERT_NAMES="${CERT_NAMES} ${line}" ;;
        esac
    done < "${CERTS_FILE}"
else
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

    FULLCHAIN_R="$(readlink -f "${FULLCHAIN}")"
    CHAIN_R="$(readlink -f "${CHAIN}")"
    PRIVKEY_R="$(readlink -f "${PRIVKEY}")"
    install -C -o yuneta -g yuneta -m 0644 "${FULLCHAIN_R}" "${DEST_BASE}/${CERT}.crt"
    install -C -o yuneta -g yuneta -m 0644 "${CHAIN_R}"     "${DEST_BASE}/${CERT}.chain"
    install -C -o yuneta -g yuneta -m 0600 "${PRIVKEY_R}"   "${DEST_PRIV}/${CERT}.key"
    echo "Copied ${CERT} -> ${DEST_BASE}/{${CERT}.crt, ${CERT}.chain, private/${CERT}.key}"
done

if [ -x /yuneta/bin/check-certs-validity.sh ]; then
    /yuneta/bin/check-certs-validity.sh "${DEST_BASE}" || true
fi

exit 0
EOF
chmod 0755 "${STAGE}/yuneta/store/certs/copy-certs.sh"

# --- sudoers (NOPASSWD) ---
cat > "${STAGE}/etc/sudoers.d/90-yuneta" <<'EOF'
# User rules for yuneta
yuneta ALL=(ALL) NOPASSWD:ALL
EOF
chmod 0440 "${STAGE}/etc/sudoers.d/90-yuneta"

#-----------------------------------------------------#
#   Write the .spec
#-----------------------------------------------------#
mkdir -p "${RPMTOP}/BUILD" "${RPMTOP}/RPMS" "${RPMTOP}/SOURCES" "${RPMTOP}/SPECS" "${RPMTOP}/SRPMS"

# Build the %files list from the stage (everything under /yuneta + /var/crash),
# with /etc config files tagged %config(noreplace) individually below.
cat > "${SPEC}" <<SPEC_EOF
# Ship pre-built (often static, unstripped) binaries untouched.
%global __os_install_post %{nil}
%global _build_id_links none
%global debug_package %{nil}
%define _staging ${STAGE}

Name:           ${PROJECT}
Version:        ${VERSION}
Release:        ${RELEASE}%{?dist}
Summary:        Yuneta's Agent

License:        MIT
URL:            https://yuneta.io
BuildArch:      ${ARCHITECTURE}
AutoReqProv:    no

Requires:       shadow-utils, rsync, rsyslog, chkconfig, initscripts, sudo
Requires:       glibc-langpack-en, glibc-langpack-es
Recommends:     vim-enhanced, tree, pipx, fail2ban, net-tools, mlocate, curl, telnet

%description
Yuneta Agent binaries, runtime directories, SysV service, certbot hooks
and helpers. Installs the complete Yuneta runtime under /yuneta and a
'yuneta' system user. RHEL/Rocky/Alma build of the same payload shipped
by the Debian .deb.

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}
cp -a %{_staging}/. %{buildroot}/

%files
%defattr(-,root,root,-)
/yuneta
%dir /var/crash
%dir /etc/yuneta
%config(noreplace) /etc/yuneta/webserver
%config(noreplace) /etc/profile.d/yuneta.sh
%config(noreplace) /etc/sysctl.d/99-yuneta-core.conf
%config(noreplace) /etc/security/limits.d/99-yuneta-core.conf
%attr(0440,root,root) %config(noreplace) /etc/sudoers.d/90-yuneta
%attr(0755,root,root) %config(noreplace) /etc/init.d/yuneta_agent
%config(noreplace) /etc/letsencrypt/renewal-hooks/deploy/reload-certs
SPEC_EOF

if [ "${BUNDLED_AUTH_KEYS}" -eq 1 ]; then
    echo '%config(noreplace) /etc/yuneta/authorized_keys' >> "${SPEC}"
fi

# ---- scriptlets ----
cat >> "${SPEC}" <<'SPEC_EOF'

%post
#######################################################################
# %post  ($1 == 1 install, $1 == 2 upgrade)
#######################################################################
set -u

info() { echo "[post] $*"; }
warn() { echo "[post] WARNING: $*" >&2; }

# Create 'yuneta' login user if missing
if ! id -u yuneta >/dev/null 2>&1; then
    info "Creating login user 'yuneta'…"
    useradd -m -d /home/yuneta -s /bin/bash -c "Yuneta User" yuneta || true
    passwd -l yuneta >/dev/null 2>&1 || true
else
    UID_NOW="$(id -u yuneta || echo 0)"
    if [ "$UID_NOW" -lt 1000 ]; then
        warn "'yuneta' exists with system UID ($UID_NOW < 1000). Keeping UID."
    fi
fi

install -d -o yuneta -g yuneta -m 0755 /home/yuneta || true

# Add 'yuneta' to a broad set of system-access groups (missing ones skipped).
# RHEL uses 'wheel' for sudo (not 'sudo'); the rest mirror the Debian list.
DEFAULT_GROUPS="adm,wheel,tty,dialout,cdrom,audio,video,plugdev,netdev,render,input,gpio,i2c,spi,uucp,wireshark,bluetooth,scanner,lp,kvm,libvirt,docker"
GROUPS="${YUNETA_GROUPS:-$DEFAULT_GROUPS}"
CREATE_MISSING="${YUNETA_CREATE_MISSING_GROUPS:-0}"

OLDIFS="$IFS"
IFS=','; set -f
for grp in $GROUPS; do
    [ -z "$grp" ] && continue
    if getent group "$grp" >/dev/null 2>&1; then
        usermod -aG "$grp" yuneta || true
    else
        if [ "$CREATE_MISSING" = "1" ]; then
            groupadd "$grp" || true
            usermod -aG "$grp" yuneta || true
        fi
    fi
done
set +f; IFS="$OLDIFS"

# Ensure agent configs exist without overwriting existing ones.
# Default node_owner is "none" (standalone node, no controlcenter); the
# agent only dials the controlcenter when node_owner != "none", so a fresh
# box stays quiet instead of looping on getaddrinfo() for an unresolvable
# URL. Operators WITH a controlcenter set YUNETA_OWNER=mycompany to swap it.
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

# Locale: rely on glibc-langpack-{en,es} (Requires); set a default if unset.
if [ ! -s /etc/locale.conf ]; then
    echo 'LANG=en_US.UTF-8' > /etc/locale.conf || true
    info "Set LANG=en_US.UTF-8 in /etc/locale.conf"
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

# Apply kernel settings (includes kernel.io_uring_disabled=0 — Yuneta needs it)
if command -v sysctl >/dev/null 2>&1; then
    sysctl --system >/dev/null 2>&1 || true
fi

# SSH authorized_keys for 'yuneta' (if bundled)
if [ -s /etc/yuneta/authorized_keys ]; then
    umask 077
    install -d -o yuneta -g yuneta -m 0700 /home/yuneta/.ssh || true
    install -m 0600 "/etc/yuneta/authorized_keys" "/home/yuneta/.ssh/authorized_keys"
    chown yuneta:yuneta "/home/yuneta/.ssh/authorized_keys"
fi

# Ensure rsyslog (classic /var/log/messages)
if command -v systemctl >/dev/null 2>&1; then
    systemctl enable --now rsyslog >/dev/null 2>&1 || true
fi

# The bundled nginx falls back to its compiled-default group 'nogroup', which
# exists on Debian but NOT on RHEL — without it nginx aborts at startup with
# `getgrnam("nogroup") failed`. Create it before the service starts (RHEL-only;
# harmless if it already exists).
if ! getent group nogroup >/dev/null 2>&1; then
    groupadd nogroup >/dev/null 2>&1 || true
fi

# Install + enable the SysV service
if [ ! -x /etc/init.d/yuneta_agent ] && [ -x /yuneta/agent/service/yuneta_agent ]; then
    install -m 0755 /yuneta/agent/service/yuneta_agent /etc/init.d/yuneta_agent
fi
if command -v chkconfig >/dev/null 2>&1 && [ -e /etc/init.d/yuneta_agent ]; then
    chkconfig --add yuneta_agent >/dev/null 2>&1 || true
    chkconfig yuneta_agent on    >/dev/null 2>&1 || true
fi

# Start the service (install and upgrade) — HONESTLY.
#
#   RPM treats a %post failure as non-fatal: the files are installed, so the
#   transaction is reported "Complete" no matter what happens here. A bare
#   `service start || true` therefore hid a failed agent behind a green
#   install. Instead: only start when io_uring is actually usable, capture the
#   real result, and tell the operator plainly when the agent is NOT running.
YUNETA_STARTED=0
YUNETA_START_MSG=""

# Re-read the EFFECTIVE io_uring state after the sysctl --system above.
# yev_loop is io_uring-based; if the kernel keeps it disabled the agent will
# abort, so do not even try (and do not pretend it started).
IOURING_NOW="$(sysctl -n kernel.io_uring_disabled 2>/dev/null || echo 0)"
if [ "$IOURING_NOW" != "0" ]; then
    YUNETA_START_MSG="io_uring is disabled (kernel.io_uring_disabled=$IOURING_NOW); agent NOT started. Enable it (sudo sysctl -w kernel.io_uring_disabled=0, or reboot) then: sudo service yuneta_agent start"
else
    if command -v service >/dev/null 2>&1; then
        service yuneta_agent start && YUNETA_STARTED=1 || YUNETA_STARTED=0
    elif [ -x /etc/init.d/yuneta_agent ]; then
        /etc/init.d/yuneta_agent start && YUNETA_STARTED=1 || YUNETA_STARTED=0
    fi
    if [ "$YUNETA_STARTED" != "1" ]; then
        YUNETA_START_MSG="yuneta_agent FAILED to start. Diagnose: systemctl status yuneta_agent.service ; journalctl -xeu yuneta_agent.service ; getenforce (SELinux can deny io_uring to confined services)"
    fi
fi

# pam_limits.so is enabled by default in RHEL's system-auth; only append if
# genuinely missing (and the file is not an authselect-managed symlink).
for pam_file in /etc/pam.d/system-auth /etc/pam.d/password-auth; do
    if [ -f "$pam_file" ] && [ ! -L "$pam_file" ]; then
        if ! grep -q '^session[[:space:]]\+required[[:space:]]\+pam_limits\.so' "$pam_file"; then
            echo 'session required pam_limits.so' >> "$pam_file"
        fi
    fi
done

# --- Honest final status (the package files are always installed; this says
#     whether the AGENT is actually running) ---
if [ "$YUNETA_STARTED" = "1" ]; then
    info "yuneta_agent is running."
else
    warn "================================================================"
    warn "Yuneta files installed, but the AGENT IS NOT RUNNING:"
    warn "  ${YUNETA_START_MSG}"
    warn "================================================================"
fi

printf "\n[Yuneta] To install developer dependencies, run:\n" >&2
printf "    sudo /yuneta/bin/install-yuneta-dev-deps.sh\n" >&2
printf "    sudo /yuneta/bin/install-certbot.sh\n\n" >&2

exit 0

%preun
#######################################################################
# %preun  ($1 == 0 uninstall, $1 == 1 upgrade)
#######################################################################
set -u
if [ "$1" = "0" ]; then
    if command -v service >/dev/null 2>&1; then
        service yuneta_agent stop || true
    else
        [ -x /etc/init.d/yuneta_agent ] && /etc/init.d/yuneta_agent stop || true
    fi
    if command -v chkconfig >/dev/null 2>&1; then
        chkconfig --del yuneta_agent >/dev/null 2>&1 || true
    fi
fi
exit 0

%postun
#######################################################################
# %postun  ($1 == 0 uninstall, $1 == 1 upgrade)
#######################################################################
set -u
if [ "$1" = "0" ]; then
    # Full uninstall: drop the init script if it survived
    if [ -e /etc/init.d/yuneta_agent ]; then
        rm -f /etc/init.d/yuneta_agent || true
    fi
fi
exit 0
SPEC_EOF

#-----------------------------------------------------#
#   Build the RPM
#-----------------------------------------------------#
if ! command -v rpmbuild >/dev/null 2>&1; then
    echo "[-] rpmbuild not found. Install it: sudo dnf install rpm-build" >&2
    exit 3
fi

echo "[i] Running rpmbuild…"
rpmbuild -bb \
    --define "_topdir ${RPMTOP}" \
    --target "${ARCHITECTURE}" \
    "${SPEC}"

#-----------------------------------------------------#
#   Place final artifact in ./dist
#-----------------------------------------------------#
mkdir -p "${OUT_DIR}"
BUILT_RPM="$(find "${RPMTOP}/RPMS" -type f -name '*.rpm' | head -n1)"
if [ -z "${BUILT_RPM}" ]; then
    echo "[-] No .rpm produced under ${RPMTOP}/RPMS" >&2
    exit 4
fi
FINAL_RPM="${OUT_DIR}/$(basename "${BUILT_RPM}")"
cp -f "${BUILT_RPM}" "${FINAL_RPM}"

echo "[✓] Final package:"
echo "    ${FINAL_RPM}"
echo
echo "Inspect with:"
echo "    rpm -qlp '${FINAL_RPM}'"
echo "    rpm -qp --scripts '${FINAL_RPM}'"
echo "Install with:"
echo "    sudo dnf -y install '${FINAL_RPM}'"
