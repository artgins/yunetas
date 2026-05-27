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

## ycommand stdin-pipe drops responses for large install-binary

**`fix(ycommand): response of large install-binary intermittently lost in stdin-pipe`**

The long-lived stdin-pipe session added in 7.4.3 is correct for
small payloads but drops responses for `install-binary` (~40 MB
base64 body) at a roughly fixed rate. Investigated 2026-05-27
against the local agent (`ws://127.0.0.1:1991`, no network).
Findings:

**Repro (test H), 4 identical large install-binary in pipe:**

```bash
{ for i in 1 2 3 4; do
    echo 'install-binary id=emailsender content64=$$(emailsender)'
    sleep 3
  done
} | ycommand --wait=15
```

Expected: 4 response lines (here `ERROR -1: Binary already
exists` because the slot is full, but a real response is
delivered).
Observed: **3** response lines. One in the middle is missing.

**Control (small payloads work):**

```bash
{ echo 'list-binaries'; sleep 1
  echo 'list-binaries'; sleep 1
  echo 'list-binaries'
} | ycommand --wait=15   # → 3× "Total: 25" reliably
```

**The dropped response paradox.** If `cmd_in_flight` actually
held until response arrived, ycommand would HANG on the lost
response and never dispatch the next command. But it does
dispatch the next command. So the gate is being cleared without
`ac_command_answer` having rendered the missing response. That
points at either:

- A reply path that flips `cmd_in_flight = FALSE` and continues
  without invoking `display_webix_result`, or
- The reply IS being received but discarded before reaching
  `ac_command_answer` (e.g. in `c_ievent_cli`'s correlation
  layer, with a stale `__md_iev__` id from the previous large
  request that hasn't been fully serialised yet).

**Operational impact.** The 7.4.3 deploy pipeline (shoot-snap +
3× install-binary + find-new-yunos + deactivate-snap) hit this
on `app.wattyzer.com` — the 3 binary slots all got registered
(agent side: confirmed) but the trailing `find-new-yunos` /
`deactivate-snap` responses got swallowed and the running yunos
were left on the old release until I re-ran those two commands
manually. Effective workaround in the meantime: issue each
`install-binary` as a separate ycommand call (≈400 ms extra
ROPC re-auth per command, acceptable for occasional deploys).

**Where to look next.**

- `c_ycommand.c` flow is internally consistent
  (`exec_one_command` returns 1, sets `cmd_in_flight`;
  `ac_command_answer` clears it + drives `run_next_pending`).
  The bug is below this layer.
- Trace WS frames in/out at the agent — confirm install-binary
  RESPONSE is actually emitted (one suspected case: agent emits
  it but on a half-fragmented frame that ycommand reassembles
  as a no-op).
- `c_websocket.c` write path on large outbound frames: when
  ycommand is mid-write of a 40 MB request, what happens if
  the agent emits a response to a previous command back at it?
  Frame interleaving / RX buffer overrun?
- `c_ievent_cli.c` request/response correlation: confirm there
  is a request-id field and the matching is strict (and not
  just FIFO-positional, which would corrupt under reordering).

**Workaround (documented for operators):** when deploying many
yunos at once, do the install-binary calls as separate `-c`
invocations and use stdin-pipe only for the trailing
small-message commands (`find-new-yunos`, `shoot-snap`,
`deactivate-snap`).
