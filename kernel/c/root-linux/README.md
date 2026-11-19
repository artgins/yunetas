# root-linux

root-linux is the library of runtime GClasses for **Linux**. It sits on top of `gobj-c` and `yev_loop`. It has the GClasses that every yuno uses: networking, protocols, timers, filesystem watch, databases, authentication, and more.

## What is inside

| Area | GClasses |
|---|---|
| Networking | `C_TCP`, `C_TCP_S` (TLS), `C_UDP`, `C_UDP_S`, `C_TCP0`, `C_TCP_S0` |
| Timers | `C_TIMER`, `C_TIMER0` |
| Protocols | `C_PROT_HTTP_SRV`, `C_PROT_HTTPS_CLI`, `C_PROT_TCP4H`, `C_IEVENT_CLI/SRV`, `C_AUTH_*`, `C_AUTHZ`, `C_AUTHS`, `C_CHANNEL`, … |
| Filesystem | `C_FS` (inotify via `fs_watcher`) |
| Databases | `C_TREEDB`, `C_NODE`, `C_ASSETS`, `C_RESOURCE`, `C_COUNTER`, `C_QIOGATE`, `C_QCONNEX`, … |
| Yuno internals | `C_YUNO`, `C_CLI`, `C_TASK`, `C_TASK_AUTHENTICATE`, entry point |

## Headers to look at

- `src/c_tcp.h`, `src/c_tcp_s.h`: TCP/TLS client GClasses
- `src/c_timer.h`: timers
- `src/c_fs.h`: filesystem watcher
- `src/c_treedb.h`, `src/c_node.h`: TreeDB graph database
- `src/c_yuno.h`, `src/entry_point.h`: yuno lifecycle

## Build

`yunetas build` builds this library after `kernel/c/gobj-c`, `kernel/c/yev_loop`, `kernel/c/ytls` and `kernel/c/timeranger2`. Every yuno in `yunos/c/` links against this library.

The top-level `CLAUDE.md` gives the full dependency order.
