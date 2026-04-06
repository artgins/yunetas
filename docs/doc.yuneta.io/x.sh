#!/usr/bin/env bash
# Build the documentation locally (no deploy).
# Output: _build/html/
set -euo pipefail

cd "$(dirname "$0")"

export BASE_URL=""
BEFORE=$(pgrep -x node 2>/dev/null | sort || true)
myst build --html
AFTER=$(pgrep -x node 2>/dev/null | sort || true)
NEW_PIDS=$(comm -13 <(echo "$BEFORE") <(echo "$AFTER"))
if [ -n "$NEW_PIDS" ]; then
    kill $NEW_PIDS 2>/dev/null || true
fi

echo "Build complete: _build/html/"
