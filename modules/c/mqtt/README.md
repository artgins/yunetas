# MQTT module

The MQTT module is an optional Yuneta module. It implements the MQTT protocol
(v3.1 / v3.1.1 and v5.0) as gclasses, and the `C_MQTT_BROKER` broker logic.
The [`mqtt_broker`](../../../yunos/c/mqtt_broker/) yuno uses it, and so can
any client gclass that speaks MQTT.

## Enable

To enable the module, set this Kconfig option at configure time (with
`menuconfig`):

```
CONFIG_MODULE_MQTT=y
```

When the option is set, the build includes `modules/c/mqtt/` and registers
the gclasses below.

## Build

The standard `yunetas build` flow builds the module when
`CONFIG_MODULE_MQTT=y` is set. For development, you can build it alone:

```bash
cd modules/c/mqtt/build
cmake .. && make install
```

## GClasses shipped

| GClass            | Source                  | Purpose                                                              |
|-------------------|-------------------------|----------------------------------------------------------------------|
| `C_PROT_MQTT2`    | `c_prot_mqtt2.c` (9.9k L)| MQTT 3.1.1 and 5.0 protocol gclass, server and client side. The protocol of the broker, and the input of a gate with no broker. The next section gives the details. |
| `C_MQTT_BROKER`   | `c_mqtt_broker.c` (4.7k L) | Broker logic on top: session/subscription/queue persistence via `tr2q_mqtt.c`, retained messages, shared subscriptions, per-tenant isolation, deny-lists. |

Helpers in the same directory:

- `mqtt_util.c`: parsing helpers.
- `tr2q_mqtt.c`: timeranger2-backed queues for inflight and queued messages.
- `treedb_schema_mqtt_broker.c`: the treedb schema for clients / topics /
  sessions / subscriptions. The file holds its graph and the literal, exactly
  what a schema-editor export writes. Thus an export replaces it whole.
- `msg2db_schema_alarms.c`: the alarm log.

### `C_PROT_MQTT2`

`C_PROT_MQTT2` is the MQTT 3.1.1 and 5.0 protocol gclass, for the server side
and the client side. It has the v5 properties (session-expiry-interval,
message-expiry-interval, will properties, …).

It is the protocol of the broker. It is also the input of a gate with no
broker. In such a gate:

- `allow_anonymous` is set (no authentication).
- There are no queues (`tranger_queues` NULL). The gclass takes a QoS 1 and
  a QoS 2 PUBLISH. It delivers a QoS 2 message at its PUBLISH. It sends the
  messages to the client in QoS 0.
- Its parent answers the CONNECT. The parent also gives the reason codes of a
  (UN)SUBSCRIBE: one per filter, as many as the packet carries.
  `max_packet_size` sets the limit of that number.

The limits per connection are `max_subscriptions_per_client` (100),
`max_topic_filter_length` (256 bytes), and `max_inflight_messages` (20).
In a broker, `max_subscriptions_per_client` is also the limit of the
session: the filters that a persistent session kept from its earlier
connections count. A new filter past it gets `0x80` (MQTT 3.1.1) or `0x97`
(MQTT 5) in the SUBACK. For example, with
`"max_subscriptions_per_client": 20` in the `kw` of the `C_PROT_MQTT2`, a
session that holds 20 filters gets `0x80` or `0x97` for a 21st, on any
connection.

`EV_DROP` with `{"session_take_over": true}` is a take-over. An MQTT 5 client
gets DISCONNECT `0x8E`. The queues close at once. If the event also has
`"delete_session": true`, the gclass deletes their topics, so a clean
take-over starts empty. The broker sends `"delete_session": true` when it does
not keep the session. Without it, the topics stay for the new connection.

The close of a server connection deletes its queue topics only when its
session ends with it. In MQTT 5, that is Session Expiry 0 at the close (a
DISCONNECT can change it). In MQTT 3.1.1, it is a clean session. Clean Start
alone never deletes them.

A server close waits until the answers already sent are written. Then, in MQTT
3.1.1, the drop follows. In MQTT 5, its DISCONNECT and `timeout_close` follow.
This is the same for any start of the close, for example a protocol error of
the client, or an `EV_DROP` during the close.

Backpressure: past `max_unwritten_bytes` (100000), the gclass pauses the
reading of a peer until the answers are written. That value is the memory of
the answers that wait in its `C_TCP`. The messages forwarded to a subscriber
do not count. The gclass slows a reader that pipelines, but never drops it. A
connection holds about the limit and one read, whether its peer reads or not.
One read is 4 KB in clear. Over TLS, it is up to about 20 KB of clear text: a
record of 16 KB and what the same read completes after it.

If a peer takes nothing during `timeout_close`, the gclass drops it, with one
WARNING. This happens only while the protocol holds answers that its transport
cannot write. When every answer is in the kernel, the connection is idle, and
only keepalive or the pool ends it.

A property that MQTT 5 does not define is a malformed packet. The gclass says
so with one WARNING.

`EV_SEND_MESSAGE` refuses a topic with a wildcard.

For more, see the MQTT page of the docs.

## Broker command surface (`C_MQTT_BROKER`)

Selected commands declared in the broker's command table:

| Command             | Purpose                                                    |
|---------------------|------------------------------------------------------------|
| `list-channels`     | List input channels of connected devices.                  |
| `list-sessions`     | List MQTT sessions (active + persisted).                   |
| `list-queues`       | List queued messages (per session / QoS).                  |
| `normal-subs`       | List non-shared subscribers.                               |
| `shared-subs`       | List shared subscribers.                                   |
| `flatten-subs`      | Flatten the subscription tree.                             |
| `list-retains`      | List retained messages (`/` is encoded as `#` in topics).  |
| `remove-retains`    | Remove retained messages by topic.                         |
| `clean-queues`      | Clean non-persistent / unused queues + sessions.           |
| `authzs`            | Show authz help for this service.                          |

Selected broker attributes:

| Attribute              | Default | Purpose                                                                       |
|------------------------|---------|-------------------------------------------------------------------------------|
| `enable_new_clients`   | `0`     | If true, auto-create unknown clients on first connect.                        |
| `mqtt_persistent_db`   | `1`     | Must be `1`: clients / sessions / retained / queued messages live in treedb+timeranger2. The broker refuses `0` at start-up (one CRITICAL, exit as `on_critical_error` says). |
| `mqtt_service`         | (yuno_role) | Service name. The default is the yuno role.                               |
| `mqtt_tenant`          | (yuno_name) | Multi-tenant key. The default is the yuno name.                           |

The full list is in `c_mqtt_broker.c:105+` (command table) and in the
`attrs_table[]` below it.

## Persistence

All durable state goes through `tr2q_mqtt.c`, which uses timeranger2
queues (one per `(tenant, role)` combination). For the timeranger2 / treedb
layer, see
[`yunos/c/yuno_agent/YUNO_TREEDB.md`](../../../yunos/c/yuno_agent/YUNO_TREEDB.md).

A load that cannot read every pending message returns -1, keeps
`first_rowid`, and refuses the backup until a load reads them all
([`tr_queue.md`](../../../docs/doc.yuneta.io/api/timeranger2/tr_queue.md)).

## Consumers

- [`yunos/c/mqtt_broker/`](../../../yunos/c/mqtt_broker/): the broker
  yuno (binds `C_MQTT_BROKER` to a TCP gate, autostart).
- [`utils/c/mqtt_tui/`](../../../utils/c/mqtt_tui/): a TUI client built
  on `C_PROT_MQTT2` for interactive testing and debugging.
- Tests under [`tests/c/c_mqtt/`](../../../tests/c/c_mqtt/).
