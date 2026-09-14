#!/usr/bin/env bash
#
#   audit-sshd.sh
#
#   Read what sshd runs with and report failures and possible improvements.
#   It changes NOTHING: every fix it proposes is the operator's decision.
#
#   It reads `sshd -T` -- the values sshd will use, whatever file set them --
#   and not sshd_config, which says only part of it (drop-ins, distribution
#   defaults and crypto policies change the rest). Then the files around it
#   (permissions, host keys), the last hour of sshd's journal (is a flood
#   happening?) and fail2ban.
#
#   Levels:
#       FAIL  lets an attacker in, or leaves the door ajar. Fix it.
#       WARN  a real weakness, or a flood happening now.
#       INFO  a possible improvement; weigh it against how the node is used.
#       OK    shown only with --all.
#
#   Exit status: 2 if any FAIL, 1 if any WARN, 0 otherwise.
#
#   `sshd -T` prints the GLOBAL values. A `Match` block can change them for
#   some users or addresses; the script says when there is one, and
#   `sshd -T -C user=U,host=H,addr=A` shows what applies to one connection.
#
#   Usage (as root, or through sudo):
#       audit-sshd.sh          failures and improvements
#       audit-sshd.sh --all    also what is already right
#
set -euo pipefail

SSHD=/usr/sbin/sshd
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

#
#   Helpers
#
usage() {
    cat <<'EOF'
Usage (as root, or through sudo):
    audit-sshd.sh          failures and improvements
    audit-sshd.sh --all    also what is already right
EOF
}

need_root() {
    if [ "${EUID:-$(id -u)}" -ne 0 ]; then
        if command -v sudo >/dev/null 2>&1; then
            exec sudo -E bash "$0" "$@"
        fi
        echo "This script must run as root (or install sudo)." >&2
        exit 1
    fi
}

N_FAIL=0
N_WARN=0
N_INFO=0
SHOW_OK=0

report() {
    local level="$1"
    shift
    case "$level" in
        FAIL)
            N_FAIL=$((N_FAIL + 1))
            ;;
        WARN)
            N_WARN=$((N_WARN + 1))
            ;;
        INFO)
            N_INFO=$((N_INFO + 1))
            ;;
        OK)
            if [ "$SHOW_OK" -eq 0 ]; then
                return 0
            fi
            ;;
    esac
    printf '[%-4s] %s\n' "$level" "$*"
}

section() {
    echo
    echo "== $*"
}

# First value of a key in `sshd -T` (keys there are lower case).
get() {
    awk -v k="$1" '$1 == k { $1 = ""; sub(/^ /, ""); print; exit }' <<<"$EFFECTIVE"
}

# Every value of a key that can repeat (hostkey, port, ...).
get_all() {
    awk -v k="$1" '$1 == k { $1 = ""; sub(/^ /, ""); print }' <<<"$EFFECTIVE"
}

# Items of a comma list that match an extended regex, comma-joined.
matching() {
    tr ',' '\n' <<<"$1" | grep -E "$2" | paste -sd, - || true
}

# Octal mode of a file, e.g. 600.
mode_of() {
    stat -c '%a' "$1"
}

sshd_unit() {
    local u
    for u in ssh sshd; do
        if systemctl cat "${u}.service" >/dev/null 2>&1; then
            echo "$u"
            return 0
        fi
    done
    return 1
}

#
#   Arguments
#
case "${1:-}" in
    "")
        ;;
    --all)
        SHOW_OK=1
        ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

need_root "$@"

if [ ! -x "$SSHD" ]; then
    echo "ERROR: $SSHD not found: is openssh-server installed?" >&2
    exit 2
fi
if ! "$SSHD" -t; then
    echo "ERROR: sshd -t fails: sshd would not start with this configuration. That is the first thing to fix." >&2
    exit 2
fi
EFFECTIVE="$("$SSHD" -T 2>/dev/null)" || true
if [ -z "$EFFECTIVE" ]; then
    echo "ERROR: sshd -T printed nothing: the effective configuration could not be read" >&2
    exit 2
fi

echo "sshd audit of $(hostname) -- $(ssh -V 2>&1)"

#
#   Who can log in, and how
#
section "Authentication"

v="$(get passwordauthentication)"
if [ "$v" = "yes" ]; then
    report FAIL "PasswordAuthentication yes: a botnet can try passwords. Fix: PasswordAuthentication no, once key login is confirmed to work"
else
    report OK "PasswordAuthentication $v"
fi

v="$(get kbdinteractiveauthentication)"
if [ "$v" = "yes" ]; then
    report FAIL "KbdInteractiveAuthentication yes: with UsePAM it is password login by another name. Fix: KbdInteractiveAuthentication no"
else
    report OK "KbdInteractiveAuthentication ${v:-(unset)}"
fi

v="$(get permitemptypasswords)"
if [ "$v" = "yes" ]; then
    report FAIL "PermitEmptyPasswords yes: an account with no password logs in with nothing. Fix: PermitEmptyPasswords no"
else
    report OK "PermitEmptyPasswords $v"
fi

v="$(get permitrootlogin)"
case "$v" in
    yes)
        report FAIL "PermitRootLogin yes: root is the one user name every botnet tries. Fix: PermitRootLogin no (or prohibit-password)"
        ;;
    prohibit-password|without-password)
        report INFO "PermitRootLogin $v: root logs in with a key. With a sudo user in place, PermitRootLogin no takes root off the table"
        ;;
    *)
        report OK "PermitRootLogin $v"
        ;;
esac

v="$(get pubkeyauthentication)"
if [ "$v" != "yes" ]; then
    report WARN "PubkeyAuthentication $v: keys are refused, so logins rely on something weaker"
else
    report OK "PubkeyAuthentication yes"
fi

v="$(get hostbasedauthentication)"
if [ "$v" = "yes" ]; then
    report WARN "HostbasedAuthentication yes: trusts other hosts' word for who the user is. Fix: HostbasedAuthentication no"
fi

v="$(get ignorerhosts)"
if [ "$v" = "no" ]; then
    report WARN "IgnoreRhosts no: .rhosts files are honoured. Fix: IgnoreRhosts yes"
fi

v="$(get permituserenvironment)"
if [ "$v" != "no" ]; then
    report WARN "PermitUserEnvironment $v: users can set LD_PRELOAD and the like for sshd's children. Fix: PermitUserEnvironment no"
fi

v="$(get strictmodes)"
if [ "$v" != "yes" ]; then
    report WARN "StrictModes $v: sshd accepts authorized_keys that others can write. Fix: StrictModes yes"
fi

v="$(get maxauthtries)"
if [ -n "$v" ] && [ "$v" -gt 6 ]; then
    report WARN "MaxAuthTries $v: more tries per connection than the default 6"
fi

allow="$(get allowusers)$(get allowgroups)"
if [ -z "$allow" ]; then
    report INFO "No AllowUsers/AllowGroups: every account with a shell can try. Listing the accounts that log in (e.g. AllowUsers yuneta) closes the rest"
else
    report OK "AllowUsers/AllowGroups set"
fi

v="$(get gssapiauthentication)"
if [ "$v" = "yes" ]; then
    report INFO "GSSAPIAuthentication yes: Kerberos login, unused on most nodes. GSSAPIAuthentication no removes that code path"
fi

v="$(get x11forwarding)"
if [ "$v" = "yes" ]; then
    report INFO "X11Forwarding yes: a server rarely needs it. X11Forwarding no"
fi

#
#   Staying reachable under a flood
#
section "Availability under a connection flood"

v="$(get logingracetime)"
if [ -n "$v" ] && [ "$v" -gt 60 ]; then
    report WARN "LoginGraceTime $v: an unauthenticated connection holds its slot ${v}s. See install-sshd-flood-guard.sh (20 s)"
else
    report OK "LoginGraceTime $v"
fi

v="$(get maxstartups)"
start="${v%%:*}"
if [ -n "$start" ] && [ "$start" -le 10 ]; then
    report WARN "MaxStartups $v: past $start unauthenticated connections sshd drops new ones AT RANDOM, yours included. See install-sshd-flood-guard.sh (50:30:200)"
else
    report OK "MaxStartups $v"
fi

v="$(get persourcemaxstartups)"
if [ -z "$v" ]; then
    report INFO "PerSourceMaxStartups unknown to this sshd (OpenSSH older than 8.5)"
elif [ "$v" = "none" ]; then
    report INFO "PerSourceMaxStartups none: one address can take every slot. See install-sshd-flood-guard.sh (10)"
else
    report OK "PerSourceMaxStartups $v"
fi

v="$(get persourcepenalties)"
if [ -z "$v" ]; then
    report INFO "PerSourcePenalties unknown to this sshd (OpenSSH older than 9.8): no built-in penalty for addresses that fail"
elif [ "$v" = "no" ]; then
    report WARN "PerSourcePenalties no: addresses that fail or never authenticate are not held back"
else
    report OK "PerSourcePenalties $v"
fi

v="$(get clientaliveinterval)"
if [ "$v" = "0" ]; then
    report INFO "ClientAliveInterval 0: a session whose client vanished stays open until TCP gives up. ClientAliveInterval 300 closes it"
fi

v="$(get usedns)"
if [ "$v" = "yes" ]; then
    report INFO "UseDNS yes: every connection waits for a reverse lookup, which a flood multiplies. UseDNS no"
fi

v="$(get loglevel)"
case "$v" in
    QUIET|FATAL|ERROR)
        report WARN "LogLevel $v: failed logins are not logged, so fail2ban sees nothing. LogLevel INFO or VERBOSE"
        ;;
    INFO)
        report INFO "LogLevel INFO: VERBOSE also logs the fingerprint of the key each login used"
        ;;
    *)
        report OK "LogLevel $v"
        ;;
esac

ports="$(get_all port | paste -sd, -)"
if [ "$ports" = "22" ]; then
    report INFO "Port 22: where botnets knock. Another port cuts the noise (not the risk); a source allowlist cuts both"
else
    report OK "Port $ports"
fi

#
#   Algorithms
#
section "Algorithms and host keys"

weak="$(matching "$(get ciphers)" '(-cbc$|^arcfour|^3des|^blowfish|^cast128)')"
if [ -n "$weak" ]; then
    report WARN "Weak ciphers enabled: $weak"
else
    report OK "No weak ciphers"
fi

weak="$(matching "$(get macs)" '(md5|ripemd|-96)')"
if [ -n "$weak" ]; then
    report WARN "Weak MACs enabled: $weak"
fi
weak="$(matching "$(get macs)" '(hmac-sha1|umac-64)')"
if [ -n "$weak" ]; then
    report INFO "SHA-1 / 64-bit MACs enabled: $weak (not broken as MACs; drop them when no old client needs them)"
fi

weak="$(matching "$(get kexalgorithms)" '(group1-sha1|group14-sha1|group-exchange-sha1)')"
if [ -n "$weak" ]; then
    report WARN "Weak key exchange enabled: $weak"
else
    report OK "No weak key exchange"
fi

for key in hostkeyalgorithms pubkeyacceptedalgorithms pubkeyacceptedkeytypes; do
    weak="$(matching "$(get "$key")" '^(ssh-rsa|ssh-dss)$')"
    if [ -n "$weak" ]; then
        report WARN "$key accepts SHA-1 signatures: $weak"
    fi
done

while read -r hostkey; do
    if [ -z "$hostkey" ] || [ ! -f "$hostkey" ]; then
        continue
    fi
    info="$(ssh-keygen -l -f "$hostkey" 2>/dev/null || true)"
    bits="${info%% *}"
    case "$info" in
        *"(DSA)"*)
            report WARN "Host key $hostkey is DSA: remove it and its HostKey line"
            ;;
        *"(RSA)"*)
            if [ -n "$bits" ] && [ "$bits" -lt 3072 ]; then
                report WARN "Host key $hostkey is RSA $bits bits: regenerate it with 3072 or more"
            fi
            ;;
    esac
    mode="$(mode_of "$hostkey")"
    if [ $((8#$mode & 8#037)) -ne 0 ]; then
        report FAIL "Host key $hostkey has mode $mode: others can read the key that proves who this server is. Fix: chmod 600 (640 root:ssh_keys on RHEL)"
    fi
done < <(get_all hostkey)

#
#   Files
#
section "Files"

for f in /etc/ssh/sshd_config /etc/ssh/sshd_config.d/*.conf; do
    if [ ! -f "$f" ]; then
        continue
    fi
    owner="$(stat -c '%U' "$f")"
    mode="$(mode_of "$f")"
    if [ "$owner" != "root" ] || [ $((8#$mode & 8#022)) -ne 0 ]; then
        report FAIL "$f is $owner, mode $mode: whoever can write it decides who logs in. Fix: chown root and chmod go-w"
    else
        report OK "$f ($owner, $mode)"
    fi
done

if ! grep -qiE '^[[:space:]]*Include[[:space:]]+/etc/ssh/sshd_config\.d/' /etc/ssh/sshd_config; then
    report INFO "sshd_config does not Include /etc/ssh/sshd_config.d/: drop-ins there are ignored"
fi

if grep -qiE '^[[:space:]]*Match[[:space:]]' /etc/ssh/sshd_config /etc/ssh/sshd_config.d/*.conf 2>/dev/null; then
    report INFO "There are Match blocks: the values above are the global ones. sshd -T -C user=U,host=H,addr=A shows one connection's"
fi

for user in root yuneta; do
    home="$(getent passwd "$user" | cut -d: -f6 || true)"
    if [ -z "$home" ]; then
        continue
    fi
    for f in "$home" "$home/.ssh" "$home/.ssh/authorized_keys"; do
        if [ ! -e "$f" ]; then
            continue
        fi
        mode="$(mode_of "$f")"
        if [ $((8#$mode & 8#022)) -ne 0 ]; then
            report WARN "$f has mode $mode: others can write the keys that let $user in. Fix: chmod go-w"
        fi
    done
done

#
#   What is happening now
#
section "Last hour in sshd's journal"

if unit="$(sshd_unit)" && command -v journalctl >/dev/null 2>&1; then
    log="$(journalctl -u "${unit}.service" --since -1h --no-pager 2>/dev/null || true)"
    drops="$(grep -ci 'drop connection' <<<"$log" || true)"
    tries="$(grep -ciE 'Invalid user|Failed password|authenticating user .*\[preauth\]' <<<"$log" || true)"
    sources="$(grep -oE 'from [0-9a-f.:]+ port|user [^ ]+ [0-9a-f.:]+ port' <<<"$log" \
        | grep -oE '[0-9a-f.:]+ port' | sort -u | wc -l)"
    if [ "$drops" -gt 0 ]; then
        report WARN "$drops connections dropped by MaxStartups/penalties in the last hour: legitimate logins lose that lottery too. See install-sshd-flood-guard.sh"
    else
        report OK "No connections dropped in the last hour"
    fi
    if [ "$tries" -gt 0 ]; then
        report INFO "$tries failed or aborted logins in the last hour, from $sources addresses"
    fi
else
    report INFO "No systemd unit or journalctl: the journal was not read"
fi

#
#   fail2ban
#
section "fail2ban"

if ! command -v fail2ban-client >/dev/null 2>&1; then
    report INFO "fail2ban is not installed: nothing bans the single address that hammers"
elif ! systemctl is-active --quiet fail2ban; then
    report WARN "fail2ban is installed but not running"
elif status="$(fail2ban-client status sshd 2>/dev/null)"; then
    banned="$(awk -F: '/Currently banned/ { gsub(/[[:space:]]/, "", $2); print $2 }' <<<"$status")"
    report OK "fail2ban watches sshd (${banned:-?} banned now)"
else
    report WARN "fail2ban runs without an sshd jail. See install-fail2ban-sshd-jail.sh"
fi

#
#   Summary
#
echo
echo "$N_FAIL FAIL, $N_WARN WARN, $N_INFO INFO."
echo "Beyond sshd: port 22 open to the world is the exposure. A source allowlist"
echo "(provider firewall or nftables) is the fix; scripts to act on the above are"
echo "in $HERE."

if [ "$N_FAIL" -gt 0 ]; then
    exit 2
fi
if [ "$N_WARN" -gt 0 ]; then
    exit 1
fi
exit 0
