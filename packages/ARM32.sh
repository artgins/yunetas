#!/bin/sh
# Build the AMD64 .deb using VERSION from ../YUNETA_VERSION (as-is, e.g. 7.0.0-b10)
# HACK: Must be executed from this script's directory.
set -eu

# Resolve script dir and enforce CWD == script dir
SCRIPT_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" 2>/dev/null && pwd -P)
CWD=$(pwd -P)
if [ "$CWD" != "$SCRIPT_DIR" ]; then
    echo "ERROR: Run this script from its directory: $SCRIPT_DIR" >&2
    exit 2
fi

PROJECT="yuneta-agent"
ARCHITECTURE="arm32"
RELEASE="2"   # manual control

VER_FILE="../YUNETA_VERSION"
if [ ! -r "$VER_FILE" ]; then
    echo "ERROR: Version file not found: $VER_FILE" >&2
    exit 1
fi

# Parse YUNETA_VERSION=... without sourcing; keep value as-is (e.g. 7.0.0-b10)
RAW_VER=$(sed -n 's/^[[:space:]]*YUNETA_VERSION[[:space:]]*=[[:space:]]*//p' "$VER_FILE" | head -n1)
RAW_VER=${RAW_VER%%\#*}
RAW_VER=${RAW_VER#\"}; RAW_VER=${RAW_VER%\"}
RAW_VER=${RAW_VER#\'}; RAW_VER=${RAW_VER%\'}
RAW_VER=$(printf "%s" "$RAW_VER" | tr -d '\r')
VERSION=$(printf "%s" "$RAW_VER" | awk '{$1=$1;print}')

if [ -z "$VERSION" ]; then
    echo "ERROR: Empty YUNETA_VERSION in $VER_FILE" >&2
    exit 1
fi

if [ ! -x ./make-yuneta-agent-deb.sh ]; then
    echo "ERROR: ./make-yuneta-agent-deb.sh not found or not executable in $SCRIPT_DIR" >&2
    exit 1
fi

echo "[i] PROJECT=$PROJECT  VERSION=$VERSION  RELEASE=$RELEASE  ARCH=$ARCHITECTURE"
exec ./make-yuneta-agent-deb.sh "$PROJECT" "$VERSION" "$RELEASE" "$ARCHITECTURE"
