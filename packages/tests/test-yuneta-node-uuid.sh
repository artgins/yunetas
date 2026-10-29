#!/bin/bash
#
#   Test of packages/templates/yuneta-node-uuid against a fake /sys.
#   Runs as a NORMAL user, never as root: as root the script drops to the
#   yuneta user, who cannot write in this test's private directory.
#
#   Copyright (c) 2026, ArtGins.
#
set -u

if [ "$(id -u)" = "0" ]; then
    echo "FAIL - run it as a normal user, not as root" >&2
    exit 1
fi

HERE="$(cd "$(dirname "$0")" && pwd)"
SCRIPT="${HERE}/../templates/yuneta-node-uuid"
if ! WORK="$(mktemp -d)" || [ -z "$WORK" ] || [ ! -d "$WORK" ]; then
    echo "FAIL - cannot create a work directory" >&2
    exit 1
fi
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

field() {
    sed -n "s/.*\"$1\"[[:space:]]*:[[:space:]]*\"\\([^\"]*\\)\".*/\\1/p" "$FILE" 2>/dev/null
}

backups() {
    ls -A "$YUNETA_NODE_UUID_DIR" | grep -c '^uuid.json.replaced-'
}

reset() {
    rm -rf "${WORK:?}/uuid" "${WORK:?}/root"
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

set_hostname() {
    mkdir -p "${YUNETA_NODE_UUID_SYSROOT}/proc/sys/kernel"
    printf '%s\n' "$1" > "${YUNETA_NODE_UUID_SYSROOT}/proc/sys/kernel/hostname"
}

del_nic() {
    rm -rf "${YUNETA_NODE_UUID_SYSROOT}/sys/class/net/$1"
}

write_file() {    # uuid source
    mkdir -p "$YUNETA_NODE_UUID_DIR"
    printf '{\n    "uuid": "%s",\n    "source": "%s",\n    "created": "2026-10-28T00:00:00Z"\n}\n' \
        "$1" "$2" > "$FILE"
}

run() {
    "$SCRIPT" "$@" >/dev/null 2>&1
}

#
#   The derivation never changes: every node already deployed holds a uuid
#   made by it, and a new formula would move them all
#
reset
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
run ensure
first="$(field uuid)"
check "dmi: source" "$(field source)" "dmi-product-uuid"
check "dmi: the formula of 7.29.1" "$first" "0204075f-4a9a-8ace-9696-0d92648cadcd"
rm -rf "$YUNETA_NODE_UUID_DIR"
run ensure
check "reinstall: same uuid" "$(field uuid)" "$first"

#
#   A disk cloned to other hardware gets its own uuid, keeps a copy, and
#   says that the yunos running keep the old one until they restart
#
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E33"
out="$("$SCRIPT" ensure 2>&1)"
check "clone: uuid changes" "$( [ "$(field uuid)" != "$first" ] && echo changed)" "changed"
check "clone: old file kept" "$(backups)" "1"
check "clone: says the yunos keep the old uuid" "$(echo "$out" | grep -c 'yunos')" "1"

#
#   Every replacement keeps its own copy, even several in the same second
#
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E34"
run ensure
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E35"
run ensure
check "clone: one copy per replacement" "$(backups)" "3"

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
#   Placeholder DMI is not an identity: falls through to the MAC. Both forms
#   of the known placeholder (SMBIOS 2.6+ and the byte-swapped older one)
#
for ph in 03000200-0400-0500-0006-000700080009 00020003-0004-0005-0006-000700080009; do
    reset
    set_dmi "$ph"
    add_nic eth0 "aa:bb:cc:00:00:01" 0
    run ensure
    check "placeholder dmi $ph: source" "$(field source)" "ethernet-mac"
    check "placeholder dmi $ph: from the MAC" "$(field uuid)" "0a0b2a12-f785-832a-99d3-758caac6dfdd"
done

#
#   ... but a uuid a node already derived from it is kept: no change of the
#   placeholder list moves a node already deployed
#
reset
set_dmi "00020003-0004-0005-0006-000700080009"
add_nic eth0 "aa:bb:cc:00:00:01" 0
old_ph="$(bash -c 'source <(sed -n "/^derive_uuid()/,/^}/p" "$1"); derive_uuid dmi-product-uuid 00020003-0004-0005-0006-000700080009' _ "$SCRIPT")"
write_file "$old_ph" "dmi-product-uuid"
run ensure
check "placeholder already used: uuid kept" "$(field uuid)" "$old_ph"
check "placeholder already used: source kept" "$(field source)" "dmi-product-uuid"

#
#   The MAC: the lowest permanent on-board one
#
reset
set_dmi "03000200-0400-0500-0006-000700080009"
add_nic eth1 "aa:bb:cc:00:00:02" 0
add_nic eth0 "aa:bb:cc:00:00:01" 0
add_nic wlan0 "aa:bb:cc:00:00:00" 0 usb
add_nic eth9 "02:11:22:33:44:55" 1
run ensure
check "mac: source" "$(field source)" "ethernet-mac"
mac_uuid="$(field uuid)"
check "mac: lowest permanent on-board one" "$mac_uuid" "0a0b2a12-f785-832a-99d3-758caac6dfdd"

#
#   The recorded source wins over a better one that appears later
#
set_dt_serial "10000000abcdef01"
run ensure
check "recorded source kept" "$(field source)" "ethernet-mac"
check "recorded source: uuid kept" "$(field uuid)" "$mac_uuid"

#
#   A new card with a lower MAC does not move the node
#
add_nic eth5 "aa:bb:cc:00:00:00" 0
run ensure
check "mac: a lower MAC added, uuid kept" "$(field uuid)" "$mac_uuid"

#
#   The interface it came from missing at a boot, another of the same box
#   present: kept, and still kept when it is back
#
del_nic eth0
run ensure
check "mac: its interface missing, uuid kept" "$(field uuid)" "$mac_uuid"
add_nic eth0 "aa:bb:cc:00:00:01" 0
run ensure
check "mac: its interface back, uuid kept" "$(field uuid)" "$mac_uuid"
check "mac: no copy, nothing was replaced" "$(backups)" "0"

#
#   A file of 7.29.1 (no list of the box's interfaces) gets it, uuid kept
#
reset
add_nic eth0 "aa:bb:cc:00:00:01" 0
add_nic eth1 "aa:bb:cc:00:00:02" 0
write_file "$mac_uuid" "ethernet-mac"
run ensure
check "mac 7.29.1 file: uuid kept" "$(field uuid)" "$mac_uuid"
check "mac 7.29.1 file: the interfaces recorded" "$(grep -c '"ethernet_ids"' "$FILE")" "1"
check "mac 7.29.1 file: no copy" "$(backups)" "0"
del_nic eth0
run ensure
check "mac 7.29.1 file: its interface missing, uuid kept" "$(field uuid)" "$mac_uuid"

#
#   A disk cloned to a box with none of these interfaces: its own uuid
#
del_nic eth1
add_nic eth0 "aa:bb:cc:00:01:01" 0
run ensure
check "mac clone: uuid changes" "$( [ "$(field uuid)" != "$mac_uuid" ] && echo changed)" "changed"

#
#   Devicetree serial on a fresh Arm board
#
reset
set_dt_serial "10000000abcdef01"
run ensure
check "devicetree: source" "$(field source)" "devicetree-serial"

#
#   Nothing readable: random once, then kept while there is no identifier;
#   an identifier that appears later replaces it (an image made where /sys
#   was not the box's own gets the box's uuid on its first boot)
#
reset
run ensure
check "random: source" "$(field source)" "random"
rnd="$(field uuid)"
run ensure
check "random: kept" "$(field uuid)" "$rnd"
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
run ensure
check "random: replaced by the hardware that appears" "$(field uuid)" "$first"

#
#   In a container the hardware is the host's, shared by every container
#   of it: random, and an existing uuid is kept as it is
#
reset
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
touch "${YUNETA_NODE_UUID_SYSROOT}/.dockerenv"
set_hostname "c0ffee000001"
run ensure
check "container: source" "$(field source)" "random"
check "container: the hostname it belongs to" "$(field hostname)" "c0ffee000001"
cnt="$(field uuid)"
run ensure
check "container: random kept across its restarts" "$(field uuid)" "$cnt"

#
#   An image built in a container: the package's install stores nothing,
#   so the image carries no uuid for every container made from it
#
reset
touch "${YUNETA_NODE_UUID_SYSROOT}/.dockerenv"
set_hostname "buildkitsandbox"
run ensure --installing
check "image build: nothing stored at install" "$( [ -e "$FILE" ] && echo stored || echo none)" "none"
run ensure --installing
check "image build: still nothing" "$( [ -e "$FILE" ] && echo stored || echo none)" "none"
check "ensure: refuses an unknown option" "$("$SCRIPT" ensure --bogus >/dev/null 2>&1; echo $?)" "2"

#
#   ... and a uuid made there all the same (the agents started while the
#   image was built) is not taken by a container of another hostname: each
#   container of the image makes its own, keeps it across its restarts, and
#   the copy of the baked one is kept
#
run ensure
baked="$(field uuid)"
set_hostname "c0ffee000001"
run ensure
one="$(field uuid)"
check "image build: the container makes its own" "$( [ "$one" != "$baked" ] && echo changed)" "changed"
check "image build: copy of the baked one" "$(backups)" "1"
check "image build: the hostname it belongs to" "$(field hostname)" "c0ffee000001"
run ensure
check "image build: its own kept across its restarts" "$(field uuid)" "$one"
check "image build: no more copies" "$(backups)" "1"

#
#   A board written from a `docker export` of that container keeps
#   /.dockerenv: it is taken for a container, and still gets its own
#
set_hostname "localhost"
run ensure
check "docker export on a board: its own uuid" "$( [ "$(field uuid)" != "$one" ] && echo changed)" "changed"

#
#   A random of 7.29.2 made in a container records no hostname: it is that
#   container's own as far as anybody can tell, and it is never moved
#
reset
touch "${YUNETA_NODE_UUID_SYSROOT}/.dockerenv"
set_hostname "c0ffee000002"
write_file "ff14fc16-2cb2-472c-9c0a-89fa43837131" "random"
run ensure
check "container, 7.29.2 random: kept" "$(field uuid)" "ff14fc16-2cb2-472c-9c0a-89fa43837131"
check "container, 7.29.2 random: no copy" "$(backups)" "0"
reset
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
mkdir -p "${YUNETA_NODE_UUID_SYSROOT}/run/systemd"
echo lxc > "${YUNETA_NODE_UUID_SYSROOT}/run/systemd/container"
write_file "$first" "dmi-product-uuid"
run ensure
check "container: existing uuid kept" "$(field uuid)" "$first"
check "container: show says it" "$("$SCRIPT" show 2>&1 | grep -c 'container')" "1"

#
#   show without root: a source it cannot read is said, not skipped
#
reset
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
chmod 000 "${YUNETA_NODE_UUID_SYSROOT}/sys/class/dmi/id/product_uuid"
add_nic eth0 "aa:bb:cc:00:00:01" 0
out="$("$SCRIPT" show 2>&1)"
check "show: an unreadable source is said" "$(echo "$out" | grep -c 'not readable')" "1"
check "show: no answer from the MAC behind it" "$(echo "$out" | grep -c 'ethernet-mac ->')" "0"
chmod 600 "${YUNETA_NODE_UUID_SYSROOT}/sys/class/dmi/id/product_uuid"

#
#   A manual uuid is never touched
#
reset
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
out="$("$SCRIPT" set "FF14FC16-2CB2-472C-9C0A-89FA43837130" 2>&1)"
check "set: says to restart the agents and the yunos" "$(echo "$out" | grep -c 'yunos')" "1"
run ensure
check "manual: kept" "$(field uuid)" "ff14fc16-2cb2-472c-9c0a-89fa43837130"
check "manual: source" "$(field source)" "manual"
check "set: refuses a non-uuid" "$("$SCRIPT" set nonsense >/dev/null 2>&1; echo $?)" "1"
check "set, no file before: no copy" "$(backups)" "0"
run set "ff14fc16-2cb2-472c-9c0a-89fa43837130"
check "set again, same uuid: no copy" "$(backups)" "0"
check "set again, same uuid: kept" "$(field uuid)" "ff14fc16-2cb2-472c-9c0a-89fa43837130"

#
#   `set` of the uuid the hardware already gave: it becomes manual, and no
#   copy is kept, the uuid did not change
#
reset
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
run ensure
created_first="$(field created)"
run set "$first"
check "set, the uuid it had: manual" "$(field source)" "manual"
check "set, the uuid it had: no copy" "$(backups)" "0"
check "set, the uuid it had: created kept" "$(field created)" "$created_first"
run set "ff14fc16-2cb2-472c-9c0a-89fa43837130"
check "set, another uuid: a copy" "$(backups)" "1"

if [ "$FAILED" -ne 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "PASSED"
