#!/usr/bin/env bash
#
#   verify-package-transition.sh
#
#   Check that a package upgrade does NOT destroy a configuration file
#   owned by the node, when the new version has stopped shipping it.
#
#   Two levels:
#     A) reproduce, with real dpkg, that the obsolete file IS deleted
#     B) run the SHIPPED hooks against a fake root
#
#   Usage:
#     ./verify-package-transition.sh                 # level A only
#     ./verify-package-transition.sh PRE POST        # A + B
#
#   PRE and POST are the REAL hooks, extracted from the package:
#     dpkg-deb -I yours.deb preinst  > /tmp/pre
#     dpkg-deb -I yours.deb postinst > /tmp/post
#     rpm -qp --scripts yours.rpm                    # %pre / %posttrans
#
#   Nothing here touches the machine outside $WORK.
#
#   Copyright (c) 2026, ArtGins.
#
set -euo pipefail

PREFIX="/opt/example"                  # adjust to your package's prefix
CONF_DIR="$PREFIX/conf"
CONF="$CONF_DIR/app.conf"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "== A) the manager deletes what the new version no longer ships =="

mkdir -p "$WORK/root/var/lib/dpkg/info" \
         "$WORK/root/var/lib/dpkg/updates" \
         "$WORK/root/var/lib/dpkg/triggers"
: > "$WORK/root/var/lib/dpkg/status"
: > "$WORK/root/var/lib/dpkg/available"

build_probe() {
    #  $1 = dir   $2 = version   $3 = "owns" to include the file
    mkdir -p "$1/DEBIAN" "$1$CONF_DIR"
    cat > "$1/DEBIAN/control" <<CTRL
Package: transition-probe
Version: $2
Architecture: all
Maintainer: probe <probe@example>
Description: transition probe
CTRL
    if [ "$3" = "owns" ]; then
        echo "STOCK" > "$1$CONF"
    fi
    dpkg-deb --build --root-owner-group "$1" "$WORK/$2.deb" > /dev/null
}

build_probe "$WORK/old" 1.0 owns      # old version: owns the file
build_probe "$WORK/new" 2.0 no        # new version: no longer ships it

#  NOTE: --force-script-chrootless would run maintainer scripts against
#  the REAL /. These probes carry no scripts, so here we only observe the
#  unpack. Level B exercises the hooks separately.
dpkg --root="$WORK/root" --force-not-root --force-script-chrootless \
     -i "$WORK/1.0.deb" > /dev/null 2>&1

echo "MY-CONFIG" > "$WORK/root$CONF"          # the operator writes theirs

dpkg --root="$WORK/root" --force-not-root --force-script-chrootless \
     -i "$WORK/2.0.deb" > /dev/null 2>&1

if [ -e "$WORK/root$CONF" ]; then
    echo "   the file survived the unpack"
    echo "   (check the premise: was it really package-owned?)"
else
    echo "   CONFIRMED: the upgrade deletes the obsolete file"
fi

PRE="${1:-}"
POST="${2:-}"

if [ -z "$PRE" ] || [ -z "$POST" ]; then
    echo
    echo "== B) skipped: pass the real hooks as arguments =="
    exit 0
fi

echo
echo "== B) the SHIPPED hooks against a fake root =="

FAKE="$WORK/fake"
mkdir -p "$FAKE/conf"

unshare -r -m sh -c "
    mount --bind '$FAKE' '$PREFIX' || exit 9

    echo 'STOCK'     > '$CONF.default'
    echo 'MY-CONFIG' > '$CONF'

    sh '$PRE'  > /dev/null 2>&1       # saves the copy
    rm -f '$CONF'                     # <-- what the manager does
    sh '$POST' 2>&1 | grep -vi chown  # restores

    printf '   result: %s\n' \"\$(cat '$CONF' 2>/dev/null || echo MISSING)\"
"

echo
echo "   expected: MY-CONFIG   (the operator's, not STOCK)"
