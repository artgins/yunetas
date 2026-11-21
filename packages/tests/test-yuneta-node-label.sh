#!/bin/bash
#
#   Test of packages/templates/yuneta-node-label against a fake /sys, with
#   the yuneta-node-uuid of this tree. Runs as a NORMAL user, never as root
#   (see test-yuneta-node-uuid.sh). The SVG is tested only where qrencode
#   is installed.
#
#   Copyright (c) 2026, ArtGins.
#
set -u

if [ "$(id -u)" = "0" ]; then
    echo "FAIL - run it as a normal user, not as root" >&2
    exit 1
fi

HERE="$(cd "$(dirname "$0")" && pwd)"
LABEL="${HERE}/../templates/yuneta-node-label"
NODE_UUID="${HERE}/../templates/yuneta-node-uuid"
if ! WORK="$(mktemp -d)" || [ -z "$WORK" ] || [ ! -d "$WORK" ]; then
    echo "FAIL - cannot create a work directory" >&2
    exit 1
fi
trap 'rm -rf "$WORK"' EXIT

export YUNETA_NODE_UUID_TOOL="$NODE_UUID"
export YUNETA_NODE_UUID_DIR="${WORK}/uuid"
export YUNETA_NODE_UUID_SYSROOT="${WORK}/root"
FILE="${YUNETA_NODE_UUID_DIR}/uuid.json"
OUT="${WORK}/out.txt"
ERR="${WORK}/err.txt"
SVG="${WORK}/label.svg"
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

stored_uuid() {
    sed -n 's/.*"uuid"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$FILE" 2>/dev/null
}

out_field() {
    sed -n "s/^$1: *//p" "$OUT" | paste -sd'|' -
}

reset() {
    rm -rf "${WORK:?}/uuid" "${WORK:?}/root" "$SVG"
    mkdir -p "${YUNETA_NODE_UUID_SYSROOT}/sys/class/net"
}

set_dt() {    # serial model
    mkdir -p "${YUNETA_NODE_UUID_SYSROOT}/sys/firmware/devicetree/base"
    printf '%s\000' "$1" > "${YUNETA_NODE_UUID_SYSROOT}/sys/firmware/devicetree/base/serial-number"
    printf '%s\000' "$2" > "${YUNETA_NODE_UUID_SYSROOT}/sys/firmware/devicetree/base/model"
}

set_dmi() {    # product_uuid vendor product
    mkdir -p "${YUNETA_NODE_UUID_SYSROOT}/sys/class/dmi/id"
    printf '%s\n' "$1" > "${YUNETA_NODE_UUID_SYSROOT}/sys/class/dmi/id/product_uuid"
    printf '%s\n' "$2" > "${YUNETA_NODE_UUID_SYSROOT}/sys/class/dmi/id/sys_vendor"
    printf '%s\n' "$3" > "${YUNETA_NODE_UUID_SYSROOT}/sys/class/dmi/id/product_name"
}

add_nic() {    # name mac addr_assign_type [usb|wireless]
    local d="${YUNETA_NODE_UUID_SYSROOT}/sys/class/net/$1"
    local dev="${YUNETA_NODE_UUID_SYSROOT}/devices/platform/$1"
    if [ "${4:-}" = "usb" ]; then
        dev="${YUNETA_NODE_UUID_SYSROOT}/devices/usb1/$1"
    fi
    mkdir -p "$d" "$dev"
    ln -s "$dev" "$d/device"
    if [ "${4:-}" = "wireless" ]; then
        mkdir -p "$d/wireless"
    fi
    echo 1 > "$d/type"
    echo "$3" > "$d/addr_assign_type"
    echo "$2" > "$d/address"
}

run_label() {
    "$LABEL" "$@" >"$OUT" 2>"$ERR"
}

#
#   A box with a chip identifier: the label carries the uuid that
#   yuneta-node-uuid stored, and only the on-board wired ports
#
reset
set_dt "0000c0ffee000042" "Example Board Rev 1"
add_nic end0 02:00:00:00:50:94 0
add_nic end1 02:00:00:00:50:95 0
add_nic wlan0 02:00:00:00:77:01 0 wireless
add_nic usb0 00:e0:4c:68:00:01 0 usb
add_nic eth9 2a:00:00:00:00:09 1
"$NODE_UUID" ensure >/dev/null 2>&1
uuid="$(stored_uuid)"
run_label --title CONTROLLER
check "chip: exit 0" "$?" "0"
check "chip: the stored uuid" "$(out_field uuid)" "$uuid"
check "chip: its source" "$(out_field source)" "devicetree-serial"
check "chip: the short id" "$(out_field short)" "${uuid%%-*}"
check "chip: the QR in upper case" "$(out_field qr)" "$(printf '%s' "$uuid" | tr 'a-f' 'A-F')"
check "chip: the model" "$(out_field model)" "Example Board Rev 1"
check "chip: on-board wired ports only, MACs in lower case" "$(out_field port)" \
    "end0 02:00:00:00:50:94|end1 02:00:00:00:50:95"
check "chip: the title" "$(out_field title)" "CONTROLLER"
check "chip: no warning" "$(wc -c < "$ERR")" "0"

#
#   The printable label
#
if command -v qrencode >/dev/null 2>&1; then
    run_label --title "A & B" --svg "$SVG"
    check "svg: exit 0" "$?" "0"
    check "svg: the text still on stdout" "$(out_field uuid)" "$uuid"
    if command -v xmllint >/dev/null 2>&1; then
        xmllint --noout "$SVG" 2>/dev/null
        check "svg: well-formed XML" "$?" "0"
    fi
    check "svg: the title escaped" "$(grep -c 'A &amp; B' "$SVG")" "1"
    check "svg: the short id" "$(grep -c ">${uuid%%-*}<" "$SVG")" "1"
    check "svg: one line per port" "$(grep -c '>end[01] ' "$SVG")" "2"
    check "svg: the height grows with the ports" \
        "$(sed -n 's/.*height="\([0-9]*\)mm".*/\1/p' "$SVG" | head -1)" "43"

    "$LABEL" --svg - >"$OUT" 2>"$ERR"
    check "svg to stdout: exit 0" "$?" "0"
    check "svg to stdout: stdout is only the SVG" "$(head -c 4 "$OUT")" "<svg"
    check "svg to stdout: the text on stderr" "$(sed -n 's/^uuid: *//p' "$ERR")" "$uuid"
else
    echo "skip - the SVG: qrencode is not installed"
fi

#
#   A stored uuid that this hardware does not give: no label
#
reset
set_dt "0000c0ffee000042" "Example Board Rev 1"
add_nic end0 02:00:00:00:50:94 0
"$NODE_UUID" ensure >/dev/null 2>&1
sed -i 's/"uuid": "[0-9a-f]\{8\}/"uuid": "deadbeef/' "$FILE"
if command -v qrencode >/dev/null 2>&1; then
    run_label --svg "$SVG"
    check "other uuid: refused" "$?" "1"
    check "other uuid: no SVG" "$([ -e "$SVG" ] && echo yes || echo no)" "no"
else
    run_label
    check "other uuid: refused" "$?" "1"
fi
check "other uuid: nothing on stdout" "$(wc -c < "$OUT")" "0"
check "other uuid: it says why" "$(grep -c 'not the one this hardware gives' "$ERR")" "1"

#
#   A random uuid (no identifier) and one set by hand: no label
#
reset
"$NODE_UUID" ensure >/dev/null 2>&1
run_label
check "random: refused" "$?" "1"

reset
set_dt "0000c0ffee000042" "Example Board Rev 1"
"$NODE_UUID" set 11111111-2222-8333-8444-555555555555 >/dev/null 2>&1
run_label
check "set by hand: refused" "$?" "1"

#
#   A machine with no chip identifier: the label, with a warning
#
reset
set_dmi "deadbeef-1234-5678-9abc-def000004e32" "ACME" "Server 1"
add_nic eno1 aa:bb:cc:00:00:01 0
"$NODE_UUID" ensure >/dev/null 2>&1
run_label
check "machine: exit 0" "$?" "0"
check "machine: the stored uuid" "$(out_field uuid)" "$(stored_uuid)"
check "machine: the model from DMI" "$(out_field model)" "ACME Server 1"
check "machine: the warning" "$(grep -c 'lower MAC changes it' "$ERR")" "1"

#
#   A machine whose ports are bond slaves: each port with its own MAC
#   (bonding_slave/perm_hwaddr), not the bond's, as yuneta-node-uuid takes
#   them
#
reset
set_dmi "deadbeef-1234-5678-9abc-def000004e32" "ACME" "Server 1"
for p in "eno1 aa:bb:cc:00:00:01" "eno2 aa:bb:cc:00:00:02"; do
    add_nic ${p% *} aa:bb:cc:00:00:01 3
    mkdir -p "${YUNETA_NODE_UUID_SYSROOT}/sys/class/net/${p% *}/bonding_slave"
    echo "${p#* }" > "${YUNETA_NODE_UUID_SYSROOT}/sys/class/net/${p% *}/bonding_slave/perm_hwaddr"
done
"$NODE_UUID" ensure >/dev/null 2>&1
run_label
check "bond: exit 0" "$?" "0"
check "bond: each port with its own MAC" "$(out_field port)" "eno1 aa:bb:cc:00:00:01|eno2 aa:bb:cc:00:00:02"

#
#   Usage
#
"$LABEL" --svg >/dev/null 2>&1
check "usage: --svg without a file" "$?" "2"
"$LABEL" --bogus >/dev/null 2>&1
check "usage: an unknown option" "$?" "2"

if [ "$FAILED" -ne 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "PASSED"
