#!/usr/bin/env python3
#
#   Copyright (c) 2026, ArtGins.
#
#   Classify the results of a run:
#       classify.py <OUT>/summary.txt [baseline.txt]
#
#   Prints the counts per class, then one markdown row per test. With a
#   baseline (a summary.txt of a reference run, e.g. ./baseline.txt), it
#   first prints every test whose exit code changed against it.
#   Classes:
#       PASS    exit 0
#       U       unsupported feature (skipped with exit 77, or fails on it)
#       U*      exit 0 but not a pass (the test never runs its case, or passes by coincidence)
#       L       the test checks mosquitto itself (its files, command line, config parser)
#       S       the spec allows both behaviours and the test pins mosquitto's choice
#       T       the test itself does not run (bug in the suite)
#       NEW     fails and is not classified: a regression, or a new test. Look at it.
#
#   The table names every test that is known to fail; any other failure is NEW.
#
import sys, collections

SUM = sys.argv[1]
B1 = "B1 retained PUBLISH sent before SUBACK (allowed by MQTT-3.8.4)"
C = {
 # ---- passes that are not passes
 '08-ssl-connect-no-auth-wrong-ca.py': ('U*', 'TLS: passes by coincidence (TLS handshake fails against a plain listener)'),
 '09-plugin-acl-change.py': ('U*', 'plugin: vacuous pass, the test never calls do_test()'),
 '09-plugin-auth-acl-pub-prop.py': ('U*', 'plugin: vacuous pass, the test never calls do_test()'),
 '09-plugin-auth-defer-unpwd-fail.py': ('U*', 'plugin: coincidental pass (bad password refused anyway, rc 5)'),
 '09-plugin-auth-msg-params.py': ('U*', 'plugin: vacuous pass, the test never calls do_test()'),
 '09-plugin-auth-unpwd-fail.py': ('U*', 'plugin: coincidental pass (refused anyway, rc 5)'),
 '09-plugin-auth-v2-unpwd-fail.py': ('U*', 'plugin: vacuous pass, the test never calls do_test()'),
 '09-plugin-auth-v3-unpwd-fail.py': ('U*', 'plugin: coincidental pass (refused anyway, rc 5)'),
 '09-plugin-auth-v4-unpwd-fail.py': ('U*', 'plugin: coincidental pass (refused anyway, rc 5)'),
 '09-plugin-auth-v5-unpwd-fail.py': ('U*', 'plugin: coincidental pass (refused anyway, rc 5)'),
 '16-config-includedir.py': ('PASS', 'include_dir is read by the stand-in (config parser feature, not the broker)'),
 # ---- failures the spec allows (the test pins mosquitto's choice)
 '01-connect-575314.py': ('S', 'client id of 65000 characters refused: longer than NAME_MAX-4 (MQTT-3.1.3-5 lets a server refuse it); the 65 KB CONNECT itself is accepted'),
 '02-subpub-qos1-message-expiry-retain.py': ('S', B1),
 # ---- other failures
 '01-connect-accept-protocol.py': ('U', 'accept_protocol_versions'),
 '01-connect-auto-id.py': ('U', 'auto_id_prefix (yuneta hardcodes "auto-")'),
 '01-connect-global-max-clients.py': ('U', 'global_max_clients (CONNACK 0x89 Server busy)'),
 '01-connect-max-keepalive.py': ('S', 'B12 v3.1.1 keepalive > max_keepalive: mosquitto refuses (rc 2), yuneta accepts'),
 '02-subpub-qos0-send-retain.py': ('S', B1 + ' (passes when the order is tolerated)'),
 '02-subpub-qos2-max-inflight-bytes.py': ('U', 'max_inflight_bytes: attr exists, not implemented (db__ready_for_flight)'),
 '03-pattern-matching.py': ('S', B1 + ' (passes when the order is tolerated)'),
 '03-publish-qos1-queued-bytes.py': ('T', 'test does not run (python2 `import Queue`); would test max_queued_bytes, not enforced (U)'),
 '03-publish-qos2-reuse-mid.py': ('S', 'B13 QoS 2 PUBLISH reusing an unreleased packet id: yuneta keeps the first (MQTT-4.3.3-9), mosquitto the second'),
 '04-retain-check-source-persist-diff-port.py': ('U', 'check_retain_source + acl_file (also ' + B1 + ')'),
 '04-retain-check-source-persist.py': ('U', 'check_retain_source + acl_file (also ' + B1 + ')'),
 '04-retain-check-source.py': ('U', 'check_retain_source + acl_file (also ' + B1 + ')'),
 '04-retain-clear-multiple.py': ('S', B1 + '; then B14 retained of several topics in another order (clearing itself works)'),
 '04-retain-qos0-repeated.py': ('S', B1 + ' (passes when the order is tolerated)'),
 '04-retain-upgrade-outgoing-qos.py': ('U', 'upgrade_outgoing_qos (also ' + B1 + ')'),
 '06-bridge-clean-session-core.py': ('U', 'bridges (not feature-gated by the test)'),
 '07-will-oversize-payload.py': ('S', 'B15 message_size_limit not applied to the will payload at CONNECT'),
 '09-acl-access-variants.py': ('U', 'acl_file'),
 '09-acl-change.py': ('U', 'acl_file'),
 '09-acl-empty-file.py': ('U', 'acl_file'),
 '09-extended-auth-change-username.py': ('U', 'enhanced authentication (AUTH) + acl_file'),
 '09-extended-auth-multistep-reauth.py': ('U', 'enhanced authentication (AUTH); CONNACK 0x8C is right for an unknown method'),
 '09-extended-auth-multistep.py': ('U', 'enhanced authentication (AUTH)'),
 '09-extended-auth-reauth.py': ('U', 'enhanced authentication (AUTH)'),
 '09-extended-auth-single.py': ('U', 'enhanced authentication (AUTH)'),
 '09-extended-auth-single2.py': ('T', 'test does not run (NameError PluginConfig); plugin (U)'),
 '10-listener-mount-point.py': ('U', 'mount_point'),
 '11-pub-props.py': ('S', B1 + ' (passes when the order is tolerated)'),
 '16-cmd-args.py': ('L', 'mosquitto command line (-h ...)'),
 '16-config-huge.py': ('L', 'mosquitto config parser + reload (plugins, password files)'),
 '16-config-missing.py': ('L', 'mosquitto --test-config'),
 '16-config-parse-errors-without-tls.py': ('L', 'mosquitto --test-config / config errors'),
}
def prefix_u(t):
    p = t[:2]
    m = {'06': 'bridges', '08': 'TLS / TLS-PSK', '09': 'plugins / password file / ACL', '13': 'websockets',
         '14': 'dynamic security plugin', '15': 'persistence plugin (persist_* module)', '16': 'TLS config parsing',
         '17': '$CONTROL API', '20': 'sparkplug-aware plugin', '21': 'PROXY protocol', '22': 'HTTP API', '23': 'password/ACL file reload'}
    if t.startswith('01-connect-unix'):
        return 'unix socket listener'
    if t.startswith('01-'):
        return 'password_file / plugin (test also requires WITH_TLS)'
    return m.get(p, 'unsupported feature')
def read_summary(path):
    res = {}
    for line in open(path):
        w = line.split()
        if len(w) >= 2:
            res[w[0]] = w[1]
    return res

if len(sys.argv) > 2:
    cur = read_summary(SUM)
    base = read_summary(sys.argv[2])
    n = 0
    for t in sorted(set(cur) | set(base)):
        if cur.get(t) != base.get(t):
            print("CHANGED %s: %s -> %s" % (t, base.get(t, 'absent'), cur.get(t, 'absent')))
            n += 1
    print("CHANGED %d test(s) against %s" % (n, sys.argv[2]))

rows = []
cnt = collections.Counter()
for line in open(SUM):
    t, rc, ms = line.split()
    if t in C and not (rc == '0' and C[t][0] in ('S', 'U', 'L', 'T')):
        cls, why = C[t]
    elif rc == '0':
        cls, why = 'PASS', ''
    elif rc == '77':
        cls, why = 'U', 'skipped (feature OFF): ' + prefix_u(t)
    elif t.startswith('09-plugin') or t.startswith('09-'):
        cls, why = 'U', 'plugin'
    elif t.startswith('15-persist'):
        cls, why = 'U', 'persistence plugin tests (need a persist_* module argument)'
    else:
        cls, why = 'NEW', 'not classified: regression or new test'
    cnt[cls] += 1
    rows.append((t, rc, cls, why))
print("COUNTS", dict(cnt))
for t, rc, cls, why in rows:
    print("| %s | %s | %s | %s |" % (t.replace('.py', ''), rc, cls, why))
