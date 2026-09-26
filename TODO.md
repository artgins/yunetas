# TODO

Only **open** work lives here. Anything shipped is deleted from this file the
moment it ships — its record is [`CHANGELOG.md`](CHANGELOG.md) (release notes),
the docs (`yunos/c/yuno_agent/YUNO_AUTH.md`,
`docs/doc.yuneta.io/yunos/mqtt_broker.md`,
`docs/doc.yuneta.io/guide/guide_tls.md`) and git history.

## Defects open after 7.25.5 (the next review round starts here)

7.25.5 was released after twenty review rounds with these items found and not
yet fixed. Each is a defect, not a design choice: fix it with a test that fails
first.

- **C_WEBSOCKET after `drop()`**: a TCP read still pending delivers `EV_RX_DATA`
  to a C_WEBSOCKET already in `ST_DISCONNECTED` ("Event NOT DEFINED in state").
  Seen when a peer sends more frames after one C_IEVENT_SRV closes the channel on.
- **`ac_identity_card` (C_IEVENT_SRV, before authentication)** logs errors with
  the whole kw dumped, once per connection: a peer-caused condition, so a capped
  warning (decoder severity rule).
- **C_TCP_S `connxs` / `tconnxs`** are `SDF_STATS` without `mt_reading`: they
  always read 0.
- **gobj-js subscriptions**: a repeated `__own_event__` / `__rename_event_name__`
  subscription is probably made twice, as C did before 7.25.5 (C fixed in
  `gobj_subscribe_event()` with `_subscription_match_kw()`).
- **C_PROT_MQTT2 `db__message_update_outgoing()`**: "QoS mismatch" is still an
  ERROR; the other sites are a WARNING plus a protocol error.
- **Agent audit `peer_field()`**: when the redacted copy cannot be allocated it
  writes the raw peer text (out-of-memory path only).
- **timeranger2.c ~7505**: the log says "stat() FAILED" where the call is now
  `lstat()`.
- **C_TRANGER handles opened through the agent** are not reaped when the
  operator's session ends (documented; they can pile up).
- **`tests/c/tr_treedb_delete_instance/README.md`** lists about half of its cases.
- About 20 older code comments say "up to this fix" without naming a version.

## Schema editing: the admin console

The console is gui_agent's **Schemas** workspace (design and traps in
`yunos/js/gui_agent/README.md`); the backend's gaps are in the next section.

What that workspace still lacks:

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

- **Applying an edit restarts the whole yuno** (`kill-yuno` → `run-yuno
    play=0` → `play-yuno`), not just the treedb: its gate goes down for the cycle, and any client
    connected to it — the editor included — has to reconnect. Reopening only the
    treedb is not available to a third party and should not be: `close-treedb`
    destroys services whose handles the owner has cached (it now refuses while
    the yuno plays). A true in-place reload would live in the owner, as a local
    method it implements — "reload your schema" — using
    `tranger2_write_topic_cols()` (called only by `tranger2_create_topic()`;
    no reload path uses it) plus
    `parse_schema_cols()` + `parse_hooks()` and a link reload when hooks or
    fkeys change.

## TreeDB / timeranger2: open items

**A flaky test**

- **`test_c_node_link_events` fails now and then** -- twice on 2026-09-15, both
  inside runs of several suites, never alone (15/15 twice). Its message was
  not kept. Next time it fails, keep `build/Testing/Temporary/LastTest.log`
  before running anything else: ctest overwrites it.

**Tests nobody has**: C_NODE commands whose behaviour has no ctest (their
permissions are tested, and some of their refusals, in `test_c_node_authz`):
`node`, `instances` (only its *"What topic_name?"* refusal is tested),
`pkey2s`, `jtree`, `parents`, `children`, `hooks`, `links`, `treedb-info`,
and what the snap commands do to the data (`shoot-snap`, `activate-snap`,
`deactivate-snap`: their answers are tested in `test_c_node_authz`, the
refusal of `snap-content` for another treedb's topic in test 20 of
`c_node_link_events`, and `gc-assets` under an active snap in `c_assets`,
case 7a), and `print-tranger`; `import-db` and `export-db` are tested only
for their error count by cause, their link failures and abort, and the file
name of the export, and a content that is not json (`c_node_link_events`,
tests 14-17 and 19). The refusals on a replica are tested since
7.25.0 (`test_c_node_authz`) and, for C_TREEDB, since 7.25.4.
In gobj-ui, the treedb views got their first wiring tests on 2026-09-23
(`test/dom_double.js`); the save kw as it leaves `publish_treedb_write` is
still untested.

## TreeDB / timeranger2: what 7.25.0 leaves open

What the changes of 7.25.0 (and of gobj-ui 7.23.193-7.23.196 / gui_treedb
0.17.52-0.17.54 / gui_agent 0.22.74, see the `CHANGELOG.md` of each) leave
open:

- **Apply is off on every in-tree yuno:** each one forces `impose_c_schema`,
  so gui_agent's Apply is off on all of them until one stops forcing it.

## TLS: `bad record mac` on a loaded server when the host is short of memory

Found 2026-09-26 in yunovatios' stress test, on the DEV machine only. A
`gate_central` (C_TCP_S, TLS, openssl) fed by 30 TLS clients of
`sim_controllers` (C_QIOGATE -> C_PROT_TCP4H -> C_TCP), ~3000 frames/s plus
backlogs of up to 10000 unacknowledged messages per link, logged `SSL_read()
FAILED` in `flush_clear_data` with error `167772441` = `0x0A000119`,
`SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC`, about 1.5 per second, steady; each
one drops the link, which then resends its whole window. The SAME traffic in
plain `tcp://` to the same gate: no protocol warning at all and no disconnect,
~8800 frames/s accepted. The same test on the central node (TLS, 3000/s): 0.

What differs is the host: 31 GB with ~300 MB free and the swap FULL. A MAC
failure is a corrupted TLS stream, and pressure is what makes short writes and
short reads likely, so the suspicion is a partial io_uring write (or read)
resumed from the wrong place in the TLS path of `C_TCP` / `ytls` -- on either
side, since the server is the one that reports it. Not reproduced under control
yet: next step is a test that forces short writes on a TLS client and checks the
peer decrypts. Repro as found: `yunovatios/yunos/sim_controllers` against its
stress realm on a host under memory pressure.

## timeranger2: a NEGATIVE `from_t` matches no record, silently

Found 2026-09-26 in yunovatios' stress test. `get_segments()` adjusts a
`from_t` below the key's first `t` to "from the start", but the per-record check
of `tranger2_match_metadata()` (`if(md_record_ex->__t__ < from_t)`, since
`0547bf2eb`, 2024-11-24) compares the `uint64_t` `__t__` with the RAW `json_int_t`
of the match_cond: a negative `from_t` becomes a huge unsigned number and every
record is left out. `tr2list <topic> --key=K --from-t=-86400` answers 0 records
where `--from-t=0` answers 997.

It is not academic: FOUR `db_history` yunos open their realtime list with
`"from_t", -3600*24` and the comment *"recupera desde el último día"* --
yunovatios (`db_history_ce`/`_co`), hidraulia, estadodelaire and wattyzer. The
value looks like the relative semantics of the pre-v7 timeranger; under
timeranger2 their start-up load hands the callback NOTHING, so whatever arrived
while the yuno was stopped (or behind) is never processed: in `raw_tracks`, never
in the history nor in its alarms. yunovatios fixes its own caller; the other
three projects are untouched and still carry it.

Decide the contract and apply it in BOTH places: either a negative `from_t` /
`from_tm` is relative (to now? to the key's last `t`?) as the callers believed,
or it is refused with a log. What it must not do is what it does: be accepted and
match nothing.

## C_NODE: every link collapses the WHOLE parent, O(children) per link

Found 2026-09-26 in yunovatios' stress test (`db_history_ce`, 20000 new devices
filed under three `device_types`). Without `with_link_events`, which is the
default, `_link_nodes()` publishes the backward-compatible
`EV_TREEDB_NODE_UPDATED` of the PARENT, and `C_NODE`'s `treedb_callback()`
answers it with `node_collapsed_view()` of that parent -- every hook list, the
ids of every child included. A parent with thousands of children costs that
much PER LINK: filing a device took ~70 ms of cpu, the history fell from
3000 to ~13 new devices/s, and a fleet of N new devices costs O(N^2). Profiled
with gdb: 10 of 15 samples in `apply_child_list_options()` under
`release_treedb_events()` of `treedb_link_nodes()`.

It only bites the FIRST link of each child (steady state does not link), but
that is exactly the moment a whole installation comes on line. Options, not
decided: publish the parent without its child lists (the subscriber that needs
the children asks for them), collapse only when the event has subscribers, or
make `with_link_events` the default once no v1 SPA depends on the parent's
update (estadodelaire and hidraulia still do -- see its memory note).

## Agent: a yuno that lost its channel is launched AGAIN while it still lives

Found 2026-09-26 on the dev node (yunovatios' `sim_controllers`). The yuno was
alive but stuck loading 13.7 M queued messages, and when the agent restarted it
could not keep the channel: `ac_on_close` logged *"yuno down"* and, as the yuno
is `must_play`, `run_yuno` launched a SECOND process at once. The first still
held its persistent queues' exclusive lock, so the second opened them *"as not
master"*, its first `trq_append()` failed and `C_QIOGATE` aborted (*"Message NOT
SAVED in the queue"*, `LOG_OPT_ABORT`: a core of 2.6 GB). Seconds later the old
one reconnected and the agent killed it (*"yuno ALREADY living, killing new
yuno"*, which kills the one reconnecting, here the OLD one).

A dropped channel is not a dead process: before relaunching, the agent should
check that the pid it knows (or `yuno.pid`) is gone, and give a live one time to
come back instead of starting a twin that fights it for its stores.

## TreeDB / timeranger2: what 7.25.5 leaves open

What the changes after 7.25.4 (`CHANGELOG.md`, Unreleased) leave open:

- **The tm marker after a rollback**: a 7.25.4-or-earlier binary appends
  out-of-order tm without `.tm_unordered`; `mark-tm-order` re-marks the topic,
  but nothing does it on its own. A per-writer stamp would make it automatic.
- A marker that cannot be written is retried only by the next append to the
  same FILE; a file never appended again keeps it missing (logged) until
  `mark-tm-order` runs.
- `mark-tm-order` runs synchronously and blocks the yuno (linear; ~19 ms for
  1 key x 30 files x 20 000 rows, `perf_timeranger2`).
- A demoted master never takes its lock back while the process lives; it needs
  a restart (documented).
- In a partial topic, operations on other ids are allowed (creates, updates,
  links); only the creates of the unloaded ids and the deletes of their
  possible parents are refused.
- **A multi-key (`rkey`) iterator** does not see keys created after it opened;
  the `key_deleted` mark reaches only the process that deleted the key. Both
  are single-node conveniences by design (philosophy.md, "the key").
- `default: {}` placeholders are dropped by save + apply, so a `required`
  column whose literal really declared `'default': {}` loses it (as in 7.25.4).
- msg2db consumers (the db_history alarms of wattyzer, yunovatios,
  estadodelaire, hidraulia) do not use `msg2db_id_incomplete()` yet: an alarm
  absent after a damaged load can be announced again as new
  (`tr_msg2db.md` has the code).
- **No red test** for: `deactivate-snap` -1 on a failed save, the fs_watcher
  root, `save_json_to_file()`'s `close()` failure, the crash window between a
  marker and its md2 row; the `kw_incref()` of C_NODE `cmd_treedbs` /
  `cmd_links` / `cmd_hooks` / `cmd_get_node` (every path to them goes
  through the command parser), of C_MQIOGATE's `view-channels`, and the
  `kw_update_missing()` of C_IEVENT_SRV's `EV_ON_CLOSE` (none of those kws
  carries a gbuffer today); the two yuno-skeleton fixes (`MSGSET_INTERNAL`,
  the timer as a pure child: checked by hand with a yuno made by
  `yuno-skeleton -p`; no ctest builds the templates); the agent's exit 0 when
  its treedb does not open (no ctest runs the agent); the entry point closing
  the log files after the leak report; `find-new-yunos create=1` skipping the
  rows already registered (the preview is tested, `c_agent_find_new_yunos`; the
  skip in `c_agent.c` runs in no ctest); the relink of a test after an installed
  archive changed (shown by hand: the build prints the link line once, then
  nothing); the test harness no longer counting *"io_uring_queue_init_params()
  pinned-memory pressure, retrying"* as an unexpected log (gobj-c
  `testing.c`: it needs the machine short of locked memory while the test
  creates its loop); `create-yuno` refusing a release name longer than
  `NAME_MAX` (`cmd_create_yuno()` lives in `c_agent.c`, which no ctest
  compiles). Not exercised live: a form Save through a real websocket drop.

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

## Inter-yuno subscriptions: a refused subscription is silent, and the cost of many

Decided 2026-09-25, no C SDK code touched yet.

- **A negative ack for a refused subscription.** `__subscribing__` is one-way:
  the client (`C_IEVENT_CLI`, C and gobj-js: `send_static_iev()`) waits for no
  answer and `C_IEVENT_SRV` sends none. When a subscription is refused
  (`max_subscriptions`, `max_subscription_size`, authz), the only trace is one
  WARNING in the server's log, on the transition (*"SUBSCRIBING refused, the
  peer holds max_subscriptions"*). The peer never knows: in a SPA the
  devices beyond the cap just stop updating, with nothing in the console, and
  after a reconnect (`resend_subscriptions()`) a different set can go silent
  if the order changes. Implement a negative ack that names the event and the
  cause, in C and gobj-js alike, and have the client log it (and publish it,
  so a view can say which data will not update). Note that an ack changes the
  wire protocol: an older client must ignore it.
- **What a large number of subscriptions costs.** hidraulia raises
  `max_subscriptions` to 20000 per channel (its SPA subscribes twice per
  device). Measure what that costs, in memory (each subscription is a json
  kept per channel, plus the refused-subscriptions count) and in speed
  (`peer_has_subscription_room()` runs `gobj_find_subscribings()` on every
  `__subscribing__`, so a SPA that subscribes N times does O(N^2) work at
  connect and at every reconnect; and a publish walks the subscription list
  of its event). Decide from the figures whether the default of 5000 stands,
  and whether the count needs to be kept instead of searched.

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

The subscription gate (`enable_subscription_authz`, YUNO_AUTH.md §4.6) is
**default-off** too. Enabling it on a yuno needs the same role model, and
every user of a treedb GUI needs `read` on the C_NODE and C_TRANGER services
it watches (C_NODE's `EV_TREEDB_NODE_*` and C_TRANGER's
`EV_TRANGER_RECORD_ADDED` are `EVF_AUTHZ_SUBSCRIBE`); a refused GUI keeps
its session and gets no live updates. `EVF_AUTHZ_INJECT` is still **declared
but not enforced** — no gate exists for `gobj_send_event`.

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

- **Periodic re-vendor from upstream** — procedure in
  `kernel/c/libjwt/README.md` (§ Re-vendor procedure).

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

- **#2 — Closing a reader's rt_disk feed races the master that feeds it.**
  Seen 2026-09-26 in yunovatios' stress test (a `db_history_ce` reading the
  `raw_tracks` of a `db_tracks_ce` that appends 3000 records/s over 30000
  keys): every orderly stop of the reader under load logs `remove_tree_walk`
  `rmdir() FAILED` *"Directory not empty"* (errno 39) on
  `<topic>/disks/<rt_id>/`. The master keeps hard-linking new md2 files into
  the per-key subdirectories of that feed while the reader removes the tree,
  so the directory is left behind with fresh links in it. Never seen without
  load (the same reader on an idle central: 0). Unknown yet whether the master
  goes on linking into a feed nobody reads after that, which would be links
  accumulating for ever. Repro: `yunovatios/yunos/sim_controllers` at 3000/s
  against the stress realm, then `kill-yuno` of its `db_history_ce`.

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

