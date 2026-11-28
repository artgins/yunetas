#!/bin/bash
#
#   Run the real yuneta_agent22 in a sandbox, linked to a control center
#   written in python (control_center.py), and send it write-tty lines that
#   name no console: they must be refused and the link must stay.
#
#   agent22 has no listener of its own: everything reaches it through its
#   link to the control center (C_IEVENT_CLI over tcp4h and TLS, pinned to
#   certs/controlcenter.crt). So it runs in namespaces of its own (user,
#   mount, network, pid): a tmpfs on /yuneta with throwaway certificates, a
#   passwd where uid 0 is `yuneta`, an /etc/hosts that sends the control
#   center's name to the loopback, its memory bounded (ulimit -v, not for an
#   agent22 built with ASan, whose shadow memory does not fit in any bound)
#   and no core.
#
#   Up to 7.30.26 a write-tty for a console that is not there (a key typed
#   right after the console closed) and one without content64 were an ERROR
#   and an EV_DROP of the whole link to the control center; and a write-tty
#   to a service that is no console (__yuno__, agent22) was sent to it:
#   "Event NOT DEFINED in state" with a stack, or agent22 writing to itself.
#   Now: one WARNING each, EV_TTY_CLOSE to the requester for a name that is
#   no console, and the link stays (a help after them is answered). Then
#   the agent is stopped in order: exit 0, no "system memory not free".
#
#   And a console of the control center that floods (`yes`) while the
#   control center does not read its link for 8 s: agent22's memory stays
#   bounded (its console flow control: the console pauses while the link
#   has console_pause_tx_queue messages waiting, and waits for it, the link
#   being its only route), and when the link is read again the console goes
#   on; close-console closes it. Up to 7.30.26 agent22 grew until it
#   aborted. The run stops at 300 MB of growth. The same with the request
#   padded with 1 MB, in a key of its own of __md_iev__ and in the host of
#   its hop: agent22 grows less than 25 MB, and the padding of its own is
#   not in the frames of output (before the fix: +150 MB or more, and every
#   chunk carried the padding).
#
#   And an operator whose only role is agent22-install-package, asserted as
#   a control center asserts it: its keys for a console that root opened
#   are refused (EV_TTY_CLOSE, a WARNING) and the console goes on for root;
#   open-console and upgrade-agent answer "No permission", and
#   install-package-status does not. Up to 7.32.4 write-tty asked no
#   permission, and the keys of such an operator reached the console.
#
#   Usage: run_test.sh <yuneta_agent22 binary>
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
[ -x "$AGENT" ] || skip "no agent22 binary at '$AGENT' (built by yunetas build)"
command -v openssl >/dev/null || skip "no 'openssl' command to make the certificates"
command -v ip >/dev/null || skip "no 'ip' command to bring up the loopback of the sandbox"
unshare -rmn --pid --fork --mount-proc true 2>/dev/null || skip "unprivileged namespaces not allowed here"
[ -d /yuneta ] || skip "no /yuneta to mount the sandbox on"

#
#   An agent22 built with ASan reserves terabytes of address space for its
#   shadow memory: under ulimit -v it cannot start, and the test would blame
#   agent22
#
VMEM_LIMIT="ulimit -v 4000000;"
if grep -aq "__asan_init" "$AGENT"; then
    VMEM_LIMIT=""
    echo "an agent22 built with ASan: no ulimit -v"
fi

#
#   Sandbox
#
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cp "$AGENT" "$WORK/yuneta_agent22"
cp "$HERE/control_center.py" "$WORK/"
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
#   The key of the operator assertions (control_center.py signs them), and
#   its public half as a JWK: x and y are the last 64 bytes of the DER
openssl ecparam -name prime256v1 -genkey -noout -out "$WORK/as.key" 2>/dev/null \
    || skip "openssl cannot make an EC P-256 key"
AS_XY=$(openssl ec -in "$WORK/as.key" -pubout -outform DER 2>/dev/null | tail -c 64 | python3 -I -c '
import base64, sys
b = sys.stdin.buffer.read()
print(base64.urlsafe_b64encode(b[:32]).rstrip(b"=").decode(), base64.urlsafe_b64encode(b[32:]).rstrip(b"=").decode())
')
read -r AS_X AS_Y <<< "$AS_XY"
[ -n "$AS_Y" ] || skip "openssl cannot write the public half of the EC key"
cat > "$WORK/yuneta_agent22.json" <<'EOF'
{
    "global": {
        "controlcenter.__json_config_variables__": {
            "__output_url__": "cc.test:19899"
        },
        "authz.master": true,
        "authz.authz_service": "agent",
        "Authz.assertion_jwks": [
            {
                "kid": "test-controlcenter",
                "kty": "EC",
                "crv": "P-256",
                "alg": "ES256",
                "use": "sig",
                "x": "__AS_X__",
                "y": "__AS_Y__"
            }
        ],
        "authz.initial_load": {
            "roles": [
                {
                    "id": "root",
                    "disabled": false,
                    "description": "Super-Owner of system",
                    "realm_id": "*",
                    "parent_role_id": "",
                    "service": "*",
                    "permission": "*"
                },
                {
                    "id": "agent22-install-package",
                    "disabled": false,
                    "description": "install-package of agent22, nothing else",
                    "realm_id": "*",
                    "parent_role_id": "",
                    "service": "agent22",
                    "permission": "install-package"
                }
            ],
            "users": [
                {
                    "id": "controlcenter",
                    "roles": [
                        "roles^root^users"
                    ]
                },
                {
                    "id": "deployer@sandbox.test",
                    "roles": [
                        "roles^agent22-install-package^users"
                    ]
                }
            ]
        }
    },
    "environment": {
        "node_owner": "sandbox"
    }
}
EOF
sed -i -e "s|__AS_X__|$AS_X|" -e "s|__AS_Y__|$AS_Y|" "$WORK/yuneta_agent22.json"

timeout 200 unshare -rmn --pid --fork --mount-proc bash -c "
    ip link set lo up || exit 1
    mount -t tmpfs none /yuneta || exit 1
    mount --bind '$WORK/passwd' /etc/passwd || exit 1
    mount --bind '$WORK/group' /etc/group || exit 1
    mount --bind '$WORK/hosts' /etc/hosts || exit 1
    mkdir -p /yuneta/agent/certs /yuneta/realms/agent /yuneta/store /yuneta/bin
    cp '$WORK/cc.crt' /yuneta/agent/certs/controlcenter.crt
    cp '$WORK/agent.crt' /yuneta/agent/certs/yuneta_agent.crt
    cp '$WORK/agent.key' /yuneta/agent/certs/yuneta_agent.key
    cp '$WORK/yuneta_agent22' /yuneta/agent/yuneta_agent22
    cp '$WORK/yuneta_agent22.json' /yuneta/agent/yuneta_agent22.json
    cd /yuneta/agent
    ulimit -c 0
    python3 -I '$WORK/control_center.py' '$WORK/cc.crt' '$WORK/cc.key' 19899 \
        'write-tty name=nosuch content64=YQ==' \
        'write-tty name=__yuno__ content64=YQ==' \
        'write-tty name=agent22 content64=YQ==' \
        'write-tty name=nosuch' \
        'help' \
        'ASSERTION_KEY $WORK/as.key' \
        'open-console name=c1 process=cat cwd=/yuneta' \
        'AS deployer@sandbox.test write-tty name=c1 content64=YQ==' \
        'write-tty name=c1 content64=Ygo=' \
        'AS deployer@sandbox.test open-console name=c2 process=cat cwd=/yuneta' \
        'AS deployer@sandbox.test upgrade-agent content64=YQ==' \
        'AS deployer@sandbox.test install-package-status' \
        'close-console name=c1' \
        'FLOOD' \
        'FLOODPAD md' \
        'FLOODPAD hop' \
        'help' > '$WORK/cc.out' 2>&1 &
    CC=\$!
    sleep 0.5
    ($VMEM_LIMIT exec ./yuneta_agent22 --config-file=/yuneta/agent/yuneta_agent22.json) > '$WORK/agent.log' 2>&1 &
    APID=\$!
    wait \$CC
    echo \"control center exit code \$?\" >> '$WORK/cc.out'
    kill -INT \$APID 2>/dev/null
    for i in \$(seq 1 150); do
        kill -0 \$APID 2>/dev/null || break
        sleep 0.1
    done
    if kill -0 \$APID 2>/dev/null; then
        echo 'the agent did not end in 15 s' >> '$WORK/cc.out'
        kill -9 \$APID
    fi
    wait \$APID
    echo \"agent exit code \$?\" >> '$WORK/cc.out'
"

#
#   Check
#
cat "$WORK/cc.out" 2>/dev/null
failed=0
if ! grep -q "^control center exit code 0$" "$WORK/cc.out" 2>/dev/null; then
    echo "TEST FAILED: the link to the control center did not live through the write-tty lines"
    failed=1
fi
for name in nosuch __yuno__ agent22; do
    if ! grep -q "^LINE 'write-tty name=$name content64=YQ==': \[\[\"EV_TTY_CLOSE\", 0, \"$name\"\]\]$" "$WORK/cc.out"; then
        echo "TEST FAILED: write-tty name=$name was not answered with EV_TTY_CLOSE"
        failed=1
    fi
done
if ! grep -q "^LINE 'help': \[\[\"EV_MT_COMMAND_ANSWER\", 0, null\]\]$" "$WORK/cc.out"; then
    echo "TEST FAILED: the help after them was not answered"
    failed=1
fi
#   An operator whose only role is agent22-install-package: its keys for a
#   console of root are refused and the console goes on for root; its
#   open-console and upgrade-agent have no permission, its
#   install-package-status has
D="deployer@sandbox.test"
if ! grep -q "^LINE 'open-console name=c1 process=cat cwd=/yuneta': .*\"EV_MT_COMMAND_ANSWER\", 0," "$WORK/cc.out"; then
    echo "TEST FAILED: root could not open the console c1"
    failed=1
fi
if ! grep -q "^LINE 'AS $D write-tty name=c1 content64=YQ==': \[\[\"EV_TTY_CLOSE\", 0, \"c1\"\]\]$" "$WORK/cc.out"; then
    echo "TEST FAILED: the keys of $D for the console of root were not refused with EV_TTY_CLOSE"
    failed=1
fi
if ! grep "^LINE 'write-tty name=c1 content64=Ygo=': " "$WORK/cc.out" | grep -q '"EV_TTY_DATA"' ||
        grep "^LINE 'write-tty name=c1 content64=Ygo=': " "$WORK/cc.out" | grep -q '"EV_TTY_CLOSE"'; then
    echo "TEST FAILED: after the keys of $D were refused, the console did not go on for root"
    failed=1
fi
for cmd in "open-console name=c2 process=cat cwd=/yuneta" "upgrade-agent content64=YQ=="; do
    perm=${cmd%% *}
    if ! grep -q "^COMMENT 'AS $D $cmd': .*No permission to '$perm'" "$WORK/cc.out"; then
        echo "TEST FAILED: $perm of $D was not refused for its permission"
        failed=1
    fi
done
if ! grep -q "^COMMENT 'AS $D install-package-status': " "$WORK/cc.out" ||
        grep -q "^COMMENT 'AS $D install-package-status': .*No permission" "$WORK/cc.out"; then
    echo "TEST FAILED: install-package-status of $D did not run (its role grants it)"
    failed=1
fi
#   A console that floods while the control center does not read its link:
#   agent22's memory bounded, the console paused and then going on
if ! grep -q "^FLOOD OK$" "$WORK/cc.out"; then
    echo "TEST FAILED: a console that floods while the link is not read (see FLOOD above)"
    failed=1
fi
#   The same with the request padded with 1 MB: memory bounded by the bytes
#   waiting, and the padding of the control center's own not in the output
for where in md hop; do
    if ! grep -q "^FLOODPAD $where OK$" "$WORK/cc.out"; then
        echo "TEST FAILED: a console opened by a request padded with 1 MB ($where, see FLOODPAD above)"
        failed=1
    fi
done
if ! grep -q "^agent exit code 0$" "$WORK/cc.out" 2>/dev/null; then
    echo "TEST FAILED: the agent did not end in order (it died?)"
    failed=1
fi
if grep -aq "system memory not free" "$WORK/agent.log"; then
    echo "TEST FAILED: the agent ended with memory not free"
    failed=1
fi
nf=$(grep -a '"function": "ac_write_tty"' "$WORK/agent.log" | grep -a "WARNING" | grep -ac '"msg": "console not found"')
nc=$(grep -a '"function": "ac_write_tty"' "$WORK/agent.log" | grep -a "WARNING" | grep -ac '"msg": "not a console"')
nr=$(grep -a '"function": "ac_write_tty"' "$WORK/agent.log" | grep -a "WARNING" | grep -ac '"msg": "content64 required"')
np=$(grep -a '"function": "ac_write_tty"' "$WORK/agent.log" | grep -a "WARNING" | grep -a "\"username\": \"$D\"" | grep -ac '"msg": "write-tty refused: no permission to open-console"')
ne=$(grep -aE 'ac_write_tty|Event NOT DEFINED in state' "$WORK/agent.log" | grep -ac "ERROR")
if [ "$nf" != "1" ] || [ "$nc" != "2" ] || [ "$nr" != "1" ] || [ "$np" != "1" ] || [ "$ne" != "0" ]; then
    echo "TEST FAILED: write-tty logged $nf/$nc/$nr/$np WARNING(s) (console not found / not a console / content64 required / refused to $D) and $ne ERROR(s), expected 1/2/1/1 and none"
    failed=1
fi
if [ $failed -ne 0 ]; then
    echo "---- agent log (errors and warnings) ----"
    grep -aE "ERROR|WARNING" "$WORK/agent.log" | cut -c1-600 | tail -40
    exit 1
fi
echo "TEST PASSED: a write-tty to no console, or without content64, was refused with a WARNING and the link to the control center stayed; the keys of an operator with only agent22-install-package were refused and the console went on for root; a console that floods while the link is not read kept agent22's memory bounded"
exit 0
