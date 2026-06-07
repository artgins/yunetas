# yuno_agent

Primary Yuneta agent. One per host. Manages the lifecycle of every yuno on the
machine (create / run / pause / kill / update / delete), exposes a control
interface to [`ycommand`](#util-ycommand) & friends, owns the local treedb that registers
binaries, configurations, yunos and realms, and brokers inter-yuno traffic.

This is the yuno that `ycommand`, [`ystats`](#util-ystats), [`ybatch`](#util-ybatch) and `controlcenter` talk
to by default.

## What lives here

| File                                                | What it is                                               |
|-----------------------------------------------------|----------------------------------------------------------|
| [`src/c_agent.c`](src/c_agent.c)                    | The agent gclass (commands, FSM, child yuno spawning)    |
| [`src/c_agent.h`](src/c_agent.h)                    | Public interface ([`register_c_agent`](https://github.com/artgins/yunetas/blob/7.5.1/yunos/c/yuno_agent/src/c_agent.c#L11593), `GOBJ_DECLARE_GCLASS`) |
| [`src/treedb_schema_yuneta_agent.c`](src/treedb_schema_yuneta_agent.c) | Schema of the persistent topics (`binaries`, `configurations`, `yunos`, …) |
| [`src/main.c`](src/main.c)                          | yuno entry point — registers gclasses, builds fixed/variable config |
| [`ENTRY_POINT.md`](ENTRY_POINT.md)                  | **Minute 0: what every yuno's `main()` actually does.** [`yuneta_entry_point()`](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/root-linux/src/entry_point.c#L286) step-by-step (argp, gbmem-setup + json allocator switch, config merge, log handlers, gclass registration), [`ydaemon.c`](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/root-linux/src/ydaemon.c) double-fork supervisor (the watcher that makes a yuno survive without an agent), signals inside the child, how `kill-yuno` interacts with the watcher, `/var/crash/core.%e` forensics wired by the `.deb`. |
| [`YUNO_LIFECYCLE.md`](YUNO_LIFECYCLE.md)                      | **The real lifecycle of a yuno under this agent.** Start here when onboarding. |
| [`DEBUGGING.md`](DEBUGGING.md)                      | **How to debug a running yuno.** Trace levels (global / gclass / gobj), log infrastructure (files + UDP + logcenter), end-to-end message tracing, SPA dev panel. |
| [`IPC.md`](IPC.md)                                  | **How yunos talk to each other.** Event model (states/actions, EVF_* flags, kw ownership), intra-yuno dispatch (send/publish/subscribe, CHILD vs SERVICE), inter-yuno ievents (C_IEVENT_SRV/CLI, `__md_iev__`), gates (TCP/HTTP/WS/MQTT layering, TLS), the SPA case, and the canonical recipes. |
| [`REALMS.md`](REALMS.md)                            | **Realms — the multi-tenancy unit.** Data model, on-disk layout, CRUD (create/update/delete-realm), what is and isn't realm-scoped (ports and certs aren't), the hierarchical `parent_realm_id`, sharp edges, recipes. |
| [`SCAFFOLDING.md`](SCAFFOLDING.md)                  | **`yuno-skeleton`** — which template for what, the templating engine (`{{var}}` content, `+var+` filenames, `_tmpl` suffix, derived `rootname`/`Rootname`/`ROOTNAME`/`__year__`), `yuno_citizen` vs `yuno_standalone`, the verbatim SERVICE vs CHILD `mt_create` blocks, the mandatory banner convention, post-scaffold checklist, recipes. |
| [`YUNO_AUTH.md`](YUNO_AUTH.md)                                | **Auth + TLS.** `auth_bff` OIDC flow (PKCE, HttpOnly cookies, `issuer` vs deprecated `idp_url`+`realm`), JWT validation via `libjwt`, the [`C_AUTHZ`](#gclass-c-authz) service + `authzs` treedb (users/roles), the `pm_*` schemas — **⚠ command authz check is commented out in [`command_parser.c`](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/command_parser.c) today**, cert auto-sync (`cert_sync_*` attrs, `reload-certs` broadcast), per-project Keycloak realms, secrets-in-cleartext risk. |
| [`GOBJ.md`](GOBJ.md)                                | **The gobj framework in 30 minutes.** gclass vs gobj, banner layout, the `GMETHODS` table (`mt_create`/`mt_start`/`mt_stop`/`mt_destroy`/`mt_writing`/`mt_reading`/etc.), full lifecycle (create→start→play↔pause→stop→destroy), every `gobj_create*` flavour, SData (`DTP_*` types + `SDF_*` flags + persistence), the runtime tree + service registry, a worked walkthrough of [`c_timer.c`](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/root-linux/src/c_timer.c) (the canonical minimal gclass), 12 sharp edges, 5 recipes. |
| [`YUNO_TREEDB.md`](YUNO_TREEDB.md)                            | **timeranger2 + treedb in 30 minutes.** The append-only log layer (per-key dirs, `.json`+`.md2` partitioning, `g_rowid`/`i_rowid`, `__t__`/`__tm__`, master/non-master lock, no `fsync`, no per-record delete since v7), the graph layer on top (topic schemas, `cols`/`hook`/`fkey`, `__md_treedb__` metadata, CRUD APIs), **the link-saves-the-child-only rule** and the **`topic_version` versioning trap**, snapshots, cross-yuno `rt_by_disk` pattern, 12 sharp edges, 6 recipes. |
| [`create-certs-self-signed/`](create-certs-self-signed/) | Helper to mint self-signed TLS certs for the agent's HTTPS endpoint |
| [`service/`](service/)                              | systemd unit + start scripts                             |
| [`certs/`](certs/)                                  | Default cert directory (populated by the helper above)   |

## Entry points

- **Local Unix socket** — used by local CLI tools (`ycommand`, `ystats`, …).
- **WebSocket** on a realm-specific URL — used for remote management and
  `controlcenter`.

## Where to go next

- **What every yuno's `main()` actually does** → [`ENTRY_POINT.md`](ENTRY_POINT.md)
  is the "minute 0" read. It explains `yuneta_entry_point()` step by step
  (argp, the [`gbmem_setup`](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/gbmem.c#L80) + `json_set_alloc_funcs` switch that load-bears
  every test allocator rule, the `fixed + variable + --config-file +
  positional` config merge that `view-config` surfaces, the log handlers,
  the gclass-registration callback). Then [`ydaemon.c`](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/root-linux/src/ydaemon.c): the double-fork
  pattern, the **per-yuno watcher** that auto-relaunches the child on any
  abnormal exit (this is what makes a yuno survive `kill -9 yuneta_agent`),
  the `waitpid` decision matrix, and [`daemon_shutdown()`](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/root-linux/src/ydaemon.c#L358)'s SIGQUIT-then-
  SIGKILL pair. Then signals inside the yuno child (signalfd, SIGQUIT
  semantics, SIGUSR1/2 as trace toggles). Then how the agent's
  `kill-yuno` interacts with the watcher and why the default doesn't
  touch it. Then `/var/crash/core.%e` post-mortem: the sysctl + PAM
  limits + `/var/crash` group ownership wired by the `.deb`, the no-PID
  pattern, the `Daemon relaunched` log line that's your only silent-crash
  alarm.
- **Operating a yuno (deploy / update / kill / pause)** → [`YUNO_LIFECYCLE.md`](YUNO_LIFECYCLE.md)
  has the full command inventory, the on-disk + treedb layout, the
  `EV_ON_OPEN` / `EV_ON_CLOSE` handshake, the sharp edges (`update-binary`
  over a live mmap, stale pids, no SIGKILL escalation, pause ≠ SIGSTOP), and
  the canonical `ycommand` recipes.
- **Debugging a running yuno** → [`DEBUGGING.md`](DEBUGGING.md) covers severity
  levels (`gobj_log_*`), trace categories (global / gclass / gobj), how to read
  the JSON log lines, the FSM `machine` trace, end-to-end message tracing
  across yunos via `ievent_gate_stack`, the `logcenter` UDP aggregator, and
  the SPA-side dev panel.
- **How yunos talk to each other** → [`IPC.md`](IPC.md) covers the event model
  (states/actions tables, `EVF_*` flags, `kw` ownership, the "IMPORTANT HACK"
  of state-before-action), intra-yuno dispatch (`gobj_send_event`,
  `gobj_publish_event`, subscriptions, CHILD vs SERVICE), the inter-yuno
  ievent layer ([`C_IEVENT_SRV`](#gclass-c-ievent-srv) / [`C_IEVENT_CLI`](#gclass-c-ievent-cli), the `__md_iev__` metadata
  block, identity card handshake, routing), commands / stats (`gobj_command`,
  [`msg_iev_build_response`](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/root-linux/src/msg_ievent.c#L541)), the gate stack (TCP/HTTP/WebSocket/MQTT, TLS,
  `public_services`), and the browser SPA case.
- **Realms (the multi-tenancy unit)** → [`REALMS.md`](REALMS.md) explains the
  `realms` topic, the composed `<name>.<role>.<env>` URL identity, the
  `/yuneta/realms/<owner>/<url>/<yuno>/{bin,logs}` on-disk layout, `create-
  realm` / `update-realm` / `delete-realm`, and the things that **look**
  per-realm but aren't (port pool, cert sync, the binary repo).
- **Scaffolding a new yuno or gclass** → [`SCAFFOLDING.md`](SCAFFOLDING.md)
  explains `yuno-skeleton`, the seven shipped templates, the templating
  engine (`{{var}}` for content, `+var+` for filenames, the `_tmpl`
  opt-in, derived `rootname`/`Rootname`/`ROOTNAME`), the structural diff
  between `yuno_citizen` and `yuno_standalone`, the verbatim SERVICE vs
  CHILD subscription block in `mt_create`, and the mandatory banner
  convention from CLAUDE.md.
- **Auth + TLS** → [`YUNO_AUTH.md`](YUNO_AUTH.md) covers the `auth_bff` OIDC flow
  (PKCE, HttpOnly cookies, the modern `issuer` config vs deprecated
  `idp_url`+`realm`), JWT validation via `libjwt`, the `C_AUTHZ` service
  with its `authzs` treedb (users / roles / users_accesses,
  `parent_role_id` inheritance, the `yuneta` super-user bypass), the
  `pm_*` schemas and the **critical** finding that the per-command
  [`gobj_user_has_authz`](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/gobj.c#L9400) check at [`command_parser.c`](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/gobj-c/src/command_parser.c) is currently
  commented out, the `cert_sync_*` machinery on the agent and the
  `reload-certs` broadcast, the per-project Keycloak realm convention,
  and the secrets-in-cleartext risk (`client_secret`, SMTP password).
- **The gobj framework in 30 minutes** → [`GOBJ.md`](GOBJ.md) explains the
  gclass / gobj distinction, the mandatory banner layout, the `GMETHODS`
  table, the full lifecycle (`gobj_create*` / `gobj_start*` / `gobj_stop*`
  / `gobj_destroy`), every `mt_*` method, SData attributes (types, flags,
  persistence), the runtime tree, the service registry, and walks through
  [`c_timer.c`](https://github.com/artgins/yunetas/blob/7.5.1/kernel/c/root-linux/src/c_timer.c) as the minimal canonical example.
- **timeranger2 + treedb in 30 minutes** → [`YUNO_TREEDB.md`](YUNO_TREEDB.md) covers
  the append-only log layer (per-key directories, `.json`+`.md2`
  partitioning, the `g_rowid`/`i_rowid` rule, master/non-master locking,
  the post-v7 absence of per-record delete), the graph layer on top
  (topic schemas, `cols`/`hook`/`fkey` flags, the `__md_treedb__`
  metadata block, node CRUD), the **link-saves-the-child-only** invariant,
  the **`topic_version` schema-versioning trap**, snapshots, and the
  cross-yuno `rt_by_disk` pattern.
- **The companion backdoor agent** → `../yuno_agent22/` is a separate yuno
  used by `controlcenter` for PTY-based remote admin. It is **not** the
  primary lifecycle manager; enable only on hosts that should be reachable
  from a control center. Two practical uses:
  1. *PTY backdoor for ops*: the controlcenter operator can drop into a
     shell on the host without touching the primary agent.
  2. *Self-update channel for the primary agent*: because `yuneta_agent22`
     is an independent process (its own `--config-file`, its own watcher),
     you can `--stop` / replace the binary / `--start` of `yuneta_agent`
     without losing the inbound channel to the host. Same `authz.master`
     wiring as the primary, but typically with `authz.master=false`
     so it follows the primary's authz treedb instead of forking one.

## Build

```bash
cd build && cmake .. && make install
```

Installed into `$YUNETAS_YUNOS/yuno_agent/`. See the top-level
[`CLAUDE.md`](../../../CLAUDE.md) for the global build flow (`yunetas init`,
`yunetas build`).
