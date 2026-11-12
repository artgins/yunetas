# Transport GClasses

Low-level network and serial I/O. All transport GClasses use **io_uring**
for non-blocking operations.

**Source:** `kernel/c/root-linux/src/c_tcp.c`, `c_tcp_s.c`, `c_udp.c`,
`c_udp_s.c`, `c_uart.c`

---

(gclass-c-tcp)=
## C_TCP

TCP transport — client and client-of-server. Supports optional TLS/SSL.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_DISCONNECTED`, `ST_WAIT_STOPPED`, `ST_WAIT_CONNECTED`, `ST_WAIT_HANDSHAKE`, `ST_CONNECTED` |
| **Input events** | `EV_CONNECT`, `EV_TX_DATA`, `EV_DROP`, `EV_TIMEOUT` |
| **Output events** | `EV_CONNECTED`, `EV_DISCONNECTED`, `EV_RX_DATA`, `EV_TX_READY`, `EV_STOPPED` (the stop is done) |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | Connection URL (for example `tcp://host:port`, `ssl://host:port`). |
| `connected` | `bool` | Current connection state (volatile, read-only). |
| `use_ssl` | `bool` | Enable TLS on the connection. |
| `crypto` | `json` | TLS configuration (certificates, keys). |
| `rx_buffer_size` | `integer` | Receive buffer size in bytes. |
| `timeout_inactivity` | `integer` | Inactivity timeout in milliseconds (`-1`, the default = no timeout). The timer is cleared when the connection ends, a drop included: up to 7.30.16 a drop that waited for a write in flight (`ST_WAIT_STOPPED`) left it armed, and it fired there (*"Event NOT DEFINED in state"*). It is armed only in `ST_CONNECTED`: while a TLS handshake runs the timer is the handshake's deadline (`timeout_tls_handshake`), and up to 7.30.17 each byte read there re-armed it with this time, so a peer that sent a byte now and then kept the handshake open past its deadline. |
| `timeout_between_connections` | `integer` | Idle delay between reconnection attempts, in milliseconds (default `2000`). `-1` (any negative value is the same): no timer, every connection waits for an `EV_CONNECT` of the owner (see `connect_on_start`). `0` is raised to 1000 ms with ONE warning per gobj (*"timeout_between_connections 0 never reconnects, the floor is used"*): up to 7.26.9 a 0 stopped the reconnect timer and the client never connected again, with no log (7.27.0; the same in `C_UART` and `C_POSTGRES`). Example: `json_pack("{s:s, s:i}", "url", url, "timeout_between_connections", 5000)`. |
| `timeout_between_connections_max` | `integer` | If `> timeout_between_connections`, the reconnect delay backs off exponentially from the base up to this cap (ms), resetting to base once a connection is established (for a TLS client, only on a successful handshake). `0` (default) = disabled, legacy fixed interval. A peer that keeps failing no longer hammers at the base cadence. |
| `connect_on_start` | `bool` | A client connects when it starts (default `true`). `false`: it stays in `ST_DISCONNECTED` until its owner sends `EV_CONNECT`. Only that FIRST connection waits for the owner: once a connection ends (an error, a drop by the peer) the client connects again after `timeout_between_connections`, as any client does, unless that is `-1` (no timer: every connection waits for an `EV_CONNECT` of the owner) -- `json_pack("{s:s, s:b, s:i}", "url", url, "connect_on_start", 0, "timeout_between_connections", -1)`. `C_SMTP_SESSION` starts its transport this way, so a yuno with nothing to send logs in to nobody (added after 7.25.20). Example in a gclass that builds its client: `json_pack("{s:s, s:b}", "url", url, "connect_on_start", 0)` |
| `disconnect_cause` | `string` | Why the last connection, or the last attempt to connect, ended: the error of the socket (`Connection refused`), a TLS failure with the backend's reason when it has one (`TLS handshake failed: SSL - An invalid SSL record was received`, `TLS: decrypt failed: ...`; up to 7.25.21 the mbedTLS backend gave none), `Local dropping`, `Inactivity timeout`, `Local stop`, a write or a connect that could not start. The FIRST cause of an end is kept: the cancel of the read that a drop or an inactivity close makes (`Operation canceled`) does not replace it. Emptied when a connection attempt starts (`EV_CONNECT`, or the reconnect timer, both through `ac_connect`) and when a connection begins (a clisrv gets no `EV_CONNECT`). A cause longer than 512 bytes is truncated with a warning. The `Disconnected` trace says it as its `cause`. A failed attempt publishes no event, only state changes: an owner that wants the cause reads this attr when it sees the change out of `ST_WAIT_CONNECTED` / `ST_WAIT_HANDSHAKE` -- `gobj_read_str_attr(tcp, "disconnect_cause")` -- rather than `gobj_log_last_message()`, which is the last ERROR of the whole process (added after 7.25.20). |
| `timeout_stop_tx` | `integer` | A stop (or a drop) waits for the write in flight, which ends when the peer takes its data. A peer that never does (it does not read, its window stays at zero) held the stop for ever -- a clisrv in `ST_WAIT_STOPPED`, and its `C_TCP_S`, waiting for it, never listening again (up to 7.25.21). Now the socket gets a `TCP_USER_TIMEOUT` of this many ms when the stop has to wait for a write: data not acknowledged for that long aborts the connection, the write ends with an error (`disconnect_cause`), and the stop ends. A window that stays at zero counts too: the kernel aborts at its first zero-window probe past the timeout, and the probes back off, so the abort can come some seconds later. The abort is said with ONE warning, *"The peer did not take the data in flight in timeout_stop_tx, connection aborted"* (`peername`, `sockname`, `url`). Up to 7.30.16 this bounded a clisrv only: the stop of a CLIENT closed its socket before looking for it, so a client's stop waited for ever for a peer that did not read. Default `10000`; `0`: no bound. A peer that reads, however slowly, is not affected. A server sets it on its clisrvs with `clisrv_kw`: `json_pack("{s:s, s:{s:i}}", "url", url, "clisrv_kw", "timeout_stop_tx", 1000)`. |
| `timeout_tls_handshake` | `integer` | The deadline of the TLS handshake, in ms (default `10000`; `0`: none), on a server and on a client. A handshake that has not ended when it runs out drops the connection with a warning, *"TLS handshake not ended in time, connection dropped"* (`peername`, `sockname`, `url`, `suppressed`): ONE per 10 s for the whole yuno, and `suppressed` is how many deadlines in the 10 s before it wrote none (up to 7.30.17 each one wrote its own: a peer that kept opening mute connections wrote the log at its will); a client then reconnects after `timeout_between_connections`, as after any failed connection. See *The deadline of the TLS handshake* (added in 7.30.17). |
| `rx_paused` | `bool` | Flow control of the host (default `false`, writable). `true`: no more reads of data are started; the read in flight still ends and publishes its `EV_RX_DATA`, and the reads of a TLS handshake go on. `false`: the read held by the pause starts again. See *Flow control: rx_paused* (added after 7.30.18). |
| `txBytes` | `integer` | Total bytes written to the socket (stat): what the kernel took, not what the peer received (see *The close resets the connection*). |
| `rxBytes` | `integer` | Total bytes received (stat). |
| `max_tx_in_progress` | `integer` | Most writes ever in flight at once on the connection (stat, since 7.25.8). `1` is the rule, see *One write in flight*; `0` before the first write. |
| `txMsgsDroppedAtStop` | `integer` | Messages queued to send and dropped by a local stop (stat, since 7.30.27), see *What a stop drops*. |
| `peername` | `string` | Remote peer address (read-only). |
| `sockname` | `string` | Local socket address (read-only). |
| `tcp_s` | `pointer` | A clisrv only: the `C_TCP_S` whose connection it holds, written by that server at the accept (legacy method) or at its start (new method). |

### Usage

Typically used as the bottom gobj of a protocol GClass (C_PROT_TCP4H,
C_WEBSOCKET and more.) or directly inside a C_CHANNEL.

### What `EV_CONNECTED` says

`EV_CONNECTED` carries `url`, `peername` and `sockname`. A TLS connection
publishes it when its handshake ends, and with `peer_cert` when the peer
presented a certificate: its `subject`, `issuer`, `cn`, `dns_names`,
`not_before`, `not_after`, `serial` and `verified`, as
[`ytls_get_peer_cert_info()`](#ytls_get_peer_cert_info) gives them. A server
with a CA asks a certificate of its clients (mutual TLS); `C_PROT_TCP4H`,
`C_CHANNEL` and `C_IOGATE` pass the kw on in their `EV_ON_OPEN`, so a service
above knows who each channel is. Trust it only with `verified` true:

```C
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    json_t *peer_cert = json_object_get(kw, "peer_cert");
    if(peer_cert && json_is_true(json_object_get(peer_cert, "verified"))) {
        const char *cn = json_string_value(json_object_get(peer_cert, "cn"));
        /* the channel is this cn */
    }
    KW_DECREF(kw)
    return 0;
}
```

On the server, `"ssl_verify_mode": "required"` in the `crypto` of the
`C_TCP_S` refuses a client without a certificate of the CA in the handshake.
The client's `crypto` gives its own with `ssl_certificate` and
`ssl_certificate_key`. Example: `archive_keeper`.

### The deadline of the TLS handshake

`EV_CONNECTED` of a TLS connection comes only when its handshake ends, so a
deadline of the host above -- `C_WEBSOCKET`'s `timeout_handshake`, an
inactivity timer of a protocol -- starts only then. The handshake itself is
bounded by `timeout_tls_handshake` (default 10 s): `C_TCP` arms it on its
`C_TIMER` when it enters `ST_WAIT_HANDSHAKE` (a clisrv with TLS gets that
timer at its start), and clears it when the handshake ends
(`ST_CONNECTED`) or the connection does. It costs nothing per read. A
client's inactivity timer (`timeout_inactivity`) shares that `C_TIMER` and is
armed only in `ST_CONNECTED`, so no byte of the handshake moves the deadline.
Its `EV_TIMEOUT` drops the connection:

- on a server, the channel is free again: a peer that opens a TCP connection
  to a `wss`/`tcps`/`https` port and sends nothing, or only the first bytes
  of a ClientHello, holds it for that long and no longer;
- on a client, the connection is dropped and the client reconnects after
  `timeout_between_connections`, as after a refused connect.

Up to 7.30.16 there was no such deadline: a few silent TCP connections held
every channel of a TLS server for ever (the next legitimate client got a
reset), and a TLS client whose peer accepted the TCP connection and said
nothing waited for ever, never reconnecting.

A server sets it on its channels in the `C_TCP` of the tree, or with
`clisrv_kw` for the legacy method; a client in its own kw:

```C
'children': [
    {
        'name': 'input-(^^__range__^^)',
        'gclass': 'C_WEBSOCKET',
        'kw': {
            'iamServer': true,
            'timeout_handshake': 5000
        },
        'children': [
            {
                'name': 'input-(^^__range__^^)',
                'gclass': 'C_TCP',
                'kw': {
                    'timeout_tls_handshake': 5000
                }
            }
        ]
    }
]
```

```C
json_t *kw_tcp = json_pack("{s:s, s:i}",
    "url", "wss://controlcenter.example.com:1988",
    "timeout_tls_handshake", 5000
);
```

The timer is a `C_TIMER`, which the yuno's periodic timeout drives
(`timeout_periodic`, 1 s by default): the drop comes between the deadline
and one period after it.

The drop is said with ONE warning per 10 s for the whole yuno, not per
connection: a peer that opens mute connections one after the other is one
cause. The deadlines of a window that no warning wrote are counted, and the
count is said when the window closes:

- the next deadline within 10 s of the end of the window writes the next
  warning, and it carries the count (`suppressed`);
- otherwise the count goes in a warning of its own, *"TLS handshakes not
  ended in time, connections dropped, not said in their window"*, written at
  the next deadline or at the stop of any `C_TCP`, the end of the yuno
  included.

Up to 7.30.18 the deadlines after the last warning were said by nobody when
no other deadline came, and when one came, even hours later, its warning
carried them as if they were its own window's.

```json
{"msg": "TLS handshake not ended in time, connection dropped",
 "timeout_tls_handshake": 1000, "url": "",
 "peername": "203.0.113.7:51544", "sockname": "10.0.0.2:1991",
 "suppressed": 902}
{"msg": "TLS handshakes not ended in time, connections dropped, not said in their window",
 "suppressed": 14}
```

`tests/c/c_tcp` (`test_tcp_test11`): a client with `timeout_inactivity` 2000
and `timeout_tls_handshake` 1000 against a peer that drips one byte each
200 ms is dropped at its deadline (up to 7.30.17 the drip held it open), and
four deadlines in one window write one warning; three after the window write
the next one, `"suppressed": 3`, and the two others of that window are said
at the end of the yuno, `"suppressed": 2`.

### Flow control: rx_paused

A host that writes what one `C_TCP` reads to another one (a relay) must not
read the first faster than the second writes: the queue of the second
(`max_tx_queue` is `0`, no limit) grows with everything the fast peer sends.
`rx_paused` is the host's brake. Written `true`, the connection starts no
more reads of data: the read in flight still ends and publishes its
`EV_RX_DATA`, and the reads of a TLS handshake go on, so a handshake never
waits for it. Written `false`, the read held by the pause starts again. The
peer is then held back by TCP itself (its window closes).

```C
// EV_RX_DATA of tcp_a: relay a COPY to tcp_b, then pause tcp_a
gobj_send_event(tcp_b, EV_TX_DATA, kw_copy, gobj);
gobj_write_bool_attr(tcp_a, "rx_paused", TRUE);

// EV_TX_READY of tcp_b (subscribed with
// gobj_subscribe_event(tcp_b, EV_TX_READY, NULL, gobj)): resume tcp_a
gobj_write_bool_attr(tcp_a, "rx_paused", FALSE);
```

Relay a copy: the gbuffer of an `EV_RX_DATA` is the read buffer of the
connection, read into again as soon as the publish returns, while a gbuffer
handed to a transport is that transport's until its `EV_TX_READY`.

While the reading is paused, an end of the connection by the peer is seen
when the reading resumes (a write sees it at once). The pause is the host's
and outlives a connection: a host that reuses a client sets it back to
`false` before starting it. `sgateway` uses it in both directions (see the
[sgateway](#yuno-sgateway) page; the copy and the pause came after 7.30.18).

### The stop

Every stop of a running `C_TCP` ends in `ST_STOPPED` and publishes
`EV_STOPPED` **once**, to its parent (or its `subscriber`):

- at once, inside `gobj_stop()`, when nothing was in flight -- a client
  already DISCONNECTED, waiting for its reconnect timer;
- later, from `ST_WAIT_STOPPED`, when the last read, write or connect is
  canceled or completes.

A host that stops its transport waits for `EV_STOPPED`, and destroys a
volatile one there. Because it can come INSIDE `gobj_stop()`, a host sets
whatever the action reads before it calls the stop:

```C
priv->stopping = TRUE;                  // before: EV_STOPPED may come at once
gobj_stop(priv->gobj_tcp);

PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);              // the usual host: C_CHANNEL, C_PROT_*, ...
    }
    KW_DECREF(kw)
    return 0;
}
```

A `C_TCP` destroyed BEFORE its `EV_STOPPED` -- in the turn of the stop, in
`ST_WAIT_STOPPED` -- is supported, and logs nothing: it is how every yuno ends
(`stop_services()` and then `gobj_end()`, with no turn of the loop between
them), and a host may take the same shortcut. The destroy cancels the read and
the write in flight, and the loop frees them when their completions come, with
no callback. A clisrv's socket, which its stop closes when it ends, is closed
by the destroy the way the stop closes it: `shutdown(SHUT_WR)` first, so the
peer reads the end of the connection (a FIN) even when data it sent is still
unread, which makes a bare `close()` send a RST. The FIN goes out after what
the clisrv wrote: while that output is still in the socket's send buffer (a
peer that does not read), the `close()` of a socket with unread data drops it,
and the peer gets a RST and no FIN, as at the end of a stop. Up to 7.29.3 the write events
were not kept, and that completion ran on the freed gobj (a use after free; a
crash in a plain build); and a clisrv's socket was left open (a socket leaked,
and the peer never saw the end of the connection). `tests/c/c_tcp`
(`test_tcp_test9`).

```C
gobj_stop(gobj_tcp);        // a write in flight: ST_WAIT_STOPPED
gobj_destroy(gobj_tcp);     // before EV_STOPPED: the write and the read are canceled
```

The socket of a CLIENT, which belongs to its connect event, is closed the same
way: only when its last write has completed. A drop or a stop with a write in
flight waits for it in `ST_WAIT_STOPPED` with the socket open, and a write that
completes short sends its rest to that socket, bounded by `timeout_stop_tx`.
A write completes when the kernel has TAKEN its bytes, not when the peer has
received them; see *The close resets the connection* below for what happens
to the bytes still in the socket.
Up to 7.30.17 the stop of the connect event closed the socket at once, and a
rest written in `ST_WAIT_STOPPED` went to that closed number: to EBADF (the
data lost, with no trace), or to whatever file or socket an `open()`,
`socket()` or `accept()` had given that number in the meantime. `tests/c/c_tcp`
(`test_tcp_test10`: a client dropped with 12 MB queued against a peer that
reads every 20 ms; a file opened right after the drop gets nothing, and the
peer gets every byte of the write in flight).

```C
gobj_send_event(gobj_tcp, EV_DROP, 0, gobj);    // a write in flight: ST_WAIT_STOPPED, socket open
// ... the write completes, its rest included: then the socket closes,
// and EV_DISCONNECTED comes
```

#### The close resets the connection

Every socket of `C_TCP` has `SO_LINGER` on with a time of `0`
(`set_tcp_socket_options()`), so its `close()` resets the connection: the
bytes still in the socket's send buffer -- taken by the kernel, not yet
received by a slow peer -- are discarded, and the peer gets a RST, not a FIN.
So a drop or a stop with a write in flight does NOT deliver that write whole
to a slow peer: the write completes into the send buffer, the socket closes,
and what the buffer still held is lost. `txBytes` counts it all. For
example, 8 MB written and a drop at once, against a peer that reads slowly:
the peer gets some 6 MB and a RST. (The CHANGELOG of 7.30.18 says of this case that *"the rest reaches
its peer"*: what 7.30.18 fixed is that the rest no longer goes to another file
or socket; it reaches the socket of its connection, and from there the close
decides.) It is a decision of the design, kept: a peer that does not read
holds no socket after its close. A protocol that must deliver everything
before closing waits for its peer to acknowledge it (an answer of the
protocol), then closes.

```C
// 8 MB sent, then at once:
gobj_send_event(gobj_tcp, EV_DROP, 0, gobj);
// the write completes into the send buffer; the close discards what the
// peer has not read yet, and the peer gets a RST
```

A stopped client starts again with `gobj_start()`: a new connect event, a new
socket. Up to 7.25.4 the stop of a client already disconnected published
nothing and kept its connect event: the host waited for an `EV_STOPPED` that
never came, and the next `gobj_start()` failed (*"yev_connect ALREADY
exists"*), so the client never connected again. `tests/c/c_tcp`
(`test_tcp_test6`).

### Data sent while closing

A drop, or a disconnection, takes `C_TCP` to `ST_WAIT_STOPPED` until its last
read, write or connect is canceled or completes; only then does it publish
`EV_DISCONNECTED`. Until then the layers above do not know, and they send.
That `EV_TX_DATA` goes where the pending queue of a dead connection goes
(`set_disconnected()` flushes it): away, with no error. What went is counted
and said ONCE per connection, when its close ends: *"tcp data sent while the
connection closes, dropped"*, WARNING, with `dropped_msgs` and
`dropped_bytes`. It is said also when the close does not end its own way:
a `C_TCP` destroyed while it still waits in `ST_WAIT_STOPPED` says it at its
destroy, and a count never carries into the next connection. A protocol that
must not lose data resends it on its own acknowledgements, as `C_QIOGATE`
does:

```C
gobj_send_event(tcp, EV_DROP, 0, gobj);     // ST_WAIT_STOPPED: the read is being canceled
gobj_send_event(tcp, EV_TX_DATA,            // dropped, counted; EV_DISCONNECTED follows
    json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
    gobj
);
```

Up to 7.25.10 `ST_WAIT_STOPPED` did not declare `EV_TX_DATA`: *"Event NOT
DEFINED in state"*, one ERROR per message -- ten thousand in an hour on a
simulator of 300 clients whose server went away. Up to 7.25.20 the data then went
with no trace at the default levels (only the `connections` trace said it,
per message). `tests/c/c_tcp` (`test_tcp_test7`, also a `C_TCP` stopped and
destroyed in `ST_WAIT_STOPPED`).

### What a host must not send

- An EMPTY gbuffer has nothing to send: it is ignored, with a WARNING
  (*"tcp tx data empty, ignored"*), and the connection stays up. Up to 7.29.3
  it was a write that does not start (below): two ERRORs, and the connection
  dropped.
- A gbuffer already queued, or the one in flight, is REFUSED: the send
  answers -1, with an ERROR (*"tcp tx data already queued or in flight,
  refused"*). The write consumes the bytes of its gbuffer, so the same
  bytes twice are two gbuffers. Up to 7.29.3 it was queued again, which
  failed and lost a reference, or its second write found it empty and
  dropped the connection.
- A gbuffer handed to a transport is that transport's until its
  `EV_TX_READY`: do not give it to a second transport (or to the same one
  again, or write into it) meanwhile. The refusal above sees a gbuffer
  waiting in the queue of ANY transport, but not one in flight in another
  one: there two writes would read and consume the same bytes. The check is
  not made, on purpose: it would need a mark in every gbuffer, kept by every
  consumer, for a misuse no caller in the SDK makes. To send the same bytes
  to two peers, send each its own gbuffer, copied BEFORE the first send:

  ```C
  size_t len = gbuffer_leftbytes(gbuf);
  gbuffer_t *copy = gbuffer_create(len, len);
  gbuffer_append(copy, gbuffer_cur_rd_pointer(gbuf), len);
  gobj_send_event(gobj_tcp_a, EV_TX_DATA,
      json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf), gobj);
  gobj_send_event(gobj_tcp_b, EV_TX_DATA,
      json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)copy), gobj);
  ```

`EV_TX_READY` after an empty write: with nothing in flight nor queued, it
comes ONE turn later (the transport posts it to itself), never inside the
`gobj_send_event()` of the sender; with a write in flight or queued, the
`EV_TX_READY` of that one stands for it. So a host that advances only on
`EV_TX_READY` goes on after an empty write, and does not recurse into its own
send. Only where a write is taken: in `ST_CONNECTED`, and, for a client that
connects on demand (`timeout_inactivity`), also while it is disconnected or
connecting -- an empty write there connects nothing. Where a write is dropped
(disconnected without `timeout_inactivity`, or closing) an empty one gets no
`EV_TX_READY` either: the host was told with `EV_DISCONNECTED`. In the fixes
after 7.29.3 an empty write got no `EV_TX_READY` at all, and such a host
waited for ever; and then only in `ST_CONNECTED`, so a client that connects on
demand waited for ever in `ST_DISCONNECTED`. `tests/c/c_tcp` (`test5`).

```C
gbuffer_t *gbuf = gbuffer_create(16, 16);      // EMPTY
gobj_send_event(gobj_tcp, EV_TX_DATA,
    json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),   // the kw owns the gbuffer
    gobj
);
// WARNING "tcp tx data empty, ignored": nothing sent, still connected,
// and EV_TX_READY on the next turn when nothing else was in flight
```

The same holds in [`C_UDP`](#gclass-c-udp), `C_UDP_S` (*"udp tx data ..."*,
but `C_UDP_S` publishes no `EV_TX_READY` at all: a gbuffer sent to it is its
own for good, see its *Transmit*) and [`C_UART`](#gclass-c-uart) (*"uart tx
data ..."*).

### A write that does not start

`EV_TX_DATA` sends one gbuffer at a time; the next ones wait in a queue.
A write that the loop refuses to start -- no memory to keep its submission,
a socket without fd (the cause is logged by `yev_start_event()`) -- will
never complete, and its bytes are not sent. A stream cannot go on with a
hole in it, so `C_TCP` destroys the write and DROPS the connection, with an
ERROR *"Cannot start a write: the connection is dropped"*: `EV_DISCONNECTED`
follows, and a client reconnects as after any disconnection. The same
applies to the TLS writes and to the rest of a partial write.

```C
// a write that does not start, as the log says it:
// ERROR "Cannot start event: ..." (yev_loop, the cause),
// ERROR "Cannot start a write: the connection is dropped", then EV_DISCONNECTED
```

Up to 7.25.4 the `-1` of the start was ignored: the write was counted in
progress for ever, the event and its gbuffer leaked, the connection stayed
up with every later write stuck behind it, and a stop waited in
`ST_WAIT_STOPPED` for ever. `tests/c/c_tcp` (`test_tcp_test5`).

### One write in flight

A connection never has more than one write in flight, TLS included. io_uring
may complete a write SHORT when the socket's send buffer is full, and
`C_TCP` then sends the rest with the same write: that rest must reach the
socket before anything written later, and it only does if nothing else was
submitted in between.

The plain path always kept the rule (the next gbuffer is written when the
current one completes). The TLS path did not, up to 7.25.7: `ytls` hands its
encrypted output over from several places -- the message being written, the
handshake, the records after it -- and each one started its own write at
once, and the completion of any of them took the next message while another
was still being written. With two in flight, a short one sent its rest AFTER
the other and the peer failed the record MAC: *"SSL_read() FAILED"*,
`error:0A000119` *"decryption failed or bad record mac"*, and the connection
dropped. It shows under a burst on a full socket -- a client resending its
window when a gate comes back -- and how often depends on the kernel: in a
stress test on a test VM (kernel 5.14) it dropped links 13606 times in 25
minutes. Now the encrypted output waits while a write is in flight, and the
next clear message is taken only when nothing is waiting.

The stat `max_tx_in_progress` says whether the rule held on a connection:

```C
hgobj gobj_tcp = gobj_bottom_gobj(gobj_prot);   // the transport of a C_PROT_*
json_int_t n = gobj_read_integer_attr(gobj_tcp, "max_tx_in_progress");
// 1: at most one write at a time; 2 or more: a reordering could happen
```

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=view-attrs gobj=<full name of the C_TCP> attribute=max_tx_in_progress'
```

`tests/c/c_tcps` (`test_tcps_test5`: 10 bursts of 400 messages of 64 KB over
TLS against an echo server, every echo checked in order, and
`max_tx_in_progress == 1` asserted -- it fails with `2` on the code before the
fix, on every run, while the echoes alone come back intact on most kernels).

### What a stop drops

A local stop (`gobj_stop()` of the `C_TCP`, or of a tree that holds it) ends
the connection: the write in flight goes on to its end, and what is queued
behind it is dropped. That is said, once per stop, an INFO (the stop is the
yuno's own, not a fault of the peer), with what went (7.30.27+; up to
7.30.26 it went in silence), and counted in the stat `txMsgsDroppedAtStop`:

```text
INFO: {"msg": "tcp data queued at a local stop, dropped", "url": "tcp://127.0.0.1:56745",
          "peername": "127.0.0.1:56745", "dropped_msgs": 1, "dropped_bytes": 27, ...}
```

`tests/c/c_tcp` (`test_tcp_test9`: two writes, a stop and the destroy in one
turn, on a client and on a clisrv: one INFO each, `txMsgsDroppedAtStop`
1).

---

(gclass-c-tcp-s)=
## C_TCP_S

TCP server — listens on a URL and accepts connections.
Creates a child C_TCP (inside a C_CHANNEL) for each accepted client.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |
| **Input events** | `EV_CONNECTED`, `EV_DISCONNECTED`, `EV_RX_DATA`, `EV_TX_READY`, `EV_DROP`, `EV_STOPPED` |
| **Output events** | `EV_CONNECTED`, `EV_DISCONNECTED`, `EV_RX_DATA`, `EV_TX_READY` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | Listening URL (for example `tcp://0.0.0.0:1234`). Written while the listen is down (a bad url, or a listen that failed), it is tried at once; written while it listens, it is taken at the next start. See *A listen that fails*. |
| `backlog` | `integer` | Listen backlog. |
| `shared` | `bool` | Enable port sharing (`SO_REUSEPORT`). |
| `crypto` | `json` | TLS configuration for accepted connections. |
| `only_allowed_ips` | `bool` | Accept only the peers in the yuno's `allowed_ips` list (whitelist mode). The `denied_ips` list applies with or without it. |
| `exitOnError` | `integer` | What a listen that fails does to the yuno (default `-1`). Not `0`: the yuno exits with this code (`-1`: its watcher launches it again), and with code `0` when it is the TLS context (not launched again). `0`: the server degrades and the yuno goes on. Set `0` where the listener is secondary for the yuno, as the agent does. See *A listen that fails*. 7.30.18 ignored it and degraded every yuno. |
| `listen_retry_msec` | `integer` | How long a listen whose address is in use (`EADDRINUSE`) is tried again, at each tick, before it fails as any other listen (default `5000`; `0`: no fast retry). See *Address in use at the start*. |
| `listening` | `bool` | Listening now (stat). False while stopped, while an address in use is tried again, and with `exitOnError` `0` while a failed listen is tried again or its url is bad, and while a TLS url has no TLS context. Running and `listening` false: the yuno goes on, and one ERROR said why. |
| `tls_ready` | `bool` | A TLS url (`use_ssl`) with its TLS context loaded (stat). `use_ssl` true and `tls_ready` false (`exitOnError` `0`): the certificates of `crypto` cannot be loaded, and the server does not listen. See *A TLS server without its TLS context*. |
| `connxs` | `integer` | Connections held now (stat): the connected clisrv `C_TCP`s of the channels this server serves whose `tcp_s` is this server, read when asked (up to 7.25.20 it read 0). |
| `tconnxs` | `integer` | Connections accepted since the gobj was created (stat; a stop and a start do not reset it, as `connxs` of `C_TCP` is not): counted at the accept with `child_tree_filter`, or, with the new method, where each clisrv accepts by itself, the sum of the own `connxs` of the clisrvs whose `tcp_s` is this server (up to 7.25.20 it read 0). |
| `refusedConnxs` | `integer` | Connections refused at accept by the ip lists (stat, since 7.25.5). |
| `noChannelConnxs` | `integer` | Connections not accepted because no channel of `child_tree_filter` was free: the server is full (stat, after 7.25.21). |
| `clisrv_kw` | `json` | Extra kw passed to each child client/server. |

The stats count by OWNER, not by address: a clisrv is this server's when its
`tcp_s` names it. Two servers can share one pool of channels (the agent's do)
and even one port, on two hosts, and a socket number is reused after a stop,
so neither a local port nor a listen fd says which server a connection came
from. Two servers on port 7816 over the same four channels:

```C
json_t *kw_a = json_pack("{s:s, s:{s:{s:s, s:b}}}",
    "url", "tcp://127.0.0.1:7816",
    "child_tree_filter", "kw", "__gclass_name__", "C_CHANNEL", "connected", 0
);
json_t *kw_b = json_pack("{s:s, s:{s:{s:s, s:b}}}",
    "url", "tcp://127.0.0.2:7816",
    "child_tree_filter", "kw", "__gclass_name__", "C_CHANNEL", "connected", 0
);
gobj_create("shared_a", C_TCP_S, kw_a, gate);  // the channels are children of gate too
gobj_create("shared_b", C_TCP_S, kw_b, gate);
// 2 peers on each: gobj_read_integer_attr(shared_a, "connxs") == 2, not 4
```

A pool is shared with `child_tree_filter` (the legacy method), as above,
where a channel is taken at each accept. With the new method each server
accepts in the channels it STARTS, on its own socket, so it cannot share
them: a server that starts beside another running one leaves the other's
running clisrvs to it and says so, ONE error per start (*"C_TCP_S new method:
channels served by another running C_TCP_S, left to it"*, with `channels`
and `other`). Up to 7.25.21 it took them all, silently: they accepted on its
socket, and the first server, listening, accepted nobody.

`tests/c/c_tcp_s_stats`.

### Stop and start again

A stop of the server (`gobj_stop()` of the `C_TCP_S` alone, not its tree)
stops what its start started: its accept, and, with the new method, the
clisrvs of its channels, which accept on its socket (their connections go
with them). With `child_tree_filter` a clisrv is a connection, and a stop of
the listener keeps it. The stop ends in `ST_STOPPED` when nothing of it
waits, or in `ST_WAIT_STOPPED` until the last accept is canceled and the last
clisrv has stopped. A clisrv stopped with a write in flight to a peer that
does not read waits `timeout_stop_tx` at most (see `C_TCP`).

A start that comes while the stop still waits -- a stop and a start in the
same turn, as a restart does -- does not listen at once: the old socket is
still open, and a second one could not bind. The server stays running in
`ST_WAIT_STOPPED`, and listens when the stop ends (`EV_LISTEN_AGAIN`, which it
posts to itself for the next cycle of the loop):

```C
gobj_stop(tcp_s);       // ST_WAIT_STOPPED: its accept is being canceled
gobj_start(tcp_s);      // running, still ST_WAIT_STOPPED: listens when the stop ends
```

Up to 7.25.20 that start created a second socket, whose bind failed (the
yuno exited, `exitOnError`), or overwrote the accept still being canceled (a
leak, *"dup_idx not match"*); a lone stop of a new-method server left its
clisrvs accepting on the closed socket, and its next start answered *"GObj
ALREADY RUNNING"* for each one; and a clisrv started again stayed in
`ST_STOPPED`, so its next stop never canceled its accept, which kept the port
bound. `tests/c/c_tcp_s_stats`.

The server waits for each of its clisrvs by its `EV_STOPPED`, to which it
subscribes when it first takes one (it asks the subscription, not `tcp_s`).
A clisrv destroyed before its stop ended -- its channel destroyed while the
server waits -- tells the server at its destroy, so the wait ends and a start
that came meanwhile listens. A server destroyed clears the `tcp_s` of its
clisrvs -- the bottom of every child of its parent, a `C_CHANNEL` or whatever
gobj its `child_tree_filter` matches: a new server made at the same address
must not take them for its own, and a clisrv tells its `tcp_s` at its own
destroy.

A TLS server keeps ONE ytls for its life (freed in its destroy): the clisrvs
of the connections that outlive a stop (`child_tree_filter`) still use it.
A start again reloads, in that same ytls, the certificates of its `crypto`,
as the `reload-certs` command does: new connections take the new
certificates, and a live connection keeps the context it was made with. It
reloads them only when they CHANGED: the `crypto` config (`trace_tls`
included: a change of it is applied by the reload), or a file it names
(`ssl_certificate`, `ssl_certificate_key`, `ssl_trusted_certificate`: inode,
size, mtime and ctime -- `cp -p`, `touch -r` or `rsync` can set an mtime, not
a ctime) -- so a pause and a play do not log *"TLS certificates reloaded"*
each time, and a failed reload is still an ERROR. What is recorded as loaded
is taken before the load, so a file swapped meanwhile is reloaded at the next
start. An update of the system CA bundle (`ssl_use_system_ca`) is NOT seen:
run `reload-certs` for it. So a restart is
also a way to apply renewed certificates:

```bash
ycommand -c 'command-agent service=agent_secure_port command=reload-certs'   # the same, without a restart
```

Up to 7.25.20 the start freed the ytls and made a new one: the next record of
a connection that lived across the stop was decrypted with freed memory.
`tests/c/c_tcps` (`test6`).

(tcp_s_ip_lists)=
### Address in use at the start

A listen whose address is in use is tried again, at each tick of the yuno's
periodic timeout (`timeout_periodic`, 1 s by default), for
`listen_retry_msec`. The server stays running in `ST_STOPPED` meanwhile, and
goes to `ST_IDLE` when it listens. The first refusal is a WARNING (*"Address
in use: listening again"*), each try writes the WARNING of
[`yev_create_accept_event()`](#yev_create_accept_event) (*"bind() FAILED"*,
`errno` 98), and the listen after them is an INFO (*"Listening: its address
in use was released"*, with `listen_retries`). A port still taken when the
time is over fails as any other listen (see *A listen that fails*): the yuno
exits (`exitOnError`), or with `exitOnError` `0` it is tried again from 1 s
up to each 60 s and the yuno goes on. A stop takes the retries back.

It is what a yuno killed and launched at once meets: the agent's restart of
a node (`deactivate-snap`) kills every yuno and launches them as soon as
their processes are gone, and the io_uring ring of a process is torn down
after the process, with the accept that holds its listening socket. A port
is held after the reap for a few ms on an idle machine, and for more than
100 ms with seven yunos killed at once on two CPUs. Up to 7.27.3 the start
failed at the first refusal: a yuno that binds early (an `auth_bff`) exited,
and its watcher launched it again 2 s later with a *"Daemon relaunched"*
ERROR. `tests/c/c_tcp_s_addr_in_use`.

(tcp_s_listen_fails)=
### A listen that fails

What a listen that fails does is the **yuno's choice**, with `exitOnError`.
A yuno whose listener is its role (a server of GUIs, a gateway) ends when it
cannot listen; a yuno for which the listener is one service on top of its
role (the agent: its role is managing its realms and yunos, and a dead agent
is reachable only over SSH) keeps going without it.

- **`exitOnError` not `0` (the default, `-1`)**: one ERROR of the cause, a
  WARNING *"Exiting"* (with `exitOnError`), and the yuno exits with that code.
  With `-1` its watcher launches it again (2 s later, then longer, up to each
  60 s), so a port held by another process is tried again by the relaunch.
  As up to 7.30.17.
- **`exitOnError` `0`**: the server stays running in `ST_STOPPED`,
  `listening` false, and the yuno's other listeners and services go on:

| The failure | What the server does (`exitOnError` `0`) |
|---|---|
| The socket cannot be made: an address held past `listen_retry_msec`, a host not on this machine yet, a name that does not resolve yet, a port it may not bind | ONE ERROR, *"yev_create_accept_event() FAILED"* (`url`, `errno`, `strerror`, `then`). It tries again at each timeout of its timer, the wait doubling from 1 s up to 60 s. Each try writes the cause of [`yev_create_accept_event()`](#yev_create_accept_event) (for an address in use, the WARNING *"bind() FAILED"*); the server says nothing more. One INFO when it listens: *"Listening again after a failed listen"*. |
| A bad url: empty, with no schema, of port 0 | One ERROR at each start: *"URL NULL"*, *"Parsing url failed"*, *"Cannot Listen on port 0"*. No try by itself: a url does not fix itself by waiting. Writing a good `url` listens at once, with the same INFO. |

```json
{
    "name": "agent_secure_port",
    "gclass": "C_TCP_S",
    "kw": {
        "url": "wss://0.0.0.0:1993",
        "exitOnError": 0
    }
}
```

7.30.18 degraded every yuno, ignored `exitOnError` (a WARNING when a config
set it) and said the first failure as *"Listen failed: not listening, trying
again"*; 7.30.19 gives the choice back, with the defaults and the messages of
7.30.17. The agent sets `exitOnError` `0` on its 1991 and 1993 in its
compiled config.

With `exitOnError` `0` a stop takes the tries back; a start tries again. The
`then` field of the ERROR says which of the two it is. To see it and to fix
it:

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=view-attrs gobj=<full name of the C_TCP_S> attribute=listening'
# false
ycommand -c 'command-yuno id=<id> service=__yuno__ command=write-attr gobj=<full name of the C_TCP_S> attribute=url value=tcp://0.0.0.0:2008'
# a bad url: listens now
```

```C
gobj_write_str_attr(tcp_s, "url", "tcp://0.0.0.0:2008");    // down: tried at once
```

A log watcher alerts on the ERROR, or on a server that runs with `listening`
false: with `exitOnError` `0` the process does not exit for it.
`tests/c/c_tcp_s_addr_in_use` (`exitOnError` `0`) and its
`test_exit_on_error` (the default exits, with its code).

(tcp_s_no_tls_context)=
### A TLS server without its TLS context

A TLS url (`tcps`, `wss`, `https`, `mqtts`, ...) whose certificate, key or
trusted certificate cannot be loaded never listens without a context. With
`exitOnError` not `0` (the default) the yuno exits with code **0**, after the
ERROR of the cause and *"TLS server without its TLS context: exiting"*: its
watcher does not launch it again (as up to 7.30.17, when ytls itself
exited). A yuno whose mission is to serve over TLS stops there.

With `exitOnError` `0` it does **not listen, and the yuno goes
on**. The server stays running in `ST_STOPPED`, with `use_ssl` true and
`listening` and `tls_ready` false. Its port is closed: a peer is refused at
connect, it does not reach a handshake. The yuno's other listeners and
services are not touched: the agent without its pair keeps 1991 (`ycommand`),
manages its realms and yunos, and opens 1993 when the pair is there.

It tries again at each timeout of its timer, the wait doubling from 1 s up to
60 s. A try loads nothing while `crypto` and the files it names are as at the
last refusal (their inode, size, mtime and ctime, as the start again of
*Stop and start again* compares them): such a try costs a `stat()` of each
file and says nothing. When something changed it loads them, and when they
load it listens, with one INFO. `reload-certs` (the yuno's, or the
listener's) tries at once, so the agent's cert-sync, which sends it after it
copies renewed certificates, opens the port without waiting. A stop takes
back the tries; a start tries again.

What is said, in order, for a certificate that is not there:

1. ERROR from ytls, the cause: *"SSL_CTX_use_certificate_chain_file()
   FAILED"* (OpenSSL) or *"mbedtls_x509_crt_parse_file() FAILED"* (mbedTLS),
   with the file.
2. ERROR from the server, ONCE: *"TLS server without its TLS context: not
   listening, trying again"*, with `url`, `ssl_certificate`,
   `ssl_certificate_key` and `ssl_trusted_certificate`.
3. Nothing at the tries that find the files unchanged. The ERROR of the cause
   again at a try whose files changed and still do not load, and at each
   `reload-certs`.
4. INFO when they load: *"TLS context loaded: listening"*.

To see it and to fix it on a node (the agent's secure port):

```bash
ycommand -c 'stats-agent service=agent_secure_port'
# {"listening": false, "tls_ready": false, ...}
sudo /usr/libexec/yuneta/yuneta-agent-cert ensure      # makes the pair
ycommand -c 'command-agent service=agent_secure_port command=reload-certs'
# listens now; without it, the next try of its timer (60 s at most)
```

`reload-certs` answers `-1` (*"TLS context cannot be loaded (check logs):
not listening, trying again"*) while the files still fail, and `0` (*"TLS
context loaded: listening"*) when they load. `view-cert` answers `-1`, *"TLS
server without its TLS context: not listening"*.

Up to 7.30.17 ytls exited the yuno with code 0 at the start, whatever the
yuno wanted: an agent without its pair stayed dead, and on a node with no SSH
nothing could bring it back. ytls never exits now (`ytls_init()` answers
`NULL`); the exit is C_TCP_S's, by `exitOnError`. Up to 7.30.14 the mbedTLS backend
listened with no context, and the first peer crashed the yuno in
`ytls_new_secure_filter()`. `tests/c/c_tcps` (`test14`, and
`test14_mbedtls` when both backends are built).

Both backends give NO context, the same way, for a `crypto` of a server
without `ssl_certificate`, and for a key that is not the key of the
certificate:

```json
"crypto": {
    "library": "mbedtls",
    "ssl_certificate": "/yuneta/agent/certs/new.crt",
    "ssl_certificate_key": "/yuneta/agent/certs/old.key"
}
```

OpenSSL says *"SSL_CTX_use_PrivateKey_file() FAILED"* (`key values
mismatch`), mbedTLS *"Key does not match cert"*. At the start the server
does not listen, as above. At `reload-certs` (or a copy of a new pair that
is half done: the new certificate is there, the old key still is) the
reload answers `-1` and the server KEEPS the context it had: it goes on
listening with the old pair. Up to 7.30.18 the mbedTLS backend took both: the
server listened, said `tls_ready`, and every handshake failed; a reload with
a new certificate and the old key said *"TLS certificates reloaded"* and
dropped the good context. `tests/c/ytls/test_server_init_not_fatal`.

### IP lists at accept

The yuno keeps two lists of peer ips: `denied_ips` and `allowed_ips`
(attributes of `__yuno__`, see [`is_ip_denied()`](#is_ip_denied)). `C_TCP_S`
asks them for every accepted connection, before it builds a channel for the
peer:

1. A loopback peer (`127.0.0.x`, `::1`, and `::ffff:127.0.0.x` on a
   dual-stack socket) is always accepted. A list cannot lock out the local
   control plane.
   A list names a peer by its ip without the port: `1.2.3.4`, `2001:db8::1`
   (no brackets), and `1.2.3.4` also for `[::ffff:1.2.3.4]` (see
   [`is_ip_denied()`](#is_ip_denied)). Since 7.25.5 the `add-` commands
   store what you type in that form (`2001:DB8::1` as `2001:db8::1`), refuse
   what is not an ip, and take a link-local address with its interface
   (`fe80::1%eth0`). See [The form of an entry](#yuno-ip-list-entries).
2. A peer in `denied_ips` is refused. This applies on every listener, with or
   without `only_allowed_ips`, and it wins over `allowed_ips`.
3. With `only_allowed_ips`, a peer that is not in `allowed_ips` is refused.

A refused connection is closed at once, and it does not use a channel of
the pool. Every refusal is counted in the stat `refusedConnxs`. It is logged
at info level (msgset `Connect Disconnect`) as `TCP_S: Ip denied` or
`TCP_S: Ip not allowed`, on the transition and not per connection: the first
refusal of a cause, then at most one each 60 seconds per cause, with
`refused` (the connections of that cause refused since the previous log,
this one included), the total `refusedConnxs` and the `peername` of the one
that is said. The minute is timed on the monotonic clock. Up to 7.25.4 there
was no stat, each refusal (only `TCP_S: Ip not allowed` then: the deny-list is
asked here since 7.25.5) wrote its line, and a refused host that reconnects in
a loop was a flood of the log.

```text
INFO  note_refused_connection: TCP_S: Ip denied  peername=203.0.113.7:51544 refused=1 refusedConnxs=1 next_log_in_ms=60000
INFO  note_refused_connection: TCP_S: Ip denied  peername=203.0.113.7:51702 refused=318 refusedConnxs=319 next_log_in_ms=60000
```

A server that is FULL -- with `child_tree_filter`, no channel free for the
connection -- refuses it the same way, as a third cause with its own stat,
`noChannelConnxs` (not `refusedConnxs`: "full" is not "denied"), and at
WARNING level, because it is a matter of capacity: `TCP_S: Connection not
accepted: no free child tree found`, the first one, then one a minute at most
with the count. Up to 7.25.21 each one was an ERROR and counted nowhere: 600
channels and 1000 simulated clients, which retry, made 38,156 of them in
minutes.

```text
WARN  note_refused_connection: TCP_S: Connection not accepted: no free child tree found  peername=10.0.0.9:40112 refused=1 noChannelConnxs=1 next_log_in_ms=60000
```

Example: ban one ip on every TCP listener of a yuno, and see the list:

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=add-denied-ip ip=203.0.113.7 denied=1'
ycommand -c 'command-yuno id=<id> service=__yuno__ command=list-denied-ips'
```

`denied` is required: without it the command answers -1, *"\<role^name>:
Denied, TRUE or FALSE?"*. `denied=0` writes `false`, which denies nothing
(`add-allowed-ip` asks `allowed` the same way). The list is persistent (`SDF_PERSIST`). `remove-denied-ip
ip=203.0.113.7` removes the ban.

:::{note}
Up to 7.25.4 the accept path asked only `allowed_ips`, and only with
`only_allowed_ips`. A denied ip was refused only by an authenticating gate,
at login (`C_AUTHZ`), after its channel was built, and a gate that does not
authenticate (an MQTT or IoT field port) accepted it.
:::

### Commands

| Command | Description |
|---------|-------------|
| `help` (`h`, `?`) | Its commands, or the help of one (`cmd=`). Since 7.25.17: it had commands and no `help` before, so asking it answered *"command not available"*. |
| `reload-certs` | Load the certificate again from the `crypto` attribute (what the certbot deploy hook asks after a renewal); active connections are kept. |
| `view-cert` | The certificate the listener has LOADED -- `subject`, `issuer`, `not_before`, `not_after`, `serial`, `days_remaining` -- and the `url` it listens on. -1 on a listener without TLS. |

A `C_TCP_S` is usually a child of a gate, and a service only when the yuno
makes it one -- the agent does: `agent_secure_port` is its `wss` listener.
Through the control center:

```bash
ycommand ... -c 'command-agent agent_id=artgins cmd2agent="command-agent service=agent_secure_port command=help"'
# or on the node
ycommand -c 'command-agent service=agent_secure_port command=view-cert'
```

```json
{"subject": "/CN=agent.artgins.com", "issuer": "/C=US/O=Let's Encrypt/CN=YE1",
 "not_before": 1790750856, "not_after": 1798526855,
 "serial": "0535E69BB16E40FFFA335B77E2EEDEDD7E73", "days_remaining": 89,
 "url": "wss://0.0.0.0:1993"}
```

The `url` carries the BIND address; the name a client must dial is the
certificate's CN -- which is how gui_agent's *For TreeDB* export finds
`wss://agent.artgins.com:1993` (since gui_agent 0.29.5).

---

(gclass-c-udp)=
## C_UDP

UDP client transport. Its socket is connected to its `url`: it sends there,
and reads what comes from there.

`C_UDP` is **not registered by the SDK** and `yunetas.h` does not include
its header: a yuno that uses it includes `c_udp.h` and registers it.

```C
#include <c_udp.h>

result += register_c_udp();     // in register_yuno_and_more()

priv->gobj_udp = gobj_create("udp", C_UDP,
    json_pack("{s:s}", "url", "udp://127.0.0.1:9000"),
    gobj    // the parent hears EV_RX_DATA, EV_TX_READY and EV_STOPPED
);
gobj_start(priv->gobj_udp);
```

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` (started: it reads and sends), `ST_WAIT_STOPPED` (a stop waiting for its read to be canceled and its send in flight to complete) |
| **Input events** | `EV_TX_DATA` (in `ST_IDLE`; in `ST_WAIT_STOPPED` it is dropped, see *Receive and stop*) |
| **Output events** | `EV_RX_DATA`, `EV_TX_READY` (every datagram queued is sent), `EV_STOPPED` (the stop is done) |

A host of `C_UDP` (it is a CHILD: its parent hears it unless a `subscriber`
is given) declares the three output events in its FSM.

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | The peer, for example `udp://127.0.0.1:9000`. |
| `rx_buffer_size` | `integer` | Receive buffer size (default `4096`). |
| `no_tx_ready_event` | `bool` | `true`: no `EV_TX_READY` is published (default `false`), as in `C_TCP`: `json_pack("{s:s, s:b}", "url", url, "no_tx_ready_event", 1)`. In the fixes after 7.29.3 it was read from a field no attribute set. |
| `peername` / `sockname` | `string` | The peer and the local address, once connected. |
| `txBytes` / `rxBytes` | `integer` | Byte counters (stats). |
| `txMsgs` / `rxMsgs` | `integer` | Datagram counters (stats). |

### Transmit

`EV_TX_DATA` sends one datagram at a time; the others wait in a queue, in
order, and `EV_TX_READY` is published when the queue is done. An empty
gbuffer is ignored, with a warning (*"udp tx data empty, ignored"*), and with
nothing in flight nor queued its `EV_TX_READY` comes one turn later (see
[`C_TCP`](#gclass-c-tcp), *What a host must not send*). A gbuffer already
queued or in flight is refused (-1, *"udp tx data already queued or in
flight, refused"*).

A datagram that cannot be sent is dropped, and the next one is sent:

- a send that does not start: an ERROR, *"Cannot send datagram: dropped"*.
  When that happens inside the host's `EV_TX_DATA` with nothing else in
  flight, its `EV_TX_READY` comes one turn later too, never inside the send:
  a host that sends its next datagram in its `EV_TX_READY` would recurse;
- a datagram the KERNEL refuses (nothing listens at the url:
  `ECONNREFUSED`; too big: `EMSGSIZE`): a WARNING, *"UDP: datagram refused
  by the kernel, dropped"*, with the `errno`.

```C
gbuffer_t *gbuf = gbuffer_create(64, 64);
gbuffer_append_string(gbuf, "hello");
gobj_send_event(gobj_udp, EV_TX_DATA,
    json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),   // the kw owns the gbuffer
    gobj
);
```

### Receive and stop

Every datagram from the peer is published as `EV_RX_DATA`. A read that fails
stops the client, with an ERROR (*"UDP: read FAILED, the client stops"*) and
`EV_STOPPED`: a connected UDP socket fails its read with `ECONNREFUSED` when
nothing listens at the url, and an EMPTY datagram is read as the end of the
socket. That stop, and the one of a connect that fails, is a `gobj_stop()`
the `C_UDP` makes of itself: it ends NOT running, so the host starts it again
when it wants, with a plain `gobj_start()`:

```C
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    // the C_UDP stopped (by itself, or by us): not running, it can be started
    gobj_post_event(gobj, EV_RESTART_UDP, json_object(), gobj);  // an event of this host: start it there
    KW_DECREF(kw)
    return 0;
}
```

The stop contract is the one of [`C_TCP`](#gclass-c-tcp): every stop of a
running `C_UDP` ends in `ST_STOPPED` and publishes `EV_STOPPED` once, when
its read is canceled and its send in flight completes: a turn after the stop
at least, since the read is always in flight. The socket is closed when the
stop ends, so the datagram in flight goes. The datagrams still waiting in the
queue are dropped (a WARNING says how many). `EV_STOPPED` is the last thing it
does with itself: the host may destroy it there.

While the stop waits, the host may not know yet (a stop the `C_UDP` made by
itself is told only by `EV_STOPPED`) and send: that `EV_TX_DATA` is dropped,
counted, and said ONCE when the stop ends, *"udp data sent while the client
stops, dropped"*, a WARNING with `dropped_msgs` and `dropped_bytes`, as
`C_TCP` says it. A `gobj_start()` while the stop waits is TAKEN: the `C_UDP`
opens a new socket when the stop ends, and that stop publishes no `EV_STOPPED`
(as a `C_TCP` client connects again at the end of its stop):

```C
gobj_stop(gobj_udp);        // a send in flight: ST_WAIT_STOPPED
gobj_start(gobj_udp);       // 0: taken, the socket opens again when the stop ends
```

A `C_UDP` destroyed before its `EV_STOPPED` (in `ST_WAIT_STOPPED`) is
supported and logs nothing, as with `C_TCP` (it is how every yuno ends): the
read and the send in flight are canceled, and the loop frees them with no
callback. Up to 7.29.3 it was an ERROR (*"GObj NOT STOPPED. UV handler
ACTIVE!"*), left from the libuv port.

Each start has a NEW read, also when the read of the stop is still canceling:
the stop ends at the FIRST completion of its read (its own failure, or the
cancel), and the completion of the cancel can come after it -- after a start
in the same turn as the stop, and after a start made by the host at its
`EV_STOPPED`. The loop frees that read at the end of its cancel, without
calling back, as in [`C_UDP_S`](#gclass-c-udp-s):

```C
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_start(src);        // the old read is still canceling: a new read
    KW_DECREF(kw)
    return 0;
}
```

Up to 7.29.3 `C_UDP` was a port never finished: `register_c_udp()` failed
(*"SMachine: input_list's event NOT in state"*: `EV_STOPPED` declared as an
input), `c_udp.h` declared another gclass name (`C_PROT_UDP`), the completion
of its connect was *"UDP: event type NOT IMPLEMENTED"*, only the first
datagram was sent (the completion asked for `ST_CONNECTED`, a state it does
not have: the others waited in the queue for ever and `EV_TX_READY` never
came), a refused datagram stopped it with only a trace, a stop published
nothing and did not wait for its send, its read event leaked, and `txBytes` /
`rxBytes` read 0 for ever. In the fixes after it: a read that failed with a
send in flight left it in `ST_WAIT_STOPPED` for ever, a stop made by itself
left it running (*"GObj ALREADY RUNNING"* at every later start), a start while
a stop waited was refused and left it the same, an `EV_TX_DATA` there was
*"Event NOT DEFINED in state"*, its socket was closed at the stop, under the
send in flight, a destroy with a send in flight was a use after free, and a
start while a stop waited, or at its `EV_STOPPED`, armed the read again with
the completion of its cancel still to come, which failed it: the `C_UDP`
stopped by itself (*"UDP: read FAILED, the client stops"*) at every such
restart.
`tests/c/c_udp_tx` (`test_c_udp_tx`, `test_c_udp_tx_refused`).

---

(gclass-c-udp-s)=
## C_UDP_S

UDP server — listens for datagrams on a configured URL, and sends datagrams
to the peer each gbuffer names.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_WAIT_STOPPED` (a stop waiting for its read to be canceled and its sends to complete), `ST_IDLE` |
| **Input events** | `EV_TX_DATA` (in `ST_IDLE`) |
| **Output events** | `EV_RX_DATA` (the gbuffer carries the peer address, and the peer as its label), `EV_STOPPED` (the stop is done). `EV_TX_READY` is declared and never published: see *Transmit* for what that means for a gbuffer sent. |

A host of `C_UDP_S` (it is a CHILD: its parent hears it unless a `subscriber`
is given) declares `EV_RX_DATA` and `EV_STOPPED` in its FSM. Up to 7.25.4 the
event table declared the STATE `ST_STOPPED` as an output event and nothing was
published at the end of a stop.

### Receive

Every datagram is published as `EV_RX_DATA`. Its gbuffer has the peer address
(`gbuffer_getaddr()`, so an answer written in it goes back to the sender) and
the peer as its LABEL, `"ip:port"` or `"[ipv6]:port"`
(`gbuffer_getlabel()`). [`C_GSS_UDP_S`](#gclass-c-gss-udp-s) keys its channels
by that label, and joins the pieces of a frame per channel until the NUL. Up to
7.25.4 the label was written only when tracing: every peer was the channel
`""`, and the pieces of the long log lines of two yunos (a line longer than one
datagram is sent in pieces) were joined into one corrupt frame on logcenter.

```C
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    const char *peer = gbuffer_getlabel(gbuf);          // "127.0.0.1:40123"
    // ... the datagram is gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf) bytes
    KW_DECREF(kw)
    return 0;
}
```

Every datagram asks the yuno's ip lists, as `C_TCP_S` asks them for a
connection ([IP lists at accept](#tcp_s_ip_lists)), before it is published:

1. A loopback peer (`127.0.0.x`, `::1`, `::ffff:127.0.0.x`) is always heard.
2. A peer in `denied_ips` is dropped, with or without `only_allowed_ips`, and
   it wins over `allowed_ips`: *"UDP_S: Ip denied, datagram dropped"*.
3. With `only_allowed_ips`, a peer that is not in `allowed_ips` is dropped:
   *"UDP_S: Ip not allowed, datagram dropped"*.

Every dropped datagram is counted in the stat `rxRefusedMsgs`. The drops are
SAID on the transition, not per datagram, because the source address of a
datagram can be forged and a flood of them was a flood of the log: the first
drop of a cause is a WARNING, then at most one each 60 seconds per cause, with
`dropped` (the datagrams of that cause dropped since the previous warning, this
one included), the total `rxRefusedMsgs`, the `peername` of the datagram that
is said and its length. The minute is timed on the monotonic clock: a clock set
back does not silence the warnings. Up to 7.25.4 `only_allowed_ips` was
documented and never read, and the deny-list was not asked: every peer was
heard.

```text
WARN  note_refused_datagram: UDP_S: Ip denied, datagram dropped  peername=203.0.113.7:5000 len=64 dropped=1 rxRefusedMsgs=1 next_warning_in_ms=60000
WARN  note_refused_datagram: UDP_S: Ip denied, datagram dropped  peername=203.0.113.9:5000 len=64 dropped=4211 rxRefusedMsgs=4212 next_warning_in_ms=60000
```

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=view-attrs gobj=<full name of the C_UDP_S>'   # rxRefusedMsgs, among its attrs
```

```C
json_t *kw_udp = json_pack("{s:s, s:b}",
    "url", "udp://0.0.0.0:5000",
    "only_allowed_ips", 1
);
```

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=add-allowed-ip ip=10.0.0.7 allowed=1'
ycommand -c 'command-yuno id=<id> service=__yuno__ command=add-denied-ip ip=203.0.113.7 denied=1'
```

A read that FAILS while the server runs (not a read canceled by a stop) stops
the server: logged as an ERROR, *"UDP: read FAILED, the server stops
listening"*. The only other stops the server makes by itself are about its
NEXT read: no memory for its new gbuffer (see *Transmit*: the host kept the
previous one), an ERROR *"UDP: no memory for the next read, the server stops
listening"*; and a read that cannot be started again (the cause logged by
`yev_start_event()` first -- no memory to keep the submission, or a gbuffer
with no room, as an `rx_buffer_size` of 0 makes it), an ERROR *"UDP: the read
cannot be started again, the server stops listening"*. Both carry the
`rx_buffer_size`. Up to 7.25.4 the second was not seen: the server stayed in
`ST_IDLE`, "running", and read nothing more, with nothing logged.

A stop the server makes by itself ends like any stop: in `ST_WAIT_STOPPED`
while a send is in flight, then `ST_STOPPED` and `EV_STOPPED` when that send
completes. The gobj still runs, so its host decides what to do (stop it, or
stop and start it again). Up to 7.25.4 a self-stop with a send in flight never
ended: the completion of the send took the path of a running server, the state
stayed `ST_WAIT_STOPPED`, `EV_STOPPED` was never published, and every
`EV_TX_DATA` of the host answered *"Event NOT DEFINED in state"*.
`tests/c/c_udp_s_self_stop`.

```C
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *  A C_UDP_S that stopped by itself still runs. Listen again LATER:
     *  EV_STOPPED is published inside its stack, never stop or start it here.
     */
    if(gobj_is_running(src)) {
        gobj_post_event(gobj, EV_RESTART_LISTENER, json_object(), gobj);   // an event of this host
    }
    KW_DECREF(kw)
    return 0;
}
```

### Transmit

`EV_TX_DATA` carries a gbuffer whose peer address was set with
`gbuffer_setaddr()` -- the gbuffer of an `EV_RX_DATA` has it already, so an
answer goes back to the sender. One datagram is sent at a time; the others
wait in a queue, in order, and each completion sends the next:

```C
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    // The answer IN the rx gbuffer: rewrite it (or not, an echo) and send the kw back
    return gobj_send_event(gobj_udp_s, EV_TX_DATA, kw, gobj);
}
```

The rx gbuffer is the host's to keep: when the host still holds it after the
`EV_RX_DATA` (an answer in it waiting in the queue or in flight, or a
reference kept for later), `C_UDP_S` reads the next datagram into a NEW
gbuffer, of `rx_buffer_size`; when nobody kept it, the same gbuffer is read
into again, as always. Up to 7.25.4 it was always cleared and read into again:
an answer in it was sent empty (dropped: *"Cannot start event: gbuffer WITHOUT
data to write"*, *"Cannot send datagram: dropped"*) or with the bytes and the
peer of the next datagram, and a zero-copy send could read memory the next read
was writing. `tests/c/c_udp_s_echo`.

A new datagram is built like this:

```C
gbuffer_t *gbuf = gbuffer_create(64, 64);
gbuffer_append_string(gbuf, "hello");
gbuffer_setaddr(gbuf, (struct sockaddr *)&peer, sizeof(peer));   // a sockaddr_in or sockaddr_in6
gobj_send_event(gobj_udp_s, EV_TX_DATA,
    json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),   // the kw owns the gbuffer
    gobj
);
```

A gbuffer sent is `C_UDP_S`'s from then on, for good: it publishes no
`EV_TX_READY`, so nothing tells the host when the send is done. The host does
not write into it again, nor send it again: each datagram is a gbuffer of its
own, and the same bytes to two peers are two gbuffers, copied before the first
send (as in [`C_TCP`](#gclass-c-tcp), *What a host must not send*). An answer
written in the rx gbuffer is the same rule: that gbuffer goes with the send.

An EMPTY gbuffer is not a datagram to send: it is ignored, with a WARNING
(*"udp tx data empty, ignored"*); up to 7.29.3 it was dropped with two
ERRORs. A gbuffer already queued, or the one in flight, is REFUSED: the send
answers -1, with an ERROR (*"udp tx data already queued or in flight,
refused"*); up to 7.29.3 it was queued again, which failed and lost a
reference.

A datagram that cannot be sent is DROPPED, and the next one in the queue is
sent; nothing leaks, and the server goes on:

- a gbuffer with no peer address, or no memory to keep the submission: an
  ERROR, *"Cannot send datagram: dropped"*, after the cause logged by
  `yev_start_event()`; a stop does not wait for it in `ST_WAIT_STOPPED`;
- a datagram the KERNEL refuses -- a peer port 0 or an address of another
  family (`EINVAL`, `EAFNOSUPPORT`), a broadcast address without
  `set_broadcast` (`EACCES`), a datagram too big (`EMSGSIZE`): a WARNING,
  *"UDP: datagram refused by the kernel, dropped"*, with the `remote-addr`,
  the `errno` and the `strerror`. Up to 7.25.4 a refused send was taken as a
  disconnection: the whole server stopped (reading too), and only a trace
  said so.

Up to 7.25.4 only the FIRST datagram of a queue was sent: the completion asked
for a state `C_UDP_S` does not have (`ST_CONNECTED`) before sending the next
one. `tests/c/c_udp_s_tx`.

### Stop and start again

A stop cancels the read and closes the socket; the datagrams still WAITING in
the queue are dropped (a WARNING says how many), and the one in flight
completes on its own. When the read is canceled and the sends are done, the
server is in `ST_STOPPED` and publishes `EV_STOPPED`. A `C_UDP_S` destroyed
before its `EV_STOPPED` (in `ST_WAIT_STOPPED`) is supported and logs nothing,
as with `C_TCP` (it is how every yuno ends): its read and its sends in flight
are canceled, and the loop frees them with no callback. Up to 7.29.3 the send events were not kept, and their completion ran
on the freed gobj (a use after free); and the destroy was an ERROR (*"GObj NOT
STOPPED. UV handler ACTIVE!"*), left from the libuv port.
`tests/c/c_udp_s_tx`.

A start after a stop opens a NEW socket and starts a NEW read on it -- also
when the read of the stop is still canceling (a stop and a start in the same
turn, as a `pause-yuno` and `play-yuno` of logcenter can be): that read is freed
by the loop at the end of its cancel, without calling back. Up to 7.25.4 the
read of the previous start was started again on the number of the socket the
stop had closed -- by then any file of the process (logcenter opens its log
file just before it starts its listener) -- and the server stopped reading with
nothing logged; and the datagram being sent at the stop was kept, so every
datagram after the start waited behind it for ever. `tests/c/c_udp_s_restart`,
and `tests/c/c_udp_s_rx` for the receive side.

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | Listening URL: `udp://0.0.0.0:5000`, or an IPv6 literal in brackets, `udp://[::1]:5000` (since 7.25.5: an IPv6 peer is kept with its real length and answered; up to 7.25.4 its address was cut to 16 bytes and the reply refused, `-EINVAL`). A secure url (`udps://`) is REFUSED at the start: TLS over datagrams is DTLS, which ytls does not implement. An ERROR, *"A secure url (udps://) is not supported by C_UDP_S: there is no DTLS"*, and the yuno exits with code 0 (`exitOnError`, the default), or with `exitOnError` false the start fails and the yuno goes on. Up to 7.25.4 it was accepted, the server listened with no TLS session, and the first datagram crashed the yuno. |
| `use_ssl` | `bool` | Always `false` (see `url`). The commands `reload-certs` and `view-cert` answer -1, *"Listener is not TLS-enabled or not running"*. |
| `shared` | `bool` | Enable port sharing. |
| `set_broadcast` | `bool` | Enable broadcast. |
| `only_allowed_ips` | `bool` | Hear only the peers in the yuno's `allowed_ips` (see *Receive*; `denied_ips` applies with or without it); writable, it takes effect at the next datagram. |
| `rx_buffer_size` | `integer` | Receive buffer size: the gbuffer of each read (a new one when the host kept the previous one, see *Transmit*). |
| `txMsgs` / `rxMsgs` / `txBytes` / `rxBytes` | `integer` | Counters (stats). |
| `rxRefusedMsgs` | `integer` | Datagrams dropped by the ip lists (stats, see *Receive*). |
| `listening` | `bool` | Listening now (stat). False while stopped, and with `exitOnError` false while a failed listen is tried again, and when its url is bad. See *A listen that fails*. |
| `exitOnError` | `bool` | What a listen that fails does to the yuno (default `true`): `true`, the yuno exits with code 0 (its watcher does not launch it again); `false`, the server degrades and the yuno goes on. 7.30.18 ignored it and degraded every yuno. `C_GSS_UDP_S` passes its own `exitOnError` to its `C_UDP_S`. |

### A listen that fails

What a listen that fails does is the yuno's choice, with `exitOnError`. With
`true`, the default, the yuno exits with code 0 after the ERROR of the cause
(its `then` says *"exiting with code 0 (exitOnError)"*), and code 0 tells its
watcher NOT to launch it again: the UDP listener of logcenter is its role. As
up to 7.30.17; 7.30.18 degraded every yuno and ignored `exitOnError`.

With `exitOnError` false the server stays running in `ST_STOPPED`,
`listening` false (its start answers `-1` for a bad url, `0` when a try is
armed), and the yuno goes on:

- **The socket or its read cannot be made** (an address in use, a host not on
  this machine yet, a name that does not resolve yet): ONE ERROR,
  *"yev_create_accept_event() FAILED"* (`url`, `errno`, `strerror`), and a
  try at each timeout of its timer, the wait doubling from 1 s up to 60 s.
  Each try writes the cause of `yev_create_accept_event()` (the WARNING
  *"bind() FAILED"* for an address in use); the server says nothing more.
  A read that cannot be started is the same, its ERROR *"Cannot start the
  read of the UDP server"* (7.30.18 said *"Cannot start the read of the UDP
  server: not listening, trying again"*), and its cause from
  `yev_start_event()` at each try. *"UDP listening ..."* is said when it
  listens, once, and after a failure one more INFO, *"Listening again after
  a failed listen"*.
- **A bad url** (empty, with no schema, of port 0, `udps://`): one ERROR at
  each start (*"URL NULL"*, *"Parsing url failed"*, *"Cannot Listen on port
  0"*, the `udps://` one above), and no try: `url` is read only, a new config
  and a start are needed.

A stop takes the tries back. A log watcher alerts on the ERROR: with
`exitOnError` false the process does not exit for it.

```C
json_t *kw = json_pack("{s:s, s:b}", "url", "udp://0.0.0.0:5000", "exitOnError", 0);
hgobj udp = gobj_create("udp", C_UDP_S, kw, gate);
gobj_start(udp);    // the port held: 0, running, listening false, tried again
```

`tests/c/c_udp_s_self_stop` (`exitOnError` false), and
`tests/c/c_tcp_s_addr_in_use`'s `test_exit_on_error` (the default exits).

### Commands

The same three as `C_TCP_S`: `help` (since 7.25.17), `reload-certs` and
`view-cert` -- the last two answer -1 here, a `C_UDP_S` has no TLS.

```bash
ycommand -c 'command-yuno id=<yuno> service=<udp server service> command=help'
```

---

(gclass-c-uart)=
## C_UART

Serial port (UART/TTY) transport. It opens its device on start (100 ms
after), and opens it again `timeout_between_connections` after a failure or
a drop.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_DISCONNECTED`, `ST_CONNECTED` |
| **Input events** | `EV_CONNECT`, `EV_TX_DATA` (in `ST_CONNECTED`; in `ST_DISCONNECTED` it is dropped, with an error), `EV_DROP` (in `ST_CONNECTED`), `EV_TIMEOUT` |
| **Output events** | `EV_CONNECTED`, `EV_DISCONNECTED`, `EV_RX_DATA`, `EV_TX_READY` (every write queued is done), `EV_STOPPED` (the stop is done) |

A host of `C_UART` (it is a CHILD: its parent hears it unless a `subscriber`
is given) declares all five output events in its FSM, in every state where
they can come. `EV_DISCONNECTED` and `EV_STOPPED` come inside `gobj_stop()`,
so a host declares them in the state it is in when it stops the `C_UART`.
Up to 7.29.3 `EV_STOPPED` was never published:
a host written against 7.29.3 that does not declare it answers *"Event NOT
DEFINED in state"* at every stop.

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | Device path (for example `/dev/ttyUSB0`). |
| `baudrate` | `integer` | Baud rate (default `9600`). A rate the tty does not know is an error (*"Bad baudrate: the line runs at 9600"*), the line runs at 9600, and the attribute is set to `9600`. A rate the driver adjusts (it took another one) is a warning (*"The tty took another baudrate: the line runs at it"*), and the attribute is set to the rate the tty took, read back from the tty after it is configured. Whoever counts the time of a frame on the line (the [Modbus master](#module-modbus), the M-Bus master) reads the speed of the line here. |
| `bytesize` | `integer` | Data bits (`5`–`8`, default `8`). Another value is an error, and `8` is used and written back. |
| `parity` | `string` | Parity: `"none"` (default), `"odd"`, `"even"`. Another value is an error, and `"none"` is used and written back. |
| `stopbits` | `integer` | Stop bits (`1` or `2`, default `1`). Another value is an error, and `1` is used and written back. |
| `xonxoff` | `bool` | Software flow control. |
| `rtscts` | `bool` | Hardware flow control. |
| `timeout_between_connections` | `integer` | Milliseconds between two opens (default `2000`; `0` is raised to 1000 ms, with one warning). |
| `max_tx_queue` | `integer` | Writes that may wait behind the one in flight (default `0`: no limit, as in `C_TCP`). When the queue is full the oldest waiting write is dropped, with an error (*"uart tx queue full, the oldest write dropped"*, said for the first drop and then once every `max_tx_queue` drops). Example: `"max_tx_queue": 16`. |

```json
{"name": "serial_bus", "gclass": "C_UART",
 "kw": {"url": "/dev/ttyUSB0", "baudrate": 9600, "parity": "none"}}
```

### Writes

Every `EV_TX_DATA` is queued, and the writes go to the tty **one at a time,
in order**. When the line takes only part of a write (a short write: the
output buffer of the tty is full), the rest goes before the next write.
`EV_TX_READY` is published when the last write queued is done, not once per
`EV_TX_DATA`. The queue has no limit unless `max_tx_queue` is set. A write
that the kernel interrupts (`EINTR`: the write of a tty runs blocking in a
worker of io_uring) goes again; so does a read. Not for ever: 16
interruptions within one second drop the line, with an error (*"The tty is
interrupted again and again: the line is dropped"*). A read that cannot be
started again drops the line too, with an error: it does not stay connected
and deaf.

What a host must not send:

- An empty gbuffer is ignored, with a warning (*"uart tx data empty,
  ignored"*). With nothing in flight nor queued its `EV_TX_READY` comes one
  turn later, never inside the send; with a write in flight or queued, the
  `EV_TX_READY` of that one stands for it (as in [`C_TCP`](#gclass-c-tcp)).
  In 7.29.3 it was an error, and a write event lost with its gbuffer; in the
  fixes after it, no `EV_TX_READY` came, and a host that advances only on it
  waited for ever.
- A gbuffer already queued, or the one in flight, is refused: the send
  answers -1, with an error (*"uart tx data already queued or in flight,
  refused"*). To send the same bytes twice, send two gbuffers. In 7.29.3
  two writes ran on the same gbuffer at once.
- An `EV_TX_DATA` in `ST_DISCONNECTED` is dropped, with an error (*"uart tx
  data while disconnected, dropped"*), as `C_TCP` does: the host was told by
  `EV_DISCONNECTED`. Up to 7.29.3 it was *"Event NOT DEFINED in state"*.

```C
gbuffer_t *gbuf = gbuffer_create(len, len);
gbuffer_append(gbuf, frame, len);
gobj_send_event(priv->gobj_uart, EV_TX_DATA,
    json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
    gobj
);
```

Up to 7.29.3 each `EV_TX_DATA` was one write, and nothing looked at how much
of it went: a write of 64 KB to a line that took 11,776 bytes sent those,
published `EV_TX_READY`, and lost the rest with no log. An interrupted write
or read dropped the line.

### The drop

`EV_DROP` (in `ST_CONNECTED`) drops the line: the writes are cut (as in a
stop, below), the tty is closed, `EV_DISCONNECTED` is published, and the
device is opened again `timeout_between_connections` later. The
[Modbus master](#module-modbus) sends it down when it cannot
frame what it reads.

```C
gobj_send_event(priv->gobj_uart, EV_DROP, 0, gobj);  // EV_DISCONNECTED comes inside
```

Up to 7.29.3 `C_UART` had no `EV_DROP`: the event was refused with an error,
and nothing was dropped.

### The stop

`C_UART` keeps the stop contract of [`C_TCP`](#gclass-c-tcp): every stop of a
running `C_UART` ends in `ST_STOPPED`, with its tty closed,
`EV_DISCONNECTED` published if it was connected, and then `EV_STOPPED`
**once**, to its parent (or its `subscriber`):

**at once, inside `gobj_stop()`**, always. A `C_UART` waits for nothing in
flight: its read and its write are freed with the line, as a drop frees
them (the event loop frees each one when its completion comes, and calls
nothing). So a `gobj_start()` right after `gobj_stop()`, in the same turn,
is taken, and opens the device again: a host that changes the `url` of its
line does exactly that.

A start never opens the device inside `gobj_start()`: it opens it 100 ms
later, on its timer, the start after a stop like any other. The open of a
tty is synchronous, so an open inside the start would publish
`EV_CONNECTED` inside the host's `gobj_start()`, before the host is ready
for it; a `C_TCP`'s `EV_CONNECTED` comes later too, when its connect
completes. A timer, and not an event posted to itself: a stop before the open
cancels the timer, while a posted `EV_CONNECT` would still come, to a stopped
`C_UART`.

```C
gobj_write_str_attr(priv->gobj_uart, "url", "/dev/ttyUSB1");
gobj_stop(priv->gobj_uart);     // EV_DISCONNECTED and EV_STOPPED come inside
gobj_start(priv->gobj_uart);    // opened again 100 ms later (at the next tick)
```

A stop does not wait for the writes. The write in flight is canceled, the
output it left in the tty is discarded (`tcflush(TCOFLUSH)`), and the writes
queued behind it are dropped. So a write that the line does not take (its
output held by flow control, or by a peer that does not read) never holds a
stop. A drop and a disconnection cut the writes the same way.

`EV_STOPPED` is the last thing it does with itself: a host may destroy it
there. A host that destroys it in the `EV_DISCONNECTED` of a stop is wrong
(the kernel says so: *"Destroying a gobj while its tree is publishing"*),
and the `C_UART` touches nothing of itself after that publication.
A stopped `C_UART` starts again with `gobj_start()`, and opens its device
anew.

```C
gobj_stop(priv->gobj_uart);     // EV_DISCONNECTED and EV_STOPPED come inside

PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    KW_DECREF(kw)
    return 0;
}
```

Up to 7.29.3 a stop stopped the read and nothing else. Stopped inside its own
`EV_RX_DATA` it kept its tty open (the next start overwrote the fd, a leak,
and its `EV_CONNECTED` reached a host that had never been told of the
disconnection); stopped with the read in flight it stayed in
`ST_WAIT_STOPPED` for ever, and the next start never opened the device;
`EV_STOPPED` was never published. And in both cases the callback of the
stopped `C_UART` answered -1 to the event loop, which **ended the loop, and
the yuno with it**. A `C_UART` destroyed with a write in flight was used
after it was freed, when the write completed; one destroyed in
`ST_WAIT_STOPPED` left its tty open. A stop with the read in flight and a
start in the same turn came back connected, with an error (*"Initial wrong
task state"*); the first fixes after 7.29.3 refused that start, and the line
stayed down for ever, until `ST_WAIT_STOPPED` was taken out.
`tests/c/c_prot_modbus_m` (`test4`, a Modbus master over a pty, stopped and
started in one turn; `test5`, a `C_UART` hosted by the test over a pty).

### Trace levels

`connections` logs each open and close of the device, `traffic` dumps what it
receives (`⏪`, with its `url`):

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=C_UART set=1 level=traffic'
```

Up to 7.29.3 both levels were read from the GLOBAL trace mask, where the bit
of a gclass level is never set: `set-gclass-trace` on `C_UART` showed nothing.
And under a deep trace the dump asked two attributes `C_UART` does not have:
each read logged two warnings (*"GClass Attribute NOT FOUND"*).
