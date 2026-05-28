(yuno-mqtt_broker)=
# `mqtt_broker`

Full MQTT **v3.1.1 + v5.0** broker. The protocol engine (`C_PROT_MQTT2`) is an
adaptation of the Mosquitto logic onto Yuneta GClasses, with sessions,
subscriptions, retained messages and per-client queues persisted on
TreeDB / TimeRanger2.

## Architecture

The yuno (`main.c`) composes two services:

```
C_AUTHZ (service "authz")        <- authentication / authorization
C_MQTT_BROKER (default service)  <- broker: sessions, subscriptions, queues, retained
    per connection:
      C_CHANNEL > C_PROT_MQTT2   <- one MQTT protocol FSM per client connection
```

`mqtt_broker` is a **citizen yuno**: it does not open its own listening socket.
MQTT client connections arrive through the node's public gateway and are
delivered to the broker's `__input_side__` as channels; each channel runs a
`C_PROT_MQTT2` instance. The same `C_PROT_MQTT2` gclass also works as an MQTT
*client* (it carries client-side attrs like `mqtt_client_id`, `mqtt_protocol`,
will/keepalive).

## Protocol support

- MQTT **v5.0** (default), **v3.1.1** and **v3.1** — negotiated per CONNECT.
- QoS **0 / 1 / 2**, capped per-listener by `max_qos`.
- Retained messages (`retain_available`), Last Will & Testament (incl. v5
  will-delay / message-expiry properties).
- Keepalive / PING, clean vs persistent sessions, **session expiry interval**
  (v5; also enforced for v3.x via `mqtt_session_expiry_interval`).
- Normal and **shared subscriptions** (`$share`).
- Flow control: in-flight and queued-message limits, `message_size_limit`,
  `max_packet_size`.

## Persistence

When `mqtt_persistent_db` is true (default), the broker keeps state in a TreeDB
(`treedb_mqtt_broker`, schema `treedb_schema_mqtt_broker.c`) plus TimeRanger2
queues:

- **clients / sessions / subscriptions** — TreeDB nodes.
- **retained messages** — TreeDB topic `retained_msgs`.
- **inflight / queued messages** — TimeRanger2 queues (per client).

Persistent sessions survive broker restarts; `clean_session` clients are
transient. Set `mqtt_persistent_db=0` for an in-memory-only broker.

## Configuration

`C_MQTT_BROKER` (the `mqtt_broker` service):

| Attribute | Default | Purpose |
|-----------|---------|---------|
| `mqtt_persistent_db` | `true` | Persist clients/sessions/subscriptions/queues/retained |
| `enable_new_clients` | `false` | Auto-create unknown clients on connect |
| `deny_subscribes` | — | JSON list of topics for which SUBSCRIBE is refused |
| `mqtt_service` | *(yuno_role)* | Service name (multi-service) |
| `mqtt_tenant` | *(yuno_name)* | Tenant id (multi-tenant) |
| `use_internal_schema` | `true` | Use the hardcoded TreeDB schema |
| `on_critical_error` | `2` | `LOG_OPT_EXIT_ZERO` (exit, no auto-restart) on error |

Session limits come from `C_AUTHZ` (e.g. `Authz.max_sessions_per_user`,
default 4 in `main.c`).

Per-connection limits live on `C_PROT_MQTT2` and apply to each client:

| Attribute | Default | Purpose |
|-----------|---------|---------|
| `max_qos` | `2` | Max QoS allowed for connecting clients |
| `retain_available` | `true` | Allow retained messages (else RETAIN clients are dropped) |
| `max_inflight_messages` | `20` | Outgoing QoS 1/2 in flight (1 = strict in-order; 0 = unlimited) |
| `max_inflight_bytes` | `0` | Byte cap on in-flight messages (0 = no limit) |
| `max_queued_messages` | `1000` | Per-client queue depth above in-flight (0 = unlimited) |
| `max_queued_bytes` | `0` | Per-client queue byte cap (0 = no limit) |
| `message_size_limit` | `0` | Max publish payload accepted (0 = MQTT max) |
| `max_packet_size` | `0` | Max MQTT packet (v5 advertises it; 0 = no limit) |

## Commands

| Command | Description |
|---------|-------------|
| `list-channels` | Input channels of connected devices |
| `list-sessions` | Active/persistent sessions |
| `list-queues` | Per-client message queues |
| `normal-subs` / `shared-subs` | List normal / shared (`$share`) subscribers |
| `flatten-subs` | Flattened subscriber view |
| `list-retains` / `remove-retains` | List / remove retained messages (note: `#` shown as `/`) |
| `clean-queues` | Drop non-persistent, not-in-use sessions and their queues |
| `authzs` | Authorization help |
| `help` | Command help |

## Debugging

| GClass | Level | Shows |
|--------|-------|-------|
| `C_MQTT_BROKER` | `messages` / `messages2` | Broker-level message flow (verbose / icon form) |
| `C_PROT_MQTT2` | `traffic` | MQTT packets in/out (no payload) |
| `C_PROT_MQTT2` | `traffic-payload` | Include payload bytes |
| `C_PROT_MQTT2` | `show-decode` | Decoded packet structure |
| `C_PROT_MQTT2` | `messages2` | Simplified per-packet trace (icons) |

Enable with
`ycommand command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=<G> set=1 level=<L>`.
