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
| **Output events** | `EV_CONNECTED`, `EV_DISCONNECTED`, `EV_RX_DATA`, `EV_TX_READY` |

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

1. A loopback peer (`127.0.0.x`, `::1`) is always accepted. A list cannot
   lock out the local control plane.
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

`denied=1` is necessary: `add-denied-ip ip=X` alone writes `X: false`, which
does not deny. The list is persistent (`SDF_PERSIST`). `remove-denied-ip
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

1. A loopback peer (`127.0.0.x`, `::1`) is always heard.
2. A peer in `denied_ips` is dropped, with or without `only_allowed_ips`, and
   it wins over `allowed_ips`: a WARNING, *"UDP_S: Ip denied, datagram
   dropped"*.
3. With `only_allowed_ips`, a peer that is not in `allowed_ips` is dropped: a
   WARNING, *"UDP_S: Ip not allowed, datagram dropped"*.

Each warning names the `peername` and the length. Up to 7.25.4
`only_allowed_ips` was documented and never read, and the deny-list was not
asked: every peer was heard.

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
listening"*. It is the only failure that stops it.

### Transmit

`EV_TX_DATA` carries a gbuffer whose peer address was set with
`gbuffer_setaddr()` -- the gbuffer of an `EV_RX_DATA` has it already, so an
answer goes back to the sender. One datagram is sent at a time; the others
wait in a queue, in order, and each completion sends the next:

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
| `rx_buffer_size` | `integer` | Receive buffer size. |

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
