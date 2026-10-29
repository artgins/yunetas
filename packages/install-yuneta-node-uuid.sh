#!/bin/bash
#
#   install-yuneta-node-uuid.sh -- the node uuid from the hardware, on a node
#   that does not install the package
#
#   The .deb / .rpm install /usr/libexec/yuneta/yuneta-node-uuid (see
#   templates/yuneta-node-uuid) and run it from both agent units. A node that
#   builds from source does not install the package -- it would replace
#   outputs/ with archives built against another glibc -- so it keeps the
#   random uuid node_uuid() wrote. This script does by hand what the package
#   does, and nothing else:
#
#     1. installs the script as /usr/libexec/yuneta/yuneta-node-uuid, root's,
#        0755 (restorecon where SELinux runs: /usr/libexec is bin_t);
#     2. makes both agent units run it before they start: a drop-in
#        /etc/systemd/system/<unit>.service.d/yuneta-node-uuid.conf, unless
#        the unit itself already does (the package's unit);
#     3. runs `yuneta-node-uuid ensure` and says the uuid before and after.
#
#   IDEMPOTENT: each step writes only what differs. A second run changes
#   nothing, says so, and reports the same uuid -- which is also the way to
#   check a node, with or without the package: run it again.
#
#   When the uuid changes, the agents and the yunos keep the old one until
#   they restart (they read it when they start). Anything that keys data or
#   declarations on the node uuid has to follow -- or keep the old one with
#   `yuneta-node-uuid set <old uuid>` before running this.
#
#   Usage:
#       sudo packages/install-yuneta-node-uuid.sh            install / check
#       sudo packages/install-yuneta-node-uuid.sh -n         say what it would do
#
#   Exit: 0 done (changed or not), 1 failed, 2 usage.
#
#   Copyright (c) 2026, ArtGins.
#
set -u

DRY_RUN=0
case "${1:-}" in
    "")             ;;
    -n|--dry-run)   DRY_RUN=1 ;;
    -h|--help)      sed -n '2,/^#   Copyright/p' "$0" | sed 's/^#\{1\} \{0,1\}//'; exit 0 ;;
    *)              echo "usage: $0 [-n|--dry-run]" >&2; exit 2 ;;
esac

if [ "$(id -u)" != "0" ]; then
    echo "[-] Run it as root: the hardware identifiers (DMI) and the units are root's" >&2
    exit 1
fi

SRC="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/templates/yuneta-node-uuid"
DST_DIR="/usr/libexec/yuneta"
DST="${DST_DIR}/yuneta-node-uuid"
UUID_FILE="/yuneta/store/agent/uuid/uuid.json"
UNITS="yuneta_agent yuneta_agent22"
DROPIN_NAME="yuneta-node-uuid.conf"
DROPIN_BODY="# Written by packages/install-yuneta-node-uuid.sh: what the package's unit
# carries, for a node that builds from source. See yuneta-node-uuid.
[Service]
ExecStartPre=-+${DST} ensure
"

if [ ! -s "$SRC" ]; then
    echo "[-] Missing $SRC: run this from a yunetas checkout" >&2
    exit 1
fi

CHANGED=0
say() { echo "[i] $*"; }
act() {
    # A change: done, or only told in a dry run
    CHANGED=1
    if [ "$DRY_RUN" = "1" ]; then
        echo "[n] would: $*"
        return 1
    fi
    echo "[+] $*"
    return 0
}

stored_uuid() {
    if [ -r "$UUID_FILE" ]; then
        sed -n 's/.*"uuid"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$UUID_FILE" | head -n1
    fi
}

BEFORE="$(stored_uuid)"
say "node uuid now: ${BEFORE:-(none)}"

#
#   1. The script, root's
#
if [ -f "$DST" ] && cmp -s "$SRC" "$DST" &&
        [ "$(stat -c '%u:%g %a' "$DST")" = "0:0 755" ]; then
    say "$DST is current"
else
    if act "install $DST (root:root 0755)"; then
        install -d -o root -g root -m 0755 "$DST_DIR" || exit 1
        install -o root -g root -m 0755 "$SRC" "$DST" || exit 1
    fi
fi
if command -v selinuxenabled >/dev/null 2>&1 && selinuxenabled &&
        command -v restorecon >/dev/null 2>&1 && [ -e "$DST" ]; then
    if [ "$DRY_RUN" = "0" ]; then
        restorecon -R "$DST_DIR" || exit 1
    fi
    say "SELinux label: $(ls -Z "$DST" 2>&1 | awk '{print $1}')"
fi

#
#   2. Both agent units run it before they start
#
RELOAD=0
if ! command -v systemctl >/dev/null 2>&1 || [ ! -d /run/systemd/system ]; then
    say "no systemd: whatever starts the agents must run '$DST ensure' first"
else
    for unit in $UNITS; do
        fragment="$(systemctl show -p FragmentPath --value "${unit}.service" 2>/dev/null)"
        dropin_dir="/etc/systemd/system/${unit}.service.d"
        dropin="${dropin_dir}/${DROPIN_NAME}"
        if [ -z "$fragment" ] || [ ! -f "$fragment" ]; then
            say "${unit}: no systemd unit, skipped"
            continue
        fi
        if grep -q "yuneta-node-uuid" "$fragment"; then
            say "${unit}: its unit already runs it ($fragment)"
            if [ -f "$dropin" ] && act "remove $dropin (the unit does it; twice is noise)"; then
                rm -f "$dropin" || exit 1
                rmdir --ignore-fail-on-non-empty "$dropin_dir"
                RELOAD=1
            fi
            continue
        fi
        if [ -f "$dropin" ] && [ "$(cat "$dropin")" = "$(printf '%s' "$DROPIN_BODY")" ]; then
            say "${unit}: drop-in is current ($dropin)"
            continue
        fi
        if act "write $dropin"; then
            install -d -o root -g root -m 0755 "$dropin_dir" || exit 1
            printf '%s' "$DROPIN_BODY" > "$dropin" || exit 1
            chmod 0644 "$dropin"
            RELOAD=1
        fi
    done
    if [ "$RELOAD" = "1" ]; then
        systemctl daemon-reload || exit 1
        say "systemctl daemon-reload"
    fi
fi

#
#   3. The uuid
#
if [ "$DRY_RUN" = "1" ]; then
    # What it would give, from the new script but never run from the
    # checkout as root: a root-owned copy.
    tmp="$(mktemp)" || exit 1
    install -m 0755 "$SRC" "$tmp" && "$tmp" show
    rm -f "$tmp"
    if [ "$CHANGED" = "0" ]; then
        say "nothing would change but, maybe, the uuid: compare 'uuid' and 'hardware' above"
    fi
    exit 0
fi

if ! "$DST" ensure; then
    echo "[-] '$DST ensure' failed: the node keeps the uuid it had" >&2
    exit 1
fi
AFTER="$(stored_uuid)"
"$DST" show

if [ "$BEFORE" != "$AFTER" ]; then
    CHANGED=1
    echo "[+] node uuid ${BEFORE:-(none)} -> ${AFTER}"
    echo "[!] The agents and the yunos keep the old uuid until they restart:"
    echo "    'sudo systemctl restart yuneta_agent', then yuneta_agent22 (one at"
    echo "    a time; 'yunetas restart-agents' does nothing here: the binary did"
    echo "    not change), then the yunos ('kill-yuno' and 'run-yuno')."
fi
if [ "$CHANGED" = "0" ]; then
    say "Nothing changed: node uuid ${AFTER}"
fi
exit 0
