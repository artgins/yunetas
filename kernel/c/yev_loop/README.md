# yev_loop

**Asynchronous event loop** that drives every Yuneta yuno. Built on **Linux `io_uring`** (not `epoll`) for zero-syscall-per-op submission/completion.

## What it provides

- `yev_loop_t` — the loop itself
- `yev_event_t` — one handle per async operation:
  - File descriptor I/O (TCP, UDP, serial, pipes)
  - Timers (one-shot and periodic)
  - Signal handlers
  - Filesystem (`fs_watcher` in `timeranger2`)
- Submit → callback pattern: every call returns immediately; the callback runs when the kernel reports completion
- Cross-loop messaging primitives used by multi-yuno deployments

There is **no threading** — scaling is achieved by running one yuno per CPU core and exchanging events between them.

## Key files

| File | Purpose |
|---|---|
| `src/yev_loop.h/.c` | Loop API, event types, submission/callback machinery |

## Static-build helpers

`src/static_resolv.c` exposes `yuneta_getaddrinfo()` / `yuneta_freeaddrinfo()` — a UDP DNS resolver that reads `/etc/resolv.conf` and `/etc/hosts` directly, bypassing glibc's NSS layer. When `CONFIG_FULLY_STATIC` is enabled, all `getaddrinfo`/`freeaddrinfo` call sites are redirected to these via macros. See the top-level `CLAUDE.md` for the full static-binary notes.

It keeps the file libc-only on purpose: its unit test `#include`s the `.c` to reach the static helpers and links nothing else, so the resolver must not reach into gobj-c. That is why it times its own cache with `CLOCK_MONOTONIC` instead of the `start_sectimer()` helpers, and reports through `syslog(3)` instead of `gobj_log_*` / `print_error()`.

Since 7.8.2 DNS answers are **cached** (fixed-size table, no allocation) for the answer's own TTL, clamped to 5..300 s. Only the DNS step is cached — numeric literals and `/etc/hosts` are already cheap, and caching `/etc/hosts` would break the expectation that editing it takes effect at once. Failures are not cached, so a recovered name server is picked up at once.

**Resolution is synchronous and happens inside the event loop.** A slow resolver therefore stalls every gobj in the process, not just the socket being opened, which is why `yev_loop.c` warns (`getaddrinfo() BLOCKED the event loop`, msgset `OS`) when any of its three resolution points exceeds 1 s. A single unresponsive `nameserver` in `/etc/resolv.conf` costs ~6 s per lookup (A + AAAA timeouts).

## Benchmarks & tests

- `performance/c/perf_yev_ping_pong`, `perf_yev_ping_pong2`
- `tests/c/yev_loop`
