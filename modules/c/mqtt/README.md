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
| `C_PROT_MQTT`     | `c_prot_mqtt.c` (8.5k L) | MQTT v3.1 / v3.1.1 protocol gclass. Parses/builds CONNECT, PUBLISH, SUBSCRIBE, etc. frames over a transport (typically `C_TCP`). |
| `C_PROT_MQTT2`    | `c_prot_mqtt2.c` (9.3k L)| MQTT v5.0 protocol gclass. Same role with v5 properties (session-expiry-interval, message-expiry-interval, will properties, …). |
| `C_MQTT_BROKER`   | `c_mqtt_broker.c` (4.7k L) | Broker logic on top: session/subscription/queue persistence via `tr2q_mqtt.c`, retained messages, shared subscriptions, per-tenant isolation, deny-lists. |

Helpers in the same directory: `mqtt_util.c` (parsing helpers),
`tr2q_mqtt.c` (timeranger2-backed queues for inflight + queued messages),
`treedb_schema_mqtt_broker.c` (the treedb schema for clients / topics /
sessions / subscriptions).

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
| `mqtt_persistent_db`   | `1`     | Persist clients / topics / inflight / queued messages via treedb+timeranger2. |
| `mqtt_service`         | (yuno_role) | Service name; defaults to the yuno role.                                  |
| `mqtt_tenant`          | (yuno_name) | Multi-tenant key; defaults to the yuno name.                              |

Full surface in `c_mqtt_broker.c:105+` (command table) and the
`attrs_table[]` below it.

## Persistence

All durable state goes through `tr2q_mqtt.c`, which uses **timeranger2**
queues (one per `(tenant, role)` combination). See
[`yunos/c/yuno_agent/TREEDB.md`](../../../yunos/c/yuno_agent/TREEDB.md)
for the timeranger2 / treedb layer.

## Consumers

- [`yunos/c/mqtt_broker/`](../../../yunos/c/mqtt_broker/) — the broker
  yuno (binds `C_MQTT_BROKER` to a TCP gate, autostart).
- [`yunos/c/mqtt_tui/`](../../../yunos/c/mqtt_tui/) — a TUI client built
  on `C_PROT_MQTT2` for interactive testing and debugging.
- Tests under [`tests/c/c_mqtt/`](../../../tests/c/c_mqtt/).

## Build

Built as part of the standard `yunetas build` flow once
`CONFIG_MODULE_MQTT=y` is set. Standalone build for development:

```bash
cd modules/c/mqtt/build
cmake .. && make install
```
