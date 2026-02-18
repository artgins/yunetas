#!/bin/bash
# =============================================================================
# run_hivemq_tests.sh
#
# Wrapper script to start yuno mqtt_broker and run HiveMQ MQTT CLI tests.
#
# Usage:
#   ./run_hivemq_tests.sh [--host HOST] [--port PORT] [--broker BINARY]
#
# Prerequisites:
#   - yuneta user must exist (useradd -m yuneta)
#   - /yuneta directory must be writable by yuneta user
#   - Java 11+ must be installed
#   - mqtt-cli.jar must be in the same directory
#   - The mqtt_broker binary must be compiled
#
# Environment:
#   BROKER_HOST  - Broker hostname (default: 127.0.0.1)
#   BROKER_PORT  - Broker port (default: 1810)
#   MQTT_BROKER  - Path to mqtt_broker binary
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BROKER_HOST="${BROKER_HOST:-127.0.0.1}"
BROKER_PORT="${BROKER_PORT:-1810}"
# Default binary location (relative to repo root)
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
DEFAULT_BROKER="${REPO_ROOT}/build/yunos/c/mqtt_broker/mqtt_broker"
MQTT_BROKER="${MQTT_BROKER:-${DEFAULT_BROKER}}"
BROKER_PID=""

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $*"; }
log_ok()   { echo -e "${GREEN}[OK]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_err()  { echo -e "${RED}[ERROR]${NC} $*" >&2; }

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --host)   BROKER_HOST="$2"; shift 2 ;;
        --port)   BROKER_PORT="$2"; shift 2 ;;
        --broker) MQTT_BROKER="$2"; shift 2 ;;
        --help)
            echo "Usage: $0 [--host HOST] [--port PORT] [--broker BINARY]"
            exit 0 ;;
        *) log_err "Unknown option: $1"; exit 1 ;;
    esac
done

cleanup() {
    if [[ -n "${BROKER_PID}" ]]; then
        log_info "Stopping mqtt_broker (PID ${BROKER_PID})"
        kill "${BROKER_PID}" 2>/dev/null || true
        wait "${BROKER_PID}" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# =============================================================================
# Check prerequisites
# =============================================================================
check_prerequisites() {
    local ok=1

    if ! java -version &>/dev/null; then
        log_err "java not found. Install Java 11+ to run HiveMQ MQTT CLI."
        ok=0
    fi

    if [[ ! -f "${SCRIPT_DIR}/mqtt-cli.jar" ]]; then
        log_err "mqtt-cli.jar not found in ${SCRIPT_DIR}/"
        log_err "Download from: https://github.com/hivemq/mqtt-cli/releases"
        ok=0
    fi

    if ! id yuneta &>/dev/null; then
        log_warn "yuneta user does not exist. Creating..."
        useradd -m yuneta 2>/dev/null || {
            log_err "Failed to create yuneta user."
            ok=0
        }
    fi

    [[ ${ok} -eq 1 ]] || exit 1
}

# =============================================================================
# Start broker
# =============================================================================
start_broker() {
    if [[ ! -x "${MQTT_BROKER}" ]]; then
        log_err "mqtt_broker binary not found or not executable: ${MQTT_BROKER}"
        log_err "Build it first: cd /path/to/yunetas && mkdir -p build && cd build && cmake .. && make mqtt_broker"
        exit 1
    fi

    # Create required /yuneta directory
    mkdir -p /yuneta/store/agent/uuid 2>/dev/null || true
    chown -R yuneta:yuneta /yuneta 2>/dev/null || true

    # Copy binary to yuneta's home for execution
    cp "${MQTT_BROKER}" /home/yuneta/mqtt_broker
    chown yuneta:yuneta /home/yuneta/mqtt_broker
    chmod +x /home/yuneta/mqtt_broker

    local config="${SCRIPT_DIR}/mqtt_broker.json"
    log_info "Starting mqtt_broker with config: ${config}"

    su -l yuneta -c "/home/yuneta/mqtt_broker --config-file ${config}" \
        >/tmp/mqtt_broker_run.log 2>&1 &
    BROKER_PID=$!

    log_info "mqtt_broker PID: ${BROKER_PID}"

    # Wait for broker to be ready
    local waited=0
    while ! nc -z "${BROKER_HOST}" "${BROKER_PORT}" 2>/dev/null; do
        sleep 0.5
        waited=$((waited+1))
        if [[ ${waited} -ge 30 ]]; then
            log_err "Broker failed to start within 15 seconds"
            log_err "Log output:"
            cat /tmp/mqtt_broker_run.log >&2
            exit 1
        fi
        # Check if process died
        if ! kill -0 "${BROKER_PID}" 2>/dev/null; then
            log_err "mqtt_broker process died unexpectedly"
            log_err "Log output:"
            cat /tmp/mqtt_broker_run.log >&2
            exit 1
        fi
    done
    log_ok "mqtt_broker listening on ${BROKER_HOST}:${BROKER_PORT}"
}

# =============================================================================
# Check if broker is already running
# =============================================================================
broker_running() {
    nc -z "${BROKER_HOST}" "${BROKER_PORT}" 2>/dev/null
}

# =============================================================================
# Main
# =============================================================================
check_prerequisites

if broker_running; then
    log_info "Broker already running at ${BROKER_HOST}:${BROKER_PORT} - skipping start"
else
    start_broker
fi

log_info "Running HiveMQ MQTT CLI tests..."
echo

BROKER_HOST="${BROKER_HOST}" BROKER_PORT="${BROKER_PORT}" \
    bash "${SCRIPT_DIR}/test_hivemq_mqtt_cli.sh" \
    --host "${BROKER_HOST}" \
    --port "${BROKER_PORT}"

EXIT_CODE=$?

echo
if [[ ${EXIT_CODE} -eq 0 ]]; then
    log_ok "All tests passed!"
else
    log_err "Some tests failed."
fi

exit ${EXIT_CODE}
