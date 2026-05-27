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

## ycommand stdin-pipe drops responses after large install-binary

**`fix(ycommand): drop responses lost after large install-binary in stdin-pipe`**

The long-lived stdin-pipe session added in 7.4.3 works fine for small
payloads but loses the response of the SECOND and subsequent commands
when an earlier command in the pipe is an `install-binary` carrying a
~40 MB base64 body. Minimal reproduction against the local agent
(`ws://127.0.0.1:1991`, no network in between):

```bash
{ echo 'install-binary id=emailsender content64=$$(emailsender)'
  sleep 8
  echo 'list-binaries'
} | ycommand --wait=15
```

Expected: two responses — a `version: 7.4.3` line for the
install-binary and the `Total: 25` table from list-binaries.
Observed: only the list-binaries response is rendered. The
install-binary response either never arrives or never gets matched
back to its command.

Control: three back-to-back `list-binaries` calls in the same pipe
shape produce three full `Total: …` responses — the gate
(`priv->cmd_in_flight`) and the dispatch loop are sound for small
messages.

Practical impact: when deploying multiple yunos in one batch, the
trailing `find-new-yunos` + `deactivate-snap` get dropped silently,
leaving the agent with the new binary slots registered but the
running yunos still on the old release. Discovered during the
7.4.3 deploy to `app.wattyzer.com` — the workaround is to issue
each `install-binary` as a separate ycommand call (≈400 ms extra
ROPC re-auth per command, acceptable for occasional deploys).

`c_ycommand.c` looks correct in isolation: `exec_one_command`
returns 1 on async dispatch, `cmd_in_flight` is set, and
`ac_command_answer` clears it + drives the next pending. The
issue is almost certainly below that layer — likely in either
the wss frame fragmentation in `c_websocket.c` for the 40 MB
outbound message, or in the request/response correlation in
`c_ievent_cli.c` when the request body is large enough that the
agent's ack races something on the client side. Reproducible
without network, so it is not a wss-on-the-wire problem.

Investigation path:
- Add a trace of WS frames (in + out) and confirm the install-
  binary RESPONSE actually leaves the agent and reaches the
  client.
- Confirm the `__md_iev__.iev_id` (or whatever correlation key
  ycommand uses) on the lost response matches the in-flight
  request id.
- Check whether c_ievent_cli silently drops a reply when the
  client is mid-send of a different (queued) outbound message.

Workaround stays as a documented operator note in the meantime;
no immediate user-facing breakage other than the noisier deploy.
