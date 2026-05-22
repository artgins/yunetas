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
