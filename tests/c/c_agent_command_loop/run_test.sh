#!/bin/bash
#
#   Run the real agent in a sandbox and send it command-agent lines that
#   name command-agent (client.py): it must answer "Loop command" and live
#
#   The agent's 1991/1993 and its pid file are fixed in the binary, so it
#   runs in namespaces of its own (user, mount, network, pid): a tmpfs on
#   /yuneta, a passwd where uid 0 is `yuneta`, and a loopback nobody else
#   sees, with its memory bounded (ulimit -v) and no core. Up to 7.30.26
#   each of those lines made the agent call itself until its stack
#   overflowed (SIGSEGV), writing one audit record per call (~35 000);
#   and so did a line with two levels of quotes, which the parser read
#   twice and the guard once. And a command-agent to the control center's
#   link, which is down in the sandbox, must be answered at once with -1
#   and one WARNING (up to 7.30.26: no answer, and an ERROR with a stack).
#   Then the agent is stopped in order: it must end with 0, with no
#   "system memory not free" (the gbmem audit of CONFIG_DEBUG_TRACK_MEMORY),
#   and its audit file must hold one record per command sent, not one per
#   call to itself.
#
#   Usage: run_test.sh <yuneta_agent binary>
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
skip() {
    echo "SKIPPED: $1"
    exit 77
}
[ -x "$AGENT" ] || skip "no agent binary at '$AGENT' (built by yunetas build)"
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
    python3 -I '$WORK/client.py'
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
    find /yuneta -type f -path '*audit*' -exec cat {} + > '$WORK/audit.txt' 2>/dev/null
"

#
#   Check
#
cat "$WORK/client.rc" 2>/dev/null
failed=0
if ! grep -q "^client exit code 0$" "$WORK/client.rc" 2>/dev/null; then
    echo "TEST FAILED: the peer did not end well"
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
#   The command to the link that is down: one WARNING, no ERROR (7.30.26:
#   an ERROR with a stack, and no answer)
nis=$(grep -a '"msg": "Not in session"' "$WORK/agent.log" | grep -ac "WARNING")
eis=$(grep -a '"msg": "Not in session"' "$WORK/agent.log" | grep -ac "ERROR")
if [ "$nis" != "1" ] || [ "$eis" != "0" ]; then
    echo "TEST FAILED: 'Not in session' logged as $nis WARNING(s) and $eis ERROR(s), expected one WARNING"
    failed=1
fi
n=$(grep -ac "command-agent" "$WORK/audit.txt" 2>/dev/null)
if [ -z "$n" ] || [ "$n" -lt 8 ] || [ "$n" -gt 20 ]; then
    echo "TEST FAILED: ${n:-no} audit records of command-agent, expected one per command sent (8 to 20)"
    failed=1
fi
if [ $failed -ne 0 ]; then
    echo "---- agent log (errors and warnings) ----"
    grep -aE "ERROR|WARNING" "$WORK/agent.log" | cut -c1-600 | tail -40
    exit 1
fi
echo "TEST PASSED: every command-agent that names command-agent answered 'Loop command', the one to a link that is down answered -1, and the agent lived"
exit 0
