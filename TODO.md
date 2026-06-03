# TODO

Tracks API renames, removals and additions between versions.

## docs: bring back graphical/diagram docs (v7 lost v6's visuals)

Yuneta is a graph system (treedb = nodes/links, gobj tree, FSMs) but the v7
docs (yunetas, mystmd) are text + tables — zero diagrams. The v6 philosophy doc
(`^yuneta/docs/yuneta/filosofia.rst`) embedded interactive draw.io graphs via
`.. raw:: html` + `app.diagrams.net`, and framed the model as **Entidad/
Relación** with **CRUDLU** (Create/Read/Update/Delete + Link/Unlink). Reinstate
visuals in the onboarding docs + doc.yuneta.io using `mermaid` (mystmd-native,
also rendered by GitHub in-repo — more maintainable than draw.io embeds):

- the **CRUDLU** model over Entidad/Relación (recover the v6 lámina);
- the **treedb graph** (topics/nodes/hooks/fkeys; left-to-right, port-colour =
  linkable topic);
- the **yuno lifecycle FSM** (create→run→play→pause→kill→delete), today prose in
  `YUNO_LIFECYCLE.md` §4.

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
