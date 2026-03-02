# MQTT Retain Usage

## Overview

This document covers all retain-related functionality in MQTT protocol, including differences between v3.1.1 and v5.

---

## Retain in MQTT Packets

### PUBLISH

**Retain flag** (bit 0, fixed header): Tells broker to store message as the retained message for that topic.

### CONNECT (Will Message)

**Will Retain** flag: If set, the Will Message is published as a retained message when the client disconnects unexpectedly.

- MQTT v3.1.1: Bit 5 of Connect Flags byte
- MQTT v5: Same location

### SUBSCRIBE (MQTT v5 only)

Two retain-related options in the Subscription Options byte:

- **Retain As Published** (bit 3): When set, broker forwards messages with their original retain flag value. When clear, broker sets retain=0 on forwarded messages.

- **Retain Handling** (bits 4-5):
  - `0` = Send retained messages at subscribe time
  - `1` = Send retained messages only if subscription is new
  - `2` = Don't send retained messages at subscribe time

### CONNACK (MQTT v5 only)

**Retain Available** property (0x25): Server indicates whether it supports retained messages. If `0`, client must not send PUBLISH with retain=1.

### Summary Table

| Packet      | Retain usage                              |
|-------------|-------------------------------------------|
| PUBLISH     | Retain flag                               |
| CONNECT     | Will Retain                               |
| SUBSCRIBE   | Retain Handling, Retain As Published (v5) |
| CONNACK     | Retain Available property (v5)            |

No retain-related options in: UNSUBSCRIBE, DISCONNECT, PUBACK, PUBREC, PUBREL, PUBCOMP, SUBACK, UNSUBACK.

---

## Retained Messages vs Client Sessions

Retained messages and client sessions are **independent concepts**.

### Retained Messages

Stored **per-topic at broker level**, completely independent of any client session.

Persist until:

- Replaced by a new retained message on the same topic
- Deleted by publishing an empty payload with retain=1
- Broker restarts (unless broker has persistence enabled)

They exist even if **no clients are connected**.

### Session State (Clean Session / Clean Start)

Stored **per-client**, contains:

- Client subscriptions
- QoS 1/2 messages pending delivery
- QoS 2 messages awaiting completion
- Will message (while connected)

### The Interaction

When a client subscribes (and Retain Handling allows it), the broker sends any existing retained message for matching topics. But:

- The retained message **stays in the broker's retain store** after delivery
- If another client subscribes later, it also receives the same retained message
- The retained message doesn't "belong" to any session

### Broker Architecture

Two separate stores are needed:

```
Broker
├── retain_store        (topic → message)  ← global, persistent
└── sessions
    ├── client_A        (subscriptions, pending msgs)
    └── client_B        (subscriptions, pending msgs)
```

The retain store is essentially a key-value store indexed by topic, while sessions are indexed by client_id.

---

## Retained Message Lifetime

### MQTT v3.1.1 Behavior

Retained messages persist **indefinitely** (live forever). The only ways to remove them:

- Publish empty payload with retain=1 to that topic
- Broker restart (if no persistence)
- Manual admin intervention

This can lead to **stale data accumulation** over time.

### MQTT v5 Solution: Message Expiry Interval

**Message Expiry Interval** property (0x02) in PUBLISH:

- Value in seconds
- Broker deletes the message when it expires
- When forwarding to subscribers, broker sends the **remaining** expiry time

Example:

```
PUBLISH
  Topic: sensor/temperature
  Retain: 1
  Properties:
    Message Expiry Interval: 3600   ← expires in 1 hour
  Payload: "22.5"
```

If no client subscribes within 3600 seconds, the retained message is automatically deleted.

### Implementation Consideration

For a broker implementation, the retain store should track:

```c
typedef struct {
    const char *topic;
    gbuffer_t *payload;
    uint8_t qos;
    uint32_t expiry_interval;    // 0 = never expires (v3.1.1 behavior)
    time_t stored_at;            // to calculate remaining time
} retained_msg_t;
```

Then periodically purge expired messages, or check expiry on access.

---

## Summary

- Without Message Expiry Interval, retained messages are essentially **immortal** — a design limitation of v3.1.1 that v5 addressed.
- Retained messages are global to the broker, not tied to any client session.
- MQTT v5 provides much better control over retain behavior through Subscription Options and Message Expiry.





# MQTT Retained Messages vs Persistent Sessions

## Scenario

1. client1 connects to the broker with permanent session (`clean_start=0`)
2. client1 subscribes to `#` topic
3. client1 disconnects
4. client2 connects and publishes a message with `retain=1`
5. client1 reconnects

**Question:** Will client1 receive the retained message when it reconnects?

---

## The Answer

**It depends on QoS, not on the retain flag.**

---

## The Key Distinction

Retained messages are delivered **at SUBSCRIBE time**, not at CONNECT time.

When client1 reconnects with `clean_start=0`:

- Broker finds existing session with `#` subscription
- Client1 does **not** re-subscribe
- Therefore, **no retained message delivery is triggered**

---

## What Actually Happens

When client2 publishes with `retain=1`, two things occur:

1. Message is **stored** in retain store (for future subscribers)
2. Message is **delivered** to all current matching subscribers

Since client1 has an active subscription in its persistent session:

| Publish QoS | Result for disconnected client1 |
|-------------|--------------------------------|
| QoS 0       | Message lost — not queued |
| QoS 1       | Message queued — delivered on reconnect |
| QoS 2       | Message queued — delivered on reconnect |

---

## So the Answer

- **QoS 0 publish**: client1 receives **nothing** on reconnect
- **QoS 1/2 publish**: client1 receives the message as a **queued message** (not because of retain, but because of the active subscription + QoS guarantee)

---

## If client1 Wants the Retained Message

client1 would need to re-subscribe after connecting. In MQTT v5, it could use:

```
SUBSCRIBE
  Topic: #
  Retain Handling: 0  (send retained messages)
```

But this creates a potential duplicate if the message was also queued.

---

## Summary

The **retain flag** ensures the message is stored for **future subscribers**. It doesn't automatically deliver to reconnecting clients with existing sessions — that's the job of **QoS and message queuing**.

| Mechanism | Purpose |
|-----------|---------|
| Retain flag | Store message for future subscribers |
| QoS 1/2 + Session | Queue messages for disconnected clients with active subscriptions |

These are **independent mechanisms** that can work together but serve different purposes.
