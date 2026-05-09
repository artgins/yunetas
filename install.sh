#!/bin/sh
#
# Yuneta runtime installer.
#
# Pulls the matching .deb from a GitHub Release and installs it via apt.
# Use this to install the runtime on a server. To develop the framework,
# follow the "Build from source" steps at https://doc.yuneta.io/installation/
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/artgins/yunetas/main/install.sh | sudo sh
#
# Pin a version (must exist as a GitHub Release):
#   curl -fsSL https://raw.githubusercontent.com/artgins/yunetas/main/install.sh | sudo sh -s -- 7.3.1
#
# Environment overrides:
#   YUNETA_REPO    GitHub repo, default "artgins/yunetas"
#   YUNETA_TAG     Release tag, default "latest" (or first positional arg)
#

set -eu

REPO="${YUNETA_REPO:-artgins/yunetas}"
TAG="${YUNETA_TAG:-${1:-latest}}"

# ----- Detect architecture --------------------------------------------------
ARCH_RAW=$(uname -m)
case "$ARCH_RAW" in
    x86_64|amd64)         ARCH="amd64" ;;
    armv6l|armv7l)        ARCH="armhf" ;;
    riscv64)              ARCH="riscv64" ;;
    *)
        echo "ERROR: no published .deb for arch '$ARCH_RAW'." >&2
        echo "       Supported: amd64, armhf, riscv64." >&2
        echo "       Build from source: https://doc.yuneta.io/installation/" >&2
        exit 1
        ;;
esac

# ----- Sanity checks --------------------------------------------------------
if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: this installer must run as root (use sudo)." >&2
    exit 1
fi

for cmd in curl dpkg apt-get; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "ERROR: required command '$cmd' not found in PATH." >&2
        exit 1
    fi
done

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

# Pull the first browser_download_url that matches the expected name.
ASSET_URL=$(printf '%s' "$JSON" \
    | grep -o "https://github.com/${REPO}/releases/download/[^\"]*yuneta-agent-[^\"]*-${ARCH}\.deb" \
    | head -n1)

if [ -z "$ASSET_URL" ]; then
    echo "ERROR: no '${ARCH}' .deb asset found in release '${TAG}'." >&2
    echo "       See https://github.com/${REPO}/releases for what's published." >&2
    echo "       Or build from source: https://doc.yuneta.io/installation/" >&2
    exit 1
fi

# ----- Download & install ---------------------------------------------------
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
DEB="${TMP}/$(basename "$ASSET_URL")"

echo "[i] Downloading $(basename "$DEB")"
curl -fsSL -o "$DEB" "$ASSET_URL"

echo "[i] Installing $DEB"
# dpkg first; apt-get -f resolves any missing deps.
if ! dpkg -i "$DEB"; then
    apt-get install -y -f
fi

echo "[✓] Yuneta runtime installed."
echo "    Re-login (or 'source /etc/profile.d/yuneta.sh') to pick up PATH."
echo "    Service: 'sudo service yuneta_agent status'"
