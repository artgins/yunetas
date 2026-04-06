#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

# Build static HTML with empty BASE_URL so the site works at the domain root
export BASE_URL=""
myst build --html

ORIGIN="./_build/html/"
DESTINE="yuneta@yuneta.io:/yuneta/gui/doc.yuneta.io/"
rsync -auvzL -e ssh \
    $ORIGIN $DESTINE
