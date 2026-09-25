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
- Submit → callback pattern: every call returns immediately. The callback
  runs when the kernel reports completion.
- Cross-loop messaging primitives used by multi-yuno deployments.

:::{important}
There is **no threading**. Scaling is achieved by running one yuno per
CPU core and exchanging events between them.
:::

(yev-loop-full-submission-queue)=
## A full submission queue

Every operation is submitted to the kernel as soon as it is prepared. When
the submission queue has no free entry, the loop first flushes it
(`io_uring_submit()`) and asks again. When the kernel still takes nothing
(`io_uring_enter()` fails; on an older kernel a CQ overflow answers
`EBUSY` until the completions are reaped), the submission is **kept** by
the loop, and one WARNING says it: *"Submission queue full and the kernel
takes nothing: kept for the next cycle"* (with `ret` and `sret`, the
error of the flush). A submission made while others are kept is kept
after them, so the kernel receives them in order (two writes of one
socket).

A failed `io_uring_submit()` can also leave its entry in the queue when the
queue has room. The loop does not wait for that entry to be submitted by
chance: at the start of each cycle it looks for submissions that the kernel
did not take, kept or in the queue, and submits them again. One WARNING
says it when this starts: *"Submissions the kernel did not take: submitted
again at each cycle"* (with `kept` and `in_queue`). While some are not
taken, the loop waits at most 10 ms for a completion and tries again (this
is not the timeout of the loop: its callback is not called). After 100
cycles an ERROR says that the operations still wait: *"Submissions not
taken by the kernel for many cycles of the loop: their operations wait"*
(with `cycles`, `kept`, `in_queue`, `ret` and `sret`). When the kernel
takes them after that, an INFO says it: *"Submissions taken by the kernel
again"*.

For the caller nothing changes: [`yev_start_event()`](<#yev_start_event>),
[`yev_start_timer_event()`](<#yev_start_timer_event>),
[`yev_stop_event()`](<#yev_stop_event>) and
[`yev_loop_stop()`](<#yev_loop_stop>) answer `0`, and the event is
`RUNNING` (or `CANCELING`) as after any submission.

A stop of an event whose submission the kernel did not take yet (kept, or
still in the queue) takes it back. The kernel never saw it, and if it is
submitted later it runs on an fd that the stop closed (a timer, a connect),
maybe already used by a new event. The loop finds the submission by its
event, not by its fd number. An entry in the queue becomes a NOP whose
completion is not delivered. The loop completes the event as a cancel does,
and the callback gets the event `STOPPED` with result `-ECANCELED` at the
next cycle. That completion needs a place in a list of the loop: when there
is no memory for it, the take-back is not made, a CRITICAL says so (*"No
memory to keep a completion: submission handed over as it is"*): the
submission goes to the kernel as it is, and the stop cancels it there as it
cancels any operation the kernel has.

The same applies to the submissions of **other** events on an fd that the
loop closes. A connect, an accept and a timer event own their fd: the loop
closes it at a stop (connect, timer) or at the free of the event. The writes
of `C_TCP` are other events on the socket of its connect event. Before the
close, every submission on that fd that the kernel did not take yet is taken
back as above, and its event gets its callback `STOPPED`, `-ECANCELED`, at
the next cycle, with a WARNING: *"An fd is closed with submissions of other
events on it that the kernel did not take: taken back, completed as
canceled"* (with `fd` and `type`). What the kernel already has is not
touched: the kernel holds its own reference to the file. Without memory for
that completion the submissions are dropped all the same, with an ERROR
(*"...: dropped, the event will not complete"*): an event that waits is
better than bytes sent to another file. Up to this fix they were handed to
the kernel at the next cycle and ran on whatever file had taken the number:
the bytes of an old connection went to a new peer, and the write callback
said they were sent.

```C
/*
 *  A write of a connection whose socket is closed before the kernel
 *  took the write: the write is canceled, it never reaches the file
 *  that takes the number next
 */
yev_event_h wr = yev_create_write_event(yev_loop, callback, gobj, yev_get_fd(yev_connect), gbuf);
yev_start_event(wr);            // 0, kept: the kernel takes nothing now
yev_stop_event(yev_connect);    // IDLE: its socket is closed, the write taken back
yev_loop_run(yev_loop, 1);      // callback: wr STOPPED, result -ECANCELED
```

In 7.25.4 and earlier, a full queue ended the process at each of the 12
places that ask for an entry: 10 used the NULL entry (a crash), and 2
logged *"io_uring_get_sqe() FAILED"* and aborted. An entry left in the
queue by a failed submit waited for the next submit that worked, which
could be never. The answer is `-1` now only when there is no memory to keep
the submission: a CRITICAL *"No memory to keep a submission"*, then an
ERROR of the caller that says what did not happen (*"No memory to keep a
submission: event NOT started"*, *"...: timer NOT started"*, *"...:
accept event NOT re-armed"*, ...). No caller aborts the process.

```C
/*
 *  A timer started while the kernel takes no submission: kept, and armed
 *  at the next cycle of the loop. yev_start_timer_event() answers 0.
 */
yev_event_h timer = yev_create_timer_event(yev_loop, callback, gobj);
yev_start_timer_event(timer, 100, FALSE);   // 0, the event is RUNNING
yev_loop_run(yev_loop, 1);                  // submitted here; the callback gets it IDLE

/*
 *  Stopped before the kernel took it: the callback gets it STOPPED,
 *  result -ECANCELED, and nothing runs on the closed fd
 */
yev_start_timer_event(timer, 10*1000, FALSE);
yev_stop_event(timer);                      // 0
yev_loop_run(yev_loop, 1);
```

The tests are `tests/c/yev_loop/yev_events/test_yevent_sq_full.c` (a full
queue), `test_yevent_sq_retry.c` (a failed submit, a stop with the fd used
again, many cycles), `test_yevent_sq_nomem.c` (no memory to keep) and
`test_yevent_close_fd_kept.c` (the fd of a connect closed with a write of
another event kept, and in the queue).

(yev-loop-zero-copy-sends)=
## Zero-copy sends

A sendmsg event ([`yev_create_sendmsg_event()`](<#yev_create_sendmsg_event>),
used by `C_UDP_S`) is sent with `io_uring_prep_sendmsg_zc()`. The kernel
may not copy the data: it reads the buffer of the event until the datagram
is transmitted. So one send gives **two** completions:

| Completion | `res` | Flag | Meaning |
|---|---|---|---|
| 1st | bytes sent, or `-errno` | `IORING_CQE_F_MORE` | The result of the send. A second completion follows. |
| 2nd | `0` | `IORING_CQE_F_NOTIF` | The kernel does not use the buffer any more. |

An error can come with `IORING_CQE_F_MORE` too (`-EMSGSIZE`, `-EBADF`).
When the first completion has no `IORING_CQE_F_MORE`, no second completion
comes.

The loop does this with them:

- The callback is called **once**, at the first completion, with the
  result. The notification does not call the callback.
- The operation ends at the notification. An event destroyed in its
  callback (or before the notification) is not freed at once: the loop
  frees it, with its gbuffer, when the notification arrives (or in
  [`yev_loop_destroy()`](<#yev_loop_destroy>) when the loop ends first).
- A stop of a sendmsg event keeps its gbuffer while a completion is still
  to come. The gbuffer is released at the last completion (see
  [A stop keeps the gbuffer](<#yev-loop-stop-keeps-gbuffer>)).
- The notification can arrive in any state of the event: the event can
  be sent again, or stopped, before the notification of the last send.
- When the kernel has no zero-copy sendmsg (the loop asks the kernel in
  [`yev_loop_create()`](<#yev_loop_create>)), the event is sent with a
  plain `io_uring_prep_sendmsg()`. It gives one completion, and the
  callback sees no difference.

```C
/*
 *  Send one datagram and destroy the event in its callback, as C_UDP_S
 *  does when all the data is sent. The callback is called once. The loop
 *  frees the event, and releases the gbuffer, after the notification.
 */
PRIVATE int send_callback(yev_event_h yev_event)
{
    if(yev_get_state(yev_event) == YEV_ST_IDLE) {
        // yev_get_result(yev_event) = bytes sent
    } else {
        // STOPPED: yev_get_result(yev_event) = -errno, for example -EMSGSIZE
    }
    yev_destroy_event(yev_event);   // safe: the free waits for the notification
    return 0;
}

gbuffer_t *gbuf = gbuffer_create(256, 256);
gbuffer_append_string(gbuf, "hello");
yev_event_h ev = yev_create_sendmsg_event(
    yev_loop, send_callback, gobj, fd, gbuf,
    (struct sockaddr *)&dst_addr, sizeof(dst_addr)
);
yev_start_event(ev);
```

In 7.25.4 and earlier the loop counted one completion for each
submission, and called the callback for both completions. An event
destroyed at the first completion was freed there, and the notification
read the freed event (a use-after-free). A callback that did not destroy
the event was called a second time with result `0`.

The test is `tests/c/yev_loop/yev_events/test_yevent_udp_zerocopy.c`.

(yev-loop-stop-keeps-gbuffer)=
## A stop keeps the gbuffer

A stop of a `RUNNING` read, write, recvmsg or sendmsg event submits a
cancel. The cancel is not done when it is submitted: until the completion
of the operation arrives, the kernel can still write into the gbuffer (a
read) or read from it (a write). So the event keeps its gbuffer while it
has a completion to come:

- [`yev_stop_event()`](<#yev_stop_event>) does not release the gbuffer
  of an event with a completion to come. The loop releases it at the LAST
  completion of the event. A cancel gives two completions, the cancel's own
  and the operation's, and the callback runs at the first negative one.
- In the usual order (the cancel's own completion, then the operation's
  `-ECANCELED`) the gbuffer is released **before** the callback, which sees
  the event `STOPPED`, with `-ECANCELED`, and without gbuffer
  (`yev_get_gbuf()` is `NULL`). When the operation completes FIRST -- it
  failed on its own before the cancel took effect (`-ECONNRESET`, then the
  cancel's `-ENOENT`) -- the callback runs with the gbuffer still there, and
  the loop releases it at the cancel's completion. A callback must not count
  on either: it neither frees the gbuffer nor keeps it.
- Between the stop and that completion, `yev_get_gbuf()` still returns
  the gbuffer. Do not free it and do not give it to another event.
- [`yev_set_gbuffer()`](<#yev_set_gbuffer>) with `NULL` on such an event
  releases the gbuffer at the last completion too. A new gbuffer is
  refused (logged, the new gbuffer is released, `-1`).
- An event started again before that completion uses its gbuffer again,
  and it is not released.

A reader that connects again, as `C_TCP` does:

```C
PRIVATE int yev_callback(yev_event_h yev_event)
{
    if(yev_get_state(yev_event) == YEV_ST_STOPPED) {
        // Do not free yev_get_gbuf(yev_event): the loop releases it, at the
        // last completion of the event (usually before this callback)
        return 0;
    }
    ...
}

// Connected again: the same read event, with a new gbuffer
if(!yev_get_gbuf(yev_reading)) {
    yev_set_gbuffer(yev_reading, gbuffer_create(rx_buffer_size, rx_buffer_size));
} else {
    gbuffer_clear(yev_get_gbuf(yev_reading));
}
yev_start_event(yev_reading);
```

In 7.25.4 and earlier the stop released the gbuffer at once, for every
type of event. Its memory could be freed and used again while the kernel
still had the read or the write.

The test is `tests/c/yev_loop/yev_events/test_yevent_stop_in_flight.c`.

(yev-loop-end-of-loop)=
## The end of a loop

An event destroyed with a completion still to come is freed by the loop at
that completion (see [`yev_destroy_event()`](<#yev_destroy_event>)). The
loop keeps a list of these events, so that none is lost when the loop ends
first:

- [`yev_destroy_event()`](<#yev_destroy_event>) never frees an event with
  a completion to come, also when the loop does not run: the kernel still
  has its buffers.
- [`yev_loop_destroy()`](<#yev_loop_destroy>) frees the events of the
  list. What the kernel never took (kept by the loop, or in the
  submission queue) is dropped. What the kernel has is canceled, and the
  completions are reaped for 1 second at most, without callbacks.
- An event whose completion did not come in that time is freed anyway,
  with an ERROR: *"Loop destroyed with events whose completions did not
  come: freed"*, whose `cancel_submitted` says whether the cancel was
  sent. After a cancel, a completion that does not come is a fault of the
  loop's accounting, not a slow kernel.
- A zero-copy send that had its result and waits for its notification is
  the exception. The notification cannot be canceled: it comes when the
  kernel frees the packet (a packet waiting for an ARP resolution that
  fails is held about 3 seconds), and until then the kernel may read the
  gbuffer. While one waits, the loop reaps for up to 5 seconds in all.
  If it still has not come, the event is **not** freed: it is left to the
  end of the process, with a WARNING *"Loop destroyed with zero-copy sends
  whose notification did not come: NOT freed, the kernel may still read
  their gbuffer"* (with `events`). A freed gbuffer, reused, would be sent
  with whatever was written into it. Up to this fix it was freed after 1
  second, with the ERROR above.
- When the submission queue has no entry for that cancel (full, and the
  kernel takes nothing), the loop says so before it waits: *"Submission
  queue full: the cancel of the events left is NOT submitted, their
  completions may not come"* (an ERROR). Then a completion that does not
  come is expected, not a fault of the accounting. In 7.25.4 these events
  were never freed and nothing was logged (see below).

A yuno ends like this, and it needs nothing more:

```C
yev_loop_run(yev_loop, -1);     // until the yuno must die
stop_services();                // the transports stop their events
gobj_end();                     // the gobjs destroy their events
yev_loop_stop(yev_loop);
yev_loop_destroy(yev_loop);     // frees the events still waiting
```

In 7.25.4 and earlier, an event destroyed while the loop ran, whose
completion did not come before the loop ended (a callback broke the loop,
a zero-copy notification was late), was never freed: a leak of the event
and its gbuffer. And an event destroyed after the loop ended was freed at
once, while the kernel could still have its read or its write.

The test is `tests/c/yev_loop/yev_events/test_yevent_loop_end_drain.c`.

(yev-loop-ipv6-peers)=
## IPv6 peers

A peer address is kept with its real length. An IPv4 address
(`struct sockaddr_in`) has 16 bytes, an IPv6 address
(`struct sockaddr_in6`) has 28 bytes:

- `sock_info_t.addr` is a `struct sockaddr_storage`, and
  `sock_info_t.addrlen` is its length. An accept, connect or recvmsg
  event keeps its IPv4 or IPv6 address there
  (`yev_get_sock_info()`).
- A recvmsg event gives the kernel the full `sockaddr_storage` at each
  receive. After a receive, `msghdr->msg_namelen` is the length of the
  peer address.
- [`yev_create_sendmsg_event()`](<#yev_create_sendmsg_event>) takes the
  length of the destination address, and gives it to the kernel.
- The gbuffer keeps the peer with its length:
  [`gbuffer_setaddr()`](../helpers/gbuffer.md#gbuffer_setaddr),
  [`gbuffer_getaddr()`](../helpers/gbuffer.md#gbuffer_getaddr),
  [`gbuffer_getaddrlen()`](../helpers/gbuffer.md#gbuffer_getaddrlen).
- A url can have an IPv6 literal in brackets: `udp://[::1]:5000`,
  `tcp://[2001:db8::10]:443`. The loop removes the brackets before it
  resolves the host.

An echo, as `C_UDP_S` does it. It works for an IPv4 or an IPv6 peer:

```C
PRIVATE int recv_callback(yev_event_h yev_event)
{
    if(yev_get_state(yev_event) == YEV_ST_IDLE) {
        gbuffer_t *gbuf = yev_get_gbuf(yev_event);

        // The peer, with the length that the kernel gave
        gbuffer_setaddr(
            gbuf,
            yev_event->msghdr->msg_name,
            yev_event->msghdr->msg_namelen
        );

        // The address lives in the gbuffer, and the send event holds the gbuffer
        yev_event_h yev_reply = yev_create_sendmsg_event(
            yev_get_loop(yev_event), send_callback, gobj, yev_get_fd(yev_event),
            gbuffer_incref(gbuf),
            gbuffer_getaddr(gbuf),
            gbuffer_getaddrlen(gbuf)
        );
        yev_start_event(yev_reply);
    }
    return 0;
}
```

In 7.25.4 and earlier the address was a `struct sockaddr` (16 bytes) in
`sock_info_t` and in the gbuffer, and a sendmsg event always gave the
kernel 16 bytes. An accept or connect event refused an IPv6 address, a
received IPv6 peer was cut to 16 bytes, and the kernel refused a reply to
it (`-EINVAL`). So `C_UDP_S` could not answer an IPv6 peer.

The test is `tests/c/yev_loop/yev_events/test_yevent_udp_ipv6.c`.

## Static-build helpers

`yev_loop.c` also exposes `yuneta_getaddrinfo()` /
`yuneta_freeaddrinfo()` — a UDP DNS resolver that reads `/etc/resolv.conf`
and `/etc/hosts` directly, bypassing glibc's NSS layer. When
`CONFIG_FULLY_STATIC` is enabled, all `getaddrinfo` / `freeaddrinfo` call
sites are redirected to these via macros, since glibc's resolver is not
available in a fully static build.

## Benchmarks & tests

- `performance/c/perf_yev_ping_pong`, `perf_yev_ping_pong2`
- `tests/c/yev_loop`

## Source code

- [`yev_loop.c`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c)
- [`yev_loop.h`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.h)

## Function reference

The individual function reference pages are listed in the left-hand
sidebar under **Event Loop API**.

(yev_create_accept_event)=
## [`yev_create_accept_event()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L2642)

`yev_create_accept_event()` creates a new accept event associated with the given event loop and callback function.

```C
yev_event_h yev_create_accept_event(
    yev_loop_h yev_loop,
    yev_callback_t callback,
    const char *listen_url,
    int backlog,
    BOOL shared,
    int ai_family,
    int ai_flags,
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the accept event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when the event is triggered. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `listen_url` | `const char *` | The URL to listen on (for example `"tcp://0.0.0.0:7000"`). |
| `backlog` | `int` | Queue size of pending connections for socket listening. |
| `shared` | `BOOL` | Whether to open the socket as shared (`SO_REUSEPORT`). |
| `ai_family` | `int` | Address family (for example `AF_UNSPEC`, `AF_INET`, `AF_INET6`). |
| `ai_flags` | `int` | Address info flags (for example `AI_V4MAPPED \| AI_ADDRCONFIG`). |
| `gobj` | `hgobj` | The associated `hgobj` object for event handling. |

**Returns**

Returns a `yev_event_h` handle to the newly created accept event, or `NULL` on failure.

---

(yev_create_connect_event)=
## [`yev_create_connect_event()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L2261)

`yev_create_connect_event()` creates a new connect event associated with the specified event loop and callback function.

```C
yev_event_h yev_create_connect_event(
    yev_loop_h yev_loop,
    yev_callback_t callback,
    const char *dst_url,
    const char *src_url,
    int ai_family,
    int ai_flags,
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the connect event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when the event is triggered. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `dst_url` | `const char *` | Destination URL to connect to (for example `"tcp://host:port"`). |
| `src_url` | `const char *` | The local address to bind before the connect: `"host:port"`, `"[ipv6]:port"` or `"schema://host:port"`, or `NULL`. An empty host binds any address, port `0` any port. A bad `src_url` is an error: logged, and the event has no socket. |
| `ai_family` | `int` | Address family (for example `AF_UNSPEC`, `AF_INET`, `AF_INET6`). |
| `ai_flags` | `int` | Address info flags (for example `AI_V4MAPPED \| AI_ADDRCONFIG`). |
| `gobj` | `hgobj` | The associated GObj instance for event handling. |

**Returns**

Returns a `yev_event_h` handle to the newly created connect event, or `NULL` on failure.

**Notes**

The host is resolved in the family of the destination address. In 7.25.4 a non-empty `src_url` was never parsed: a dynamic build failed the connect with *"getaddrinfo() src_url FAILED"*, and a static one bound the socket to the loopback address with a port of the kernel's choice, whatever the `src_url` said.

A destination name can resolve to several addresses (`localhost` is `::1` and `127.0.0.1`). An address in whose family the `src_url` has no address (a src `127.0.0.1` and the destination `::1`) is skipped silently, and the next address is tried; only when no address is left is it an ERROR, *"Cannot get addr to connect"*, with the `src_url`.

```C
// localhost is [::1] first here: the IPv6 address is skipped, the connect goes to 127.0.0.1
yev_event_h ev = yev_create_connect_event(
    yev_loop, callback, "tcp://localhost:5000", "127.0.0.1:40000", AF_UNSPEC, 0, gobj
);
```

A `src_url` that fails otherwise is an ERROR, and the event has no socket: *"Bad src_url: cannot bind the connect"* for a `src_url` that cannot be parsed (`"[::1:5000"`, a missing `]`) or does not fit, *"getaddrinfo() src_url FAILED"* for a host that cannot be resolved for another reason (the resolver fails), *"bind() src_url FAILED"* for a bind the kernel refuses (a port in use, an address that is not the node's), with `errno`.

```C
// Connect to [::1]:5000 from the local port 40000
yev_event_h ev = yev_create_connect_event(
    yev_loop, callback, "tcp://[::1]:5000", "[::1]:40000", AF_UNSPEC, 0, gobj
);
if(yev_get_fd(ev) < 0) {
    // bad url or src_url: already logged
}
yev_start_event(ev);
```

---

(yev_create_read_event)=
## [`yev_create_read_event()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L3087)

`yev_create_read_event()` creates a new read event associated with a given event loop, callback function, file descriptor, and buffer.

```C
yev_event_h yev_create_read_event(
    yev_loop_h      yev_loop,
    yev_callback_t  callback,
    hgobj           gobj,
    int            fd,
    gbuffer_t *    gbuf
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the read event will be registered. |
| `callback` | `yev_callback_t` | The function to be called when the read event is triggered. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `gobj` | `hgobj` | The associated object that will handle the event. |
| `fd` | `int` | The file descriptor to monitor for read events. |
| `gbuf` | `gbuffer_t *` | The buffer where the read data will be stored. |

**Returns**

Returns a `yev_event_h` handle to the newly created read event, or `NULL` if the creation fails.

**Notes**

The event will be monitored for readability, and when data is available, the specified `callback` function will be invoked.

---

(yev_create_timer_event)=
## [`yev_create_timer_event()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L2185)

`yev_create_timer_event()` creates a new timer event associated with the specified event loop and callback function.

```C
yev_event_h yev_create_timer_event(
    yev_loop_h      yev_loop,
    yev_callback_t  callback,
    hgobj           gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the timer event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when the timer event triggers. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `gobj` | `hgobj` | The associated `hgobj` object for the event. |

**Returns**

Returns a `yev_event_h` handle to the newly created timer event, or `NULL` on failure.

**Notes**

The timer event must be started using [`yev_start_timer_event()`](<#yev_start_timer_event>) before it becomes active.

---

(yev_create_write_event)=
## [`yev_create_write_event()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L3128)

`yev_create_write_event()` creates a write event associated with a given file descriptor and buffer within the specified event loop.

```C
yev_event_h yev_create_write_event(
    yev_loop_h      yev_loop,
    yev_callback_t  callback,
    hgobj           gobj,
    int            fd,
    gbuffer_t *    gbuf
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop in which the write event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when the event is triggered. If it returns -1, [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `gobj` | `hgobj` | The associated GObj instance for event handling. |
| `fd` | `int` | The file descriptor to be monitored for write readiness. |
| `gbuf` | `gbuffer_t *` | The buffer containing data to be written. If `NULL`, no buffer is associated. |

**Returns**

Returns a handle to the newly created write event (`yev_event_h`). If creation fails, `NULL` is returned.

**Notes**

The write event monitors the specified file descriptor for write readiness. Use [`yev_set_gbuffer()`](<#yev_set_gbuffer>) to modify the associated buffer.

---

(yev_destroy_event)=
## [`yev_destroy_event()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L2048)

`yev_destroy_event()` releases the resources associated with a given event. This makes sure of proper cleanup.

```C
void yev_destroy_event(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the event that will be destroyed. |

**Returns**

This function does not return a value.

**Notes**

The socket of a `timer`, `accept` or `connect` event (the fd the event
created) is closed before destruction. A `read`, `write`, `recvmsg`, `sendmsg`
or `poll` event does not own its fd: it is the caller's, and it stays open.
An event with a completion still to come (a running or canceled operation, or the notification of a zero-copy send) is not freed at once, also when the loop does not run: its callback is not called again, and the loop frees it at its last completion, or in [`yev_loop_destroy()`](<#yev_loop_destroy>). See [The end of a loop](<#yev-loop-end-of-loop>).

```C
yev_stop_event(yev_reading);    // a RUNNING read: the cancel is submitted
yev_destroy_event(yev_reading); // freed at the completion of the cancel
```

---

(yev_event_type_name)=
## [`yev_event_type_name()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L3299)

`yev_event_type_name()` returns a string representation of the event type associated with the given `yev_event_h` handle.

```C
const char *yev_event_type_name(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the event whose type name is to be retrieved. |

**Returns**

A pointer to a constant string representing the event type name.

**Notes**

The returned string is statically allocated and must not be modified or freed by the caller.

---

(yev_flag_strings)=
## [`yev_flag_strings()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L3325)

`yev_flag_strings()` returns an array of string representations for `yev_flag_t` enumeration values.

```C
const char **yev_flag_strings(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A pointer to a NULL-terminated array of strings representing `yev_flag_t` values.

**Notes**

The returned array provides human-readable names for `yev_flag_t` flags, which can be useful for debugging and logging.

---

(yev_get_state_name)=
## [`yev_get_state_name()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L1200)

`yev_get_state_name()` retrieves the name of the current state of the specified event.

```C
const char *yev_get_state_name(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | The event handle whose state name is to be retrieved. |

**Returns**

A string representing the name of the current state of the event.

**Notes**

The returned string corresponds to one of the predefined event states.

---

(yev_get_yuno)=
## [`yev_get_yuno()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L1250)

`yev_get_yuno()` retrieves the `yuno` object associated with the given event loop.

```C
hgobj yev_get_yuno(
    yev_loop_h yev_loop
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | Handle to the event loop from which to retrieve the `yuno` object. |

**Returns**

Returns the `hgobj` object associated with the specified event loop.

**Notes**

The returned `hgobj` can be `NULL` if the event loop is not properly initialized.

---

(yev_loop_create)=
## [`yev_loop_create()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L126)

`yev_loop_create()` initializes a new event loop associated with a given `hgobj` instance, allocating resources for event management.

```C
int yev_loop_create(
    hgobj          yuno,
    unsigned       entries,
    int           keep_alive,
    yev_callback_t callback,
    yev_loop_h    *yev_loop
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yuno` | `hgobj` | The `hgobj` instance associated with the event loop. |
| `entries` | `unsigned` | The maximum number of event entries the loop can handle. |
| `keep_alive` | `int` | Specifies whether the loop must persist after processing events. |
| `callback` | `yev_callback_t` | A callback function invoked for each event. Returning `-1` will break [`yev_loop_run()`](<#yev_loop_run>). |
| `yev_loop` | `yev_loop_h *` | Pointer to store the created event loop handle. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

If `callback` is `NULL`, a default callback will be used when processing events in [`yev_loop_run()`](<#yev_loop_run>).

---

(yev_loop_destroy)=
## [`yev_loop_destroy()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L293)

`yev_loop_destroy()` releases all resources associated with the given event loop and terminates its execution.

```C
void yev_loop_destroy(
    yev_loop_h yev_loop
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | Handle to the event loop to be destroyed. |

**Returns**

This function does not return a value.

**Notes**

After calling `yev_loop_destroy()`, the `yev_loop_h` handle becomes invalid and must not be used.
Before it closes the ring, it frees the destroyed events whose completions have not come: it cancels what the kernel still has, reaps the completions for 1 second at most (no callback is called), and frees what is left then with an ERROR *"Loop destroyed with events whose completions did not come: freed"*. A zero-copy send whose notification has not come is waited for up to 5 seconds, and then NOT freed, with a WARNING *"Loop destroyed with zero-copy sends whose notification did not come: NOT freed, the kernel may still read their gbuffer"*: the kernel may still read its gbuffer. When there is no submission entry for that cancel, it logs *"Submission queue full: the cancel of the events left is NOT submitted, their completions may not come"* first. See [The end of a loop](<#yev-loop-end-of-loop>).

```C
yev_loop_stop(yev_loop);
yev_loop_run_once(yev_loop);    // optional: reaps what is ready
yev_loop_destroy(yev_loop);     // frees the destroyed events still waiting
```

---

(yev_loop_reset_running)=
## [`yev_loop_reset_running()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L1087)

`yev_loop_reset_running()` resets the running state of the given event loop, clearing any active execution flags.

```C
void yev_loop_reset_running(
    yev_loop_h yev_loop
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | Handle to the event loop whose running state will be reset. |

**Returns**

This function does not return a value.

**Notes**

Use [`yev_loop_reset_running()`](<#yev_loop_reset_running>) to make sure that the event loop is reset before restarting it.

---

(yev_loop_run)=
## [`yev_loop_run()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L793)

`yev_loop_run()` starts the event loop and processes events until stopped or a timeout occurs.

```C
int yev_loop_run(
    yev_loop_h yev_loop,
    int        timeout_in_seconds
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | Handle to the event loop instance. |
| `timeout_in_seconds` | `int` | Maximum time in seconds to run the loop before returning. |

**Returns**

Returns 0 on successful execution, or -1 if an error occurs.

**Notes**

If a callback function returns -1, the loop will break and exit early.

Every cycle begins with [`gobj_deliver_posted_events()`](../gobj/events_state.md#gobj_deliver_posted_events), which delivers what the gobjs posted with [`gobj_post_event()`](../gobj/events_state.md#gobj_post_event). It happens before the completions, so an event posted while the loop was not yet running does not wait for a completion that may never arrive. While messages are pending, or the loop holds a completion it made itself (a stop that took back a submission the kernel had not taken, see [`yev_stop_event()`](<#yev_stop_event>)), the loop does not block on the ring: it takes a completion if one is ready, and returns to the queue if not.

---

(yev_loop_run_once)=
## [`yev_loop_run_once()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L970)

`yev_loop_run_once()` executes a single iteration of the event loop, processing one event if available.

```C
int yev_loop_run_once(
    yev_loop_h yev_loop
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | Handle to the event loop instance. |

**Returns**

Returns 0 on success, or a negative value on failure.

**Notes**

This function processes at most one event and then returns immediately. To continuously process events, use [`yev_loop_run()`](<#yev_loop_run>).

One turn also means one delivery of the posted events: it calls [`gobj_deliver_posted_events()`](../gobj/events_state.md#gobj_deliver_posted_events) before it reads the completions. The callers of this function use it to let pending work settle, and a posted event is pending work.

---

(yev_loop_stop)=
## [`yev_loop_stop()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L1058)

`yev_loop_stop()` stops the execution of the event loop, transitioning it to the idle state.

```C
int yev_loop_stop(
    yev_loop_h yev_loop
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | Handle to the event loop to be stopped. |

**Returns**

Returns 0 on success, or a negative value on failure.

**Notes**

Stopping the event loop using [`yev_loop_stop()`](<#yev_loop_stop>) will cause it to exit its execution cycle, but it can be restarted using [`yev_loop_run()`](<#yev_loop_run>).
With a full submission queue that the kernel does not take, the stop is kept and made at the next cycle of the loop (see [A full submission queue](<#yev-loop-full-submission-queue>)).

---

(yev_protocol_set_protocol_fill_hints_fn)=
## [`yev_protocol_set_protocol_fill_hints_fn()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L1095)

`yev_protocol_set_protocol_fill_hints_fn()` sets a custom function to fill protocol hints based on a given schema.

```C
int yev_protocol_set_protocol_fill_hints_fn(
    yev_protocol_fill_hints_fn_t yev_protocol_fill_hints_fn
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_protocol_fill_hints_fn` | `yev_protocol_fill_hints_fn_t` | A function pointer that defines how protocol hints must be filled based on the schema. |

**Returns**

Returns `0` on success, or `-1` on failure.

**Notes**

This function allows customization of protocol hint filling, which is useful for adapting to different network configurations.

---

(yev_set_gbuffer)=
## [`yev_set_gbuffer()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L1222)

`yev_set_gbuffer()` associates a [`gbuffer_t *`](#gbuffer_t) with a given `yev_event_h`. If a previous buffer exists, it is freed before setting the new one.

```C
int yev_set_gbuffer(
    yev_event_h  yev_event,
    gbuffer_t   *gbuf
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | The event handle to which the buffer will be assigned. |
| `gbuf` | `gbuffer_t *` | The buffer to associate with the event. If `NULL`, the current buffer is reset. |

**Returns**

Returns `0` on success, or `-1` if an error occurs.

**Notes**

This function is only applicable for events created using [`yev_create_read_event()`](<#yev_create_read_event>) and [`yev_create_write_event()`](<#yev_create_write_event>).
When the event has an operation in the kernel, its gbuffer is not released at once: `NULL` releases it at the last completion, and a new gbuffer is refused with an error (the new gbuffer is released). See [A stop keeps the gbuffer](<#yev-loop-stop-keeps-gbuffer>).

```C
if(!yev_get_gbuf(yev_event)) {
    yev_set_gbuffer(yev_event, gbuffer_create(4096, 4096));
}
```

---

(yev_start_event)=
## [`yev_start_event()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L1258)

`yev_start_event()` starts the specified event, transitioning it to the running state if applicable.

```C
int yev_start_event(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the event that must be started. |

**Returns**

Returns 0 on success, or a negative value on failure.

**Notes**

For timer events, use [`yev_start_timer_event()`](<#yev_start_timer_event>) instead.
With a full submission queue that the kernel does not take, the submission is kept and made at the next cycle of the loop, and the answer is `0` (see [A full submission queue](<#yev-loop-full-submission-queue>)).

---

(yev_start_timer_event)=
## [`yev_start_timer_event()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L1743)

`yev_start_timer_event()` starts a timer event, creating the handler file descriptor if it does not exist.

```C
int yev_start_timer_event(
    yev_event_h yev_event,
    time_t      timeout_ms,
    BOOL        periodic
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the event that will be started as a timer. |
| `timeout_ms` | `time_t` | Timeout in milliseconds. A value of `timeout_ms <= 0` is equivalent to calling [`yev_stop_event()`](<#yev_stop_event>). |
| `periodic` | `BOOL` | If `TRUE`, the timer will be periodic. Otherwise, it will be a one-shot timer. |

**Returns**

Returns `0` on success, or `-1` on failure.

**Notes**

To start a timer event, use [`yev_start_timer_event()`](<#yev_start_timer_event>) instead of [`yev_start_event()`](<#yev_start_event>).
If the timer is in the `IDLE` state, it can be reused. If it is `STOPPED`, a new timer event must be created.

---

(yev_stop_event)=
## [`yev_stop_event()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L1901)

`yev_stop_event()` stops the specified event. This makes sure that its associated file descriptor is closed if applicable. This operation is idempotent. This means it can be called multiple times without adverse effects.

```C
int yev_stop_event(
    yev_event_h yev_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the event that must be stopped. |

**Returns**

Returns `0` on success, or `-1` if an error occurs.

**Notes**

If the event is a `connect` or a `timer` event, its fd is closed. The fd of an `accept` event (the listening socket) is never closed by a stop: it belongs to its owner.
If the event is in an idle state, it can be reused. Otherwise, a new event must be created.
A `RUNNING` event whose submission the kernel did not take yet, kept by the loop or still in the submission queue (see [A full submission queue](<#yev-loop-full-submission-queue>)), is not canceled in the kernel: the submission is taken back, so it never runs on the closed fd, and the callback gets the event `STOPPED` with result `-ECANCELED` at the next cycle, as after a cancel.
The gbuffer of an event with a completion to come is released at its LAST completion, not by the stop. See [A stop keeps the gbuffer](<#yev-loop-stop-keeps-gbuffer>).
A take-back made by a posted action (`gobj_post_event()`) is delivered at the next cycle too: the loop does not block while it holds a completion of its own.

`-1` without memory for the cancel (the submission queue full, the kernel taking nothing, and no memory to keep the submission): *"No memory to keep a submission: event NOT canceled"*. The event is left exactly as it was -- `RUNNING`, with its gbuffer and its fd -- and its operation completes normally later, with its callback. In 7.25.4 a stop on a full submission queue released the gbuffer and closed the fd of a timer or a connect first, and then used a NULL entry: the process crashed.

A stop does not reach the callback of an event that is `IDLE` and not a timer (it becomes `STOPPED` at once), of an `IDLE` zero-copy send whose notification is still to come, and of a `RUNNING` event stopped by `yev_destroy_event()` while the loop is stopping (its callback is dropped). An `IDLE` timer gets its callback at once, `STOPPED` with `-ECANCELED`.

```C
yev_stop_event(yev_reading);    // the read is canceled; its gbuffer waits for the completion
// ... the callback gets yev_reading STOPPED, -ECANCELED; the gbuffer is released
//     at the last completion (usually before the callback: yev_get_gbuf() == NULL)
```

---

(yev_create_poll_event)=
## [`yev_create_poll_event()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L3045)

Creates a poll event for monitoring a file descriptor.

```C
yev_event_h yev_create_poll_event(
    yev_loop_h yev_loop,
    yev_callback_t callback,
    hgobj gobj,
    int fd,
    unsigned poll_mask
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the poll event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when the event is triggered. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `gobj` | `hgobj` | The associated GObj instance for event handling. |
| `fd` | `int` | The file descriptor to monitor. |
| `poll_mask` | `unsigned` | Bitmask specifying the poll conditions to monitor (for example `POLLIN`, `POLLOUT`). |

**Returns**

Returns a `yev_event_h` handle to the newly created poll event, or `NULL` on failure.

---

(yev_create_recvmsg_event)=
## [`yev_create_recvmsg_event()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L3169)

Creates a recvmsg event for receiving messages with socket address information.

```C
yev_event_h yev_create_recvmsg_event(
    yev_loop_h yev_loop,
    yev_callback_t callback,
    hgobj gobj,
    int fd,
    gbuffer_t *gbuf
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the recvmsg event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when a message is received. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `gobj` | `hgobj` | The associated GObj instance for event handling. |
| `fd` | `int` | The socket file descriptor to receive messages on. |
| `gbuf` | `gbuffer_t *` | The buffer where the received data will be stored. |

**Returns**

Returns a `yev_event_h` handle to the newly created recvmsg event, or `NULL` on failure.

**Notes**

After a receive, the peer address is in `msghdr->msg_name` (the `addr` of
`yev_get_sock_info()`, a
`struct sockaddr_storage`) and its length in `msghdr->msg_namelen`: 16
bytes for an IPv4 peer, 28 bytes for an IPv6 peer. See
[IPv6 peers](<#yev-loop-ipv6-peers>).

---

(yev_create_sendmsg_event)=
## [`yev_create_sendmsg_event()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L3218)

Creates a sendmsg event for sending messages with a destination address.

```C
yev_event_h yev_create_sendmsg_event(
    yev_loop_h yev_loop,
    yev_callback_t callback,
    hgobj gobj,
    int fd,
    gbuffer_t *gbuf,
    const struct sockaddr *dst_addr,
    socklen_t dst_addrlen
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the sendmsg event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when the message was sent. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `gobj` | `hgobj` | The associated GObj instance for event handling. |
| `fd` | `int` | The socket file descriptor to send messages on. |
| `gbuf` | `gbuffer_t *` | The buffer containing the data to be sent. |
| `dst_addr` | `const struct sockaddr *` | Pointer to the destination socket address. It is not copied: it must live as long as the event (`C_UDP_S` gives the address kept in the gbuffer of the event). |
| `dst_addrlen` | `socklen_t` | The length of `dst_addr`: `sizeof(struct sockaddr_in)` for IPv4, `sizeof(struct sockaddr_in6)` for IPv6. |

**Returns**

Returns a `yev_event_h` handle to the newly created sendmsg event, or `NULL` on failure.

**Notes**

The send is zero-copy when the kernel has it: the callback is called once,
and the loop frees a destroyed event only after the kernel releases the
buffer. See [Zero-copy sends](<#yev-loop-zero-copy-sends>).

**BREAKING** in 7.25.5: the `dst_addrlen` parameter is new. Up to 7.25.4 the
event gave the kernel `sizeof(struct sockaddr)` (16 bytes), and a send to an
IPv6 address failed with `-EINVAL`. See [IPv6 peers](<#yev-loop-ipv6-peers>).
A start with no address, or with a length of 0 or larger than a `struct
sockaddr_storage`, is refused: [`yev_start_event()`](<#yev_start_event>)
answers `-1` and logs *"Cannot start event: sendmsg addr NULL or bad addr
length"* (7.25.4: *"Cannot start event: sendmsg addr NULL"*). `C_UDP_S` then
drops that datagram (*"Cannot send datagram: dropped"*) and sends the next
one.

```C
struct sockaddr_in6 dst = {0};
dst.sin6_family = AF_INET6;
dst.sin6_addr = in6addr_loopback;
dst.sin6_port = htons(5000);

yev_event_h ev = yev_create_sendmsg_event(
    yev_loop, send_callback, gobj, fd, gbuf,
    (struct sockaddr *)&dst, sizeof(dst)
);
yev_start_event(ev);
```

---

(yev_dup2_accept_event)=
## [`yev_dup2_accept_event()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L2991)

Creates a duplicate accept event from a raw listen socket file descriptor.

```C
yev_event_h yev_dup2_accept_event(
    yev_loop_h yev_loop,
    yev_callback_t callback,
    int fd_listen,
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_loop` | `yev_loop_h` | The event loop handle in which the accept event will be created. |
| `callback` | `yev_callback_t` | The callback function to be invoked when a connection is accepted. If it returns -1, the loop in [`yev_loop_run()`](<#yev_loop_run>) will break. |
| `fd_listen` | `int` | The raw listen socket file descriptor. |
| `gobj` | `hgobj` | The associated GObj instance for event handling. |

**Returns**

Returns a `yev_event_h` handle to the newly created accept event, or `NULL` on failure.

---

(yev_dup_accept_event)=
## [`yev_dup_accept_event()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L2934)

Creates a duplicate accept event based on an existing server accept event.

```C
yev_event_h yev_dup_accept_event(
    yev_event_h yev_server_accept,
    int dup_idx,
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_server_accept` | `yev_event_h` | Handle to the existing server accept event to duplicate. |
| `dup_idx` | `int` | Index identifying the duplicate accept event. |
| `gobj` | `hgobj` | The associated GObj instance for event handling. |

**Returns**

Returns a `yev_event_h` handle to the newly created duplicate accept event, or `NULL` on failure.

---

(yev_rearm_connect_event)=
## [`yev_rearm_connect_event()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/yev_loop/src/yev_loop.c#L2314)

Prepares or reuses a connect event by establishing a connection to a destination URL.

```C
int yev_rearm_connect_event(
    yev_event_h yev_event,
    const char *dst_url,
    const char *src_url,
    int ai_family,
    int ai_flags
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `yev_event` | `yev_event_h` | Handle to the connect event to rearm. |
| `dst_url` | `const char *` | Destination URL to connect to (for example `"tcp://host:port"`). |
| `src_url` | `const char *` | The local address to bind before the connect: `"host:port"`, `"[ipv6]:port"` or `"schema://host:port"`, or `NULL`. An empty host binds any address, port `0` any port. A bad `src_url` is an error: logged, and the event has no socket. |
| `ai_family` | `int` | Address family (for example `AF_UNSPEC`, `AF_INET`, `AF_INET6`). |
| `ai_flags` | `int` | Address info flags (for example `AI_V4MAPPED \| AI_ADDRCONFIG`). |

**Returns**

Returns the file descriptor on success, or `-1` on error. The addresses of the destination are tried in order, as in [`yev_create_connect_event()`](#yev_create_connect_event): one in whose family the `src_url` has no address is skipped.

---

(set_measure_times)=
## [`set_measure_times()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/testing.c#L405)

`set_measure_times()` enables per-operation latency measurement inside
the event loop for a subset of `yev_event` types. The measurements are
surfaced by [`get_measure_times()`](#get_measure_times) and are used by
the ping-pong benchmarks under `performance/c/`.

```C
void set_measure_times(int types); // -1 = all types
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `types` | `int` | Bitmask of `yev_type_t` values to measure, or `-1` to measure every event type. Pass `0` to stop measuring. |

**Returns**

This function does not return a value.

**Notes**

Measurement is off by default to avoid paying the cost on hot paths.
Only turn it on for benchmarks or targeted diagnostics.

---

(get_measure_times)=
## [`get_measure_times()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/gobj-c/src/testing.c#L422)

`get_measure_times()` returns the bitmask of `yev_event` types that
currently have latency measurement enabled. Used together with
[`set_measure_times()`](#set_measure_times).

```C
int get_measure_times(void);
```

**Returns**

The bitmask of `yev_type_t` values being measured, or `0` if measurement
is disabled.

---

