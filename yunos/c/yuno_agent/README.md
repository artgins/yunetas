# yuno_agent

Primary Yuneta agent. One per host. Manages the lifecycle of every yuno on the
machine (create / run / pause / kill / update / delete), exposes a control
interface to `ycommand` & friends, owns the local treedb that registers
binaries, configurations, yunos and realms, and brokers inter-yuno traffic.

This is the yuno that `ycommand`, `ystats`, `ybatch` and `controlcenter` talk
to by default.

## What lives here

| File                                                | What it is                                               |
|-----------------------------------------------------|----------------------------------------------------------|
| [`src/c_agent.c`](src/c_agent.c)                    | The agent gclass (commands, FSM, child yuno spawning)    |
| [`src/c_agent.h`](src/c_agent.h)                    | Public interface (`register_c_agent`, `GOBJ_DECLARE_GCLASS`) |
| [`src/treedb_schema_yuneta_agent.c`](src/treedb_schema_yuneta_agent.c) | Schema of the persistent topics (`binaries`, `configurations`, `yunos`, …) |
| [`src/main.c`](src/main.c)                          | yuno entry point — registers gclasses, builds fixed/variable config |
| [`LIFECYCLE.md`](LIFECYCLE.md)                      | **The real lifecycle of a yuno under this agent.** Start here when onboarding. |
| [`DEBUGGING.md`](DEBUGGING.md)                      | **How to debug a running yuno.** Trace levels (global / gclass / gobj), log infrastructure (files + UDP + logcenter), end-to-end message tracing, SPA dev panel. |
| [`IPC.md`](IPC.md)                                  | **How yunos talk to each other.** Event model (states/actions, EVF_* flags, kw ownership), intra-yuno dispatch (send/publish/subscribe, CHILD vs SERVICE), inter-yuno ievents (C_IEVENT_SRV/CLI, `__md_iev__`), gates (TCP/HTTP/WS/MQTT layering, TLS), the SPA case, and the canonical recipes. |
| [`REALMS.md`](REALMS.md)                            | **Realms — the multi-tenancy unit.** Data model, on-disk layout, CRUD (create/update/delete-realm), what is and isn't realm-scoped (ports and certs aren't), the hierarchical `parent_realm_id`, sharp edges, recipes. |
| [`SCAFFOLDING.md`](SCAFFOLDING.md)                  | **`yuno-skeleton`** — which template for what, the templating engine (`{{var}}` content, `+var+` filenames, `_tmpl` suffix, derived `rootname`/`Rootname`/`ROOTNAME`/`__year__`), `yuno_citizen` vs `yuno_standalone`, the verbatim SERVICE vs CHILD `mt_create` blocks, the mandatory banner convention, post-scaffold checklist, recipes. |
| [`create-certs-self-signed/`](create-certs-self-signed/) | Helper to mint self-signed TLS certs for the agent's HTTPS endpoint |
| [`service/`](service/)                              | systemd unit + start scripts                             |
| [`certs/`](certs/)                                  | Default cert directory (populated by the helper above)   |

## Entry points

- **Local Unix socket** — used by local CLI tools (`ycommand`, `ystats`, …).
- **WebSocket** on a realm-specific URL — used for remote management and
  `controlcenter`.

## Where to go next

- **Operating a yuno (deploy / update / kill / pause)** → [`LIFECYCLE.md`](LIFECYCLE.md)
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
  ievent layer (`C_IEVENT_SRV` / `C_IEVENT_CLI`, the `__md_iev__` metadata
  block, identity card handshake, routing), commands / stats (`gobj_command`,
  `msg_iev_build_response`), the gate stack (TCP/HTTP/WebSocket/MQTT, TLS,
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
- **The companion backdoor agent** → `../yuno_agent22/` is a separate yuno
  used by `controlcenter` for PTY-based remote admin. It is **not** the
  primary lifecycle manager; enable only on hosts that should be reachable
  from a control center.

## Build

```bash
cd build && cmake .. && make install
```

Installed into `$YUNETAS_YUNOS/yuno_agent/`. See the top-level
[`CLAUDE.md`](../../../CLAUDE.md) for the global build flow (`yunetas init`,
`yunetas build`).
