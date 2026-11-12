#!/bin/bash
#
#   Run the real agent in a sandbox and send it command-agent lines that
#   name command-agent (client.py): it must answer "Loop command" and live
#
#   The agent's 1991/1993 and its pid file are fixed in the binary, so it
#   runs in namespaces of its own (user, mount, network, pid): a tmpfs on
#   /yuneta, a passwd where uid 0 is `yuneta`, and a loopback nobody else
#   sees, with its memory bounded (ulimit -v, not for an agent built with
#   ASan, whose shadow memory does not fit in any bound) and no core. Up to
#   7.30.26 each of those lines made the agent call itself until its stack
#   overflowed (SIGSEGV), writing one audit record per call (~35 000);
#   and so did a line with two levels of quotes, which the parser read
#   twice and the guard once. And a command-agent to the control center's
#   link, which is down in the sandbox, must be answered at once with -1
#   and one WARNING (up to 7.30.26: no answer, and an ERROR with a stack),
#   and so must a stats-agent of it. A content64 that is not base64 is
#   answered -1 with one WARNING (up to 7.30.26: an ERROR with a stack), and
#   so is a config content64 that is empty or not json, with nothing of the
#   content in the log (7.30.26: ERRORs with a stack and a dump of the whole
#   config); a config that is json but not an object, or whose __version__
#   or __description__ is not a string, and one that its comment cut breaks,
#   are answered -1 with one WARNING that says so (7.30.26: "path MUST BE a
#   json str", ERRORs with a stack; "not json" with no error), and a
#   "gbuffer" key that the comment cut makes is dropped (7.30.26 stored it).
#   A secret whose key the shell's quotes split never reaches the
#   audit file (7.30.26 wrote it). A write-tty to a service that is no
#   console is answered EV_TTY_CLOSE with a WARNING (7.30.26: "Event NOT
#   DEFINED in state", or the agent writing to itself).
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
if [ "$nis" != "2" ] || [ "$eis" != "0" ]; then
    echo "TEST FAILED: 'Not in session' logged as $nis WARNING(s) and $eis ERROR(s), expected two WARNINGs (command, stats)"
    failed=1
fi
#   A content64 that is not base64: one WARNING per command, no ERROR
#   (7.30.26: "b64_decode() FAILED", an ERROR with a stack)
nb=$(grep -a '"msg": "base64 of a peer not decodable, refused"' "$WORK/agent.log" | grep -ac "WARNING")
eb=$(grep -a 'b64_decode() FAILED' "$WORK/agent.log" | grep -ac "ERROR")
if [ "$nb" != "3" ] || [ "$eb" != "0" ]; then
    echo "TEST FAILED: a bad content64 logged $nb WARNING(s) and $eb ERROR(s), expected three WARNINGs"
    failed=1
fi
#   A config content64 that is empty or not json: one WARNING each, no
#   ERROR, and nothing of the content in the log (7.30.26: "len is ZERO" and
#   "json_loads() FAILED", ERRORs with a stack, and a dump of the whole
#   config)
ne=$(grep -a '"msg": "content64 of a config is empty, refused"' "$WORK/agent.log" | grep -ac "WARNING")
nj=$(grep -a '"msg": "content64 of a config is not json, refused"' "$WORK/agent.log" | grep -ac "WARNING")
ej=$(grep -aE 'len is ZERO|json_loads\(\) FAILED' "$WORK/agent.log" | grep -ac "ERROR")
dj=$(grep -acE 'comments removed|PRIVATE KEY|MIIEvQ' "$WORK/agent.log")
if [ "$ne" != "2" ] || [ "$nj" != "2" ] || [ "$ej" != "0" ] || [ "$dj" != "0" ]; then
    echo "TEST FAILED: an empty / not json config logged $ne / $nj WARNING(s), $ej ERROR(s) and $dj line(s) of its content, expected two WARNINGs each and nothing else"
    failed=1
fi
#   A "gbuffer" key in a config, however its comment cut makes it: dropped
#   with one WARNING per config (7.30.26: only the plain key, the two that
#   the cut makes were stored)
ngb=$(grep -a "Json of a peer with 'gbuffer' keys, a binary field name: dropped" "$WORK/agent.log" | grep -ac "WARNING")
if [ "$ngb" != "3" ]; then
    echo "TEST FAILED: $ngb WARNING(s) for a \"gbuffer\" key in a config, expected three"
    failed=1
fi
#   A config that is json and is not once its comments are cut: the WARNING
#   says that (7.30.26: "not json", with no error and no position)
ncut=$(grep -a '"msg": "content64 of a config is not json once its comments are removed, refused"' "$WORK/agent.log" | grep -ac "WARNING")
if [ "$ncut" != "1" ]; then
    echo "TEST FAILED: $ncut WARNING(s) for a config that its comment cut breaks, expected one"
    failed=1
fi
#   A config that is json but not an object, or whose __version__ or
#   __description__ is not a string: one WARNING per order, no ERROR
#   (7.30.26: "path MUST BE a json str", an ERROR with a stack, up to two
#   per order)
nno=$(grep -a '"msg": "content64 of a config is not a json object, refused"' "$WORK/agent.log" | grep -ac "WARNING")
nns=$(grep -a '"msg": "content64 of a config with a key of its own that is not a string, refused"' "$WORK/agent.log" | grep -ac "WARNING")
ens=$(grep -a 'path MUST BE a json str' "$WORK/agent.log" | grep -ac "ERROR")
if [ "$nno" != "2" ] || [ "$nns" != "4" ] || [ "$ens" != "0" ]; then
    echo "TEST FAILED: a config of a bad shape logged $nno / $nns WARNING(s) and $ens ERROR(s), expected two and four WARNINGs and no ERROR"
    failed=1
fi
#   A write-tty to a service that is not a console: one WARNING each, no
#   ERROR (7.30.26: "Event NOT DEFINED in state" with a stack, and the agent
#   writing to itself: "content64 required")
#   And one without content64: one WARNING (7.30.26: an ERROR)
nc=$(grep -a '"msg": "not a console"' "$WORK/agent.log" | grep -ac "WARNING")
nr=$(grep -a '"msg": "content64 required"' "$WORK/agent.log" | grep -ac "WARNING")
ec=$(grep -aE 'Event NOT DEFINED in state|content64 required' "$WORK/agent.log" | grep -ac "ERROR")
if [ "$nc" != "2" ] || [ "$nr" != "1" ] || [ "$ec" != "0" ]; then
    echo "TEST FAILED: a write-tty to a service that is no console / without content64 logged $nc / $nr WARNING(s) and $ec ERROR(s), expected two WARNINGs and one"
    failed=1
fi
#   A secret's key split by the shell's quotes: never in the audit file
#   (7.30.26 wrote the five of client.py with their secret)
na=$(grep -ac "AUDITSEKRET" "$WORK/audit.txt" 2>/dev/null)
if [ "${na:-0}" != "0" ]; then
    echo "TEST FAILED: $na audit record(s) hold a secret whose key the shell's quotes split"
    grep -a "AUDITSEKRET" "$WORK/audit.txt" | cut -c1-300
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
echo "TEST PASSED: every command-agent that names command-agent answered 'Loop command', a command and a stats to a link that is down answered -1, a bad content64 and a bad config -1 with a WARNING, no secret of a quoted key in the audit, a write-tty to no console refused, and the agent lived"
exit 0
