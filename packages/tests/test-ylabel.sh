#!/bin/bash
#
#   Test of packages/templates/ylabel: the arguments it gives to the label
#   tool, which label tool it takes, and that it goes through sudo. The label
#   tool and sudo are stubs that write down what they get
#   (test-yuneta-node-label.sh tests the label tool). Runs as a NORMAL user.
#
#   Copyright (c) 2026, ArtGins.
#
set -u

if [ "$(id -u)" = "0" ]; then
    echo "FAIL - run it as a normal user, not as root" >&2
    exit 1
fi

HERE="$(cd "$(dirname "$0")" && pwd)"
YLABEL="${HERE}/../templates/ylabel"
if ! WORK="$(mktemp -d)" || [ -z "$WORK" ] || [ ! -d "$WORK" ]; then
    echo "FAIL - cannot create a work directory" >&2
    exit 1
fi
trap 'rm -rf "$WORK"' EXIT

LOG="${WORK}/log"
OUT="${WORK}/out.txt"
ERR="${WORK}/err.txt"
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
#   Stubs: a sudo that writes down that it ran and runs the rest, and a
#   label tool that writes down its name and its arguments
#
mkdir -p "$LOG" "${WORK}/libexec"
cat > "${WORK}/sudo" <<STUBEOF
#!/bin/bash
echo sudo > "${LOG}/sudo"
exec "\$@"
STUBEOF
cat > "${WORK}/libexec/yuneta-node-label" <<STUBEOF
#!/bin/bash
echo libexec "\$*" > "${LOG}/tool"
exit "\${STUB_EXIT:-0}"
STUBEOF
chmod 755 "${WORK}/sudo" "${WORK}/libexec/yuneta-node-label"
export YUNETA_SUDO="${WORK}/sudo"
export YUNETA_NODE_LABEL_TOOL="${WORK}/libexec/yuneta-node-label"

run_ylabel() {
    rm -f "${LOG}/sudo" "${LOG}/tool"
    "$YLABEL" "$@" >"$OUT" 2>"$ERR"
}

#
#   The title, and the print
#
run_ylabel
check "no argument: exit 0" "$?" "0"
check "no argument: title CONTROLLER, printed" "$(cat "${LOG}/tool")" "libexec --title CONTROLLER --print"
check "no argument: through sudo" "$(cat "${LOG}/sudo")" "sudo"

run_ylabel CONTROLADOR
check "a title: exit 0" "$?" "0"
check "a title: that title" "$(cat "${LOG}/tool")" "libexec --title CONTROLADOR --print"

run_ylabel "CAJA 7"
check "a title with a space: one argument" "$(cat "${LOG}/tool")" "libexec --title CAJA 7 --print"

STUB_EXIT=1 run_ylabel
check "the label tool fails: its exit" "$?" "1"

#
#   Which label tool: the package's, else the one beside ylabel, also
#   through a symlink; none of them, a refusal
#
TREE="${WORK}/tree"
mkdir -p "$TREE" "${WORK}/bin"
cp "$YLABEL" "${TREE}/ylabel"
cat > "${TREE}/yuneta-node-label" <<STUBEOF
#!/bin/bash
echo beside "\$*" > "${LOG}/tool"
STUBEOF
chmod 755 "${TREE}/yuneta-node-label"
ln -s "${TREE}/ylabel" "${WORK}/bin/ylabel"

rm -f "${LOG}/tool"
"${TREE}/ylabel" >"$OUT" 2>"$ERR"
check "package tool present: it is taken first" "$(cut -d' ' -f1 "${LOG}/tool")" "libexec"

rm -f "${LOG}/tool"
YUNETA_NODE_LABEL_TOOL="${WORK}/none" "${TREE}/ylabel" >"$OUT" 2>"$ERR"
check "no package tool: exit 0" "$?" "0"
check "no package tool: the one beside ylabel" "$(cat "${LOG}/tool")" "beside --title CONTROLLER --print"

rm -f "${LOG}/tool"
YUNETA_NODE_LABEL_TOOL="${WORK}/none" "${WORK}/bin/ylabel" >"$OUT" 2>"$ERR"
check "through a symlink: the one beside its target" "$(cut -d' ' -f1 "${LOG}/tool")" "beside"

rm -f "${TREE}/yuneta-node-label"
YUNETA_NODE_LABEL_TOOL="${WORK}/none" "${TREE}/ylabel" >"$OUT" 2>"$ERR"
check "no label tool: exit 1" "$?" "1"
check "no label tool: it names both places" "$(grep -c "${WORK}/none nor ${TREE}/yuneta-node-label" "$ERR")" "1"

#
#   Usage
#
run_ylabel -h
check "usage: -h, exit 0" "$?" "0"
check "usage: -h prints the usage" "$(grep -c '^usage: ylabel' "$OUT")" "1"
run_ylabel A B
check "usage: two arguments, exit 2" "$?" "2"
run_ylabel --print
check "usage: an option, exit 2" "$?" "2"
run_ylabel ""
check "usage: an empty title, exit 2" "$?" "2"
check "usage: nothing run" "$( [ -e "${LOG}/tool" ] && echo ran || echo none)" "none"

if [ "$FAILED" -ne 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "PASSED"
