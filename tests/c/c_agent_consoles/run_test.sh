#!/bin/bash
#
#   Run the real agent in a sandbox, and open consoles from sessions on its
#   1991 (client.py): a session that floods its console and does not read,
#   a joined session that does not read while the other one does, a
#   joined session that leaves, and the console of a session that presents
#   the card of a yuno, and an opener that does not read with a joined
#   session that does. The agent's memory must stay bounded, a session
#   that does not read is taken off the console with ONE WARNING when it
#   stops another one (an opener alone on its console is waited for), the
#   console lives until the session that opened it leaves (and then every
#   session still joined is told), and a console never reaches another
#   session. Up to 7.30.26 the agent grew until it aborted, a joiner that
#   left closed the console for everybody without telling them, and the
#   console of a session with a yuno's card went to the next session of
#   its channel.
#
#   The agent's 1991 and its pid file are fixed in the binary, so it runs in
#   namespaces of its own (user, mount, network, pid): a tmpfs on /yuneta, a
#   passwd where uid 0 is `yuneta`, and a loopback nobody else sees, with its
#   memory bounded (ulimit -v, not for an agent built with ASan, whose
#   shadow memory does not fit in any bound) and no core. Then the agent is
#   stopped in order: it must end with 0 and with no "system memory not
#   free".
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

timeout 240 unshare -rmn --pid --fork --mount-proc bash -c "
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
if [ -n "$KEEP_AGENT_LOG" ]; then
    cp "$WORK/agent.log" "$KEEP_AGENT_LOG"
fi
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
#   The two sessions that did not read: ONE WARNING each
nslow=$(grep -a '"msg": "console output not taken by its session, console closed for it"' "$WORK/agent.log" | grep -ac "WARNING")
nopen=$(grep -a '"msg": "console output not taken by its session, console closed for it"' "$WORK/agent.log" | grep -ac "yes, the console is closed")
if [ "$nslow" != "3" ] || [ "$nopen" != "2" ]; then
    echo "TEST FAILED: $nslow WARNING(s) for a session that does not take its console output (expected 3: the joiner of case 2, the openers of cases 6 and 7), $nopen for a session that opened it (expected 2: cases 6 and 7; the opener alone of case 1 is never taken off)"
    failed=1
fi
#   The output of a console for a route not listening: said once per route,
#   never per chunk
ngone=$(grep -a '"msg": "requester gone before its answer, answer dropped"' "$WORK/agent.log" | grep -ac "EV_TTY_DATA")
if [ "$ngone" -gt 2 ]; then
    echo "TEST FAILED: $ngone INFO(s) \"requester gone\" for the output of a console (expected at most 2)"
    failed=1
fi
if [ $failed -ne 0 ]; then
    echo "---- agent log (errors and warnings) ----"
    grep -aE "ERROR|WARNING" "$WORK/agent.log" | grep -o "\"function\".*" | cut -c1-400 | tail -40
    exit 1
fi
echo "TEST PASSED: the agent's memory stayed bounded with a session that does not read, a joiner that left did not close the console, and no console reached another session"
exit 0
