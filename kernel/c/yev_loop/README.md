# yev_loop

`yev_loop` is the asynchronous event loop that drives every Yuneta yuno. It is built on Linux `io_uring` (not `epoll`), so the submission and the completion of an operation need no system call each.

## What it provides

- `yev_loop_t`: the loop itself
- `yev_event_t`: one handle for each asynchronous operation:
  - File descriptor I/O (TCP, UDP, serial, pipes)
  - Timers (one-shot and periodic)
  - Signal handlers
  - Filesystem (`fs_watcher` in `timeranger2`)
- A pattern of submission and callback. Every call returns immediately. The callback runs when the kernel reports the completion.
- Delivery of posted events. Every cycle of `yev_loop_run()` starts with a call to `gobj_deliver_posted_events()`. So an action that used `gobj_post_event()` to leave its stack continues on the next turn. While posted events wait, or while the loop holds a completion that it made itself (a take-back, see below), the loop does not block on the ring.
- Primitives for messages between loops, for deployments with more than one yuno

There is no threading. To scale, you run one yuno per CPU core, and the yunos exchange events.

## Submissions, stops and addresses

- The loop never loses a submission. When the submission queue is full, the loop flushes it and asks again. If the kernel still takes nothing, the loop KEEPS the submission and makes it at the next cycle, in order, with one WARNING (*"Submission queue full and the kernel takes nothing: kept for the next cycle"*). If a failed `io_uring_submit()` left an entry in the queue, the loop submits it again at each cycle. It logs a WARNING when this starts, an ERROR after 100 cycles, and after that ERROR an INFO when the kernel takes the entry. Only a submission that the loop has no memory to keep fails. The loop logs a CRITICAL *"No memory to keep a submission"*. Then the caller answers -1 with an ERROR that says what did not happen. Up to 7.25.4, a full queue ended the process.
- A stop of a RUNNING event reaches the callback as `STOPPED` with `-ECANCELED` (or the error of the operation). It gets there in one of two ways:
    - By a cancel in the kernel.
    - By a take-back, when the submission of the event is kept or still in the queue. The loop makes that completion and delivers it at the next cycle. This is also true when a posted action made the stop.
- A stop does NOT reach the callback in these cases:
    - The event is IDLE and not a timer. It becomes `STOPPED` at once. An IDLE timer gets its callback at once, with `-ECANCELED`.
    - The event is an IDLE zero-copy send whose notification did not come yet.
    - `yev_destroy_event()` stops a RUNNING event while the loop is stopping. The loop drops the callback.
    - The stop fails for lack of memory. It answers -1 and gives up NOTHING. The event stays RUNNING with its gbuffer and its fd, and its operation completes normally later. Up to 7.25.4, the stop gave up both first.
- A stop keeps the gbuffer of an operation that the kernel still has. It releases the gbuffer at the LAST completion of the event. In the usual order, the completion of the cancel comes first and the completion of the operation comes second. Then the release happens before the callback, and the callback sees no gbuffer. If the operation completes first (it failed on its own before the cancel), the callback still sees the gbuffer. The loop then releases it at the completion of the cancel. Until then, `yev_set_gbuffer()` refuses to replace it.
- A stop closes the fd of a connect event or of a timer event. It never closes the fd of an accept event, because the listening socket belongs to its owner.
- Before the loop closes an fd, it takes back the submissions of OTHER events on that fd that the kernel did not take yet. The loop closes an fd on a stop or a free of a connect or timer event, and on the free of an accept event. An example of such a submission is a write of `C_TCP` on the socket of its connect event. Each one completes as canceled (`STOPPED`, `-ECANCELED`) at the next cycle, with a WARNING (*"An fd is closed with submissions of other events on it that the kernel did not take: taken back, completed as canceled"*). If the loop gave them to the kernel later, they will run on the file that took the fd number next. The bytes of an old connection then go to a new peer. (7.25.4 kept no submission. An entry that a failed submit left in the queue went to the kernel at the next submit that worked.)
- The loop reads the owner of an untaken submission from the entry (its fd and its event). For this reason, the loop clears both from every entry that it hands out, before it prepares the entry. Also, the stop of a RUNNING timer or connect prepares its cancel before it closes the fd. The reason is that a ring slot keeps the fd and the event of its last operation. If a scan read them, it will take back an old event. The result is an extra callback, and, if that event is freed, a completion on freed memory.
- A zero-copy UDP send has two completions (`IORING_CQE_F_MORE`, then `IORING_CQE_F_NOTIF`). The callback runs once, at the first. The notification only ends the operation. If the kernel has no zero-copy sendmsg (`yev_loop_create()` probes it), the loop uses a plain sendmsg.
- The loop keeps an IPv6 peer with its real length (`struct sockaddr_storage` plus `addrlen`, and `gbuffer_setaddr(gbuf, addr, addrlen)`). As a result, an IPv6 accept, connect and UDP answer work. A url can hold an IPv6 literal in brackets: `udp://[::1]:5000`.
- A connect binds its `src_url`: `"127.0.0.1:5000"`, `"[::1]:5000"` or `"tcp://127.0.0.1:5000"`. The loop resolves it in the family of the destination. If the `src_url` is bad, or the resolution or the bind fails, the loop logs it and the connect gets no socket.
- `yev_loop_destroy()` frees what is left. It cancels what the kernel still has and collects the completions for up to 1 s. Then it frees the rest with an ERROR (*"Loop destroyed with events whose completions did not come: freed"*, with `cancel_submitted`). A zero-copy send whose notification did not come is the exception:
    - A notification cannot be canceled. It comes when the kernel frees the packet, about 3 s later while an ARP resolution fails.
    - The kernel can still read the gbuffer. So the loop waits for the notification up to 5 s. Then it does NOT free the event, and it logs a WARNING (*"Loop destroyed with zero-copy sends whose notification did not come: NOT freed, the kernel may still read their gbuffer"*).
    - If the submission queue has no entry for that cancel, the loop first says so (*"Submission queue full: the cancel of the events left is NOT submitted, their completions may not come"*).
    - In 7.25.4, the loop never freed these events and logged nothing.

## Key files

| File | Purpose |
|---|---|
| `src/yev_loop.h/.c` | Loop API, event types, submission/callback machinery |

## Static-build helpers

`src/static_resolv.c` exposes `yuneta_getaddrinfo()` and `yuneta_freeaddrinfo()`. They are a UDP DNS resolver that reads `/etc/resolv.conf` and `/etc/hosts` directly, and does not use the NSS layer of glibc. When `CONFIG_FULLY_STATIC` is enabled, macros redirect all `getaddrinfo`/`freeaddrinfo` calls to these functions. The top-level `CLAUDE.md` has the full notes on static binaries.

The file uses only libc, on purpose. Its unit test includes the `.c` with `#include` to reach the static helpers, and links nothing else. So the resolver must not use gobj-c. For this reason, it times its own cache with `CLOCK_MONOTONIC` and not with the `start_sectimer()` helpers. It also reports through `syslog(3)` and not through `gobj_log_*` / `print_error()`.

Since 7.8.2, the resolver caches DNS answers (a table of fixed size, with no allocation). It keeps each answer for the TTL of that answer, clamped to 5..300 s. It caches only the DNS step. Numeric literals and `/etc/hosts` are already cheap. Also, if the resolver cached `/etc/hosts`, an edit of that file will not take effect at once. The resolver does not cache failures, so it uses a name server again as soon as that server recovers.

The resolution is synchronous and happens inside the event loop. So a slow resolver stops every gobj in the process, not only the socket that opens. For this reason, `yev_loop.c` warns (`getaddrinfo() BLOCKED the event loop`, msgset `OS`) when one of its three resolution points takes more than 1 s. One `nameserver` in `/etc/resolv.conf` that does not answer costs ~6 s per lookup (A + AAAA timeouts).

## Benchmarks & tests

- `performance/c/perf_yev_ping_pong`, `perf_yev_ping_pong2`
- `tests/c/yev_loop`
