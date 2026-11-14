#!/bin/bash
#
#   Copyright (c) 2026, ArtGins.
#
#   Run mosquitto's broker tests one at a time against the yuneta mqtt_broker
#   (see README.md). Run setup.sh once first.
#
#   Usage: run_all.sh [test.py ...]       (default: every broker test)
#
#   Environment:
#       MOSQ_WORK       work directory made by setup.sh (default ~/.yuneta/mosquitto-suite)
#       MQTT_BROKER     broker binary to test (default $YUNETAS_YUNOS/mqtt_broker,
#                       else /yuneta/development/yunetas/outputs/yunos/mqtt_broker)
#       OUT             results directory (default $MOSQ_WORK/results-<date>)
#
HERE=$(cd "$(dirname "$0")" && pwd)
W=${MOSQ_WORK:-$HOME/.yuneta/mosquitto-suite}
YUNOS=${YUNETAS_YUNOS:-/yuneta/development/yunetas/outputs/yunos}
BROKER=${MQTT_BROKER:-$YUNOS/mqtt_broker}
OUT=${OUT:-$W/results-$(date +%Y%m%d-%H%M%S)}

if [ ! -d "$W/test/broker" ]; then
    echo "No suite in $W: run $HERE/setup.sh first" >&2
    exit 1
fi
if [ ! -x "$BROKER" ]; then
    echo "No broker binary at $BROKER (set MQTT_BROKER)" >&2
    exit 1
fi

#
#   Install the broker, its template config and the stand-in (a copy each
#   run, so a rebuild during the run does not change what is tested)
#
mkdir -p "$OUT" "$W/root/src" "$W/runs"
cp "$BROKER" "$W/mqtt_broker"
cp "$HERE/../mqtt_broker.json" "$W/mqtt_broker.json"
cp "$HERE/standin_mosquitto.py" "$W/root/src/mosquitto"
chmod +x "$W/root/src/mosquitto"
"$W/mqtt_broker" --version > "$OUT/broker.txt" 2>&1
echo "broker: $BROKER ($(head -1 "$OUT/broker.txt"))"
echo "results: $OUT"

. "$HERE/features.env"
cd "$W/test/broker" || exit 1
LIST="$@"
if [ -z "$LIST" ]; then
    LIST=$(ls *.py | grep -v -E 'mosq_test_helper|_helper\.py|^persist_|msg_sequence_test|^test\.py|dynsec_helper')
fi

#
#   Kill a broker of this work directory left by a test (by its executable)
#
kill_mine() {
    for p in $(pgrep -x mqtt_broker); do
        exe=$(readlink /proc/$p/exe 2>/dev/null)
        if [ "$exe" = "$W/mqtt_broker" ]; then
            kill -9 $p
            echo "killed leftover $p" >&2
        fi
    done
}

#
#   Run
#
for t in $LIST; do
    kill_mine
    start=$(date +%s%N)
    BUILD_ROOT=$W/root timeout -k 5 60 python3 $t > "$OUT/$t.txt" 2>&1 < /dev/null
    rc=$?
    end=$(date +%s%N)
    echo "$t $rc $(( (end - start) / 1000000 ))ms" | tee -a "$OUT/summary.txt"
    kill_mine
    rm -rf "$W"/runs/ybroker-*
done

echo
if [ $# -eq 0 ] && [ -f "$HERE/baseline.txt" ]; then
    python3 "$HERE/classify.py" "$OUT/summary.txt" "$HERE/baseline.txt" | grep -E '^(CHANGED|COUNTS|\| .* \| NEW \|)'
else
    python3 "$HERE/classify.py" "$OUT/summary.txt" | grep -E '^(COUNTS|\| .* \| NEW \|)'
fi
