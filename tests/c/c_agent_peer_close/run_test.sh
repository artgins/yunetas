#!/bin/bash
#
#   Run the real agent in a sandbox, and close its peers while orders go to
#   them (client.py): a yuno that closes while the operator's stats-yuno
#   and command-yuno go to it, and an operator that closes while the answer
#   to its command comes back. Each order that reaches the yuno closing must
#   be answered (-1), the answer to an operator closing is dropped with a
#   WARNING, and nothing may be "Event NOT DEFINED in state".
#
#   The agent's 1991 and its pid file are fixed in the binary, so it runs in
#   namespaces of its own (user, mount, network, pid): a tmpfs on /yuneta, a
#   passwd where uid 0 is `yuneta`, and a loopback nobody else sees, with its
#   memory bounded (ulimit -v) and no core. The peer stops the agent
#   (SIGSTOP) while it writes, so that the agent reads the Close of a peer
#   and the order to that peer in one turn of its loop. Up to 7.30.26 the
#   order was "Event NOT DEFINED in state" (an ERROR with a stack) in the
#   peer's C_WEBSOCKET, which had left its session at the Close, and was
#   never answered. Then the agent is stopped in order: it must end with 0
#   and with no "system memory not free".
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

timeout 100 unshare -rmn --pid --fork --mount-proc bash -c "
    ip link set lo up || exit 1
    mount -t tmpfs none /yuneta || exit 1
    mount --bind '$WORK/passwd' /etc/passwd || exit 1
    mount --bind '$WORK/group' /etc/group || exit 1
    mkdir -p /yuneta/agent/certs /yuneta/realms/agent /yuneta/store /yuneta/bin
    cp '$WORK/yuneta_agent' /yuneta/agent/yuneta_agent
    cp '$WORK/agent.json' /yuneta/agent/yuneta_agent.json
    cd /yuneta/agent
    ulimit -c 0
    (ulimit -v 4000000; exec ./yuneta_agent --config-file=/yuneta/agent/yuneta_agent.json) > '$WORK/agent.log' 2>&1 &
    APID=\$!
    python3 -u -I '$WORK/client.py' \$APID '$WORK/logcenter'
    echo \"client exit code \$?\" > '$WORK/client.rc'
    kill -CONT \$APID 2>/dev/null
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
    echo "TEST FAILED: the peers did not end well"
    failed=1
fi
if ! grep -q "^agent exit code 0$" "$WORK/client.rc" 2>/dev/null; then
    echo "TEST FAILED: the agent did not end in order (it died?)"
    failed=1
fi
if grep -aq "system memory not free" "$WORK/agent.log"; then
    blocks=$(grep -ac '"msg": "mem-not-free"' "$WORK/agent.log")
    echo "TEST FAILED: the agent ended with memory not free ($blocks blocks)"
    failed=1
fi
#   An order to a peer that is closing: a WARNING, never "Event NOT DEFINED"
#   (7.30.26: an ERROR with a stack in the peer's C_WEBSOCKET, per order)
nd=$(grep -ac "Event NOT DEFINED in state" "$WORK/agent.log")
if [ "$nd" != "0" ]; then
    echo "TEST FAILED: $nd x \"Event NOT DEFINED in state\" in the agent"
    failed=1
fi
#   Each order to the yuno closing (two in each of 4 x 3 cycles) is one
#   WARNING "Not in session"; the answer to an operator closing (one in each
#   of 3 cycles) is dropped with one WARNING "Transport closing, nothing
#   sent" (one per session)
nis=$(grep -a '"msg": "Not in session"' "$WORK/agent.log" | grep -ac "WARNING")
nc=$(grep -a '"msg": "Transport closing, nothing sent"' "$WORK/agent.log" | grep -ac "WARNING")
if [ "$nis" != "24" ] || [ "$nc" != "3" ]; then
    echo "TEST FAILED: $nis WARNING(s) \"Not in session\" (expected 24) and $nc \"Transport closing, nothing sent\" (expected 3)"
    failed=1
fi
if [ $failed -ne 0 ]; then
    echo "---- agent log (errors and warnings) ----"
    grep -aE "ERROR|WARNING" "$WORK/agent.log" | grep -o "\"function\".*" | cut -c1-400 | tail -40
    exit 1
fi
echo "TEST PASSED: every order to a peer that was closing was answered or dropped with a WARNING, never \"Event NOT DEFINED\", and the agent lived"
exit 0
