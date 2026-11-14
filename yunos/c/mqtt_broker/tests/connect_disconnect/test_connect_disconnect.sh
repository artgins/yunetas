#!/bin/bash
#
#   Run the CONNECT / DISCONNECT protocol tests against a broker on
#   127.0.0.1:1810 (see README.md). Extra arguments go to the script.
#
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "${SCRIPT_DIR}/mqtt_connect_disconnect_test.py" --host 127.0.0.1 --port 1810 --timeout 3 "$@"
