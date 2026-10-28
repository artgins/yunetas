#!/bin/bash
#
#   Test of packages/templates/yuneta-node-uuid against a fake /sys.
#   Runs as any user: ownership is only set when the script runs as root.
#
#   Copyright (c) 2026, ArtGins.
#
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
SCRIPT="${HERE}/../templates/yuneta-node-uuid"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

export YUNETA_NODE_UUID_DIR="${WORK}/uuid"
export YUNETA_NODE_UUID_SYSROOT="${WORK}/root"
FILE="${YUNETA_NODE_UUID_DIR}/uuid.json"
FAILED=0

#
#   Helpers
#
check() {
    local what="$1" got="$2" expected="$3"
    if [ "$got" = "$expected" ]; then
        echo "ok   - $what"
    else
        echo "FAIL - $what: got '$got', expected '$expected'"
        FAILED=1
    fi
}

field() {
    sed -n "s/.*\"$1\"[[:space:]]*:[[:space:]]*\"\\([^\"]*\\)\".*/\\1/p" "$FILE"
}

reset() {
    rm -rf "$YUNETA_NODE_UUID_DIR" "$YUNETA_NODE_UUID_SYSROOT"
    mkdir -p "${YUNETA_NODE_UUID_SYSROOT}/sys/class/net"
}

set_dmi() {
    mkdir -p "${YUNETA_NODE_UUID_SYSROOT}/sys/class/dmi/id"
    printf '%s\n' "$1" > "${YUNETA_NODE_UUID_SYSROOT}/sys/class/dmi/id/product_uuid"
}

set_dt_serial() {
    mkdir -p "${YUNETA_NODE_UUID_SYSROOT}/sys/firmware/devicetree/base"
    printf '%s\000' "$1" > "${YUNETA_NODE_UUID_SYSROOT}/sys/firmware/devicetree/base/serial-number"
}

add_nic() {    # name mac addr_assign_type [usb]
    local d="${YUNETA_NODE_UUID_SYSROOT}/sys/class/net/$1"
    local dev="${YUNETA_NODE_UUID_SYSROOT}/devices/platform/$1"
    if [ "${4:-}" = "usb" ]; then
        dev="${YUNETA_NODE_UUID_SYSROOT}/devices/usb1/$1"
    fi
    mkdir -p "$d" "$dev"
    ln -s "$dev" "$d/device"
    echo 1 > "$d/type"
    echo "$3" > "$d/addr_assign_type"
    echo "$2" > "$d/address"
}

run() {
    "$SCRIPT" "$@" >/dev/null 2>&1
}

#
#   A fresh node takes its uuid from DMI, and a reinstall gives the same one
#
reset
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
run ensure
first="$(field uuid)"
check "dmi: source" "$(field source)" "dmi-product-uuid"
check "dmi: version 8 uuid" "$(echo "$first" | cut -c15)" "8"
rm -rf "$YUNETA_NODE_UUID_DIR"
run ensure
check "reinstall: same uuid" "$(field uuid)" "$first"

#
#   A disk cloned to other hardware gets its own uuid, and keeps a copy
#
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E33"
run ensure
check "clone: uuid changes" "$( [ "$(field uuid)" != "$first" ] && echo changed)" "changed"
check "clone: old file kept" "$(ls "$YUNETA_NODE_UUID_DIR" | grep -c '^uuid.json.replaced-')" "1"

#
#   A legacy random uuid (no source) is replaced by the hardware one
#
reset
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
mkdir -p "$YUNETA_NODE_UUID_DIR"
echo '{"uuid": "ff14fc16-2cb2-472c-9c0a-89fa43837130"}' > "$FILE"
run ensure
check "legacy: replaced by hardware" "$(field uuid)" "$first"

#
#   Placeholder DMI is not an identity: falls through to the MAC
#
reset
set_dmi "03000200-0400-0500-0006-000700080009"
add_nic eth1 "aa:bb:cc:00:00:02" 0
add_nic eth0 "aa:bb:cc:00:00:01" 0
add_nic wlan0 "aa:bb:cc:00:00:00" 0 usb
add_nic eth9 "02:11:22:33:44:55" 1
run ensure
check "placeholder dmi: source" "$(field source)" "ethernet-mac"
mac_uuid="$(field uuid)"
rm -rf "${YUNETA_NODE_UUID_SYSROOT}/sys/class/net/eth1"
run ensure
check "mac: lowest permanent on-board one" "$(field uuid)" "$mac_uuid"

#
#   The recorded source wins over a better one that appears later
#
set_dt_serial "10000000abcdef01"
run ensure
check "recorded source kept" "$(field source)" "ethernet-mac"

#
#   Devicetree serial on a fresh Arm board
#
reset
set_dt_serial "10000000abcdef01"
run ensure
check "devicetree: source" "$(field source)" "devicetree-serial"

#
#   Nothing readable: random once, then kept
#
reset
run ensure
check "random: source" "$(field source)" "random"
rnd="$(field uuid)"
run ensure
check "random: kept" "$(field uuid)" "$rnd"

#
#   A manual uuid is never touched
#
reset
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
run set "FF14FC16-2CB2-472C-9C0A-89FA43837130"
run ensure
check "manual: kept" "$(field uuid)" "ff14fc16-2cb2-472c-9c0a-89fa43837130"
check "manual: source" "$(field source)" "manual"
check "set: refuses a non-uuid" "$("$SCRIPT" set nonsense >/dev/null 2>&1; echo $?)" "1"

if [ "$FAILED" -ne 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "PASSED"
