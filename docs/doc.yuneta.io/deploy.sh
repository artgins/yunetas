#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"
REPO_ROOT="$(git rev-parse --show-toplevel)"

# Stamp current version from YUNETA_VERSION into index.md
VERSION=$(sed 's/YUNETA_VERSION=//' "$REPO_ROOT/YUNETA_VERSION")
TODAY=$(date +%Y-%m-%d)
sed -i "s|\*\*Current version: \[.*\](.*)\*\*|\*\*Current version: [${VERSION}](https://github.com/artgins/yunetas/tree/${VERSION})\*\*|" index.md
sed -i "s|\*Documentation updated: .*\*|\*Documentation updated: ${TODAY}\*|" index.md

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

# Safety guard: a failed/empty build must never reach the --delete rsync, or it
# would wipe the live site. Require the built entry page before syncing.
if [ ! -f "${ORIGIN}index.html" ]; then
    echo "ERROR: ${ORIGIN}index.html missing; build failed or incomplete — aborting deploy." >&2
    exit 1
fi

# --delete mirrors the build onto the server: pages and content-hashed assets
# dropped from the build (renamed/moved TOC nodes, stale assets) are removed on
# the server too. --delete-after defers removals until the transfer succeeds.
rsync -auvzL --delete-after -e ssh \
    "$ORIGIN" "$DESTINE"
