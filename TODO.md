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

- **F-005 — MQTT broker publish-side ACL missing.** Design/implement the
  `MOSQ_ACL_WRITE` check on PUBLISH in `c_prot_mqtt2.c:6524` (`C_PROT_MQTT2`,
  the complete implementation; the `c_prot_mqtt.c` mirror is DEPRECATED — don't
  invest ACL work there). `mosquitto_acl_check()` does NOT exist in this tree
  (comments only), and there is **no authorization data model**: the `users`
  topic in `treedb_schema_mqtt_broker.c` carries no per-topic publish/subscribe
  ACL field and there is no `acls` topic. Authentication exists
  (`mqtt_check_password` vs the `users` credentials); authorization must be
  **DESIGNED** (policy source + per-user/group topic-filter schema + a shared
  `mqtt_acl_check()` helper called from both gclasses), not merely wired. Any
  client that can connect (gated only by `allow_anonymous`) can publish to any
  topic. Deferred — broker-owner design decision. The read/subscribe stubs in
  `c_mqtt_broker.c` (3102, 4263) are separate.

Not reviewed for memory-safety (delegated to libpq / lower priority):
modules/postgres (libpq wrapper), the yuno_agent control plane and watchfs
command-exec (prior fixes 8c03eb686/5dbede6a1 + authz gating — re-audit if the
agent's `SDF_WR` command attrs become remote-writable). Findings detail in the
security-review workspace (`modules/`, `yunos/`).
