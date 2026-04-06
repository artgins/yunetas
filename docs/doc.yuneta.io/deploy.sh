#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

# Build static HTML with empty BASE_URL so the site works at the domain root
# myst build --html spawns a temporary node server that doesn't always exit;
# snapshot node PIDs before and kill only the new ones after the build.
export BASE_URL=""
BEFORE=$(pgrep -x node 2>/dev/null | sort || true)
myst build --html
AFTER=$(pgrep -x node 2>/dev/null | sort || true)
NEW_PIDS=$(comm -13 <(echo "$BEFORE") <(echo "$AFTER"))
if [ -n "$NEW_PIDS" ]; then
    kill $NEW_PIDS 2>/dev/null || true
fi

ORIGIN="./_build/html/"
DESTINE="yuneta@yuneta.io:/yuneta/gui/doc.yuneta.io/"
rsync -auvzL -e ssh \
    $ORIGIN $DESTINE
