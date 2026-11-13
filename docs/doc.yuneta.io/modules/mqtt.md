(module-mqtt)=
# MQTT

**Kconfig:** `CONFIG_MODULE_MQTT` — **GClasses:** `C_PROT_MQTT2`, `C_MQTT_BROKER`

Full MQTT protocol implementation (v3.1.1 + v5.0) with a persistent message
broker backed by TreeDB.

## C_PROT_MQTT2

MQTT protocol with publish, subscribe, QoS levels, and event-driven
message handling. `C_PROT_MQTT2` is the current implementation. It works as
the broker's per-connection protocol, as the input of a gate with no broker
(see [`C_PROT_MQTT2` as the input of a gate](#mqtt-prot2-gate)), and as a
standalone client (see [`mqtt_tui`](../utilities/mqtt_tui.md)). The deprecated
`C_PROT_MQTT` was removed after 7.30.26 (see [moving a gate from
`C_PROT_MQTT`](#mqtt-moving-from-prot-mqtt)).

**As a client** (`iamServer` false):

- **Without queues** (`tranger_queues` NULL) the client takes QoS 0 only:
  `max_qos` goes to 0 at the CONNACK, BEFORE `EV_ON_OPEN`, so a QoS 1 or 2
  publish of its user is refused (`-1`, ERROR *"Mqtt publish: qos not
  supported"*), in `EV_ON_OPEN` too; a SUBSCRIBE asks QoS 0 whatever the user
  asks; and a QoS 2 PUBLISH of the broker disconnects it (DISCONNECT `0x93`,
  a WARNING). Up to 7.30.26 a QoS 1 publish in `EV_ON_OPEN` went to the NULL
  queue and crashed the yuno, a SUBSCRIBE asked the user's QoS, and the QoS 2
  PUBLISH released its payload twice.
- **With queues** they are opened BEFORE `EV_ON_OPEN` is published, so its
  user may publish in any QoS there (up to 7.30.26 they opened after it: the
  NULL queue, a crash). Its close deletes their topics only when its
  `clean_start` attribute is set (the client does not set it itself).
- **`max_packet_size`** bounds what the broker may send: a larger packet is
  refused at its header (WARNING *"Incoming packet exceeds max_packet_size"*,
  DISCONNECT `0x95` Packet too large; `0x82` up to 7.31.1), and an MQTT 5
  CONNECT announces it (Maximum Packet Size). The size is the whole packet,
  as the server counts it (see the table below). Up to 7.30.26 it was the
  server's only, and a client kept what its broker announced, up to 256 MB.
- **What a broker may send it**, checked as the server checks a client (each
  one a WARNING and the end of the connection; up to 7.30.26 each one was let
  through):
    - a second CONNACK in the session: a protocol error, DISCONNECT `0x82`
      (MQTT 5; in 3.1.1 a DISCONNECT has no reason code), then the close. The queues are not opened again and
      the user does not hear `EV_ON_OPEN` again;
    - a PUBLISH of QoS 3, of QoS 0 with DUP set, or whose topic has a
      wildcard (`a/#`, `a/+/b`) or is empty: malformed, DISCONNECT `0x81`
      (MQTT 5). The number of levels is not checked: MQTT puts no limit on
      it, so a topic of 250 levels (`l/l/.../l`) from a broker is delivered
      (the 200 separators of the server are its own limit, below), and the
      user can publish one (see `EV_SEND_MESSAGE` below);
    - a DISCONNECT of the broker is its last packet: nothing after it in the
      same read is processed (a PUBLISH behind it is not delivered), and its
      properties are freed (up to 7.30.26 they leaked).
- **A refused CONNECT.** A CONNACK with any reason code but `0` ends the
  connection, and the user hears `EV_ON_CLOSE` with `reason_code` in its kw,
  after ONE WARNING:

    | Protocol | Code | The WARNING | What the client does |
    |---|---|---|---|
    | MQTT 3.1.1 | `1`..`5`, and a reserved one past `5` | *"Mqtt connection refused"* | a refusal: it sends nothing (the broker closes) |
    | MQTT 5 | `>= 0x80` | *"Mqtt connection refused"* | a refusal: it sends nothing |
    | MQTT 5, a CONNACK in the form of 3.1.1 (no properties) | `1`..`5` | *"Mqtt connection refused"*, `connack` `MQTT 3.1.1` | a refusal by a 3.1.1 broker: it sends nothing, and `reason_code` is the MQTT 5 code of the same meaning: `1` -> `0x84`, `2` -> `0x85`, `3` -> `0x88`, `4` -> `0x86`, `5` -> `0x87` |
    | MQTT 5, a CONNACK in the form of 3.1.1 (no properties) | `0`, and past `5` | *"Mqtt protocol error: CONNACK of MQTT 3.1.1 to an MQTT 5 client"*, with `reason_code` | not a CONNACK of MQTT 5, and not a refusal of 3.1.1: it sends nothing (the broker reads 3.1.1), and `reason_code` is `0x81` (Malformed Packet: the properties are missing). Not dumped |
    | MQTT 5 | `1`..`0x7F` | *"Mqtt protocol error: CONNACK reason code not defined in MQTT 5"*, with `reason_code` | a protocol error (MQTT 5 defines no code below `0x80` but Success; `1`..`5` are codes of 3.1.1): DISCONNECT `0x82`. The CONNACK is not dumped: it is not malformed |

  The `reason` of each WARNING is a text. Up to 7.30.26 an MQTT 5 refusal was
  taken for a protocol error: a DISCONNECT `0x82` to the broker, a dump of
  the CONNACK, and no `EV_ON_CLOSE`; a 3.1.1 code past `5`, and an MQTT 5 code
  from `0x06` to `0x7F`, ended the connection with the WARNING and no
  `EV_ON_CLOSE`; a CONNACK in the form of 3.1.1 to an MQTT 5 client was
  *"Error in properties"*, DISCONNECT `0x81` and a dump, with no
  `EV_ON_CLOSE`; and an MQTT 5 CONNACK with a code from `1` to `5` was taken
  for a refusal, its `reason` the text of the MQTT 5 code of that number
  (*"Granted QoS 2"* for `2`), with no DISCONNECT. A CONNACK of MQTT 5 with
  the code `0x06`, one of a 3.1.1 broker that refuses an MQTT 5 client's
  user, and one of 3.1.1 with the code `0`, as the user sees them:

    ```text
    broker:  20 03 00 06 00        CONNACK, reason code 0x06
    user:    EV_ON_CLOSE           {"reason_code": 6, "protocol_version": 5, ...}
    client:  DISCONNECT 0x82       Protocol error, to the broker

    broker:  20 02 00 04           CONNACK of 3.1.1, 4: bad user name or password
    user:    EV_ON_CLOSE           {"reason_code": 134, "protocol_version": 5, ...}

    broker:  20 02 00 00           CONNACK of 3.1.1, 0: accepted, by a broker of 3.1.1
    user:    EV_ON_CLOSE           {"reason_code": 129, "protocol_version": 5, ...}
    ```

```C
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    int reason_code = (int)kw_get_int(gobj, kw, "reason_code", 0, 0);
    if(reason_code != 0) {
        // the broker refused the CONNECT. MQTT 5: 0x86 bad user or password,
        // 0x87 not authorized, ...; MQTT 3.1.1: 4 bad user or password,
        // 5 not authorized, a reserved code past 5
    }
    KW_DECREF(kw)
    return 0;
}
```

```json
{
    "gclass": "C_PROT_MQTT2",
    "kw": {
        "iamServer": false,
        "mqtt_client_id": "dev-17",
        "max_packet_size": 66560
    }
}
```

**As a server** (`iamServer` true), **the close of a connection deletes its
queue topics only when its session ends with it:** in MQTT 5 when its Session
Expiry Interval is 0 at the close (the one of the CONNECT, or the one of its
DISCONNECT [MQTT-3.14.2.2.2]), in MQTT 3.1.1 when it came with a clean
session. Clean
Start (MQTT 5) cleans at the CONNECT only [MQTT-3.1.2-4]: a session with
Clean Start 1 and Session Expiry 300 keeps its messages across the close, and
the broker deletes them when the session expires. A connection taken over is
told by the broker's `EV_DROP` (see below), and its close deletes nothing.

```text
A  CONNECT x (MQTT 5, Clean Start 1, Session Expiry 300)     CONNACK 0
A  SUBSCRIBE q/# QoS 1; 25 QoS 1 to q/x, not acked           20 in flight, 5 queued
A  DISCONNECT, closes                                        the session and its queues kept
C  5 QoS 1 to q/x                                            queued for x
B  CONNECT x (MQTT 5, Clean Start 0, Session Expiry 300)     CONNACK 1, the 30 messages
```

Up to 7.30.26 the close deleted the topics whenever the connection had come
with Clean Start 1: B got 5 of the 30, against [MQTT-4.4.0-1]. And when B came
while A was still connected (or still closing), the close of A deleted the
topics B had open: the queue of B then read a freed topic at each backup
check, every second (the ERROR *"json refcount 0"*, or a crash with messages
queued and acknowledged).

**After the client's DISCONNECT the connection ends within `timeout_close`.**
The DISCONNECT is the last packet of the client (MQTT 5 §3.14.4): the server
reads nothing after it, and lets the client close, as it MUST
[MQTT-3.14.4-1]. A client that has not closed after `timeout_close` is closed
by the server, with no DISCONNECT of its own and ONE WARNING *"Client did not
close after its DISCONNECT, closed by the server"*; its keepalive is no longer
checked. The server does not close first because every TCP socket of Yuneta
closes with `SO_LINGER` 0, a RST: a client that half-closes after its
DISCONNECT (HiveMQ's `shutdownOutput()`) then gets ENOTCONN. The broker
applies what the DISCONNECT says at the close: its Session Expiry Interval,
and its will, published only with `0x04` (Disconnect with Will Message) and
discarded with any other reason or in MQTT 3.1.1 [MQTT-3.1.2-10]. A client
that reconnects before its old connection closed finds it applied all the
same: the take-over reads the Session Expiry and the will of the open
connection, not of the session record:

```text
A  CONNECT x (MQTT 5, Session Expiry 60, will on w/x)          CONNACK 0
A  DISCONNECT 0x00, Session Expiry 0, closes;  CONNECT x at once   CONNACK 0, session present 0, no will
B  CONNECT y (will on w/y);  DISCONNECT 0x04, stays open        after timeout_close: WARNING, closed,
                                                                the will on w/y published
```

Up to 7.31.1 the server waited for the client for ever: a client that
reconnected at once found the session it had just expired, a DISCONNECT `0x04`
with the socket open never had its will published, and a normal DISCONNECT did
not discard the will of the session, which was published at the close all
the same.

**An outgoing PUBLISH over the Maximum Packet Size the client announced** is
not sent [MQTT-3.1.2-25] and leaves the queue, with a WARNING *"Dropping too
large outgoing PUBLISH"* (`client_id`, `topic`, `packetlen`,
`maximum_packet_size`). Up to 7.31.1 it was an ERROR, and the message stayed
queued and was tried again, with a new packet id each time.

**An MQTT 5 PUBACK, PUBREC, PUBREL or PUBCOMP of 3 or 4 bytes** (the packet id
and a reason code, with or without an empty Property Length) is accepted
(§3.4.2.1: the reason code and the properties may be omitted). Up to 7.31.1 it
was refused, *"packet too long"* and DISCONNECT `0x81`: Mosquitto's checks of
the whole Remaining Length were applied to what was left after the packet id.

**Trace levels (`C_PROT_MQTT2`):** `traffic` (packets, no payload),
`traffic-payload`, `show-decode` (decoded packet structure), `messages2`.

**Secrets:** `user_passw`, `jwt` and `auth_data` (the MQTT 5 AUTH data of the
connection) are `SDF_SECRET`: `view-attrs`, `view-gobj` and
`list-persistent-attrs` show them as `********` (an empty one stays empty).
Up to 7.25.20 `auth_data` showed in clear.

A frame the protocol refuses is dumped in the log, at every trace level
(*"MQTT malformed packet: <command>"*, length-capped). Of a CONNECT only the
head is dumped, the protocol name, level, flags and keep alive, and of an AUTH
only its reason code: the properties and the payload carry the credentials.
Up to 7.25.20 the whole CONNECT was dumped, password included. A CONNECT
refused by policy is not malformed and is not dumped: its CONNACK says why
(identifier rejected, not authorized, protocol version, …) and a WARNING
names the cause. Up to 7.30.26 it was dumped as well.

**A property that MQTT 5 does not define** (its identifier is none of the
27 of MQTT 5) makes the packet malformed, from any peer and before its
authentication: ONE WARNING *"Mqtt unknown property"* (`MSGSET_MQTT`, with the
`identifier` and the `peername`, no stack), the capped dump of the packet, and
DISCONNECT `0x81`. Up to 7.30.26 each such packet logged two ERRORs with no
peer (*"Mqtt unknown property"*, *"Mqtt empty property"*) and a WARNING
*"Mqtt Unsupported property type"*. A PUBLISH with the property 127:

```text
client:  30 09 00 03 74 2F 38 02 7F 00 7A   PUBLISH t/8, properties [127: 0]
server:  WARNING "Mqtt unknown property" {"identifier": 127, "peername": "..."}
         DISCONNECT 0x81, the close
```

A property of MQTT 5 in a packet that may not carry it (a Payload Format
Indicator in a CONNECT) is one WARNING *"Mqtt invalid property of command"*,
whose `command` is the packet's name; up to 7.30.26 it was printed as a
number (a string through `%d`), in all eleven sites:

```text
client:  CONNECT (MQTT 5) with the property 0x01
server:  WARNING "Mqtt invalid property of command" {"command": "CMD_CONNECT", "identifier": 1, "peername": "..."}
```

```text
"msg": "MQTT malformed packet: CMD_CONNECT (credentials not dumped)",
"data": {"0000": "00 04 4D 51 54 54 05 C2 00 3C     ..MQTT...<"}
```

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=view-attrs gobj=<C_PROT_MQTT2 full name> attribute=auth_data'
# {"C_PROT_MQTT2^<name>": "********"}
```

(mqtt-prot2-gate)=
### `C_PROT_MQTT2` as the input of a gate

A gate takes what its devices publish and sends it up. It is not a broker: it
keeps no session from one connection to the next, and it forwards nothing from
one client to another. After 7.30.25 `C_PROT_MQTT2` serves this case without
`C_MQTT_BROKER`. Its parent (the decoder of the gate) is its upper level, and
the protocol has no queues (`tranger_queues` NULL). Put it in the gclass tree
of the gate's channel:

```json
{
    "name": "input-(^^__range__^^)",
    "gclass": "C_PROT_MQTT2",
    "kw": {
        "iamServer": true,
        "allow_anonymous": true,
        "message_size_limit": 65536,
        "max_packet_size": 66560
    },
    "children": [
        {
            "name": "input-(^^__range__^^)",
            "gclass": "C_TCP"
        }
    ]
}
```

| Attribute | Default | What it does in a gate |
|---|---|---|
| `allow_anonymous` | `false` | `true`: every CONNECT is accepted without asking `C_AUTHZ`. A username and a password, if the device sends them, are not checked. With `false` the CONNECT is authenticated against the `treedb_name` service (the broker's), which a gate does not have: every device gets CONNACK `5` / `0x87`. |
| `max_subscriptions_per_client` | `100` | Topic filters one connection holds. Each new filter past it gets `0x80` (MQTT 3.1.1) or `0x97` Quota exceeded (MQTT 5) in the SUBACK, and the client stays. One WARNING per connection. |
| `max_topic_filter_length` | `256` | A filter longer than this, in bytes, gets `0x80` or `0x8F` Topic Filter invalid in the SUBACK. One WARNING per connection. An UNSUBSCRIBE is never refused by it. |
| `max_inflight_messages` | `20` | QoS 2 packet ids that wait for their PUBREL. One more is a protocol error of the client: it is disconnected, with a WARNING. MQTT 5 clients are told it in the CONNACK (Receive Maximum). |
| `message_size_limit` | `0` | A PUBLISH with a larger payload is not delivered. Its QoS flow is completed (MQTT 5: PUBACK or PUBREC `0x95`), and the client stays. |
| `max_packet_size` | `0` | A packet larger than this is refused when its header arrives, before its bytes are kept, and the client is disconnected: an MQTT 5 one gets DISCONNECT `0x95` Packet too large. The size is the WHOLE packet, as MQTT 5 counts the Maximum Packet Size: the fixed header byte, the Remaining Length field and the rest (with `50`, a PUBLISH of 51 bytes is refused). Up to 7.31.1 the Remaining Length alone was compared, so 2 to 5 bytes more got in, and the DISCONNECT was `0x82`. Without it a packet is kept whole (up to 256 MB) before `message_size_limit` is checked, but a CONNECT, which comes before the peer is authenticated, is still refused over 64 KB (up to 7.31.1 over 1000 bytes in every case, which refused an MQTT 5 CONNECT with properties or a token as its password). It also bounds how many filters one (UN)SUBSCRIBE carries to the upper level (see below). |
| `max_qos` | `2` | `0` or `1`: a PUBLISH of a higher QoS disconnects the client. MQTT 5 clients are told it in the CONNACK (Maximum QoS). Up to 7.30.26 that property was not sent, and each CONNECT logged an ERROR. |
| `timeout_close` | `3000` | Milliseconds a connection the server closes waits: in MQTT 5, after its DISCONNECT, for the client to close; in MQTT 3.1.1, for its answers to be written. Then the transport is dropped, with a WARNING *"Timeout waiting mqtt disconnected"*; a transport whose write in flight the peer does not take waits `timeout_stop_tx` of `C_TCP` (10 s) more, and aborts with its own WARNING (see below). |
| `max_unwritten_bytes` | `100000` | Memory that the protocol's answers (PINGRESP, PUBACK, PUBREC, PUBREL, PUBCOMP, SUBACK, UNSUBACK...) may hold in the send queue of the transport before the reading of the peer is paused. Each answer counts its size plus about 300 bytes of buffer. A peer that reads is slowed to what it takes, however much it pipelines; one that takes nothing of what waits for it during `timeout_close` is dropped, with one WARNING (see below). The messages a broker forwards to a subscriber do not count. `0`: no limit. |

The two subscription limits count the filters of the CONNECTION: the
subscriptions of a session that a broker restores are not counted.

**Without queues:**

- A QoS 0 or 1 PUBLISH is delivered to the upper level, and a QoS 1 one gets
  its PUBACK.
- A QoS 2 PUBLISH is delivered when it arrives, and only its packet id waits
  for the PUBREL (the second method of MQTT 3.1.1 4.3.3). A PUBLISH with the
  same id before the PUBREL gets PUBREC again and is not delivered again.
- A message to the client goes in QoS 0, whatever QoS it asks for, and a
  SUBSCRIBE is granted QoS 0: without a queue the server cannot keep a
  message until it is acknowledged.
- An ack of a packet id the server never used (PUBACK, PUBREC, PUBCOMP,
  PUBREL) is the client's: PUBREL gets PUBCOMP, PUBREC gets PUBREL, and a
  WARNING says it, once per connection (up to 7.30.26, once per ack).
- A PUBLISH past `message_size_limit` is dropped (MQTT 5: PUBACK or PUBREC
  `0x95`), and the WARNING *"Mqtt: Dropped too large PUBLISH"* says it once
  per connection, with the limit and the size. Up to 7.30.26 it was one per
  PUBLISH: 16 000 WARNINGs and 11 MB of log for 1 GB sent.

**A PUBLISH whose topic has a wildcard** (`a/+`), is empty or has more than
200 `/` separators (`TOPIC_HIERARCHY_LIMIT`: 201 levels pass, 202 do not) is
malformed: the WARNING *"Mqtt: invalid topic in PUBLISH, disconnecting"*
names the topic, then DISCONNECT `0x81` (MQTT 5). The wildcard is the rule of
MQTT [MQTT-3.3.2-2]; the separators are the limit of this server, as of
Mosquitto (MQTT puts none, and the client does not apply it). Up to 7.30.26
that WARNING said *"Mqtt will: invalid topic"*, as if it were a will.

**A DISCONNECT of the client is its last packet** (MQTT 5 3.14.4): nothing
the client sends after it is processed, and the server waits for the client
to close (the keepalive bounds the wait). Up to 7.30.26 the server went on
reading, and an MQTT 5 DISCONNECT with a reason code and no Session Expiry
Interval (`e0 02 00 00`, what most MQTT 5 clients send) logged an ERROR with a
stack.

**What the upper level answers.** The protocol publishes to its parent, and
reads the return of the publication:

| Event | kw | The upper level |
|---|---|---|
| `EV_ON_OPEN` | the client: `id`, `protocol_version`, `clean_start`, `peername`, the will (its payload as `gbuffer`) | returns `0`: CONNACK accepted, session present 0. Returns `-2`: CONNACK not authorized, and the connection closes. |
| `EV_ON_IEV_MESSAGE` with `__iev_event__` `EV_MQTT_MESSAGE` | `topic`, `qos`, `retain`, `gbuffer` (the payload, peer bytes) | takes the message. Returns `1` when it took it (MQTT 5: PUBACK Success); `0` is "no matching subscribers" (`0x10`). |
| `EV_ON_IEV_MESSAGE` with `__iev_event__` `EV_MQTT_SUBSCRIBE` | `subs` (`sub`, `qos`, …), `gbuffer` | appends ONE reason code per filter of `subs` to the `gbuffer`, in order: the QoS granted, or `>= 0x80` to refuse it. The filters the protocol refused are not in `subs`, and a filter repeated in the packet is in it once. |
| `EV_ON_IEV_MESSAGE` with `__iev_event__` `EV_MQTT_UNSUBSCRIBE` | `subs`, `gbuffer` | appends ONE reason code per filter of `subs`, in order: `0x00` Success, `0x11` No subscription existed, or `>= 0x80` to refuse it. Every distinct filter of the packet is in `subs`, once: the protocol refuses none. MQTT 3.1.1 has no codes in its UNSUBACK, but the upper level writes them all the same. |
| `EV_ON_CLOSE` | `client_id`, `peername` | the connection ended. |

**The reason codes.** The `gbuffer` has room for exactly one code per filter
of `subs`, however many the packet carries. A code that is missing is an
internal error of the upper level: an ERROR *"The upper level did not answer
one reason code per filter"*, and `0x80` for the filters left without one
(the last ones). A code too many does not fit: the `gbuffer` refuses it with
an ERROR. A filter the upper level refuses (`>= 0x80`) is not held by the
connection, so it does not count against `max_subscriptions_per_client`.
Up to 7.30.26 the buffer held 12 KB: a SUBSCRIBE or an UNSUBSCRIBE of more
than 12 287 filters logged two ERRORs with a stack per code past it, and the
UNSUBACK went out with fewer codes than filters.

**How many filters reach the upper level.** Each DISTINCT filter of the
packet, once. A filter the connection already holds is passed up again and
not counted again: MQTT says it replaces the subscription. A filter repeated
in the same SUBSCRIBE (or UNSUBSCRIBE) goes up once: the repetition replaces
the options of the first one (its QoS, its retain handling), and every
occurrence takes the reason code the upper level wrote for that filter, so
the SUBACK (UNSUBACK) still has one code per filter of the packet. The bound
is the packet: with `max_packet_size` 66560, a SUBSCRIBE carries at most
about 16 600 filters (4 bytes each at least) and an UNSUBSCRIBE about 22 000
(3 bytes each). An example, the codes of a SUBSCRIBE of `a/1`, `a/1`,
`deny/x` and `a/2` by an MQTT 5 client, with an upper level that grants in
QoS 0 and refuses `deny/x` with `0x87`:

```text
SUBSCRIBE  a/1 a/1 deny/x a/2  ->  subs: [a/1, deny/x, a/2]  ->  SUBACK 0x00 0x00 0x87 0x00
                                                                (held: a/1, a/2: counted 2)
```

Up to 7.30.26 every occurrence went up, at about 4 µs each: one anonymous
peer sending SUBSCRIBEs of 16 000 copies of `a` at 1 MB/s held a gate at
100 % CPU. Now a repetition, and a filter the protocol refuses, cost one
lookup in a dictionary and nothing more: the reason code of each occurrence
is kept in a C array, not in a json, and the options of a repeated filter are
written once. The CPU of the gate's process per packet, measured over a TCP
connection of the same machine (a client of another process sends 300
packets of each shape, four rounds alternated with 7.30.26; development
machine, with `CONFIG_DEBUG_TRACK_MEMORY`):

| Packet | CPU per packet | 7.30.26 |
|---|---|---|
| SUBSCRIBE of 64 KB, 16 000 copies of `a` | 0.52-0.59 ms | not comparable: ERRORs with a stack for each packet |
| SUBSCRIBE of 64 KB, 7900 distinct filters, 7800 past `max_subscriptions_per_client` | 0.60-0.70 ms | 4.1-4.3 ms |
| UNSUBSCRIBE of 48 KB, 16 000 copies of `a` | 0.43-0.48 ms | not comparable: ERRORs with a stack for each packet |
| UNSUBSCRIBE of 55 KB, 7900 distinct filters | 4.4-6.3 ms | 2.5 ms |

An UNSUBSCRIBE keeps its occurrences the same way, and every distinct filter
goes up, so the distinct ones are slower than in 7.30.26: the price of the
dictionary that sends a repeated filter up once. A gate at 100 % CPU takes
about 9-12 MB/s of such UNSUBSCRIBEs (7.30.26: about 22 MB/s). A slower
machine costs more for each shape.

A minimal upper level, the action of a decoder:

```C
PRIVATE int ac_on_iev_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_event_t iev_event = (gobj_event_t)(uintptr_t)kw_get_int(
        gobj, kw, "__iev_event__", 0, KW_REQUIRED
    );
    int ret = 0;

    if(iev_event == EV_MQTT_MESSAGE) {
        gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
        json_t *jn_msg = gbuf? anystring2json_from_peer(    // peer bytes
            gobj, gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf), gobj
        ) : NULL;
        if(json_is_object(jn_msg)) {
            process_reading(gobj, kw_get_str(gobj, kw, "topic", "", 0), jn_msg);
            ret = 1;
        }
        JSON_DECREF(jn_msg)

    } else if(iev_event == EV_MQTT_SUBSCRIBE || iev_event == EV_MQTT_UNSUBSCRIBE) {
        gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
        json_t *subs = kw_get_list(gobj, kw, "subs", 0, KW_REQUIRED);
        int idx; json_t *jn_sub;
        json_array_foreach(subs, idx, jn_sub) {
            int code = 0;
            if(iev_event == EV_MQTT_SUBSCRIBE) {
                code = (int)kw_get_int(gobj, jn_sub, "qos", 0, KW_REQUIRED);
            }
            gbuffer_append_char(gbuf, (uint8_t)code);
        }
    }

    KW_DECREF(kw)
    return ret;
}
```

Declare `EV_ON_IEV_MESSAGE` in its FSM and in its `event_types` (an input
event, flag `0`), and include `c_prot_mqtt2.h` for the `EV_MQTT_*` names. To
send a message to the device, send `EV_SEND_MESSAGE` to the protocol:

```C
json_t *kw_send = json_pack("{s:s, s:I, s:i, s:b}",
    "topic", "cmnd/dev-17/POWER",
    "gbuffer", (json_int_t)(uintptr_t)str2gbuf("%s", "ON"),
    "qos", 0,
    "retain", 0
);
gobj_send_event(gobj_bottom_gobj(gobj), EV_SEND_MESSAGE, kw_send, gobj);
```

`gobj_send_event()` returns `-1` and the message is not sent when:

- the topic is not one a PUBLISH can carry: it has a wildcard (`+`, `#`) or
  it is empty [MQTT-3.3.2-2]; or, on the SERVER side only, it is past the
  server's own limit: more than 200 `/` separators (`TOPIC_HIERARCHY_LIMIT`,
  as Mosquitto's). An ERROR *"Mqtt publish: topic invalid"* names it. A
  subscription FILTER is not a topic to publish to: up to 7.30.26 a message
  to `cmnd/dev-17/#` was sent as it was. A CLIENT applies no limit of levels,
  by `EV_SEND_MESSAGE` nor by `EV_MQTT_PUBLISH`: MQTT puts none, and its
  broker decides (a topic `l/l/.../l` of 250 levels goes out);
- the connection is closing (its DISCONNECT sent, waiting `timeout_close`): a
  WARNING *"Mqtt: the connection is closing, message not sent"*, once per
  connection. Up to 7.30.26 it was an ERROR *"Event NOT DEFINED in state"*,
  with a stack. The message is not queued either: a broker's QoS 1 or 2
  message for a persistent session that arrives in that window is lost, as it
  was before (then with the ERROR).

**What the protocol does not do, and the gate must:** the same client id on a
second connection is not taken over. A broker drops the first connection; a
gate looks for the decoder of that id in its other channels and sends it
`EV_DROP`. Say that it is a take-over, so an MQTT 5 client is told why
(DISCONNECT `0x8E` Session taken over; without the key it gets `0x00`). A
broker sends the same key, and `"delete_session": true` when the new
connection does not keep the session, which closes the queues of the old one
and deletes their topics, whether the OLD connection kept its session or
not (up to 7.30.26 only when it did not: the new connection, with a clean
session, opened the same topics and got the old session's messages). A
take-over without it closes the queues of the old connection too, and leaves
their topics to the new one: the end of the old connection does not touch
them any more. A gate has no queues and needs only the first:

```C
gobj_send_event(
    gobj_bottom_gobj(gobj_old_decoder),     // its C_PROT_MQTT2
    EV_DROP,
    json_pack("{s:b}", "session_take_over", 1),
    gobj
);
```

In MQTT 3.1.1 a server sends no DISCONNECT, and the old connection is dropped
as soon as what was already sent to it is written. In MQTT 5 it is dropped
after its DISCONNECT, within `timeout_close`;
in that window the connection is closing, and another `EV_DROP` drops it as
soon as what was sent to it is written, at once when nothing waits (up to
7.30.26 it was an ERROR *"Event NOT DEFINED in state"*, with a stack). A decoder that has dropped its connection is no longer the client's:
it must stop taking part in the take-over, or every new connection of that id
sends `EV_DROP` to all the connections still closing.

**A connection the gate cuts gets the answers sent before the cut.** A gate
cuts a connection that sends what is not a device's by posting `EV_DROP` to
its `C_PROT_MQTT2`, from the action that saw it: the protocol answers the
packet it is in first, and the rest of what was read with it (the gate takes
none of it). In MQTT 5 the server then sends DISCONNECT and waits
`timeout_close` for the client to close. In MQTT 3.1.1 it sends no
DISCONNECT: the drop waits for the answers already sent (the `EV_TX_READY` of
its transport), and the connection closes once they are written. The same
holds for every close of a server in session: a protocol error of the client
(a DISCONNECT with flags, a malformed packet), and an `EV_DROP` that comes
while the close waits (a cut of the gate posted before the malformed packet
of the same read). In MQTT 5 the answers go first, then the DISCONNECT `0x82`
of a protocol error, then the close. Nothing read after the close starts is
processed. Three QoS 1 readings and a cut message in one write of a 3.1.1
client:

```text
client:  PUBLISH 1  PUBLISH 2  PUBLISH 3  PUBLISH 4 (the gate cuts it)
server:  PUBACK 1   PUBACK 2   PUBACK 3   PUBACK 4   then the close
```

A peer that does not read waits longer, and is said twice. At `timeout_close`
(3 s) the protocol drops the transport, with the WARNING *"Timeout waiting
mqtt disconnected"*; the `C_TCP` then waits for its write in flight up to its
`timeout_stop_tx` (10 s), and aborts the connection with the WARNING *"The
peer did not take the data in flight in timeout_stop_tx, connection
aborted"*: about 13 s from the cut to the close. Meanwhile the peer may go on
sending; nothing of it is processed. This holds in MQTT 5 too, after its
DISCONNECT.

Up to 7.30.26 the 3.1.1 drop went to the `C_TCP` at once, which discards
what waits behind its write in flight: the client got `PUBACK 1` and the
close, and would send the three readings again (the gate had taken them);
`SUBSCRIBE a/b` and a cut `SUBSCRIBE` in one write got `SUBACK 0x00` and the
close, without the `0x80`. A protocol error of an MQTT 5 client lost its
answers the same way (`PUBACK 1`, then the close), and an MQTT 5 peer that
did not read was dropped at once.

**The answers a peer does not read are bounded: backpressure.** Each packet
a peer sends can queue an answer in the `C_TCP`: a PINGRESP of 2 bytes holds
about 340 bytes of memory, a SUBACK of a SUBSCRIBE of 16000 filters 16 KB. A
read of 4 KB answers 2048 PINGREQ, and the `C_TCP` writes one packet per turn
of the loop, even to a peer that reads.

The protocol counts the memory of the answers it hands to the transport since
the transport's queue was last empty (`EV_TX_READY`); each answer counts its
size plus about 300 bytes. The messages a broker forwards to a subscriber
(PUBLISH) are not answers: they are not counted. Past `max_unwritten_bytes`
(100000), after the packet that passed it:

- **the reading of the peer is paused.** The rest of the read waits in the
  protocol (a copy), and the `C_TCP` reads nothing more (`rx_paused`). The
  peer's own TCP window then stops it. In clear the rest is at most one read
  of the `C_TCP` (`rx_buffer_size`, 4 KB). Over TLS it is clear text: a read
  of 4 KB can complete a record of up to 16 KB (the TLS limit, in OpenSSL and
  in mbedTLS), and the records that the same read completes after it are
  kept too, so the rest is up to about 20 KB. Measured with 10 clients
  pipelining PINGREQ for 15 s: 4094 bytes at most in clear; over TLS 16 376
  with writes of 64 KB (records of 16 KB), and 19 570 with a write of 16 KB
  followed by writes of 200 bytes.
- **The `EV_TX_READY` of the queue written resumes it:** the rest is read
  while the `C_TCP` is still paused, and the `C_TCP` reads again only when all
  of the rest was read with no new pause. If the rest passes the bound again,
  what is left of it waits (shorter each time) for the next `EV_TX_READY`.
  Nothing is added to the rest but the read that was already in flight when
  the reading was first paused (a TLS read can bring several records: that
  is the 20 KB above).
- **`timeout_close` (3 s) at most between two looks:** if the queue is empty,
  the reading resumes; if something of it was written since the last look,
  the protocol waits another `timeout_close`; if nothing was written, the
  peer does not read: the connection is dropped at once, with one WARNING
  *"Mqtt: the peer does not read its answers, connection dropped"* (with
  `client_id`, `peername`, `unwritten_packets`, `answer_bytes` and
  `max_unwritten_bytes`). No DISCONNECT is sent (it would wait behind the
  rest), and the rest of the read is not processed.

So a connection holds about the bound of answers (about 100 KB) and one read
(4 KB in clear, up to about 20 KB over TLS), whether its peer reads or not, whatever the queue of forwarded messages holds,
and the cost on each packet is a counter. 20 peers that send PINGREQ without
end and read every answer hold about 2.6 MB in all, for as long as they go
on. A pool of 1000 anonymous connections that do not read holds about
130 MB, not the 3.7 GB of the bound in packets that came before it. A peer
that reads is never dropped for its answers, however much it pipelines: 3000
QoS 1 PUBLISH or 30000 PINGREQ in one write get every answer, at the pace the
peer takes them. **A peer that does not read is dropped after
`timeout_close` only while the protocol holds answers its transport cannot
write.** Once every answer is in the kernel (the transport's queue empty),
the protocol holds nothing of the connection and reads again: if the peer
then sends nothing more (its own kernel stalled on full buffers, or it
simply stopped), the connection is idle like any other, and only its
keepalive (or the pool's bound, or the peer's close) ends it. A peer whose
kernel still takes a little at each look is not dropped either: it holds the
same bounded memory until one of those ends it.

A client that publishes 3000 QoS 1 in one write, reading:

```text
client:  PUBLISH QoS 1 x 3000, in one write, reads everything
server:  PUBACK 1 .. 325, its reading paused; PUBACK 326 .. 650 when they are
         written; ... PUBACK 3000. No WARNING, not dropped
```

A peer that sends 10000 PINGREQ and never reads (its receive buffer of 4 KB
full):

```text
server:  PINGRESP 1 .. 327, its reading paused; nothing written for timeout_close
         WARNING "Mqtt: the peer does not read its answers, connection dropped"
         {"unwritten_packets": ..., "answer_bytes": 100062, "max_unwritten_bytes": 100000}
```

A bound and a wait of its own, for a gate whose devices are slow to read:

```json
{
    "gclass": "C_PROT_MQTT2",
    "kw": {
        "iamServer": true,
        "allow_anonymous": true,
        "max_unwritten_bytes": 400000,
        "timeout_close": 10000
    }
}
```

After the drop the protocol says nothing more of the connection. The
`C_TCP` keeps its queue until its stop ends: at once if the peer's socket
takes the write in flight. Otherwise the `C_TCP` waits `timeout_stop_tx`
(10 s), aborts, and logs its own WARNING *"The peer did not take the data in
flight in timeout_stop_tx, connection aborted"*. The connection's
`EV_ON_CLOSE` comes then. So a peer that does not read holds about
`max_unwritten_bytes` of answers (plus the messages forwarded to it) for at
most `timeout_close` after its last progress plus `timeout_stop_tx`.

Up to 7.30.26 there was no bound: one anonymous peer grew a gate by about
400 MB per 2 MB of PINGREQ, whether it read or not, until the process died.
The first bound, `max_unwritten_packets` (10000, never released), counted
packets: a peer that sent exactly 10000 PINGREQ was never checked (3.7 MB
held per connection), a SUBACK of 16 KB counted as one packet, and the
messages forwarded to a subscriber that read everything were counted in the
queue (it was dropped). The bound in bytes that came next (never released)
dropped instead of pausing: a reader that pipelines cannot show progress
inside one read, so a client of 1000 QoS 1 in one write was dropped after 4
PUBACKs. And a drop was said three times: its WARNING, then *"Timeout waiting
mqtt disconnected"* at `timeout_close`, then the `C_TCP`'s abort. The first
pause that came next (never released) let the `C_TCP` read again before the
rest held was read: the rest paused the reading again after about 331
PINGREQ, the new read came while paused and went behind it, and a peer that
pipelined PINGREQ AND read its answers grew the rest by about 3.4 KB on each
pause, copied whole each time: the memory without bound and the CPU at
100 %, with no WARNING (a broker with 50 such peers, 41 to 105 MB in 30 s). `max_tx_queue` of `C_TCP` is not a way out: it discards the oldest
packet (the stream is broken) with an ERROR every `max_tx_queue` packets.

The protocol refuses on its own a client id with a character that is not
allowed (CONNACK `2` Identifier rejected, WARNING *"Mqtt: client_id contains
invalid character"*), before the upper level sees it.

(mqtt-moving-from-prot-mqtt)=
**Moving a gate from `C_PROT_MQTT`.** `C_PROT_MQTT` was removed after 7.30.26;
a yuno that still creates it gets *"gclass NOT FOUND"* at its start. What
changes:

| `C_PROT_MQTT` | `C_PROT_MQTT2` |
|---|---|
| `EV_ON_MESSAGE` with `mqtt_action` `publishing` / `subscribing` / `unsubscribing` | `EV_ON_IEV_MESSAGE` with `EV_MQTT_MESSAGE` / `EV_MQTT_SUBSCRIBE` / `EV_MQTT_UNSUBSCRIBE` |
| `EV_ON_OPEN` `client_id` | `EV_ON_OPEN` `id` |
| `EV_SEND_MESSAGE` `topic_name` | `EV_SEND_MESSAGE` `topic`, `qos`, `retain` |
| the gate's `gobj_mqtt_clients`, `gobj_mqtt_topics`, `gobj_mqtt_users` (`C_RESOURCE2`) | none: remove them |
| a SUBSCRIBE recorded by the protocol | the upper level answers its reason codes |
| a client's PUBLISH forwarded to the clients subscribed to it | nothing is forwarded |
| `allow_anonymous` `true` by default | `false` by default: set it in the gate |

`C_PROT_MQTT` forwarded a client's PUBLISH to the other clients subscribed to
its topic. A few anonymous subscribers that do not read took the memory of the
gate: in QoS 0 the send queue of `C_TCP` has no limit. A gate on
`C_PROT_MQTT2` forwards nothing.

Up to 7.30.25 a `C_PROT_MQTT2` without queues was not usable by a gate: it
authenticated every CONNECT against the broker's treedb, it took no QoS 1 or
2 PUBLISH (`max_qos` went to 0 at the CONNACK, and the client was
disconnected), and a PUBACK, PUBREC, PUBCOMP or PUBREL of any peer walked the
NULL queue: the yuno crashed. That case was reachable in a broker with
`mqtt_persistent_db` false (a configuration the broker refuses at its start
after 7.30.26: it has no mode without its treedb). Without a `C_MQTT_BROKER` each PUBLISH also
logged a WARNING *"ACL skipped"*; an anonymous client has no user and no
group, so it is not said any more for one.

After 7.30.25 a server sends DISCONNECT only in MQTT 5: in MQTT 3.1.1 it is a
packet of the client. A 3.1.1 client that breaks the protocol is dropped as
soon as the answers already sent to it are written (after 7.30.26; in 7.30.26
at once, and the answers queued behind the write in flight were lost); up to
7.30.25 it got a DISCONNECT, and the drop waited for `timeout_close` (3 s).
An MQTT 5 client that breaks the protocol gets its answers, then the
DISCONNECT `0x82`, then the close (after 7.30.26; in 7.30.26 the answers
behind the write in flight were lost).

## C_MQTT_BROKER

Full MQTT message broker — subscriber management, message routing,
session handling, and persistent storage using TreeDB on timeranger2.

**Trace levels:** `messages`, `messages2`.

**Attribute `__username__`** (`SDF_RD`): the broker writes the yuno's user
here before it opens its treedb, and its `treedbs` service asks the
permission of the broker's own `open-treedb` and `close-treedb` with it,
read from their `src`. That service is a `C_TREEDB` the broker creates
itself, in `open_database()` at its start, and destroys in
`close_database()` at its stop. **`mqtt_persistent_db` must be true** (the
default): the broker has no mode without its treedb, and `false` is refused
at the start with one critical log, *"mqtt_persistent_db false is not
supported: ..."*, then the exit that `on_critical_error` says (`2`, exit 0).
Up to 7.30.26 `false` was accepted, and the broker refused every CONNECT and
logged two ERRORs per tick. The user is not put in the kw of those commands: a kw
with `__username__` is a peer's command for `C_TREEDB` (see
[the broker's treedb](#mqtt-broker-treedb-user)).

**The topic `users` of its treedb administers the users of authz**
(`EV_ADD_USER`, `EV_REJECT_USER`), and only from the events its own treedb
publishes: an `EV_TREEDB_NODE_CREATED` / `_UPDATED` / `_DELETED` from any other
`src` (a peer of a gate sends them by name, they are public in `C_NODE`) is
refused with a braked WARNING, *"treedb event not published by the treedb of
the broker, refused"*. Up to 7.30.26 it was obeyed. See
[the users of the treedb](#mqtt-broker-users-sync).

**The packet id of a message to a client is its place in the client's queue**
(`((rowid - 1) mod 65535) + 1`), so it is the same at every send: a QoS 1/2
PUBLISH that was not acknowledged is sent again after a reconnect, or after a
restart of the broker, with its original packet id and DUP 1 [MQTT-4.4.0-1],
and so is its PUBREL. Nothing is saved for it. Up to 7.31.1 each send took a
new id: a client that reconnected got `mid 2` for the message it had as
`mid 1`.

**The will is published with its Message Expiry Interval** (MQTT 5 §3.1.3.2.4),
kept in the session (`will_expiry_interval`, `sessions` topic version 9). Up to
7.31.1 it went out with none.

**Shared subscriptions: each share group gets its copy, in turn among its
members.** `$share/<group>/<filter>` keeps one topic tree per group, and a
message matching the filter of two groups goes to one member of each [MQTT 5
4.8.2]; within a group the members take turns (round robin, as Mosquitto), and
a member that does not take it passes the turn to the next. A client in two
groups gets the message once per group. The command `shared-subs` shows the
group first:

```json
{
    "one": {"02A": {"share": {"test": {"@subs": {"client2": {"qos": 0, "id": 0, "options": 0},
                                                  "client3": {"qos": 0, "id": 0, "options": 0}}}}}},
    "two": {"02A": {"share": {"test": {"@subs": {"client3": {"qos": 0, "id": 0, "options": 0},
                                                  "client4": {"qos": 0, "id": 0, "options": 0}}}}}}
}
```

Up to 7.31.1 the group was not kept: every group of a filter was one tree, and
only one member of all of them got each message, always the first.

**The subscriptions of a persistent session survive a restart of the broker.**
Each SUBSCRIBE and UNSUBSCRIBE saves them in the session
(`subscriptions`: one `{topic_filter, qos, identifier, options}` per filter,
one write per packet), and the broker rebuilds its subscription trees from
them when its treedb opens, with one INFO *"Subscriptions of the persistent
sessions restored"*. A temporary session (MQTT 5 Session Expiry 0, MQTT 3.1.1
clean session) saves none. Up to 7.31.1 the column existed and nothing wrote
it: a session came back after a restart with session present 1 and no
subscription, and got nothing [MQTT-3.2.2-2].

Driven by the [`mqtt_broker`](../yunos/mqtt_broker.md) yuno — see that page for
the protocol/persistence/configuration details.

## C API

The helpers the three gclasses share have pages of their own:

- [MQTT utilities](#module-mqtt-util) (`mqtt_util.h`) -- names of the
  protocol's codes, validation of topics and UTF-8, the record of a message.
- [tr2q](#module-mqtt-tr2q) (`tr2q_mqtt.h`) -- the persistent queues of a
  session, on timeranger2.

## Registration

Register the gclasses a yuno uses once, in `register_yuno_and_more()`, before
creating them; a second call of the same one fails with *"GClass ALREADY
created"*. Each one returns `0`, or `-1` (logged) when the gclass cannot be
created.

```C
int register_c_prot_mqtt2(void);
int register_c_mqtt_broker(void);
```

The `mqtt_broker` yuno registers the protocol and the broker:

```C
static int register_yuno_and_more(void)
{
    int result = 0;
    result += register_c_prot_mqtt2();
    result += register_c_mqtt_broker();
    return result;  // a gclass refused stops the yuno (7.27.0+)
}
```

(register_c_prot_mqtt2)=
### `register_c_prot_mqtt2()` — `C_PROT_MQTT2`

The MQTT protocol: broker side, a gate's input with no broker, and client side
(`mqtt_tui` registers it with its own client gclass and `C_EDITLINE`, without
the broker).

(register_c_mqtt_broker)=
### `register_c_mqtt_broker()` — `C_MQTT_BROKER`

The broker.
