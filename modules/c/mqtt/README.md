# MQTT module

Optional Yuneta module implementing the **MQTT protocol** (both v3.1 /
v3.1.1 and v5.0) as gclasses, plus the `C_MQTT_BROKER` broker logic. Used
by the [`mqtt_broker`](../../../yunos/c/mqtt_broker/) yuno and by any
client gclass that wants to speak MQTT.

## Enable

Selected at configure time via Kconfig:

```
CONFIG_MODULE_MQTT=y
```

(via `menuconfig`). When set, `modules/c/mqtt/` is added to the build
and the gclasses below are registered.

## GClasses shipped

| GClass            | Source                  | Purpose                                                              |
|-------------------|-------------------------|----------------------------------------------------------------------|
| `C_PROT_MQTT2`    | `c_prot_mqtt2.c` (9.9k L)| MQTT 3.1.1 and 5.0 protocol gclass, server and client side, with the v5 properties (session-expiry-interval, message-expiry-interval, will properties, …). The protocol of the broker, and the input of a gate with no broker: `allow_anonymous` (no authentication), no queues (`tranger_queues` NULL: QoS 1 and 2 PUBLISH taken, QoS 2 delivered at its PUBLISH, messages to the client in QoS 0), its parent answering the CONNECT and the reason codes of a (UN)SUBSCRIBE (one per filter, as many as the packet carries: `max_packet_size` bounds them). Per connection: `max_subscriptions_per_client` (100), `max_topic_filter_length` (256 bytes), `max_inflight_messages` (20). `EV_DROP` with `{"session_take_over": true}` is a take-over (MQTT 5: DISCONNECT `0x8E`; with `"delete_session": true` too, as the broker sends it when the session is not kept, the queues are closed); a server's close waits for the answers already sent to be written (MQTT 3.1.1: then the drop; MQTT 5: then its DISCONNECT and `timeout_close`), whatever starts it (a protocol error of the client, an `EV_DROP` during the close); past `max_unwritten_bytes` (100000: the memory of the answers waiting in its `C_TCP`; the messages forwarded to a subscriber do not count) the reading of a peer is paused until they are written (backpressure: a reader that pipelines is slowed, never dropped), and a peer that takes nothing during `timeout_close` is dropped, with one WARNING; a property that MQTT 5 does not define is a malformed packet, said with one WARNING; `EV_SEND_MESSAGE` refuses a topic with a wildcard. See the MQTT page of the docs. |
| `C_MQTT_BROKER`   | `c_mqtt_broker.c` (4.7k L) | Broker logic on top: session/subscription/queue persistence via `tr2q_mqtt.c`, retained messages, shared subscriptions, per-tenant isolation, deny-lists. |

Helpers in the same directory: `mqtt_util.c` (parsing helpers),
`tr2q_mqtt.c` (timeranger2-backed queues for inflight + queued messages),
`treedb_schema_mqtt_broker.c` (the treedb schema for clients / topics /
sessions / subscriptions; the file holds its graph and the literal, exactly
what a schema-editor export writes, so an export replaces it whole) and `msg2db_schema_alarms.c` (the alarm log).

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
| `mqtt_persistent_db`   | `1`     | Must be `1`: clients / sessions / retained / queued messages live in treedb+timeranger2. `0` is refused at start-up (one CRITICAL, exit as `on_critical_error` says). |
| `mqtt_service`         | (yuno_role) | Service name; defaults to the yuno role.                                  |
| `mqtt_tenant`          | (yuno_name) | Multi-tenant key; defaults to the yuno name.                              |

Full surface in `c_mqtt_broker.c:105+` (command table) and the
`attrs_table[]` below it.

## Persistence

All durable state goes through `tr2q_mqtt.c`, which uses **timeranger2**
queues (one per `(tenant, role)` combination). See
[`yunos/c/yuno_agent/YUNO_TREEDB.md`](../../../yunos/c/yuno_agent/YUNO_TREEDB.md)
for the timeranger2 / treedb layer.

A load that cannot read every pending message returns -1, keeps
`first_rowid`, and refuses the backup until a load reads them all
([`tr_queue.md`](../../../docs/doc.yuneta.io/api/timeranger2/tr_queue.md)).

## Consumers

- [`yunos/c/mqtt_broker/`](../../../yunos/c/mqtt_broker/) — the broker
  yuno (binds `C_MQTT_BROKER` to a TCP gate, autostart).
- [`utils/c/mqtt_tui/`](../../../utils/c/mqtt_tui/) — a TUI client built
  on `C_PROT_MQTT2` for interactive testing and debugging.
- Tests under [`tests/c/c_mqtt/`](../../../tests/c/c_mqtt/).

## Build

Built as part of the standard `yunetas build` flow once
`CONFIG_MODULE_MQTT=y` is set. Standalone build for development:

```bash
cd modules/c/mqtt/build
cmake .. && make install
```
