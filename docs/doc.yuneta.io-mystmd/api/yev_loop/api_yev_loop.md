# Event Loop API

`yev_loop` is the **asynchronous event loop** that drives every Yuneta
yuno. It is built on **Linux `io_uring`** (not `epoll`) for
zero-syscall-per-op submission/completion.

## What it provides

- **`yev_loop_t`** — the loop itself.
- **`yev_event_t`** — one handle per async operation:
  - File descriptor I/O (TCP, UDP, serial, pipes)
  - Timers (one-shot and periodic)
  - Signal handlers
  - Filesystem events (delegated to `fs_watcher` in `timeranger2`)
- Submit → callback pattern: every call returns immediately; the callback
  runs when the kernel reports completion.
- Cross-loop messaging primitives used by multi-yuno deployments.

:::{important}
There is **no threading**. Scaling is achieved by running one yuno per
CPU core and exchanging events between them.
:::

## Static-build helpers

`yev_loop.c` also exposes `yuneta_getaddrinfo()` /
`yuneta_freeaddrinfo()` — a UDP DNS resolver that reads `/etc/resolv.conf`
and `/etc/hosts` directly, bypassing glibc's NSS layer. When
`CONFIG_FULLY_STATIC` is enabled, all `getaddrinfo` / `freeaddrinfo` call
sites are redirected to these via macros. See the top-level `CLAUDE.md`
for the full static-binary notes.

## Benchmarks & tests

- `performance/c/perf_yev_ping_pong`, `perf_yev_ping_pong2`
- `tests/c/yev_loop`

## Source code

- [`yev_loop.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/yev_loop/src/yev_loop.c)
- [`yev_loop.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/yev_loop/src/yev_loop.h)

## Function reference

The individual function reference pages are listed in the left-hand
sidebar under **Event Loop API**.
