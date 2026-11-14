#!/bin/bash
#
#   Copyright (c) 2026, ArtGins.
#
#   Prepare the work directory to run mosquitto's broker test suite against
#   the yuneta mqtt_broker (see README.md).
#
#   - Fetches test/ of Eclipse Mosquitto at the pinned commit (the suite is not
#     copied into this repo: it is EPL-2.0 OR BSD-3-Clause and it is fetched).
#   - Applies mosq_test.patch.
#
#   Usage: setup.sh [work_dir]     (default: $MOSQ_WORK or ~/.yuneta/mosquitto-suite)
#
set -e

#
#   Configuration
#
MOSQ_REPO=https://github.com/eclipse-mosquitto/mosquitto.git
MOSQ_COMMIT=fda14f2c196d9d2fcdd07d6c5d31a673524f7368    # 2.1.2 develop, 2026-11-05
HERE=$(cd "$(dirname "$0")" && pwd)
W=${1:-${MOSQ_WORK:-$HOME/.yuneta/mosquitto-suite}}

#
#   Fetch the suite
#
mkdir -p "$W"
W=$(cd "$W" && pwd)
SRC=$W/mosquitto-src
if [ ! -d "$SRC/.git" ]; then
    git clone --filter=blob:none --no-checkout "$MOSQ_REPO" "$SRC"
fi
git -C "$SRC" fetch --quiet origin "$MOSQ_COMMIT" || true
git -C "$SRC" sparse-checkout set test
git -C "$SRC" -c advice.detachedHead=false checkout --quiet "$MOSQ_COMMIT"
echo "mosquitto suite at $(git -C "$SRC" log -1 --format='%h %cd %s' --date=short)"

#
#   Working copy of test/ with the patch
#
rm -rf "$W/test"
cp -r "$SRC/test" "$W/test"
patch -p1 -d "$W" < "$HERE/mosq_test.patch"
mkdir -p "$W/root/src" "$W/runs"

echo
echo "Work directory ready: $W"
echo "Run the suite with:   MOSQ_WORK=$W $HERE/run_all.sh"
