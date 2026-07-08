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
(`treedb_mqtt_broker`, schema [`treedb_schema_mqtt_broker.c`](https://github.com/artgins/yunetas/blob/7.7.2/modules/c/mqtt/src/treedb_schema_mqtt_broker.c)) plus TimeRanger2
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
| `enable_acl` | `false` | Enforce per-group publish/subscribe ACLs (see [Authorization](#mqtt-acl)) |
| `deny_subscribes` | — | JSON list of topics for which SUBSCRIBE is refused |
| `mqtt_service` | *(yuno_role)* | Service name (multi-service) |
| `mqtt_tenant` | *(yuno_name)* | Tenant id (multi-tenant) |
| `use_internal_schema` | `true` | Use the hardcoded TreeDB schema |
| `on_critical_error` | `2` | `LOG_OPT_EXIT_ZERO` (exit, no auto-restart) on error |

Session limits come from [`C_AUTHZ`](#gclass-c-authz) (e.g. `Authz.max_sessions_per_user`,
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

(mqtt-acl)=
## Authorization (publish/subscribe ACL)

Authentication (who the client is) is handled by [`C_AUTHZ`](#gclass-c-authz) —
see [Authentication, authorisation, and TLS](../../../yunos/c/yuno_agent/YUNO_AUTH.md). Authorization of
*which topics a client may use* is a separate, broker-local mechanism: a
**per-group topic ACL** stored in the broker's own TreeDB.

The model (model A — group-based ACL in the treedb):

- The `client_groups` topic
  ([`treedb_schema_mqtt_broker.c`](https://github.com/artgins/yunetas/blob/7.7.2/modules/c/mqtt/src/treedb_schema_mqtt_broker.c))
  carries two array columns: **`publish_acl`** and **`subscribe_acl`**, each a
  list of MQTT topic-filter patterns (`+` / `#` wildcards).
- A `clients` row fkeys to one or more `client_groups`. A client is allowed an
  access if **any** of its groups has a pattern matching the topic, using the
  same `topic_matches_sub()` wildcard logic the broker uses for routing.

### Posture (backward-compatible, default off)

Enforcement is gated by the `enable_acl` attr on `C_MQTT_BROKER` (default
`false`). The ACL helper allows when:

- `enable_acl` is **off** (default) — no enforcement, allow-all; OR
- the client's groups define **no patterns** for that access — allow-all
  (so adding the columns doesn't break an existing broker until ACLs are
  authored).

With `enable_acl` **on** and patterns authored, a topic must match one of the
client's group patterns. An **unknown client** (with ACL on) is **denied**.
Both PUBLISH and SUBSCRIBE denials are **logged** (never silently dropped).

### Wiring

The two sides enforce at the point where each one's outcome is decided, both
through the same `mqtt_acl_check()` helper:

- **PUBLISH** — `C_PROT_MQTT2` decides the publish outcome, so it asks the broker
  over a **direct** `EV_MQTT_ACL_CHECK` event (`0` = allowed / `-1` = denied) —
  a direct `gobj_send_event`, not a publish (the published path runs through the
  intermediate `C_CHANNEL` / `C_IOGATE`, which don't carry the event). A denied
  publish is rejected with `MQTT_RC_NOT_AUTHORIZED`. If no `C_MQTT_BROKER`
  service is found the protocol **fails open** (allows) with a warning.
- **SUBSCRIBE** — the per-topic SUBACK reason is built in the broker's
  `ac_mqtt_subscribe`, so the check lives there (alongside the existing
  `deny_subscribes` gate), calling `mqtt_acl_check(..., "read")` directly per
  requested filter. A denied filter is **not** added and its SUBACK reason is
  `MQTT_RC_NOT_AUTHORIZED` (v5) / `0x80` (v3.x); the rest of the SUBSCRIBE
  succeeds (per-topic, as MQTT requires).

```{note}
The A/B/C model choice and a future default-deny flip remain a deployment
decision — see `TODO.md` § *Security: MQTT broker — open follow-ups* (F-005).
```

### Authoring an ACL

```bash
# 1. Turn enforcement on for the broker (config attr, then restart the yuno):
#      "enable_acl": true   (SDF_WR|SDF_PERSIST)

# 2. Give a group publish/subscribe patterns (broker's own treedb):
ycommand -c 'command-yuno id=<yuno> service=<mqtt_service> command=update-node \
    topic_name=client_groups \
    record={"id":"g_limited","publish_acl":["sensors/+/up"],"subscribe_acl":["sensors/+/cmd"]} \
    options={"create":1}'

# 3. Link the client to the group (clients.client_groups fkey):
ycommand -c 'command-yuno id=<yuno> service=<mqtt_service> command=link-nodes \
    hook=client_groups parent_topic=client_groups parent_id=g_limited \
    child_topic=clients child_id=<client_id>'
```

A group with empty `publish_acl`/`subscribe_acl` keeps allow-all for that access
(useful for an unrestricted `g_open` group while others are constrained).

Regression test: [`tests/c/c_mqtt/acl`](https://github.com/artgins/yunetas/blob/7.7.2/tests/c/c_mqtt/acl)
(`main_acl.c` + `c_acl.c`) drives `EV_MQTT_ACL_CHECK` at an embedded broker —
the exact path the protocol gate uses — across matching/non-matching publish and
subscribe, `#` wildcard (incl. the parent level), the two allow-all fallbacks,
and the unknown-client deny.

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
