(module-mqtt)=
# MQTT

**Kconfig:** `CONFIG_MODULE_MQTT` — **GClasses:** `C_PROT_MQTT`, `C_PROT_MQTT2`, `C_MQTT_BROKER`

Full MQTT protocol implementation (v3.1.1 + v5.0) with a persistent message
broker backed by TreeDB.

## C_PROT_MQTT / C_PROT_MQTT2

MQTT protocol with publish, subscribe, QoS levels, and event-driven
message handling. `C_PROT_MQTT2` is the current implementation (and works as
both the broker's per-connection protocol and a standalone client — see
[`mqtt_tui`](../utilities/mqtt_tui.md)). `C_PROT_MQTT` is the legacy version
kept for backward compatibility.

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

A SUBSCRIBE or UNSUBSCRIBE writes the record only when it changes it, and at
most once a second for each connection. A change inside that second is
written by the next packet of the client after it (a PINGREQ is enough), or
when the client disconnects. A re-subscription with the same options, a
refused filter and an UNSUBSCRIBE of a filter that the client does not hold
write nothing. So a client that sends 400 SUBSCRIBE and UNSUBSCRIBE in one
second writes its record once, and once more at its next packet; if the gate
stops without an orderly shutdown in that second, the change is lost and the
client subscribes again when it connects.

#### Messages in flight and queued

Two more attributes limit the QoS 1 and 2 messages of one connection, in
memory. `0` is no limit.

| Attribute | Default | Past the limit |
|---|---|---|
| `max_inflight_messages` | `20` | To the client: a message waits in the client's queue, and goes out, with the packet id it got, when a PUBACK, PUBCOMP or failed PUBREC frees a slot. An MQTT 5 client that sends a lower Receive Maximum in its CONNECT gets that many in flight. From the client: it is also the Receive Maximum announced to MQTT 5 clients. A client with this many QoS 2 messages waiting for their PUBREL that sends one more PUBLISH breaks the protocol: MQTT 5 gets DISCONNECT `0x93` (Receive Maximum exceeded), and the connection closes, also for MQTT 3.1.1. One WARNING. |
| `max_queued_messages` | `1000` | A message to the client past `max_inflight_messages` + this is dropped. The client stays connected. One WARNING per connection. |

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
