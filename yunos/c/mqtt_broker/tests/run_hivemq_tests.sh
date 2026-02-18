#!/bin/bash
# =============================================================================
# run_hivemq_tests.sh
#
# Wrapper script to run HiveMQ MQTT CLI tests against a running mqtt_broker.
#
# The broker must already be running before executing this script.
#
# Usage:
#   ./run_hivemq_tests.sh [--host HOST] [--port PORT]
#
# Prerequisites:
#   - mqtt_broker must be running on BROKER_HOST:BROKER_PORT
#   - Java 11+ must be installed
#   - mqtt-cli.jar must be in the same directory
#
# Environment:
#   BROKER_HOST  - Broker hostname (default: 127.0.0.1)
#   BROKER_PORT  - Broker port (default: 1810)
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BROKER_HOST="${BROKER_HOST:-127.0.0.1}"
BROKER_PORT="${BROKER_PORT:-1810}"

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
        --help)
            echo "Usage: $0 [--host HOST] [--port PORT]"
            exit 0 ;;
        *) log_err "Unknown option: $1"; exit 1 ;;
    esac
done

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

    [[ ${ok} -eq 1 ]] || exit 1
}

# =============================================================================
# Check if broker is running
# =============================================================================
check_broker() {
    if ! nc -z "${BROKER_HOST}" "${BROKER_PORT}" 2>/dev/null; then
        log_err "Broker not reachable at ${BROKER_HOST}:${BROKER_PORT}"
        log_err "Start the mqtt_broker before running tests."
        exit 1
    fi
    log_ok "Broker is running at ${BROKER_HOST}:${BROKER_PORT}"
}

# =============================================================================
# Main
# =============================================================================
check_prerequisites
check_broker

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
