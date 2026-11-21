#!/bin/bash
#
#   Test of packages/templates/install-package against a fake root, with
#   stubs for curl, systemd-run, systemctl, apt-get, dpkg-deb, dnf, rpm and
#   journalctl. Runs as a NORMAL user, never as root: the program takes its
#   test root only from a user that is not root.
#
#   Copyright (c) 2026, ArtGins.
#
set -u

if [ "$(id -u)" = "0" ]; then
    echo "FAIL - run it as a normal user, not as root" >&2
    exit 1
fi

HERE="$(cd "$(dirname "$0")" && pwd)"
PROGRAM="${HERE}/../templates/install-package"
if ! WORK="$(mktemp -d)" || [ -z "$WORK" ] || [ ! -d "$WORK" ]; then
    echo "FAIL - cannot create a work directory" >&2
    exit 1
fi
trap 'rm -rf "$WORK"' EXIT

ROOT="${WORK}/root"
DATA="${ROOT}/yuneta/agent/install-package"
STATE="${ROOT}/var/lib/yuneta/install-package"
STUBS="${WORK}/stubs"
TOOLS="${WORK}/tools"
LOG="${WORK}/calls.log"
FIXTURE="${WORK}/yuneta-agent-7.31.29-1-amd64.deb"
URL="https://github.com/artgins/yunetas/releases/download/7.31.29/yuneta-agent-7.31.29-1-amd64.deb"
FAILED=0

#
#   Helpers
#
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
#   The tools the program needs, and nothing else on its PATH: a stub that
#   is missing is a tool the node does not have
#
mkdir -p "$TOOLS"
for t in head stat mkdir rm mv chmod date awk tr id mktemp sha256sum tail cat dd cp sh; do
    ln -s "$(command -v "$t")" "${TOOLS}/$t"
done

stub() {    # name body
    printf '#!/bin/sh\necho "%s $*" >> "%s"\n%s\n' "$1" "$LOG" "$2" > "${STUBS}/$1"
    chmod 0755 "${STUBS}/$1"
}

reset() {
    rm -rf "$ROOT" "$STUBS" "$LOG"
    mkdir -p "$DATA" "$STUBS"
    : > "$LOG"
    printf 'the bytes of a package\n' > "$FIXTURE"
    stub systemctl ':'
    stub systemd-run ':'
    stub journalctl ':'
    stub dpkg-deb 'echo yuneta-agent'
    stub rpm 'printf yuneta-agent'
    stub apt-get ':'
    stub dnf ':'
    # curl: copy the fixture to the file of -o
    stub curl 'while [ $# -gt 0 ]; do if [ "$1" = "-o" ]; then cp "'"$FIXTURE"'" "$2"; exit $?; fi; shift; done; exit 2'
    printf '%s' "$URL" > "${DATA}/url"
    printf '%s' "$(sha256sum "$FIXTURE" | cut -c1-64)" > "${DATA}/sha256"
}

ip() {
    env -i PATH="${STUBS}:${TOOLS}" YUNETA_INSTALL_PACKAGE_TEST_ROOT="$ROOT" \
        ${MAX:+YUNETA_INSTALL_PACKAGE_TEST_MAX_BYTES="$MAX"} "$PROGRAM" "$@" 2>&1
}

calls() {    # the calls of one tool, one per line
    grep -c "^$1 " "$LOG"
}

#
#   Without root and without a test root: refused
#
out="$(env -i PATH="/usr/bin:/bin" "$PROGRAM" start 2>&1)"
check "not root: refused" "$(echo "$out" | grep -c '^refused: run it as root')" "1"

#
#   start: checks the data, copies it to the state dir of root, starts the
#   unit with no data on its command line
#
reset
out="$(ip start)"
rc=$?
check "start: exit 0" "$rc" "0"
check "start: says the unit and the package" \
    "$(echo "$out" | grep -cE '^started yuneta-install-package-[0-9]{14} yuneta-agent-7.31.29-1-amd64.deb$')" "1"
check "start: url copied" "$(cat "${STATE}/url")" "$URL"
check "start: sha256 copied" "$(cat "${STATE}/sha256")" "$(cat "${DATA}/sha256")"
check "start: state dir 0755" "$(stat -c %a "$STATE")" "755"
check "start: one systemd-run" "$(calls systemd-run)" "1"
check "start: the unit runs '<program> run'" "$(grep -c " ${PROGRAM} run\$" "$LOG")" "1"
check "start: no url on the command line of systemd-run" "$(grep '^systemd-run ' "$LOG" | grep -c 'github')" "0"
check "start: no result while it runs" "$( [ -e "${STATE}/result" ] && echo there || echo none)" "none"

#
#   start refuses the data that is not valid, and never shows it
#
start_refused() {    # what
    local out rc
    out="$(ip start)"
    rc=$?
    check "$1: exit 2" "$rc" "2"
    check "$1: refused" "$(echo "$out" | grep -c '^refused: ')" "1"
    check "$1: no unit" "$(calls systemd-run)" "0"
}

reset
printf 'SECRET-LINE https://x/y.deb\n' > "${WORK}/secret"
rm -f "${DATA}/url"
ln -s "${WORK}/secret" "${DATA}/url"
start_refused "url is a link"
check "url is a link: its target is not shown" "$(ip start | grep -c SECRET)" "0"

reset
rm -f "${DATA}/sha256"
ln -s "${WORK}/secret" "${DATA}/sha256"
start_refused "sha256 is a link"

reset
mkdir "${DATA}/url.d"
rm -f "${DATA}/url"
mv "${DATA}/url.d" "${DATA}/url"
start_refused "url is a directory"

reset
mv "$DATA" "${WORK}/elsewhere"
ln -s "${WORK}/elsewhere" "$DATA"
start_refused "the data dir is a link"

for bad in "http://example.com/yuneta-agent-1-amd64.deb" \
        "https://example.com/yuneta agent-1-amd64.deb" \
        "https://example.com/yuneta'agent-1-amd64.deb" \
        "https://example.com/yuneta\"agent-1-amd64.deb" \
        "https://example.com/yuneta\`agent-1-amd64.deb" \
        "https://example.com/yuneta\\agent-1-amd64.deb" \
        "$(printf 'https://example.com/yuneta\001agent-1-amd64.deb')" \
        "$(printf 'https://example.com/a.deb\nhttps://example.com/b.deb')" \
        "$(printf 'https://example.com/yuneta-\303\251-amd64.deb')" \
        "https://example.com/yuneta-agent-1-amd64.tar" \
        "https://example.com/.hidden.deb" \
        "https://example.com/a%24b.deb" \
        "https://example.com/[1-1000000000]/yuneta-agent-1-amd64.deb" \
        "https://example.com/{a,b}/yuneta-agent-1-amd64.deb" \
        "https://example.com/a]b/yuneta-agent-1-amd64.deb" \
        "https://example.com/a}b/yuneta-agent-1-amd64.deb" \
        "https://example.com/" \
        "https://$(head -c 2040 /dev/zero | tr '\0' 'a')/yuneta-agent-1-amd64.deb"; do
    reset
    printf '%s' "$bad" > "${DATA}/url"
    start_refused "bad url $(printf '%s' "$bad" | head -c 50 | tr -c '[:graph:]' '?')"
done

for bad in "" "abc" "$(head -c 64 /dev/zero | tr '\0' 'G')" \
        "$(head -c 64 /dev/zero | tr '\0' 'A')" "$(head -c 65 /dev/zero | tr '\0' 'a')"; do
    reset
    printf '%s' "$bad" > "${DATA}/sha256"
    start_refused "bad sha256 '$(printf '%s' "$bad" | head -c 10)'"
done

#
#   start refuses a node that lacks a tool, and a second install
#
reset
rm -f "${STUBS}/curl"
start_refused "no curl"
check "no curl: says it" "$(ip start | grep -c 'has no curl')" "1"

reset
rm -f "${STUBS}/dpkg-deb"
start_refused "a .deb with no dpkg-deb"

reset
printf '%s' "https://example.com/yuneta-agent-7.31.29-1.x86_64.rpm" > "${DATA}/url"
rm -f "${STUBS}/dnf"
start_refused "a .rpm with no dnf"

reset
stub systemctl 'echo "yuneta-install-package-20261121000000.service loaded active running yuneta-agent package"'
start_refused "another install runs"
check "another install runs: names it" \
    "$(ip start | grep -c 'another install-package runs: yuneta-install-package-20261121000000.service')" "1"

reset
stub systemctl 'echo "Failed to connect to bus: No such file or directory" >&2; exit 1'
start_refused "systemctl fails: fail closed"
check "systemctl fails: says why" \
    "$(ip start | grep -c 'systemctl list-units FAILED, so it cannot tell whether another install-package runs: Failed to connect to bus')" "1"

reset
mkdir -p -m 0775 "$STATE"
chmod 0775 "$STATE"
start_refused "state dir writable by the group"

reset
mkdir -p "${ROOT}/var/lib" "${WORK}/other-lib"
ln -s "${WORK}/other-lib" "${ROOT}/var/lib/yuneta"
start_refused "state parent is a link"

reset
check "start with an argument: refused" "$(ip start /tmp | grep -c '^refused: start takes no argument')" "1"
check "an unknown command: refused" "$(ip bogus | grep -c '^refused: usage')" "1"

#
#   run: download, sha256, package name, install
#
run_ok_setup() {
    reset
    ip start >/dev/null
    : > "$LOG"
}

run_ok_setup
out="$(ip run)"
rc=$?
check "run: exit 0" "$rc" "0"
check "run: result" "$(cat "${STATE}/result")" "done 0"
check "run: result 0644" "$(stat -c %a "${STATE}/result")" "644"
check "run: apt-get update, then install" \
    "$(grep '^apt-get ' "$LOG" | awk '{print $2}' | paste -sd, -)" "update,install"
check "run: apt-get install of the download, keeping the configurations" \
    "$(grep -c '^apt-get install -y -o Dpkg::Options::=--force-confold .*/download\.[A-Za-z0-9]*/yuneta-agent-7.31.29-1-amd64.deb$' "$LOG")" "1"
check "run: the download dir is removed" "$(ls -d "${STATE}"/download.* 2>/dev/null | wc -l)" "0"
curl_line="$(grep '^curl ' "$LOG")"
for opt in "--globoff" "--proto =https" "--proto-redir =https" "--retry 3" "--connect-timeout 30" \
        "--speed-limit 1024" "--speed-time 120" "--max-filesize 1073741824"; do
    check "run: curl $opt" "$(printf '%s\n' "$curl_line" | grep -c -- " $opt ")" "1"
done
check "run: curl downloads into the state dir of root" \
    "$(printf '%s\n' "$curl_line" | grep -c -- "-o ${STATE}/download\.")" "1"

run_ok_setup
stub apt-get 'if [ "$1" = update ]; then exit 100; fi'
out="$(ip run)"
check "run, apt-get update fails: the install goes on" "$(grep -c '^apt-get install ' "$LOG")" "1"
check "run, apt-get update fails: says it" "$(echo "$out" | grep -c 'apt-get update FAILED')" "1"
check "run, apt-get update fails: result" "$(cat "${STATE}/result")" "done 0"

run_ok_setup
stub apt-get 'if [ "$1" = install ]; then exit 100; fi'
ip run >/dev/null
check "run, apt-get install fails: result" "$(cat "${STATE}/result")" "done 100"

run_ok_setup
printf 'other bytes\n' > "$FIXTURE"
ip run >/dev/null
check "run, another sha256: result" "$(cat "${STATE}/result")" "sha256-mismatch"
check "run, another sha256: nothing installed" "$(calls apt-get)" "0"
check "run, another sha256: download removed" "$(ls -d "${STATE}"/download.* 2>/dev/null | wc -l)" "0"

run_ok_setup
stub dpkg-deb 'echo openssh-server'
ip run >/dev/null
check "run, another package: result" "$(cat "${STATE}/result")" "not-yuneta-agent yuneta-agent-7.31.29-1-amd64.deb"
check "run, another package: nothing installed" "$(calls apt-get)" "0"

run_ok_setup
stub dpkg-deb 'echo "dpkg-deb: error: not a Debian format archive" >&2; exit 2'
out="$(ip run)"
check "run, dpkg-deb fails: result" "$(cat "${STATE}/result")" "not-yuneta-agent yuneta-agent-7.31.29-1-amd64.deb"
check "run, dpkg-deb fails: its error is in the output" "$(echo "$out" | grep -c 'not a Debian format archive')" "1"
check "run, dpkg-deb fails: says it" "$(echo "$out" | grep -c 'dpkg-deb cannot read')" "1"
check "run, dpkg-deb fails: nothing installed" "$(calls apt-get)" "0"

run_ok_setup
stub curl 'exit 22'
ip run >/dev/null
check "run, download fails: result" "$(cat "${STATE}/result")" "download-failed 22"
check "run, download fails: nothing installed" "$(calls apt-get)" "0"

run_ok_setup
rm -f "${STUBS}/curl"
ip run >/dev/null
check "run, no curl: result" "$(cat "${STATE}/result")" "refused no-curl"

#
#   A download over the bound fails, whatever the server says (ulimit -f)
#
run_ok_setup
stub curl 'while [ $# -gt 0 ]; do if [ "$1" = "-o" ]; then dd if=/dev/zero of="$2" bs=1024 count=16 2>/dev/null; exit $?; fi; shift; done; exit 2'
MAX=8192 ip run >/dev/null
check "run, a download over the bound: refused" "$(sed 's/ .*//' "${STATE}/result")" "download-failed"
check "run, a download over the bound: nothing installed" "$(calls apt-get)" "0"
unset MAX

run_ok_setup
rm -f "${STATE}/url"
ip run >/dev/null
check "run, no data: result" "$(cat "${STATE}/result")" "refused bad-data"

#
#   run of a .rpm: rpm -qp names it, dnf installs it
#
reset
printf '%s' "https://example.com/yuneta-agent-7.31.29-1.x86_64.rpm" > "${DATA}/url"
ip start >/dev/null
: > "$LOG"
ip run >/dev/null
check "rpm: result" "$(cat "${STATE}/result")" "done 0"
check "rpm: dnf install" "$(grep -c '^dnf install -y .*/yuneta-agent-7.31.29-1.x86_64.rpm$' "$LOG")" "1"
check "rpm: no apt-get" "$(calls apt-get)" "0"

#
#   journal
#
reset
ip journal >/dev/null
check "journal: 40 lines by default" \
    "$(grep -c '^journalctl -u yuneta-install-package-\* -n 40 --no-pager -o short-iso$' "$LOG")" "1"
ip journal 200 >/dev/null
check "journal 200" "$(grep -c ' -n 200 ' "$LOG")" "1"
for bad in 0 1001 abc "-1" "10 20" "99999"; do
    check "journal '$bad': refused" "$(ip journal "$bad" | grep -c '^refused: ')" "1"
done
check "journal, two arguments: refused" "$(ip journal 1 2 | grep -c '^refused: ')" "1"

if [ "$FAILED" -ne 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "PASSED"
