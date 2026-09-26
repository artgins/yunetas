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
delivered to the broker's `__input_side__` as channels. Each channel runs a
`C_PROT_MQTT2` instance. The same `C_PROT_MQTT2` gclass also works as an MQTT
*client* (it carries client-side attrs like `mqtt_client_id`, `mqtt_protocol`,
will/keepalive).

## Protocol support

- MQTT **v5.0** (default), **v3.1.1** and **v3.1** — negotiated per CONNECT.
- QoS **0 / 1 / 2**, capped per-listener by `max_qos`.
- Retained messages (`retain_available`), Last Will & Testament (incl. v5
  will-delay / message-expiry properties).
- Keepalive / PING, clean vs persistent sessions, **session expiry interval**
  (v5, and also enforced for v3.x through `mqtt_session_expiry_interval`).
- Normal and **shared subscriptions** (`$share`).
- Flow control: in-flight and queued-message limits, `message_size_limit`,
  `max_packet_size`.

## Persistence

When `mqtt_persistent_db` is true (default), the broker keeps state in a TreeDB
(`treedb_mqtt_broker`, schema [`treedb_schema_mqtt_broker.c`](https://github.com/artgins/yunetas/blob/7.25.7/modules/c/mqtt/src/treedb_schema_mqtt_broker.c)) plus TimeRanger2
queues:

- **clients / sessions / subscriptions** — TreeDB nodes.
- **retained messages** — TreeDB topic `retained_msgs`.
- **inflight / queued messages** — TimeRanger2 queues (per client).

Persistent sessions survive broker restarts. `clean_session` clients are
transient. Set `mqtt_persistent_db=0` for an in-memory-only broker.

The queues' tranger is opened with `on_critical_error` `LOG_OPT_EXIT_ZERO`: a
CRITICAL of timeranger2 ends the broker. The periodic backup of a queue
(`backup_queue_size` of `C_PROT_MQTT2`) is the exception: a backup whose new
topic cannot be created (no space, a `mkdir` that fails) moves the backup back
and the queue goes on in its topic, not backed up; the CRITICAL of the failed
create is logged and does not exit, so the messages of the queue never stay
in `<queue>.bak` (see [`tranger2_backup_topic()`](#tranger2_backup_topic)).
A queue that could not open its topic again after a failed backup takes it
again as soon as it can be opened (see [`trq_check_backup()`](#trq_check_backup)).

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
| `on_critical_error` | `2` | `LOG_OPT_EXIT_ZERO` (exit, no auto-restart) on error |

Session limits are from [`C_AUTHZ`](#gclass-c-authz) (for example `Authz.max_sessions_per_user`,
default 4 in `main.c`).

Per-connection limits live on `C_PROT_MQTT2` and apply to each client:

| Attribute | Default | Purpose |
|-----------|---------|---------|
| `max_qos` | `2` | Max QoS allowed for connecting clients |
| `retain_available` | `true` | Allow retained messages (else RETAIN clients are dropped) |
| `max_inflight_messages` | `20` | Outgoing QoS 1/2 in flight (1 = strict in-order, 0 = no maximum of the broker's own: the client's Receive Maximum still applies) |
| `max_inflight_bytes` | `0` | Byte cap on in-flight messages (0 = no limit) |
| `max_queued_messages` | `1000` | Per-client queue depth above in-flight (0 = unlimited) |
| `max_queued_bytes` | `0` | Per-client queue byte cap (0 = no limit) |
| `message_size_limit` | `0` | Max publish payload accepted (0 = MQTT max) |
| `max_packet_size` | `0` | Max MQTT packet (v5 advertises it, 0 = no limit) |

`max_inflight_messages` also bounds the INCOMING QoS 2 messages that wait for
their PUBREL (the receive maximum the broker advertises): a client that sends
more is disconnected (`RECEIVE_MAXIMUM_EXCEEDED`). When a persistent session is
reloaded with more such messages than that (the limit was lowered between two
connections), the ones beyond it wait QUEUED, and each goes in flight when a
PUBREL frees a slot, as mosquitto does: with ITS OWN packet id, the client's,
and its PUBREC is sent again then. Up to 7.25.4 the broker moved a queued one
when the in-flight list was NOT empty (the quota test was inverted), gave it a
NEW packet id and sent the PUBREC with it: the PUBREL of the client for its own
id found nothing (*"Message not found"*), and the message was never released
to its subscribers. `tests/c/c_mqtt` (`test_mqtt_queued_in`).

The queues of `C_PROT_MQTT2` (`tr2q_mqtt`) are the same on both sides, the
broker's and a client's (a client with `tranger_queues`). The messages above
`max_inflight_messages` are QUEUED: on disk, without their content in memory.

- **A message that expires before it is sent gives its slot to the next
  one.** An outgoing message with `expiry_interval` is checked when it goes in
  flight: expired, it is discarded, and the queued messages go in flight while
  there is room and are sent. Up to 7.25.4 nothing moved after an expiry: the
  queued messages waited for more traffic that might never come.
- **No backup while messages are queued.** `tr2q_check_backup()` is called
  every second when nothing is in flight; with messages still queued it does
  nothing and answers `0`. A backup re-creates the topic empty, and a queued
  message keeps only the rowid of its record: after a backup its content could
  not be read any more (*"No message content in queue entry"*, the message
  lost). Up to 7.25.4 the backup was made.
- **A queued message goes in flight with its content.**
  `tr2q_move_from_queued_to_inflight()` reads the content FIRST; when it
  cannot, the message stays queued, with an ERROR *"Cannot load the content of
  a queued message: it stays queued"*, and `-1`. Up to 7.25.4 it was moved in
  flight first: an incoming QoS 2 message then stayed in flight with no content
  and no PUBREC, and nobody completed it.
- **A QoS 2 message received again with DUP=1** (its PUBREC was lost) replaces
  the copy that waits for its PUBREL, found by its PACKET ID in flight or
  queued, with a WARNING *"QoS 2 message received again (dup): it replaces the
  copy waiting for its PUBREL"*: it is delivered once. Up to 7.25.4 a client
  searched the copy by the rowid of its queue record: the old copy stayed for
  ever, and the PUBREL of the next message with that packet id delivered the
  stale copy again (an unrelated message whose rowid equalled the id was
  removed instead). A client also logged *"QoS mismatch"* at every QoS 2
  PUBREL (the flag bits of the message were compared with the qos level).

```text
raw broker -> client                      client -> raw broker
PUBLISH 5 'a' (QoS 2)                     PUBREC 5
PUBLISH 5 'a' (QoS 2, DUP)                PUBREC 5      the copy is replaced
PUBREL 5                                  PUBCOMP 5     'a' delivered once
PUBLISH 5 'b' (QoS 2)                     PUBREC 5
PUBREL 5                                  PUBCOMP 5     'b' delivered (7.25.4: 'a' again)
```

`tests/c/c_mqtt` (`test_mqtt_client_queues`: a client against a raw broker)
and `tests/c/tr_queue` (`test_tr2q_queued`).

**The out window is the lower of the two maximums.** A MQTT 5 client says in
its CONNECT how many QoS 1/2 messages it takes unacknowledged (Receive
Maximum). The broker sends at most the lower of that and its own
`max_inflight_messages`; the others wait queued, and each ack sends the next.
With `max_inflight_messages` 0 (no maximum of the broker's own) the client's
Receive Maximum is the limit [MQTT-3.3.4-9]. Up to 7.25.4 the broker checked
only its OWN maximum: with it at 0 it sent every queued message at once, over
the client's Receive Maximum, and the client disconnected (0x93, *Receive
Maximum exceeded*).

```text
max_inflight_messages 0, client Receive Maximum 2, four QoS 1 messages for it:
broker -> PUBLISH 1, PUBLISH 2          (3 and 4 wait)
client -> PUBACK 1, PUBACK 2
broker -> PUBLISH 3, PUBLISH 4
```

**An ack of the wrong kind is a protocol error, not a delivery.** A PUBACK is
the ack of QoS 1, PUBREC/PUBCOMP of QoS 2. An ack of the other QoS (a PUBCOMP
of a QoS 1 message, a PUBACK of a QoS 2 one), or a PUBCOMP of a QoS 2 message
still waiting for its PUBREC, is the PEER's error: a WARNING (`MSGSET_MQTT`,
with `client_id`, `peername`, `mid`, `msg_qos`, `expected_qos`), *"QoS
mismatch"* or *"Unexpected message state for QoS 2"*, the message STAYS in
flight, and the broker answers a MQTT 5 client with DISCONNECT 0x82
(*Protocol Error*) and closes -- as mosquitto (`MOSQ_ERR_PROTOCOL`). The
message is sent again at the next session. An ack of an unknown packet id is a
WARNING too, *"Message not found in trq_out_msgs"*, and nothing else. Up to
7.25.4 the three were ERRORs, and the first two removed the message as
delivered: its QoS 2 exchange was skipped.

```text
broker -> PUBLISH 4 (QoS 1)
client -> PUBCOMP 4          WARNING "QoS mismatch", msg_qos 1, expected_qos 2
broker -> DISCONNECT 0x82    message 4 still pending
```

`tests/c/c_mqtt` (`test_mqtt_out_flight`: a raw MQTT 5 client against the
broker).

(mqtt-acl)=
## Authorization (publish/subscribe ACL)

Authentication (who the client is) is handled by [`C_AUTHZ`](#gclass-c-authz) —
see [Authentication, authorization, and TLS](../../../yunos/c/yuno_agent/YUNO_AUTH.md). Authorization of
*which topics a client can use* is a separate, broker-local mechanism: a
**per-group topic ACL** stored in the broker's own TreeDB.

The model (model A — group-based ACL in the treedb):

- The `client_groups` topic
  ([`treedb_schema_mqtt_broker.c`](https://github.com/artgins/yunetas/blob/7.25.7/modules/c/mqtt/src/treedb_schema_mqtt_broker.c))
  carries two array columns: **`publish_acl`** and **`subscribe_acl`**, each a
  list of MQTT topic-filter patterns (`+` / `#` wildcards).
- A `clients` row fkeys to one or more `client_groups`. A client is allowed an
  access if **any** of its groups has a pattern matching the topic, using the
  same `topic_matches_sub()` wildcard logic the broker uses for routing.

### Posture (backward-compatible, default off)

Enforcement is gated by the `enable_acl` attr on `C_MQTT_BROKER` (default
`false`). The ACL helper allows when:

- `enable_acl` is **off** (default) — no enforcement, allow-all. OR
- the client's groups define **no patterns** for that access — allow-all
  (so adding the columns does not break an existing broker until ACLs are
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
  intermediate `C_CHANNEL` / `C_IOGATE`, which do not carry the event). A denied
  publish is rejected with `MQTT_RC_NOT_AUTHORIZED`. If no `C_MQTT_BROKER`
  service is found the protocol **fails open** (allows) with a warning.
- **SUBSCRIBE** — the per-topic SUBACK reason is built in the broker's
  `ac_mqtt_subscribe`, so the check lives there (alongside the existing
  `deny_subscribes` gate), calling `mqtt_acl_check(..., "read")` directly per
  requested filter. A denied filter is **not** added and its SUBACK reason is
  `MQTT_RC_NOT_AUTHORIZED` (v5) / `0x80` (v3.x). The rest of the SUBSCRIBE
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

Regression test: [`tests/c/c_mqtt/acl`](https://github.com/artgins/yunetas/blob/7.25.7/tests/c/c_mqtt/acl)
(`main_acl.c` + `c_acl.c`) drives `EV_MQTT_ACL_CHECK` at an embedded broker —
the exact path the protocol gate uses — across matching/non-matching publish and
subscribe, `#` wildcard (incl. the parent level), the two allow-all fallbacks,
and the unknown-client deny.

## Commands

| Command | Description |
|---------|-------------|
| `list-channels` | Input channels of connected devices |
| `list-sessions` | Active/persistent sessions |
| `list-queues` | Per-client message queues: the names, or with `queue=<name>` the messages of one (`level=0..3`, `pending=1`, `qos=`; the conditions add up: `qos=1` lists the PENDING QoS 1 messages, `pending=0 qos=1` the delivered ones) |
| `normal-subs` / `shared-subs` | List normal / shared (`$share`) subscribers |
| `flatten-subs` | Flattened subscriber view |
| `list-retains` / `remove-retains` | List / remove retained messages (note: `#` shown as `/`) |
| `clean-queues` | Drop non-persistent, not-in-use sessions and their queues |
| `authzs` | Authorization help |
| `help` | Command help |

`list-queues queue=<name>` of a queue that exists and cannot be opened (its
`keys/` cannot be listed) answers `-1`, *"cannot open the queue '\<name>', see
the log"*; one whose messages cannot all be read answers `-1` with the messages
it read, *"the list is PARTIAL"*. Up to 7.25.4 both answered `0`: an empty, or
a short, queue. A store whose topics cannot be listed answers `-1` for
`list-queues` (*"cannot list the topics of the store, see the log"*) and
`clean-queues` (*"cannot list the topics of the store, nothing cleaned, see the
log"*); the cause (`errno`) is in the ERROR *"Cannot list the topics of the
store"* that the log carries. Up to 7.25.4 both answered an empty list with
`0`.

```bash
ycommand -c 'command-yuno id=<id> service=mqtt_broker command=list-queues queue=client1 level=3'
ycommand -c 'command-yuno id=<id> service=mqtt_broker command=list-queues queue=client1-OUT qos=1'             # the QoS 1 messages still to deliver
ycommand -c 'command-yuno id=<id> service=mqtt_broker command=list-queues queue=client1-OUT pending=0 qos=1'   # the QoS 1 messages delivered
```

Up to 7.25.4 `qos=` replaced the condition of `pending=1` (the default): it
listed the delivered messages of that QoS too, a backlog that did not exist.

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
