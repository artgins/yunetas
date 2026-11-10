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
  NULL queue, a crash).
- **`max_packet_size`** bounds what the broker may send: a larger packet is
  refused at its header (WARNING *"Incoming packet exceeds max_packet_size"*,
  DISCONNECT `0x82`), and an MQTT 5 CONNECT announces it (Maximum Packet
  Size). Up to 7.30.26 it was the server's only, and a client kept what its
  broker announced, up to 256 MB.

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
| `max_packet_size` | `0` | A packet larger than this is refused when its header arrives, before its bytes are kept, and the client is disconnected. Without it a packet is kept whole (up to 256 MB) before `message_size_limit` is checked. It also bounds how many filters one (UN)SUBSCRIBE carries to the upper level (see below). |
| `max_qos` | `2` | `0` or `1`: a PUBLISH of a higher QoS disconnects the client. MQTT 5 clients are told it in the CONNACK (Maximum QoS). Up to 7.30.26 that property was not sent, and each CONNECT logged an ERROR. |
| `timeout_close` | `3000` | Milliseconds an MQTT 5 connection waits, after the server's DISCONNECT, for the client to close. Then the transport is dropped, with a WARNING *"Timeout waiting mqtt disconnected"*. |

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
| `EV_ON_IEV_MESSAGE` with `__iev_event__` `EV_MQTT_SUBSCRIBE` | `subs` (`sub`, `qos`, …), `gbuffer` | appends ONE reason code per filter of `subs` to the `gbuffer`, in order: the QoS granted, or `>= 0x80` to refuse it. The filters the protocol refused are not in `subs`. |
| `EV_ON_IEV_MESSAGE` with `__iev_event__` `EV_MQTT_UNSUBSCRIBE` | `subs`, `gbuffer` | appends ONE reason code per filter of `subs`, in order: `0x00` Success, `0x11` No subscription existed, or `>= 0x80` to refuse it. Every filter of the packet is in `subs`: the protocol refuses none. MQTT 3.1.1 has no codes in its UNSUBACK, but the upper level writes them all the same. |
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

**How many filters reach the upper level.** As many as one packet carries.
A filter repeated in the same SUBSCRIBE, or one the connection already holds,
is passed up again and not counted again: MQTT says it replaces the
subscription, and wants one code for each filter of the packet. The bound is
the packet: with `max_packet_size` 66560, a SUBSCRIBE carries at most about
16 600 filters (4 bytes each at least) and an UNSUBSCRIBE about 22 000 (3
bytes each). An example, the codes of a SUBSCRIBE of `a/1`, `a/1` and `a/2`
by an MQTT 5 client, with an upper level that grants all three in QoS 0:

```text
SUBSCRIBE  a/1 a/1 a/2      ->  subs: [a/1, a/1, a/2]  ->  SUBACK 0x00 0x00 0x00
                                                         (held: a/1, a/2: counted 2)
```

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

- the topic is not one a PUBLISH can carry: it has a wildcard (`+`, `#`), it
  is empty, or it has more than 200 levels. An ERROR *"Mqtt publish: topic
  invalid"* names it. A subscription FILTER is not a topic to publish to: up
  to 7.30.26 a message to `cmnd/dev-17/#` was sent as it was [MQTT-3.3.2-2];
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
(DISCONNECT `0x8E` Session taken over; without the key it gets `0x00`):

```C
gobj_send_event(
    gobj_bottom_gobj(gobj_old_decoder),     // its C_PROT_MQTT2
    EV_DROP,
    json_pack("{s:b}", "session_take_over", 1),
    gobj
);
```

In MQTT 3.1.1 a server sends no DISCONNECT, and the old connection is dropped
at once. In MQTT 5 it is dropped after its DISCONNECT, within `timeout_close`;
in that window the connection is closing, and another `EV_DROP` drops it at
once (up to 7.30.26 it was an ERROR *"Event NOT DEFINED in state"*, with a
stack). A decoder that has dropped its connection is no longer the client's:
it must stop taking part in the take-over, or every new connection of that id
sends `EV_DROP` to all the connections still closing.

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
`mqtt_persistent_db` false. Without a `C_MQTT_BROKER` each PUBLISH also
logged a WARNING *"ACL skipped"*; an anonymous client has no user and no
group, so it is not said any more for one.

After 7.30.25 a server sends DISCONNECT only in MQTT 5: in MQTT 3.1.1 it is a
packet of the client. A 3.1.1 client that breaks the protocol is dropped at
once; up to 7.30.25 it got a DISCONNECT, and the drop waited for
`timeout_close` (3 s).

## C_MQTT_BROKER

Full MQTT message broker — subscriber management, message routing,
session handling, and persistent storage using TreeDB on timeranger2.

**Trace levels:** `messages`, `messages2`.

**Attribute `__username__`** (`SDF_RD`): the broker writes the yuno's user
here before it opens its treedb, and its `treedbs` service asks the
permission of the broker's own `open-treedb` and `close-treedb` with it,
read from their `src`. That service is a `C_TREEDB` the broker creates
itself, in `open_database()` at its start, only when `mqtt_persistent_db` is
on, and destroys in `close_database()` at its stop. The user is not put in the kw of those commands: a kw
with `__username__` is a peer's command for `C_TREEDB` (see
[the broker's treedb](#mqtt-broker-treedb-user)).

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
