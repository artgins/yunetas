# MQTT “Will” information (Last Will and Testament)

**What it is:**  
A *Will message* is an application message that a client asks the broker to publish **on its behalf** if the client disconnects **unexpectedly** (e.g., network drop, keepalive timeout). It’s set during `CONNECT`.

**Why it’s used:**
- Signal liveness/health (`status/device123 = offline`)
- Trigger failover/recovery logic
- Notify other clients of abrupt shutdowns

**When it’s sent vs. not sent**
- **Sent:** connection loss, keepalive timeout, crash, unclean drop.
- **Not sent:** client ends with a clean `DISCONNECT`, or the broker closes the old session normally (e.g., takeover by a new connection using the same ClientID).

---

## Fields (what you provide on CONNECT)

### MQTT 3.1.1
- **Will Topic**
- **Will Payload** (opaque bytes)
- **Will QoS** (0/1/2)
- **Will Retain** (true/false)

### MQTT 5.0 (adds properties)
- **Will Delay Interval** (publish only after N seconds; canceled if the client reconnects cleanly before the delay elapses)
- **Message Expiry Interval**
- **Content Type**, **Payload Format Indicator** (e.g., UTF-8)
- **Response Topic**, **Correlation Data**
- **User Properties** (key/value metadata)

---

## Mosquitto CLI examples

**Observe will publications**
```bash
    mosquitto_sub -t 'status/#' -v
```

# How the MQTT “Will” relates to topics

- **Exactly one topic per connection.**  
  A Will is a single PUBLISH the broker will emit **once** if the client disconnects uncleanly. It has **one** `Will Topic` (no wildcards) plus QoS/retain (and properties in MQTT 5). You can’t attach a Will to multiple topics.

- **Topic must be a topic name (no `+`/`#`).**  
  Because it’s a publish, the Will topic cannot contain wildcards. Wildcards are only for **subscriptions**.

- **Delivery uses normal topic matching.**  
  When the broker publishes the Will to that topic, **any** subscribers whose filters match (including wildcards or shared subscriptions) will receive it. If `retain=true`, it also becomes/updates the retained message on that topic.

- **Per-connection, not per-topic.**  
  The Will is bound to the client session/connection. If you need “offline” signals for several topics, typical patterns are:
  - Use **one canonical status topic per client**, e.g. `status/<clientId> = online/offline`, and let subscribers use wildcards.
  - Run **multiple logical clients** (distinct ClientIDs), each with its own Will.
  - Use broker-side logic (plugin/bridge) to fan out if truly necessary.

- **When it fires (and not).**  
  Fires on network loss/unclean drop/keepalive timeout.  
  Not sent on clean `DISCONNECT` (unless MQTT 5 uses “Disconnect with Will Message”).

---

## Mosquitto C API quick refs

**MQTT 3.1.1**

```c
struct mosquitto *m = mosquitto_new("device123", true, NULL);

/* topic: no wildcards; payload: arbitrary bytes */
int rc = mosquitto_will_set(
    m,
    "status/device123",              /* topic */
    (int)strlen("offline"),          /* payloadlen */
    "offline",                       /* payload */
    1,                               /* qos */
    true                             /* retain */
);

rc = mosquitto_connect(m, "broker", 1883, 30);
```

**MQTT 5.0 (with Will Delay)**

```c
struct mosquitto *m = mosquitto_new("device123", true, NULL);

mosquitto_property *props = NULL;
mosquitto_property_add_int32(&props, MQTT_PROP_WILL_DELAY_INTERVAL, 30);

int rc = mosquitto_will_set_v5(
    m,
    "status/device123",
    (int)strlen("offline"),
    "offline",
    1,           /* qos */
    true,        /* retain */
    props
);

rc = mosquitto_connect_bind_v5(m, "broker", 1883, 30, NULL, NULL);
```

## Key takeaways

One Will per connection, one concrete topic.

Subscribers use wildcard filters to “fan-in.”

Retain makes the Will act as the last-known state for late joiners.
