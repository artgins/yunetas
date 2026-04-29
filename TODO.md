# TODO

Tracks API renames, removals and additions between versions.

## Auth: complete OIDC migration

The `c_auth_bff` (BFF) and `c_task_authenticate` (ROPC task) gclasses
were migrated from a Keycloak-only path scheme to standard OIDC
discovery + explicit endpoint override (commits `b916096`, `1e9a543`).
Items below close out the migration.

### Deprecated, scheduled for removal

Targeted for removal once all callers are migrated and one release
has shipped with the deprecation warning in place.

- **`c_auth_bff` attrs `idp_url` and `realm`** — replaced by `issuer`
  (with discovery) or `token_endpoint`+`end_session_endpoint`
  (explicit). `mt_create` already emits a warning when these are
  used. Production batch `auth_bff.1801.json` already migrated to
  `issuer`.

- **`c_task_authenticate` attr `auth_url`** — replaced by `issuer`
  or explicit endpoints. `mt_start` already emits a warning. Six
  callers below still set this attr.

- **`c_task_authenticate` attr `auth_system`** — was a no-op since
  at least 2024 (the `SWITCHS` body falls through identically for
  any value). Kept only for back-compat of the six callers below.

### Callers to migrate

Six gclasses still pass `auth_url`+`auth_system` to
`C_TASK_AUTHENTICATE` from CLI args / batch JSON. Each needs a new
`--issuer` (and/or `--token-endpoint` / `--end-session-endpoint`)
option, the existing `--auth-url` flag wired to the legacy attr,
and a deprecation note in the help text.

- `yunos/c/yuno_cli/src/c_cli.c` (lines 280–281, 1723–1785)
- `yunos/c/mqtt_tui/src/c_mqtt_tui.c` (lines 177–178, 447–449, 913–914)
- `utils/c/ycommand/src/c_ycommand.c` (lines 211–212, 428–430, 748–749)
- `utils/c/ystats/src/c_ystats.c` (lines 48–49, 141–143, 185–186)
- `utils/c/ytests/src/c_ytests.c` (lines 60–61, 170–172, 222–223)
- `utils/c/ybatch/src/c_ybatch.c` (lines 61–62, 167–169, 219–220)

### Test coverage gaps

- **No tests for `c_task_authenticate`.** No unit tests at all
  exist under `tests/c/c_task_authenticate/`. A mock-IdP harness
  similar to `tests/c/c_auth_bff/c_mock_keycloak.c` would let us
  exercise the discovery + token + logout chain, the legacy
  `auth_url` deprecation warning, and the
  `result_save_discovery` failure path that publishes
  `EV_ON_TOKEN` with result=-1.

- **No regression test for the BFF legacy path.** Tests 1–7 and
  10 use `issuer`; 8/9/11/12 use explicit endpoints. The
  `idp_url`+`realm` deprecation warning emitted by `mt_create`
  is not covered by any test. A 17th test that sets the legacy
  attrs and asserts the warning fires (and the flow still
  works) would close this gap before the attrs are removed.

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
