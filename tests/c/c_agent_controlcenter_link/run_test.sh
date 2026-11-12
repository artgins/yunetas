#!/bin/bash
#
#   Run the real agent in a sandbox, linked to a control center written in
#   python (client.py), and send it command-agent and stats-agent to
#   service=controlcenter while the link is in session: each must be
#   answered with what the control center said. Answers that a session of
#   the agent sends (naming the link, or no requester) must reach nobody,
#   and an answer must not reach the session that took the channel of the
#   one that asked.
#
#   The agent's 1991 and its pid file are fixed in the binary, so it runs in
#   namespaces of its own (user, mount, network, pid): a tmpfs on /yuneta
#   with throwaway certificates (the link is tcp4h over TLS, pinned to
#   certs/controlcenter.crt), a passwd where uid 0 is `yuneta`, an
#   /etc/hosts that sends the control center's name to the loopback, its
#   memory bounded (ulimit -v, not for an agent built with ASan, whose
#   shadow memory does not fit in any bound) and no core.
#
#   Up to 7.30.26 the request went up with the operator's channel of the
#   agent as its `src_service`, which is no service: the link dropped the
#   control center's answer ("service of the event not found, event
#   dropped") and the operator waited for its timeout. Then the agent is
#   stopped in order: exit 0, no "system memory not free".
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
command -v openssl >/dev/null || skip "no 'openssl' command to make the certificates"
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
#   The link's url is tcps://<machine>.<node_owner>.<__output_url__>
MACHINE=$(uname -m | tr _ -)
printf '127.0.0.1 localhost\n127.0.0.1 %s.sandbox.cc.test\n' "$MACHINE" > "$WORK/hosts"
openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=yuneta_agent.yuneta.io" \
    -addext "subjectAltName=DNS:yuneta_agent.yuneta.io" \
    -keyout "$WORK/cc.key" -out "$WORK/cc.crt" 2>/dev/null || skip "openssl cannot make a certificate"
openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=agent" \
    -keyout "$WORK/agent.key" -out "$WORK/agent.crt" 2>/dev/null || skip "openssl cannot make a certificate"

timeout 100 unshare -rmn --pid --fork --mount-proc bash -c "
    ip link set lo up || exit 1
    mount -t tmpfs none /yuneta || exit 1
    mount --bind '$WORK/passwd' /etc/passwd || exit 1
    mount --bind '$WORK/group' /etc/group || exit 1
    mount --bind '$WORK/hosts' /etc/hosts || exit 1
    mkdir -p /yuneta/agent/certs /yuneta/realms/agent /yuneta/store /yuneta/bin
    cp '$WORK/cc.crt' /yuneta/agent/certs/controlcenter.crt
    cp '$WORK/agent.crt' /yuneta/agent/certs/yuneta_agent.crt
    cp '$WORK/agent.key' /yuneta/agent/certs/yuneta_agent.key
    cp '$WORK/yuneta_agent' /yuneta/agent/yuneta_agent
    cp '$WORK/agent.json' /yuneta/agent/yuneta_agent.json
    cd /yuneta/agent
    ulimit -c 0
    python3 -u -I '$WORK/client.py' '$WORK/cc.crt' '$WORK/cc.key' 19898 > '$WORK/client.out' 2>&1 &
    CPID=\$!
    sleep 0.5
    ($VMEM_LIMIT exec ./yuneta_agent --config-file=/yuneta/agent/yuneta_agent.json) > '$WORK/agent.log' 2>&1 &
    APID=\$!
    wait \$CPID
    echo \"client exit code \$?\" >> '$WORK/client.out'
    kill -INT \$APID 2>/dev/null
    for i in \$(seq 1 150); do
        kill -0 \$APID 2>/dev/null || break
        sleep 0.1
    done
    if kill -0 \$APID 2>/dev/null; then
        echo 'the agent did not end in 15 s' >> '$WORK/client.out'
        kill -9 \$APID
    fi
    wait \$APID
    echo \"agent exit code \$?\" >> '$WORK/client.out'
"

#
#   Check
#
cat "$WORK/client.out" 2>/dev/null
failed=0
if ! grep -q "^client exit code 0$" "$WORK/client.out" 2>/dev/null; then
    echo "TEST FAILED: the operator or the control center did not end well"
    failed=1
fi
if ! grep -q "^agent exit code 0$" "$WORK/client.out" 2>/dev/null; then
    echo "TEST FAILED: the agent did not end in order (it died?)"
    failed=1
fi
if grep -aq "system memory not free" "$WORK/agent.log"; then
    blocks=$(grep -ac '"msg": "mem-not-free"' "$WORK/agent.log")
    echo "TEST FAILED: the agent ended with memory not free ($blocks blocks)"
    failed=1
fi
#   7.30.26: the link dropped each answer of the control center
nd=$(grep -ac "service of the event not found, event dropped" "$WORK/agent.log")
if [ "$nd" != "0" ]; then
    echo "TEST FAILED: $nd answer(s) of the control center dropped by the link (\"service of the event not found\")"
    failed=1
fi
#   An answer of a session to no requester is a peer's doing: a WARNING
#   with a brake, never an ERROR "child not found" (7.30.26). Within the
#   interval of the brake, one line for each kind: bob's two answers naming
#   the link, and bob's three to "nosuch" with the control center's one.
ncf=$(grep -ac '"msg": "child not found"' "$WORK/agent.log")
nfl=$(grep -a '"msg": "answer naming a requester of a link, not from a link: dropped"' "$WORK/agent.log" | grep -ac "WARNING")
nnr=$(grep -a '"msg": "requester of the answer not found: dropped"' "$WORK/agent.log" | grep -ac "WARNING")
if [ "$ncf" != "0" ] || [ "$nfl" != "1" ] || [ "$nnr" != "1" ]; then
    echo "TEST FAILED: $ncf ERROR(s) \"child not found\" (expected 0), $nfl WARNING(s) for an answer naming the link not from it (expected 1) and $nnr for an answer to no requester (expected 1)"
    failed=1
fi
#   The answer of the session that left is dropped as the requester's leaving
ngone=$(grep -ac '"msg": "requester gone before its answer, answer dropped"' "$WORK/agent.log")
if [ "$ngone" = "0" ]; then
    echo "TEST FAILED: the answer of the session that left was not dropped as its leaving"
    failed=1
fi
if [ $failed -ne 0 ]; then
    echo "---- agent log (errors and warnings) ----"
    grep -aE "ERROR|WARNING" "$WORK/agent.log" | grep -o '"function".*' | cut -c1-400 | tail -40
    exit 1
fi
echo "TEST PASSED: command-agent and stats-agent to the control center in session were answered with what it said; answers of a session reached nobody; an answer did not reach the session that took the requester's channel"
exit 0
