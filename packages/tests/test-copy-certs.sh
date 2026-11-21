#!/bin/bash
#
#   Test of packages/templates/copy-certs.sh and of the certbot deploy hook
#   packages/templates/reload-certs, in a fake root, with a stub of setpriv
#   that writes down the user it is asked to run as. Runs as a NORMAL user,
#   never as root: the scripts take their test root only from a user that
#   is not root.
#
#   Copyright (c) 2026, ArtGins.
#
set -u

if [ "$(id -u)" = "0" ]; then
    echo "FAIL - run it as a normal user, not as root" >&2
    exit 1
fi

HERE="$(cd "$(dirname "$0")" && pwd)"
COPY="${HERE}/../templates/copy-certs.sh"
HOOK="${HERE}/../templates/reload-certs"
if ! WORK="$(mktemp -d)" || [ -z "$WORK" ] || [ ! -d "$WORK" ]; then
    echo "FAIL - cannot create a work directory" >&2
    exit 1
fi
trap 'rm -rf "$WORK"' EXIT

ROOT="${WORK}/root"
LE="${ROOT}/etc/letsencrypt"
CERTS="${ROOT}/yuneta/store/certs"
LOG="${WORK}/calls.log"
STUBS="${WORK}/stubs"
ME="$(id -un)"
FAILED=0

check() {
    local what="$1" got="$2" expected="$3"
    if [ -z "$got" ] || [ -z "$expected" ]; then
        echo "FAIL - $what: got '$got', expected '$expected' (an empty value proves nothing)"
        FAILED=1
    elif [ "$got" = "$expected" ]; then
        echo "ok   - $what"
    else
        echo "FAIL - $what: got '$got', expected '$expected'"
        FAILED=1
    fi
}

#
#   setpriv: writes down its arguments and runs the command after "--"
#
mkdir -p "$STUBS"
cat > "${STUBS}/setpriv" <<EOF
#!/bin/sh
echo "setpriv \$*" >> "$LOG"
while [ \$# -gt 0 ] && [ "\$1" != "--" ]; do
    shift
done
shift
exec "\$@"
EOF
chmod 0755 "${STUBS}/setpriv"

add_cert() {    # name content
    mkdir -p "${LE}/archive/$1" "${LE}/live/$1"
    for f in fullchain chain privkey; do
        printf '%s %s\n' "$2" "$f" > "${LE}/archive/$1/${f}1.pem"
        ln -sfn "../../archive/$1/${f}1.pem" "${LE}/live/$1/${f}.pem"
    done
}

reset() {
    rm -rf "$ROOT" "$LOG"
    : > "$LOG"
    mkdir -p "$LE/live" "${ROOT}/yuneta/bin" "${ROOT}/run"
}

cc() {
    env YUNETA_COPY_CERTS_TEST_ROOT="$ROOT" YUNETA_COPY_CERTS_TEST_SETPRIV="${STUBS}/setpriv" \
        "$COPY" 2>&1
}

as_me_count() {    # the setpriv calls that drop to the owner, with its environment, and run $1
    grep "^setpriv --reuid=${ME} --regid=${ME} --init-groups --reset-env -- " "$LOG" | grep -c -- "$1"
}

owner_parts() {    # the calls of the owner's part (one per certificate), since the log was emptied
    grep "^setpriv " "$LOG" | grep -c -- "-- bash -c"
}

mtimes() {    # the mtimes of the three files of a certificate
    stat -c %Y "${CERTS}/$1.crt" "${CERTS}/$1.chain" "${CERTS}/private/$1.key" | paste -sd' ' -
}

#
#   Not root and no test root: refused
#
out="$(env -u YUNETA_COPY_CERTS_TEST_ROOT "$COPY" 2>&1)"
check "not root: refused" "$(echo "$out" | grep -c 'must run as root')" "1"

#
#   Auto-discovery: every live certificate, written by the owner
#
reset
add_cert example.com "first"
add_cert other.org "second"
printf '#!/bin/sh\necho "validity $*" >> "%s"\n' "$LOG" > "${ROOT}/yuneta/bin/check-certs-validity.sh"
chmod 0755 "${ROOT}/yuneta/bin/check-certs-validity.sh"
out="$(cc)"
check "discover: exit 0" "$?" "0"
check "discover: the crt" "$(cat "${CERTS}/example.com.crt")" "first fullchain"
check "discover: the chain" "$(cat "${CERTS}/example.com.chain")" "first chain"
check "discover: the key" "$(cat "${CERTS}/private/example.com.key")" "first privkey"
check "discover: the other one" "$(cat "${CERTS}/other.org.crt")" "second fullchain"
check "discover: crt 0644" "$(stat -c %a "${CERTS}/example.com.crt")" "644"
check "discover: key 0600" "$(stat -c %a "${CERTS}/private/example.com.key")" "600"
check "discover: private/ 0700" "$(stat -c %a "${CERTS}/private")" "700"
check "discover: the directories made by the owner" "$(as_me_count 'install -d')" "2"
check "discover: one call of the owner per certificate" "$(owner_parts)" "2"
check "discover: said for each" "$(echo "$out" | grep -c '^Copied ')" "2"
check "discover: no temp file left" "$(find "$CERTS" -name '*.??????' | wc -l)" "0"
check "discover: check-certs-validity run by the owner" "$(as_me_count 'check-certs-validity.sh')" "1"
check "discover: check-certs-validity ran" "$(grep -c "^validity ${CERTS}$" "$LOG")" "1"

#
#   An unchanged certificate keeps its mtime (the agent's cert-sync
#   reloads the yunos on a change of size or mtime); a changed one is
#   written
#
touch -d '2026-01-01 00:00:00' "${CERTS}"/*.crt "${CERTS}"/*.chain "${CERTS}"/private/*.key
touch -d '2026-01-02 00:00:00' "${WORK}/mark"
before="$(stat -c %Y "${CERTS}/example.com.crt") $(stat -c %Y "${CERTS}/private/example.com.key")"
: > "$LOG"
out="$(cc)"
check "unchanged: exit 0" "$?" "0"
check "unchanged: mtime kept" "$(stat -c %Y "${CERTS}/example.com.crt") $(stat -c %Y "${CERTS}/private/example.com.key")" "$before"
check "unchanged: nothing written" "$(find "$CERTS" -newer "${WORK}/mark" -type f | wc -l)" "0"
check "unchanged: no Copied" "$(echo "$out" | grep -c '^Copied')" "0"
check "unchanged: check-certs-validity not run" "$(grep -c '^validity' "$LOG")" "0"
chmod 0640 "${CERTS}/example.com.crt"
cc >/dev/null
check "unchanged, another mode: the mode set again" "$(stat -c %a "${CERTS}/example.com.crt")" "644"
check "unchanged, another mode: mtime kept" "$(stat -c %Y "${CERTS}/example.com.crt")" "${before%% *}"
printf 'renewed fullchain\n' > "${LE}/archive/example.com/fullchain1.pem"
: > "$LOG"
out="$(cc)"
check "changed: written" "$(cat "${CERTS}/example.com.crt")" "renewed fullchain"
check "changed: only that file" "$(find "$CERTS" -newer "${WORK}/mark" -type f | sed "s|${CERTS}/||" | paste -sd' ' -)" "example.com.crt"
check "changed: Copied only that certificate" "$(echo "$out" | grep '^Copied' | grep -c 'example.com')$(echo "$out" | grep -c '^Copied')" "11"
check "changed: check-certs-validity run" "$(grep -c '^validity' "$LOG")" "1"
check "changed: mtime moved" "$( [ "$(stat -c %Y "${CERTS}/example.com.crt")" != "${before%% *}" ] && echo moved)" "moved"

#
#   certs.list: a bad name is skipped with a line, the others go on
#
reset
add_cert example.com "first"
mkdir -p "$CERTS"
add_cert b.org "bee"
add_cert c.org "cee"
printf '%s\n' "# a comment" "" "   # an indented comment" "../../../etc" ".hidden" "a/b" "x..y" "bad*name" \
    "  example.com	 " "b.org c.org" > "${CERTS}/certs.list"
out="$(cc)"
check "certs.list: exit 0" "$?" "0"
check "certs.list: the good name copied" "$(cat "${CERTS}/example.com.crt")" "first fullchain"
check "certs.list: each bad name said" "$(echo "$out" | grep -c 'not a valid name')" "5"
check "certs.list: a name with blanks around it" "$(echo "$out" | grep -c '^Copied example.com ')" "1"
check "certs.list: two names on a line" "$(cat "${CERTS}/b.org.crt") $(cat "${CERTS}/c.org.crt")" "bee fullchain cee fullchain"
check "certs.list: nothing else written" "$(ls "$CERTS" | grep -vc -e '^example.com' -e '^b.org' -e '^c.org' -e '^private$' -e '^certs.list$')" "0"
check "certs.list: read by the owner" "$(as_me_count "cat ${CERTS}/certs.list")" "1"

#
#   A certs.list that the owner cannot read: exit 1 and nothing copied (up
#   to the round before, the cat failed inside a process substitution and
#   the script said nothing and exited 0)
#
reset
add_cert example.com "first"
mkdir -p "$CERTS"
printf 'example.com\n' > "${CERTS}/certs.list"
chmod 0000 "${CERTS}/certs.list"
out="$(cc)"
rc=$?
chmod 0644 "${CERTS}/certs.list"
check "certs.list unreadable: exit 1" "$rc" "1"
check "certs.list unreadable: said" "$(echo "$out" | grep -c "FAILED: ${ME} cannot read ${CERTS}/certs.list")" "1"
check "certs.list unreadable: nothing copied" "$( [ -e "${CERTS}/example.com.crt" ] && echo copied || echo no)" "no"

#
#   No key goes through TMPDIR: a TMPDIR nobody can enter changes nothing
#
reset
add_cert example.com "first"
mkdir -p "${WORK}/notmp"
chmod 0000 "${WORK}/notmp"
out="$(TMPDIR="${WORK}/notmp" cc)"
rc=$?
chmod 0755 "${WORK}/notmp"
check "TMPDIR not enterable: exit 0" "$rc" "0"
check "TMPDIR not enterable: copied" "$(cat "${CERTS}/private/example.com.key")" "first privkey"

#
#   A copy that fails: said, exit 1, and the set of that certificate stays
#   whole (the new key is not put beside the old crt); the others done
#
reset
add_cert example.com "first"
add_cert other.org "second"
cc >/dev/null
printf 'renewed\n' > "${LE}/archive/example.com/fullchain1.pem"
printf 'renewed\n' > "${LE}/archive/example.com/privkey1.pem"
printf 'renewed\n' > "${LE}/archive/other.org/privkey1.pem"
chmod 0555 "$CERTS"
out="$(cc)"
rc=$?
chmod 0755 "$CERTS"
check "a write that fails: exit 1" "$rc" "1"
check "a write that fails: said" "$(echo "$out" | grep -c "FAILED to copy example.com (rc=2): .*${CERTS}/example.com.crt")" "1"
check "a write that fails: the old key stays with the old crt" "$(cat "${CERTS}/private/example.com.key") / $(cat "${CERTS}/example.com.crt")" "first privkey / first fullchain"
check "a write that fails: no temporary file left" "$(find "$CERTS" -name '*.??????' | wc -l)" "0"
check "a write that fails: the other certificate done" "$(cat "${CERTS}/private/other.org.key")" "renewed"

#
#   A move that fails after others were done: the files already moved are
#   put back (the old set whole), and no temporary or .old file stays
#
REAL_MV="$(command -v mv)"
reset
add_cert example.com "first"
cc >/dev/null
for f in fullchain chain privkey; do
    printf 'renewed %s\n' "$f" > "${LE}/archive/example.com/${f}1.pem"
done
printf '#!/bin/sh\nfor a in "$@"; do\n    case "$a" in\n        *.crt)\n            echo "mv: cannot move" >&2\n            exit 1\n            ;;\n    esac\ndone\nexec "%s" "$@"\n' "$REAL_MV" > "${STUBS}/mv"
chmod 0755 "${STUBS}/mv"
out="$(PATH="${STUBS}:$PATH" cc)"
rc=$?
check "a move that fails: exit 1" "$rc" "1"
check "a move that fails: said" "$(echo "$out" | grep -c 'FAILED to move .*example.com.crt: the old files put back')" "1"
check "a move that fails: the old set whole" "$(cat "${CERTS}/private/example.com.key") / $(cat "${CERTS}/example.com.chain") / $(cat "${CERTS}/example.com.crt")" "first privkey / first chain / first fullchain"
check "a move that fails: no temporary or .old file left" "$(find "$CERTS" -name '*.??????' | wc -l)" "0"

# ... and when the old files cannot be put back either: said apart
printf '#!/bin/sh\nfor a in "$@"; do\n    case "$a" in\n        *.crt|*.key.old.*)\n            exit 1\n            ;;\n    esac\ndone\nexec "%s" "$@"\n' "$REAL_MV" > "${STUBS}/mv"
out="$(PATH="${STUBS}:$PATH" cc)"
check "a move that fails, and its undo: rc 3" "$(echo "$out" | grep -c 'FAILED to copy example.com (rc=3)')" "1"
check "a move that fails, and its undo: said" "$(echo "$out" | grep -c 'may not match')" "1"
check "a move that fails, and its undo: the old key kept, and named" \
    "$(cat "$(echo "$out" | sed -n 's/.*it is kept in \([^ ]*\)$/\1/p')")" "first privkey"
rm -f "${STUBS}/mv"

#
#   One run at a time: two runs at once (the hook and the tick) never
#   interleave their parts. A slow setpriv writes down when each part of
#   the owner begins and ends
#
reset
add_cert example.com "first"
add_cert other.org "second"
cat > "${STUBS}/setpriv-slow" <<EOF
#!/bin/sh
while [ \$# -gt 0 ] && [ "\$1" != "--" ]; do
    shift
done
shift
echo "begin" >> "${WORK}/order.log"
sleep 0.2
"\$@"
rc=\$?
echo "end" >> "${WORK}/order.log"
exit \$rc
EOF
chmod 0755 "${STUBS}/setpriv-slow"
: > "${WORK}/order.log"
slow_cc() {
    env YUNETA_COPY_CERTS_TEST_ROOT="$ROOT" YUNETA_COPY_CERTS_TEST_SETPRIV="${STUBS}/setpriv-slow" "$COPY" >/dev/null 2>&1
}
slow_cc &
p1=$!
slow_cc &
p2=$!
wait "$p1"
r1=$?
wait "$p2"
r2=$?
check "two runs at once: both end well" "$r1 $r2" "0 0"
check "two runs at once: no part begins inside another" \
    "$(awk '$1 == "begin" {if (open) bad = 1; open = 1} $1 == "end" {open = 0} END {print bad ? "interleaved" : "one at a time"}' "${WORK}/order.log")" "one at a time"
check "two runs at once: parts of both ran" "$(grep -c '^begin' "${WORK}/order.log")" "$(grep -c '^end' "${WORK}/order.log")"
check "the lock is under /run" "$( [ -e "${ROOT}/run/yuneta-copy-certs.lock" ] && echo yes)" "yes"

#
#   A node with no certificate: said to the journal once, not at each tick,
#   and again after one came and went
#
reset
printf '#!/bin/sh\necho "logger $*" >> "%s"\n' "$LOG" > "${STUBS}/logger"
chmod 0755 "${STUBS}/logger"
for i in 1 2 3; do
    YUNETA_COPY_CERTS_TEST_LOGGER="${STUBS}/logger" cc >/dev/null
done
check "no certificate: said once in three runs" "$(grep -c 'No certificates to copy' "$LOG")" "1"
add_cert example.com "first"
YUNETA_COPY_CERTS_TEST_LOGGER="${STUBS}/logger" cc >/dev/null
check "no certificate: the mark goes when there is one" "$( [ -e "${CERTS}/.no-certificates" ] && echo there || echo gone)" "gone"
rm -rf "${LE}/live/example.com"
YUNETA_COPY_CERTS_TEST_LOGGER="${STUBS}/logger" cc >/dev/null
check "no certificate again: said again" "$(grep -c 'No certificates to copy' "$LOG")" "2"

#
#   The order of the moves: the key first, the crt last (the crt is what
#   the agent's cert-sync watches)
#
reset
add_cert example.com "first"
cc >/dev/null
for f in fullchain chain privkey; do
    printf 'renewed %s\n' "$f" > "${LE}/archive/example.com/${f}1.pem"
done
touch -d '2026-01-01 00:00:00' "${CERTS}/example.com.crt" "${CERTS}/example.com.chain" "${CERTS}/private/example.com.key"
cc >/dev/null
k="$(stat -c %.9Z "${CERTS}/private/example.com.key")"
c="$(stat -c %.9Z "${CERTS}/example.com.crt")"
check "moves: the crt not before the key" "$(awk -v k="$k" -v c="$c" 'BEGIN {print (c >= k)? "yes" : "no"}')" "yes"
check "moves: all three new" "$(cat "${CERTS}/example.com.crt") / $(cat "${CERTS}/private/example.com.key")" "renewed fullchain / renewed privkey"

#
#   What it says also goes to the journal (logger -t copy-certs): the
#   agent's cert-sync runs it with its output on /dev/null
#
reset
add_cert example.com "first"
mkdir -p "$CERTS"
printf '%s\n' "../x" "example.com" > "${CERTS}/certs.list"
printf '#!/bin/sh\necho "logger $*" >> "%s"\n' "$LOG" > "${STUBS}/logger"
chmod 0755 "${STUBS}/logger"
YUNETA_COPY_CERTS_TEST_LOGGER="${STUBS}/logger" cc >/dev/null
check "journal: the Skip" "$(grep -c "^logger -t copy-certs -p user.warning -- Skip '../x'" "$LOG")" "1"
check "journal: the Copied" "$(grep -c '^logger -t copy-certs -p user.info -- Copied example.com' "$LOG")" "1"
chmod 0000 "${CERTS}/certs.list"
YUNETA_COPY_CERTS_TEST_LOGGER="${STUBS}/logger" cc >/dev/null
chmod 0644 "${CERTS}/certs.list"
check "journal: the FAILED" "$(grep -c '^logger -t copy-certs -p user.err -- FAILED: .* cannot read' "$LOG")" "1"

#
#   A pem that resolves out of /etc/letsencrypt: skipped
#
reset
mkdir -p "${ROOT}/secret" "${LE}/live/evil.com"
printf 'SECRET\n' > "${ROOT}/secret/key"
for f in fullchain chain; do
    printf 'x\n' > "${LE}/live/evil.com/${f}.pem"
done
ln -s "${ROOT}/secret/key" "${LE}/live/evil.com/privkey.pem"
add_cert good.com "good"
out="$(cc)"
check "out of letsencrypt: said" "$(echo "$out" | grep -c 'Skip evil.com: .*not under')" "1"
check "out of letsencrypt: not copied" "$( [ -e "${CERTS}/private/evil.com.key" ] && echo copied || echo no)" "no"
check "out of letsencrypt: the others go on" "$(cat "${CERTS}/good.com.crt")" "good fullchain"

#
#   A symlinked private/ (or certs/): refused, nothing written there
#
reset
add_cert example.com "first"
mkdir -p "$CERTS" "${WORK}/elsewhere"
ln -s "${WORK}/elsewhere" "${CERTS}/private"
out="$(cc)"
rc=$?
check "private/ is a link: exit 1" "$rc" "1"
check "private/ is a link: said" "$(echo "$out" | grep -c "Refused: ${CERTS}/private is a symbolic link")" "1"
check "private/ is a link: nothing written there" "$(ls -A "${WORK}/elsewhere" | wc -l)" "0"
rm -rf "${WORK}/elsewhere"

reset
add_cert example.com "first"
mkdir -p "${ROOT}/yuneta/store" "${WORK}/elsewhere"
ln -s "${WORK}/elsewhere" "$CERTS"
out="$(cc)"
check "certs/ is a link: exit 1" "$?" "1"
check "certs/ is a link: nothing written there" "$(ls -A "${WORK}/elsewhere" | wc -l)" "0"
rm -rf "${WORK}/elsewhere"

#
#   A destination file that is a link: that certificate is skipped
#
reset
add_cert example.com "first"
mkdir -p "${CERTS}/private"
printf 'precious\n' > "${WORK}/target"
ln -s "${WORK}/target" "${CERTS}/private/example.com.key"
out="$(cc)"
check "a key that is a link: said" "$(echo "$out" | grep -c 'Skip example.com: .*is a symbolic link')" "1"
check "a key that is a link: its target untouched" "$(cat "${WORK}/target")" "precious"

#
#   The hook: copy-certs, the web server, ycommand as the owner, and
#   restart-yuneta as the owner when ycommand fails
#
hook() {
    env YUNETA_RELOAD_CERTS_TEST_ROOT="$ROOT" YUNETA_RELOAD_CERTS_TEST_SETPRIV="${STUBS}/setpriv" \
        "$HOOK" 2>&1
}
out="$(env -u YUNETA_RELOAD_CERTS_TEST_ROOT "$HOOK" 2>&1)"
check "hook, not root: refused" "$(echo "$out" | grep -c 'as root')" "1"

reset
mkdir -p "${ROOT}/yuneta/store/certs" "${ROOT}/yuneta/bin/nginx/sbin"
for f in yuneta/store/certs/copy-certs.sh yuneta/bin/nginx/sbin/nginx yuneta/bin/restart-yuneta; do
    printf '#!/bin/sh\necho "%s $*" >> "%s"\n' "$(basename "$f")" "$LOG" > "${ROOT}/$f"
    chmod 0755 "${ROOT}/$f"
done
printf '#!/bin/sh\necho "ycommand $*" >> "%s"\nexit 1\n' "$LOG" > "${ROOT}/yuneta/bin/ycommand"
chmod 0755 "${ROOT}/yuneta/bin/ycommand"
hook >/dev/null
for i in $(seq 1 50); do
    if grep -q '^restart-yuneta' "$LOG"; then
        break
    fi
    sleep 0.1
done
check "hook: copy-certs.sh ran" "$(grep -c '^copy-certs.sh' "$LOG")" "1"
check "hook: nginx reloaded" "$(grep -c '^nginx -s reload' "$LOG")" "1"
check "hook: ycommand as the owner" "$(as_me_count 'ycommand')" "1"
check "hook: restart-yuneta ran" "$(grep -c '^restart-yuneta' "$LOG")" "1"
check "hook: restart-yuneta as the owner" "$(as_me_count 'restart-yuneta')" "1"
check "hook: copy-certs.sh not through setpriv (it reads root's keys)" "$(grep '^setpriv' "$LOG" | grep -c 'copy-certs')" "0"
check "hook: last run stamped" "$( [ -s "${ROOT}/var/lib/yuneta/last-deploy-hook-run" ] && echo yes)" "yes"
check "hook: copy-certs.sh OK said" "$(grep -c 'copy-certs.sh OK' "${ROOT}/var/log/yuneta/deploy-hook.log")" "1"

#
#   With systemd: restart-yuneta goes to a transient unit of its own, as
#   the owner (the background job would die with the oneshot unit of
#   certbot). copy-certs.sh that fails: FAILED. A stamp that cannot be
#   written: said
#
printf '#!/bin/sh\necho "copy-certs.sh $*" >> "%s"\nexit 1\n' "$LOG" > "${ROOT}/yuneta/store/certs/copy-certs.sh"
printf '#!/bin/sh\necho "systemd-run $*" >> "%s"\n' "$LOG" > "${STUBS}/systemd-run"
chmod 0755 "${STUBS}/systemd-run"
mkdir -p "${ROOT}/run/systemd/system"
rm -f "${ROOT}/var/lib/yuneta/last-deploy-hook-run"
chmod 0555 "${ROOT}/var/lib/yuneta"
: > "$LOG"
PATH="${STUBS}:$PATH" hook >/dev/null
chmod 0755 "${ROOT}/var/lib/yuneta"
HLOG="${ROOT}/var/log/yuneta/deploy-hook.log"
check "hook, copy-certs fails: FAILED said" "$(grep -c 'copy-certs.sh FAILED (rc=1)' "$HLOG")" "1"
check "hook, systemd: restart-yuneta in a unit of Type=exec, as the owner" \
    "$(grep -cE "^systemd-run -p Type=exec --unit=yuneta-restart-[0-9]{14} --uid=${ME} --gid=${ME} .* ${ROOT}/yuneta/bin/restart-yuneta\$" "$LOG")" "1"
check "hook, systemd: it waits for the exec (no --no-block)" "$(grep '^systemd-run' "$LOG" | grep -c -- '--no-block')" "0"
check "hook, systemd: not in the background" "$(grep -c '^restart-yuneta' "$LOG")" "0"
check "hook, systemd: the unit said" "$(grep -c 'restart-yuneta started as .* in the unit yuneta-restart-' "$HLOG")" "1"
check "hook: a stamp that cannot be written is said" "$(grep -c 'cannot write .*last-deploy-hook-run' "$HLOG")" "1"

#
#   A unit that does not start (203/EXEC: SELinux on /yuneta): said, not
#   taken for a start, and nothing in the background
#
printf '#!/bin/sh\necho "systemd-run $*" >> "%s"\necho "Job for x.service failed" >&2\nexit 1\n' "$LOG" > "${STUBS}/systemd-run"
: > "$LOG"
: > "$HLOG"
PATH="${STUBS}:$PATH" hook >/dev/null
sleep 0.5
check "hook, the unit fails: said" "$(grep -c 'restart-yuneta did NOT start in the unit yuneta-restart-.*rc=1' "$HLOG")" "1"
check "hook, the unit fails: not 'started'" "$(grep -c 'restart-yuneta started' "$HLOG")" "0"
check "hook, the unit fails: nothing in the background" "$(grep -c '^restart-yuneta' "$LOG")" "0"
rm -rf "${ROOT}/run"

#
#   restart-yuneta (written by both packagers). Its paths, the unit files
#   it looks for, /run/systemd/system and /proc/self/cgroup are moved under
#   the test root. With a unit file of the agent: the agent restarted in
#   its unit (sudo -n systemctl), also from logcenter under the agent (its
#   INVOCATION_ID). Without one (a node before 7.25.22): from logcenter,
#   yshutdown and --start; from the transient unit of the certbot hook,
#   refused (an agent started there dies with the unit)
#
ry_run() {    # cgroup-line [env...]
    local cg="$1"
    shift
    printf '0::%s\n' "$cg" > "${WORK}/cgroup"
    env "$@" PATH="${STUBS}:$PATH" "$RY" 2>&1
}
for builder in deb/make-yuneta-agent-deb.sh rpm/make-yuneta-agent-rpm.sh; do
    reset
    RY="${WORK}/restart-yuneta"
    awk 'index($0, "yuneta/bin/restart-yuneta\" <<"){f=1; next} f && /^EOF$/{exit} f' \
        "${HERE}/../${builder}" | sed \
        -e "s|/yuneta/|${ROOT}/yuneta/|g" \
        -e "s|^SYSTEMD_RUN_DIR=.*|SYSTEMD_RUN_DIR=\"${ROOT}/run/systemd/system\"|" \
        -e "s|^AGENT_UNIT_FILES=.*|AGENT_UNIT_FILES=\"${ROOT}/usr/lib/systemd/system/yuneta_agent.service\"|" \
        -e "s|^CGROUP_FILE=.*|CGROUP_FILE=\"${WORK}/cgroup\"|" > "$RY"
    chmod 0755 "$RY"
    check "restart-yuneta ($builder): the test replaced its paths" \
        "$(grep -c -e "^SYSTEMD_RUN_DIR=\"${ROOT}" -e "^AGENT_UNIT_FILES=\"${ROOT}" -e "^CGROUP_FILE=\"${WORK}" "$RY")" "3"
    mkdir -p "${ROOT}/yuneta/agent" "${ROOT}/run/systemd/system"
    printf '#!/bin/sh\necho "yshutdown $*" >> "%s"\n' "$LOG" > "${ROOT}/yuneta/bin/yshutdown"
    printf '#!/bin/sh\necho "yuneta_agent $*" >> "%s"\n' "$LOG" > "${ROOT}/yuneta/agent/yuneta_agent"
    printf '#!/bin/sh\necho "sudo $*" >> "%s"\n' "$LOG" > "${STUBS}/sudo"
    printf '#!/bin/sh\necho "systemctl $*" >> "%s"\nexit 1\n' "$LOG" > "${STUBS}/systemctl"
    chmod 0755 "${ROOT}/yuneta/bin/yshutdown" "${ROOT}/yuneta/agent/yuneta_agent" "${STUBS}/sudo" "${STUBS}/systemctl"

    # With the unit file: restarted in the unit, also from logcenter
    mkdir -p "${ROOT}/usr/lib/systemd/system"
    touch "${ROOT}/usr/lib/systemd/system/yuneta_agent.service"
    out="$(ry_run /system.slice/yuneta_agent.service INVOCATION_ID=0123abcd)"
    check "restart-yuneta ($builder), units, logcenter: exit 0" "$?" "0"
    check "restart-yuneta ($builder), units, logcenter: restarted in the unit" "$(grep -c '^sudo -n systemctl restart yuneta_agent.service' "$LOG")" "1"
    check "restart-yuneta ($builder), units: no systemctl cat asked" "$(grep -c '^systemctl cat' "$LOG")" "0"
    check "restart-yuneta ($builder), units: no --start" "$(grep -c '^yuneta_agent --start' "$LOG")" "0"

    # Without it: from logcenter (under the agent), yshutdown and --start
    rm -f "${ROOT}/usr/lib/systemd/system/yuneta_agent.service"
    : > "$LOG"
    out="$(ry_run /system.slice/yuneta_agent.service INVOCATION_ID=0123abcd)"
    check "restart-yuneta ($builder), no units, logcenter: yshutdown" "$(grep -c '^yshutdown' "$LOG")" "1"
    check "restart-yuneta ($builder), no units, logcenter: the agent started" "$(grep -c '^yuneta_agent --start' "$LOG")" "1"

    # Without it, from the transient unit of the hook: refused
    : > "$LOG"
    out="$(ry_run /system.slice/yuneta-restart-20261121000000.service INVOCATION_ID=0123abcd)"
    rc=$?
    check "restart-yuneta ($builder), no units, the hook's unit: exit 1" "$rc" "1"
    check "restart-yuneta ($builder), no units, the hook's unit: said" "$(echo "$out" | grep -c 'transient unit of the certbot hook')" "1"
    check "restart-yuneta ($builder), no units, the hook's unit: no yunos stopped" "$(grep -c '^yshutdown' "$LOG")" "0"
    check "restart-yuneta ($builder), no units, the hook's unit: no agent started" "$(grep -c '^yuneta_agent' "$LOG")" "0"
    rm -f "${STUBS}/systemctl" "${STUBS}/sudo"
done

if [ "$FAILED" -ne 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "PASSED"
