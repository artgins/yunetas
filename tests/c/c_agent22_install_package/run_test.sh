#!/bin/bash
#
#   Run the real yuneta_agent22 in a sandbox, linked to the control center
#   of c_agent22_write_tty (control_center.py), and send it install-package
#   and install-package-status.
#
#   The sandbox is the one of c_agent22_write_tty (user, mount, network and
#   pid namespaces; a tmpfs on /yuneta, so this is no node that builds from
#   source), plus: a tmpfs on /var/lib, a directory of the test on
#   /usr/libexec/yuneta, and stubs on /usr/bin/sudo (it runs its command),
#   systemd-run (it writes its arguments in /yuneta/systemd-run.log),
#   systemctl and journalctl. The root program is the real one,
#   packages/templates/install-package, run as the root of the namespace.
#
#   What it pins:
#     1. With no /usr/libexec/yuneta/install-package (a package of 7.31.29
#        or before): install-package is refused with an answer that says so,
#        and nothing is written. Up to 7.31.29 agent22 wrote a script into
#        /yuneta/agent/install-package and had root run it.
#     2. With it: install-package answers "installing <file> in the unit
#        yuneta-install-package-<time>"; the unit runs
#        "/usr/libexec/yuneta/install-package run" with no url on its
#        command line; the url and the sha256 are in
#        /var/lib/yuneta/install-package, root's; agent22 wrote no script.
#     3. An http url, and one with a curl glob ([], {}), are refused by
#        agent22.
#     4. install-package-status reads the result of root's directory, and
#        the one an agent22 of 7.31.29 left in /yuneta/agent/install-package
#        when root's has none.
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
CC_PY="$HERE/../c_agent22_write_tty/control_center.py"
PROGRAM="$HERE/../../../packages/templates/install-package"
skip() {
    echo "SKIPPED: $1"
    exit 77
}
[ -x "$AGENT" ] || skip "no agent22 binary at '$AGENT' (built by yunetas build)"
[ -f "$CC_PY" ] || skip "no control center at '$CC_PY'"
[ -x "$PROGRAM" ] || skip "no install-package program at '$PROGRAM'"
command -v openssl >/dev/null || skip "no 'openssl' command to make the certificates"
command -v ip >/dev/null || skip "no 'ip' command to bring up the loopback of the sandbox"
command -v curl >/dev/null || skip "no 'curl': the program refuses a node without it"
unshare -rmn --pid --fork --mount-proc true 2>/dev/null || skip "unprivileged namespaces not allowed here"
[ -d /yuneta ] || skip "no /yuneta to mount the sandbox on"
for f in /usr/bin/sudo /usr/bin/systemd-run /usr/bin/systemctl /usr/bin/journalctl; do
    [ -e "$f" ] || skip "no $f to put a stub on"
done
if command -v apt-get >/dev/null && command -v dpkg-deb >/dev/null; then
    PKG="yuneta-agent-7.31.29-1-amd64.deb"
elif command -v dnf >/dev/null && command -v rpm >/dev/null; then
    PKG="yuneta-agent-7.31.29-1.x86_64.rpm"
else
    skip "neither apt-get + dpkg-deb nor dnf + rpm"
fi
URL="https://github.com/artgins/yunetas/releases/download/7.31.29/$PKG"
SHA="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

#
#   Sandbox
#
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cp "$AGENT" "$WORK/yuneta_agent22"
cp "$CC_PY" "$WORK/"
cp "$PROGRAM" "$WORK/install-package"
mkdir -p "$WORK/libexec"
printf 'yuneta:x:0:0:yuneta:/yuneta:/bin/bash\n' > "$WORK/passwd"
printf 'yuneta:x:0:yuneta\n' > "$WORK/group"
#   The link's url is tcps://<machine>.<node_owner>.<__output_url__>
MACHINE=$(uname -m | tr _ -)
printf '127.0.0.1 localhost\n127.0.0.1 %s.sandbox.cc.test\n' "$MACHINE" > "$WORK/hosts"
printf '#!/bin/sh\nif [ "$1" = "-n" ]; then\n    shift\nfi\nexec "$@"\n' > "$WORK/sudo"
printf '#!/bin/sh\necho "$*" >> /yuneta/systemd-run.log\n' > "$WORK/systemd-run"
printf '#!/bin/sh\nexit 0\n' > "$WORK/systemctl"
printf '#!/bin/sh\necho "journalctl $*"\n' > "$WORK/journalctl"
chmod 0755 "$WORK/sudo" "$WORK/systemd-run" "$WORK/systemctl" "$WORK/journalctl"
openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=yuneta_agent.yuneta.io" \
    -addext "subjectAltName=DNS:yuneta_agent.yuneta.io" \
    -keyout "$WORK/cc.key" -out "$WORK/cc.crt" 2>/dev/null || skip "openssl cannot make a certificate"
openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=agent" \
    -keyout "$WORK/agent.key" -out "$WORK/agent.crt" 2>/dev/null || skip "openssl cannot make a certificate"
cat > "$WORK/yuneta_agent22.json" <<'EOF'
{
    "global": {
        "controlcenter.__json_config_variables__": {
            "__output_url__": "cc.test:19899"
        },
        "authz.master": true,
        "authz.authz_service": "agent",
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
                }
            ],
            "users": [
                {
                    "id": "controlcenter",
                    "roles": [
                        "roles^root^users"
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

timeout 120 unshare -rmn --pid --fork --mount-proc bash -c "
    ip link set lo up || exit 1
    mount -t tmpfs none /yuneta || exit 1
    mount -t tmpfs none /var/lib || exit 1
    mount --bind '$WORK/passwd' /etc/passwd || exit 1
    mount --bind '$WORK/group' /etc/group || exit 1
    mount --bind '$WORK/hosts' /etc/hosts || exit 1
    if [ -d /usr/libexec/yuneta ]; then
        mount --bind '$WORK/libexec' /usr/libexec/yuneta || exit 1
    else
        mount -t tmpfs none /usr/libexec || exit 1
        mkdir -p /usr/libexec/yuneta || exit 1
        mount --bind '$WORK/libexec' /usr/libexec/yuneta || exit 1
    fi
    for f in sudo systemd-run systemctl journalctl; do
        mount --bind '$WORK/'\$f /usr/bin/\$f || exit 1
    done
    mkdir -p /yuneta/agent/certs /yuneta/realms/agent /yuneta/store /yuneta/bin
    cp '$WORK/cc.crt' /yuneta/agent/certs/controlcenter.crt
    cp '$WORK/agent.crt' /yuneta/agent/certs/yuneta_agent.crt
    cp '$WORK/agent.key' /yuneta/agent/certs/yuneta_agent.key
    cp '$WORK/yuneta_agent22' /yuneta/agent/yuneta_agent22
    cp '$WORK/yuneta_agent22.json' /yuneta/agent/yuneta_agent22.json
    cd /yuneta/agent
    ulimit -c 0
    python3 -I '$WORK/control_center.py' '$WORK/cc.crt' '$WORK/cc.key' 19899 \
        'install-package url=$URL sha256=$SHA' \
        'SH ls -A /yuneta/agent/install-package 2>/dev/null | sed \"s/^/NOPROGRAM FILE /\"' \
        'SH cp $WORK/install-package /usr/libexec/yuneta/install-package' \
        'install-package url=$URL sha256=$SHA' \
        'SH ls -A /yuneta/agent/install-package | sed \"s/^/DATA FILE /\"' \
        'SH sed \"s/^/UNIT ARGS /\" /yuneta/systemd-run.log' \
        'SH sed \"s/^/STATE URL /\" /var/lib/yuneta/install-package/url' \
        'SH stat -c \"STATE MODE %a %u\" /var/lib/yuneta/install-package' \
        'install-package url=http://example.com/$PKG sha256=$SHA' \
        'install-package url=https://example.com/[1-1000000000]/$PKG sha256=$SHA' \
        'install-package-status' \
        'SH echo \"done 0\" > /var/lib/yuneta/install-package/result' \
        'install-package-status' \
        'SH rm -f /var/lib/yuneta/install-package/result; echo \"done 7\" > /yuneta/agent/install-package/result' \
        'install-package-status' \
        'help' > '$WORK/cc.out' 2>&1 &
    CC=\$!
    sleep 0.5
    (ulimit -v 4000000; exec ./yuneta_agent22 --config-file=/yuneta/agent/yuneta_agent22.json) > '$WORK/agent.log' 2>&1 &
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
fail() {
    echo "TEST FAILED: $1"
    failed=1
}
answers() {    # the n-th answer of a line: result
    grep "^LINE '$1': " "$WORK/cc.out" | sed -n "${2}p"
}
comments() {
    grep "^COMMENT '$1': " "$WORK/cc.out" | sed -n "${2}p"
}
L_INSTALL="install-package url=$URL sha256=$SHA"
L_STATUS="install-package-status"

grep -q "^control center exit code 0$" "$WORK/cc.out" || fail "the link to the control center did not live through the lines"
#   1. no program
answers "$L_INSTALL" 1 | grep -q '"EV_MT_COMMAND_ANSWER", -1' ||
    fail "install-package with no /usr/libexec/yuneta/install-package was not refused"
comments "$L_INSTALL" 1 | grep -q 'has no /usr/libexec/yuneta/install-package: nothing done' ||
    fail "the refusal does not say the package has no /usr/libexec/yuneta/install-package"
grep -q "^NOPROGRAM FILE " "$WORK/cc.out" && fail "with no program, agent22 still wrote files in /yuneta/agent/install-package"
#   2. with the program
answers "$L_INSTALL" 2 | grep -q '"EV_MT_COMMAND_ANSWER", 0' ||
    fail "install-package with the program did not start"
comments "$L_INSTALL" 2 | grep -qE "installing $PKG in the unit yuneta-install-package-[0-9]{14}\. " ||
    fail "the answer does not name the file and the unit"
grep -qE "^UNIT ARGS --unit=yuneta-install-package-[0-9]{14} .* /usr/libexec/yuneta/install-package run$" "$WORK/cc.out" ||
    fail "the unit does not run '/usr/libexec/yuneta/install-package run'"
grep "^UNIT ARGS " "$WORK/cc.out" | grep -q "github" && fail "the url is on the command line of systemd-run"
grep -qx "STATE URL $URL" "$WORK/cc.out" || fail "the url is not in /var/lib/yuneta/install-package/url"
grep -qx "STATE MODE 755 0" "$WORK/cc.out" || fail "/var/lib/yuneta/install-package is not 0755 and root's"
[ "$(grep -c '^DATA FILE ' "$WORK/cc.out")" = "2" ] && grep -qx "DATA FILE url" "$WORK/cc.out" &&
    grep -qx "DATA FILE sha256" "$WORK/cc.out" ||
    fail "agent22 must write only url and sha256 in /yuneta/agent/install-package"
#   3. an http url
answers "install-package url=http://example.com/$PKG sha256=$SHA" 1 | grep -q '"EV_MT_COMMAND_ANSWER", -1' ||
    fail "an http url was not refused"
answers "install-package url=https://example.com/\[1-1000000000\]/$PKG sha256=$SHA" 1 | grep -q '"EV_MT_COMMAND_ANSWER", -1' ||
    fail "a url with a curl glob ([1-1000000000]) was not refused"
comments "install-package url=https://example.com/\[1-1000000000\]/$PKG sha256=$SHA" 1 | grep -q 'url must be https://' ||
    fail "a url with a curl glob was not refused by agent22 itself"
#   4. install-package-status
comments "$L_STATUS" 1 | grep -q 'install-package: no result (running, or none ran)' ||
    fail "install-package-status while it runs: not 'no result'"
comments "$L_STATUS" 2 | grep -q 'install-package: done 0$' ||
    fail "install-package-status does not read /var/lib/yuneta/install-package/result"
comments "$L_STATUS" 3 | grep -q 'install-package: done 7$' ||
    fail "install-package-status does not read the result an older agent22 left"
grep -q "^LINE 'help': \[\[\"EV_MT_COMMAND_ANSWER\", 0, null\]\]$" "$WORK/cc.out" || fail "the help at the end was not answered"
grep -q "^agent exit code 0$" "$WORK/cc.out" || fail "the agent did not end in order (it died?)"
grep -aq "system memory not free" "$WORK/agent.log" && fail "the agent ended with memory not free"
if [ $failed -ne 0 ]; then
    echo "---- agent log (errors and warnings) ----"
    grep -aE "ERROR|WARNING" "$WORK/agent.log" | cut -c1-600 | tail -40
    exit 1
fi
echo "TEST PASSED: install-package is refused on a package with no root program, and with it agent22 writes only data and the program starts the unit; install-package-status reads root's result and an older one"
exit 0
