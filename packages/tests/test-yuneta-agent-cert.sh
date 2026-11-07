#!/bin/bash
#
#   Test of packages/templates/yuneta-agent-cert in a private directory.
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
if ! WORK="$(mktemp -d)" || [ -z "$WORK" ] || [ ! -d "$WORK" ]; then
    echo "FAIL - cannot create a work directory" >&2
    exit 1
fi
trap 'rm -rf "$WORK"' EXIT

export YUNETA_AGENT_CERT_DIR="${WORK}/certs"
CRT="${YUNETA_AGENT_CERT_DIR}/yuneta_agent.crt"
KEY="${YUNETA_AGENT_CERT_DIR}/yuneta_agent.key"
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
    rm -rf "${WORK:?}/certs"
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
#   Usage
#
check "an unknown command: refused" "$("$SCRIPT" nonsense >/dev/null 2>&1; echo $?)" "2"

if [ "$FAILED" -ne 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "PASSED"
