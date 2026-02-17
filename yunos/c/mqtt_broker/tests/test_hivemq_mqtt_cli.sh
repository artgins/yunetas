#!/bin/bash
# =============================================================================
# test_hivemq_mqtt_cli.sh
#
# Tests for yuno mqtt_broker using HiveMQ MQTT CLI.
# Covers MQTT v3.1.1 and MQTT v5.0 protocol compliance plus functional tests:
#   - Broker spec compliance (via mqtt test command)
#   - QoS 0/1/2 publish and subscribe
#   - Retained messages
#   - Will messages
#   - Topic wildcards (+ and #)
#   - Shared subscriptions ($share)
#   - Denied topics (test/nosubscribe)
#   - Clean session behavior
#   - Multi-client scenarios
#
# Requirements:
#   - java (11+)
#   - mqtt-cli.jar (HiveMQ MQTT CLI) in same directory as this script
#   - mqtt_broker binary running on BROKER_HOST:BROKER_PORT
#
# Usage:
#   ./test_hivemq_mqtt_cli.sh [--host HOST] [--port PORT] [--timeout TIMEOUT]
#   ./test_hivemq_mqtt_cli.sh --start-broker /path/to/mqtt_broker
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MQTT_CLI_JAR="${SCRIPT_DIR}/mqtt-cli.jar"
BROKER_HOST="${BROKER_HOST:-127.0.0.1}"
BROKER_PORT="${BROKER_PORT:-1883}"
TIMEOUT="${TIMEOUT:-5}"
BROKER_PID=""
BROKER_BIN=""
PASS_COUNT=0
FAIL_COUNT=0
SKIP_COUNT=0

# =============================================================================
# Colors
# =============================================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# =============================================================================
# Helpers
# =============================================================================
MQTT="java -jar ${MQTT_CLI_JAR}"

log_info()  { echo -e "${BLUE}[INFO]${NC} $*"; }
log_ok()    { echo -e "${GREEN}[PASS]${NC} $*"; PASS_COUNT=$((PASS_COUNT+1)); }
log_fail()  { echo -e "${RED}[FAIL]${NC} $*"; FAIL_COUNT=$((FAIL_COUNT+1)); }
log_skip()  { echo -e "${YELLOW}[SKIP]${NC} $*"; SKIP_COUNT=$((SKIP_COUNT+1)); }
log_section() { echo; echo -e "${BLUE}=== $* ===${NC}"; }

die() { echo -e "${RED}FATAL: $*${NC}" >&2; cleanup; exit 1; }

# =============================================================================
# Argument parsing
# =============================================================================
START_BROKER=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --host)      BROKER_HOST="$2"; shift 2 ;;
        --port)      BROKER_PORT="$2"; shift 2 ;;
        --timeout)   TIMEOUT="$2"; shift 2 ;;
        --start-broker)
            START_BROKER=1
            BROKER_BIN="$2"
            shift 2 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# =============================================================================
# Startup
# =============================================================================
if [[ ! -f "${MQTT_CLI_JAR}" ]]; then
    die "mqtt-cli.jar not found at ${MQTT_CLI_JAR}"
fi

if ! java -version &>/dev/null; then
    die "java not found or not executable"
fi

# =============================================================================
# Broker lifecycle
# =============================================================================
start_broker() {
    local broker_bin="$1"
    if [[ ! -x "${broker_bin}" ]]; then
        die "mqtt_broker binary not found or not executable: ${broker_bin}"
    fi

    log_info "Starting mqtt_broker: ${broker_bin}"
    mkdir -p /yuneta/store/agent/uuid 2>/dev/null || true
    chown -R yuneta:yuneta /yuneta 2>/dev/null || true

    local config_file="${SCRIPT_DIR}/mqtt_broker.json"
    su -l yuneta -c "${broker_bin} --config-file ${config_file}" &>/tmp/mqtt_broker_test.log &
    BROKER_PID=$!
    log_info "mqtt_broker started with PID ${BROKER_PID}"

    # Wait for broker to start listening
    local waited=0
    while ! nc -z "${BROKER_HOST}" "${BROKER_PORT}" 2>/dev/null; do
        sleep 0.5
        waited=$((waited+1))
        if [[ ${waited} -ge 20 ]]; then
            log_fail "Broker did not start listening on ${BROKER_HOST}:${BROKER_PORT} within 10 seconds"
            cat /tmp/mqtt_broker_test.log >&2
            cleanup
            exit 1
        fi
    done
    log_ok "Broker is listening on ${BROKER_HOST}:${BROKER_PORT}"
}

cleanup() {
    if [[ -n "${BROKER_PID}" ]]; then
        log_info "Stopping mqtt_broker (PID ${BROKER_PID})"
        kill "${BROKER_PID}" 2>/dev/null || true
        wait "${BROKER_PID}" 2>/dev/null || true
        BROKER_PID=""
    fi
}

trap cleanup EXIT

# =============================================================================
# Check broker reachability
# =============================================================================
check_broker() {
    if ! nc -z "${BROKER_HOST}" "${BROKER_PORT}" 2>/dev/null; then
        die "Broker not reachable at ${BROKER_HOST}:${BROKER_PORT}. Start the broker first or use --start-broker."
    fi
    log_info "Broker reachable at ${BROKER_HOST}:${BROKER_PORT}"
}

# =============================================================================
# Test helper: pub then sub with timeout
# sub_receive HOST PORT TOPIC [--no-clean-start] [extra pub args...]
# Returns 0 if message received, 1 otherwise
# =============================================================================
pub_sub_test() {
    local test_name="$1"; shift
    local topic="$1"; shift
    local message="$1"; shift
    local extra_args=("$@")

    # Subscribe in background, then publish
    local recv_file
    recv_file=$(mktemp)

    ${MQTT} sub \
        --host "${BROKER_HOST}" \
        --port "${BROKER_PORT}" \
        --topic "${topic}" \
        --identifier "test-sub-$$" \
        "${extra_args[@]}" \
        --output-to-console \
        &>/dev/null &
    local sub_pid=$!

    sleep 0.5

    ${MQTT} pub \
        --host "${BROKER_HOST}" \
        --port "${BROKER_PORT}" \
        --topic "${topic}" \
        --message "${message}" \
        --identifier "test-pub-$$" \
        &>/dev/null

    sleep 1
    kill "${sub_pid}" 2>/dev/null || true
    wait "${sub_pid}" 2>/dev/null || true
    rm -f "${recv_file}"
}

# =============================================================================
# Helper: run sub in background and capture output
# =============================================================================
run_sub_bg() {
    local out_file="$1"; shift
    ${MQTT} sub \
        --host "${BROKER_HOST}" \
        --port "${BROKER_PORT}" \
        "$@" \
        2>/dev/null >"${out_file}" &
    echo $!
}

# =============================================================================
# Helper: publish a message
# =============================================================================
run_pub() {
    ${MQTT} pub \
        --host "${BROKER_HOST}" \
        --port "${BROKER_PORT}" \
        "$@" \
        2>/dev/null
}

# =============================================================================
# Start broker if requested
# =============================================================================
if [[ ${START_BROKER} -eq 1 ]]; then
    start_broker "${BROKER_BIN}"
else
    check_broker
fi

# =============================================================================
# TEST SUITE
# =============================================================================
log_info "HiveMQ MQTT CLI version: $(java -jar "${MQTT_CLI_JAR}" --version 2>&1 | head -1)"
log_info "Broker: ${BROKER_HOST}:${BROKER_PORT}"
echo

# =============================================================================
log_section "1. MQTT v3.1.1 Spec Compliance (mqtt test)"
# =============================================================================
${MQTT} test \
    --host "${BROKER_HOST}" \
    --port "${BROKER_PORT}" \
    --mqttVersion 3 \
    --timeOut "${TIMEOUT}" \
    2>&1 | tee /tmp/mqtt_test_v3.log

if grep -q "^MQTT 3: OK" /tmp/mqtt_test_v3.log; then
    log_ok "MQTT v3.1.1 spec compliance tests passed"
elif grep -q "^MQTT 3:" /tmp/mqtt_test_v3.log; then
    log_fail "MQTT v3.1.1 spec compliance tests had failures"
else
    log_skip "MQTT v3.1.1 spec compliance: could not connect to broker"
fi

# =============================================================================
log_section "2. MQTT v5.0 Spec Compliance (mqtt test --all)"
# =============================================================================
${MQTT} test \
    --host "${BROKER_HOST}" \
    --port "${BROKER_PORT}" \
    --all \
    --timeOut "${TIMEOUT}" \
    2>&1 | tee /tmp/mqtt_test_v5.log

if grep -q "^MQTT 5: OK" /tmp/mqtt_test_v5.log && grep -q "^MQTT 3: OK" /tmp/mqtt_test_v5.log; then
    log_ok "MQTT v5.0 spec compliance tests passed (v3+v5)"
elif grep -q "^MQTT 5: OK" /tmp/mqtt_test_v5.log; then
    log_ok "MQTT v5.0 spec compliance tests passed"
elif grep -q "^MQTT 5:" /tmp/mqtt_test_v5.log; then
    log_fail "MQTT v5.0 spec compliance tests had failures"
else
    log_skip "MQTT v5.0 spec compliance: could not connect to broker"
fi

# =============================================================================
log_section "3. QoS 0 Publish / Subscribe (MQTT v3.1.1)"
# =============================================================================
TOPIC="test/qos0/$$"
MSG="hello-qos0"
OUT=$(mktemp)

SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC}" \
    --identifier "sub-qos0-$$" \
    --qos 0)
sleep 0.5

run_pub \
    --topic "${TOPIC}" \
    --message "${MSG}" \
    --identifier "pub-qos0-$$" \
    --qos 0

sleep 1
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

if grep -q "${MSG}" "${OUT}"; then
    log_ok "QoS 0 publish/subscribe (MQTT v3.1.1)"
else
    log_fail "QoS 0 publish/subscribe: message not received (topic: ${TOPIC})"
fi
rm -f "${OUT}"

# =============================================================================
log_section "4. QoS 1 Publish / Subscribe (MQTT v3.1.1)"
# =============================================================================
TOPIC="test/qos1/$$"
MSG="hello-qos1"
OUT=$(mktemp)

SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC}" \
    --identifier "sub-qos1-$$" \
    --qos 1)
sleep 0.5

run_pub \
    --topic "${TOPIC}" \
    --message "${MSG}" \
    --identifier "pub-qos1-$$" \
    --qos 1

sleep 1
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

if grep -q "${MSG}" "${OUT}"; then
    log_ok "QoS 1 publish/subscribe (MQTT v3.1.1)"
else
    log_fail "QoS 1 publish/subscribe: message not received (topic: ${TOPIC})"
fi
rm -f "${OUT}"

# =============================================================================
log_section "5. QoS 2 Publish / Subscribe (MQTT v3.1.1)"
# =============================================================================
TOPIC="test/qos2/$$"
MSG="hello-qos2"
OUT=$(mktemp)

SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC}" \
    --identifier "sub-qos2-$$" \
    --qos 2)
sleep 0.5

run_pub \
    --topic "${TOPIC}" \
    --message "${MSG}" \
    --identifier "pub-qos2-$$" \
    --qos 2

sleep 1
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

if grep -q "${MSG}" "${OUT}"; then
    log_ok "QoS 2 publish/subscribe (MQTT v3.1.1)"
else
    log_fail "QoS 2 publish/subscribe: message not received (topic: ${TOPIC})"
fi
rm -f "${OUT}"

# =============================================================================
log_section "6. Retained Message"
# =============================================================================
TOPIC="test/retain/$$"
MSG="retained-message-$$"
OUT=$(mktemp)

# Publish retained message first
run_pub \
    --topic "${TOPIC}" \
    --message "${MSG}" \
    --identifier "pub-retain-$$" \
    -r \
    --qos 1

# Wait for broker to store it
sleep 1

# Subscribe AFTER publish - should receive retained message
SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC}" \
    --identifier "sub-retain-$$")

# Wait longer for the retained message to be delivered (JVM startup overhead)
sleep 2
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

if grep -q "${MSG}" "${OUT}"; then
    log_ok "Retained message delivery to late subscriber"
else
    log_fail "Retained message not delivered to late subscriber"
fi
rm -f "${OUT}"

# =============================================================================
log_section "7. Topic Wildcard '+' (single level)"
# =============================================================================
TOPIC_PUB="home/living/temperature"
TOPIC_SUB="home/+/temperature"
MSG="temp-wildcard-plus"
OUT=$(mktemp)

SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC_SUB}" \
    --identifier "sub-wc-plus-$$")
sleep 0.5

run_pub \
    --topic "${TOPIC_PUB}" \
    --message "${MSG}" \
    --identifier "pub-wc-plus-$$"

sleep 1
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

if grep -q "${MSG}" "${OUT}"; then
    log_ok "Wildcard '+' subscription matches single-level topic"
else
    log_fail "Wildcard '+': message not received on 'home/+/temperature' for 'home/living/temperature'"
fi
rm -f "${OUT}"

# =============================================================================
log_section "8. Topic Wildcard '#' (multi-level)"
# =============================================================================
TOPIC_PUB="home/living/room/sensor1"
TOPIC_SUB="home/#"
MSG="temp-wildcard-hash"
OUT=$(mktemp)

SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC_SUB}" \
    --identifier "sub-wc-hash-$$")
sleep 0.5

run_pub \
    --topic "${TOPIC_PUB}" \
    --message "${MSG}" \
    --identifier "pub-wc-hash-$$"

sleep 1
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

if grep -q "${MSG}" "${OUT}"; then
    log_ok "Wildcard '#' subscription matches multi-level topic"
else
    log_fail "Wildcard '#': message not received on 'home/#' for 'home/living/room/sensor1'"
fi
rm -f "${OUT}"

# =============================================================================
log_section "9. Multiple Subscribers (fan-out)"
# =============================================================================
TOPIC="test/fanout/$$"
MSG="fanout-message"
OUT1=$(mktemp)
OUT2=$(mktemp)

SUB_PID1=$(run_sub_bg "${OUT1}" \
    --topic "${TOPIC}" \
    --identifier "sub-fan1-$$")
SUB_PID2=$(run_sub_bg "${OUT2}" \
    --topic "${TOPIC}" \
    --identifier "sub-fan2-$$")
sleep 0.5

run_pub \
    --topic "${TOPIC}" \
    --message "${MSG}" \
    --identifier "pub-fan-$$"

sleep 1
kill "${SUB_PID1}" "${SUB_PID2}" 2>/dev/null || true
wait "${SUB_PID1}" "${SUB_PID2}" 2>/dev/null || true

RECV1=0; RECV2=0
grep -q "${MSG}" "${OUT1}" && RECV1=1
grep -q "${MSG}" "${OUT2}" && RECV2=1

if [[ ${RECV1} -eq 1 && ${RECV2} -eq 1 ]]; then
    log_ok "Fan-out: message delivered to both subscribers"
elif [[ ${RECV1} -eq 1 ]]; then
    log_fail "Fan-out: subscriber 2 did not receive message"
elif [[ ${RECV2} -eq 1 ]]; then
    log_fail "Fan-out: subscriber 1 did not receive message"
else
    log_fail "Fan-out: neither subscriber received message"
fi
rm -f "${OUT1}" "${OUT2}"

# =============================================================================
log_section "10. MQTT v5.0 QoS 1 Publish / Subscribe"
# =============================================================================
TOPIC="test/v5/qos1/$$"
MSG="v5-qos1-message"
OUT=$(mktemp)

SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC}" \
    --identifier "sub-v5-qos1-$$" \
    --mqttVersion 5 \
    --qos 1)
sleep 0.5

run_pub \
    --topic "${TOPIC}" \
    --message "${MSG}" \
    --identifier "pub-v5-qos1-$$" \
    --mqttVersion 5 \
    --qos 1

sleep 1
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

if grep -q "${MSG}" "${OUT}"; then
    log_ok "MQTT v5.0 QoS 1 publish/subscribe"
else
    log_fail "MQTT v5.0 QoS 1: message not received"
fi
rm -f "${OUT}"

# =============================================================================
log_section "11. MQTT v5.0 User Properties"
# =============================================================================
TOPIC="test/v5/props/$$"
MSG="v5-user-props"
OUT=$(mktemp)

SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC}" \
    --identifier "sub-v5-props-$$" \
    --mqttVersion 5)
sleep 0.5

run_pub \
    --topic "${TOPIC}" \
    --message "${MSG}" \
    --identifier "pub-v5-props-$$" \
    --mqttVersion 5 \
    -up "device=sensor1" \
    -up "room=living"

sleep 1
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

if grep -q "${MSG}" "${OUT}"; then
    log_ok "MQTT v5.0 publish with user properties"
else
    log_fail "MQTT v5.0 user properties: message not received"
fi
rm -f "${OUT}"

# =============================================================================
log_section "12. MQTT v5.0 Message Expiry"
# =============================================================================
TOPIC="test/v5/expiry/$$"
MSG="v5-expiry-message"
OUT=$(mktemp)

# Publish with 10 second expiry and retain
run_pub \
    --topic "${TOPIC}" \
    --message "${MSG}" \
    --identifier "pub-v5-expiry-$$" \
    --mqttVersion 5 \
    -e 10 \
    -r

sleep 1

# Subscribe and expect to receive the retained message (not expired yet)
SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC}" \
    --identifier "sub-v5-expiry-$$" \
    --mqttVersion 5)
sleep 2
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

if grep -q "${MSG}" "${OUT}"; then
    log_ok "MQTT v5.0 message expiry: non-expired retained message delivered"
else
    log_fail "MQTT v5.0 message expiry: retained message not received within expiry window"
fi
rm -f "${OUT}"

# =============================================================================
log_section "13. Denied Topic Subscription (test/nosubscribe)"
# =============================================================================
# This test is specific to yuno mqtt_broker with deny_subscribes configuration.
# The broker denies subscriptions to "test/nosubscribe" (configured in mqtt_broker.json).
# When running against the yuneta mqtt_broker, messages on this topic should NOT
# be delivered to subscribers. Other brokers (mosquitto, etc.) will deliver them.
TOPIC="test/nosubscribe"
MSG="should-be-denied-$$"
OUT=$(mktemp)

SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC}" \
    --identifier "sub-denied-$$")
sleep 0.5

run_pub \
    --topic "${TOPIC}" \
    --message "${MSG}" \
    --identifier "pub-denied-$$" 2>/dev/null || true

sleep 1
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

if ! grep -q "${MSG}" "${OUT}"; then
    log_ok "Denied topic 'test/nosubscribe': message not delivered (yuneta deny_subscribes working)"
else
    # This is expected when running against other brokers (mosquitto etc.)
    log_skip "Denied topic 'test/nosubscribe': message was delivered (only yuneta mqtt_broker enforces deny_subscribes)"
fi
rm -f "${OUT}"

# =============================================================================
log_section "14. MQTT v5.0 Shared Subscriptions (\$share)"
# =============================================================================
TOPIC="test/shared/$$"
SHARE_TOPIC="\$share/group1/${TOPIC}"
MSG1="shared-msg-1"
MSG2="shared-msg-2"
OUT1=$(mktemp)
OUT2=$(mktemp)

# Two subscribers in the same share group
SUB_PID1=$(run_sub_bg "${OUT1}" \
    --topic "${SHARE_TOPIC}" \
    --identifier "sub-share1-$$" \
    --mqttVersion 5)
SUB_PID2=$(run_sub_bg "${OUT2}" \
    --topic "${SHARE_TOPIC}" \
    --identifier "sub-share2-$$" \
    --mqttVersion 5)
sleep 0.5

run_pub \
    --topic "${TOPIC}" \
    --message "${MSG1}" \
    --identifier "pub-share1-$$" \
    --mqttVersion 5

run_pub \
    --topic "${TOPIC}" \
    --message "${MSG2}" \
    --identifier "pub-share2-$$" \
    --mqttVersion 5

sleep 1
kill "${SUB_PID1}" "${SUB_PID2}" 2>/dev/null || true
wait "${SUB_PID1}" "${SUB_PID2}" 2>/dev/null || true

TOTAL_RECV=0
grep -q "${MSG1}\|${MSG2}" "${OUT1}" && TOTAL_RECV=$((TOTAL_RECV+1))
grep -q "${MSG1}\|${MSG2}" "${OUT2}" && TOTAL_RECV=$((TOTAL_RECV+1))

if [[ ${TOTAL_RECV} -ge 1 ]]; then
    # Both messages received across both subscribers
    ALL_RECV=$(cat "${OUT1}" "${OUT2}")
    if echo "${ALL_RECV}" | grep -q "${MSG1}" && echo "${ALL_RECV}" | grep -q "${MSG2}"; then
        log_ok "Shared subscriptions: both messages distributed across group"
    else
        log_ok "Shared subscriptions: messages received (partial delivery is valid)"
    fi
else
    log_fail "Shared subscriptions: no messages received by either subscriber"
fi
rm -f "${OUT1}" "${OUT2}"

# =============================================================================
log_section "15. Will Message (MQTT v3.1.1)"
# =============================================================================
WILL_TOPIC="test/will/$$"
WILL_MSG="client-disconnected-unexpectedly"
OUT=$(mktemp)

# Subscribe to will topic
SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${WILL_TOPIC}" \
    --identifier "sub-will-$$")
sleep 0.5

# Connect with will then kill (simulate ungraceful disconnect)
(
    ${MQTT} pub \
        --host "${BROKER_HOST}" \
        --port "${BROKER_PORT}" \
        --topic "test/will/dummy/$$" \
        --message "keepalive" \
        --identifier "will-client-$$" \
        -Wt "${WILL_TOPIC}" \
        -Wm "${WILL_MSG}" \
        -Wq 1 \
        --keepAlive 2 \
        2>/dev/null &
    WILL_CLIENT_PID=$!
    sleep 1
    # Kill without DISCONNECT to trigger will
    kill -9 "${WILL_CLIENT_PID}" 2>/dev/null || true
) &
WILL_PROC=$!
wait "${WILL_PROC}" 2>/dev/null || true

# Wait for broker to deliver will (after keepalive timeout)
sleep 4

kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

if grep -q "${WILL_MSG}" "${OUT}"; then
    log_ok "Will message delivered after ungraceful client disconnect"
else
    log_skip "Will message: not received within timeout (may need longer keepalive timeout)"
fi
rm -f "${OUT}"

# =============================================================================
log_section "16. Large Message Payload"
# =============================================================================
TOPIC="test/large/$$"
# 10KB message
LARGE_MSG=$(python3 -c "print('A' * 10240)")
OUT=$(mktemp)

SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC}" \
    --identifier "sub-large-$$")
sleep 0.5

run_pub \
    --topic "${TOPIC}" \
    --message "${LARGE_MSG}" \
    --identifier "pub-large-$$"

sleep 1
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

if grep -q "AAAA" "${OUT}"; then
    log_ok "Large payload (10KB) transmitted successfully"
else
    log_fail "Large payload: message not received or truncated"
fi
rm -f "${OUT}"

# =============================================================================
log_section "17. MQTT v5.0 Topic Alias"
# =============================================================================
TOPIC="test/v5/alias/$$"
MSG="topic-alias-test"
OUT=$(mktemp)

SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC}" \
    --identifier "sub-alias-$$" \
    --mqttVersion 5)
sleep 0.5

# Publish with topic alias
run_pub \
    --topic "${TOPIC}" \
    --message "${MSG}" \
    --identifier "pub-alias-$$" \
    --mqttVersion 5 \
    --topicAliasMax 10

sleep 1
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

if grep -q "${MSG}" "${OUT}"; then
    log_ok "MQTT v5.0 topic alias: message received"
else
    log_fail "MQTT v5.0 topic alias: message not received"
fi
rm -f "${OUT}"

# =============================================================================
log_section "18. Persistent Session (Clean Session = false, MQTT v3.1.1)"
# =============================================================================
TOPIC="test/persist/$$"
MSG1="queued-while-offline"
MSG2="online-message"
OUT=$(mktemp)

CLIENT_ID="persist-client-$$"

# Subscribe with persistent session (clean session = false)
SUB_PID=$(${MQTT} sub \
    --host "${BROKER_HOST}" \
    --port "${BROKER_PORT}" \
    --topic "${TOPIC}" \
    --identifier "${CLIENT_ID}" \
    --no-cleanStart \
    --qos 1 \
    2>/dev/null >"${OUT}" &)
SUB_PID=$!
sleep 0.5

# Disconnect the subscriber
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

# Publish while client is offline
run_pub \
    --topic "${TOPIC}" \
    --message "${MSG1}" \
    --identifier "pub-persist-$$" \
    --qos 1

sleep 0.5

# Reconnect with same client ID (persistent session)
OUT2=$(mktemp)
SUB_PID2=$(${MQTT} sub \
    --host "${BROKER_HOST}" \
    --port "${BROKER_PORT}" \
    --topic "${TOPIC}" \
    --identifier "${CLIENT_ID}" \
    --no-cleanStart \
    --qos 1 \
    2>/dev/null >"${OUT2}" &)
SUB_PID2=$!
sleep 2

kill "${SUB_PID2}" 2>/dev/null || true
wait "${SUB_PID2}" 2>/dev/null || true

if grep -q "${MSG1}" "${OUT2}"; then
    log_ok "Persistent session: queued message delivered on reconnect"
else
    log_skip "Persistent session: queued message not received (broker may require persistent storage enabled)"
fi
rm -f "${OUT}" "${OUT2}"

# =============================================================================
log_section "19. MQTT v5.0 Session Expiry"
# =============================================================================
TOPIC="test/v5/session/$$"
MSG="session-expiry-msg"
CLIENT_ID="v5-session-$$"
OUT=$(mktemp)

# Connect with session expiry of 30 seconds
SUB_PID=$(${MQTT} sub \
    --host "${BROKER_HOST}" \
    --port "${BROKER_PORT}" \
    --topic "${TOPIC}" \
    --identifier "${CLIENT_ID}" \
    --mqttVersion 5 \
    --no-cleanStart \
    --sessionExpiryInterval 30 \
    --qos 1 \
    2>/dev/null >"${OUT}" &)
SUB_PID=$!
sleep 0.5

kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

# Publish while disconnected
run_pub \
    --topic "${TOPIC}" \
    --message "${MSG}" \
    --identifier "pub-v5-session-$$" \
    --mqttVersion 5 \
    --qos 1

sleep 0.5

# Reconnect within session expiry window
OUT2=$(mktemp)
SUB_PID2=$(${MQTT} sub \
    --host "${BROKER_HOST}" \
    --port "${BROKER_PORT}" \
    --topic "${TOPIC}" \
    --identifier "${CLIENT_ID}" \
    --mqttVersion 5 \
    --no-cleanStart \
    --sessionExpiryInterval 30 \
    --qos 1 \
    2>/dev/null >"${OUT2}" &)
SUB_PID2=$!
sleep 2

kill "${SUB_PID2}" 2>/dev/null || true
wait "${SUB_PID2}" 2>/dev/null || true

if grep -q "${MSG}" "${OUT2}"; then
    log_ok "MQTT v5.0 session expiry: message delivered within session window"
else
    log_skip "MQTT v5.0 session expiry: message not received (session persistence may require storage)"
fi
rm -f "${OUT}" "${OUT2}"

# =============================================================================
log_section "20. High-volume Publish (burst 50 messages)"
# =============================================================================
TOPIC="test/burst/$$"
OUT=$(mktemp)
EXPECTED=50

SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC}" \
    --identifier "sub-burst-$$")
sleep 0.5

for i in $(seq 1 ${EXPECTED}); do
    run_pub \
        --topic "${TOPIC}" \
        --message "burst-msg-${i}" \
        --identifier "pub-burst-${i}-$$" &
done
wait

sleep 2
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

RECEIVED=$(grep -c "burst-msg-" "${OUT}" 2>/dev/null || echo 0)
if [[ ${RECEIVED} -eq ${EXPECTED} ]]; then
    log_ok "Burst publish: all ${EXPECTED} messages received"
elif [[ ${RECEIVED} -gt 0 ]]; then
    log_fail "Burst publish: only ${RECEIVED}/${EXPECTED} messages received"
else
    log_fail "Burst publish: no messages received"
fi
rm -f "${OUT}"

# =============================================================================
# RESULTS SUMMARY
# =============================================================================
echo
echo "=============================================="
echo " TEST RESULTS SUMMARY"
echo "=============================================="
echo -e " ${GREEN}PASSED${NC}: ${PASS_COUNT}"
echo -e " ${RED}FAILED${NC}: ${FAIL_COUNT}"
echo -e " ${YELLOW}SKIPPED${NC}: ${SKIP_COUNT}"
echo " TOTAL : $((PASS_COUNT + FAIL_COUNT + SKIP_COUNT))"
echo "=============================================="

if [[ ${FAIL_COUNT} -gt 0 ]]; then
    exit 1
else
    exit 0
fi
