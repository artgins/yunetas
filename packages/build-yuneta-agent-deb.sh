#!/usr/bin/env bash
#######################################################################
#               build-yuneta-agent-deb.sh
#######################################################################
# Build a .deb for Yunetas Agent (no init integration yet)
#
# Usage:
#   ./build-yuneta-agent-deb.sh <project> <version> <release> <arch>
#
# Example:
#   ./build-yuneta-agent-deb.sh yuneta-agent 7.0.0 1 amd64
#
# Notes:
# - Script must live in yunetas/packages/
# - Builds under ./build/ and places the final .deb in ./dist (fixed)
#######################################################################

set -euo pipefail
umask 022

# Resolve the script's real directory (yunetas/packages) to anchor relative paths.
# HACK: Works regardless of launch location (cron, make, symlink, different CWD).
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd -P)"
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
mkdir -p "${WORKDIR}/yuneta/agent"
mkdir -p "${WORKDIR}/yuneta/bin"
mkdir -p "${WORKDIR}/yuneta/gui"
mkdir -p "${WORKDIR}/yuneta/realms"
mkdir -p "${WORKDIR}/yuneta/repos"
mkdir -p "${WORKDIR}/yuneta/store/certs/private"  # private will be 0700 in postinst
mkdir -p "${WORKDIR}/yuneta/share"

# --- Binaries to include (must exist in BIN_DIR) ---
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

# Copy binaries with correct perms; verify existence
for BINARY in "${BINARIES[@]}"; do
    SRC="${BIN_DIR%/}/${BINARY}"
    if [ ! -x "${SRC}" ]; then
        echo "[-] Missing or non-executable binary: ${SRC}" >&2
        exit 2
    fi
    install -D -m 0755 "${SRC}" "${WORKDIR}/yuneta/bin/${BINARY}"
done

# Optional profile snippet
cat > "${WORKDIR}/etc/profile.d/yuneta.sh" <<'EOF'
# Yuneta environment
export YUNETA_DIR=/yuneta
export PATH="/yuneta/bin:$PATH"
EOF
chmod 0644 "${WORKDIR}/etc/profile.d/yuneta.sh"

# --- control file ---
cat > "${WORKDIR}/DEBIAN/control" <<EOF
Package: ${PROJECT}
Version: ${VERSION}-${RELEASE}
Section: utils
Priority: optional
Architecture: ${ARCHITECTURE}
Maintainer: ArtGins S.L. <support@artgins.com>
Depends: adduser
Description: Yunetas Agent
 Yunetas Agent binaries and runtime directories.
EOF

# --- postinst (set ownership/perms, handle private certs) ---
cat > "${WORKDIR}/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
# Create yuneta system user if missing
if ! id -u yuneta >/dev/null 2>&1; then
    adduser --system --no-create-home --group yuneta || true
fi

# Set ownership for runtime dirs
if [ -d /yuneta ]; then
    chown -R yuneta:yuneta /yuneta || true
fi

# Enforce strict perms on private certs directory
if [ -d /yuneta/store/certs/private ]; then
    chmod 0700 /yuneta/store/certs/private || true
fi

exit 0
EOF
chmod 0755 "${WORKDIR}/DEBIAN/postinst"

# --- prerm (placeholder) ---
cat > "${WORKDIR}/DEBIAN/prerm" <<'EOF'
#!/bin/sh
set -e
exit 0
EOF
chmod 0755 "${WORKDIR}/DEBIAN/prerm"

# --- postrm (purge cleanup placeholder) ---
cat > "${WORKDIR}/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e
# For purge-time cleanup, uncomment if desired:
# if [ "$1" = "purge" ]; then
#     deluser --system yuneta 2>/dev/null || true
#     # rm -rf /yuneta
# fi
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

# All control files default to 0644
find "${WORKDIR}/DEBIAN" -type f -print0 | xargs -0 chmod 0644

# Maintainer scripts must be executable
chmod 0755 "${WORKDIR}/DEBIAN/postinst" \
            "${WORKDIR}/DEBIAN/prerm" \
            "${WORKDIR}/DEBIAN/postrm"

# ---------- Build .deb (root ownership inside package) ----------
mkdir -p "${BASE_DEST}"
OUT_DEB_WORK="${BASE_DEST}/${PACKAGE}.deb"
echo "[i] Building ${OUT_DEB_WORK}"
dpkg-deb --build --root-owner-group "${WORKDIR}" "${OUT_DEB_WORK}"

# ---------- Place final artifact in ./dist ----------
mkdir -p "${OUT_DIR}"
FINAL_DEB="${OUT_DIR}/${PACKAGE}.deb"
mv -f "${OUT_DEB_WORK}" "${FINAL_DEB}"

echo "[✓] Final package:"
echo "    ${FINAL_DEB}"
echo
echo "Install with:"
echo "  sudo apt -y install '${FINAL_DEB}'"
