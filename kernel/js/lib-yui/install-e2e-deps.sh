#!/usr/bin/env bash
# install-e2e-deps.sh — install everything Playwright needs to run the
# lib-yui end-to-end test suite locally on Linux.
#
# Browsers (Chromium / Firefox / WebKit) are downloaded by Playwright
# itself; system libraries the WebKit build links against are not
# bundled and must be installed via apt.  Chromium and Firefox bundle
# all their deps, so this script only has to cover WebKit.
#
# Run it once after `npm install`:
#
#       cd kernel/js/lib-yui
#       npm install
#       ./install-e2e-deps.sh
#       npm run test:e2e
#
# After this, `npm run test:e2e` works without any sudo prompt.

set -eu

SCRIPT_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" 2>/dev/null && pwd -P)
cd "$SCRIPT_DIR"

if [ ! -d node_modules/@playwright ]; then
    echo "ERROR: @playwright/test not installed.  Run 'npm install' first." >&2
    exit 1
fi

echo "[1/2] Installing Playwright browser binaries (chromium, firefox, webkit) — no sudo needed..."
npx playwright install chromium firefox webkit

echo "[2/2] Installing apt deps WebKit links against (libgstreamer-plugins-bad1.0-0, libavif16, ...) — needs sudo..."
sudo npx playwright install-deps webkit

echo
echo "Done.  Run the suite with:"
echo "    npm run test:e2e"
