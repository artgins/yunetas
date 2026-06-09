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

echo "[i] Installing $(basename "$PKG")"
if [ "$FAMILY" = "debian" ]; then
    # dpkg first; apt-get -f resolves any missing deps.
    if ! dpkg -i "$PKG"; then
        apt-get install -y -f
    fi
else
    # dnf resolves the package's deps from the now-enabled repos.
    dnf -y install "$PKG"
fi
echo "[✓] Yuneta runtime installed."

# ----- Certbot (TLS for the bundled web server) -----------------------------
# Runtime/ops tool — installed on BOTH distros, regardless of --runtime-only,
# via the bundled distro-correct helper (snap on Debian, EPEL dnf on RHEL).
if [ "$FAMILY" = "debian" ]; then
    CERTBOT_HELPER="/yuneta/bin/install-certbot-snap.sh"
else
    CERTBOT_HELPER="/yuneta/bin/install-certbot.sh"
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
    if [ "$RUNTIME_ONLY" = "1" ]; then
        return 1
    fi
    # Prompt only when a real terminal is reachable (works under curl|sh too,
    # where stdin is the pipe but /dev/tty is the controlling terminal).
    if [ -r /dev/tty ] && [ -w /dev/tty ]; then
        printf "Install the developer toolchain (git, mercurial, clang, gcc, cmake, ...)? [Y/n] " > /dev/tty
        ans=""
        read ans < /dev/tty || ans=""
        case "$ans" in
            [nN]*)
                return 1
                ;;
            *)
                return 0
                ;;
        esac
    fi
    # No TTY (piped/CI): default to installing everything.
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

echo ""
echo "[✓] Done."
echo "    Re-login (or 'source /etc/profile.d/yuneta.sh') to pick up PATH."
echo "    Service: 'sudo service yuneta_agent status'"
