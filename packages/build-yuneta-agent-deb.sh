#!/usr/bin/env bash
#######################################################################
#               build-yuneta-agent-deb.sh
#######################################################################
# Build a .deb for Yunetas Agent
#
# Usage:
#   ./build-yuneta-agent-deb.sh <project> <version> <release> <arch> [--bin-dir <dir>]
# Example:
#   ./build-yuneta-agent-deb.sh yuneta-agent 7.0.0 1 amd64 --bin-dir ./out/bin
#
# Notes:
# - Writes to ./build/deb/<arch>/<project>-<version>-<release>-<arch>.deb
#######################################################################

set -euo pipefail

if [ "${1:-}" = "--help" ] || [ "$#" -lt 4 ]; then
    echo "Usage: $0 <project> <version> <release> <arch> [--bin-dir <dir>]"
    exit 1
fi

PROJECT="$1"; shift
VERSION="$1"; shift
RELEASE="$1"; shift
ARCHITECTURE="$1"; shift

PACKAGE="${PROJECT}-${VERSION}-${RELEASE}-${ARCHITECTURE}"
BASE_DEST="./build/deb/${ARCHITECTURE}"
WORKDIR="${BASE_DEST}/${PACKAGE}"
BIN_DIR="/yuneta/bin/"

echo "[i] Building package tree at: ${WORKDIR}"

# Fresh workspace
rm -rf "${WORKDIR}"
mkdir -p "${WORKDIR}/DEBIAN"
mkdir -p "${WORKDIR}/etc/profile.d"
mkdir -p "${WORKDIR}/etc/init.d"
mkdir -p "${WORKDIR}/yuneta/agent"
mkdir -p "${WORKDIR}/yuneta/bin"
mkdir -p "${WORKDIR}/yuneta/gui"
mkdir -p "${WORKDIR}/yuneta/realms"
mkdir -p "${WORKDIR}/yuneta/repos"
mkdir -p "${WORKDIR}/yuneta/store/certs"
mkdir -p "${WORKDIR}/yuneta/store/certs/private" # TODO permission only for yuneta user
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

# --- Minimal control file (adjust fields as needed) ---
# Tip: keep Depends small; add python3 only if truly needed
cat > "${WORKDIR}/DEBIAN/control" <<EOF
Package: ${PROJECT}
Version: ${VERSION}-${RELEASE}
Section: utils
Priority: optional
Architecture: ${ARCHITECTURE}
Maintainer: ArtGins S.L. <administracion@artgins.com>
Description: Yunetas Agent
 Yunetas Agent binaries and runtime directories.
Depends: adduser, libc6
EOF

# --- postinst (POSIX sh) ---
cat > "${WORKDIR}/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
# Create yuneta system user if missing
if ! id -u yuneta >/dev/null 2>&1; then
    adduser --system --no-create-home --group yuneta || true
fi
# Set ownership for runtime dirs
chown -R yuneta:yuneta /yuneta || true

# If using systemd, you could enable/start here (optional):
# if command -v systemctl >/dev/null 2>&1; then
#     systemctl daemon-reload || true
#     systemctl enable yuneta-agent.service || true
#     systemctl start yuneta-agent.service || true
# fi

exit 0
EOF
chmod 0755 "${WORKDIR}/DEBIAN/postinst"

# --- prerm (optional; stop services before removal) ---
cat > "${WORKDIR}/DEBIAN/prerm" <<'EOF'
#!/bin/sh
set -e
# if command -v systemctl >/dev/null 2>&1; then
#     systemctl stop yuneta-agent.service || true
# fi
exit 0
EOF
chmod 0755 "${WORKDIR}/DEBIAN/prerm}"

# --- postrm (purge cleanup) ---
cat > "${WORKDIR}/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e
if [ "$1" = "purge" ]; then
    # Remove user and data only on purge; be conservative
    deluser --system yuneta 2>/dev/null || true
    # rm -rf /yuneta   # Uncomment if you want a full purge
fi
exit 0
EOF
chmod 0755 "${WORKDIR}/DEBIAN/postrm"

# Ensure directory permissions (dirs 755, regular files already set)
find "${WORKDIR}" -type d -print0 | xargs -0 chmod 0755

# Build .deb (root ownership inside package)
OUT_DEB="${BASE_DEST}/${PACKAGE}.deb"
echo "[i] Building ${OUT_DEB}"
dpkg-deb --build --root-owner-group "${WORKDIR}" "${OUT_DEB}"

echo "[✓] Done: ${OUT_DEB}"
echo "Install with:"
echo "  sudo apt -y install ./$(basename "${OUT_DEB}")"
