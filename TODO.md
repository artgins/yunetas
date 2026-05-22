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
  browser redirect — viable for `yuno_cli` and `ycommand`
  (which already prompt the user) but not for headless
  callers like `ybatch` / `ystats` / `ytests`.  Out of scope
  for the current migration; flagged here so it surfaces when
  the ROPC failure mode hits the first non-Keycloak deployment.

## tranger2: restore delete-record (v6 → v7)

v6 of Yuneta exposed a delete-record API on timeranger.  v7
dropped it and has not put it back yet.  When a real consumer
needs it, this is the plan.

The log files stay **append-only** — deletion does not rewrite
the log.  The per-key **index is mutable**, and the flag
`sf_deleted_record = 0x0400` is already reserved for this
purpose at `kernel/c/timeranger2/src/timeranger2.h:138` (with
its own `// TODO no used by now` comment) and enumerated in
the string table at `kernel/c/timeranger2/src/timeranger2.c:60`.

- **Index-level delete**: set `sf_deleted_record` on the
  indexed entry so every reader (typed scans, `rt_by_disk`
  followers, `list-*` commands, treedb lookups) skips it.
  This is enough for normal "logically deleted" semantics.
- **Optional record-bytes zeroing**: overwrite the record
  payload in the log file with zeros.  Useful when the record
  carries sensitive data and the on-disk bytes must not stay
  recoverable even though the index already hides it.
- v6 already had a working implementation — its index-mutation
  pattern is the natural reference when this lands.

Until then, the "clean up a few records" operator workflow is
cosmetic only: re-append the same key with a sentinel value /
cleanup-tagged source so the last-wins-per-key projection
hides the original.  Full wipe still requires stopping the
yuno and removing the topic store, which drops legitimate
data with it.
