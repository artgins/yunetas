# TODO

Only **open** work lives here. Anything shipped is deleted from this file the
moment it ships — its record is [`CHANGELOG.md`](CHANGELOG.md) (release notes),
the docs (`yunos/c/yuno_agent/YUNO_AUTH.md`,
`docs/doc.yuneta.io/yunos/mqtt_broker.md`,
`docs/doc.yuneta.io/guide/guide_tls.md`) and git history.

Six sections, by what the next step is:

1. **[Defects to fix](#1-defects-to-fix)** — wrong behaviour, nothing left to
   decide.
2. **[Missing tests](#2-missing-tests)** — the code is fixed, the proof is
   not there.
3. **[Decisions pending](#3-decisions-pending)** — the work cannot start
   until a choice is made; several are live defects meanwhile.
4. **[Accepted limitations](#4-accepted-limitations)** — known, documented or
   deliberate, kept as they are for now.
5. **[Future features and improvements](#5-future-features-and-improvements)**
6. **[Operations and deployment](#6-operations-and-deployment)** — per-node
   procedures, rollouts and recurring maintenance, not code.

Severity in parentheses where one was assigned.

---

## 1. Defects to fix

Nothing open in the SDK.

### Projects (code outside this repo)

Moved to the TODO of each project on 2026-10-02: `msg2db_id_incomplete()` in
the db_history alarms (wattyzer, yunovatios, estadodelaire, hidraulia), and the
url in `C_GATE_PVPC`'s logs (wattyzer).

---

## 2. Missing tests

- **`test_c_node_link_events` fails now and then** — twice on 2026-09-15,
  both inside runs of several suites, never alone; no failure recorded since
  2026-09-28. Its message was not kept. On 2026-10-03 one test of a
  serial run of the 25 c_node/tr_treedb/c_treedb tests failed and its log
  was overwritten by the rerun (three reruns green): it may have been this
  one. Next time it fails, keep
  `build/Testing/Temporary/LastTest.log` before running anything else (ctest
  overwrites it).
- **Each test binary has a fixed port and a fixed directory under
  `~/tests_yuneta`**: two whole suites run at once on one machine collide
  (`ctest -j` within one run is safe since `scripts/check_test_ports.py` and
  the `RESOURCE_LOCK`s of 7.26.1). Ports and dirs chosen at run time would
  end it. A sibling of `check_test_ports.py` that fails when two binaries
  name the same `DATABASE` without a shared lock is not written yet.
- **The module build dirs and the root `build/` ping-pong on
  `outputs/lib`**: each tree builds its own copy of the kernel archives and
  installs it over the other's, so each `yunetas test` relinks every
  executable even with nothing changed. Either the tests link the library
  targets when they exist in the same tree, or the root tree stops
  installing what the module dirs install. Touches the build of every
  consumer: needs its own review.
- **`c_gss_udp_s_self_stop` phase 11 does not prove its order** (nit): it
  relies on the subscription order for its restart to come before the start
  C_GSS_UDP_S would post. Looked at 2026-10-03: in that order the restart's
  `mt_stop` clears `start_when_stopped`, so the start is never even posted --
  the "stale start" is not observable from outside, and the guard
  `!gobj_is_running(udp_s)` of `ac_start_udp_server()` is reached only by a
  start that does not go through `mt_stop`, which no public call makes.
  What the phase proves is the outcome (the datagram after it is heard).
- **gobj-ui**: the save kw as it leaves `publish_treedb_write()`
  (`c_yui_treedb_topic_with_form.js`) is untested. Not reachable by vitest:
  the document double does not render Tabulator rows (`element.after`) nor
  `C_YUI_FORM`, and the form opens only from them (tried 2026-10-03). The way
  is a browser check on a deployed SPA (the QA drivers of gui_treedb /
  gui_agent), reading the frame the save sends.
- **No red test** for fixes made after 7.25.4: the entry point closing the log files after the leak report;
  the relink of a test after an installed archive changed; the test harness
  not counting *"io_uring_queue_init_params() pinned-memory pressure,
  retrying"* as an unexpected log (it needs the machine short of locked
  memory). And everything in `c_agent.c`, which no ctest compiles: the
  agent's exit 0 when its treedb does not open, `find-new-yunos create=1`
  skipping rows already registered (only the preview is tested,
  `c_agent_find_new_yunos`), `create-yuno` refusing a release name longer
  than `NAME_MAX`. Not exercised live: a form Save through a real websocket
  drop.
---

## 3. Decisions pending

- **C_TRANGER handles opened through the agent are not reaped** (**FUTURE**,
  user 2026-10-03: not now, no stop-gap; do not raise it as pending) when the
  operator's session ends (documented in `api/gclass/data.md`). A
  `command-yuno` reaches the yuno over its one C_IEVENT_CLI link to the agent,
  which is what is stamped as `src_gobj`; the reaping paths
  (`mt_subscription_deleted`, `ac_on_close` of a C_IEVENT_SRV, `mt_stop`)
  never fire for it, and the operator's session lives in the agent (behind a
  control center, not even there). **Decide**: an agent-to-yuno "session
  ended" notification, and a rule for when several sessions of one user are
  one owner.
- **MQTT broker ACL: model and default-deny** (Rosa). **FUTURE** (user,
  2026-10-03: not now; do not raise it as pending until asked). Model A (per-group
  `publish_acl` / `subscribe_acl` in the broker treedb, `enable_acl` default
  off) ships; see `mqtt_broker.md`. Open: A vs B (reuse `C_AUTHZ` via
  `gobj_user_has_authz` — one authz system, but its checker is per authz name,
  not per topic pattern, so it needs extending) vs C (a broker config attr
  holding the pattern map: no schema migration, off the treedb/UI); and
  whether to flip enforcement to default-deny (validate on staging first).
  The ACL is keyed by the `client_id` the client chooses, and no client is
  bound to the user that authenticated it (checked 2026-10-03: the `clients`
  topic has no user link, CONNECT compares nothing): any authenticated user
  can take another client's `client_id` and its groups. So the ACL is not a
  boundary between users until the model binds them. (The will message
  obeys the ACL since 2026-10-03.)
- **Packaging: the sparse SDK in the `.deb` serves one glibc at a time.**
  **DECIDED** (user, 2026-10-03): the protection IS the chosen solution --
  `libc_guard.cmake` refuses a build under another glibc, and what to do
  then (build elsewhere and ship binaries, build the SDK from source on that
  node) is the choice of whoever uses Yuneta for their projects, not ours.
  A package per distro (the matrix below) is **FUTURE**, and only once
  Yuneta is very stable. Not a pending decision; the analysis stays for
  that day. The
  `.deb` installs a sparse SDK (`outputs/`, `outputs_ext/`, `tools/`,
  `.config`) so a node can compile a project against the published runtime.
  The `outputs/lib/*.a` are static archives tied to the glibc that built them
  (`_dl_x86_cpu_features`); linking under another glibc succeeds and corrupts
  the heap at run time, which `tools/cmake/libc_guard.cmake` stops at
  configure time. Since 7.8.6-3 the `.deb` is built in `debian:13` (glibc
  2.41) and the `.rpm` in `rockylinux:9` (2.34), so it works on exactly one
  distro per package: Ubuntu and Debian 12 nodes cannot compile against it.
  Options, cheapest first:
  - **Drop the build half of the `.deb`** (keep the runtime binaries). Honest
    about what the package is, and how deploys already work (build on a dev
    machine, `yunetas sync-binaries`). Not a size argument (~77 MB of a 1.1 GB
    tree). Was the preference — but its premise "no one compiles on a node" is
    no longer true: both yunovatios nodes build their project yunos against
    the sparse SDK (7.9.3). Weigh that before choosing.
  - **Build the `.deb` on a matrix** (one per distro base). The external
    archives must be rebuilt per base too (19 of 31, 61 MB of 71; runner time,
    not new machinery); `install.sh` must detect the distro version and name
    the asset; unknown: OpenSSL / ncurses / pcre2 under much newer gcc.
  - **Shared libraries instead of static archives**: built against the oldest
    supported glibc, they run on every newer one; gives up
    `CONFIG_FULLY_STATIC` for the SDK libs.
  - **Distribution packaging**: correct by construction, highest cost.

  Not a route: snap / flatpak (confinement forbids `/yuneta`, io_uring,
  spawning yunos, `/var/crash`). An OCI image as the build environment is the
  portable form of the matrix option. Not urgent while every node is ours.

---

## 4. Accepted limitations

### timeranger2 and treedb

- **A t marker that cannot be written** (`<file>.unordered`, a late
  `__t__`) is retried only by the next append to the same FILE; a file
  never appended again keeps it missing (logged), and a reload reads that
  file's t range from its first and last rows. (`mark-tm-order`, which
  could write it again, went with the tm markers, 2026-10-03.)
- **A demoted master never takes its lock back** while the process lives; it
  needs a restart (documented).
- **In a partial topic, operations on other ids are allowed** (creates,
  updates, links); only the creates of the unloaded ids and the deletes of
  their possible parents are refused.
- **A multi-key (`rkey`) iterator** does not see keys created after it
  opened; the `key_deleted` mark reaches only the process that deleted the
  key. Single-node conveniences by design (philosophy.md, "the key").
- **`default: {}` placeholders are dropped by save + apply**
  (`prune_schema_node()`), so a `required` column whose literal really
  declared `'default': {}` loses it. The lesser of two evils, commented in
  `c_treedb.c`.
- **Apply is off on every in-tree yuno**: the agent, the control center and
  `C_AUTHZ` force `impose_c_schema`, so gui_agent's Apply cannot act on any of
  them until one stops forcing it.

### Traces and secrets

- **The masking rule's limits** (C and JS alike). It over-masks ordinary trace
  text (`token=x is fine now` → `token=********`; a clause up to the next
  `word=` disappears; `command_mask_secret_line()` hides its own ` <...>`
  marker after a secret), and under-masks across a newline (`password=a
  b\nuser=bob` shows `b`) and a later word containing `=` (`token=abc def==`
  shows `def==`, base64 padding). The price of passwords with blanks.
- **Secrets a trace cannot tell**: a credential in RECEIVED bytes that is
  neither an HTTP credential header, nor a `name=value`, nor a json `"name":
  value` with a secret's name; a credential cut between two reads (the masking
  of received bytes is per buffer); and a parameter of a command for a REMOTE
  service whose name is not a secret's name (no command table at hand says it
  is `SDF_SECRET`).

### Resolver

- **`getaddrinfo()` runs synchronously inside the loop** (`yev_loop.c`:
  connect, source bind, listen). Every gobj, timer and completion of the
  process stops while it resolves. 7.8.2 cached the answers (one lookup per
  host per TTL) and made the cost visible (*"getaddrinfo() BLOCKED the event
  loop"*). A resolution that cannot block would be an async step in the FSM.
  Not urgent while nodes have a working `resolv.conf`.

### Auth

- **OIDC discovery requires `end_session_endpoint`.** `save_oidc_discovery`
  (`c_auth_bff.c`) and `c_task_authenticate` stop when it is absent. Auth0
  does not publish it (proprietary `/v2/logout`) and some Cognito setups omit
  it, so discovery fails there; the workaround is to set explicit
  `token_endpoint` + `end_session_endpoint`, which skips discovery. Kept as
  is; Authentik and Keycloak publish it. (Not live-tested beyond Keycloak: no
  tenants.)

### emailsender

- **A 4xx at MAIL FROM ("451 rate exceeded") costs a retry**, so a rate limit
  dead-letters the head in ~30 s, and the next ones while it lasts. By design.

### Queues

- **A queue link delivers at least once**: when an ack is lost the sender
  resends and the receiver stores the message twice (2026-09-28, `tr2check` on
  a stress store of 36.3 M records: 6.1 M duplicates, same key, `seq` and
  `tm`, stored 3.5 min apart). Repeats are filtered ABOVE the queue, by each
  application, where the data is stored. `C_QIOGATE` carries raw data and
  cannot discriminate by a payload field (a fleet of GPS units may all send
  with the same `id`); if it ever filters anything, it is by **metadata**
  (`__tm__`), never by data. (A filter by key and payload field was written
  and reverted, `02bf70ea5` / `cac246730`.) The application work is in
  yunovatios' TODO.

### Protocols

- **`C_PROT_MQTT` is not migrated to `peername` attribution**: deprecated,
  still in hidraulia production, goes away with it. `C_PROT_MQTT2` is the
  target for protocol work.

### Events

- **A published kw's gbuffer is shared by every subscriber, read cursor
  included** (the design, unchanged): every action the kw passes through sees
  and can process it; one that consumes moves the cursor for the next, one
  that only looks does not. Written in `GOBJ.md` §8.13. C_IOGATE's "send to
  all" copies per channel because each channel's transport consumes.

---

## 5. Future features and improvements

### timeranger2 and C_TRANGER

- **A follower re-reads the first row of the md2 on every notification.**
  Profiled 2026-09-27 on yunovatios' `db_history_ce`: 10.9 % of its work was
  `load_cache_cell_from_disk()` → `load_first_and_last_record_md()`.
  `b2f972382` removed the per-append open/close in the common case (a
  kept-open md2 descriptor, `fstat` instead of `open`+`lseek`), but
  `update_new_records_from_disk()` still reloads a KNOWN cell: the first row
  and the last row on every append, plus a `file_exists()` for the
  `.unordered` marker. For a known cell the first row
  cannot change (md2 files are append-only, a torn tail is cut from the END):
  take the row count from the size, skip the first row, take the last row
  from what `publish_new_rt_disk_records()` reads anyway, keep `fr_t` /
  `fr_tm`. Keep the whole path for a new cell and for a marked file, and keep
  the torn-tail check. Needs a `test_rt_disk*` test and an A/B against the
  last tag; nobody has measured the descriptor path against the 10.9 % yet.
- **Share one rt_disk feed per topic across Live cards.** On a reader
  (`master:false`) C_TRANGER each `open-rt` opens a `tranger2_open_rt_disk`
  feed = one inotify instance, so N Live cards = N instances (a node sat at
  128/128 `max_user_instances` on 2026-07-12; the packages now raise it to
  4096 in `99-yuneta-core.conf`, `info-inotify` shows the usage — that only
  raises the ceiling). One refcounted feed per topic caps it at one instance
  per followed topic. It is a design change across C and the SPA, not a small
  one (read 2026-09-16): the `rt_id` a client gets back stops naming a feed
  and names a SUBSCRIPTION to a shared one, so `cmd_close_rt` and
  `reap_handles_of()` must decrement a refcount instead of closing;
  `publish_rt_callback()` stamps the shared feed's `rt_id` into every publish,
  so gui_treedb's `live_filter()` (`c_tranger_view.js`) must go back to
  filtering by key in the same change, or the cards go silent; and the master
  path (`rt_mem`) has no inotify, so decide whether the shared feed is
  unconditional (two publish contracts otherwise). Same release, coordinated
  deploy, plus a test that opens two cards on one topic and counts inotify
  instances.
- **A follower hears its OWN consumption: half its inotify queue is echo.**
  Each hard link the follower consumes is an `IN_DELETE` in a watched key
  directory, which it then ignores (*"it's me"*). Half of its queue under load
  is that echo, which is why a follower overflowed at ~3000 records/s and why
  one burst overflows it twice. An overflow no longer aborts (the feed is
  rescanned) but costs a rescan of every key. Watch the key directories
  without `IN_DELETE` (the root keeps it: the key-delete signal is a
  `.d<seq>.<key>` directory made and removed there); needs a per-level mask in `fs_watcher`, and `C_FS` still
  wants file deletes.

### Schema editing

- **Applying an edit restarts the whole yuno** (`kill-yuno` → `run-yuno
  play=0` → `play-yuno`), not just the treedb: its gate goes down and every
  client — the editor included — reconnects. Reopening only the treedb is not
  available to a third party and should not be (`close-treedb` destroys
  services whose handles the owner has cached). A true in-place reload lives
  in the owner, as a local method — "reload your schema" — using
  `tranger2_write_topic_cols()` (today only called by
  `tranger2_create_topic()`) plus `parse_schema_cols()` + `parse_hooks()` and
  a link reload when hooks or fkeys change.
- **Not every treedb is projected into `__system__`.** `C_AUTHZ` creates its
  `C_NODE` directly (`c_authz.c` ~557) instead of going through `C_TREEDB`'s
  `open-treedb`, so `treedb_authzs` never reaches `__system__` and cannot be
  edited; any other direct `C_NODE` consumer is in the same position.
- **The Schemas console's authz is the target yuno's, and nobody provisions
  it.** A console user needs `read` / `create` / `update` / `delete` of that
  `C_NODE` in **that yuno's** `C_AUTHZ`; an operator who can log into the
  control center still gets `-403` per topic on a yuno where they have no
  role. Part of the authz-roles work. Before diagnosing one: `descs` and
  `nodes` take DIFFERENT authz (`a_schemas` and `a_nodes` in `c_node.c`), so
  an identity with one and not the other loads the schema and is then refused
  per topic — which reads as a broken build and is a missing role.

### Stats

- **The yuno sends its own stats** (option (b) of the stats push). Today the
  agent's `watch-yuno-stats` SAMPLES its yunos (option (a)) and the control
  center relays `EV_YUNO_STATS`. (b): `C_YUNO` gets a `publish-stats` command
  and, while on, sends `EV_YUNO_STATS` to its agent from the timer that
  already runs `load_stats()` — the producer publishes, but only once every
  project yuno of every node is rebuilt, so (a) stays as the fallback. Note
  the transport: a subscription travels only from a CLIENT to a SERVER (the
  agent is the server of its yunos' channels, the control center of the
  agents'), so neither can subscribe downstream; what goes upstream is an
  inter-event SENT along a route kept from a request, like the PTY mirror.

### Inter-yuno subscriptions (decided 2026-09-25)

- **A negative ack for a refused subscription.** `__subscribing__` is one-way
  (C and gobj-js `send_static_iev()`); when `C_IEVENT_SRV` refuses one
  (`max_subscriptions`, `max_subscription_size`, authz) the only trace is a
  WARNING in the server's log, on the transition. The peer never knows: in a
  SPA the devices beyond the cap just stop updating, with nothing in the
  console, and after a reconnect a different set can go silent. Send a
  negative ack that names the event and the cause, in C and gobj-js alike;
  the client logs it and publishes it, so a view can say which data will not
  update. It changes the wire protocol: an older client must ignore it.
- **Measure what many subscriptions cost.** hidraulia raises
  `max_subscriptions` to 20000 per channel. Memory (a json per subscription
  per channel) and speed (`peer_has_subscription_room()` runs
  `gobj_find_subscribings()` on every `__subscribing__`: O(N²) at connect and
  reconnect; a publish walks the event's subscription list). Decide from the
  figures whether the default of 5000 stands and whether the count should be
  kept instead of searched.

### Resolver

- **Nameservers are tried strictly in order, 3 s × 2 each**
  (`YUNETA_DNS_QUERY_TIMEOUT`, A then AAAA): a dead first entry always costs
  6 s. Cheapest first: drop the per-query timeout, remember which nameserver
  last answered and start there, or query them concurrently and take the
  first reply.

### Auth

- **ROPC → device / client-credentials** — deferred until a non-Keycloak IdP
  is adopted. `c_task_authenticate`'s `action_get_token` uses
  `grant_type=password`, which works because every deployed IdP is Keycloak.
  All 6 callers (`ycli`, `ycommand`, `ystats`, `ytests`, `ybatch`,
  `mqtt_tui`) are headless CLI/server tools with no browser and no local
  listener, so PKCE does not fit. Interactive use → **Device Authorization
  Grant** (RFC 8628: print URL + user code, poll with
  `urn:ietf:params:oauth:grant-type:device_code`, handle
  `authorization_pending` / `slow_down`, discover
  `device_authorization_endpoint`). Headless CI (`ybatch`, `ytests`) →
  **Client Credentials** (a service-account client + secret). Scope:
  `c_task_authenticate` (new FSMs, discovery fields, config attrs) + the 6
  callers + tests + docs; keep ROPC as a Keycloak fallback. **Do not point any
  CLI at a ROPC-disabled IdP before this lands.**
- **`EVF_AUTHZ_INJECT` is declared but not enforced**: no gate exists for
  `gobj_send_event` (`gobj.h` ~333).

### Tools

- **The `yunetas` CLI does not refresh the agents after a source build.** On
  package nodes `install.sh` restarts `yuneta_agent22` once the main agent is
  confirmed healthy on the new binary. On nodes that build from source both
  agents are restarted by hand, and nothing says the spare is stale — which is
  how it ran a fixed bug for five days on four nodes. The report half exists
  (`tools/agent/audit-agents.sh`); left: after a build that replaced
  `/yuneta/agent/*`, the CLI restarts the main agent, verifies it, then the
  spare (never the spare first: it is the only way into a node whose main
  agent is broken).

### gui_treedb

- **Backend features with no UI** (the SPA uses 15 of ~45 C_NODE / C_TRANGER
  commands). Highest operator value, in order: **snapshots** (`snaps`,
  `snap-content`, `shoot-snap`, `activate-snap`, `deactivate-snap`: tag a
  version, browse it read-only, roll back); **backup / restore** (`export-db`
  / `import-db`, a base64 payload maps to a browser download / file input);
  and the relationship inspector (`parents` / `children` / `links`, plus
  `jtree`, whose ready-made tree with `__path__` is ignored in favour of a
  flat `nodes` list).

---

## 6. Operations and deployment

- **The agents' systemd units, still to verify through the packages.** The
  units and `--pid-file` were checked by hand on wattyzer (Debian 13,
  2026-10-02: crash relaunch, restart keeping the yunos, an agent outside its
  unit moved in, a hand-run second agent leaving alone), not through a built
  package. Before the release that ships them: build the `.deb` and upgrade a
  node from the 7.25.21 package (the SysV generated unit "active (exited)",
  agent22 outside); and the `.rpm` on yunovatios central (Rocky 9, the node
  whose acceptance test asked for it, package only). wattyzer runs both
  units now, with hand-installed unit files and agents built locally.
- **xscripts start the agents with `systemctl`** once the units ship: the
  `create-*.sh` of the operation repos (estadodelaire, yunovatios, artgins)
  stop and start the agent by hand (`--stop` / `--start`), which puts it
  outside its unit -- the case the units exist to end.
- **a.com: deploy controlcenter config 7** (both planes, 1996 and 1997;
  the artgins operation repo, `ce894df`): `__input_side__` not autostarted
  nor autoplayed. With the controlcenter binary that carries the C_TIMER0
  rate tick (next release); `update-config` of the row in use, or
  `create-config` + `find-new-yunos create=1` + a restart of just those two.

- **Per-command authz gate — production enablement.** **FUTURE** (user,
  2026-10-03: not now; do not raise it as pending until asked). NOT by
  flipping the default: a yuno with no C_AUTHZ (gates, db_history,
  logcenter...) would refuse every external command, the agent's forwarded
  ones included (`authz_checker()` answers FALSE with no C_AUTHZ). Only in
  the five yunos that hold one, by config. `enable_command_authz`
  is default-off (YUNO_AUTH.md §4.5). Per node:
  - provision **every principal that sends commands TO each C_AUTHZ yuno**
    with `__execute_command__` / root — not only `yuneta` / admins but the
    **control center** user(s) that reach the agent's `:1993` port (the agent
    store has `yuneta` + `yuneta_admin@…` + `yunetas_admin@…`, NOT
    `yuneta_agent@…`);
  - confirm each of the 5 C_AUTHZ stores (agent, agent22 [shared],
    controlcenter, mqtt_broker, emailsender) holds the `root` / `yuneta`
    model at runtime (a store that missed `Authz.initial_load` re-seeds on its
    next master start, so this is a check, not a repair);
  - run a real **low-privilege deny test** on staging (needs a non-root
    external principal);
  - set `enable_command_authz: true` per yuno, the agent first, staging →
    production. (User management no longer waits for it: C_AUTHZ asks the
    permissions of `treedb_authzs` on its own since the cloud review of
    2026-10-02; the gate is still what guards every OTHER command, the
    agent's `install-binary` / `run-yuno` among them.)
- **Subscription gate.** `enable_subscription_authz` (YUNO_AUTH.md §4.6) is
  default-off too. Enabling it needs the same role model, and every user of a
  treedb GUI needs `read` on the C_NODE and C_TRANGER services it watches
  (`EV_TREEDB_NODE_*` and `EV_TRANGER_RECORD_ADDED` are
  `EVF_AUTHZ_SUBSCRIBE`); a refused GUI keeps its session and gets no live
  updates.
- **ytls TLS posture — per-gate rollout** (validate on staging). The support
  is in ytls and the IdP-client crypto blocks of the project batches are
  hardened (`ssl_use_system_ca` + `ssl_verify_mode=required`). Left: check
  that every client crypto block in the realm configs sets the CA (or the
  explicit `ssl_allow_insecure_client` opt-out for IoT gates); raise the
  server-side gates explicitly where wanted; set the IoT-compat profile
  (`ssl_min_version` + `ssl_ciphers "@SECLEVEL=0"`, OpenSSL backend) on legacy
  gates — no batch carries it yet.
- **Vendored libjwt: periodic re-vendor** from upstream (procedure in
  `kernel/c/libjwt/README.md`, § Re-vendor procedure). Vendored base
  `375e539`, analysed up to upstream v3.6.1, plus local NULL-safety
  backports (2026-09).
- **Memory-safety review not done yet**: `modules/postgres` (libpq wrapper;
  delegated to libpq, lower priority).
