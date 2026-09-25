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
| `timeout_inactivity` | `integer` | Inactivity timeout in seconds (`-1` = no timeout). |
| `timeout_between_connections` | `integer` | Idle delay between reconnection attempts, in milliseconds (default `2000`). |
| `timeout_between_connections_max` | `integer` | If `> timeout_between_connections`, the reconnect delay backs off exponentially from the base up to this cap (ms), resetting to base once a connection is established (for a TLS client, only on a successful handshake). `0` (default) = disabled, legacy fixed interval. A peer that keeps failing no longer hammers at the base cadence. |
| `txBytes` | `integer` | Total bytes transmitted (stat). |
| `rxBytes` | `integer` | Total bytes received (stat). |
| `peername` | `string` | Remote peer address (read-only). |
| `sockname` | `string` | Local socket address (read-only). |

### Usage

Typically used as the bottom gobj of a protocol GClass (C_PROT_TCP4H,
C_WEBSOCKET and more.) or directly inside a C_CHANNEL.

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

A stopped client starts again with `gobj_start()`: a new connect event, a new
socket. Up to 7.25.4 the stop of a client already disconnected published
nothing and kept its connect event: the host waited for an `EV_STOPPED` that
never came, and the next `gobj_start()` failed (*"yev_connect ALREADY
exists"*), so the client never connected again. `tests/c/c_tcp`
(`test_tcp_test6`).

### A write that does not start

`EV_TX_DATA` sends one gbuffer at a time; the next ones wait in a queue.
A write that the loop refuses to start -- no memory to keep its submission,
an empty gbuffer, a socket without fd (the cause is logged by
`yev_start_event()`) -- will never complete, and its bytes are not sent. A
stream cannot go on with a hole in it, so `C_TCP` destroys the write and
DROPS the connection, with an ERROR *"Cannot start a write: the connection
is dropped"*: `EV_DISCONNECTED` follows, and a client reconnects as after any
disconnection. The same applies to the TLS writes and to the rest of a
partial write.

```C
gbuffer_t *gbuf = gbuffer_create(16, 16);      // EMPTY: the write does not start
gobj_send_event(gobj_tcp, EV_TX_DATA,
    json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),   // the kw owns the gbuffer
    gobj
);
// ERROR "Cannot start event: gbuffer WITHOUT data to write" (yev_loop),
// ERROR "Cannot start a write: the connection is dropped", then EV_DISCONNECTED
```

Up to 7.25.4 the `-1` of the start was ignored: the write was counted in
progress for ever, the event and its gbuffer leaked, the connection stayed
up with every later write stuck behind it, and a stop waited in
`ST_WAIT_STOPPED` for ever. `tests/c/c_tcp` (`test_tcp_test5`).

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
| `url` | `string` | Listening URL (for example `tcp://0.0.0.0:1234`). |
| `backlog` | `integer` | Listen backlog. |
| `shared` | `bool` | Enable port sharing (`SO_REUSEPORT`). |
| `crypto` | `json` | TLS configuration for accepted connections. |
| `only_allowed_ips` | `bool` | Accept only the peers in the yuno's `allowed_ips` list (whitelist mode). The `denied_ips` list applies with or without it. |
| `connxs` | `integer` | Current connection count (stat). |
| `tconnxs` | `integer` | Total connection count (stat). |
| `clisrv_kw` | `json` | Extra kw passed to each child client/server. |

(tcp_s_ip_lists)=
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

A refused connection is closed at once and logged at info level, with the
peer, as `TCP_S: Ip denied` or `TCP_S: Ip not allowed` (msgset
`Connect Disconnect`). It does not use a channel of the pool.

Example: ban one ip on every TCP listener of a yuno, and see the list:

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=add-denied-ip ip=203.0.113.7 denied=1'
ycommand -c 'command-yuno id=<id> service=__yuno__ command=list-denied-ips'
```

`denied` is required: without it the command answers -1, *"<role^name>:
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
| `help` | Show available commands. |
| `view-services` | List connected client channels. |

---

(gclass-c-udp)=
## C_UDP

UDP client transport — send and receive datagrams.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_DISCONNECTED`, `ST_CONNECTED` |
| **Input events** | `EV_CONNECT`, `EV_TX_DATA`, `EV_DROP`, `EV_TIMEOUT` |
| **Output events** | `EV_CONNECTED`, `EV_DISCONNECTED`, `EV_RX_DATA`, `EV_TX_READY` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | Connection URL. |
| `rx_buffer_size` | `integer` | Receive buffer size. |
| `txBytes` / `rxBytes` | `integer` | Byte counters (stats). |
| `txMsgs` / `rxMsgs` | `integer` | Message counters (stats). |

---

(gclass-c-udp-s)=
## C_UDP_S

UDP server — listens for datagrams on a configured URL, and sends datagrams
to the peer each gbuffer names.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_WAIT_STOPPED` (a stop waiting for its read to be canceled and its sends to complete), `ST_IDLE` |
| **Input events** | `EV_TX_DATA` (in `ST_IDLE`) |
| **Output events** | `EV_RX_DATA` (the gbuffer carries the peer address, and the peer as its label), `EV_STOPPED` (the stop is done). `EV_TX_READY` is declared and never published. |

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
listening"*. The only other stop is a read that has no memory for its NEW
gbuffer (see *Transmit*: the host kept the previous one): an ERROR, *"UDP: no
memory for the next read, the server stops listening"*, with the
`rx_buffer_size` it asked for.

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
server is in `ST_STOPPED` and publishes `EV_STOPPED`.

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
| `url` | `string` | Listening URL: `udp://0.0.0.0:5000`, or an IPv6 literal in brackets, `udp://[::1]:5000` (since 7.25.5: an IPv6 peer is kept with its real length and answered; up to 7.25.4 its address was cut to 16 bytes and the reply refused, `-EINVAL`). |
| `shared` | `bool` | Enable port sharing. |
| `set_broadcast` | `bool` | Enable broadcast. |
| `only_allowed_ips` | `bool` | Hear only the peers in the yuno's `allowed_ips` (see *Receive*; `denied_ips` applies with or without it); writable, it takes effect at the next datagram. |
| `rx_buffer_size` | `integer` | Receive buffer size: the gbuffer of each read (a new one when the host kept the previous one, see *Transmit*). |
| `txMsgs` / `rxMsgs` / `txBytes` / `rxBytes` | `integer` | Counters (stats). |
| `rxRefusedMsgs` | `integer` | Datagrams dropped by the ip lists (stats, see *Receive*). |

---

(gclass-c-uart)=
## C_UART

Serial port (UART/TTY) transport.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_DISCONNECTED`, `ST_WAIT_CONNECTED`, `ST_CONNECTED` |
| **Input events** | `EV_CONNECT`, `EV_TX_DATA`, `EV_DROP`, `EV_TIMEOUT` |
| **Output events** | `EV_CONNECTED`, `EV_DISCONNECTED`, `EV_RX_DATA` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | Device path (for example `/dev/ttyUSB0`). |
| `baudrate` | `integer` | Baud rate (default `115200`). |
| `bytesize` | `integer` | Data bits (`5`–`8`, default `8`). |
| `parity` | `string` | Parity: `"N"`, `"E"`, `"O"`. |
| `stopbits` | `integer` | Stop bits (`1` or `2`). |
| `xonxoff` | `bool` | Software flow control. |
| `rtscts` | `bool` | Hardware flow control. |
