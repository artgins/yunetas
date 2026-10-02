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
| `connect_on_start` | `bool` | A client connects when it starts (default `true`). `false`: it stays in `ST_DISCONNECTED` until its owner sends `EV_CONNECT`. Only that FIRST connection waits for the owner: once a connection ends (an error, a drop by the peer) the client connects again after `timeout_between_connections`, as any client does, unless that is `-1` (no timer: every connection waits for an `EV_CONNECT` of the owner) -- `json_pack("{s:s, s:b, s:i}", "url", url, "connect_on_start", 0, "timeout_between_connections", -1)`. `C_SMTP_SESSION` starts its transport this way, so a yuno with nothing to send logs in to nobody (added after 7.25.20). Example in a gclass that builds its client: `json_pack("{s:s, s:b}", "url", url, "connect_on_start", 0)` |
| `disconnect_cause` | `string` | Why the last connection, or the last attempt to connect, ended: the error of the socket (`Connection refused`), a TLS failure with the backend's reason when it has one (`TLS handshake failed: SSL - An invalid SSL record was received`, `TLS: decrypt failed: ...`; up to 7.25.21 the mbedTLS backend gave none), `Local dropping`, `Inactivity timeout`, `Local stop`, a write or a connect that could not start. The FIRST cause of an end is kept: the cancel of the read that a drop or an inactivity close makes (`Operation canceled`) does not replace it. Emptied when a connection attempt starts (`EV_CONNECT`, or the reconnect timer, both through `ac_connect`) and when a connection begins (a clisrv gets no `EV_CONNECT`). A cause longer than 512 bytes is truncated with a warning. The `Disconnected` trace says it as its `cause`. A failed attempt publishes no event, only state changes: an owner that wants the cause reads this attr when it sees the change out of `ST_WAIT_CONNECTED` / `ST_WAIT_HANDSHAKE` -- `gobj_read_str_attr(tcp, "disconnect_cause")` -- rather than `gobj_log_last_message()`, which is the last ERROR of the whole process (added after 7.25.20). |
| `timeout_stop_tx` | `integer` | A stop (or a drop) waits for the write in flight, which ends when the peer takes its data. A peer that never does (it does not read, its window stays at zero) held the stop for ever -- a clisrv in `ST_WAIT_STOPPED`, and its `C_TCP_S`, waiting for it, never listening again (up to 7.25.21). Now the socket gets a `TCP_USER_TIMEOUT` of this many ms when the stop has to wait for a write: data not acknowledged for that long aborts the connection, the write ends with an error (`disconnect_cause`), and the stop ends. Default `10000`; `0`: no bound. A peer that reads, however slowly, is not affected. A server sets it on its clisrvs with `clisrv_kw`: `json_pack("{s:s, s:{s:i}}", "url", url, "clisrv_kw", "timeout_stop_tx", 1000)`. |
| `txBytes` | `integer` | Total bytes transmitted (stat). |
| `rxBytes` | `integer` | Total bytes received (stat). |
| `max_tx_in_progress` | `integer` | Most writes ever in flight at once on the connection (stat, since 7.25.8). `1` is the rule, see *One write in flight*; `0` before the first write. |
| `peername` | `string` | Remote peer address (read-only). |
| `sockname` | `string` | Local socket address (read-only). |
| `tcp_s` | `pointer` | A clisrv only: the `C_TCP_S` whose connection it holds, written by that server at the accept (legacy method) or at its start (new method). |

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
DEFINED in state"*, one ERROR per message -- ten thousand in an hour on a sim
of 300 controllers whose central went away. Up to 7.25.20 the data then went
with no trace at the default levels (only the `connections` trace said it,
per message). `tests/c/c_tcp` (`test_tcp_test7`, also a `C_TCP` stopped and
destroyed in `ST_WAIT_STOPPED`).

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
window when a gate comes back -- and how often depends on the kernel: in
yunovatios' stress test on a Rocky 9 central (kernel 5.14) it dropped links
13606 times in 25 minutes. Now the encrypted output waits while a write is in
flight, and the next clear message is taken only when nothing is waiting.

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
channels and 1000 simulated controllers, which retry, made 38,156 of them in
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
| `url` | `string` | Listening URL: `udp://0.0.0.0:5000`, or an IPv6 literal in brackets, `udp://[::1]:5000` (since 7.25.5: an IPv6 peer is kept with its real length and answered; up to 7.25.4 its address was cut to 16 bytes and the reply refused, `-EINVAL`). A secure url (`udps://`) is REFUSED at the start: TLS over datagrams is DTLS, which ytls does not implement. An ERROR, *"A secure url (udps://) is not supported by C_UDP_S: there is no DTLS"*, and the start fails (with `exitOnError`, the default, the yuno exits). Up to 7.25.4 it was accepted, the server listened with no TLS session, and the first datagram crashed the yuno. |
| `use_ssl` | `bool` | Always `false` (see `url`). The commands `reload-certs` and `view-cert` answer -1, *"Listener is not TLS-enabled or not running"*. |
| `shared` | `bool` | Enable port sharing. |
| `set_broadcast` | `bool` | Enable broadcast. |
| `only_allowed_ips` | `bool` | Hear only the peers in the yuno's `allowed_ips` (see *Receive*; `denied_ips` applies with or without it); writable, it takes effect at the next datagram. |
| `rx_buffer_size` | `integer` | Receive buffer size: the gbuffer of each read (a new one when the host kept the previous one, see *Transmit*). |
| `txMsgs` / `rxMsgs` / `txBytes` / `rxBytes` | `integer` | Counters (stats). |
| `rxRefusedMsgs` | `integer` | Datagrams dropped by the ip lists (stats, see *Receive*). |

### Commands

The same three as `C_TCP_S`: `help` (since 7.25.17), `reload-certs` and
`view-cert` -- the last two answer -1 here, a `C_UDP_S` has no TLS.

```bash
ycommand -c 'command-yuno id=<yuno> service=<udp server service> command=help'
```

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
