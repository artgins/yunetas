#!/bin/bash
#
#   Run the real agent in a sandbox and drive it as a peer of its 1991
#
#   The agent's 1991/1993 and its pid file are fixed in the binary, so it
#   runs in namespaces of its own (user, mount, network, pid): a tmpfs on
#   /yuneta, a passwd where uid 0 is `yuneta`, and a loopback nobody else
#   sees. client.py speaks to it; then the agent is stopped in order, and
#   its log must have no "BAD gbuf_decref()" and no "system memory not free"
#   (the gbmem audit of an orderly end, CONFIG_DEBUG_TRACK_MEMORY).
#
#   Usage: run_test.sh <yuneta_agent binary> <logcenter binary>
#   Exit 77 (skipped) when this machine cannot run it, saying why.
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#

#
#   What it needs
#
HERE=$(dirname "$(readlink -f "$0")")
AGENT="$1"
YUNO="$2"
skip() {
    echo "SKIPPED: $1"
    exit 77
}
[ -x "$AGENT" ] || skip "no agent binary at '$AGENT' (built by yunetas build)"
[ -x "$YUNO" ] || skip "no logcenter binary at '$YUNO' (built by yunetas build)"
python3 -c "import websockets" 2>/dev/null || skip "python3 without the websockets module"
command -v ip >/dev/null || skip "no 'ip' command to bring up the loopback of the sandbox"
unshare -rmn --pid --fork --mount-proc true 2>/dev/null || skip "unprivileged namespaces not allowed here"
[ -d /yuneta ] || skip "no /yuneta to mount the sandbox on"

#
#   Sandbox
#
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cp "$AGENT" "$WORK/yuneta_agent"
cp "$YUNO" "$WORK/logcenter"
cp "$HERE/client.py" "$HERE/agent.json" "$WORK/"
printf 'yuneta:x:0:0:yuneta:/yuneta:/bin/bash\n' > "$WORK/passwd"
printf 'yuneta:x:0:yuneta\n' > "$WORK/group"

unshare -rmn --pid --fork --mount-proc bash -c "
    ip link set lo up || exit 1
    mount -t tmpfs none /yuneta || exit 1
    mount --bind '$WORK/passwd' /etc/passwd || exit 1
    mount --bind '$WORK/group' /etc/group || exit 1
    mkdir -p /yuneta/agent/certs /yuneta/realms/agent /yuneta/store /yuneta/bin
    cp '$WORK/yuneta_agent' /yuneta/agent/yuneta_agent
    cp '$WORK/agent.json' /yuneta/agent/yuneta_agent.json
    cd /yuneta/agent
    ./yuneta_agent --config-file=/yuneta/agent/yuneta_agent.json > '$WORK/agent.log' 2>&1 &
    APID=\$!
    python3 -I '$WORK/client.py' '$WORK/logcenter'
    echo \"client exit code \$?\" > '$WORK/client.rc'
    kill -INT \$APID 2>/dev/null
    for i in \$(seq 1 150); do
        kill -0 \$APID 2>/dev/null || break
        sleep 0.1
    done
    if kill -0 \$APID 2>/dev/null; then
        echo 'the agent did not end in 15 s' >> '$WORK/client.rc'
        kill -9 \$APID
    fi
    wait \$APID
    echo \"agent exit code \$?\" >> '$WORK/client.rc'
"

#
#   Check
#
cat "$WORK/client.rc" 2>/dev/null
failed=0
if ! grep -q "^client exit code 0$" "$WORK/client.rc" 2>/dev/null; then
    echo "TEST FAILED: the peer did not end well (killed with every process of the user?)"
    failed=1
fi
if ! grep -q "^agent exit code 0$" "$WORK/client.rc" 2>/dev/null; then
    echo "TEST FAILED: the agent did not end in order"
    failed=1
fi
n=$(grep -ac "BAD gbuf_decref" "$WORK/agent.log")
if [ "$n" != "0" ]; then
    echo "TEST FAILED: $n x \"BAD gbuf_decref()\" in the agent: a gbuffer of a peer released twice"
    failed=1
fi
if grep -aq "system memory not free" "$WORK/agent.log"; then
    blocks=$(grep -ac '"msg": "mem-not-free"' "$WORK/agent.log")
    echo "TEST FAILED: the agent ended with memory not free ($blocks blocks)"
    failed=1
fi
n=$(grep -ac "identity card refused: its pid is not a process of that yuno, not killed" "$WORK/agent.log")
if [ "$n" != "3" ]; then
    echo "TEST FAILED: $n warnings of a card pid not killed, expected 3"
    failed=1
fi
if [ $failed -ne 0 ]; then
    echo "---- agent log (errors and warnings) ----"
    grep -aE "ERROR|WARNING" "$WORK/agent.log" | cut -c1-600 | tail -40
    exit 1
fi
echo "TEST PASSED: the agent killed only its own yuno's process, and released every gbuffer of the peer once"
exit 0
