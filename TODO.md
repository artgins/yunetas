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
base64 body) **as a function of inter-command sleep**. Investigated
2026-05-27 against the local agent (`ws://127.0.0.1:1991`, no
network). The first hypothesis — WS frame interleaving in
c_websocket — was **ruled out** after reading the kernel TX path:
both the client side (ac_send_message builds one combined frame
and dispatches a single EV_TX_DATA) and the server side (header +
payload as two EV_TX_DATA events through c_tcp's FIFO dl_tx queue)
are serial; nothing on the wire interleaves.

The actual signal is timing:

**Timing matrix (all against local ws://127.0.0.1:1991):**

| Test | N dispatches | sleep | Responses |
|------|--------------|-------|-----------|
| K    | 2            | 1 s   | 1         |
| L    | 2            | 5 s   | 2 ✓       |
| M    | 3            | 3 s   | 2         |
| H/I  | 4            | 3 s   | 3         |
| J    | 6            | 5 s   | 6 ✓       |

The losing pattern is **inter-command sleep < server processing
time** (one install-binary on this host takes ≈ 4-5 s of CPU/disk
for the base64 decode + gbuf2file + treedb lookup). With sleep ≥ 5
every request completes its round-trip before the next echo from
the pipe arrives, and no response is dropped. With sleep < 5,
roughly one in N responses goes missing — `--wait=60` does not
recover them.

**Reproducer (test K — minimum):**

```bash
{ echo 'install-binary id=emailsender content64=$$(emailsender)'
  sleep 1
  echo 'install-binary id=emailsender content64=$$(emailsender)'
} | ycommand --wait=20
```

Expected: 2 `ERROR -1: Binary already exists` lines (slot already
filled). Observed: 1.

**Control (small payloads work):**

```bash
{ echo 'list-binaries'; sleep 1
  echo 'list-binaries'; sleep 1
  echo 'list-binaries'
} | ycommand --wait=15   # → 3× "Total: 25" reliably
```

**Agent-side smoking gun.** Every test run ends with the agent
logging `cause: "Connection reset by peer"` on the input-* channel
that served the ycommand session — i.e. the CLIENT is closing the
TCP rather than sending a WS close. ycommand reaches `exit()` while
the agent still has unread responses buffered, the kernel turns
that into RST, and any reply still in flight at that moment is
lost.

Why does ycommand exit early on the small-sleep case but not on
the big-sleep case? Hypothesis (not yet confirmed with
instrumentation):

- With sleep ≥ server processing time, each request's response
  arrives before the next echo, so `cmd_in_flight` stays TRUE
  while the NEXT line is being read from stdin → no exit timer
  scheduled.
- With sleep < processing time, the stdin reader buffers all N
  lines into `pending_commands` quickly, EOF is reached early,
  and the `ac_command_answer` post-response check
  (`queue empty && stdin_closed && !cmd_in_flight → set exit
  timer`) can fire transiently between a response landing and
  the next `run_next_pending` dispatch — scheduling exit before
  the last response actually arrives.

That or the c_websocket / c_ievent_cli ASYNC dispatch chain is
flipping `cmd_in_flight = FALSE` on a path other than the real
`ac_command_answer` (e.g. an EV_TX_READY arriving from c_tcp
gets accidentally treated as a command response somewhere).
Needs instrumentation to confirm.

**Operational impact.** The 7.4.3 deploy pipeline (shoot-snap +
3× install-binary + find-new-yunos + deactivate-snap) hit this
on `app.wattyzer.com` — the 3 binary slots all got registered
(agent side: confirmed) but the trailing `find-new-yunos` /
`deactivate-snap` responses got swallowed and the running yunos
were left on the old release until I re-ran those two commands
manually. Effective workaround in the meantime: issue each
`install-binary` as a separate ycommand call (≈400 ms extra
ROPC re-auth per command, acceptable for occasional deploys).

**Where to look next** (in c_ycommand.c, the bug is on the client
side; c_websocket TX is clean):

- Add a counter trace to `exec_one_command` (dispatch count) and
  `ac_command_answer` (response count) — confirm whether the
  agent is even being asked for the missing response, or the
  client never sent the request.
- Confirm the exit-timer scheduling logic at lines 2614-2622 of
  `c_ycommand.c` cannot fire while another response is still
  in flight from a previous command.
- Inspect `priv->stdin_closed` transitions: maybe it's being
  set TRUE before the read truly hit EOF (e.g. a partial-read
  error path that flags stdin_closed prematurely).
- Verify on the agent side that the matching install-binary
  REQUEST actually arrived (use `set-gclass-trace gclass=
  C_IEVENT_SRV set=1 level=ievents2` and grep for the cmd
  string in the latest agent log). If the request never landed,
  the client is dropping it before sending.

**Workaround (documented for operators):** when deploying many
yunos at once, do the install-binary calls as separate `-c`
invocations and use stdin-pipe only for the trailing
small-message commands (`find-new-yunos`, `shoot-snap`,
`deactivate-snap`). Alternative: insert a `sleep` between
install-binary lines that is ≥ the server-side processing time
(≈ 5 s for a 40 MB binary on a fast local host, more over wss
to a remote agent).
