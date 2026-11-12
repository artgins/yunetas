#!/bin/bash
#
#   Run the real agent in a sandbox, and send it answers from its 1991
#   (client.py): a reading of a watched yuno and the answer to a
#   publish-stats that a session sends instead of the yuno, and the answers
#   of a yuno to a session that left before them, while another session
#   took its channel. The forged answers must be dropped with a WARNING,
#   and the answers to the session that left must not reach the one that
#   took its channel.
#
#   The agent's 1991 and its pid file are fixed in the binary, so it runs in
#   namespaces of its own (user, mount, network, pid): a tmpfs on /yuneta, a
#   passwd where uid 0 is `yuneta`, and a loopback nobody else sees, with its
#   memory bounded (ulimit -v, not for an agent built with ASan, whose
#   shadow memory does not fit in any bound) and no core. Up to 7.30.26 the
#   watcher got the reading a session forged (or another yuno), the agent
#   took the yuno as refusing its publication, the session that took the
#   channel got the answer of the one that left, and a session that sent a
#   `__temp__` of its own made the agent take its number for a pointer
#   (SIGSEGV). Then the agent is stopped in order: it must
#   end with 0 and with no "system memory not free".
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
#   An agent built with ASan reserves terabytes of address space for its
#   shadow memory: under ulimit -v it cannot start, and the test would blame
#   the agent
#
VMEM_LIMIT="ulimit -v 4000000;"
if grep -aq "__asan_init" "$AGENT"; then
    VMEM_LIMIT=""
    echo "an agent built with ASan: no ulimit -v"
fi

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
    ($VMEM_LIMIT exec ./yuneta_agent --config-file=/yuneta/agent/yuneta_agent.json) > '$WORK/agent.log' 2>&1 &
    APID=\$!
    python3 -u -I '$WORK/client.py' '$WORK/logcenter'
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
nd=$(grep -ac "Event NOT DEFINED in state" "$WORK/agent.log")
if [ "$nd" != "0" ]; then
    echo "TEST FAILED: $nd x \"Event NOT DEFINED in state\" in the agent"
    failed=1
fi
#   The two answers a session sent for the agent: one WARNING (with a brake),
#   and the publication of the yuno is not taken as refused
nfy=$(grep -a '"msg": "answer for the agent, not from the yuno it names: dropped"' "$WORK/agent.log" | grep -ac "WARNING")
nref=$(grep -ac '"msg": "publish-stats refused, the agent asks for these stats"' "$WORK/agent.log")
if [ "$nfy" != "1" ] || [ "$nref" != "0" ]; then
    echo "TEST FAILED: $nfy WARNING(s) for an answer for the agent not from its yuno (expected 1), the publication taken as refused $nref time(s) (expected 0)"
    failed=1
fi
#   The answer of the yuno to a service that is no requester: one WARNING
nnr=$(grep -a '"msg": "requester of the answer not found: dropped"' "$WORK/agent.log" | grep -ac "WARNING")
if [ "$nnr" != "1" ]; then
    echo "TEST FAILED: $nnr WARNING(s) for an answer to a service that is no requester (expected 1)"
    failed=1
fi
#   The answers to the sessions that left (command-yuno, stats-yuno,
#   authzs-yuno) are dropped as their leaving
ngone=$(grep -ac '"msg": "requester gone before its answer, answer dropped"' "$WORK/agent.log")
if [ "$ngone" -lt 3 ]; then
    echo "TEST FAILED: $ngone answer(s) dropped as the leaving of their requester (expected 3)"
    failed=1
fi
#   The acks of play / pause a session sent: one WARNING (with a brake), no
#   ERROR of a channel read as a yuno's
nack=$(grep -a '"msg": "ack not from a yuno: dropped"' "$WORK/agent.log" | grep -ac "WARNING")
nhs=$(grep -ac '"msg": "hs_yuno NULL"' "$WORK/agent.log")
ngn=$(grep -ac '"msg": "gobj NULL"' "$WORK/agent.log")
if [ "$nack" != "1" ] || [ "$nhs" != "0" ] || [ "$ngn" != "0" ]; then
    echo "TEST FAILED: $nack WARNING(s) for an ack not from a yuno (expected 1), $nhs \"hs_yuno NULL\" and $ngn \"gobj NULL\" (expected 0)"
    failed=1
fi
#   The stop with a watch armed: no ERROR of a treedb already closed
nhn=$(grep -ac '"msg": "hgobj NULL or DESTROYED"' "$WORK/agent.log")
if [ "$nhn" != "0" ]; then
    echo "TEST FAILED: $nhn ERROR(s) \"hgobj NULL or DESTROYED\" (expected 0)"
    failed=1
fi
if [ $failed -ne 0 ]; then
    echo "---- agent log (errors and warnings) ----"
    grep -aE "ERROR|WARNING" "$WORK/agent.log" | grep -o "\"function\".*" | cut -c1-400 | tail -40
    exit 1
fi
echo "TEST PASSED: the answers a session forged for the agent were dropped, and the answers to a session that left did not reach the one that took its channel"
exit 0
