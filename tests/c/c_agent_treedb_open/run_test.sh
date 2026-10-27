#!/bin/bash
#
#   Run test_c_agent_treedb_open and check how it ends
#
#   The second open of the agent's treedb must end the process with exit 0
#   (ydaemon does not relaunch it) and "Cannot start agent treedb" on the
#   console. Any other exit code fails, and so does exit 0 without the
#   message: a run that never reached the second open ends with 0 too.
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#

#
#   Run
#
binary="$1"
if [ -z "$binary" ] || [ ! -x "$binary" ]; then
    echo "run_test.sh: no test binary given ($binary)"
    exit 1
fi

output=$("$binary" 2>&1)
rc=$?
printf '%s\n' "$output"

#
#   Check
#
if [ $rc -ne 0 ]; then
    echo "TEST FAILED: exit code $rc, expected 0 (the agent must end with exit 0 when its treedb does not open)"
    exit 1
fi
if ! printf '%s\n' "$output" | grep -q "Cannot start agent treedb: "; then
    echo "TEST FAILED: exit 0 without \"Cannot start agent treedb\": the second open was never refused"
    exit 1
fi
echo "TEST PASSED: the agent ended with exit 0 when its treedb did not open"
exit 0
