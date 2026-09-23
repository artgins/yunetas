# TODO

Only **open** work lives here. Anything shipped is deleted from this file the
moment it ships — its record is [`CHANGELOG.md`](CHANGELOG.md) (release notes),
the docs (`yunos/c/yuno_agent/YUNO_AUTH.md`,
`docs/doc.yuneta.io/yunos/mqtt_broker.md`,
`docs/doc.yuneta.io/guide/guide_tls.md`) and git history.

## Schema editing: the admin console

The console is gui_agent's **Schemas** workspace (design and traps in
`yunos/js/gui_agent/README.md`); the backend's gaps are in the next section.

What that workspace still lacks:

- **Applying needs an agent carrying the `ac_final_count` fix of 7.13.0.** The workspace
    does it now (`kill-yuno` → `run-yuno play=0` → `play-yuno`, confirmed
    first), but those commands answer through `ac_final_count()`, which dropped
    the answer of any client behind a controlcenter until now. Against an older
    agent the sequence stops at the first step with the yuno KILLED and not
    restarted; the tab gives up after 30 s and says so, but the yuno stays
    down until somebody runs it. Deploy the agent before using Apply on a
    node.

- **Authz is the yuno's, and nobody provisions it.** The commands run in the
    target yuno with the logged-in identity, so a console user needs the `read`
    / `create` / `update` / `delete` authz of that `C_NODE` in **that yuno's**
    `C_AUTHZ`. Today an operator who can log into the controlcenter still gets
    `-403` per topic on a yuno where they have no role. Part of the authz-roles
    work, not of the console.

    Worth knowing before diagnosing one: **`descs` and `nodes` take DIFFERENT
    authz** (`a_schema` and `a_nodes` in `c_node.c`). An identity that has one
    and not the other loads the schema and is then refused per topic, so the
    view fills its tabs and answers `-403` for every table — which reads as a
    broken build and is a missing role. Verified on the agents' own treedbs
    with `claudia@artgins.com` (2026-08-19).

## Schema editing: what the `__system__` treedb still needs

The meta-treedb is filled, reconciles by `schema_version` and rebuilds a schema
(7.13.0, `YUNO_TREEDB.md` §3.11). What it does not do yet:

- **Nothing is ever removed from a projection.** By design: an element that is
    in `__system__` and not in the incoming schema cannot be told apart from an
    operator addition. The consequence is that a topic or column dropped from
    the C literal lingers in `__system__` until somebody removes it explicitly,
    and with `impose_c_schema=0` it stays in the schema the treedb opens
    with. Telling the two cases apart needs state the projection does not carry
    (which side wrote each element), so decide that before adding a rule.

- **Not every treedb is projected at all.** `C_AUTHZ` creates its `C_NODE`
    directly instead of going through `C_TREEDB`'s `open-treedb`, so
    `treedb_authzs` never reaches `__system__` and cannot be edited. Any other
    direct `C_NODE` consumer is in the same position.

- **A removed column with data behind it is still nobody's problem.** The
    write guard refuses what cannot produce a working schema, but dropping a
    column that has values in the topic's records is a legal schema and a data
    decision: the records keep the field, every reader stops showing it, and
    nothing says so. Deciding what the GUI does here (warn, refuse, offer to
    keep it hidden) needs the record side, not the schema side.

- **Applying an edit costs a `pause-yuno` + `play-yuno`.** That is the whole
    yuno, not just the treedb: its gate goes down for the cycle, and any client
    connected to it — the editor included — has to reconnect. Reopening only the
    treedb is not available to a third party and should not be: `close-treedb`
    destroys services whose handles the owner has cached (it now refuses while
    the yuno plays). A true in-place reload would live in the owner, as a local
    method it implements — "reload your schema" — using
    `tranger2_write_topic_cols()` (written, called by nobody) plus
    `parse_schema_cols()` + `parse_hooks()` and a link reload when hooks or
    fkeys change.

## TreeDB / timeranger2: open findings of the 2026-09-15 review

What is still open of the first review (the rest is in `CHANGELOG.md`).

**Found while fixing (2026-09-15)**

- **`test_c_node_link_events` fails now and then** -- twice on 2026-09-15, both
  inside runs of several suites, never alone (15/15 twice). Its message was
  not kept. Next time it fails, keep `build/Testing/Temporary/LastTest.log`
  before running anything else: ctest overwrites it.

**Medium**

- **tr_treedb**: `treedb_delete_instance()` does not unlink, and
  that matters in ONE case only (analysed 2026-09-15): the loader links only the
  `id` index (`load_all_links()`) and hooks dedup by child id, so a non-primary
  CHILD instance never sits in a parent's hook. (That premise was false for a
  DICT hook until M15 of the 2026-09-21 review: it took the newest instance.
  It holds now -- a dict hook keeps the primary.) A non-primary PARENT instance
  can hold children linked during the session; deleting it leaves them under no
  visible parent until the reload re-hangs them from the primary (their fkey
  names only the id). The agent's `delete-config` / `delete-binary` refuse a
  version still in use ("Using in N yunos") unless `force=1`. Low impact; a
  fix would move those children to the primary's hook.
- **gobj-ui: found while fixing, not from the review and not fixed** (all LATENT, a
  static scan of who hosts whom): three hosts do not declare an output event
  of a gobj they subscribe to. `C_YUI_NODE` creates `C_YUI_NAV` as a pure
  child (`c_yui_node.js:705/760/969`) and declares neither
  `EV_NAV_ITEM_CLOSE` nor `EV_DRAWER_CLOSE_REQUESTED`, which `c_yui_nav.js`
  really does publish -- the likeliest of the three to bite.
  `C_YUI_TREEDB_TOPIC_WITH_FORM` creates `C_YUI_JSON` as a pure child
  (`:2522/2651`) without declaring `EV_EXPAND_PATH`, which only fires on a
  `__collapsed__` sentinel and those come from the backend, not from a schema
  or a cell. `C_YUI_TREEDB_TOPICS` hosting `C_YUI_TREEDB_SCHEMA` is the
  documented opt-in case, not a defect.

**Tests nobody has** (in order of damage): `delete_instance` with links.
C_NODE commands with no ctest: `node`, `instances`, `pkey2s`, `jtree`,
`parents`, `children`, `hooks`, `links`, `treedb-info`, the snap commands
(their permissions are tested, their behaviour is not), `import-db` /
`export-db` and `print-tranger`. The refusals on a replica are tested since
7.25.0 (`test_c_node_authz`) and, for C_TREEDB, since the 2026-09-23 round.
In gobj-ui, the treedb views got their first wiring tests on 2026-09-23
(`test/dom_double.js`); the save kw as it leaves `publish_treedb_write` is
still untested.

## TreeDB / timeranger2: open findings of the 2026-09-21 review

The second review (read at 7.24.1) shipped in 7.25.0 and in gobj-ui
7.23.193-7.23.196 / gui_treedb 0.17.52-0.17.54 / gui_agent 0.22.74 (see the
`CHANGELOG.md` of each). What its fixes left open:

- **A1-A3:** C_NODE's `snap-content` does not ask
  `treedb_is_treedbs_topic()` -- it can no longer leave the database, but it
  can read a topic of the tranger that is not a topic of that treedb.
- **M36:** every in-tree yuno forces `impose_c_schema`, so gui_agent's Apply
  is off on all of them until one stops forcing it.

**Low, worth keeping** (M23, M34 and M40 were lowered to here by their
verifiers):
the `EV_TREEDB_NODE_*` feed is outside the `read` permission, because the subscription authz is commented out
(`c_ievent_srv.c:1373`, `gobj.c:8754`); a refused `__graphs__` write is
recorded as saved and never retried (`c_g6_nodes_tree.js:3282`);
refs `topic^id^hook` are written with `snprintf`
into `char[NAME_MAX]` and truncate silently; hook membership is tested by bare
id, so two children of different topics with one id collide; an id holding `^`
is accepted and makes every ref to the node undecodable; JS `kw_get_str()`
stringifies its default (`0` becomes the truthy `"0"`); `cmd_treedbs` /
`cmd_links` / `cmd_hooks` still pair `json_incref(kw)` with the wrong decref;
the warning *"Parent
ref already in child fkey"* still fires in the legitimate case of 4e4dcdc00,
once per `create-yuno`.

## TreeDB / timeranger2: what the reviews of 2026-09-23 left open

Three independent reviews of the 7.25.4 fixes and four fix rounds
(`CHANGELOG.md`, Unreleased). What is still open:

- `rmrdir()` (gobj-c helpers) uses `stat()`, which follows symlinks: a dangling
  symlink makes it fail, and a symlink to a directory makes it walk into the
  target and delete what is there. Use `lstat()` and never descend a link.
- **The tm marker after a rollback**: a 7.25.4-or-earlier binary appends
  out-of-order tm without `.tm_unordered`; `mark-tm-order` re-marks the topic,
  but nothing does it on its own. A per-writer stamp would make it automatic.
- A marker that cannot be written is retried only by the next append to the
  same FILE; a file never appended again keeps it missing (logged) until
  `mark-tm-order` runs.
- `mark-tm-order` runs synchronously and blocks the yuno (linear; ~80 ms for
  4 keys x 3 650 files on a warm cache).
- A demoted master never takes its lock back while the process lives; it needs
  a restart (documented).
- treedb deletes of a parent do not see links from children that did not load
  (a topic with `load_failed` keys); in a partial topic, operations on other
  ids are allowed.
- **`treedb_delete_instance()`**: a tombstone write that fails partway still
  logs and answers 0.
- A C literal and an operator's saved draft of the same topic are not merged:
  the literal wins (said as `withdrawn_at_open`).
- A store whose schema file is already behind what runs (written whole by an
  older release) stays inconsistent for that topic (the running definition is
  only in `topic_cols.json`).
- A draft column whose `order` is not its position (e.g. 99) reads as unsaved
  right after a save.
- **A multi-key (`rkey`) iterator** does not see keys created after it opened;
  the `key_deleted` mark reaches only the process that deleted the key. Both
  are single-node conveniences by design (philosophy.md, "the key").
- `import-db` keys its error-count stats on `gobj_log_last_message()`.
- ***"Child node without fkey field"*** is logged as an ERROR at every open,
  once per node, when an fkey column is filled by no hook any more.
- gobj-ui `C_YUI_TREEDB_TOPICS`: a topic-table write in flight when the session
  drops refreshes the topic after the failure, and the adapter logs
  *"cannot route 'nodes' -- not in session"* (rare).
- gui_agent: an apply that times out with one owner applied and another silent
  ends with no restart.
- The agent's `audit/` directory grows ~0.6-1 GB a day with no retention
  (19 GB on wattyzer, 90 GB on the dev machine).
- An md2 whose last row write was torn (size not whole rows) is an
  unacknowledged append but stays "damage": truncate it to whole rows instead.
- **No red test** for: `deactivate-snap` -1 on a failed save, the fs_watcher
  root, `save_json_to_file()`'s `close()` failure, the crash window between a
  marker and its md2 row. Not exercised live: a form Save through a real
  websocket drop.
- A test binary is not relinked by `cmake --build build` after `make install`
  of a library it links by name: a per-module test run can execute the old
  library. `yunetas clean && yunetas build && yunetas test` is not affected.

## Agent: the spare agent is only refreshed on the package path

`install.sh` restarts `yuneta_agent22` after an upgrade, once the main agent is
confirmed healthy on the new binary (7.12.0). That covers the **runtime** nodes,
which install the `.deb` / `.rpm`.

It does not cover the nodes that **build from source**: there the binaries come
from `yunetas build` and both agents are restarted by hand. Nothing checks, and
nothing says the spare is stale — which is exactly how it went five days unnoticed
on four nodes, running the version-comparison bug 7.12.0 fixes.

The natural home is the `yunetas` CLI, since it is what put the binaries there:
after a build that replaced `/yuneta/agent/*`, restart the main agent, verify it,
then the spare. Same order as `install.sh` — the spare is the only way into a node
whose main agent is broken, so it is never touched first.

The REPORT half is done: `tools/agent/audit-agents.sh` (see `tools/README.md`).
What is left is the automation: the `yunetas` CLI restarting the agents itself
after a build that replaced `/yuneta/agent/*`, main first and the spare only
once the main one is confirmed healthy.

## gui_treedb: leftovers from the 2026-07-13 audit

One item, and it is a feature project rather than a leftover:

- **Backend features with no UI** (the SPA uses 15 of ~45 C_NODE/C_TRANGER
  commands). Highest operator value, in order: **snapshots** (`snaps`,
  `snap-content`, `shoot-snap`, `activate-snap`, `deactivate-snap` — tag a
  version, browse it read-only, roll back: all there, zero UI); **backup /
  restore** (`export-db` / `import-db`, a base64 payload maps straight to a
  browser download / file input); and the relationship inspector
  (`parents` / `children` / `links`, plus `jtree`, whose ready-made tree with
  `__path__` we ignore in favour of a flat `nodes` list).

## Resolver: name resolution still blocks the event loop

7.8.2 cached DNS answers and made the cost visible (`getaddrinfo() BLOCKED the
event loop`, plus the syslog trail from `static_resolv.c`). That bounded the
blast radius of the `central.yunovatios.es` outage — a black-holed first
`nameserver` costing ~6 s per lookup — but the shape of the problem is intact:

- **`getaddrinfo()` runs synchronously inside the loop** (`yev_loop.c`, three
  call sites: connect, source bind, listen). Every gobj, timer and pending
  completion in the process stops for the duration. The cache means one lookup
  per host per TTL instead of one per connect, but that first lookup still
  freezes everything. A resolution that cannot block would have to be an async
  step in the FSM, like any other I/O — which is the framework's own rule.
- **Nameservers are tried strictly in order, 3 s x2 each**
  (`YUNETA_DNS_QUERY_TIMEOUT`, A then AAAA). A dead first entry always costs
  6 s before the second is reached. Options, cheapest first: drop the per-query
  timeout, remember which nameserver last answered and start there, or query
  them concurrently and take the first reply.

Neither is urgent while nodes have a working `resolv.conf`; both are what turns
a misconfigured node from an outage into a log line.

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
  mqtt_broker, emailsender) holds the `root`/`yuneta` model at runtime — a
  store that missed `Authz.initial_load` re-seeds itself on its next master
  start (`C_NODE` applies the attr), so this is a check, not a repair;
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
  Remaining is the per-gate **deployment** config: set the CA (or the explicit
  `ssl_allow_insecure_client` opt-out) on each client crypto block in the realm
  config, and raise the server-side gates.

## Security: `denied_ips` is never consulted at accept

`c_yuno` keeps the two lists — `allowed_ips` and `denied_ips` (`SDF_PERSIST`,
with their `list-`/`add-`/`remove-` commands) — and exports both readers,
`is_ip_allowed()` and `is_ip_denied()`. But the accept path
(`c_tcp_s.c`, `yev_callback` on `fd_clisrv`) asks only **`is_ip_allowed`**, and
only when the gate carries `only_allowed_ips`. So:

- **the allow-list works** (whitelist mode, loopback exempted);
- **the deny-list does not stop a connection.** `is_ip_denied()` has exactly
  one caller, `c_authz.c:802`, so a denied ip is refused at *authentication* —
  which means an unauthenticated gate (an IoT/mqtt field port) accepts it,
  builds its channel tree, and lets the protocol run.

Found while banning an internet scanner off a yunovatios mqtt gate: adding the
scanner's ip to `denied_ips` would have changed nothing on that port, so the
ban had to be keyed by the identity the peer announces instead (project-side,
`gate_energia.denied_clients`).

Open decisions before wiring `is_ip_denied()` into the accept path:

- **cost per accept**: it is a json dict lookup per connection, the same one
  `is_ip_allowed` already pays, so only on the gates that have a list;
- **whether the check belongs in `c_tcp_s` or in `c_iogate`** — dropping at
  accept never spends a channel of the pool, which is the point;
- **`only_allowed_ips` is a badly named door**: it gates the whole ip check,
  so a gate that wants a deny-list today has to turn on whitelist mode. The
  deny-list should apply unconditionally when non-empty.

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

## Observability: source-IP attribution in decoder logs — remaining pass

The `peername` roll-out across the protocol/decoder error logs shipped
2026-06-21 (kernel + hidraulia + estadodelaire); its record is `CHANGELOG.md`
and git history. What was intentionally skipped and is still open:

- The scope rule is `CLAUDE.md`'s decoder-severity question, *"could a
  remote peer trigger this with bad bytes?"* (`c_prot_mqtt2.c` and
  `c_prot_http_cl.c` are done, 7.22.0).

- **Still open: wattyzer `C_GATE_PVPC`** — an outbound client too, and so
  likely the same answer as `c_prot_http_cl.c` (nothing a peer can trigger; the
  url or the endpoint name is the useful field). Unlooked-at. It lives in the
  wattyzer repo, not here.

- **Out of scope, do not migrate:** `C_PROT_MQTT` (`modules/c/mqtt`) is
  deprecated but still in Hidraulia production.

The canonical read pattern is the one in `c_websocket.c` / `c_prot_mqtt2.c`:
read `peername` off the bottom gobj once, in the cold error branch.

## C_TRANGER: realtime feed (Live cards) — inotify scalability

Context: `open-rt`/`close-rt` + `EV_TRANGER_RECORD_ADDED` (public) power
gui_treedb's Live records card. On a **non-master (reader)** C_TRANGER —
e.g. `db_history_wz`, `master:false` — each `open-rt` opens a
`tranger2_open_rt_disk` feed = **one inotify instance**. The problem surfaced
under real use (found 2026-07-12 on e.com, where the node sat at 128/128
`fs.inotify.max_user_instances`, its default):

- **#1 — Share one rt_disk feed per topic across Live cards.** Today each Live
  card opens its own per-key feed → N cards = N inotify instances on a reader
  backend. Open a single `rt_disk` feed per topic (`key=""`, all keys),
  refcounted, and let each subscriber filter by key on its
  `EV_TRANGER_RECORD_ADDED` subscription (subscriptions cost no inotify). Caps
  usage at **1 inotify per followed topic** regardless of card count. Small,
  high-value change.

  Note (2026-07-14): a Live card now subscribes filtering on **its own feed's
  `rt_id`**, not on the key — with SEVERAL feeds alive, a `{topic, key}` filter
  matches every publish of that key and the cards double each other's rows.
  Under this design there is only ONE feed, so its publishes all carry the same
  `rt_id` and the subscribers MUST go back to filtering by key: whoever
  implements it has to flip `live_filter()` in `c_tranger_view.js` in the same
  change, or the cards go silent.

  **Read in depth 2026-09-16, and it is NOT the small change this entry calls
  it.** What `open-rt` returns today IS the feed: `cmd_open_rt` opens one
  `tranger2_open_rt_mem/disk` per `rt_id` and `register_handle()` files it, so
  one client = one feed = one inotify. Sharing means the `rt_id` a client gets
  back stops naming a feed and starts naming a SUBSCRIPTION to a shared one,
  and everything keyed on that assumption moves with it:

  - `cmd_close_rt` must decrement a refcount and close the underlying feed only
    at zero, instead of closing what it finds;
  - `reap_handles_of()` (the session-death and subscriber-death reaper) must
    decrement too, not `tranger2_close_list()` — otherwise one dead session
    takes the feed away from every other card on that topic;
  - `publish_rt_callback()` stamps the SHARED feed's `rt_id` into every
    publish, which is exactly why the SPA has to filter by key again;
  - the master path (`rt_mem`) has no inotify and no such problem, so the
    sharing is only worth it on a reader — but doing it on one side only
    leaves two publish contracts, and the SPA cannot tell which it is talking
    to. Decide whether the shared feed is unconditional.

  So it is one design change across C and the SPA, landing in the same release
  with a coordinated deploy, plus a test that opens two cards on one topic and
  counts inotify instances (`info-inotify`). Worth doing — the node sat at
  128/128 — but not in passing.

Node-side mitigation (already provisioned, independent of the above): the deb/rpm
packagers ship `99-yuneta-core.conf` raising the default
`fs.inotify.max_user_instances` of 128 — too low for a node running ~12 yunos
with rt_disk followers — to 4096 (`max_user_watches = 524288`,
`max_queued_events = 65536`). Observe live usage with
`ycommand -c 'info-inotify'` (limits + this yuno's instances/watches). It only
raises the ceiling: **#1 still multiplies instances per Live card**, which is
what the remaining work above fixes.

## Packaging: the sparse SDK in the `.deb` serves one glibc at a time

The `.deb` installs a sparse SDK under `/yuneta/development/yunetas`
(`outputs/`, `outputs_ext/`, `tools/`, `.config` — no sources) so a node can
compile a project against the published runtime without a source tree. That
promise does not hold today, and cannot hold for more than one glibc at a time.

The shipped `outputs/lib/*.a` are **static** archives: they reference glibc
internals (`_dl_x86_cpu_features`, backing the ifunc `memcpy`/`strlen`
dispatch) whose layout moves between releases. Linking fresh objects against
them under a different glibc succeeds silently and corrupts the heap at run
time — SIGABRT inside `_int_malloc` seconds after start, no framework error
first. `tools/cmake/libc_guard.cmake` stops it at configure time via
`outputs/lib/yuneta_libc.stamp`.

Since 7.8.6-3 the `.deb` is built in a `debian:13` container (glibc 2.41),
matching Debian 13 nodes, which can build. Before that it came off an
`ubuntu-22.04` runner (glibc 2.35) that **no node ran**, so the guard fired
everywhere and the sparse SDK was dead weight in the package. (The EL9 `.rpm`
never had that problem: `rockylinux:9`, glibc 2.34, matching Rocky 9 nodes. The
guard compares only `major.minor`, so EL9 point releases — 2.34-231 vs
2.34-272 — do not break it.)

So the promise now holds, but for exactly one distro per package: an Ubuntu
22.04/24.04/26.04 node still cannot compile against the shipped `.deb`, and
neither can Debian 12. Moving the base moved the boundary; it did not remove
it, which is what the options below are about.

Options, in rough order of cost:

- **Drop the build half of the `.deb`** (leaves `outputs/lib`, headers and
  `.config` out; keeps the runtime binaries). Honest about what the package is,
  and matches how deploys already work — binaries are built on a dev machine
  and pushed with `yunetas sync-binaries`. **Current preference.** Note this is
  *not* a size argument: the archives and headers are ~77 MB of a 1.1 GB tree
  (the static yunos are 944 MB of it), so the `.deb` would barely shrink. The
  reason is that the package ships, documents and maintains a capability the
  guard blocks on every node.
- **Build the `.deb` on a matrix** (22.04 / 24.04 / 26.04) and publish one per
  base. Keeps static linking and keeps the sparse SDK working. The fallback if
  on-node compilation is ever needed again. What it actually costs:
  - **The external archives must be rebuilt per base too**, not just the SDK
    ones — and they are the bulk: 19 of the 31 archives and 61 MB of the 71
    (OpenSSL, mbedTLS, pcre2, ncurses, liburing, jansson). The workflow already
    builds them from source (`extrae.sh` + `configure-libs.sh`), so this is
    runner time, not new machinery; jobs run in parallel, so wall-clock stays
    near the current ~15 min.
  - **Asset selection becomes real work.** Three `.deb`s instead of one means a
    naming scheme and an `install.sh` that detects the distro *version*, not
    just the family (`apt` vs `dnf`, all it does today) — plus a new failure
    mode when a node's version is not covered.
  - **Unknown: third-party code under much newer compilers.** 22.04 ships gcc
    11, 26.04 ships gcc 15. Whether OpenSSL/ncurses/pcre2 build clean four gcc
    majors forward is untested here; assume it needs work before costing this
    option.
- **Ship shared libraries instead of static archives.** glibc versions its
  symbols, so a `.so` built against the oldest supported glibc links and runs
  on every newer one — one artifact, no matrix. It gives up the
  `CONFIG_FULLY_STATIC` property for the SDK libs, which is a deliberate
  feature of this project, so it is a real trade, not a free win.
- **Distribution packaging** (Debian/Fedora build against their own glibc).
  Correct by construction and the highest cost by far: their policies, their
  schedule, their review, and a version lag we do not control.

Not a route: **snap / flatpak**. Snap confinement grants only `$HOME`, so a
snap-delivered toolchain cannot read `/yuneta/...` — this repo already hit that
with snap-packaged CLI tools (see the note in `CLAUDE.md`), and the agent
itself writes `/yuneta`, spawns yunos, uses io_uring and dumps cores to
`/var/crash`, all of which confinement exists to prevent. What *does* work in
that family is an **OCI image as the build environment** — a container pinned
to the glibc the archives were built against, i.e. a portable form of the
matrix option.

Decide in the cold. Nothing here is urgent while every node is ours and no one
compiles on one — and less urgent since 7.8.6-3, which at least aims the one
supported glibc at a distro that is actually deployed.

