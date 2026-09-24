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
| **States** | `ST_STOPPED`, `ST_WAIT_STOPPED` (a stop waiting for its sends), `ST_IDLE` |
| **Input events** | `EV_TX_DATA` (in `ST_IDLE`) |
| **Output events** | `EV_RX_DATA` (the gbuffer carries the peer address) |

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

A datagram that cannot be sent -- a gbuffer with no peer address, or no
memory to keep the submission -- is DROPPED with an ERROR (*"Cannot send
datagram: dropped"*, after the cause logged by `yev_start_event()`), and the
next one in the queue is sent; nothing leaks, and a stop does not wait for
it in `ST_WAIT_STOPPED`. Up to 7.25.4 only the FIRST datagram of a queue was
sent: the completion asked for a state `C_UDP_S` does not have
(`ST_CONNECTED`) before sending the next one. `tests/c/c_udp_s_tx`.

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | Listening URL: `udp://0.0.0.0:5000`, or an IPv6 literal in brackets, `udp://[::1]:5000` (since 7.25.5: an IPv6 peer is kept with its real length and answered; up to 7.25.4 its address was cut to 16 bytes and the reply refused, `-EINVAL`). |
| `shared` | `bool` | Enable port sharing. |
| `set_broadcast` | `bool` | Enable broadcast. |
| `only_allowed_ips` | `bool` | Restrict to allowed IPs only. |
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
