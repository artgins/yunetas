#!/bin/bash
#
#   Test of packages/templates/yuneta-agent-cert in a private directory,
#   and of what a node that builds from source installs beside it: the
#   agent's service/ helpers (in a fake root) and the build's warning.
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
SCRIPT="${HERE}/../templates/yuneta-agent-cert"
NODE_UUID_SCRIPT="${HERE}/../templates/yuneta-node-uuid"
if ! WORK="$(mktemp -d)" || [ -z "$WORK" ] || [ ! -d "$WORK" ]; then
    echo "FAIL - cannot create a work directory" >&2
    exit 1
fi
trap 'rm -rf "$WORK"' EXIT

export YUNETA_AGENT_CERT_DIR="${WORK}/certs"
CRT="${YUNETA_AGENT_CERT_DIR}/yuneta_agent.crt"
KEY="${YUNETA_AGENT_CERT_DIR}/yuneta_agent.key"
UUID_MARK="${YUNETA_AGENT_CERT_DIR}/yuneta_agent.uuid"
#   The node uuid of the test, never the machine's: none until a case makes one
export YUNETA_NODE_UUID_DIR="${WORK}/uuid"
OPENSSL="${YUNETA_AGENT_CERT_OPENSSL:-}"
if [ -z "$OPENSSL" ]; then
    if [ -x /yuneta/development/yunetas/outputs_ext/bin/openssl ]; then
        OPENSSL=/yuneta/development/yunetas/outputs_ext/bin/openssl
    else
        OPENSSL="$(command -v openssl)"
    fi
fi
FAILED=0

#
#   The certificate that came with the agent up to 7.30.5 (public: only its
#   fingerprint matters to the script)
#
PUBLIC_CRT="$(cat <<'PEM'
-----BEGIN CERTIFICATE-----
MIIFdTCCA10CFAN7B5ofEeRw5LI/Kmpb/drILVfsMA0GCSqGSIb3DQEBCwUAMHYx
CzAJBgNVBAYTAkVTMRIwEAYDVQQIDAlDYXN0ZWxsb24xDzANBgNVBAoMBll1bmV0
YTEfMB0GA1UEAwwWeXVuZXRhX2FnZW50Lnl1bmV0YS5pbzEhMB8GCSqGSIb3DQEJ
ARYSbml5YW1ha2FAeXVuZXRhLmlvMCAXDTIxMDQyNDA2Mjg1MloYDzIxMjEwMzMx
MDYyODUyWjB2MQswCQYDVQQGEwJFUzESMBAGA1UECAwJQ2FzdGVsbG9uMQ8wDQYD
VQQKDAZZdW5ldGExHzAdBgNVBAMMFnl1bmV0YV9hZ2VudC55dW5ldGEuaW8xITAf
BgkqhkiG9w0BCQEWEm5peWFtYWthQHl1bmV0YS5pbzCCAiIwDQYJKoZIhvcNAQEB
BQADggIPADCCAgoCggIBAMQUIvkUMfpSNtPdmFKyjX3Jm4bXJX/vf6gGc6sZ+EGz
uiRIUDr1Z15Zo28qBc1PDfYeHi3CM8ik/v33nnA6fgVdx55s+1RZGizRIYRgTOUM
UF6ylNq3le+uv1qe0FEIL2Hsb1C0/tMUYKr6/YLWx/4rSxe3WBMopBlFDDbXXrBf
iUqipwPgbPZVPf7cD+9ViLCBiO+2pbgfTjQIEAcpv4cM7WKYnk/uuMP4TndlIUg9
pD5JWlzjUloevpyoGFW4ek7+Mu2lMopIoaUIofz3DAfsL7KipA/3UQ3TemumXWl0
yTw10kkdPhnTn2y99ingmaRvjtZk5KqAFGy/bWdWudy9JPhe3ZbxFi2sqrRKF71Y
0+DDgiXIMvDKt8ADA+FYXgvwqoswAbcvPsOPgi5FEmFDeNcgifVeXAGTiZ592NMd
cmRh6YNOLhlCh9YdrBUS1YrSlvmlQSvxNIVqMq6g+iJE98G3PoK6oKQnatEdYT39
hgmE5P5GICgxozXuoRhI6RdcCq+oVZb7+bmdIIZ8JY4TCEqU8jLDgYol80yAFGxn
E37KiXNcD0AKsSzHFFwMsrGKcYuEI8Ojt72yBFZx050vEppE6YBNh9KlGtlKTXE+
76A4h1jzCyzuk2u1GP2rxhU3QpTPYyB7jOp210iWVBo/0BChgQebHNLu+sajyPgd
AgMBAAEwDQYJKoZIhvcNAQELBQADggIBABqPw7lpz0EEOnjSkzvMir8kXUqcwV+a
a3VBr04go+hEpMkxrZ8tKJaFH8EIY3Quz4QnIr540ApEDcR4lEaU16A7+qAOiRJ+
hsNTv7OLIPbjGq0tyASryj65UnqwtXwZ5SDtTIY+KWFrULedvpr7mruWcsZ8soo1
wQ3bdqr3bWza7zy1WEne8/yAx6PhTbFti/BbUU1ebKmDqI9WU6OPK3VguhcgLGt1
MQphf8tGc0FbK1GXlsjB5qQrTtjtfWo1MAxrNbYvdb7jNCsIcA7YCjNXWP1BhSGY
RLj7yLNHDGGsIJdF7hWxerzdnFLUKFxugXB8WlQW4r8qR1bf+xapCdn9tf+NLdKl
Cu3PYrtKLl5TwmErQURNgaadEXYvJcWNqqpF/D+a5+S/eq6cvtsXQg/7pjXHmsFU
DlaHIecVGEtUt1gSG9iRCCd3ZeVZRLQzzleenVAS7qKXXe/RL0edx4es2hl80kzQ
LJllKe5900A9TZj/QXMzRy8ebx6U1Mj59Ub1cmkDcNHkt/k+E+jnufPP+oJu+g7l
vtonzJLYMHYmPyEddB0sCEgnpoYt7FZ2GHumWLQwSCDKRzdzkz2roV3b82xQWkJK
HA8yM1uGT2lB+Nemhy65+7vysU+I6e+6jlK3mCBHJPZusQnNNZLt4VPBKQOkU76h
/+6J2UbxVTya
-----END CERTIFICATE-----
PEM
)"

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

fpr() {
    "$OPENSSL" x509 -in "$1" -noout -fingerprint -sha256 2>/dev/null | sed 's/.*=//'
}

copies() {
    ls -A "$YUNETA_AGENT_CERT_DIR" | grep -c '\.replaced-'
}

reset() {
    rm -rf "${WORK:?}/certs" "${WORK:?}/uuid" "${WORK:?}/root"
    mkdir -p "$YUNETA_AGENT_CERT_DIR"
}

make_other_pair() {    # crt key
    "$OPENSSL" req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes \
        -keyout "$2" -out "$1" -subj "/CN=other" -days 30 >/dev/null 2>&1
}

run() {
    "$SCRIPT" "$@" 2>&1
}

#
#   No pair: one is made
#
reset
out="$(run ensure)"
check "missing: made" "$(echo "$out" | grep -c 'certificate made')" "1"
check "missing: the key is the certificate's" \
    "$( [ "$("$OPENSSL" x509 -in "$CRT" -noout -pubkey)" = "$("$OPENSSL" pkey -in "$KEY" -pubout 2>/dev/null)" ] && echo yes)" "yes"
check "missing: named yuneta_agent.yuneta.io" \
    "$("$OPENSSL" x509 -in "$CRT" -noout -subject | grep -c 'CN *= *yuneta_agent.yuneta.io')" "1"
check "missing: the name in the SAN" \
    "$("$OPENSSL" x509 -in "$CRT" -noout -ext subjectAltName | grep -c 'DNS:yuneta_agent.yuneta.io')" "1"
check "missing: an EC key" "$("$OPENSSL" pkey -in "$KEY" -noout -text 2>/dev/null | grep -c 'ASN1 OID: prime256v1')" "1"
check "missing: the key is not readable by others" "$(stat -c %a "$KEY")" "640"
if "$OPENSSL" req -help 2>&1 | grep -q -- '-not_before'; then
    check "missing: valid from 2000" "$("$OPENSSL" x509 -in "$CRT" -noout -startdate | grep -c 'Jan  1 00:00:00 2000')" "1"
    check "missing: valid to 2126" "$("$OPENSSL" x509 -in "$CRT" -noout -enddate | grep -c 'Dec 31 23:59:59 2126')" "1"
fi
first="$(fpr "$CRT")"
check "missing: no copy (nothing was there)" "$(copies)" "0"

#
#   Its own pair: kept
#
out="$(run ensure)"
check "own: kept" "$(fpr "$CRT")" "$first"
check "own: says nothing" "$( [ -z "$out" ] && echo quiet)" "quiet"
check "own: no copy" "$(copies)" "0"

#
#   The pair published with Yuneta: replaced, the old files kept
#
reset
printf '%s\n' "$PUBLIC_CRT" > "$CRT"
make_other_pair "${WORK}/x.crt" "$KEY"
check "public: show says it" "$(run show | grep -c 'state:   public')" "1"
out="$(run ensure)"
check "public: replaced" "$( [ "$(fpr "$CRT")" != "47:BE:EA:44:BA:DB:F5:B6:61:57:3C:FD:2B:CE:13:83:54:C2:8F:A9:15:2A:9A:36:85:CA:EF:56:C3:49:35:F4" ] && echo replaced)" "replaced"
check "public: a WARNING that says it" "$(echo "$out" | grep -c 'WARNING: the agent.s certificate was on a key published')" "1"
check "public: the two old files kept" "$(copies)" "2"
check "public: the old certificate in its copy" \
    "$(fpr "$(ls "$YUNETA_AGENT_CERT_DIR"/yuneta_agent.crt.replaced-* | head -1)")" \
    "47:BE:EA:44:BA:DB:F5:B6:61:57:3C:FD:2B:CE:13:83:54:C2:8F:A9:15:2A:9A:36:85:CA:EF:56:C3:49:35:F4"
check "public: show says own now" "$(run show | grep -c 'state:   own')" "1"

#
#   The published test key (localhost.key) under a certificate of its own:
#   still a published key, replaced
#
reset
LOCALHOST_KEY="${HERE}/../../yunos/c/yuno_agent/certs/localhost.key"
if [ -r "$LOCALHOST_KEY" ]; then
    cp "$LOCALHOST_KEY" "$KEY"
    "$OPENSSL" req -x509 -new -key "$KEY" -out "$CRT" -subj "/CN=resigned" -days 30 >/dev/null 2>&1
    check "a published key under a new certificate: show says public" "$(run show | grep -c 'state:   public')" "1"
    out="$(run ensure)"
    check "a published key under a new certificate: replaced" \
        "$( [ "$("$OPENSSL" pkey -in "$KEY" -pubout 2>/dev/null)" != "$("$OPENSSL" pkey -in "$LOCALHOST_KEY" -pubout 2>/dev/null)" ] && echo replaced)" "replaced"
fi

#
#   A key that is not the certificate's: replaced
#
reset
make_other_pair "$CRT" "${WORK}/a.key"
make_other_pair "${WORK}/b.crt" "$KEY"
out="$(run ensure)"
check "mismatch: a WARNING" "$(echo "$out" | grep -c 'did not match')" "1"
check "mismatch: now a pair" \
    "$( [ "$("$OPENSSL" x509 -in "$CRT" -noout -pubkey)" = "$("$OPENSSL" pkey -in "$KEY" -pubout 2>/dev/null)" ] && echo yes)" "yes"

#
#   A pair of the node's own, not made here (its own CA): kept
#
reset
make_other_pair "$CRT" "$KEY"
own="$(fpr "$CRT")"
out="$(run ensure)"
check "a pair of its own: kept" "$(fpr "$CRT")" "$own"
check "a pair of its own: no copy" "$(copies)" "0"

#
#   Only the certificate: made again, the certificate kept as a copy
#
reset
make_other_pair "$CRT" "${WORK}/c.key"
out="$(run ensure)"
check "no key: made" "$(echo "$out" | grep -c 'certificate made')" "1"
check "no key: the certificate kept" "$(copies)" "1"

#
#   An openssl whose default config file is not there (the packaged SDK's
#   names the directory of the machine that built it): made all the same
#
reset
out="$(OPENSSL_CONF=/nonexistent/openssl.cnf run ensure)"
check "no default config: made" "$(echo "$out" | grep -c 'certificate made')" "1"
check "no default config: nothing left behind" \
    "$(ls -A "$YUNETA_AGENT_CERT_DIR" | grep -c '\.new\.')" "0"

#
#   A cloned disk: box A makes its uuid (yuneta-node-uuid, against a fake
#   /sys) and its pair; the disk is copied to box B, another DMI: B gets
#   another uuid, and another key. The key of A stays A's.
#
reset
SYSROOT="${WORK}/root"
set_dmi() {
    mkdir -p "${SYSROOT}/sys/class/dmi/id" "${SYSROOT}/sys/class/net"
    printf '%s\n' "$1" > "${SYSROOT}/sys/class/dmi/id/product_uuid"
}
node_uuid_ensure() {
    YUNETA_NODE_UUID_SYSROOT="$SYSROOT" "$NODE_UUID_SCRIPT" ensure >/dev/null 2>&1
}
node_uuid() {
    sed -n 's/.*"uuid"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "${YUNETA_NODE_UUID_DIR}/uuid.json"
}
spki() {
    "$OPENSSL" pkey -in "$KEY" -pubout -outform DER 2>/dev/null | sha256sum | cut -d' ' -f1
}
set_dmi "DEADBEEF-1234-5678-9ABC-DEF000004E32"
node_uuid_ensure
uuid_a="$(node_uuid)"
run ensure >/dev/null
key_a="$(spki)"
check "box A: the pair is bound to its uuid" "$(cat "$UUID_MARK" 2>/dev/null)" "$uuid_a"
out="$(run ensure)"
check "box A again: kept" "$(spki)" "$key_a"
check "box A again: says nothing" "$( [ -z "$out" ] && echo quiet)" "quiet"
set_dmi "DEADBEEF-1234-5678-9ABC-DEF000004E33"
node_uuid_ensure
uuid_b="$(node_uuid)"
check "box B: another uuid" "$( [ -n "$uuid_b" ] && [ "$uuid_b" != "$uuid_a" ] && echo other)" "other"
check "box B: show says cloned" "$(run show | grep -c 'state:   cloned')" "1"
out="$(run ensure)"
check "box B: another key" "$( [ -n "$(spki)" ] && [ "$(spki)" != "$key_a" ] && echo other)" "other"
check "box B: the key is the certificate's" \
    "$( [ "$("$OPENSSL" x509 -in "$CRT" -noout -pubkey)" = "$("$OPENSSL" pkey -in "$KEY" -pubout 2>/dev/null)" ] && echo yes)" "yes"
check "box B: a WARNING that says it" "$(echo "$out" | grep -c 'WARNING: .*made for node uuid')" "1"
check "box B: bound to its own uuid" "$(cat "$UUID_MARK" 2>/dev/null)" "$uuid_b"
check "box B: the old certificate kept" \
    "$(ls -A "$YUNETA_AGENT_CERT_DIR" | grep -c '^yuneta_agent\.crt\.replaced-')" "1"
check "box B: the old key NOT kept" \
    "$(ls -A "$YUNETA_AGENT_CERT_DIR" | grep -c '^yuneta_agent\.key\.')" "0"
check "box B: A's key in no file" "$(
    n=0
    for f in "$YUNETA_AGENT_CERT_DIR"/* "$YUNETA_AGENT_CERT_DIR"/.[!.]*; do
        if [ -f "$f" ] && [ "$("$OPENSSL" pkey -in "$f" -pubout -outform DER 2>/dev/null | sha256sum | cut -d' ' -f1)" = "$key_a" ]; then
            n=$((n + 1))
        fi
    done
    echo "$n")" "0"
check "box B: the WARNING says the old key is gone" "$(echo "$out" | grep -c 'its key removed')" "1"
out="$(run ensure)"
check "box B again: kept" "$( [ -z "$out" ] && echo quiet)" "quiet"

#
#   A pair made here by an earlier version, no uuid beside it: bound to the
#   node uuid it finds, then replaced on a clone
#
rm -f "$UUID_MARK"
key_b="$(spki)"
out="$(run ensure)"
check "made before the binding: kept" "$(spki)" "$key_b"
check "made before the binding: bound now" "$(cat "$UUID_MARK" 2>/dev/null)" "$uuid_b"
set_dmi "DEADBEEF-1234-5678-9ABC-DEF000004E34"
node_uuid_ensure
run ensure >/dev/null
check "made before the binding, then cloned: another key" \
    "$( [ -n "$(spki)" ] && [ "$(spki)" != "$key_b" ] && echo other)" "other"

#
#   A pair not made here (its own CA) on a clone: kept, never bound, even
#   where a pair made here and bound was before it
#
check "a pair of its own: put over a bound one" "$( [ -e "$UUID_MARK" ] && echo bound)" "bound"
make_other_pair "$CRT" "$KEY"
own="$(fpr "$CRT")"
run ensure >/dev/null
check "a pair of its own: kept" "$(fpr "$CRT")" "$own"
set_dmi "DEADBEEF-1234-5678-9ABC-DEF000004E35"
node_uuid_ensure
run ensure >/dev/null
check "a pair of its own, on a clone: kept" "$(fpr "$CRT")" "$own"
rm -rf "${WORK:?}/certs"
mkdir -p "$YUNETA_AGENT_CERT_DIR"
make_other_pair "$CRT" "$KEY"
run ensure >/dev/null
check "a pair of its own: not bound" "$( [ ! -e "$UUID_MARK" ] && echo none)" "none"

#
#   No node uuid yet (a source node before the agent's first start): a pair,
#   bound at the first ensure that finds the uuid
#
reset
run ensure >/dev/null
check "no node uuid: a pair" "$( [ -s "$KEY" ] && echo made)" "made"
check "no node uuid: not bound" "$( [ ! -e "$UUID_MARK" ] && echo none)" "none"
node_uuid_ensure
run ensure >/dev/null
check "the node uuid appears: bound" "$(cat "$UUID_MARK" 2>/dev/null)" "$(node_uuid)"

#
#   The lock: a .lock that cannot be opened is said, and the work is done
#
reset
mkdir "${YUNETA_AGENT_CERT_DIR}/.lock"
out="$(run ensure)"
check "a .lock it cannot open: says it" "$(echo "$out" | grep -c 'WARNING: cannot open .*\.lock, going on without the lock')" "1"
check "a .lock it cannot open: made all the same" "$(echo "$out" | grep -c 'certificate made')" "1"

#
#   Two ensures at once (the two agents' units share the pair): one pair,
#   made once
#
reset
"$SCRIPT" ensure > "${WORK}/e1" 2>&1 &
p1=$!
"$SCRIPT" ensure > "${WORK}/e2" 2>&1 &
p2=$!
wait "$p1"
wait "$p2"
check "two at once: made once" "$(cat "${WORK}/e1" "${WORK}/e2" | grep -c 'certificate made')" "1"
check "two at once: the key is the certificate's" \
    "$( [ "$("$OPENSSL" x509 -in "$CRT" -noout -pubkey)" = "$("$OPENSSL" pkey -in "$KEY" -pubout 2>/dev/null)" ] && echo yes)" "yes"
check "two at once: no copy" "$(copies)" "0"

#
#   Both agents' units run it, after the node uuid; so does the drop-in of a
#   node that builds from source (drop-ins run in the order of their names)
#
TEMPLATES="${HERE}/../templates"
for unit in yuneta_agent yuneta_agent22; do
    check "$unit.service: the node uuid, then the pair" "$(
        grep -E '^ExecStartPre=.*/yuneta-(node-uuid|agent-cert) ensure$' "${TEMPLATES}/${unit}.service" \
            | sed 's/.*yuneta-\(node-uuid\|agent-cert\).*/\1/' | paste -sd' ' -)" "node-uuid agent-cert"
done
installer_value() {    # file variable
    sed -n "s/^$2=\"\(.*\)\"\$/\1/p" "$1"
}
check "install-yuneta-agent-cert.sh: both units" \
    "$(installer_value "${HERE}/../install-yuneta-agent-cert.sh" UNITS)" "yuneta_agent yuneta_agent22"
uuid_dropin="$(installer_value "${HERE}/../install-yuneta-node-uuid.sh" DROPIN_NAME)"
cert_dropin="$(installer_value "${HERE}/../install-yuneta-agent-cert.sh" DROPIN_NAME)"
check "install-yuneta-agent-cert.sh: its drop-in runs after the node uuid's" \
    "$(printf '%s\n%s\n' "$cert_dropin" "$uuid_dropin" | LC_ALL=C sort | paste -sd' ' -)" "$uuid_dropin $cert_dropin"

#
#   A node that builds from source installs the agent's service/ (its
#   CMakeLists.txt), and the postinst seeds /etc/init.d/yuneta_agent from
#   there: it is the .deb's SysV script, which runs the node uuid, then the
#   pair, before it starts the agents outside systemd
#
deb_sysv="$(sed -n '/^cat > "${WORKDIR}\/etc\/init.d\/yuneta_agent" <<'"'"'EOF'"'"'$/,/^EOF$/p' \
    "${HERE}/../deb/make-yuneta-agent-deb.sh" | sed '1d;$d')"
check "the .deb's SysV script: found" "$( [ -n "$deb_sysv" ] && echo found)" "found"
check "yuno_agent/service/yuneta_agent: the .deb's SysV script" "$(
    if [ "$deb_sysv" = "$(cat "${HERE}/../../yunos/c/yuno_agent/service/yuneta_agent")" ]; then
        echo same
    else
        echo different
    fi)" "same"
#   ... and the two helpers beside it, which install and remove the service
for n in install-yuneta-service.sh remove-yuneta-service.sh; do
    deb_helper="$(sed -n '/^cat > "${WORKDIR}\/yuneta\/agent\/service\/'"$n"'" <<'"'"'EOF'"'"'$/,/^EOF$/p' \
        "${HERE}/../deb/make-yuneta-agent-deb.sh" | sed '1d;$d')"
    check "the .deb's $n: found" "$( [ -n "$deb_helper" ] && echo found)" "found"
    check "yuno_agent/service/$n: the .deb's" "$(
        if [ "$deb_helper" = "$(cat "${HERE}/../../yunos/c/yuno_agent/service/$n")" ]; then
            echo same
        else
            echo different
        fi)" "same"
done
for f in "${HERE}/../deb/make-yuneta-agent-deb.sh" "${HERE}/../rpm/make-yuneta-agent-rpm.sh"; do
    check "$(basename "$f"): the SysV start runs the node uuid, then the pair" "$(
        grep -E '^ +/usr/libexec/yuneta/yuneta-(node-uuid|agent-cert) ensure \|\| true$' "$f" \
            | sed 's/.*yuneta-\(node-uuid\|agent-cert\).*/\1/' | paste -sd' ' -)" "node-uuid agent-cert"
done

#
#   A new pair is served by both agents once restarted, one at a time
#
check "install-yuneta-agent-cert.sh: says to restart both agents" \
    "$(grep -c 'systemctl restart yuneta_agent; sudo systemctl restart yuneta_agent22' "${HERE}/../install-yuneta-agent-cert.sh")" "1"

#
#   install-yuneta-service.sh, the .deb's and the .rpm's, in a fake root:
#   the helper's paths are moved under it, and systemctl, pgrep, su, sleep
#   and the SysV tools are stubs that write what they are asked in a log
#
SROOT="${WORK}/sroot"
STUBS="${WORK}/stubs"
SLOG="${WORK}/service.log"
SERVICE_DIR="${HERE}/../../yunos/c/yuno_agent/service"
mkdir -p "$STUBS"
cat > "${STUBS}/systemctl" <<'EOF'
#!/bin/sh
echo "systemctl $*" >> "$SLOG"
case "$1" in
    show)
        echo 0
        ;;
    restart)
        for _d in usr/lib/systemd/system lib/systemd/system etc/systemd/system run/systemd/generator.late; do
            if [ -f "${SROOT}/${_d}/$2" ]; then
                case " ${FAIL_RESTART} " in
                    *" $2 "*)
                        exit 1
                        ;;
                esac
                exit 0
            fi
        done
        echo "Failed to restart $2: Unit $2 not found." >&2
        exit 5
        ;;
esac
exit 0
EOF
cat > "${STUBS}/pgrep" <<'EOF'
#!/bin/sh
if [ "$1" = "-x" ] && [ -f "${SROOT}/pgrep/$2" ]; then
    cat "${SROOT}/pgrep/$2"
    exit 0
fi
exit 1
EOF
for stub in su runuser invoke-rc.d service chkconfig; do
    printf '#!/bin/sh\necho "%s $*" >> "$SLOG"\nexit 0\n' "$stub" > "${STUBS}/${stub}"
done
printf '#!/bin/sh\nexit 0\n' > "${STUBS}/sleep"
chmod 0755 "$STUBS"/*
#   A node without diffutils (a minimal RHEL): cmp is not found
NOCMP="${WORK}/nocmp"
mkdir -p "$NOCMP"
printf '#!/bin/sh\necho "cmp: not found" >&2\nexit 127\n' > "${NOCMP}/cmp"
chmod 0755 "${NOCMP}/cmp"

service_root() {    # 1: a systemd that runs, 0: none
    rm -rf "${SROOT:?}"
    mkdir -p "${SROOT}/yuneta/agent/service" "${SROOT}/etc/init.d" "${SROOT}/usr/sbin" \
        "${SROOT}/pgrep" "${SROOT}/proc"
    cp "${SERVICE_DIR}/yuneta_agent" "${SROOT}/yuneta/agent/service/yuneta_agent"
    cp "${STUBS}/su" "${SROOT}/usr/sbin/update-rc.d"
    sed -i 's/"su /"update-rc.d /' "${SROOT}/usr/sbin/update-rc.d"
    if [ "$1" = "1" ]; then
        mkdir -p "${SROOT}/run/systemd/system"
    fi
}
service_units() {
    mkdir -p "${SROOT}/usr/lib/systemd/system"
    touch "${SROOT}/usr/lib/systemd/system/yuneta_agent.service" \
        "${SROOT}/usr/lib/systemd/system/yuneta_agent22.service"
}
agent_running() {    # name pid executable argv0; no executable: it cannot be read
    echo "$2" >> "${SROOT}/pgrep/$1"
    mkdir -p "${SROOT}/proc/$2"
    if [ -n "$3" ]; then
        ln -s "$3" "${SROOT}/proc/$2/exe"
    fi
    printf '%s\000--config-file=/yuneta/agent/%s.json\000' "$4" "$1" > "${SROOT}/proc/$2/cmdline"
}
#   What /proc/<pid>/exe reads for an agent started as /yuneta/agent/<name>:
#   its binary replaced on disk, /yuneta a symlink, its binary renamed
#   (*.bak-pre-<version>), or nothing (root without CAP_SYS_PTRACE)
AGENT_FORMS="replaced symlink renamed unreadable"
agent_exe() {    # form name
    case "$1" in
        replaced)
            echo "${SROOT}/yuneta/agent/$2 (deleted)"
            ;;
        symlink)
            echo "/data/yuneta/agent/$2"
            ;;
        renamed)
            echo "${SROOT}/yuneta/agent/$2.bak-pre-7.30.17"
            ;;
        *)
            echo ""
            ;;
    esac
}
run_helper() {    # helper; FAIL_RESTART: the units whose restart fails
    sed "s#\([ \"'(=]\)/\(run\|usr\|lib\|etc\|yuneta\|proc\)/#\1${SROOT}/\2/#g" "$1" > "${WORK}/helper.sh"
    : > "$SLOG"
    env PATH="${EXTRA_PATH:+${EXTRA_PATH}:}${STUBS}:${PATH}" SLOG="$SLOG" SROOT="$SROOT" \
        FAIL_RESTART="${FAIL_RESTART:-}" sh "${WORK}/helper.sh" > "${WORK}/helper.out" 2>&1
}
stops() {
    grep -c -- '--stop' "$SLOG"
}
restarts() {
    grep '^systemctl restart' "$SLOG" | sed 's/.* //' | paste -sd' ' -
}
sysv_starts() {
    grep -cE '^(invoke-rc.d|service) yuneta_agent start$' "$SLOG"
}

rpm_helper="${WORK}/rpm-install-yuneta-service.sh"
sed -n '/^cat > "${STAGE}\/yuneta\/agent\/service\/install-yuneta-service.sh" <<'"'"'EOF'"'"'$/,/^EOF$/p' \
    "${HERE}/../rpm/make-yuneta-agent-rpm.sh" | sed '1d;$d' > "$rpm_helper"
check "the .rpm's install-yuneta-service.sh: found" "$( [ -s "$rpm_helper" ] && echo found)" "found"
deb_helper="${WORK}/deb-install-yuneta-service.sh"
sed -n '/^cat > "${WORKDIR}\/yuneta\/agent\/service\/install-yuneta-service.sh" <<'"'"'EOF'"'"'$/,/^EOF$/p' \
    "${HERE}/../deb/make-yuneta-agent-deb.sh" | sed '1d;$d' > "$deb_helper"
check "install-yuneta-service.sh: the .rpm's is the .deb's up to the SysV enable" "$(
    if [ "$(sed '/^# Enable SysV/,$d' "$deb_helper")" = "$(sed '/^if command -v chkconfig/,$d' "$rpm_helper")" ]; then
        echo same
    else
        echo different
    fi)" "same"

for helper in "${SERVICE_DIR}/install-yuneta-service.sh" "$rpm_helper"; do
    case "$helper" in
        "$rpm_helper")
            flavour=".rpm"
            ;;
        *)
            flavour=".deb"
            ;;
    esac

    #   A node that builds from source, systemd and no units, the agents
    #   started by hand: none of them stopped, the SysV start (up to 7.30.16
    #   both were stopped and left down, and the helper said 0)
    service_root 1
    agent_running yuneta_agent 101 "${SROOT}/yuneta/agent/yuneta_agent" /yuneta/agent/yuneta_agent
    agent_running yuneta_agent22 102 "${SROOT}/yuneta/agent/yuneta_agent22" /yuneta/agent/yuneta_agent22
    run_helper "$helper"
    rc=$?
    check "$flavour service, systemd without units: 0" "$rc" "0"
    check "$flavour service, systemd without units: no agent stopped" "$(stops)" "0"
    check "$flavour service, systemd without units: no unit restarted" "$(grep -c '^systemctl restart' "$SLOG")" "0"
    check "$flavour service, systemd without units: the SysV start" "$(sysv_starts)" "1"

    #   ... with the unit systemd generated from the SysV script: agent22
    #   has no unit, and stays up
    mkdir -p "${SROOT}/run/systemd/generator.late"
    touch "${SROOT}/run/systemd/generator.late/yuneta_agent.service"
    run_helper "$helper"
    check "$flavour service, a generated unit: no agent stopped" "$(stops)" "0"
    check "$flavour service, a generated unit: the SysV start" "$(sysv_starts)" "1"

    #   The units: each agent running outside its unit is stopped, then its
    #   unit restarted, the main agent first
    service_root 1
    service_units
    agent_running yuneta_agent 101 "${SROOT}/yuneta/agent/yuneta_agent" /yuneta/agent/yuneta_agent
    agent_running yuneta_agent22 102 "${SROOT}/yuneta/agent/yuneta_agent22 (deleted)" /yuneta/agent/yuneta_agent22
    run_helper "$helper"
    rc=$?
    check "$flavour service, the units: 0" "$rc" "0"
    check "$flavour service, the units: both agents outside stopped first" \
        "$(grep -E '^(su |systemctl restart)' "$SLOG" | sed 's/^su .*\/\(yuneta_agent[0-9]*\) .*/stop:\1/; s/^systemctl restart \(.*\)\.service/restart:\1/' | paste -sd' ' -)" \
        "stop:yuneta_agent restart:yuneta_agent stop:yuneta_agent22 restart:yuneta_agent22"
    check "$flavour service, the units: no SysV start" "$(sysv_starts)" "0"

    #   ... a restart that fails: said, not 0, and agent22 left as it is
    FAIL_RESTART="yuneta_agent.service" run_helper "$helper"
    rc=$?
    check "$flavour service, the main agent's unit fails: not 0" "$( [ "$rc" -ne 0 ] && echo failed)" "failed"
    check "$flavour service, the main agent's unit fails: agent22 untouched" "$(restarts)" "yuneta_agent.service"
    check "$flavour service, the main agent's unit fails: agent22 not stopped" "$(stops)" "1"
    FAIL_RESTART="yuneta_agent22.service" run_helper "$helper"
    rc=$?
    check "$flavour service, agent22's unit fails: not 0" "$( [ "$rc" -ne 0 ] && echo failed)" "failed"

    #   ... an agent outside its unit is found whatever its executable
    #   reads: started as /yuneta/agent/<name> (argv[0], as its own --stop
    #   takes it; up to 7.30.17 only by its executable)
    for form in $AGENT_FORMS; do
        service_root 1
        service_units
        agent_running yuneta_agent 101 "$(agent_exe "$form" yuneta_agent)" /yuneta/agent/yuneta_agent
        run_helper "$helper"
        check "$flavour service, the agent outside its unit, $form: stopped" "$(stops)" "1"
    done

    #   ... a process named like an agent that was not started as one (the
    #   SysV script, whose argv[0] is its shell; a script of that name) is
    #   not stopped
    service_root 1
    service_units
    agent_running yuneta_agent 201 /usr/bin/dash /bin/sh
    agent_running yuneta_agent22 202 /usr/bin/python3.13 /usr/bin/python3
    run_helper "$helper"
    check "$flavour service, a process named like an agent: not stopped" "$(stops)" "0"
    check "$flavour service, a process named like an agent: both units restarted" \
        "$(restarts)" "yuneta_agent.service yuneta_agent22.service"

    #   No systemd: an /etc/init.d/yuneta_agent of an earlier agent is
    #   replaced, the old one kept beside it, not executable
    service_root 0
    printf '#!/bin/sh\nexit 0\n' > "${SROOT}/etc/init.d/yuneta_agent"
    chmod 0755 "${SROOT}/etc/init.d/yuneta_agent"
    run_helper "$helper"
    rc=$?
    check "$flavour service, an old SysV script: 0" "$rc" "0"
    check "$flavour service, an old SysV script: replaced" "$(
        cmp -s "${SROOT}/etc/init.d/yuneta_agent" "${SERVICE_DIR}/yuneta_agent" && echo replaced)" "replaced"
    old_sysv="$(ls "${SROOT}/etc/init.d/" | grep -c '^yuneta_agent\..*\.bak$')"
    check "$flavour service, an old SysV script: kept beside it" "$old_sysv" "1"
    check "$flavour service, an old SysV script: kept, not executable" \
        "$(stat -c %a "${SROOT}"/etc/init.d/yuneta_agent.*.bak 2>/dev/null)" "644"
    check "$flavour service, an old SysV script: the SysV start" "$(sysv_starts)" "1"
    run_helper "$helper"
    check "$flavour service, the SysV script of this agent: untouched" \
        "$(ls "${SROOT}/etc/init.d/" | grep -c '^yuneta_agent\..*\.bak$')" "1"
    #   ... and none: installed
    rm -f "${SROOT}/etc/init.d/"*
    run_helper "$helper"
    check "$flavour service, no SysV script: installed" "$(
        cmp -s "${SROOT}/etc/init.d/yuneta_agent" "${SERVICE_DIR}/yuneta_agent" && echo installed)" "installed"
    check "$flavour service, no SysV script: nothing kept" \
        "$(ls "${SROOT}/etc/init.d/" | grep -c '\.bak$')" "0"

    #   The units, and an old SysV script (`service yuneta_agent` runs it):
    #   replaced too
    service_root 1
    service_units
    printf '#!/bin/sh\nexit 0\n' > "${SROOT}/etc/init.d/yuneta_agent"
    run_helper "$helper"
    check "$flavour service, the units and an old SysV script: replaced" "$(
        cmp -s "${SROOT}/etc/init.d/yuneta_agent" "${SERVICE_DIR}/yuneta_agent" && echo replaced)" "replaced"

    #   No cmp (no diffutils): the SysV script of this agent is left as it
    #   is, and an old one is still replaced
    service_root 0
    cp "${SERVICE_DIR}/yuneta_agent" "${SROOT}/etc/init.d/yuneta_agent"
    EXTRA_PATH="$NOCMP" run_helper "$helper"
    EXTRA_PATH="$NOCMP" run_helper "$helper"
    check "$flavour service, no cmp, the SysV script of this agent: untouched" \
        "$(ls "${SROOT}/etc/init.d/" | grep -c '\.bak$')" "0"
    printf '#!/bin/sh\nexit 0\n' > "${SROOT}/etc/init.d/yuneta_agent"
    EXTRA_PATH="$NOCMP" run_helper "$helper"
    check "$flavour service, no cmp, an old SysV script: replaced" "$(
        cmp -s "${SROOT}/etc/init.d/yuneta_agent" "${SERVICE_DIR}/yuneta_agent" && echo replaced)" "replaced"
done

#
#   The packages' own scripts find an agent by its argv[0] too: the
#   postinsts' _agent_into_its_unit() (the .deb's, the .rpm's %post), the
#   .rpm's %pre, which stops the main agent for an upgrade, and the SysV
#   script's _agent_outside_unit() (the .deb's is service/'s, checked above)
#
deb_into="$(sed -n '/^_agent_into_its_unit() {$/,/^}$/p' "${HERE}/../deb/make-yuneta-agent-deb.sh")"
rpm_into="$(sed -n '/^    _agent_into_its_unit() {$/,/^    }$/p' "${HERE}/../rpm/make-yuneta-agent-rpm.sh")"
rpm_pre="$(sed -n '/^%pre$/,/^%post$/p' "${HERE}/../rpm/make-yuneta-agent-rpm.sh" \
    | sed -n '/^if \[ "\$1" = "2" \]; then$/,/^fi$/p')"
deb_outside="$(sed -n '/^_agent_outside_unit() {$/,/^}$/p' "${SERVICE_DIR}/yuneta_agent")"
rpm_outside="$(sed -n '/^_agent_outside_unit() {$/,/^}$/p' "${HERE}/../rpm/make-yuneta-agent-rpm.sh")"
check "the .deb's postinst: _agent_into_its_unit found" "$( [ -n "$deb_into" ] && echo found)" "found"
check "the .rpm's %post: _agent_into_its_unit found" "$( [ -n "$rpm_into" ] && echo found)" "found"
check "the .rpm's %pre: the upgrade's stop found" "$( [ -n "$rpm_pre" ] && echo found)" "found"
check "the .deb's SysV script: _agent_outside_unit found" "$( [ -n "$deb_outside" ] && echo found)" "found"
check "the .rpm's SysV script: _agent_outside_unit found" "$( [ -n "$rpm_outside" ] && echo found)" "found"
run_snippet() {    # code call [args]
    printf '%s\n%s\n' "$1" "$2" > "${WORK}/snippet.in"
    sed "s#\([ \"'(=]\)/\(run\|usr\|lib\|etc\|yuneta\|proc\)/#\1${SROOT}/\2/#g" "${WORK}/snippet.in" \
        > "${WORK}/snippet.sh"
    : > "$SLOG"
    shift 2
    env PATH="${STUBS}:${PATH}" SLOG="$SLOG" SROOT="$SROOT" FAIL_RESTART="" \
        sh "${WORK}/snippet.sh" "$@" > "${WORK}/snippet.out" 2>&1
}
for into in deb rpm; do
    if [ "$into" = "deb" ]; then
        code="$deb_into"
    else
        code="$rpm_into"
    fi
    service_root 1
    service_units
    agent_running yuneta_agent 201 /usr/bin/dash /bin/sh
    run_snippet "$code" "_agent_into_its_unit yuneta_agent"
    check ".$into postinst, a process named like the agent: not stopped" "$(stops)" "0"
    check ".$into postinst, a process named like the agent: the unit restarted" "$(restarts)" "yuneta_agent.service"
    service_root 1
    service_units
    agent_running yuneta_agent 101 "${SROOT}/yuneta/agent/yuneta_agent" /yuneta/agent/yuneta_agent
    run_snippet "$code" "_agent_into_its_unit yuneta_agent"
    check ".$into postinst, the agent outside its unit: stopped" "$(stops)" "1"
    #   ... whatever its executable reads ("(deleted)" is what an agent
    #   outside its unit reads there: the package has just replaced it)
    for form in $AGENT_FORMS; do
        service_root 1
        service_units
        agent_running yuneta_agent 101 "$(agent_exe "$form" yuneta_agent)" /yuneta/agent/yuneta_agent
        run_snippet "$code" "_agent_into_its_unit yuneta_agent"
        check ".$into postinst, the agent outside its unit, $form: stopped" "$(stops)" "1"
    done
done
service_root 1
printf '#!/bin/sh\n' > "${SROOT}/yuneta/agent/yuneta_agent"
chmod 0755 "${SROOT}/yuneta/agent/yuneta_agent"
agent_running yuneta_agent 201 /usr/bin/dash /bin/sh
run_snippet "$rpm_pre" "" 2
check ".rpm %pre, a process named like the agent: not stopped" "$(stops)" "0"
for form in $AGENT_FORMS; do
    service_root 1
    printf '#!/bin/sh\n' > "${SROOT}/yuneta/agent/yuneta_agent"
    chmod 0755 "${SROOT}/yuneta/agent/yuneta_agent"
    agent_running yuneta_agent 101 "$(agent_exe "$form" yuneta_agent)" /yuneta/agent/yuneta_agent
    run_snippet "$rpm_pre" "" 2
    check ".rpm %pre, the agent on its old binary, $form: stopped" "$(stops)" "1"
done
for sysv in deb rpm; do
    if [ "$sysv" = "deb" ]; then
        code="$deb_outside"
    else
        code="$rpm_outside"
    fi
    service_root 1
    agent_running yuneta_agent 201 /usr/bin/dash /bin/sh
    run_snippet "$code" "_agent_outside_unit yuneta_agent && echo outside || echo none"
    check ".$sysv SysV script, a process named like the agent (itself): none" "$(cat "${WORK}/snippet.out")" "none"
    for form in plain $AGENT_FORMS; do
        service_root 1
        if [ "$form" = "plain" ]; then
            agent_running yuneta_agent 101 "${SROOT}/yuneta/agent/yuneta_agent" /yuneta/agent/yuneta_agent
        else
            agent_running yuneta_agent 101 "$(agent_exe "$form" yuneta_agent)" /yuneta/agent/yuneta_agent
        fi
        run_snippet "$code" "_agent_outside_unit yuneta_agent && echo outside || echo none"
        check ".$sysv SysV script, the agent outside its unit, $form: found" "$(cat "${WORK}/snippet.out")" "outside"
    done
done

#
#   The warning of the agent's build (yuno_agent/CMakeLists.txt) about the
#   scripts a git pull does not update, against a fake root
#
if command -v cmake >/dev/null 2>&1; then
    CROOT="${WORK}/croot"
    cmake_warning() {
        {
            echo "set(_yuneta_templates \"${TEMPLATES}\")"
            echo "set(_yuneta_sysv \"${SERVICE_DIR}/yuneta_agent\")"
            sed -n '/^install(CODE \[\[$/,/^\]\])$/p' "${HERE}/../../yunos/c/yuno_agent/CMakeLists.txt" \
                | sed '1d;$d' \
                | sed "s#\"/usr/libexec/#\"${CROOT}/usr/libexec/#g; s#\"/etc/#\"${CROOT}/etc/#g"
        } > "${WORK}/warning.cmake"
        cmake -P "${WORK}/warning.cmake" 2>&1
    }
    cmake_root() {
        rm -rf "${CROOT:?}"
        mkdir -p "${CROOT}/usr/libexec/yuneta" "${CROOT}/etc/init.d"
    }
    cmake_root
    check "build warning: a node that took neither script: quiet" \
        "$( [ -z "$(cmake_warning)" ] && echo quiet)" "quiet"
    cp "${TEMPLATES}/yuneta-node-uuid" "${CROOT}/usr/libexec/yuneta/"
    check "build warning: the node uuid without the pair's script: said" \
        "$(cmake_warning | grep -c 'yuneta-agent-cert is missing')" "1"
    cp "${TEMPLATES}/yuneta-agent-cert" "${CROOT}/usr/libexec/yuneta/"
    check "build warning: both scripts of this tree: quiet" \
        "$( [ -z "$(cmake_warning)" ] && echo quiet)" "quiet"
    rm -f "${CROOT}/usr/libexec/yuneta/yuneta-node-uuid"
    check "build warning: the pair's script without the node uuid: said" \
        "$(cmake_warning | grep -c 'yuneta-node-uuid is missing')" "1"
    cp "${TEMPLATES}/yuneta-node-uuid" "${CROOT}/usr/libexec/yuneta/"
    printf '#!/bin/sh\nexit 0\n' > "${CROOT}/etc/init.d/yuneta_agent"
    check "build warning: an old SysV script: said" \
        "$(cmake_warning | grep -c '/etc/init.d/yuneta_agent is not this tree')" "1"
    cp "${SERVICE_DIR}/yuneta_agent" "${CROOT}/etc/init.d/yuneta_agent"
    check "build warning: the SysV script of this tree: quiet" \
        "$( [ -z "$(cmake_warning)" ] && echo quiet)" "quiet"
else
    echo "skip - no cmake: the build warning is not checked"
fi

#
#   Usage
#
check "an unknown command: refused" "$("$SCRIPT" nonsense >/dev/null 2>&1; echo $?)" "2"

if [ "$FAILED" -ne 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "PASSED"
