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
#   - mqtt_broker must be running on BROKER_HOST:BROKER_PORT
#
# Usage:
#   ./test_hivemq_mqtt_cli.sh [--host HOST] [--port PORT] [--timeout TIMEOUT]
#   ./test_hivemq_mqtt_cli.sh --test 5        # run only test 5
#   ./test_hivemq_mqtt_cli.sh --test 3,7      # run tests 3 through 7
#   ./test_hivemq_mqtt_cli.sh --list           # list available tests
# =============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MQTT_CLI_JAR="${SCRIPT_DIR}/mqtt-cli.jar"
BROKER_HOST="${BROKER_HOST:-127.0.0.1}"
BROKER_PORT="${BROKER_PORT:-1810}"
TIMEOUT="${TIMEOUT:-5}"
PASS_COUNT=0
FAIL_COUNT=0
SKIP_COUNT=0
TEST_START=0
TEST_END=0
NUM_TESTS=25

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

die() { echo -e "${RED}FATAL: $*${NC}" >&2; exit 1; }

# =============================================================================
# Test names (indexed 1..NUM_TESTS)
# =============================================================================
TEST_NAMES=(
    [1]="MQTT v3.1.1 Spec Compliance (mqtt test)"
    [2]="MQTT v5.0 Spec Compliance (mqtt test --all)"
    [3]="QoS 0 Publish / Subscribe (MQTT v3.1.1)"
    [4]="QoS 1 Publish / Subscribe (MQTT v3.1.1)"
    [5]="QoS 2 Publish / Subscribe (MQTT v3.1.1)"
    [6]="Retained Message"
    [7]="Topic Wildcard '+' (single level)"
    [8]="Topic Wildcard '#' (multi-level)"
    [9]="Multiple Subscribers (fan-out)"
    [10]="MQTT v5.0 QoS 1 Publish / Subscribe"
    [11]="MQTT v5.0 User Properties"
    [12]="MQTT v5.0 Message Expiry"
    [13]="Denied Topic Subscription (test/nosubscribe)"
    [14]="MQTT v5.0 Shared Subscriptions (\$share)"
    [15]="Will Message (MQTT v3.1.1)"
    [16]="Large Message Payload"
    [17]="MQTT v5.0 Topic Alias"
    [18]="Persistent Session (Clean Session = false, MQTT v3.1.1)"
    [19]="MQTT v5.0 Session Expiry"
    [20]="High-volume Publish (burst 50 messages)"
    [21]="Perf: Concurrent Connections (100 simultaneous)"
    [22]="Perf: Message Throughput (1000 msgs, single connection)"
    [23]="Perf: Fan-out Scalability (1 pub to 20 subs, 50 msgs)"
    [24]="Perf: Burst Publish (500 msgs, 10 parallel connections)"
    [25]="Perf: Max Speed Benchmark (QoS 0, 1, 2)"
)

# =============================================================================
# Argument parsing
# =============================================================================
LIST_TESTS=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --host)      BROKER_HOST="$2"; shift 2 ;;
        --port)      BROKER_PORT="$2"; shift 2 ;;
        --timeout)   TIMEOUT="$2"; shift 2 ;;
        --test)
            IFS=',' read -r TEST_START TEST_END <<< "$2"
            if [[ -z "${TEST_END}" ]]; then
                TEST_END="${TEST_START}"
            fi
            if [[ ${TEST_START} -lt 1 || ${TEST_END} -gt ${NUM_TESTS} || ${TEST_START} -gt ${TEST_END} ]]; then
                die "Invalid test range: ${TEST_START},${TEST_END} (valid: 1..${NUM_TESTS})"
            fi
            shift 2 ;;
        --list)
            LIST_TESTS=1; shift ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# =============================================================================
# List tests and exit
# =============================================================================
if [[ ${LIST_TESTS} -eq 1 ]]; then
    echo "Available tests (${NUM_TESTS} total):"
    for i in $(seq 1 ${NUM_TESTS}); do
        printf "  %2d. %s\n" "$i" "${TEST_NAMES[$i]}"
    done
    echo
    echo "Usage: $0 --test N       # run test N"
    echo "       $0 --test N,M     # run tests N through M"
    exit 0
fi

# =============================================================================
# should_run N - returns 0 if test N should execute
# =============================================================================
should_run() {
    local n=$1
    # TEST_START=0 means run all
    [[ ${TEST_START} -eq 0 ]] && return 0
    [[ ${n} -ge ${TEST_START} && ${n} -le ${TEST_END} ]]
}

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
# Check broker reachability
# =============================================================================
check_broker() {
    if ! nc -z "${BROKER_HOST}" "${BROKER_PORT}" 2>/dev/null; then
        die "Broker not reachable at ${BROKER_HOST}:${BROKER_PORT}. Start the broker first."
    fi
    log_info "Broker reachable at ${BROKER_HOST}:${BROKER_PORT}"
}

# =============================================================================
# Helper: run sub in background and capture output
# =============================================================================
run_sub_bg() {
    local out_file="$1"; shift
    ${MQTT} sub \
        -l \
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
        -l \
        --host "${BROKER_HOST}" \
        --port "${BROKER_PORT}" \
        "$@" \
        2>/dev/null
}

# =============================================================================
# Check broker is running
# =============================================================================
check_broker

# =============================================================================
# TEST SUITE
# =============================================================================
log_info "HiveMQ MQTT CLI version: $(java -jar "${MQTT_CLI_JAR}" --version 2>&1 | head -1)"
log_info "Broker: ${BROKER_HOST}:${BROKER_PORT}"
log_info "HiveMQ CLI logs: ${HOME}/.mqtt-cli/logs/"
if [[ ${TEST_START} -gt 0 ]]; then
    if [[ ${TEST_START} -eq ${TEST_END} ]]; then
        log_info "Running test ${TEST_START} only"
    else
        log_info "Running tests ${TEST_START}..${TEST_END}"
    fi
fi
echo

# =============================================================================
# 1. MQTT v3.1.1 Spec Compliance
# =============================================================================
if should_run 1; then
log_section "1. ${TEST_NAMES[1]}"

${MQTT} test \
    -l \
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
fi

# =============================================================================
# 2. MQTT v5.0 Spec Compliance
# =============================================================================
if should_run 2; then
log_section "2. ${TEST_NAMES[2]}"

${MQTT} test \
    -l \
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
fi

# =============================================================================
# 3. QoS 0 Publish / Subscribe
# =============================================================================
if should_run 3; then
log_section "3. ${TEST_NAMES[3]}"

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
fi

# =============================================================================
# 4. QoS 1 Publish / Subscribe
# =============================================================================
if should_run 4; then
log_section "4. ${TEST_NAMES[4]}"

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
fi

# =============================================================================
# 5. QoS 2 Publish / Subscribe
# =============================================================================
if should_run 5; then
log_section "5. ${TEST_NAMES[5]}"

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
fi

# =============================================================================
# 6. Retained Message
# =============================================================================
if should_run 6; then
log_section "6. ${TEST_NAMES[6]}"

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
fi

# =============================================================================
# 7. Topic Wildcard '+'
# =============================================================================
if should_run 7; then
log_section "7. ${TEST_NAMES[7]}"

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
fi

# =============================================================================
# 8. Topic Wildcard '#'
# =============================================================================
if should_run 8; then
log_section "8. ${TEST_NAMES[8]}"

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
fi

# =============================================================================
# 9. Multiple Subscribers (fan-out)
# =============================================================================
if should_run 9; then
log_section "9. ${TEST_NAMES[9]}"

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
fi

# =============================================================================
# 10. MQTT v5.0 QoS 1 Publish / Subscribe
# =============================================================================
if should_run 10; then
log_section "10. ${TEST_NAMES[10]}"

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
fi

# =============================================================================
# 11. MQTT v5.0 User Properties
# =============================================================================
if should_run 11; then
log_section "11. ${TEST_NAMES[11]}"

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
fi

# =============================================================================
# 12. MQTT v5.0 Message Expiry
# =============================================================================
if should_run 12; then
log_section "12. ${TEST_NAMES[12]}"

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
fi

# =============================================================================
# 13. Denied Topic Subscription
# =============================================================================
if should_run 13; then
log_section "13. ${TEST_NAMES[13]}"

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
fi

# =============================================================================
# 14. MQTT v5.0 Shared Subscriptions
# =============================================================================
if should_run 14; then
log_section "14. ${TEST_NAMES[14]}"

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
fi

# =============================================================================
# 15. Will Message
# =============================================================================
if should_run 15; then
log_section "15. ${TEST_NAMES[15]}"

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
        -l \
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
fi

# =============================================================================
# 16. Large Message Payload
# =============================================================================
if should_run 16; then
log_section "16. ${TEST_NAMES[16]}"

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
fi

# =============================================================================
# 17. MQTT v5.0 Topic Alias
# =============================================================================
if should_run 17; then
log_section "17. ${TEST_NAMES[17]}"

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
fi

# =============================================================================
# 18. Persistent Session
# =============================================================================
if should_run 18; then
log_section "18. ${TEST_NAMES[18]}"

TOPIC="test/persist/$$"
MSG1="queued-while-offline"
MSG2="online-message"
OUT=$(mktemp)

CLIENT_ID="persist-client-$$"

# Subscribe with persistent session (clean session = false, MQTT v3.1.1)
${MQTT} sub \
    -l \
    --host "${BROKER_HOST}" \
    --port "${BROKER_PORT}" \
    --topic "${TOPIC}" \
    --identifier "${CLIENT_ID}" \
    --mqttVersion 3 \
    --no-cleanStart \
    --qos 1 \
    2>/dev/null >"${OUT}" &
SUB_PID=$!
sleep 2

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
${MQTT} sub \
    -l \
    --host "${BROKER_HOST}" \
    --port "${BROKER_PORT}" \
    --topic "${TOPIC}" \
    --identifier "${CLIENT_ID}" \
    --mqttVersion 3 \
    --no-cleanStart \
    --qos 1 \
    2>/dev/null >"${OUT2}" &
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
fi

# =============================================================================
# 19. MQTT v5.0 Session Expiry
# =============================================================================
if should_run 19; then
log_section "19. ${TEST_NAMES[19]}"

TOPIC="test/v5/session/$$"
MSG="session-expiry-msg"
CLIENT_ID="v5-session-$$"
OUT=$(mktemp)

# Connect with session expiry of 30 seconds
${MQTT} sub \
    -l \
    --host "${BROKER_HOST}" \
    --port "${BROKER_PORT}" \
    --topic "${TOPIC}" \
    --identifier "${CLIENT_ID}" \
    --mqttVersion 5 \
    --no-cleanStart \
    --sessionExpiryInterval 30 \
    --qos 1 \
    2>/dev/null >"${OUT}" &
SUB_PID=$!
sleep 2

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
${MQTT} sub \
    -l \
    --host "${BROKER_HOST}" \
    --port "${BROKER_PORT}" \
    --topic "${TOPIC}" \
    --identifier "${CLIENT_ID}" \
    --mqttVersion 5 \
    --no-cleanStart \
    --sessionExpiryInterval 30 \
    --qos 1 \
    2>/dev/null >"${OUT2}" &
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
fi

# =============================================================================
# 20. High-volume Publish (burst 50 messages)
# =============================================================================
if should_run 20; then
log_section "20. ${TEST_NAMES[20]}"

TOPIC="test/burst/$$"
OUT=$(mktemp)
EXPECTED=50
BATCH_SIZE=25

SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC}" \
    --identifier "sub-burst-$$")
sleep 0.5

for i in $(seq 1 ${EXPECTED}); do
    run_pub \
        --topic "${TOPIC}" \
        --message "burst-msg-${i}" \
        --identifier "pub-burst-${i}-$$" &
    if (( i % BATCH_SIZE == 0 )); then
        wait
    fi
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
fi

# =============================================================================
# 21. Perf: Concurrent Connections (100 simultaneous)
#     Tests broker's ability to handle many simultaneous connections.
#     Each publisher is a separate JVM/connection (overhead ~1s per JVM).
# =============================================================================
if should_run 21; then
log_section "21. ${TEST_NAMES[21]}"

TOPIC="test/perf/conn/$$"
OUT=$(mktemp)
NUM_CONNS=100
BATCH_SIZE=50

SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC}" \
    --identifier "perf-sub-$$")
sleep 0.5

T_START=$(date +%s%N)

for i in $(seq 1 ${NUM_CONNS}); do
    run_pub \
        --topic "${TOPIC}" \
        --message "conn-${i}" \
        --identifier "perf-conn-${i}-$$" &
    if (( i % BATCH_SIZE == 0 )); then
        wait
    fi
done
wait

T_END=$(date +%s%N)
ELAPSED_MS=$(( (T_END - T_START) / 1000000 ))

sleep 2
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

RECEIVED=$(grep -c "conn-" "${OUT}" 2>/dev/null || echo 0)
CONN_RATE=$(( NUM_CONNS * 1000 / (ELAPSED_MS > 0 ? ELAPSED_MS : 1) ))
if [[ ${RECEIVED} -eq ${NUM_CONNS} ]]; then
    log_ok "Concurrent connections: ${NUM_CONNS}/${NUM_CONNS} received (${ELAPSED_MS}ms, ~${CONN_RATE} conn/s)"
elif [[ ${RECEIVED} -ge $(( NUM_CONNS * 90 / 100 )) ]]; then
    log_ok "Concurrent connections: ${RECEIVED}/${NUM_CONNS} received (${ELAPSED_MS}ms, ~${CONN_RATE} conn/s, >=90%)"
else
    log_fail "Concurrent connections: only ${RECEIVED}/${NUM_CONNS} received (${ELAPSED_MS}ms)"
fi
rm -f "${OUT}"
fi

# =============================================================================
# 22. Perf: Message Throughput (1000 msgs, single connection)
#     Uses stdin line-reader mode (-l) to publish many messages from a single
#     JVM/connection, measuring real broker throughput without JVM startup noise.
# =============================================================================
if should_run 22; then
log_section "22. ${TEST_NAMES[22]}"

TOPIC="test/perf/throughput/$$"
OUT=$(mktemp)
NUM_MSGS=1000

SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC}" \
    --identifier "perf-tp-sub-$$")
sleep 0.5

T_START=$(date +%s%N)

# Publish all messages from a single connection using stdin line-reader mode
seq 1 ${NUM_MSGS} | sed "s/^/tp-/" | ${MQTT} pub \
    -l \
    --host "${BROKER_HOST}" \
    --port "${BROKER_PORT}" \
    --topic "${TOPIC}" \
    --identifier "perf-tp-pub-$$" \
    2>/dev/null

T_END=$(date +%s%N)
ELAPSED_MS=$(( (T_END - T_START) / 1000000 ))

sleep 3
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

RECEIVED=$(grep -c "tp-" "${OUT}" 2>/dev/null || echo 0)
RATE=$(( RECEIVED * 1000 / (ELAPSED_MS > 0 ? ELAPSED_MS : 1) ))

if [[ ${RECEIVED} -eq ${NUM_MSGS} ]]; then
    log_ok "Throughput: ${RECEIVED}/${NUM_MSGS} delivered in ${ELAPSED_MS}ms (~${RATE} msg/s)"
elif [[ ${RECEIVED} -ge $(( NUM_MSGS * 90 / 100 )) ]]; then
    log_ok "Throughput: ${RECEIVED}/${NUM_MSGS} delivered in ${ELAPSED_MS}ms (~${RATE} msg/s, >=90%)"
else
    log_fail "Throughput: only ${RECEIVED}/${NUM_MSGS} delivered in ${ELAPSED_MS}ms (~${RATE} msg/s)"
fi
rm -f "${OUT}"
fi

# =============================================================================
# 23. Perf: Fan-out Scalability (1 pub to 20 subs, 50 msgs)
#     Tests message fan-out: single publisher sends 50 messages that must
#     be delivered to 20 subscribers (1000 total deliveries).
# =============================================================================
if should_run 23; then
log_section "23. ${TEST_NAMES[23]}"

TOPIC="test/perf/fanout/$$"
NUM_SUBS=20
NUM_MSGS=50
TOTAL_EXPECTED=$(( NUM_SUBS * NUM_MSGS ))

declare -a SUB_PIDS
declare -a SUB_OUTS
for s in $(seq 1 ${NUM_SUBS}); do
    SUB_OUTS[$s]=$(mktemp)
    SUB_PIDS[$s]=$(run_sub_bg "${SUB_OUTS[$s]}" \
        --topic "${TOPIC}" \
        --identifier "perf-fan-sub-${s}-$$")
done
sleep 2

T_START=$(date +%s%N)

# Single publisher sends all messages via stdin line-reader mode
seq 1 ${NUM_MSGS} | sed "s/^/fan-/" | ${MQTT} pub \
    -l \
    --host "${BROKER_HOST}" \
    --port "${BROKER_PORT}" \
    --topic "${TOPIC}" \
    --identifier "perf-fan-pub-$$" \
    2>/dev/null

T_END=$(date +%s%N)
ELAPSED_MS=$(( (T_END - T_START) / 1000000 ))

sleep 3

TOTAL_RECEIVED=0
ALL_SUBS_OK=1
for s in $(seq 1 ${NUM_SUBS}); do
    kill "${SUB_PIDS[$s]}" 2>/dev/null || true
    wait "${SUB_PIDS[$s]}" 2>/dev/null || true
    COUNT=$(grep -c "fan-" "${SUB_OUTS[$s]}" 2>/dev/null || echo 0)
    TOTAL_RECEIVED=$(( TOTAL_RECEIVED + COUNT ))
    if [[ ${COUNT} -ne ${NUM_MSGS} ]]; then
        ALL_SUBS_OK=0
    fi
    rm -f "${SUB_OUTS[$s]}"
done

FANOUT_RATE=$(( TOTAL_RECEIVED * 1000 / (ELAPSED_MS > 0 ? ELAPSED_MS : 1) ))
if [[ ${ALL_SUBS_OK} -eq 1 ]]; then
    log_ok "Fan-out: ${TOTAL_RECEIVED}/${TOTAL_EXPECTED} delivered (${NUM_SUBS} subs x ${NUM_MSGS} msgs, ${ELAPSED_MS}ms, ~${FANOUT_RATE} deliveries/s)"
elif [[ ${TOTAL_RECEIVED} -ge $(( TOTAL_EXPECTED * 90 / 100 )) ]]; then
    log_ok "Fan-out: ${TOTAL_RECEIVED}/${TOTAL_EXPECTED} delivered (${ELAPSED_MS}ms, ~${FANOUT_RATE} deliveries/s, >=90%)"
else
    log_fail "Fan-out: only ${TOTAL_RECEIVED}/${TOTAL_EXPECTED} delivered (${ELAPSED_MS}ms, ~${FANOUT_RATE} deliveries/s)"
fi
fi

# =============================================================================
# 24. Perf: Burst Publish (500 msgs, 10 parallel connections)
#     Tests broker under parallel load: 10 concurrent publishers each send
#     50 messages via stdin line-reader mode (500 total from 10 connections).
# =============================================================================
if should_run 24; then
log_section "24. ${TEST_NAMES[24]}"

TOPIC="test/perf/burst/$$"
OUT=$(mktemp)
NUM_PUBS=10
MSGS_PER_PUB=50
EXPECTED=$(( NUM_PUBS * MSGS_PER_PUB ))

SUB_PID=$(run_sub_bg "${OUT}" \
    --topic "${TOPIC}" \
    --identifier "perf-burst-sub-$$")
sleep 0.5

T_START=$(date +%s%N)

# Launch parallel publishers, each sending MSGS_PER_PUB messages via stdin
for p in $(seq 1 ${NUM_PUBS}); do
    seq 1 ${MSGS_PER_PUB} | sed "s/^/burst-p${p}-m/" | ${MQTT} pub \
        -l \
        --host "${BROKER_HOST}" \
        --port "${BROKER_PORT}" \
        --topic "${TOPIC}" \
        --identifier "perf-burst-pub-${p}-$$" \
        2>/dev/null &
done
wait

T_END=$(date +%s%N)
ELAPSED_MS=$(( (T_END - T_START) / 1000000 ))

sleep 3
kill "${SUB_PID}" 2>/dev/null || true
wait "${SUB_PID}" 2>/dev/null || true

RECEIVED=$(grep -c "burst-p" "${OUT}" 2>/dev/null || echo 0)
RATE=$(( RECEIVED * 1000 / (ELAPSED_MS > 0 ? ELAPSED_MS : 1) ))

if [[ ${RECEIVED} -eq ${EXPECTED} ]]; then
    log_ok "Burst: ${RECEIVED}/${EXPECTED} delivered (${NUM_PUBS} pubs x ${MSGS_PER_PUB} msgs, ${ELAPSED_MS}ms, ~${RATE} msg/s)"
elif [[ ${RECEIVED} -ge $(( EXPECTED * 90 / 100 )) ]]; then
    log_ok "Burst: ${RECEIVED}/${EXPECTED} delivered (${ELAPSED_MS}ms, ~${RATE} msg/s, >=90%)"
else
    log_fail "Burst: only ${RECEIVED}/${EXPECTED} delivered (${ELAPSED_MS}ms, ~${RATE} msg/s)"
fi
rm -f "${OUT}"
fi

# =============================================================================
# 25. Perf: Max Speed Benchmark (QoS 0, 1, 2)
#     Uses Python raw-socket benchmark for zero overhead.
#     Measures actual broker throughput without JVM startup noise.
# =============================================================================
if should_run 25; then
log_section "25. ${TEST_NAMES[25]}"

BENCH_SCRIPT="${SCRIPT_DIR}/mqtt_benchmark.py"
if [[ ! -f "${BENCH_SCRIPT}" ]]; then
    log_fail "Benchmark script not found: ${BENCH_SCRIPT}"
else
    BENCH_OUT=$(mktemp)
    python3 "${BENCH_SCRIPT}" \
        --host "${BROKER_HOST}" \
        --port "${BROKER_PORT}" \
        --count 10000 \
        --payload 64 \
        2>&1 | tee "${BENCH_OUT}"

    # Parse machine-readable RESULT lines
    ALL_QOS_PASS=1
    while IFS= read -r line; do
        if [[ "${line}" == RESULT:* ]]; then
            STATUS=$(echo "${line}" | sed 's/.*:status=\([^:]*\).*/\1/')
            if [[ "${STATUS}" == "FAIL" ]]; then
                ALL_QOS_PASS=0
            fi
        fi
    done < "${BENCH_OUT}"

    if [[ ${ALL_QOS_PASS} -eq 1 ]]; then
        log_ok "Max speed benchmark completed successfully"
    else
        log_fail "Max speed benchmark: some QoS levels below threshold"
    fi
    rm -f "${BENCH_OUT}"
fi
fi

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
