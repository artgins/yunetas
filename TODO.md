# TODO

Tracks API renames, removals and additions between versions.

> The 2026-06 security-hardening batch (gobj-c / root-linux / ytls / yev_loop /
> timeranger2 / libjwt / modbus / dba_postgres / emailsender / mqtt) shipped in
> **7.5.2** — see [`CHANGELOG.md`](CHANGELOG.md). Only the still-open follow-ups
> remain below.

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

## emu_device: finish the output-side replay path

`emu_device`'s frame-emission was ported from the removed timeranger v1 API to
v7. A 2026-06-06 runtime validation (fixture generator added at
`yunos/c/emu_device/tests/gen_frame64_fixture.c` — builds a timeranger2 topic
of base64 `frame64` records; driven against a TCP sink) found the **replay/load
path works** but the **output side had never run** and carried a chain of
integration bugs:

**Validated working:** `mt_play` opens the v7 tranger, `tranger2_open_list` +
the collect callback loads every `frame64` record into memory (e.g. 5/5),
config (`url`/`path`/`topic`) applies, autoplay fires, and `ac_on_open` is
delivered (`is_playing=1`).

**Fixed (this commit):**
1. C_TCP's client attr is `url` (string); `create_output_side` passed
   `urls`:[…] (array) → "GClass Attribute NOT FOUND: urls", url never set.
   Now passes `url`.
2. C_TCP is `gcflag_manual_start`, so `gobj_start_tree` skips it and the
   transport never started/connected. Now started explicitly (as every C_TCP
   client does, cf. `c_prot_http_cl.c:188` / `c_websocket.c:336`). With 1+2 the
   sink now sees a connection.

**Still open — emission (0 bytes at the sink despite a connection):**
`send_frame_b64` sends `EV_SEND_MESSAGE` to the `C_IOGATE` (`__output_side__`),
whose `ac_send_message` routes via `send_one_rotate` over its pool of *opened*
channels. The hand-built `C_CHANNEL` is not registered in that pool (the manual
`create_output_side` stack doesn't integrate with C_IOGATE's channel
management), so nothing is emitted. Fix options: send `EV_SEND_MESSAGE`
directly to the hand-built channel/`C_PROT_RAW` instead of the gate, or make
the channel register with the IOGATE pool. Then re-verify end-to-end:

- Build a fixture: `gen_frame64_fixture <path> <db> <topic> <n>`; sink with
  `nc -lk <port>` (or the python sink used in validation).
- `emu_device --url=tcp://127.0.0.1:<port> --path=<path> --database=<db>
  --topic=<topic> --window=1 --interval=300`.
- Confirm the `leading` then `window` frames/`interval` arrive at the sink;
  `read-parameters` shows "frames sent" advancing, "frames loaded" = matched;
  and a start/stop (play/pause) cycle is gbmem-leak-clean (the binary's
  `set_memory_check_list` reports at exit).

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

Re-enabling is the canonical fix but is **not** a blind change. A deeper
analysis (2026-06-06) found a migration landmine that makes a naive re-arm
unsafe:

### The deny-all landmine

`entry_point.c` registers `authz_checker` (from `c_authz.c`) as the global
checker **by default in every root-linux yuno** (`entry_point.c:85,499`). That
checker is **fail-closed**: if it can't find a running `C_AUTHZ` service
(`gobj_find_service_by_gclass(C_AUTHZ, TRUE)`), it returns `FALSE` — deny.
But a `C_AUTHZ` service is instantiated only by a few yunos (mqtt_broker,
yuno_agent, controlcenter); **most yunos don't run one.**

So simply un-commenting `command_parser.c:86-124` would, on every yuno without
a `C_AUTHZ` service, **deny all ~133 `SDF_AUTHZ_X` commands** — including
internal flows (cert reload, node/treedb CRUD). Widespread breakage. The
`gobj.c:9451` fail-open (`TRUE` when no checker) is *not* reached in practice
because the checker is always registered.

### Resolution model (verified)

`gobj_user_has_authz(gobj, "__execute_command__", kw, src)` →
per-gclass `mt_authz_checker` (none defined) → global `authz_checker` →
else `TRUE`. `authz_checker` pulls `__username__` from `kw`, else from
`src`'s attr; missing → deny. `__execute_command__` is a global authz
(`gobj.c` `global_authz_table`); permissions come from the `c_authz` treedb
role model. There is **no `src==gobj` self bypass** today, so internal
`gobj_command(child, "...", ..., child)` calls (e.g. `c_yuno.c` cert-reload
walk, `c_yuno.c:5305/5350`) would be denied.

### Re-arm plan (for the framework owner to decide)

1. **Pick a migration posture** (policy call):
   - *Gated opt-in (recommended):* a yuno attr `enable_command_authz`
     (default FALSE); the check runs only when ON. Zero breakage by default;
     authz-using deployments opt in. Closes the hole where authz is actually
     configured.
   - *Fail-open without C_AUTHZ:* re-arm + make `authz_checker` **allow** when
     no `C_AUTHZ` service is configured (enforce only where authz exists).
   - *Strict:* always enforce; breaks every non-authz yuno until roles are
     configured.
2. **Add a `src==gobj` self bypass** (needed in all postures) so internal
   self-issued commands aren't denied — at the `command_parser` SDF_AUTHZ_X
   gate or in `gobj_user_has_authz`.
3. **Fix the leak in the commented block**: the `kw_authz` built with
   `json_pack`/`json_object_set` is never `decref`'d — must be freed on re-arm.
4. Enumerate which roles/permissions the `SDF_AUTHZ_X` commands require
   (data-driven via the `c_authz` treedb).
5. Confirm internal `src`=self / `__username__`-bearing callers are granted
   (e.g. `c_mqtt_broker.c:1008/1124` pass `__username__` in `kw` — good
   pattern; the `c_yuno.c` cert walk relies on the self bypass).
6. Re-arm `command_parser.c:86-124`; do **not** substitute transport-layer
   authz (that proves identity, not permission).
7. Regression-test internal command paths after.

Found by a security review of gobj-c (finding f002, 2026-06-05; deepened
2026-06-06). **Deferred to the framework owner** — the posture in step 1 is a
deployment-policy decision, not a mechanical fix. No code changed yet.

## Security: ytls TLS posture (deployment decisions)

From a security review of `kernel/c/ytls` (2026-06-05). These are
configuration/posture choices, not memory-safety bugs — they need a policy
decision, so they're tracked here rather than blindly changed. (The memory-safety
defects from the same review — the `encrypt_data` re-entrant UAF and the double
`gbuffer_get` — shipped in 7.5.2.)

**Config knobs added 2026-06-06 (non-breaking); secure-by-default flips applied
2026-06-07.** The protocol-floor and renegotiation *defaults* have now been
flipped to the secure posture (TLS1.2 floor, reneg disabled) — these ARE
behavior changes, but each downgrade and each rejection is logged (yuneta "no
silent" norm) so the reduced-security gates and the rejected legacy peers are
enumerable. The IoT escape hatch is preserved (lower the floor / re-enable
reneg per-gate). The mbedTLS verify *defaults* are deliberately left as-is
(IoT). Per-gate config rollout + the OpenSSL-verify gap remain (see below):

- **Protocol floor** — `ssl_min_version` knob
  (`SSLv3`/`TLS1.0`/`TLS1.1`/`TLS1.2`/`TLS1.3`).
  **DONE — secure-by-default flip on branch `security/tls-floor-policy`**
  (2026-06-07, pending review/merge). Both backends now floor at **TLS1.2 by
  default** (mbedTLS 4.x can't go lower anyway; OpenSSL default flipped from the
  historical SSLv3/TLS1.0 floor). The IoT escape hatch is preserved: an OpenSSL
  gate can still LOWER the floor to `TLS1.0`/`TLS1.1` (paired with
  `ssl_ciphers "@SECLEVEL=0"`); mbedTLS cannot — legacy peers must use the
  OpenSSL backend. The flip is acceptable because neither the downgrade nor the
  rejection is silent (yuneta norm):
  - an explicit floor **below TLS1.2** logs a `gobj_log_warning` at ctx build
    ("legacy floor below TLS1.2 / IoT-compat downgrade") → downgraded gates are
    enumerable;
  - a peer **rejected by the floor** logs a default-on warning in
    `do_handshake()` ("TLS handshake rejected", with the OpenSSL/mbedTLS reason
    + offered version) → the legacy IoT devices that need an opt-down are
    identifiable from the logs. Peer address comes from the transport gobj's
    drop log for the same connection.
  - `ssl_min_version` knob also ADDED to the mbedTLS backend (was OpenSSL-only)
    for config portability (accepts `TLS1.2`/`TLS1.3`; can only raise).
  - Test: `tests/c/ytls/test_tls_floor_openssl.c` — (A) explicit downgrade is
    logged; (B) a real TLS1.0 ClientHello is rejected by the default floor and
    traced. Full suite green.
  - Remaining deployment step (Rosa): roll out per-gate configs — raise
    high-level gates explicitly where wanted, set the IoT-compat profile on the
    legacy gates — validated on staging (e.com) before prod (h.es).
- **TLS renegotiation** (`src/tls/openssl.c`) — `ssl_disable_renegotiation`.
  **DONE — flipped to secure-by-default** (branch `security/tls-posture-knobs`,
  2026-06-07). Default is now DISABLED (`SSL_OP_NO_RENEGOTIATION`; TLS1.3 has no
  reneg anyway). Set `ssl_disable_renegotiation: false` to re-enable on a gate
  that needs it — that explicit downgrade logs a `gobj_log_warning`
  ("renegotiation explicitly enabled", enumerable). Test C in
  `test_tls_floor_openssl.c`.
- **mbedTLS verify mode** (`src/tls/mbedtls.c`) — `ssl_verify_mode` knob
  (`required`/`optional`/`none`). The `get_verify_result` gap is **DONE** (same
  branch): after a successful handshake the backend now calls
  `mbedtls_ssl_get_verify_result()` and, when a CA was configured
  (`has_ca_cert`), logs a `gobj_log_warning` decoding the flags if the peer cert
  did NOT verify — so a cert tolerated under `VERIFY_OPTIONAL` is no longer
  silently accepted. The accept/reject decision is unchanged.
  **Defaults deliberately NOT flipped** (no CA → `NONE`; server+CA →
  `OPTIONAL`; client+CA → `REQUIRED`): flipping no-CA→fail-closed would break
  IoT (PSK/self-signed/no-CA), so fail-closed stays opt-in via
  `ssl_verify_mode=required`. The decision to require verification on specific
  gates is a per-gate deployment call.
- **OpenSSL peer verification — still a GAP** (`src/tls/openssl.c`). The
  OpenSSL backend never calls `SSL_CTX_set_verify`, so its default verify mode
  is `VERIFY_NONE`: a **client does not validate the server certificate** (MITM
  surface) and a server never requests/verifies client certs even with a CA
  loaded. Flipping this on by default is breaking (every gate would need a CA
  configured), so it needs its own decision + an `ssl_verify_mode`-style knob
  for the OpenSSL backend (symmetry with mbedTLS). Not done here.

## Security: vendored libjwt — maintenance follow-ups

The GHSA-q843-6q5f-w55g algorithm-confusion fix, the full `cfd8902` hardening,
and an in-tree regression test (`tests/c/libjwt/test_jwt_alg_confusion.c`) all
shipped in 7.5.2. The forgery test was **broadened** (malformed JWKs, malformed
/ `none` / `null` tokens, and `jwt_checker_*` NULL-safety, ported from upstream
`tests/jwt_security.c` — 48 checks). Remaining (no code change):

- **`jwks_*` keyring NULL-safety not backported.** Upstream hardens
  `jwks_item_get(NULL)` / `jwks_free(NULL)` against a NULL set; the vendored
  v3.2.1+2 copy still derefs `jwk_set->head` (`jwks.c:201`) and crashes. NOT
  reachable from `c_authz` (keyring always valid), so left as a low-sev drift
  item rather than backported; the regression test documents and skips it. Fold
  into the next re-vendor.
- **Periodic re-vendor from upstream** — the vendored copy is at v3.2.1+2
  (`375e539`); step-by-step procedure documented in
  [`kernel/c/libjwt/README.md`](kernel/c/libjwt/README.md) (§ Re-vendor
  procedure). N/A here: the JWKS-curl `Content-Length`/`atol` fixes
  (`jwks-curl.c` is disabled in CMakeLists). Full drift analysis: the
  security-review workspace `UPSTREAM-DRIFT.md`.

## Security: MQTT broker — open follow-ups

The Modbus / dba_postgres / emailsender / MQTT property-length-underflow fixes
shipped in 7.5.2. The F-004 post-publish UAF was analyzed and ruled a likely
false positive (publish delivery bottoms out in async io_uring `EV_TX_DATA`, so
the publisher gobj isn't freed synchronously inside `gobj_publish_event`). Open:

- **F-005 — MQTT broker publish-side ACL missing.** Any client that can connect
  (gated only by `allow_anonymous`) can publish to ANY topic. The
  `MOSQ_ACL_WRITE` check on PUBLISH in `c_prot_mqtt2.c` (~line 6557; the gate
  has the denial scaffolding — `rc=0` hardcoded, then `MQTT_RC_NOT_AUTHORIZED →
  process_bad_message`) is the only enforcement point to add. **Work only in
  `c_prot_mqtt2.c`** (`C_PROT_MQTT2`, the complete impl); `c_prot_mqtt.c` is
  DEPRECATED — do not touch.

  **Design proposal (deferred to the broker owner — 2026-06-06 analysis).** The
  building blocks already exist; what's missing is the ACL data + the check:
  - *Identity:* the connection carries `__username__` (SDF_VOLATIL, set after
    `gobj_authenticate`); `clients` rows fkey to `client_groups`.
  - *Matcher:* `topic_matches_sub()` (`c_mqtt_broker.c:2121`) already does MQTT
    wildcard (`+`/`#`) matching — reuse it.
  - *Recommended model — group-based ACL in the treedb:* add `publish_acl` and
    `subscribe_acl` cols (arrays of topic-filter patterns) to the
    `client_groups` topic in `treedb_schema_mqtt_broker.c` (bump
    `schema_version`/`topic_version`; treedb migrates additively). A shared
    helper `mqtt_acl_check(broker, username_or_client, topic, access)` in
    `c_mqtt_broker.c` resolves the client's group and matches the topic against
    that group's pattern list via `topic_matches_sub`. Scales per-group (not
    per-user) and integrates with the existing treedb/UI.
  - *Wiring:* `c_prot_mqtt2.c` asks the broker over an event (the in-code TODO's
    suggestion: "create a new event and publish to broker, if returns 0 is
    permitted") — add e.g. `EV_MQTT_ACL_CHECK` handled by `c_mqtt_broker`,
    returning allow/deny. One helper, two call sites (publish here; subscribe
    later).
  - *Default posture (backward-compatible):* gate enforcement behind a broker
    attr `enable_acl` (default FALSE) OR treat "no ACL patterns defined for the
    group" as allow-all — so existing brokers (wattyzer/estadodelaire) don't
    break until ACLs are authored. Flip to default-deny is a later, separate
    decision (validate on staging first).
  - *Alternatives considered:* (B) reuse the framework C_AUTHZ via
    `gobj_user_has_authz` — one authz system, but its checker is per-authz-name
    allow/deny, not topic-pattern, so it'd need extending + couples MQTT to the
    global authz treedb; (C) a broker config attr holding the pattern map — no
    schema migration, fastest, but config-file-managed and off the treedb/UI.
  - *Symmetry:* the read/subscribe stubs (`c_mqtt_broker.c:3102`, `4263`;
    `c_prot_mqtt2.c` SUBSCRIBE/UNSUBSCRIBE) should use the same helper once the
    model lands. Out of scope until the model is chosen.

  No code changed — the model choice (A/B/C) and default posture are the
  broker-owner's call.

Not reviewed for memory-safety (delegated to libpq / lower priority):
modules/postgres (libpq wrapper), the yuno_agent control plane and watchfs
command-exec (prior fixes 8c03eb686/5dbede6a1 + authz gating — re-audit if the
agent's `SDF_WR` command attrs become remote-writable). Findings detail in the
security-review workspace (`modules/`, `yunos/`).

## Security: cross-cutting path/buffer hardening

Root causes behind the per-sink path-traversal fixes shipped in 7.5.2
(`c_node` content64, `cmd_install_binary`, export-db, `C_RESOURCE2`,
agent read-* confinement). These are hot shared helpers in `gobj-c`, so they
were deferred from the 7.5.2 batch for separate review.

- **`build_path()` (helpers.c) — DONE.** The first variadic segment is now
  treated as a trusted, immutable root; `.`/`..` in every *subsequent* segment
  are resolved lexically and clamped so they can never rise above it
  (`clamp_path_tail()`). A `../../etc/passwd` tail is confined under the root
  (`/yuneta/store/etc/passwd`), never escapes it. Defence-in-depth behind the
  per-sink validators — no caller passes intentional `..` (all runtime segments
  are filenames/ids), and no real `http://` URL flows through (the lone `url`
  segment is a dotted identifier), so behaviour for legitimate callers is
  unchanged. Also fixed a missing `NULL` sentinel at a `readdir` caller
  (helpers.c) that walked `va_arg` past its args (UB). Unit test:
  `tests/c/build_path/test_build_path.c` (19 cases incl. the
  traversal-confinement set); full kernel suite (timeranger2/treedb/kw/…)
  green against the reinstalled lib.

- **`gbuffer_cur_rd_pointer()` (gbuffer.h) — DONE.** Added a central NULL guard
  (`if(!gbuf) return NULL;`) to the three inline pointer accessors
  `gbuffer_cur_rd_pointer` / `gbuffer_cur_wr_pointer` / `gbuffer_head_pointer`,
  hardening the whole content64/base64 NULL-deref family at the source.
  Zero-regression: the guard only fires when `gbuf == NULL`, which already
  crashed (`gbuffer_create` always allocs `data` for a valid gbuf, or returns
  NULL), so valid usage is unchanged; it just turns a SIGSEGV into a NULL
  return that the common `json_string(...)` sinks absorb gracefully. The guard
  is NOT silent — it emits `gobj_log_error(0, LOG_OPT_TRACE_STACK, ...)` (same
  style as `gbuffer_incref`/`gbuffer_decref`), so a NULL gbuf still surfaces a
  logged error + backtrace to chase the root cause instead of relying on the
  former crash/"Daemon relaunched" signal. Per-site guards remain where
  explicit error logging is wanted. Unit test:
  `tests/c/gbuffer/test_gbuffer_guards.c` (NULL paths + valid path + the
  json_string sink); full kernel suite green incl. the gbuffer-serialize
  heavy tests (tr_msg, msg_interchange).
