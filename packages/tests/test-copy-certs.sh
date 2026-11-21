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
    mkdir -p "$LE/live" "${ROOT}/yuneta/bin"
}

cc() {
    env YUNETA_COPY_CERTS_TEST_ROOT="$ROOT" YUNETA_COPY_CERTS_TEST_SETPRIV="${STUBS}/setpriv" \
        "$COPY" 2>&1
}

as_me_count() {    # the setpriv calls that drop to the owner and run $1
    grep "^setpriv --reuid=${ME} --regid=${ME} --init-groups -- env HOME=" "$LOG" | grep -c -- "$1"
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
check "discover: every file installed by the owner" "$(as_me_count 'install -C')" "6"
check "discover: check-certs-validity run by the owner" "$(as_me_count 'check-certs-validity.sh')" "1"
check "discover: check-certs-validity ran" "$(grep -c "^validity ${CERTS}$" "$LOG")" "1"

#
#   An unchanged certificate keeps its mtime (the agent's cert-sync
#   reloads the yunos on a change of size or mtime); a changed one is
#   written
#
touch -d '2026-01-01 00:00:00' "${CERTS}/example.com.crt" "${CERTS}/private/example.com.key"
before="$(stat -c %Y "${CERTS}/example.com.crt") $(stat -c %Y "${CERTS}/private/example.com.key")"
cc >/dev/null
check "unchanged: mtime kept" "$(stat -c %Y "${CERTS}/example.com.crt") $(stat -c %Y "${CERTS}/private/example.com.key")" "$before"
printf 'renewed fullchain\n' > "${LE}/archive/example.com/fullchain1.pem"
cc >/dev/null
check "changed: written" "$(cat "${CERTS}/example.com.crt")" "renewed fullchain"
check "changed: mtime moved" "$( [ "$(stat -c %Y "${CERTS}/example.com.crt")" != "${before%% *}" ] && echo moved)" "moved"

#
#   certs.list: a bad name is skipped with a line, the others go on
#
reset
add_cert example.com "first"
mkdir -p "$CERTS"
printf '%s\n' "# a comment" "" "../../../etc" ".hidden" "a/b" "x..y" "bad name" "example.com" > "${CERTS}/certs.list"
out="$(cc)"
check "certs.list: exit 0" "$?" "0"
check "certs.list: the good name copied" "$(cat "${CERTS}/example.com.crt")" "first fullchain"
check "certs.list: each bad name said" "$(echo "$out" | grep -c 'not a valid name')" "5"
check "certs.list: nothing else written" "$(ls "$CERTS" | grep -vc -e '^example.com' -e '^private$' -e '^certs.list$')" "0"
check "certs.list: read by the owner" "$(as_me_count "cat ${CERTS}/certs.list")" "1"

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

if [ "$FAILED" -ne 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "PASSED"
