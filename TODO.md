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
