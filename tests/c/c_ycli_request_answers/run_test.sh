#!/bin/bash
#
#   ycli and the answers of the agent's own requests (read-file,
#   read-binary-file, edit-config...): a peer written in python
#   (fake_agent.py) sends ycli some it did not ask, and answers the ones it
#   asks. ycli must act on the asked ones only. Up to 7.30.26 it wrote the
#   content of any in ~/.yuneta/configs/.
#
#   ycli reads a terminal, so the peer runs it in a pty of its own, without
#   ncurses (`ycli.use_ncurses` 0), and types its lines. The peer listens on
#   127.0.0.1:1991, so it runs in namespaces of its own (user, mount,
#   network): a loopback nobody else sees, and a passwd where uid 0 is
#   `yuneta`, with HOME in a work dir.
#
#   Usage: run_test.sh <ycli binary>
#   Exit 77 (skipped) when this machine cannot run it, saying why.
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#

#
#   What it needs
#
HERE=$(dirname "$(readlink -f "$0")")
YCLI="$1"
skip() {
    echo "SKIPPED: $1"
    exit 77
}
[ -x "$YCLI" ] || skip "no ycli binary at '$YCLI' (built by yunetas build)"
command -v ip >/dev/null || skip "no 'ip' command to bring up the loopback of the sandbox"
unshare -rmn true 2>/dev/null || skip "unprivileged namespaces not allowed here"

#
#   Sandbox
#
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cp "$HERE/fake_agent.py" "$WORK/"
printf 'yuneta:x:0:0:yuneta:/yuneta:/bin/bash\n' > "$WORK/passwd"
printf 'yuneta:x:0:yuneta\n' > "$WORK/group"

timeout 100 unshare -rmn bash -c "
    ip link set lo up || exit 1
    mount --bind '$WORK/passwd' /etc/passwd || exit 1
    mount --bind '$WORK/group' /etc/group || exit 1
    python3 -u -I '$WORK/fake_agent.py' '$YCLI' '$WORK'
"
rc=$?
if [ $rc -ne 0 ]; then
    echo "TEST FAILED: ycli acted on an answer it did not ask, or not on one it asked (exit $rc)"
    exit 1
fi
echo "TEST PASSED: ycli acted only on the answers of the requests it made"
exit 0
