#!/usr/bin/env bash
# run-tests.sh — run every lib-yui test (unit + e2e) in one shot.
#
#       cd kernel/js/lib-yui
#       ./run-tests.sh                 # full matrix
#       ./run-tests.sh --unit          # parser + focus-trap only
#       ./run-tests.sh --e2e           # Playwright only
#       ./run-tests.sh --browser=chromium
#
# Prerequisites:
#       npm install
#       ./install-e2e-deps.sh          # browsers + apt deps for webkit
#
# The script always runs from this directory, regardless of where it
# was invoked.  Exits non-zero on the first failure.

set -eu

SCRIPT_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" 2>/dev/null && pwd -P)
cd "$SCRIPT_DIR"

run_unit=1
run_e2e=1
e2e_args=""

for arg in "$@"; do
    case "$arg" in
        --unit)
            run_e2e=0
            ;;
        --e2e)
            run_unit=0
            ;;
        --browser=*)
            e2e_args="$e2e_args --project=${arg#--browser=}"
            ;;
        --help|-h)
            sed -n '2,16p' "$0"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg" >&2
            exit 2
            ;;
    esac
done

if [ ! -d node_modules ]; then
    echo "ERROR: node_modules missing.  Run 'npm install' first." >&2
    exit 1
fi

if [ "$run_unit" = "1" ]; then
    echo "============================================================"
    echo " Unit tests (node --test) — parser + focus-trap"
    echo "============================================================"
    npm test
fi

if [ "$run_e2e" = "1" ]; then
    if [ ! -d node_modules/@playwright ]; then
        echo "ERROR: @playwright/test not installed.  Run 'npm install'." >&2
        exit 1
    fi
    echo
    echo "============================================================"
    echo " End-to-end tests (Playwright) — chromium + firefox + webkit"
    echo "============================================================"
    # shellcheck disable=SC2086
    npx playwright test $e2e_args --reporter=list
fi

echo
echo "All requested tests passed."
