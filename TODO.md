# TODO

Tracks API renames, removals and additions between versions.

## Auth: OIDC migration follow-ups

The `c_auth_bff` (BFF) and `c_task_authenticate` (ROPC task) gclasses
were migrated from a Keycloak-only path scheme to standard OIDC
discovery + explicit endpoint override.

`c_task_authenticate` and its six callers (`c_cli`, `c_mqtt_tui`,
`c_ycommand`, `c_ystats`, `c_ytests`, `c_ybatch`) had their legacy
`auth_url` and `auth_system` attrs **removed** outright (no users
remained), and the `azp` attr was renamed to `client_id` to match the
form parameter actually sent on `/token` and `/logout`.

`c_auth_bff` still keeps its own `idp_url` + `realm` legacy pair,
now flagged with `SDF_DEPRECATED` so the framework emits the
standard "Deprecated attribute used in gobj creation" warning at
`gobj_create` time (one per attr).  Removal is scheduled once one
release has shipped with the warning in place.

### Smoke tests against real IdPs

Mocked tests cover the wire format; they do not cover quirks of
real IdP discovery documents.

- **Keycloak** — redeploy `auth_bff.1801.json` (already migrated
  to `issuer`) and verify login / refresh / logout work end-to-end
  against a real Keycloak realm.

- **Auth0 / Cognito / Authentik** — discovery document layouts
  differ subtly (Auth0 uses `/oauth/token`, Cognito uses
  `/oauth2/token`, Authentik uses `/application/o/token/`).
  Configure a test BFF against each and confirm the discovery
  response is parsed correctly and the follow-up token call
  succeeds. ROPC (`grant_type=password`) is **not** supported
  on most modern tenants of these IdPs by default — the BFF
  `/auth/callback` PKCE flow is the path that actually works
  cross-IdP; `c_task_authenticate` will fail there until ROPC
  is replaced (see below).

### Independent of OIDC migration

- **Replace ROPC in `c_task_authenticate`** — the task posts
  `grant_type=password` against the token endpoint. Discovery
  fixes the URL but does not fix the grant: Auth0, Cognito,
  Azure AD and Authentik either disable ROPC by default or
  refuse it for confidential clients. Migrating to PKCE
  (authorization code + code_verifier) would make the task
  work against any modern IdP, at the cost of needing a
  browser redirect — viable for `ycli` and `ycommand`
  (which already prompt the user) but not for headless
  callers like `ybatch` / `ystats` / `ytests`.  Out of scope
  for the current migration; flagged here so it surfaces when
  the ROPC failure mode hits the first non-Keycloak deployment.

## emu_device: runtime-validate the replay path

`emu_device`'s frame-emission was ported from the removed timeranger v1
API to v7 (`tranger2_open_list` + collect callback; output side built in
code like `sgateway`; `window`/`interval` emission of base64 `frame64`
records). It is **compile-verified only** — not yet runtime-tested,
because validating it needs a timeranger2 topic whose records carry a
base64 `frame64` field plus a TCP sink, neither of which was available.

When a fixture exists, verify end-to-end:

- Point `emu_device` at a `frame64` topic (`path`/`database`/`topic`) and
  a TCP sink (`url`, e.g. `nc -lk <port>`), with `window`/`interval` set.
- Confirm on connect it sends the `leading` frame then `window` frames per
  `interval`; `read-parameters` shows "frames sent" advancing and "frames
  loaded" = the number matched.
- Specifically check: `tranger2_open_list` collects via the load callback,
  the output side connects and emits, and `mt_pause` cleans up with no
  gbmem leak (run a start/stop cycle for the shutdown audit).

Note: `open_list` loads all matching records into memory (the v1 path was a
lazy cursor) — point it at a bounded range for large topics.

## Security: re-enable per-command authorization in command_parser

The `SDF_AUTHZ_X` / `gobj_user_has_authz` check at the command-dispatch
boundary in `kernel/c/gobj-c/src/command_parser.c:86-124` is **commented out**,
and has been since before the folder refactor (`299409d74`). A default global
authz checker is registered (`entry_point.c:85`) and would work if called, but
the only call site is disabled — so the 131 `SDF_AUTHZ_X` command entries carry
an authz flag that nothing reads.

Impact: **broken access control** — any *authenticated* principal can run any
`SDF_AUTHZ_X` command regardless of role/permission. This is NOT a remote-unauth
issue: authentication is enforced upstream (the `st_session` FSM gate plus the
explicit `authenticated` recheck at `c_ievent_srv.c:1492`).

Re-enabling is the canonical fix but is **not** a blind change — the role model
is data-driven (the `c_authz` treedb). Before re-arming the block:

1. Enumerate which roles/permissions the `SDF_AUTHZ_X` commands require.
2. Confirm internal `src`=self callers are granted (e.g. `c_mqtt_broker.c:4411`,
   `c_yuno.c:5305`) so internal command flows don't break.
3. Re-arm `command_parser.c:86-124`; do **not** substitute transport-layer authz
   (that proves identity, not permission).
4. Regression-test internal command paths after.

Found by a security review of gobj-c (finding f002, 2026-06-05). Note:
`gobj_user_has_authz` also fails open (`gobj.c:9451` returns TRUE when no checker
is registered) — keep in mind if the checker is ever deregistered.

## Security: ytls TLS posture (deployment decisions)

From a security review of `kernel/c/ytls` (2026-06-05). These are
configuration/posture choices, not memory-safety bugs — they need a policy
decision, so they're tracked here rather than blindly changed.

- **Protocol floor hardcoded to SSLv3/TLS1.0** (`src/tls/openssl.c:367`,
  `min_proto_version=0`). There is **no config knob** to raise it, so embedders
  can't opt into a modern floor. Decide a minimum (TLS1.2+ recommended) and
  optionally expose it via config; weigh against any legacy client that still
  needs old TLS.
- **mbedTLS `VERIFY_NONE` when no CA file is set** (`src/tls/mbedtls.c:332`);
  the server-with-CA path uses `VERIFY_OPTIONAL` with **no** `get_verify_result`
  check. Client connections without a configured CA therefore do not validate
  the peer certificate. Decide the desired default (fail-closed vs. opt-in) per
  client/server role.
- **TLS renegotiation left enabled** (`SSL_OP_NO_RENEGOTIATION` is commented
  out, openssl backend). Consider disabling unless a use case needs it.

(The memory-safety/integrity defects found in the same review — the
encrypt_data re-entrant UAF and the double `gbuffer_get` — were fixed
separately; see the `fix(ytls): re-entrant UAF in encrypt_data ...` commit.
The UAF's reachability — `SSL_write` returning WANT_* during egress while the
app frees the session from `on_clear_data_cb` — was not reproduced in a
deterministic test and would benefit from an execution-verified PoC.)

## Security: timeranger2 on-disk record hardening (contingent / local)

From a security review of `kernel/c/timeranger2` (2026-06-05). The on-disk
md2 record header (`__offset__`/`__size__`, big-endian) is read and used to
drive I/O. The dominant read sink (`read_record_content`) was hardened in the
`fix(timeranger2): validate on-disk md2 record offset/size ...` commit. The
follow-ups below are now all **DONE**; they were **contingent on the on-disk
topic files being a trust boundary** — which is LOCAL only (replication is by
hard-link, same host/fs) and OFF by default in production (`rt_by_disk=FALSE` in
`c_mqtt_broker`/`c_node`; only the `emu_device` test yuno enables it):

- **`tranger2_delete_instance` zero-payload write loop — DONE.** Before zeroing,
  it now `fstat`s the data file and rejects the wipe (LOG_CRITICAL + return -1)
  if `payload_offset`/`payload_size` (straight off the on-disk md row) don't lie
  within the file — same offset+size-vs-filesize validation as
  `read_record_content`, offset-checked first so the subtraction can't wrap.
  This removes the cross-record overwrite / data-destruction primitive a forged
  header gave. (A finer same-segment check would need segment boundaries; the
  file-bound guard is the primary fix and matches the read sink.)
- **`fs_watcher.c` inotify parse loop — DONE.** The loop now bounds each event
  to the buffer (header fits + `sizeof(event)+event->len` within `buffer+len`)
  before dereferencing `event->len`/`event->name`. Kernel-framed input (low
  attacker reach), defense-in-depth.
- **`publish_new_rt_disk_records` (`timeranger2.c`) — DONE.** Now checks
  `read_md`'s return and `continue`s on failure (the struct is uninitialized on
  a failed read, so the prior code fed garbage `__offset__`/`__size__` into
  `is_deleted_instance` / `read_record_content`).

## Security: yev_loop — event UAF fix (MERGED)

From a security review of `kernel/c/yev_loop` (2026-06-05). The DNS-parser
hardening (bounds in `static_resolv.c` + unpredictable transaction id) landed
on main.

- **F-005 use-after-free in `yev_destroy_event` — DONE (merged 60d531d31).**
  Destroying a still-running event freed the `yev_event` while its io_uring
  cancel/op CQE was still in flight; the completion later re-entered
  `callback_cqe` on freed memory (deref + indirect call through a freed
  `->callback`). Fixed with an in-flight CQE refcount + deferred free
  (`really_free_yev_event` / `track_submit` / `destroy_requested` in
  `yev_loop.c`). Merged on main (was branch `fix/yev-destroy-uaf`, commit
  394b00d5f). Caveat retained for completeness: the UAF itself was not
  reproduced deterministically (would need an abrupt destroy-while-in-flight +
  ASAN); the accounting is safe only while multishot accept stays disabled
  (`multishot_available=0`), since multishot yields many CQEs per submit.
- Lower-priority / by-design: the DNS source port is left to the OS ephemeral
  assignment (RFC 6056 randomized) rather than explicitly randomized — adequate,
  noted for completeness. `dns_parse_response` record-count/`rdlen` advances are
  bounded by the existing per-RR guards plus the F-001 decode fix.

## Security: vendored libjwt is behind upstream — follow-ups

The vendored libjwt (`kernel/c/libjwt`) is at **upstream v3.2.1+2 (375e539)**,
26 commits behind v3.3.3. A security review (2026-06-05) backported the two
reachable items that landed on main:

- **CRITICAL — done:** `49c730a` / GHSA-q843-6q5f-w55g algorithm-confusion JWT
  forgery (RSA/EC JWK used as HMAC key). Was reachable from `c_authz.c`'s verify
  loop (RS256 checker + attacker HS256 token slipped past `__verify_config_post`).
  Backported (all 4 layers). **Recommended follow-up: port upstream's
  `jwt_security.c` test suite** (76 cases incl. the forgery PoC) — yuneta has no
  jwt-level test harness, so the forgery-rejection isn't covered by a
  deterministic test here.
- **Hardening — done:** the reachable subset of `cfd8902` (JWK octet/RSA-PSS
  NULL/bounds guards in `openssl/jwk-parse.c`, `strncpy` error-copy, volatile
  constant-time compare).

The low-severity `cfd8902` remainder is now **DONE** (ranks 5-8 of the drift
doc's "to incorporate" list):
- **Key-material scrub before free** — portable `jwt_scrub_and_free()` helper in
  `jwt-private.h` (volatile secure-zero, no OpenSSL coupling for the generic
  header); HMAC key scrubbed in `jwks.c __item_free`; PEM scrubbed with
  `OPENSSL_cleanse` in `openssl_process_item_free` (different allocator, kept on
  `OPENSSL_free`).
- **`secure_getenv("JWT_CRYPTO")`** under `__GLIBC__` (with `_GNU_SOURCE`) in
  `jwt-crypto-ops.c`, falling back to `getenv` elsewhere.
- **builder/checker OOM JSON leak** — `jwt_builder_new`/`jwt_checker_new` now
  `json_decref` the object that did allocate before dropping `__cmd` (and return
  NULL explicitly).
- **Missing-alg error message** — `jwt_parse_head` writes
  `Cannot find "alg" in header` before its terminal `return 1`.

Ranks 1-4 (JWK octet bounds, RSA-PSS alg guard, `strncpy`, volatile compare)
were already backported in `f85c26031`. N/A here: the JWKS-curl
Content-Length/atol fixes (`jwks-curl.c` is disabled in CMakeLists; JWKS is
loaded from a config attr).

Remaining (no code change): **port upstream's `jwt_security.c` test suite** (no
in-tree forgery regression test) and a periodic re-vendor from upstream. Full
analysis: the security-review workspace `UPSTREAM-DRIFT.md`.

## Security: modules/yunos review — fixed + follow-ups

A security review of `modules/c` (MQTT/Modbus) and `yunos/c` (apps), 2026-06-05,
landed these fixes on main: Modbus MBAP `length<3` heap-overflow guard
(`c_prot_modbus_m.c`); dba_postgres SQL-injection (conn-less escaping in
`record2insertsql`); emailsender CR/LF header/command injection (central reject
in `tira_dela_cola`). Remaining MQTT items (modules/c/mqtt), tracked not fixed:

- **F-004 — post-publish reentrancy/UAF — ANALYZED, likely FALSE POSITIVE on the
  publish path; no code change.** In `handle__publish_s` (`c_prot_mqtt2.c`)
  `priv->protocol_version` is read and `send__puback(gobj,...)` called after the
  synchronous `gobj_publish_event(...)` "to broker". Static reachability trace:
  that publish lands in the broker's `sub__messages_queue`, which delivers to
  each subscriber via `send__publish` → `gobj_send_event(bottom, EV_TX_DATA)` →
  c_tcp **enqueues on io_uring (async)**. No `gobj_stop`/`gobj_destroy`/`EV_DROP`
  runs synchronously on the publish path; the only synchronous broker `EV_DROP`
  (`c_mqtt_broker.c:3757`) is CONNECT-time session takeover of the *previous*
  channel — a different gobj. So the publisher gobj is not freed inside
  `gobj_publish_event`, and the post-publish reads are safe. Note: were it
  reachable, a post-hoc `gobj_is_running(gobj)` guard would NOT fix it (the
  pointer would already be dangling) — it would need deferred destroy, not a
  guard. A definitive close still nominally wants the runtime PoC the review
  asked for, but the static evidence points to false-positive. Left unpatched on
  purpose (no blind guard).
- **F-005 — MQTT broker publish-side ACL missing.** Design/implement the
  `MOSQ_ACL_WRITE` check on PUBLISH in `c_prot_mqtt2.c:6524` (`C_PROT_MQTT2`),
  the complete MQTT implementation. `c_prot_mqtt.c:7349` (`C_PROT_MQTT`) has the
  same gap (`rc = 0; // TODO ...`, with the
  `if(rc==MOSQ_ERR_ACL_DENIED){...MQTT_RC_NOT_AUTHORIZED}` block already in
  place) but it is DEPRECATED — to be removed once its gates migrate to
  C_PROT_MQTT2 — so don't invest ACL work there; just be aware it remains an
  unauthorized-publish surface until it is deleted. The read/subscribe stubs in
  `c_mqtt_broker.c` (3102, 4263) are separate.
  NOTE: `mosquitto_acl_check()` is the upstream mosquitto plugin API and does
  NOT exist in this tree (only in comments) — there is nothing to uncomment.
  There is also NO authorization data model: the `users` topic in
  `treedb_schema_mqtt_broker.c` carries no per-topic publish/subscribe ACL field
  and there is no `acls` topic. Authentication exists (`mqtt_check_password` vs
  the `users` credentials); authorization must be DESIGNED (policy source +
  per-user/group topic-filter schema + a shared `mqtt_acl_check()` helper called
  from both gclasses), not merely wired. Any client that can connect (gated only
  by `allow_anonymous`) can publish to any topic. Deferred — broker-owner
  design decision.
- **F-002/F-003 — MQTT property-length underflow (LOW) — DONE in C_PROT_MQTT2.**
  `property_read` (`c_prot_mqtt2.c`) now routes every `2+slen` string/binary
  decrement through `property_len_consume()`, which rejects the packet (malformed)
  instead of letting the unsigned `*len` wrap and desync the property parse. The
  4 sites (string, binary, user-property×2) are covered. The mirror in
  `c_prot_mqtt.c` (~3463) was intentionally NOT patched — that gclass is
  DEPRECATED (see the C_PROT_MQTT deprecation note); it stays a robustness-only
  desync (bounded by `gbuffer_get`, no OOB) until removed.

Not reviewed for memory-safety (delegated to libpq / lower priority):
modules/postgres (libpq wrapper), the yuno_agent control plane and watchfs
command-exec (prior fixes 8c03eb686/5dbede6a1 + authz gating — re-audit if the
agent's `SDF_WR` command attrs become remote-writable). Findings detail in the
security-review workspace (`modules/`, `yunos/`).
