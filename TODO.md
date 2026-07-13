# TODO

Only **open** work lives here. Anything shipped is deleted from this file the
moment it ships — its record is [`CHANGELOG.md`](CHANGELOG.md) (release notes),
the docs (`yunos/c/yuno_agent/YUNO_AUTH.md`,
`docs/doc.yuneta.io/yunos/mqtt_broker.md`,
`docs/doc.yuneta.io/guide/guide_tls.md`) and git history.

## gui_treedb: leftovers from the 2026-07-13 audit

The four gclasses whose runtime lived outside the automaton are done
(`C_TRANGER_VIEW`, `C_TREEDB_CONFIG`, `C_TREEDB_LOGIN`; `C_TREEDB_LINKS` was
already fine). What the audit left open:

- **Raw `setTimeout` used as an FSM timer.** `c_treedb_links.js` (the 15 s scan
  timeout, whose callback publishes events and mutates state — the gclass has no
  `C_TIMER` child at all) and `c_treedb_view.js` (the deferred rebind, which
  destroys gobjs and swaps DOM). Both should be a `C_TIMER` pure child +
  `EV_TIMEOUT` / a dedicated event, so the deferral shows up in the trace.
- **`c_treedb_picker.js` bypasses the shell router**: `window.location.hash =
  "#/settings"` hardcodes hash routing while `use_hash` is an attr. It should
  send an event whose action calls `yui_shell_navigate`.
- **Backend features with no UI** (the SPA uses 15 of ~45 C_NODE/C_TRANGER
  commands). Highest operator value, in order: **snapshots** (`snaps`,
  `snap-content`, `shoot-snap`, `activate-snap`, `deactivate-snap` — tag a
  version, browse it read-only, roll back: all there, zero UI); **backup /
  restore** (`export-db` / `import-db`, a base64 payload maps straight to a
  browser download / file input); server-side `filter` + `size` on `nodes` (the
  topics view pulls the WHOLE topic and filters client-side); `backward` on
  `open-iterator` (newest-first paging); and the relationship inspector
  (`parents` / `children` / `links`, plus `jtree`, whose ready-made tree with
  `__path__` we ignore in favour of a flat `nodes` list).

## c_tranger: reclaim iterators of a session that never subscribes

`mt_subscription_deleted` now closes the realtime feeds and iterators a
subscriber leaked when its last subscription goes (see the `open-rt` duplicate
fix in `CHANGELOG.md`). But a client that only PAGES (`open-iterator` +
`get-page`, no `open-rt`) never subscribes to anything, so its iterators are
still reclaimed only at `mt_stop` — gui_treedb browsing Rows cards without a
Live card leaks one iterator per card per dead session. Memory only (no
duplicate records), but it needs a session-death hook that does not depend on a
subscription: the natural candidate is for the command's `src` channel to notify
the service on close.

A leaked **filtered** iterator now costs more than an empty handle: it holds its
row index (one rowid per matching record), so a leaked card over a wide time
range pins a proportional array until `mt_stop`. Same fix, higher stakes.

## Auth: OIDC migration follow-ups

- **Real-IdP smoke tests beyond Keycloak.** Auth0 / Cognito / Authentik are not
  live-tested (no tenants). Code finding from the discovery contract:
  `save_oidc_discovery` hard-requires `end_session_endpoint` and aborts
  (`STOP_TASK`) if absent. **Auth0 does NOT publish `end_session_endpoint`**
  (proprietary `/v2/logout`); some Cognito setups omit it too → discovery would
  fail. Workaround exists (set explicit `token_endpoint` +
  `end_session_endpoint`, skips discovery). **Decision needed:** relax the
  requirement (degrade to local logout when absent) vs document the
  explicit-endpoints requirement for those IdPs. Authentik exposes it.
- **ROPC → device/client-credentials migration — deferred until a non-Keycloak
  IdP is adopted.** `action_get_token` in `c_task_authenticate` uses
  `grant_type=password` (username + password + client_id, single round-trip).
  Works today because every deployed IdP is Keycloak (permits ROPC). Becomes
  necessary when an IdP that disables ROPC by default (Auth0 / Cognito / Azure
  AD / Authentik) is adopted. Not a drop-in swap to PKCE:
  - All 6 callers (`ycli`, `ycommand`, `ystats`, `ytests`, `ybatch`, `mqtt_tui`)
    are CLI/server tools with **no browser and no local HTTP listener**; the tree
    has no device-flow, loopback-redirect, or browser-open primitive. Classic
    PKCE (authorization code + loopback) does **not** fit — these run headless over
    SSH. (`c_auth_bff`'s PKCE is server-side for the web SPA, a different context.)
  - Correct replacements split by use:
    - **Interactive** (`ycli`; the others when a human runs them) → **Device
      Authorization Grant** (RFC 8628): print URL + user code, poll the token
      endpoint with `urn:ietf:params:oauth:grant-type:device_code` (handle
      `authorization_pending` / `slow_down`). Discover `device_authorization_endpoint`.
      No password in the tool; works on every IdP.
    - **Headless CI** (`ybatch`, `ytests` — no human at all) → device flow can't
      work either; use **Client Credentials** (a service-account client + secret),
      machine-to-machine. The token subject is the service account, not a user.
  - Scope when undeferred: `c_task_authenticate` (new FSMs + discovery fields +
    config attrs) + all 6 callers + tests + docs. Keep ROPC as a fallback for
    Keycloak. **Do not point any CLI at a ROPC-disabled IdP before this lands.**

## Security: per-command authz gate — production enablement

The gate (`enable_command_authz`) is **default-off** (design in YUNO_AUTH.md
§4.5). To enable in production, per node:

- provision **every principal that sends commands TO each C_AUTHZ yuno** with
  `__execute_command__`/root — not only `yuneta`/admins but the **controlcenter**
  user(s) that reach the agent's `:1993` port (the agent store currently has
  `yuneta` + `yuneta_admin@…` + `yunetas_admin@…`, NOT `yuneta_agent@…`);
- confirm each of the 5 C_AUTHZ stores (agent, agent22[shared], controlcenter,
  mqtt_broker, emailsender) holds the `root`/`yuneta` model at runtime — re-seed
  via `update-node` if a store was non-empty and missed `Authz.initial_load`;
- run a real **low-privilege deny test** on staging (needs a non-root external
  principal — infeasible on the yuneta-only local plano);
- then set `enable_command_authz: true` per yuno (pilot the agent first),
  staging → production.

Event-level authz (`EVF_AUTHZ_INJECT` / `EVF_AUTHZ_SUBSCRIBE`) is still
**declared but not enforced** — no gate exists for `gobj_send_event` /
`gobj_subscribe_event`.

## Security: ytls TLS posture — per-gate rollout

Remaining is **per-gate deployment config** (validate on staging):

- raise high-level gates explicitly where wanted; set the IoT-compat profile
  (`ssl_min_version` + `ssl_ciphers "@SECLEVEL=0"`, OpenSSL backend) on legacy
  gates;
- turn on peer verification per high-level gate (`ssl_trusted_certificate` or
  `ssl_use_system_ca`); IoT gates opt out with `ssl_allow_insecure_client=true`.
  **Done in 7.6.0:** TLS *clients* now fail closed — a no-CA client is
  *refused* at ctx/state build time (not just logged), and the `C_AUTH_BFF`
  `crypto` / `c_authz` `kc_crypto` IdP clients default to a verifying posture.
  Remaining is the per-gate **deployment** config: set the CA (or the explicit
  `ssl_allow_insecure_client` opt-out) on each client crypto block in the realm
  config, and raise the server-side gates.

## Security: MQTT broker ACL — model + default-deny decision

The publish + subscribe ACL (model A: per-group `publish_acl`/`subscribe_acl`,
`enable_acl` default off) is in the broker treedb — see mqtt_broker.md. Open
decisions (Rosa):

- the **A/B/C model choice** — A = treedb group ACL (shipped); B = reuse the
  framework `C_AUTHZ` via `gobj_user_has_authz` (one authz system, but its
  checker is per-authz-name not topic-pattern → needs extending); C = a broker
  config attr holding the pattern map (no schema migration, off the treedb/UI);
- whether to flip enforcement to **default-deny** (validate on staging first).

## Security: not yet reviewed for memory-safety

- `modules/postgres` (libpq wrapper) — delegated to libpq, lower priority.
- the `yuno_agent` control plane + `watchfs` command-exec — re-audit if the
  agent's `SDF_WR` command attrs become remote-writable (prior fixes
  `8c03eb686` / `5dbede6a1` + authz gating).

## Security: vendored libjwt — maintenance

- **Backport `jwks_*` keyring NULL-safety** (`jwks_item_get(NULL)` /
  `jwks_free(NULL)`) at the next re-vendor — the vendored v3.2.1+2 copy derefs
  `jwk_set->head` (`jwks.c:201`). Low-sev: not reachable from `c_authz` (keyring
  always valid); the regression test documents and skips it.
- **Periodic re-vendor from upstream** — procedure in
  `kernel/c/libjwt/README.md` (§ Re-vendor procedure).

## Agent: deploy UX — find-new-yunos preview

- **`find-new-yunos` preview lists rows that are already registered.**
  `cmd_find_new_yunos` (`yunos/c/yuno_agent/src/c_agent.c`) iterates **every**
  yuno row and emits a `create-yuno …` line whenever a newer binary/config
  version exists for that role. On a **resumed upgrade** (a prior run already
  ran `find-new-yunos create=1`, so the new-version rows exist, but never
  promoted them) the OLD primary rows survive and still match, so the preview
  re-lists all of them as "would be created". `create=1` then fails per row with
  "Yuno already exists". Harmless now — the CLI 0.11.1 fall-through treats that
  as idempotent and proceeds to `deactivate-snap` — but the preview is
  misleading. **Fix:** skip a row in the preview when a yuno instance at the
  target (`yuno_role`, `yuno_name`, new `role_version`/`name_version`) already
  exists (the same `gobj_list_nodes` check `create-yuno` does at its
  "already exists" guard). Consolidated project — read in depth, preserve the
  `create=1` semantics, before touching.

## Observability: source-IP attribution in decoder logs — DONE (2026-06-21)

- **`peername` added to the protocol/decoder error logs.** Protocol/decoder
  gclasses logged ERROR/WARNING events (malformed payload, unsupported HTTP
  method, missing param, bad framing/CRC, …) **without the source IP**, because
  `peername` is set on the bottom `C_TCP` at accept/connect (`c_tcp.c:483`,
  `SDF_VOLATIL`) and the upper layers never copied it into their own logs. The
  canonical read pattern (already in `c_websocket.c` / `c_prot_mqtt2.c`) is now
  applied at each remote-data parse-error log:
  ```c
  const char *peername = "";
  if(gobj_has_bottom_attr(gobj, "peername")) {
      peername = gobj_read_str_attr(gobj, "peername");
  }
  ```
  read once in the cold error branch (or once per function for the dense GPS
  decoders) and emitted as `"peername", "%s", peername`.
  - **Kernel (yunetas):** `c_prot_tcp4h.c` (head-too-long, protocol-error
    disconnect, protocol timeouts) and `ghttp_parser.c` (the invalid-UTF-8
    header-value store error; the main "non-HTTP data received" violation
    already carried peername). `c_prot_http_sr.c` and `c_channel.c` had only
    registration / internal "no bottom" logs — not remote-attributable, left
    untouched.
  - **Hidraulia (private):** `C_DECODER_HTTP` (gate_auraair, 5 logs),
    `C_DECODER_MQTT` (gate_mqtts), `C_DECODER_CAUDAL`, `C_DECODER_ENCHUFE`.
  - **Estadodelaire (private):** `C_DECODER_MQTT`, `C_DECODER_HTTP`,
    `C_DECODER_ENCHUFE`, and the raw-TCP GPS decoders `C_DECODER_GPS_ERM` (17),
    `C_DECODER_GPS_JT808` (8), `C_DECODER_GPS_TELTONIKA2` (66).
  - **Deferred (next pass, intentionally skipped):** outbound clients where
    peername is the remote *server*, low attribution value — `c_prot_http_cl.c`
    and wattyzer `C_GATE_PVPC`; and the `c_prot_mqtt2.c` gap-fill (214 logs,
    already the most-instrumented). `C_PROT_MQTT` (`modules/c/mqtt`, deprecated
    but kept in Hidraulia production) stays out of scope — do not migrate it.
  - Unrelated/already handled: `string2json`/`gbuf2json` records carry no gobj
    context by design (kernel helpers); the invalid-UTF-8-breaks-logcenter
    problem is fixed separately (glogger UTF-8 escaping).

## C_TRANGER: realtime feed (Live cards) — inotify scalability + leak

Context: `open-rt`/`close-rt` + `EV_TRANGER_RECORD_ADDED` (public) power
gui_treedb's Live records card. On a **non-master (reader)** C_TRANGER —
e.g. `db_history_wz`, `master:false` — each `open-rt` opens a
`tranger2_open_rt_disk` feed = **one inotify instance**. Two problems surface
under real use (found 2026-07-12 on e.com, where the node sat at 128/128
`fs.inotify.max_user_instances`, its default):

- **#1 — Share one rt_disk feed per topic across Live cards.** Today each Live
  card opens its own per-key feed → N cards = N inotify instances on a reader
  backend. Open a single `rt_disk` feed per topic (`key=""`, all keys),
  refcounted, and let each subscriber filter by key on its
  `EV_TRANGER_RECORD_ADDED` subscription (subscriptions cost no inotify). Caps
  usage at **1 inotify per followed topic** regardless of card count. Small,
  high-value change.

- **#2 — Tie the feed to the ievent session (root-cause of the leak).** A
  SPA that reloads/closes the tab (F5) never sends `close-rt`, so the backend
  feed — and its inotify instance — leaks until the yuno restarts (worse than
  the iterator/file-handle leak already noted, because inotify is a scarce
  per-user kernel resource). Cleanest fix: **arm/close the rt feed from the
  subscription hooks** (`mt_subscription_added` / `mt_subscription_deleted`)
  instead of standalone `open-rt`/`close-rt` commands — then the feed's
  lifetime rides the subscription, which `C_IEVENT_SRV` already drops when the
  channel closes. (General alternative: have `C_IEVENT_SRV` close a session's
  command-created iterators/feeds on channel close — also fixes the Phase-1
  iterator leak.)

Node-side mitigation (independent of the above): the default
`fs.inotify.max_user_instances = 128` is too low for a node running ~12 yunos
with rt_disk followers. **DONE** — the deb/rpm packagers now provision
`99-yuneta-core.conf` with `max_user_instances = 4096`,
`max_user_watches = 524288`, `max_queued_events = 65536`. Observe live usage
with `ycommand -c 'info-inotify'` (limits + this yuno's instances/watches).
Note this only mitigates; it does not fix #1/#2, which still leak/multiply
instances under real use.
