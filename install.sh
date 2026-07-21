#!/bin/sh
#
# Yuneta installer — one command, one flow, both distro families.
#
# Detects the distro (Debian/Ubuntu -> apt, RHEL/Rocky/Alma/Fedora -> dnf),
# pulls the matching package (.deb / .rpm) from a GitHub Release, installs it,
# installs certbot (TLS for the bundled web server), and then installs the full
# developer toolchain (git, mercurial, clang, gcc, cmake, ninja, wget, pipx, ...)
# so the box can build yunos right away. No second script for the user to
# remember — it all happens in this one run.
#
# On RHEL it first enables EPEL + CRB, because mercurial / ninja-build / pipx
# and the -devel/-static packages live there, and dnf cannot pull a package
# from a repo it enables in the same transaction.
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/artgins/yunetas/main/install.sh | sudo sh
#
# Pin a version (must exist as a GitHub Release):
#   ... | sudo sh -s -- 7.5.7
#
# Runtime only (skip the developer toolchain — pure deployment box):
#   ... | sudo sh -s -- --runtime-only
#
# Environment overrides:
#   YUNETA_REPO          GitHub repo, default "artgins/yunetas"
#   YUNETA_TAG           Release tag, default "latest" (or a positional arg)
#   YUNETA_RUNTIME_ONLY  "1" to skip the developer toolchain
#

set -eu

REPO="${YUNETA_REPO:-artgins/yunetas}"
TAG="${YUNETA_TAG:-latest}"
RUNTIME_ONLY="${YUNETA_RUNTIME_ONLY:-0}"

# ----- Parse args (flags + optional version) --------------------------------
for arg in "$@"; do
    case "$arg" in
        --runtime-only)
            RUNTIME_ONLY=1
            ;;
        -h|--help)
            echo "Yuneta installer. Usage:"
            echo "  curl -fsSL .../install.sh | sudo sh [-s -- [VERSION] [--runtime-only]]"
            echo "  VERSION        a published Release tag (default: latest)"
            echo "  --runtime-only skip the developer toolchain"
            exit 0
            ;;
        -*)
            echo "ERROR: unknown option '$arg'." >&2
            exit 1
            ;;
        *)
            TAG="$arg"
            ;;
    esac
done

# ----- Root check -----------------------------------------------------------
if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: this installer must run as root (use sudo)." >&2
    exit 1
fi

# ----- Detect distro family -------------------------------------------------
ID=""
ID_LIKE=""
if [ -r /etc/os-release ]; then
    # shellcheck disable=SC1091
    . /etc/os-release
fi
FAMILY=""
case " ${ID:-} ${ID_LIKE:-} " in
    *" debian "*|*" ubuntu "*)
        FAMILY="debian"
        ;;
    *" rhel "*|*" fedora "*|*" centos "*)
        FAMILY="rhel"
        ;;
esac
if [ -z "$FAMILY" ]; then
    echo "ERROR: unsupported distribution (ID='${ID:-?}')." >&2
    echo "       Supported: Debian/Ubuntu and RHEL/Rocky/Alma/Fedora." >&2
    echo "       Build from source: https://doc.yuneta.io/installation/" >&2
    exit 1
fi

# ----- Detect architecture & package flavour --------------------------------
ARCH_RAW=$(uname -m)
if [ "$FAMILY" = "debian" ]; then
    EXT="deb"
    case "$ARCH_RAW" in
        x86_64|amd64)
            PKGARCH="amd64"
            ;;
        armv6l|armv7l)
            PKGARCH="armhf"
            ;;
        riscv64)
            PKGARCH="riscv64"
            ;;
        *)
            PKGARCH=""
            ;;
    esac
    ASSET_RE="https://github.com/${REPO}/releases/download/[^\"]*yuneta-agent-[^\"]*-${PKGARCH}\.deb"
else
    EXT="rpm"
    case "$ARCH_RAW" in
        x86_64|amd64)
            PKGARCH="x86_64"
            ;;
        aarch64|arm64)
            PKGARCH="aarch64"
            ;;
        riscv64)
            PKGARCH="riscv64"
            ;;
        *)
            PKGARCH=""
            ;;
    esac
    ASSET_RE="https://github.com/${REPO}/releases/download/[^\"]*yuneta-agent-[^\"]*\.${PKGARCH}\.rpm"
fi
if [ -z "$PKGARCH" ]; then
    echo "ERROR: no published ${EXT} for arch '$ARCH_RAW'." >&2
    echo "       Build from source: https://doc.yuneta.io/installation/" >&2
    exit 1
fi

# ----- Sanity: required commands --------------------------------------------
if [ "$FAMILY" = "debian" ]; then
    REQ="curl dpkg apt-get"
else
    REQ="curl dnf rpm"
fi
for cmd in $REQ; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "ERROR: required command '$cmd' not found in PATH." >&2
        exit 1
    fi
done

# ----- RHEL: enable EPEL + CRB before installing ----------------------------
# The package's weak deps (pipx, fail2ban) and the developer toolchain
# (mercurial, ninja-build, pipx, -devel/-static) live in EPEL/CRB. dnf cannot
# pull a package from a repo it enables in the same transaction, so enable them
# up front, in this flow — not as a separate step the user has to remember.
if [ "$FAMILY" = "rhel" ] && [ "${ID:-}" != "fedora" ]; then
    echo "[i] Enabling EPEL + CRB..."
    dnf -y install epel-release || true
    if command -v crb >/dev/null 2>&1; then
        crb enable || true
    else
        dnf config-manager --set-enabled crb 2>/dev/null \
            || dnf config-manager --set-enabled powertools 2>/dev/null \
            || true
    fi
fi

# ----- Locate the asset URL -------------------------------------------------
if [ "$TAG" = "latest" ]; then
    API_URL="https://api.github.com/repos/${REPO}/releases/latest"
else
    API_URL="https://api.github.com/repos/${REPO}/releases/tags/${TAG}"
fi

echo "[i] Querying release: $API_URL"
JSON=$(curl -fsSL "$API_URL") || {
    echo "ERROR: failed to query release '${TAG}' for ${REPO}." >&2
    exit 1
}

ASSET_URL=$(printf '%s' "$JSON" | grep -o "$ASSET_RE" | head -n1)
if [ -z "$ASSET_URL" ]; then
    echo "ERROR: no '${PKGARCH}' ${EXT} asset found in release '${TAG}'." >&2
    echo "       See https://github.com/${REPO}/releases for what's published." >&2
    echo "       Or build from source: https://doc.yuneta.io/installation/" >&2
    exit 1
fi

# ----- Download & install the package ---------------------------------------
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
PKG="${TMP}/$(basename "$ASSET_URL")"

echo "[i] Downloading $(basename "$PKG")"
curl -fsSL -o "$PKG" "$ASSET_URL"

# apt fetches as the unprivileged '_apt' user even for a local file. mktemp
# gives us a 0700 root-owned dir it cannot traverse, so apt drops its sandbox
# and says so in an N: line. Nothing breaks, but the warning is noise on an
# otherwise clean install — let '_apt' read the file instead.
chmod 0755 "$TMP"
chmod 0644 "$PKG"

echo "[i] Installing $(basename "$PKG")"
if [ "$FAMILY" = "debian" ]; then
    # apt-get resolves the package's deps from a local file (needs a path with a
    # '/', which $PKG always has). `dpkg -i` cannot: it leaves the package
    # unconfigured and prints a wall of dependency errors on every clean install
    # — `gdb` did exactly that on 7.8.5. The dpkg path stays only as a fallback
    # for an apt too old to take a file argument.
    if ! apt-get install -y "$PKG"; then
        dpkg -i "$PKG" || apt-get install -y -f
    fi
else
    # dnf resolves the package's deps from the now-enabled repos.
    dnf -y install "$PKG"
fi
echo "[✓] Yuneta runtime installed."

# ----- Certbot (TLS for the bundled web server) -----------------------------
# Runtime/ops tool — installed on BOTH distros, regardless of --runtime-only,
# via the bundled distro-correct helper (snap on Debian, EPEL dnf on RHEL).
# Both families ship the helper as install-certbot.sh since 7.8.6; the -snap
# name is the pre-7.8.6 Debian one, kept for a node installing an older release.
CERTBOT_HELPER="/yuneta/bin/install-certbot.sh"
if [ ! -x "$CERTBOT_HELPER" ] && [ -x /yuneta/bin/install-certbot-snap.sh ]; then
    CERTBOT_HELPER="/yuneta/bin/install-certbot-snap.sh"
fi
echo "[i] Installing certbot..."
if [ -x "$CERTBOT_HELPER" ]; then
    if "$CERTBOT_HELPER"; then
        echo "[✓] certbot installed."
    else
        echo "[!] certbot install reported issues (see above)." >&2
    fi
else
    echo "[!] $CERTBOT_HELPER not found; skipping certbot." >&2
fi

# ----- Developer toolchain (default on; --runtime-only to skip) -------------
# Delegates to the bundled, distro-correct, resilient dev-deps helper so the
# package list has a single source of truth. This keeps everything in one run.
want_dev() {
    # No prompts, no stops: install the toolchain unless --runtime-only.
    if [ "$RUNTIME_ONLY" = "1" ]; then
        return 1
    fi
    return 0
}

if want_dev; then
    echo "[i] Installing developer toolchain..."
    if [ -x /yuneta/bin/install-yuneta-dev-deps.sh ]; then
        if /yuneta/bin/install-yuneta-dev-deps.sh; then
            echo "[✓] Developer toolchain installed."
        else
            echo "[!] Developer toolchain install reported issues (see above)." >&2
        fi
    else
        echo "[!] /yuneta/bin/install-yuneta-dev-deps.sh not found; skipping toolchain." >&2
    fi
else
    echo "[i] Skipped developer toolchain (pure runtime install)."
    echo "    Install it later: sudo /yuneta/bin/install-yuneta-dev-deps.sh"
fi

# ----- Verify the agent came up ---------------------------------------------
# The package's postinst starts the service through invoke-rc.d and ignores the
# result; on a systemd box the init script's output goes to the journal, so
# nothing about the start ever reaches this terminal. Without this check the
# run ends on a green tick whether or not anything is running. Ask the
# processes directly — the init script's own `status` exit code cannot be used,
# it reports the WEB server (absent on a fresh box), not the agent.
agent_running() {
    pat="$1"
    if command -v pgrep >/dev/null 2>&1; then
        pgrep -f "/agent/${pat}( |\$)" >/dev/null 2>&1
    else
        pidof -x "$pat" >/dev/null 2>&1
    fi
}

# --start forks and returns, so give the daemon a moment to appear.
AGENT_UP=0
tries=0
while [ "$tries" -lt 10 ]; do
    if agent_running yuneta_agent; then
        AGENT_UP=1
        break
    fi
    tries=$((tries + 1))
    sleep 1
done

AGENT22_UP=0
if agent_running yuneta_agent22; then
    AGENT22_UP=1
fi

echo ""
echo "    Re-login (or 'source /etc/profile.d/yuneta.sh') to pick up PATH."
echo "    Service: 'sudo service yuneta_agent status'"
echo ""
if [ "$AGENT_UP" = "1" ]; then
    if [ "$AGENT22_UP" = "1" ]; then
        echo "[✓] Done — yuneta_agent and yuneta_agent22 are running."
    else
        echo "[✓] Done — yuneta_agent is running (yuneta_agent22 is NOT)."
    fi
else
    echo "[!] Installed, but yuneta_agent is NOT running." >&2
    echo "    Start it:  sudo service yuneta_agent start" >&2
    echo "    Why not:   sudo journalctl -t yuneta_agent_init -n 50" >&2
    echo "               ls -lt /yuneta/realms/*/*/logs/" >&2
    exit 1
fi
