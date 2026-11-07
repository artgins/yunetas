#!/bin/bash
#
#   install-yuneta-agent-cert.sh -- this node's own agent certificate, on a
#   node that does not install the package
#
#   The .deb / .rpm install /usr/libexec/yuneta/yuneta-agent-cert (see
#   templates/yuneta-agent-cert) and run it from the agent's unit. A node that
#   builds from source makes its pair at each build of the agent, but the
#   unit does not ask for it at each start until it has the script. This
#   script does by hand what the package does, and nothing else:
#
#     1. installs the script as /usr/libexec/yuneta/yuneta-agent-cert, root's,
#        0755 (restorecon where SELinux runs: /usr/libexec is bin_t);
#     2. makes the agent's unit run it before it starts: a drop-in
#        /etc/systemd/system/yuneta_agent.service.d/yuneta-agent-cert.conf,
#        unless the unit itself already does (the package's unit);
#     3. runs `yuneta-agent-cert ensure` and says the certificate before and
#        after.
#
#   IDEMPOTENT: each step writes only what differs. A second run changes
#   nothing and says so.
#
#   When the pair changes, the agent serves the old one until it restarts,
#   and the clients of its port 1993 that trusted the old certificate need
#   the node's new one (it is public: copy /yuneta/agent/certs/yuneta_agent.crt).
#
#   Usage:
#       sudo packages/install-yuneta-agent-cert.sh            install / check
#       sudo packages/install-yuneta-agent-cert.sh -n         say what it would do
#
#   Exit: 0 done (changed or not), 1 failed, 2 usage.
#
#   Copyright (c) 2026, ArtGins.
#
set -u

DRY_RUN=0
case "${1:-}" in
    "")
        ;;
    -n|--dry-run)
        DRY_RUN=1
        ;;
    -h|--help)
        sed -n '2,/^#   Copyright/p' "$0" | sed 's/^#\{1\} \{0,1\}//'
        exit 0
        ;;
    *)
        echo "usage: $0 [-n|--dry-run]" >&2
        exit 2
        ;;
esac

if [ "$(id -u)" != "0" ]; then
    echo "[-] Run it as root: the units and /usr/libexec are root's" >&2
    exit 1
fi

SRC="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/templates/yuneta-agent-cert"
DST_DIR="/usr/libexec/yuneta"
DST="${DST_DIR}/yuneta-agent-cert"
CRT="/yuneta/agent/certs/yuneta_agent.crt"
UNITS="yuneta_agent"
DROPIN_NAME="yuneta-agent-cert.conf"
DROPIN_BODY="# Written by packages/install-yuneta-agent-cert.sh: what the package's unit
# carries, for a node that builds from source. See yuneta-agent-cert.
[Service]
ExecStartPre=-+${DST} ensure
"

if [ ! -s "$SRC" ]; then
    echo "[-] Missing $SRC: run this from a yunetas checkout" >&2
    exit 1
fi

CHANGED=0
say() {
    echo "[i] $*"
}
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

cert_fpr() {
    local ossl=/yuneta/development/yunetas/outputs_ext/bin/openssl
    if [ ! -x "$ossl" ]; then
        ossl="$(command -v openssl)"
    fi
    if [ -r "$CRT" ] && [ -n "$ossl" ]; then
        "$ossl" x509 -in "$CRT" -noout -fingerprint -sha256 2>/dev/null | sed 's/.*=//'
    fi
}

BEFORE="$(cert_fpr)"
say "agent certificate now: ${BEFORE:-(none)}"

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
#   2. The agent's unit runs it before it starts
#
RELOAD=0
if ! command -v systemctl >/dev/null 2>&1 || [ ! -d /run/systemd/system ]; then
    say "no systemd: whatever starts the agents must run '$DST ensure' first"
else
    for unit in $UNITS; do
        # A unit that does not exist answers an empty path, and no error
        if ! fragment="$(systemctl show -p FragmentPath --value "${unit}.service")"; then
            echo "[-] systemctl show ${unit}.service failed" >&2
            exit 1
        fi
        dropin_dir="/etc/systemd/system/${unit}.service.d"
        dropin="${dropin_dir}/${DROPIN_NAME}"
        if [ -z "$fragment" ] || [ ! -f "$fragment" ]; then
            say "${unit}: no systemd unit, skipped"
            continue
        fi
        if grep -q "yuneta-agent-cert" "$fragment"; then
            say "${unit}: its unit already runs it ($fragment)"
            if [ -f "$dropin" ] && act "remove $dropin (the unit does it; twice is noise)"; then
                rm -f "$dropin" || exit 1
                if ! rmdir --ignore-fail-on-non-empty "$dropin_dir"; then
                    echo "[!] cannot remove $dropin_dir: left there, empty" >&2
                fi
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
            chmod 0644 "$dropin" || exit 1
            RELOAD=1
        fi
    done
    if [ "$RELOAD" = "1" ]; then
        systemctl daemon-reload || exit 1
        say "systemctl daemon-reload"
    fi
fi

#
#   3. The pair
#
if [ "$DRY_RUN" = "1" ]; then
    tmp="$(mktemp)" || exit 1
    if ! install -m 0755 "$SRC" "$tmp"; then
        rm -f "$tmp"
        exit 1
    fi
    if ! "$tmp" show; then
        echo "[!] '$SRC show' failed: what it would do is unknown" >&2
    fi
    rm -f "$tmp"
    if [ "$CHANGED" = "0" ]; then
        say "nothing would change but, maybe, the pair: see 'state' above"
    fi
    exit 0
fi

if ! "$DST" ensure; then
    echo "[-] '$DST ensure' failed: the agent keeps the pair it had" >&2
    exit 1
fi
AFTER="$(cert_fpr)"
if ! "$DST" show; then
    echo "[!] '$DST show' failed" >&2
fi

if [ "$BEFORE" != "$AFTER" ]; then
    CHANGED=1
    echo "[+] agent certificate ${BEFORE:-(none)} -> ${AFTER}"
    echo "[!] The agent serves the old pair until it restarts:"
    if command -v systemctl >/dev/null 2>&1 && systemctl is-active --quiet yuneta_agent; then
        echo "      sudo systemctl restart yuneta_agent"
    else
        echo "      (outside systemd, as yuneta)"
        echo "      /yuneta/agent/yuneta_agent --config-file=/yuneta/agent/yuneta_agent.json --stop"
        echo "      /yuneta/agent/yuneta_agent --start --config-file=/yuneta/agent/yuneta_agent.json"
    fi
    echo "    and whatever else serves those files; the clients of port 1993"
    echo "    that trusted the old certificate need this one: $CRT"
fi
if [ "$CHANGED" = "0" ]; then
    say "Nothing changed: agent certificate ${AFTER}"
fi
exit 0
