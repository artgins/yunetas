# root-linux

Runtime GClasses for **Linux**. This layer sits on top of `gobj-c` + `yev_loop` and implements the building blocks that every yuno uses: networking, protocols, timers, filesystem watching, databases, auth, etc.

## What's inside

| Area | GClasses |
|---|---|
| Networking | `C_TCP`, `C_TCP_S` (TLS), `C_UDP`, `C_UDP_S`, `C_TCP0`, `C_TCP_S0` |
| Timers | `C_TIMER`, `C_TIMER0` |
| Protocols | `C_PROT_HTTP_SRV`, `C_PROT_HTTPS_CLI`, `C_PROT_TCP4H`, `C_IEVENT_CLI/SRV`, `C_AUTH_*`, `C_AUTHZ`, `C_AUTHS`, `C_CHANNEL`, … |
| Filesystem | `C_FS` (inotify via `fs_watcher`) |
| Databases | `C_TREEDB`, `C_NODE`, `C_RESOURCE`, `C_COUNTER`, `C_QIOGATE`, `C_QCONNEX`, … |
| Yuno internals | `C_YUNO`, `C_CLI`, `C_TASK`, `C_TASK_AUTHENTICATE`, entry point |

## Headers to look at

- `src/c_tcp.h`, `src/c_tcp_s.h` — TCP/TLS client GClasses
- `src/c_timer.h` — timers
- `src/c_fs.h` — filesystem watcher
- `src/c_treedb.h`, `src/c_node.h` — TreeDB graph db
- `src/c_yuno.h`, `src/entry_point.h` — yuno lifecycle

## Build

Built automatically by `yunetas build` once `kernel/c/gobj-c`, `kernel/c/yev_loop`, `kernel/c/ytls` and `kernel/c/timeranger2` are up. Every yuno in `yunos/c/` links against this library.

See the top-level `CLAUDE.md` for the full dependency order.
