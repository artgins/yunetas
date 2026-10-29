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

#
#   The pid namespace this runs in: the initial one (4026531836, the
#   kernel's PROC_PID_INIT_INO) is a system booted on its own, any other a
#   container
#
set_pidns() {
    mkdir -p "${YUNETA_NODE_UUID_SYSROOT}/proc/self/ns"
    ln -sfn "pid:[$1]" "${YUNETA_NODE_UUID_SYSROOT}/proc/self/ns/pid"
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
#   /.dockerenv, and its hostname: it boots its own init, in the initial pid
#   namespace, so it is no container and its hardware gives its uuid
#
set_pidns 4026531836
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
run ensure
check "docker export on a board, same hostname: the hardware read" "$(field source)" "dmi-product-uuid"
check "docker export on a board, same hostname: its own uuid" "$(field uuid)" "$first"
check "docker export on a board: copy of the container's" "$(backups)" "2"
out="$("$SCRIPT" show 2>&1)"
check "docker export on a board: show reads the hardware" "$(echo "$out" | grep -c 'container')" "0"

#
#   ... and one written from a container that had no hardware identifier to
#   read either: the random it carries was the container's. The board is no
#   container (the initial pid namespace), so it is not ours even with the
#   same hostname: a new random, a copy of the old file, and it says why
#
reset
touch "${YUNETA_NODE_UUID_SYSROOT}/.dockerenv"
set_hostname "c0ffee000006"
run ensure
exported="$(field uuid)"
check "docker export, no identifier: the container's random" "$(field container)" "docker"
set_pidns 4026531836
check "docker export on a board, no identifier: show says ensure replaces it" "$("$SCRIPT" show 2>&1 | grep -c 'this is no container: ensure makes a new one')" "1"
out="$("$SCRIPT" ensure 2>&1)"
board="$(field uuid)"
check "docker export on a board, no identifier: a new random" "$( [ -n "$board" ] && [ "$board" != "$exported" ] && echo changed)" "changed"
check "docker export on a board, no identifier: source" "$(field source)" "random"
check "docker export on a board, no identifier: no container recorded" "$(grep -c '"container"' "$FILE")" "0"
check "docker export on a board, no identifier: copy of the container's" "$(backups)" "1"
check "docker export on a board, no identifier: says why" "$(echo "$out" | grep -c 'this is no container')" "1"
run ensure
check "docker export on a board, no identifier: its random kept" "$(field uuid)" "$board"
check "docker export on a board, no identifier: no more copies" "$(backups)" "1"

#
#   ... but the same file where the pid namespace cannot be read is still
#   the container's: only the initial pid namespace says "no container"
#
reset
touch "${YUNETA_NODE_UUID_SYSROOT}/.dockerenv"
set_hostname "c0ffee000006"
run ensure
exported="$(field uuid)"
rm -f "${YUNETA_NODE_UUID_SYSROOT}/.dockerenv"
run ensure
check "container random, pid namespace unknown, no marker: kept" "$(field uuid)" "$exported"

#
#   systemd-detect-virt reads /.dockerenv and /run/.containerenv in ANY pid
#   namespace (and answers "docker" on that board), so its docker / podman
#   is taken only out of the initial one. There only what it reads from the
#   kernel counts (wsl, openvz, proot).
#
FAKE_VIRT="${WORK}/systemd-detect-virt"
fake_virt() {
    printf '#!/bin/sh\necho %s\n' "$1" > "$FAKE_VIRT"
    chmod +x "$FAKE_VIRT"
}
for v in docker podman; do
    reset
    set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
    touch "${YUNETA_NODE_UUID_SYSROOT}/.dockerenv"
    set_pidns 4026531836
    fake_virt "$v"
    YUNETA_NODE_UUID_DETECT_VIRT="$FAKE_VIRT" run ensure
    check "systemd-detect-virt '$v' in the initial pid namespace: the hardware" "$(field source)" "dmi-product-uuid"
    reset
    set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
    set_pidns 4026532512
    fake_virt "$v"
    YUNETA_NODE_UUID_DETECT_VIRT="$FAKE_VIRT" run ensure
    check "systemd-detect-virt '$v' in another pid namespace: a container" "$(field container)" "$v"
done
reset
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
set_pidns 4026531836
fake_virt wsl
YUNETA_NODE_UUID_DETECT_VIRT="$FAKE_VIRT" run ensure
check "systemd-detect-virt 'wsl' in the initial pid namespace: a container" "$(field container)" "wsl"

#
#   A pid namespace that is not the initial one, with no marker at all: a
#   container. That is a BuildKit `RUN` step (no /.dockerenv, no container=,
#   PID 1 is the step's shell), and containerd / Kubernetes, which write no
#   marker: an install there must not store the build host's DMI or MAC in
#   the image. A system booted on its own never runs out of the initial one.
#
reset
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
add_nic eth0 "aa:bb:cc:00:00:01" 0
set_pidns 4026532512
set_hostname "buildkitsandbox"
check "another pid namespace, no marker, install: exit 3" "$("$SCRIPT" ensure --installing >/dev/null 2>&1; echo $?)" "3"
check "another pid namespace, no marker, install: nothing stored" "$( [ -e "$FILE" ] && echo stored || echo none)" "none"
run ensure
check "another pid namespace, no marker: random" "$(field source)" "random"
check "another pid namespace, no marker: container-other" "$(field container)" "container-other"

#
#   The same against the real kernel: a BuildKit `RUN` step emulated with an
#   unprivileged user + pid namespace and a /proc of its own, with no
#   /.dockerenv, no container= and an empty /run:
#
#       unshare -Upf --mount-proc <the script> ensure --installing
#
#   with YUNETA_NODE_UUID_SYSROOT a directory whose proc and sys are links
#   to the real /proc and /sys (so the pid namespace and the hardware are
#   the kernel's, and systemd-detect-virt is not asked: ours alone decides).
#   Skipped where unprivileged user namespaces are refused.
#
EMU="${WORK}/emu"
mkdir -p "${EMU}/run"
ln -s /proc "${EMU}/proc"
ln -s /sys "${EMU}/sys"
emulated() {    # args of the script
    env -u container YUNETA_NODE_UUID_SYSROOT="$EMU" \
        unshare -Upf --mount-proc "$SCRIPT" "$@" >/dev/null 2>&1
}
if unshare -Upf --mount-proc true >/dev/null 2>&1; then
    reset
    check "BuildKit RUN step (unshare -Upf), install: exit 3" "$(emulated ensure --installing; echo $?)" "3"
    check "BuildKit RUN step (unshare -Upf), install: nothing stored" "$( [ -e "$FILE" ] && echo stored || echo none)" "none"
    emulated ensure
    check "BuildKit RUN step (unshare -Upf): random" "$(field source)" "random"
    check "BuildKit RUN step (unshare -Upf): container-other" "$(field container)" "container-other"
    if [ "$(readlink /proc/self/ns/pid)" = "pid:[4026531836]" ]; then
        reset
        env -u container YUNETA_NODE_UUID_SYSROOT="$EMU" "$SCRIPT" ensure --installing >/dev/null 2>&1
        check "the same SYSROOT in the initial pid namespace: no container" "$(grep -c '"container"' "$FILE")" "0"
    else
        echo "skip - this test does not run in the initial pid namespace"
    fi
else
    echo "skip - unshare -Upf --mount-proc refused here: no BuildKit emulation"
fi

#
#   /.dockerenv in another pid namespace: a container
#
reset
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
touch "${YUNETA_NODE_UUID_SYSROOT}/.dockerenv"
set_pidns 4026532512
set_hostname "c0ffee000004"
run ensure
check "/.dockerenv in its own pid namespace: a container" "$(field source)" "random"
reset
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
mkdir -p "${YUNETA_NODE_UUID_SYSROOT}/run"
touch "${YUNETA_NODE_UUID_SYSROOT}/run/.containerenv"
set_pidns 4026531836
run ensure
check "/run/.containerenv in the initial pid namespace: the hardware" "$(field source)" "dmi-product-uuid"

#
#   In the initial pid namespace only what this boot made counts. An Ubuntu
#   image ships /run/systemd/container with "docker": a board written from
#   it and booted with /run on its root filesystem keeps that file, and
#   systemd-detect-virt answers from it. Those files count there only on a
#   tmpfs /run, which every systemd boot mounts anew; PID 1's container=
#   always counts (it is the live process's)
#
set_run_fs() {    # the fstype of /run in mountinfo, "" for none mounted
    mkdir -p "${YUNETA_NODE_UUID_SYSROOT}/proc/self"
    printf '22 1 8:1 / / rw,relatime shared:1 - ext4 /dev/sda1 rw\n' \
        > "${YUNETA_NODE_UUID_SYSROOT}/proc/self/mountinfo"
    if [ -n "$1" ]; then
        printf '25 22 0:23 / /run rw,nosuid,nodev shared:5 - %s %s rw\n' "$1" "$1" \
            >> "${YUNETA_NODE_UUID_SYSROOT}/proc/self/mountinfo"
    fi
}
set_marker() {    # file under SYSROOT, content
    mkdir -p "$(dirname "${YUNETA_NODE_UUID_SYSROOT}/$1")"
    printf '%s\n' "$2" > "${YUNETA_NODE_UUID_SYSROOT}/$1"
}
board() {    # a board in the initial pid namespace, /run of that fstype
    reset
    set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
    set_pidns 4026531836
    set_run_fs "$1"
}
board ""
touch "${YUNETA_NODE_UUID_SYSROOT}/.dockerenv"
set_marker run/systemd/container docker
run ensure
check "Ubuntu image on a board, /run on the root fs: the hardware" "$(field source)" "dmi-product-uuid"
fake_virt docker
YUNETA_NODE_UUID_DETECT_VIRT="$FAKE_VIRT" run ensure
check "Ubuntu image on a board, detect-virt 'docker': the hardware" "$(field source)" "dmi-product-uuid"
for fs in "" ext4 overlay; do
    board "$fs"
    set_marker run/systemd/container lxc
    fake_virt lxc
    YUNETA_NODE_UUID_DETECT_VIRT="$FAKE_VIRT" run ensure
    check "stale run/systemd/container 'lxc', /run '${fs:-not mounted}': the hardware" "$(field source)" "dmi-product-uuid"
    board "$fs"
    set_marker run/host/container-manager systemd-nspawn
    fake_virt systemd-nspawn
    YUNETA_NODE_UUID_DETECT_VIRT="$FAKE_VIRT" run ensure
    check "stale run/host/container-manager, /run '${fs:-not mounted}': the hardware" "$(field source)" "dmi-product-uuid"
done
board tmpfs
set_marker run/systemd/container lxc
run ensure
check "run/systemd/container on a tmpfs /run, initial pid namespace: a container" "$(field container)" "lxc"
board tmpfs
set_marker run/host/container-manager systemd-nspawn
run ensure
check "run/host/container-manager on a tmpfs /run, initial pid namespace: a container" "$(field container)" "systemd-nspawn"
board ""
mkdir -p "${YUNETA_NODE_UUID_SYSROOT}/proc/1"
printf 'HOME=/\000container=lxc\000' > "${YUNETA_NODE_UUID_SYSROOT}/proc/1/environ"
run ensure
check "PID 1's container=, initial pid namespace: a container" "$(field container)" "lxc"
reset
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
set_pidns 4026532512
set_run_fs ""
set_marker run/systemd/container docker
run ensure
check "run/systemd/container in another pid namespace: names it" "$(field container)" "docker"

#
#   An image built in a chroot (over an exported Ubuntu rootfs, or a
#   debootstrap) runs in the initial pid namespace and does not see the
#   build host's /run, so no marker counts. But / is not the root of PID 1,
#   which is what `systemd-detect-virt --chroot` asks: the install stores
#   nothing there (exit 3), and a uuid made there all the same is not taken
#   by a board written from the image. A board has / as the root of PID 1.
#
set_pid1_root() {    # "same": / is the root of PID 1; "other": it is not;
                     # "hidden": it cannot be looked at (a link that leads nowhere)
    mkdir -p "${YUNETA_NODE_UUID_SYSROOT}/proc/1" "${WORK}/host-root"
    if [ "$1" = "same" ]; then
        ln -sfn ../.. "${YUNETA_NODE_UUID_SYSROOT}/proc/1/root"
    elif [ "$1" = "hidden" ]; then
        ln -sfn "${WORK}/no-such-root" "${YUNETA_NODE_UUID_SYSROOT}/proc/1/root"
    else
        ln -sfn "${WORK}/host-root" "${YUNETA_NODE_UUID_SYSROOT}/proc/1/root"
    fi
}
board ""
touch "${YUNETA_NODE_UUID_SYSROOT}/.dockerenv"
set_marker run/systemd/container docker
set_hostname "buildhost"
set_pid1_root other
check "chroot over an Ubuntu rootfs, install: exit 3" "$("$SCRIPT" ensure --installing >/dev/null 2>&1; echo $?)" "3"
check "chroot over an Ubuntu rootfs, install: nothing stored" "$( [ -e "$FILE" ] && echo stored || echo none)" "none"
run ensure
check "chroot: the host's hardware is not read" "$(field source)" "random"
check "chroot: recorded as a chroot" "$(field container)" "chroot"
check "chroot: show says it" "$("$SCRIPT" show 2>&1 | grep -c 'this is a chroot')" "1"
built="$(field uuid)"
rm -rf "${YUNETA_NODE_UUID_SYSROOT}/sys/class/dmi"
set_pid1_root same
out="$("$SCRIPT" ensure 2>&1)"
check "board from a chroot-built image, no identifier: a new random" "$( [ -n "$(field uuid)" ] && [ "$(field uuid)" != "$built" ] && echo changed)" "changed"
check "board from a chroot-built image, no identifier: no container recorded" "$(grep -c '"container"' "$FILE")" "0"
check "board from a chroot-built image, no identifier: copy of the image's" "$(backups)" "1"
check "board from a chroot-built image, no identifier: says why" "$(echo "$out" | grep -c 'this is no container')" "1"
board ""
set_pid1_root other
check "debootstrap chroot (no marker), install: exit 3" "$("$SCRIPT" ensure --installing >/dev/null 2>&1; echo $?)" "3"
check "debootstrap chroot (no marker), install: nothing stored" "$( [ -e "$FILE" ] && echo stored || echo none)" "none"
board ""
set_pid1_root same
run ensure
check "/ is the root of PID 1: the hardware" "$(field source)" "dmi-product-uuid"

#
#   PID 1's root cannot be looked at (a root without CAP_SYS_PTRACE, in a
#   user namespace, refused by a security module or the runtime, or no
#   root): the mount on / is compared, /proc/1/mountinfo against
#   /proc/self/mountinfo, device and root, never the mount id. The same one
#   is the system, whatever the image left in it (the box of V50.1.2: /run
#   on ext4 holding /run/systemd/container "docker", where a root with
#   `setpriv --bounding-set=-sys_ptrace` stored a random uuid as a container
#   and the next full root replaced it). Another one is a container's (or a
#   chroot's): its markers name it.
#
set_pid1_mount() {    # the mountinfo line of PID 1's / (a mountinfo of no / for "")
    mkdir -p "${YUNETA_NODE_UUID_SYSROOT}/proc/1"
    printf '29 1 0:26 / /proc rw - proc proc rw\n' > "${YUNETA_NODE_UUID_SYSROOT}/proc/1/mountinfo"
    if [ -n "$1" ]; then
        printf '%s\n' "$1" >> "${YUNETA_NODE_UUID_SYSROOT}/proc/1/mountinfo"
    fi
}
add_self_mount() {    # one more line of this process's mountinfo
    printf '%s\n' "$1" >> "${YUNETA_NODE_UUID_SYSROOT}/proc/self/mountinfo"
}
hidden_board() {    # an Ubuntu image on a board, /run on its root fs
    board ""
    touch "${YUNETA_NODE_UUID_SYSROOT}/.dockerenv"
    set_marker run/systemd/container docker
    set_hostname "board01"
    set_pid1_root hidden
}
hidden_board
set_pid1_mount '1 0 8:1 / / rw,relatime shared:1 - ext4 /dev/sda1 rw'
check "PID 1's root hidden, the same mount on /, install: the hardware" "$("$SCRIPT" ensure --installing >/dev/null 2>&1; echo $?)" "0"
check "PID 1's root hidden, the same mount on /: the hardware" "$(field source)" "dmi-product-uuid"
check "PID 1's root hidden, the same mount on /: no container recorded" "$(grep -c '"container"' "$FILE")" "0"
hidden_board
set_pid1_mount '7 0 8:1 / / rw,relatime shared:1 - ext4 /dev/sda1 rw'
run ensure
check "PID 1's root hidden, the same mount on / of another id (unshare -m): the hardware" "$(field source)" "dmi-product-uuid"
hidden_board
set_pid1_mount '1 0 8:2 / / rw,relatime shared:1 - ext4 /dev/sda2 rw'
check "PID 1's root hidden, another mount on /, install: exit 3" "$("$SCRIPT" ensure --installing >/dev/null 2>&1; echo $?)" "3"
run ensure
check "PID 1's root hidden, another device on /: a container, named by its markers" "$(field container)" "docker"
hidden_board
set_pid1_mount '1 0 8:1 /@ / rw,relatime shared:1 - btrfs /dev/sda1 rw'
run ensure
check "PID 1's root hidden, another root of the same device on /: a container" "$(field container)" "docker"
hidden_board
set_pid1_mount '1 0 8:1 / / rw,relatime shared:1 - ext4 /dev/sda1 rw'
add_self_mount '40 22 0:50 / / rw,relatime - overlay overlay rw'
run ensure
check "PID 1's root hidden, an overlay mounted over its / (the last one counts): a container" "$(field container)" "docker"
hidden_board
set_pid1_mount '1 0 8:1 / / rw,relatime shared:1 - ext4 /dev/sda1 rw'
printf '23 1 0:26 / /proc rw - proc proc rw\n' > "${YUNETA_NODE_UUID_SYSROOT}/proc/self/mountinfo"
run ensure
check "PID 1's root hidden, no mount on this / (a chroot into a directory): a container" "$(field container)" "docker"
hidden_board
set_pid1_mount ""
run ensure
check "PID 1's root hidden, no / in PID 1's mountinfo: as the system" "$(field source)" "dmi-product-uuid"
hidden_board
rm -f "${YUNETA_NODE_UUID_SYSROOT}/proc/1/mountinfo"
run ensure
check "PID 1's root hidden, PID 1's mountinfo unreadable: as the system" "$(field source)" "dmi-product-uuid"

#
#   The same against the real kernel, as a normal user. The root of a user
#   namespace cannot look at PID 1's root, as a root without CAP_SYS_PTRACE
#   cannot (Docker's), nor one refused by a security module or by the
#   runtime: the mount on / tells then. On the system's own / it is the
#   system, in a private mount namespace too (a copy of the same mount):
#
#       unshare -Ur  <the script's container_name>
#       unshare -Urm <the script's container_name>
#
#   Another / is a container run with --pid=host, as Toolbx and distrobox
#   run theirs (rootless Podman: a user and a mount namespace, the image as
#   /), or a chroot: emulated with a chroot entered in a user and mount
#   namespace, with and without /proc (with no /proc at all no system was
#   booted there):
#
#       unshare -Urm sh -c 'mount --rbind /usr R/usr; mount --rbind /dev R/dev;
#                           mount --rbind /proc R/proc; chroot R <its container_name>'
#
#   A chroot entered by the REAL root (the image build) sees PID 1's root
#   and it is not / -- the fake "other" above; checked by hand with
#   `sudo unshare -m` + the same mounts + chroot (it answers "chroot"). A
#   real root with no CAP_SYS_PTRACE on the box itself is checked by hand
#   too, in `sudo unshare -m --propagation private` with a ramfs on /run
#   holding /run/systemd/container "docker":
#   `setpriv --bounding-set=-sys_ptrace --inh-caps=-sys_ptrace` answers no
#   container, and its `ensure` stores the DMI uuid (it stored a random of
#   "container": "docker" before the mount on / was compared).
#   Skipped where unprivileged user namespaces are refused.
#
probe_container() {    # the script's container_name, "none" for none
    local v
    v="$(bash -c 'eval "$(sed -n "1,/^RESTART_NOTE=/p" "$1")"; container_name' _ "$1" 2>/dev/null)"
    printf '%s' "${v:-none}"
}
if unshare -Ur true >/dev/null 2>&1; then
    check "a normal user in the initial pid namespace: nothing told" \
        "$(unset container; export YUNETA_NODE_UUID_SYSROOT="$EMU"; probe_container "$SCRIPT")" "none"
    check "a root in a user namespace on the system's own / (unshare -Ur): the system" \
        "$(env -u container YUNETA_NODE_UUID_SYSROOT="$EMU" \
            unshare -Ur bash -c "$(declare -f probe_container); probe_container \"\$1\"" _ "$SCRIPT")" "none"
    if unshare -Urm true >/dev/null 2>&1; then
        check "a root in a user and mount namespace on the system's own / (unshare -Urm): the system" \
            "$(env -u container YUNETA_NODE_UUID_SYSROOT="$EMU" \
                unshare -Urm bash -c "$(declare -f probe_container); probe_container \"\$1\"" _ "$SCRIPT")" "none"
    fi
else
    echo "skip - unshare -Ur refused here: no emulation of a root that cannot look at PID 1's root"
fi
CHR="${WORK}/chroot"
if [ -L /bin ] && [ -L /lib ] && unshare -Urm true >/dev/null 2>&1; then
    mkdir -p "${CHR}/usr" "${CHR}/dev" "${CHR}/proc"
    for d in bin sbin lib lib64 lib32 libx32; do
        if [ -L "/$d" ]; then
            ln -s "$(readlink "/$d")" "${CHR}/$d"
        fi
    done
    cp "$SCRIPT" "${CHR}/yuneta-node-uuid"
    in_chroot() {    # with /proc (0|1)
        local probe
        probe="$(declare -f probe_container); probe_container /yuneta-node-uuid"
        env -u container -u YUNETA_NODE_UUID_SYSROOT YUNETA_NODE_UUID_DETECT_VIRT=none-here \
            unshare -Urm bash -c '
                if ! mount --rbind /usr "$1/usr" || ! mount --rbind /dev "$1/dev"; then
                    exit 1
                fi
                if [ "$2" = "1" ] && ! mount --rbind /proc "$1/proc"; then
                    exit 1
                fi
                exec chroot "$1" /usr/bin/bash -c "$3"
            ' _ "$CHR" "$1" "$probe" 2>/dev/null
    }
    check "another / in a user namespace (a --pid=host Toolbx, or a chroot), /proc mounted: a container" "$(in_chroot 1)" "container-other"
    check "chroot in a user namespace, no /proc: a chroot" "$(in_chroot 0)" "chroot"
else
    echo "skip - no merged /usr, or unshare -Urm refused here: no chroot emulation"
fi

#
#   PID 1's environment that may not be opened writes nothing on stderr: root
#   passes `-r` and the open is refused all the same (a --pid=host container),
#   and an error of the redirection is written before a `2>/dev/null` that
#   follows it. A socket stands for it here: `-r` passes, the open fails
#   (ENXIO). Every postinst and agent unit showed "/proc/1/environ:
#   Permission denied".
#
if command -v python3 >/dev/null 2>&1; then
    board ""
    mkdir -p "${YUNETA_NODE_UUID_SYSROOT}/proc/1"
    python3 -c 'import socket, sys; socket.socket(socket.AF_UNIX).bind(sys.argv[1])' \
        "${YUNETA_NODE_UUID_SYSROOT}/proc/1/environ"
    check "PID 1's environment cannot be opened: nothing on stderr" \
        "$("$SCRIPT" ensure 2>&1 >/dev/null | wc -l)" "0"
    check "PID 1's environment cannot be opened: the hardware" "$(field source)" "dmi-product-uuid"
else
    echo "skip - no python3 to make a socket: no unopenable environment"
fi

#
#   A uuid of the hardware is never moved by a "container" verdict, which
#   reads no hardware: a root that took itself for a container must not
#   change a node (the ethernet-mac case; the DMI one is above)
#
reset
add_nic eth0 "aa:bb:cc:00:00:01" 0
run ensure
macnode="$(field uuid)"
check "ethernet-mac node: source" "$(field source)" "ethernet-mac"
touch "${YUNETA_NODE_UUID_SYSROOT}/.dockerenv"
set_pidns 4026532512
run ensure
check "ethernet-mac node seen as a container: uuid kept" "$(field uuid)" "$macnode"
check "ethernet-mac node seen as a container: no copy" "$(backups)" "0"

#
#   No here-string, no here-document: bash before 5.1 writes them to a
#   temporary file, and a read-only /tmp (a chroot, an image build) turns
#   that into a wrong answer
#
check "no here-string or here-document in the script" "$(grep -c '<<' "$SCRIPT")" "0"

#
#   The install in a container says, by its exit status, that it stored
#   nothing (3): the package then does not start the agents, whose start
#   would store the uuid in the image being built. With a uuid stored, or
#   on a box, 0
#
reset
touch "${YUNETA_NODE_UUID_SYSROOT}/.dockerenv"
set_hostname "buildkitsandbox"
check "install in a container, nothing stored: exit 3" "$("$SCRIPT" ensure --installing >/dev/null 2>&1; echo $?)" "3"
run ensure
check "install in a container, a uuid stored: exit 0" "$("$SCRIPT" ensure --installing >/dev/null 2>&1; echo $?)" "0"
reset
set_dmi "4C4C4544-0042-3510-8051-B4C04F4A4E32"
check "install on a box: exit 0" "$("$SCRIPT" ensure --installing >/dev/null 2>&1; echo $?)" "0"
check "install on a box: stored" "$(field uuid)" "$first"

#
#   A container with no hostname (empty, or the kernel's "(none)"): its
#   random belongs to no hostname, and says so; kept across its restarts,
#   replaced in a copy that has one
#
for nohost in "" "(none)"; do
    reset
    touch "${YUNETA_NODE_UUID_SYSROOT}/.dockerenv"
    set_hostname "$nohost"
    run ensure
    nh="$(field uuid)"
    check "container, hostname '$nohost': random" "$(field source)" "random"
    check "container, hostname '$nohost': recorded as none" "$(grep -c '"hostname": ""' "$FILE")" "1"
    run ensure
    check "container, hostname '$nohost': kept across its restarts" "$(field uuid)" "$nh"
    set_hostname "c0ffee000003"
    run ensure
    check "container, hostname '$nohost': a copy with a hostname makes its own" "$( [ "$(field uuid)" != "$nh" ] && echo changed)" "changed"
    check "container, hostname '$nohost': copy kept" "$(backups)" "1"
done

#
#   ... and the same for a file of 7.29.3, which wrote no "hostname" line
#   when there was none
#
reset
touch "${YUNETA_NODE_UUID_SYSROOT}/.dockerenv"
set_hostname "c0ffee000005"
mkdir -p "$YUNETA_NODE_UUID_DIR"
printf '{\n    "uuid": "%s",\n    "source": "random",\n    "container": "docker",\n    "created": "2026-10-28T00:00:00Z"\n}\n' \
    "ff14fc16-2cb2-472c-9c0a-89fa43837132" > "$FILE"
run ensure
check "container, 7.29.3 random of no hostname: replaced" "$( [ "$(field uuid)" != "ff14fc16-2cb2-472c-9c0a-89fa43837132" ] && echo changed)" "changed"

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
