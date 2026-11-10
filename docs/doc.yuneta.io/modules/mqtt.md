(module-mqtt)=
# MQTT

**Kconfig:** `CONFIG_MODULE_MQTT` — **GClasses:** `C_PROT_MQTT`, `C_PROT_MQTT2`, `C_MQTT_BROKER`

Full MQTT protocol implementation (v3.1.1 + v5.0) with a persistent message
broker backed by TreeDB.

## C_PROT_MQTT / C_PROT_MQTT2

MQTT protocol with publish, subscribe, QoS levels, and event-driven
message handling. `C_PROT_MQTT2` is the current implementation. It works as
the broker's per-connection protocol, as the input of a gate with no broker
(see [`C_PROT_MQTT2` as the input of a gate](#mqtt-prot2-gate)), and as a
standalone client (see [`mqtt_tui`](../utilities/mqtt_tui.md)). `C_PROT_MQTT`
is the legacy version, kept until the remaining gates move to `C_PROT_MQTT2`.

**Trace levels (`C_PROT_MQTT2`):** `traffic` (packets, no payload),
`traffic-payload`, `show-decode` (decoded packet structure), `messages2`.

**Secrets:** `user_passw`, `jwt` and `auth_data` (the MQTT 5 AUTH data of the
connection, `C_PROT_MQTT` too) are `SDF_SECRET`: `view-attrs`, `view-gobj` and
`list-persistent-attrs` show them as `********` (an empty one stays empty).
Up to 7.25.20 `auth_data` showed in clear.

A frame the protocol refuses is dumped in the log, at every trace level
(*"MQTT malformed packet: <command>"*, length-capped). Of a CONNECT only the
head is dumped, the protocol name, level, flags and keep alive, and of an AUTH
only its reason code: the properties and the payload carry the credentials.
Up to 7.25.20 the whole CONNECT was dumped, password included.

```text
"msg": "MQTT malformed packet: CMD_CONNECT (credentials not dumped)",
"data": {"0000": "00 04 4D 51 54 54 05 C2 00 3C     ..MQTT...<"}
```

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=view-attrs gobj=<C_PROT_MQTT2 full name> attribute=auth_data'
# {"C_PROT_MQTT2^<name>": "********"}
```

### `C_PROT_MQTT`: the records of the clients and their limits

`C_PROT_MQTT` keeps one record per client id in the store its gate gives it
as `gobj_mqtt_clients` (a [`C_RESOURCE2`](#gclass-c-resource2)). Only a
client with a session to keep has its record on disk: MQTT 3.1.1 with clean
session 0, MQTT 5 with a session expiry interval above 0. Every other client
(MQTT 3.1.1 clean session 1, MQTT 5 with no session expiry, an id the gate
assigned) has its record in memory only, and the record is removed when the
client disconnects.

- A later CONNECT with the same id and clean session 0 starts a new session:
  CONNACK with session present 0, no subscriptions.
- A CONNECT with clean session 1 over a session that exists ends that
  session: its record and its file are removed when the client disconnects.
- A subscription to a filter that the client already holds replaces it, as
  MQTT says: SUBACK granted, the client stays connected.
- A CONNECT that is refused (for example MQTT 3.1.1 with keep alive 0) leaves
  no record: the record that the CONNECT made, or made a session of, stays in
  memory only until the CONNACK is sent.
- A record that is loaded from disk after an unclean stop says that its
  client is connected. The protocol does not believe it: a message to it is
  not sent, and its client connects again with no error.

Four attributes limit what clients can keep. `0` is no limit, or never.

| Attribute | Default | Past the limit |
|---|---|---|
| `max_subscriptions_per_client` | `100` | Each new filter of the SUBSCRIBE is refused in the SUBACK (MQTT 5 `0x97` Quota exceeded, MQTT 3.1.1 `0x80` Failure). The client stays connected. One WARNING per connection. |
| `max_topic_filter_length` | `256` | A filter of a SUBSCRIBE longer than this, in bytes, is refused in the SUBACK (MQTT 5 `0x8F` Topic Filter invalid, MQTT 3.1.1 `0x80` Failure). The client stays connected. One WARNING per connection. MQTT allows 65535. An UNSUBSCRIBE is never refused by it: a longer filter that the client holds (kept by 7.30.22, or before the limit went down) is removed. |
| `max_persistent_sessions` | `1000` | A CONNECT that adds a session is refused (CONNACK: MQTT 5 `0x97`, MQTT 3.1.1 `3` Server unavailable) and the connection closes, unless an expired session makes room (next row). A client without a session, and a client whose session exists, still connect. One WARNING per minute in the process, with the CONNECTs refused since the last one. |
| `persistent_session_expiry` | `86400` (one day) | Seconds a session is kept after its client disconnects. It applies to MQTT 3.1.1 clean session 0, and it is the maximum for the Session Expiry Interval of MQTT 5 (a shorter interval of the client applies). When the sessions reach `max_persistent_sessions`, the CONNECT that needs room removes every session that expired, record and file. Under the limit nothing is removed, and with `max_persistent_sessions` `0` no session is removed. |

A session's record keeps the time its client disconnected (`disconnected_t`,
seconds of the wall clock, written with the record) and the expiry the client
asked (`session_expiry`). A record that does not have `disconnected_t` (its
client was connected when the process stopped, or a version before 7.30.23
wrote it) counts from the first time a CONNECT needs the room.

Set them in the `kw` of the protocol gobj, in the gclass tree of the gate's
channel:

```json
{
    "name": "mqtt",
    "gclass": "C_PROT_MQTT",
    "kw": {
        "iamServer": true,
        "max_subscriptions_per_client": 20,
        "max_topic_filter_length": 128,
        "max_persistent_sessions": 200,
        "persistent_session_expiry": 604800
    }
}
```

With this configuration, a device that subscribes `cmnd/dev-17/#` gets its
SUBACK. A filter of 129 bytes gets `0x80` (MQTT 3.1.1) and the device stays
connected. A device that has been disconnected for more than 7 days loses its
session when the 200 sessions are full and a new device connects with clean
session 0.

The WARNINGs (`msgset` `Mqtt`):

```text
"msg": "Mqtt: SUBSCRIBE past max_subscriptions_per_client, filters refused", "max_subscriptions_per_client": 100, "client_id": "dev-17", "peername": "203.0.113.5:40112", "sub": "m/100"
"msg": "Mqtt: topic filter longer than max_topic_filter_length, refused", "max_topic_filter_length": 256, "length": 1000, "client_id": "dev-17", "peername": "203.0.113.5:40112", "sub": "xxxxxxxx..."
"msg": "Mqtt: CONNECT past max_persistent_sessions, new sessions refused", "max_persistent_sessions": 1000, "refused": 1, "client_id": "x-91", "peername": "203.0.113.5:40114"
```

A PUBLISH looks up its topic once in the subscriptions of each record, so its
cost grows with the records, not with their subscriptions. It writes nothing
to disk: a QoS 1 or 2 message goes only to a connected subscriber, with the
next packet id of that client.

A record is written only when what is saved of it changed: every key but
the ones that start with `_` is compared with what was written last. A
re-subscription with the same options, a refused filter, an UNSUBSCRIBE of a
filter that the client does not hold, and a CONNECT or a disconnection that
changes nothing that is saved write nothing.

A change is written at most once a second for each connection: a SUBSCRIBE,
an UNSUBSCRIBE, and the CONNECT of a session that exists (that its client is
connected). The first change of a new session is written at once. A change
inside the second waits, and is written by the first of these:

- a timer, one second after the last change (it runs on the ticks of the
  yuno, `timeout_periodic`, so the change is on disk within two seconds);
- the next packet of the client after the second;
- the disconnection.

For example, a device whose session exists connects again and subscribes two
filters, then says nothing more:

```text
t=0.0  CONNECT, clean session 0       waits (connected)
t=0.1  SUBSCRIBE cmnd/dev-17/#         waits
t=0.2  SUBSCRIBE stat/dev-17/#         waits
t=1.2 to t=2.2                         the timer writes the record once: connected, both filters
```

So a client that sends 400 SUBSCRIBE and UNSUBSCRIBE in one second writes its
record once, and once more about a second later. A client that connects and
disconnects 200 times in one second writes its record once: each
disconnection dates the record (`disconnected_t`, in seconds), and that is
the only key that changes. If the gate stops without an orderly shutdown
before a change is written, the change is lost, and the client subscribes
again when it connects.

#### Sessions in memory only

A gate whose sessions do not have to survive a restart creates its three
stores (`gobj_mqtt_clients`, `gobj_mqtt_topics`, `gobj_mqtt_users`) with
`persistent` false. Then nothing is written to disk and no directory is
made. A client with a session (clean session 0) keeps its subscriptions
across its connections while the gate runs, and the limits above apply in
the same way. When the gate stops, every session is lost, and so is a user
made with `create-user`.

```c
priv->gobj_mqtt_clients = gobj_create_service(
    "mqtt_clients",
    C_RESOURCE2,
    json_pack("{s:s, s:s, s:b}",
        "service", "mqtt",
        "database", "clients",
        "persistent", 0
    ),
    gobj
);
```

Do the same for `mqtt_topics` and `mqtt_users`, then give the three stores
to the protocol gobj (`gobj_mqtt_clients`, `gobj_mqtt_topics`,
`gobj_mqtt_users`). A store with `persistent` true reads its records from
disk when it starts (`gobj_start()`): a gate that keeps its sessions on disk
must start its stores, or the sessions written before a restart are never
read.

#### Messages in flight and queued

Two more attributes limit the QoS 1 and 2 messages of one connection, in
memory. `0` is no limit.

| Attribute | Default | Past the limit |
|---|---|---|
| `max_inflight_messages` | `20` | To the client: a message waits in the client's queue, and goes out, with the packet id it got, when a PUBACK, PUBCOMP or failed PUBREC frees a slot. An acknowledgement ends only a message in flight, never a queued one with the same packet id. An MQTT 5 client that sends a lower Receive Maximum in its CONNECT gets that many in flight. From the client: it is also the Receive Maximum announced to MQTT 5 clients. A client with this many QoS 2 messages waiting for their PUBREL that sends one more PUBLISH breaks the protocol: MQTT 5 gets DISCONNECT `0x93` (Receive Maximum exceeded), and the connection closes, also for MQTT 3.1.1. One WARNING. |
| `max_queued_messages` | `1000` | A message to the client past `max_inflight_messages` + this is dropped, and takes no packet id. The client stays connected. One WARNING per connection. |

```json
{
    "name": "mqtt",
    "gclass": "C_PROT_MQTT",
    "kw": {
        "iamServer": true,
        "max_inflight_messages": 10,
        "max_queued_messages": 500
    }
}
```

With this configuration, a device that does not acknowledge gets 10 QoS 1
messages; the next 500 wait, and the rest are dropped. Each PUBACK of the
device sends the oldest waiting message. A client that sends 11 QoS 2
messages with no PUBREL is disconnected at the 11th, with no PUBREC for it.
The messages of a connection, in flight and queued, are freed when it
closes.

```text
"msg": "Mqtt: PUBLISH with max_inflight_messages QoS 2 unreleased, disconnecting", "max_inflight_messages": 10, "client_id": "dev-17", "peername": "203.0.113.5:40112"
"msg": "Mqtt: messages to the client past max_queued_messages, dropped", "max_inflight_messages": 10, "max_queued_messages": 500, "client_id": "dev-17", "peername": "203.0.113.5:40112"
```

`max_inflight_bytes` and `max_queued_bytes` are not applied.

`message_size_limit` (bytes, default `0`, no limit) refuses a bigger PUBLISH
payload. The message is not delivered, the QoS flow is completed (PUBACK or
PUBREC; MQTT 5 with reason `0x95`, Packet too large), and the client stays
connected. One WARNING per message: *"Mqtt: Dropped too large PUBLISH"*.

```json
"kw": {
    "iamServer": true,
    "message_size_limit": 4096
}
```

Set `message_size_limit` in every gate whose devices send small payloads. With
`0`, a PUBLISH is limited only by the largest memory block of the yuno
(`MEM_MAX_BLOCK` of its `main.c`, 200 MB in the `yuno_citizen` template), and
each subscriber that gets the message keeps its own copy of the payload
while it is in flight or queued: one PUBLISH of 200 MB to 10 subscribers can
hold 2 GB. A device that sends a few hundred bytes is well served by 4096.

Up to 7.30.21 every client id left a record that was never removed, in memory
and on disk; a client could hold any number of subscriptions; each PUBLISH
walked every subscription of every record; and a re-subscription was taken
for a malformed packet and disconnected the client. One anonymous client that
connected with new ids, or that subscribed many filters and left, slowed
every later message of the gate, and a few of them took it down by memory.

Up to 7.30.22 a session never expired, so 1000 new ids with clean session 0
refused every new device with a session for ever; a topic filter could be
65535 bytes; each QoS 1 or 2 PUBLISH rewrote the whole file of every
subscriber with a session, connected or not (every packet id was 1); a
refused CONNECT could leave a record; the SUBSCRIBE of a connection that
another CONNECT took over logged *"client not found"*; and a record reloaded
after an unclean stop logged an ERROR for each message to it.

Up to 7.30.23 a QoS 2 PUBLISH sent twice (the second one a duplicate, as a
client does when it retries) freed a message that was still waiting for its
PUBREL: the PUBREL read freed memory and the disconnection freed it again,
which took the gate down (from 7.0.0). `max_inflight_messages` was announced
and not applied: a client that did not acknowledge, or that sent QoS 2
messages with no PUBREL, made the gate keep all of them in memory. Each
SUBSCRIBE and UNSUBSCRIBE rewrote the whole record, also when it changed
nothing: one connection whose record held 100 long filters could write about
200 MB a second. The topic-filter
length also refused an UNSUBSCRIBE, so a filter kept from before could not be
removed. A DISCONNECT of MQTT 5 with properties and no Session Expiry
Interval logged an ERROR; and a client with an id the gate assigned logged
*"path NOT FOUND"* at its first QoS 1 or 2 message. With `message_size_limit`
set, a PUBLISH past it was freed twice, which took the gate down; and the
Receive Maximum of an MQTT 5 client was never kept.

Up to 7.30.24 a PUBACK, PUBREC or PUBCOMP found its message by the packet id
alone, also among the queued messages, and a dropped message took a packet id
too. When the packet ids of a slow subscriber wrapped, an acknowledgement could
end a queued message that was never sent and leave the one in flight for
ever: the subscriber got nothing more, and nothing was logged (from 7.30.24,
the first version with a queue). Each CONNECT and each disconnection of a
session rewrote its whole record (from before 7.2.0): a client that connected
and disconnected in a loop with 100 filters made the gate write about 80 MB a
second. A change inside the second of the (UN)SUBSCRIBE writes waited for the
next packet of the client, which for a client that says nothing more is its
keep alive.

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
| `max_packet_size` | `0` | A packet larger than this is refused when its header arrives, before its bytes are kept, and the client is disconnected. Without it a packet is kept whole (up to 256 MB) before `message_size_limit` is checked. |

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
  WARNING says it.

**What the upper level answers.** The protocol publishes to its parent, and
reads the return of the publication:

| Event | kw | The upper level |
|---|---|---|
| `EV_ON_OPEN` | the client: `id`, `protocol_version`, `clean_start`, `peername`, the will (its payload as `gbuffer`) | returns `0`: CONNACK accepted, session present 0. Returns `-2`: CONNACK not authorized, and the connection closes. |
| `EV_ON_IEV_MESSAGE` with `__iev_event__` `EV_MQTT_MESSAGE` | `topic`, `qos`, `retain`, `gbuffer` (the payload, peer bytes) | takes the message. Returns `1` when it took it (MQTT 5: PUBACK Success); `0` is "no matching subscribers" (`0x10`). |
| `EV_ON_IEV_MESSAGE` with `__iev_event__` `EV_MQTT_SUBSCRIBE` | `subs` (`sub`, `qos`, …), `gbuffer` | appends ONE reason code per filter of `subs` to the `gbuffer`, in order: the QoS granted, or `>= 0x80` to refuse it. The filters the protocol refused are not in `subs`. |
| `EV_ON_IEV_MESSAGE` with `__iev_event__` `EV_MQTT_UNSUBSCRIBE` | `subs`, `gbuffer` | appends one code per filter (`0` Success). |
| `EV_ON_CLOSE` | `client_id`, `peername` | the connection ended. |

A missing reason code is an internal error (ERROR *"The upper level did not
answer one reason code per filter"*), and its filter gets `0x80`.

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

**What the protocol does not do, and the gate must:** the same client id on a
second connection is not taken over. A broker drops the first connection; a
gate looks for the decoder of that id in its other channels and sends it
`EV_DROP`.

The protocol refuses on its own a client id with a character that is not
allowed (CONNACK `2` Identifier rejected, WARNING *"Mqtt: client_id contains
invalid character"*), before the upper level sees it.

**Moving a gate from `C_PROT_MQTT`:**

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
int register_c_prot_mqtt(void);
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

(register_c_prot_mqtt)=
### `register_c_prot_mqtt()` — `C_PROT_MQTT`

The deprecated protocol, kept until the remaining gates migrate to
`C_PROT_MQTT2`.

(register_c_prot_mqtt2)=
### `register_c_prot_mqtt2()` — `C_PROT_MQTT2`

The MQTT protocol, broker side and client side (`mqtt_tui` registers it with
its own client gclass and `C_EDITLINE`, without the broker).

(register_c_mqtt_broker)=
### `register_c_mqtt_broker()` — `C_MQTT_BROKER`

The broker.
