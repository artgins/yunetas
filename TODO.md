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

**Low, worth keeping:**
the warning *"Parent ref already in child fkey"* still fires in the legitimate case of
4e4dcdc00, once per `create-yuno`.

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

