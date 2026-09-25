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
- Delivery of posted events: every cycle of `yev_loop_run()` starts by calling `gobj_deliver_posted_events()`, so an action that used `gobj_post_event()` to leave the stack it was on continues on the next turn. While any are pending, or while the loop holds a completion it made itself (a take-back, see below), the loop does not block on the ring
- Cross-loop messaging primitives used by multi-yuno deployments

There is **no threading** — scaling is achieved by running one yuno per CPU core and exchanging events between them.

## Submissions, stops and addresses

- **A submission is never lost.** When the submission queue is full, the loop flushes it and asks again. When the kernel still takes nothing, the submission is KEPT and made at the next cycle, in order, with one WARNING (*"Submission queue full and the kernel takes nothing: kept for the next cycle"*). An entry that a failed `io_uring_submit()` left in the queue is submitted again at each cycle (a WARNING when it starts, an ERROR after 100 cycles, and after that ERROR an INFO when the kernel takes it). Only a submission that there is no memory to keep fails: a CRITICAL *"No memory to keep a submission"*, then the caller answers -1 with an ERROR that says what did not happen. Up to 7.25.4 a full queue ended the process.
- **A stop of a RUNNING event reaches the callback** as `STOPPED` with `-ECANCELED` (or the error of the operation): by a cancel in the kernel, or, when its submission is kept or still in the queue, by taking it back -- the loop makes that completion and delivers it at the next cycle, also when a posted action made the stop. A stop does NOT reach the callback when the event is IDLE and not a timer (it becomes `STOPPED` at once; an IDLE timer gets its callback at once, with `-ECANCELED`), when it is an IDLE zero-copy send whose notification is still to come, when `yev_destroy_event()` stops a RUNNING event while the loop is stopping (the callback is dropped), and when the stop fails for lack of memory: it answers -1 and gives up NOTHING -- the event stays RUNNING with its gbuffer and its fd, and its operation completes normally later (up to 7.25.4 it had given both up first). A stop keeps the gbuffer of an operation that the kernel still has, and releases it at the LAST completion of the event: in the usual order (the cancel's own completion, then the operation's) that is before the callback, which sees no gbuffer; when the operation completes first (it failed on its own before the cancel), the callback still sees the gbuffer, and it is released at the cancel's completion. `yev_set_gbuffer()` refuses to replace it until then. A stop closes the fd of a connect or a timer event, never that of an accept (the listening socket belongs to its owner). Before the loop closes an fd (a stop or a free of a connect or timer event, the free of an accept event), it takes back the submissions of OTHER events on that fd that the kernel has not taken yet -- a write of `C_TCP` on the socket of its connect event: each one completes as canceled (`STOPPED`, `-ECANCELED`) at the next cycle, with a WARNING (*"An fd is closed with submissions of other events on it that the kernel did not take: taken back, completed as canceled"*). Handed to the kernel later, they would run on whatever file took the number next: the bytes of an old connection would go to a new peer. (7.25.4 kept no submission; an entry that a failed submit left in the queue went to the kernel at the next submit that worked.) The owner of an untaken submission is read from the entry (its fd and its event), so every entry the loop hands out is cleared of both before it is prepared, and the stop of a RUNNING timer or connect prepares its cancel before it closes the fd: a ring slot keeps the fd and the event of its last operation, and a scan that read them would take back an old event -- an extra callback and, with that event freed, a completion on freed memory.
- **A zero-copy UDP send has two completions** (`IORING_CQE_F_MORE`, then `IORING_CQE_F_NOTIF`). The callback runs once, at the first; the notification only ends the operation. A kernel without zero-copy sendmsg (probed at `yev_loop_create()`) gets a plain sendmsg.
- **An IPv6 peer is kept with its real length** (`struct sockaddr_storage` plus `addrlen`; `gbuffer_setaddr(gbuf, addr, addrlen)`), so an IPv6 accept, connect and UDP answer work. A url can hold an IPv6 literal in brackets: `udp://[::1]:5000`.
- **A connect binds its `src_url`**: `"127.0.0.1:5000"`, `"[::1]:5000"` or `"tcp://127.0.0.1:5000"`, resolved in the family of the destination. A bad one, a failed resolution or a failed bind is logged and the connect gets no socket.
- **`yev_loop_destroy()` frees what is left**: it cancels what the kernel still has, collects the completions for up to 1 s, and frees the rest with an ERROR (*"Loop destroyed with events whose completions did not come: freed"*, with `cancel_submitted`). A zero-copy send whose notification has not come is the exception: a notification cannot be canceled (it comes when the kernel frees the packet, about 3 s later while an ARP resolution fails) and the kernel may still read the gbuffer, so the loop waits for it up to 5 s, and then does NOT free the event, with a WARNING (*"Loop destroyed with zero-copy sends whose notification did not come: NOT freed, the kernel may still read their gbuffer"*). When the submission queue has no entry for that cancel it says so first (*"Submission queue full: the cancel of the events left is NOT submitted, their completions may not come"*); in 7.25.4 these events were never freed and nothing was logged.

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
