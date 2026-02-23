# Complete MQTT Protocol Guide - Q&A Format

## Table of Contents

1. [The Retain Flag in MQTT](#1-the-retain-flag-in-mqtt)
2. [Retain Flag vs Last Will and Testament](#2-retain-flag-vs-last-will-and-testament)
3. [MQTT Subscriptions and Reconnection](#3-mqtt-subscriptions-and-reconnection)
4. [Re-subscribing is Idempotent](#4-re-subscribing-is-idempotent)
5. [Messages During Disconnection](#5-messages-during-disconnection)
6. [Broker Resource Exhaustion with Persistent Sessions](#6-broker-resource-exhaustion-with-persistent-sessions)
7. [The AUTH Command in MQTT 5.0](#7-the-auth-command-in-mqtt-50)
8. [Standard AUTH Methods](#8-standard-auth-methods)
9. [Maximum Payload Size in MQTT](#9-maximum-payload-size-in-mqtt)
10. [MQTT Protocol Maximum - Variable Length Encoding](#10-mqtt-protocol-maximum---variable-length-encoding)
11. [QoS Message Persistence During Disconnections](#11-qos-message-persistence-during-disconnections)
12. [UNSUBSCRIBE and Reason Codes](#12-unsubscribe-and-reason-codes)
13. [When Does Broker Send PUBACK](#13-when-does-broker-send-puback)
14. [Mosquitto Client Library Callback Behavior](#14-mosquitto-client-library-callback-behavior)
15. [PUBACK Means Broker Receipt, Not Subscriber Delivery](#15-puback-means-broker-receipt-not-subscriber-delivery)
16. [Publisher Clean Session and Subscriber Queues](#16-publisher-clean-session-and-subscriber-queues)
17. [One Client ID = One Session](#17-one-client-id--one-session)
18. [Protocol-Defined $ Topics](#18-protocol-defined--topics)
19. [Client Storage Requirements for QoS > 0](#19-client-storage-requirements-for-qos--0)

---

## 1. The Retain Flag in MQTT

**Q: Explain me the retain flag in MQTT**

The **retain flag** in MQTT is a message property that tells the broker to store the last message published on a topic and automatically send it to new subscribers.

### How it works

When you publish a message with the retain flag set to true:

1. The MQTT broker stores that message along with its topic
2. Any client that subscribes to that topic in the future immediately receives the retained message, even if they weren't connected when it was originally published
3. Each topic can have only one retained message - publishing a new retained message replaces the previous one

### Common use cases

**State information**: Retained messages are perfect for publishing the current state of devices. For example, a temperature sensor might publish its current reading with the retain flag. New subscribers instantly see the latest temperature without waiting for the next update.

**Configuration data**: Settings or configuration parameters that clients need immediately upon connection.

**Last known status**: Device online/offline status or "last will" messages often use the retain flag so monitoring systems can quickly see device states.

### Clearing retained messages

To remove a retained message from a topic, publish an empty message (zero-length payload) with the retain flag set to true on that topic. The broker will delete the stored retained message.

### Important notes

- Retained messages persist on the broker until explicitly cleared or replaced
- Not all messages should be retained - only use it for state information that makes sense to deliver to late subscribers
- The retain flag is independent of QoS (Quality of Service) levels
- Some brokers may have limits on retained message storage

Think of it like a bulletin board: the retained message is the note pinned to the board that anyone can read when they walk by, rather than a message shouted once into a room.

---

## 2. Retain Flag vs Last Will and Testament

**Q: It's like the Will message for disconnecting, isn't it?**

Not quite - they're related but serve different purposes!

### Key differences

**Retain flag**: You explicitly publish a message with retain=true whenever you want. It stores your *current state* that you're actively reporting. For example, a sensor publishes "temperature: 25°C" with retain flag, and new subscribers immediately see this value.

**Last Will and Testament (LWT)**: This is a message the broker automatically publishes *on your behalf* when you disconnect unexpectedly. You configure it when connecting to the broker, but you don't control when it's sent - the broker does that when it detects your abnormal disconnection.

### How they work together

They're often used in combination! A common pattern is:

1. When a device connects, it publishes `device/status: "online"` with retain=true
2. It also sets a Last Will message of `device/status: "offline"` with retain=true
3. If the device disconnects gracefully, it publishes `device/status: "offline"` with retain=true itself
4. If the device crashes or loses connection unexpectedly, the broker publishes the LWT message `device/status: "offline"` with retain=true

This way, any new subscriber to `device/status` will immediately see whether the device is currently online or offline, regardless of when they subscribe.

So the Last Will is about *what happens when you disconnect*, while the retain flag is about *storing the last message for future subscribers*. They complement each other nicely for status tracking!

---

## 3. MQTT Subscriptions and Reconnection

**Q: The subscribe messages must be sent each time a client connecting or are permanents?**

**Subscribe messages must be sent each time a client connects** - they are not permanent on the broker.

### What happens on reconnection

Every time an MQTT client connects to the broker:

1. The connection is established
2. The client must send SUBSCRIBE messages again for all the topics it wants to receive
3. The broker starts forwarding messages from those topics to the client

The broker does **not** remember your subscriptions between connections. This is by design in the MQTT protocol.

### Why this matters with retained messages

This is actually why retained messages are so useful! When your client:

1. Disconnects (intentionally or due to network issues)
2. Reconnects to the broker
3. Re-subscribes to topics

It immediately receives all the retained messages on those topics, giving it the current state without waiting for new publishes.

### Persistent sessions (Clean Session flag)

There's one exception: if you connect with the **Clean Session flag set to false** (or Session Expiry Interval > 0 in MQTT 5.0), the broker can maintain:

- Your subscriptions
- Undelivered QoS 1 and QoS 2 messages

But this requires you to:
- Use the same Client ID each time
- Set the appropriate session flags
- Have a broker that supports persistent sessions

Even with persistent sessions, many clients still explicitly re-subscribe on connection for clarity and to ensure they have the subscriptions they need.

So in most typical implementations: yes, you re-subscribe every time you connect.

---

## 4. Re-subscribing is Idempotent

**Q: Re-subscribe are idempotent so...**

Exactly! **Re-subscribing is idempotent** - you can subscribe to the same topic multiple times without any negative effects.

### What happens when you re-subscribe

If you subscribe to a topic you're already subscribed to:

- The broker simply acknowledges the subscription (sends a SUBACK)
- Your existing subscription continues working normally
- You don't receive duplicate messages
- The QoS level gets updated if you specified a different one

### Practical implications

This idempotency makes client code simpler and more robust:

```
// You can safely do this without checking if already subscribed
client.subscribe("sensors/temperature");
client.subscribe("sensors/temperature");  // No problem!
```

**Common patterns that rely on this:**

1. **Reconnection logic**: Your client can blindly re-subscribe to all topics on every connection without worrying about duplication

2. **Multiple code paths**: Different parts of your application can subscribe to the same topic without coordination

3. **Retained messages**: You'll receive the retained message again when you re-subscribe (which is usually what you want - to get the current state)

### One caveat

While re-subscribing is safe, be aware that if there's a retained message on that topic, you'll receive it again each time you subscribe. This is usually fine and expected behavior, but your application should handle receiving the same retained message multiple times gracefully.

So yes, you're right - the idempotent nature of subscriptions makes MQTT robust and easy to work with!

---

## 5. Messages During Disconnection

**Q: What happen with messages published when a client was disconnected and connect again**

It depends on the **QoS level** and whether you're using **persistent sessions**:

### Without persistent sessions (Clean Session = true)

This is the default and most common case:

**QoS 0 (At most once)**: Messages published while you were disconnected are **lost forever**. You'll never receive them.

**QoS 1 and QoS 2**: Also **lost** if you don't have a persistent session. The broker doesn't queue messages for you.

**Retained messages**: When you reconnect and re-subscribe, you **do** receive the last retained message on each topic, giving you the current state.

### With persistent sessions (Clean Session = false)

If you connect with Clean Session = false and use the **same Client ID**:

**QoS 0**: Still lost - no queuing even with persistent sessions

**QoS 1 (At least once)**: The broker **queues** these messages while you're disconnected and delivers them when you reconnect

**QoS 2 (Exactly once)**: The broker **queues** these messages and ensures exactly-once delivery when you reconnect

**Retained messages**: You receive them as usual when you re-subscribe

### Practical example

```
Time 0: Client connects with Clean Session = true, subscribes to "sensors/#"
Time 1: Client disconnects
Time 2: Someone publishes "sensors/temp: 25°C" (QoS 1, no retain)
Time 3: Someone publishes "sensors/temp: 26°C" (QoS 1, retain=true)
Time 4: Client reconnects and re-subscribes

Result: Client receives only "26°C" (the retained message)
        The "25°C" message was lost
```

### Common pattern

Many applications combine these features:
- Use retained messages for state that must be known
- Use QoS 0 for real-time data where missing a few samples is acceptable
- Use persistent sessions + QoS 1/2 for critical messages that can't be lost

So the short answer: **messages are usually lost unless you use persistent sessions with QoS 1 or 2, but retained messages always give you the last known state**.

---

## 6. Broker Resource Exhaustion with Persistent Sessions

**Q: If clients send a lot of messages with qos > 0 and they don't reconnect, the broker can be collapsed**

**Absolutely yes!** This is a real operational concern with MQTT brokers and persistent sessions.

### The problem

When a client with a persistent session (Clean Session = false) disconnects:

1. The broker queues all QoS 1 and QoS 2 messages for that client
2. The queue keeps growing as long as messages are published to subscribed topics
3. If the client never reconnects, the queue never gets cleared
4. Memory usage grows indefinitely → broker can run out of memory and crash

### Broker protections

Most production MQTT brokers have safeguards:

**Session expiry**: Brokers can be configured to expire persistent sessions after a timeout (e.g., 1 hour, 24 hours). In MQTT 5.0, this is explicit with the Session Expiry Interval.

**Queue limits**: Many brokers limit the number of queued messages per client. Once the limit is reached:
- New messages may be dropped
- The session might be terminated
- Oldest messages might be discarded

**Memory limits**: Brokers can enforce per-client or total memory limits.

**Client takeover**: If a new client connects with the same Client ID, it typically takes over the session and clears the old queue.

### Best practices

**For clients:**
- Use Clean Session = true unless you truly need message persistence
- If using persistent sessions, reconnect regularly or use reasonable Session Expiry values
- Clean up (connect with Clean Session = true) when done

**For broker operators:**
- Configure session expiry timeouts
- Set per-client queue limits
- Monitor broker memory usage
- Consider using authentication to prevent malicious clients from creating many persistent sessions

**For system design:**
- Use persistent sessions only for critical clients
- Consider if you really need QoS > 0, or if retained messages + QoS 0 is sufficient
- For high-volume topics, avoid persistent sessions with QoS 1/2

So yes, this is a potential DoS vector and resource exhaustion issue that needs to be managed carefully in production systems!

---

## 7. The AUTH Command in MQTT 5.0

**Q: Command AUTH, explain me**

The **AUTH** packet in MQTT is used for **enhanced authentication** - it's a feature introduced in **MQTT 5.0** that enables more sophisticated authentication methods beyond simple username/password.

### Basic concept

AUTH allows for challenge-response authentication flows that require multiple round-trips between client and broker. Think of authentication methods like:

- SCRAM (Salted Challenge Response Authentication Mechanism)
- Kerberos
- OAuth tokens
- Custom authentication protocols

### How it works

The AUTH packet enables a **multi-step authentication handshake**:

1. **Client** sends CONNECT with an Authentication Method specified
2. **Broker** responds with AUTH (challenge)
3. **Client** responds with AUTH (response to challenge)
4. This can continue for multiple rounds
5. **Broker** finally sends CONNACK (success/failure)

Or the client can initiate re-authentication during an active connection.

### AUTH packet contents

- **Authentication Method**: A string identifying the auth mechanism (e.g., "SCRAM-SHA-256")
- **Authentication Data**: Binary data for the challenge or response
- **Reason Code**: Why this AUTH was sent
- **Reason String**: Human-readable explanation (optional)

### Use cases

**Enhanced security**: More secure than plaintext username/password, even over TLS

**Token-based auth**: OAuth or JWT tokens that can be refreshed

**Re-authentication**: A client can re-authenticate during a session without reconnecting (useful for token refresh)

**Mutual authentication**: Both client and broker can prove their identities

**Enterprise integration**: Support for corporate authentication systems like Kerberos or LDAP with challenge-response

### Example flow (SCRAM)

```
Client → Broker: CONNECT (Auth Method: "SCRAM-SHA-256")
Broker → Client: AUTH (Challenge: server nonce, salt, iteration count)
Client → Broker: AUTH (Response: proof based on password + challenge)
Broker → Client: CONNACK (Success + server proof)
```

### Important notes

- **MQTT 5.0 only** - not available in MQTT 3.1.1
- Both client and broker must support the same Authentication Method
- If either doesn't support enhanced auth, fall back to username/password in CONNECT
- The broker can reject the authentication at any AUTH step
- Re-authentication can happen mid-session to refresh credentials

### When to use it

- You need stronger security than username/password
- You're integrating with enterprise authentication systems
- You need token refresh without reconnecting
- Compliance requires specific authentication mechanisms

For most simple IoT applications, basic username/password over TLS is sufficient. AUTH is for more complex security requirements.

---

## 8. Standard AUTH Methods

**Q: What AUTH methods are standard?**

Actually, **MQTT 5.0 does not define any standard authentication methods** - it only provides the AUTH packet mechanism itself.

### The specification

The MQTT 5.0 specification:
- Defines the AUTH packet structure
- Describes the authentication flow
- Leaves the Authentication Method as a **UTF-8 string** that client and broker must agree upon
- Does **not** standardize any specific method names or implementations

### Common implementations

In practice, brokers and clients have implemented various methods, but there's no official registry. Commonly seen:

**SCRAM family**:
- `SCRAM-SHA-1`
- `SCRAM-SHA-256`
- `SCRAM-SHA-512`

These are based on RFC 5802 (SASL SCRAM) and are probably the most widely supported.

**OAuth/JWT**:
- Method names vary by implementation
- `OAUTH2`, `JWT`, or custom strings
- Each broker may implement differently

**Custom methods**:
- Broker-specific authentication schemes
- Integration with proprietary systems
- Method names chosen by the implementer

### The problem

This lack of standardization means:
- A client using `SCRAM-SHA-256` with one broker might not work with another
- No guaranteed interoperability between different MQTT implementations
- You need to check your specific broker's documentation

### Popular broker support

**Mosquitto**: Supports enhanced auth via plugins, but you need to implement or find plugins for specific methods

**HiveMQ**: Supports custom authentication via extensions/plugins

**EMQX**: Supports various methods through configuration and plugins

**AWS IoT Core**: Doesn't use AUTH packets - uses its own certificate-based authentication

### Practical reality

Most MQTT deployments still use:
- Username/password in CONNECT packet
- TLS client certificates
- Token-based auth passed as username/password

The AUTH packet mechanism exists but isn't widely adopted yet, partly due to this lack of standardization.

So to answer directly: **no standard methods are defined** - it's intentionally left open for implementations to define their own.

---

## 9. Maximum Payload Size in MQTT

**Q: What is the maximum size of payload in MQTT?**

The **maximum payload size in MQTT is 256 MB** (268,435,455 bytes to be exact).

### Technical details

This limit comes from the MQTT protocol specification itself:

- MQTT uses a variable-length encoding for the "Remaining Length" field in the packet header
- This encoding can represent values from 0 to 268,435,455 (256 MB - 1 byte)
- The Remaining Length includes the variable header + payload
- So the actual maximum payload is slightly less than 256 MB (after accounting for headers)

### Practical considerations

**Most brokers set much lower limits:**
- Mosquitto: Default is 100 MB, but configurable
- HiveMQ: Configurable, often set to a few MB
- EMQX: Configurable per listener
- AWS IoT Core: 128 KB maximum

**Why lower limits in practice:**

1. **Memory usage**: Large messages consume broker memory
2. **Network congestion**: Big messages can block the connection
3. **Processing time**: Large payloads take time to process
4. **DoS protection**: Prevents malicious clients from overwhelming the broker

### Best practices

**Keep messages small:**
- Ideally under 1 KB for IoT sensor data
- Under 10 KB for most use cases
- Use compression for larger data if needed

**For large data:**
- Split into multiple messages
- Use file transfer protocols (HTTP, FTP) and send only metadata via MQTT
- Use MQTT to coordinate, not to transfer bulk data
- Consider using QoS 0 for very frequent small messages

**Example of what to avoid:**
```
❌ Publishing a 50 MB video file via MQTT
✓ Publishing a URL/reference to the video file
```

So while the protocol allows up to 256 MB, practical deployments use much smaller limits, and good design keeps messages as small as possible.

---

## 10. MQTT Protocol Maximum - Variable Length Encoding

**Q: But what is the maximum size defined in the protocol? can be until length field of 5 bytes?**

You're absolutely right to question this! Let me clarify the **protocol specification** precisely.

### MQTT Protocol Maximum

The MQTT protocol uses a **variable-length encoding** for the "Remaining Length" field in the fixed header.

### Variable Length Encoding

- Each byte encodes 7 bits of data and 1 continuation bit
- **Maximum of 4 bytes** can be used (not 5!)
- This gives: 7 bits × 4 bytes = 28 bits of data

### Maximum value calculation

With 4 bytes:
- Maximum value = 2^28 - 1 = **268,435,455 bytes**
- This is approximately **256 MB**

### Why not 5 bytes?

The MQTT specification **explicitly limits** the Remaining Length field to a maximum of **4 bytes**. From the MQTT 3.1.1 and 5.0 specifications:

> "The maximum number of bytes in the Remaining Length field is four."

So even though the encoding scheme could theoretically continue, the protocol standard caps it at 4 bytes.

### Breaking down the 256 MB limit

```
Remaining Length (4 bytes max) = 268,435,455 bytes
This includes:
  - Variable Header (varies by packet type)
  - Payload

So actual maximum payload ≈ 256 MB minus headers
```

### Summary

- **Protocol limit**: 4 bytes for Remaining Length field (not 5)
- **Maximum value**: 268,435,455 bytes (~256 MB)
- **Actual payload**: Slightly less after subtracting headers

So no, it cannot be 5 bytes according to the MQTT specification - it's strictly limited to 4 bytes maximum.

---

## 11. QoS Message Persistence During Disconnections

**Q: What happens if the broker or the client disconnect unexpectedly, the pending messages with QoS 1,2 are lost?**

Great question! The answer depends on whether you're using **persistent sessions** and the specific scenario.

### With Persistent Sessions (Clean Session = false)

**Messages are NOT lost** - the MQTT protocol is designed to handle this:

#### If the CLIENT disconnects unexpectedly:

**QoS 1 messages:**
- Messages the client was sending to broker: Stored by client, will be resent on reconnect
- Messages broker was sending to client: Queued by broker, delivered when client reconnects
- If client hasn't sent PUBACK, broker will resend on reconnect

**QoS 2 messages:**
- The multi-step handshake (PUBLISH → PUBREC → PUBREL → PUBCOMP) resumes where it left off
- Both client and broker maintain state of in-flight messages
- Ensures exactly-once delivery even across disconnections

#### If the BROKER crashes/restarts:

This is more critical and depends on broker configuration:

**Without persistent storage:**
- ❌ All queued messages are LOST
- ❌ Session state is LOST
- Clients must reconnect and start fresh

**With persistent storage (broker configured to persist to disk):**
- ✅ Queued QoS 1 and QoS 2 messages are saved to disk
- ✅ Session state is preserved
- ✅ In-flight message state is preserved
- When broker restarts, it recovers sessions and delivers messages

### Without Persistent Sessions (Clean Session = true)

**Messages ARE lost:**
- No session state is maintained
- No message queuing
- When client reconnects, it starts with a clean slate

### Example scenarios

#### Scenario 1: Client crash with persistent session
```
1. Client subscribed to "sensors/temp" with QoS 1, persistent session
2. Client crashes
3. Broker receives 10 messages on "sensors/temp"
4. Broker queues all 10 messages
5. Client reconnects with same Client ID
6. ✅ Client receives all 10 queued messages
```

#### Scenario 2: Broker crash WITHOUT disk persistence
```
1. Client disconnected, has persistent session
2. Broker queuing 100 messages for that client (in RAM only)
3. Broker crashes
4. Broker restarts
5. ❌ All 100 queued messages are lost
6. Client reconnects but gets nothing
```

#### Scenario 3: Broker crash WITH disk persistence
```
1. Client disconnected, has persistent session
2. Broker queuing messages and writing to disk
3. Broker crashes
4. Broker restarts and loads state from disk
5. ✅ Client reconnects and receives queued messages
```

### Production considerations

**For critical systems:**
- Use persistent sessions (Clean Session = false)
- Configure broker with disk persistence
- Use QoS 1 or 2 for important messages
- Monitor broker health and implement failover

**Popular brokers with persistence:**
- **Mosquitto**: Use `persistence true` in config
- **HiveMQ**: Enterprise version has persistence
- **EMQX**: Supports persistence to disk
- **AWS IoT Core**: Managed service with built-in persistence

**Trade-offs:**
- Disk persistence adds latency
- Requires more storage
- Slower broker restart (must load from disk)
- But ensures message durability

### Summary Table

| Scenario | Clean Session = true | Clean Session = false (no broker persistence) | Clean Session = false (with broker persistence) |
|----------|---------------------|-------------------------------------------|------------------------------------------------|
| Client disconnects | ❌ Lost | ✅ Queued in broker RAM | ✅ Queued and persisted to disk |
| Broker crashes | ❌ Lost | ❌ Lost (RAM cleared) | ✅ Recovered from disk |

### Conclusion

**QoS 1/2 messages can survive unexpected disconnections, but only if you use persistent sessions AND the broker is configured with disk persistence for true durability.**

---

## 12. UNSUBSCRIBE and Reason Codes

**Q: Studying the protocol I see that the command unsubscribe doesn't return error if the topic or subscription pattern does not exist. There is no reason code or similar in suback.**

You're absolutely right! Let me clarify the behavior for both SUBSCRIBE and UNSUBSCRIBE:

### SUBACK (Subscribe Acknowledgment)

**SUBACK DOES include reason codes** to indicate success or failure for each subscription:

#### MQTT 3.1.1 Return Codes:
- `0x00` - Maximum QoS 0
- `0x01` - Maximum QoS 1
- `0x02` - Maximum QoS 2
- `0x80` - Failure

So the broker CAN tell you if a subscription failed (though not specifically why in 3.1.1).

#### MQTT 5.0 Reason Codes (more detailed):
- `0x00` - Granted QoS 0
- `0x01` - Granted QoS 1
- `0x02` - Granted QoS 2
- `0x80` - Unspecified error
- `0x83` - Implementation specific error
- `0x87` - Not authorized
- `0x8F` - Topic Filter invalid
- `0x91` - Packet Identifier in use
- `0x97` - Quota exceeded
- `0x9E` - Shared Subscriptions not supported
- `0xA1` - Subscription Identifiers not supported
- `0xA2` - Wildcard Subscriptions not supported

### UNSUBACK (Unsubscribe Acknowledgment)

**You're correct about UNSUBSCRIBE!**

#### MQTT 3.1.1:
- UNSUBACK has **NO reason codes at all**
- Just acknowledges receipt of UNSUBSCRIBE
- No way to know if the subscription existed or not
- Always succeeds (idempotent operation)

#### MQTT 5.0 (improved):
UNSUBACK now includes reason codes:
- `0x00` - Success
- `0x11` - No subscription existed
- `0x80` - Unspecified error
- `0x83` - Implementation specific error
- `0x87` - Not authorized
- `0x8F` - Topic Filter invalid
- `0x91` - Packet Identifier in use

### Why this design?

**UNSUBSCRIBE is intentionally idempotent:**
- Unsubscribing from a non-existent subscription is not an error
- It achieves the desired state: you're not subscribed
- Similar to how deleting a file that doesn't exist might not be an error in some systems
- Simplifies client logic - no need to track what you're subscribed to

**Practical implications:**
```
client.unsubscribe("sensors/temp");  // Works fine
client.unsubscribe("sensors/temp");  // Also works fine, no error
client.unsubscribe("never/subscribed/to/this");  // Still fine in MQTT 3.1.1
```

### Summary

- **SUBACK**: Always has reason codes (even in 3.1.1) to indicate subscription success/failure
- **UNSUBACK in MQTT 3.1.1**: No reason codes, always succeeds
- **UNSUBACK in MQTT 5.0**: Added reason codes, can tell you if subscription didn't exist
- **Design philosophy**: Unsubscribe is idempotent - the goal is to not be subscribed, whether you were before or not

So you're absolutely correct about MQTT 3.1.1 UNSUBACK having no error indication!

---

## 13. When Does Broker Send PUBACK

**Q: When does broker send CMD_PUBACK? not for qos = 0?**

Correct! **PUBACK is only sent for QoS 1 messages, never for QoS 0.**

### QoS Level Acknowledgments

#### QoS 0 (At most once)
- **No acknowledgment at all**
- Publisher sends PUBLISH
- Broker forwards to subscribers
- No PUBACK, no confirmation
- Fire and forget

```
Publisher → Broker: PUBLISH (QoS 0)
(no acknowledgment)
```

#### QoS 1 (At least once)
- **PUBACK is sent**
- Publisher sends PUBLISH
- Receiver sends PUBACK to acknowledge
- If no PUBACK received, publisher resends

```
Publisher → Broker: PUBLISH (QoS 1)
Broker → Publisher: PUBACK

Broker → Subscriber: PUBLISH (QoS 1)
Subscriber → Broker: PUBACK
```

#### QoS 2 (Exactly once)
- **No PUBACK - uses PUBREC/PUBREL/PUBCOMP instead**
- Four-step handshake for exactly-once delivery

```
Publisher → Broker: PUBLISH (QoS 2)
Broker → Publisher: PUBREC
Publisher → Broker: PUBREL
Broker → Publisher: PUBCOMP
```

### When PUBACK is sent

The broker sends PUBACK in two scenarios:

1. **When receiving QoS 1 from a publisher:**
   - Acknowledges it received and accepted the message
   - Publisher can now discard the message

2. **Subscribers send PUBACK to broker for QoS 1:**
   - When subscriber receives QoS 1 message from broker
   - Tells broker it received the message
   - Broker can stop trying to deliver it

### Summary Table

| QoS Level | Acknowledgment | Command |
|-----------|----------------|---------|
| QoS 0 | None | - |
| QoS 1 | Simple ACK | PUBACK |
| QoS 2 | Four-step handshake | PUBREC, PUBREL, PUBCOMP |

So yes, you're exactly right: **PUBACK is only for QoS 1, never for QoS 0**.

---

## 14. Mosquitto Client Library Callback Behavior

**Q: I mean, seems that mosquitto callback to client when send a publish to broker**

Ah, I understand now! You're talking about the **client library callback behavior**, not the protocol itself.

Yes, you're right - this is a **Mosquitto client library implementation detail**, not part of the MQTT protocol.

### The `on_publish` callback

In the Mosquitto C client library (libmosquitto), there's an `on_publish` callback:

```c
void mosquitto_publish_callback_set(
    struct mosquitto *mosq,
    void (*on_publish)(struct mosquitto *, void *, int)
);
```

**When this callback is triggered:**

- **QoS 0**: Called immediately after sending (local confirmation only, no network ACK)
- **QoS 1**: Called when PUBACK is received from broker
- **QoS 2**: Called when PUBCOMP is received from broker (end of handshake)

### Why this matters

For **QoS 0**, the callback fires right away because:
- There's no PUBACK in the protocol
- The library can't wait for broker confirmation (there is none)
- It just confirms "message was sent to network"
- Does NOT mean the broker received it

For **QoS 1/2**, the callback fires when:
- Actual protocol acknowledgment is received
- PUBACK (QoS 1) or PUBCOMP (QoS 2)
- Confirms the broker accepted the message

### Practical implications

```c
// QoS 0
mosquitto_publish(mosq, NULL, "topic", len, payload, 0, false);
// on_publish callback fires immediately (local only)

// QoS 1
mosquitto_publish(mosq, NULL, "topic", len, payload, 1, false);
// on_publish callback fires when PUBACK received from broker

// QoS 2
mosquitto_publish(mosq, NULL, "topic", len, payload, 2, false);
// on_publish callback fires when PUBCOMP received from broker
```

### Other client libraries

This behavior varies by implementation:

**Paho MQTT:**
- Similar behavior
- QoS 0 callbacks fire immediately
- QoS 1/2 wait for protocol acknowledgment

**MQTT.js (JavaScript):**
- Callbacks behave similarly
- QoS 0 resolves immediately
- QoS 1/2 wait for protocol ACK

### Summary

- **MQTT protocol**: No PUBACK for QoS 0
- **Mosquitto library**: Calls `on_publish` callback for QoS 0 anyway (local confirmation)
- **For QoS 1**: Callback waits for actual PUBACK from broker
- **For QoS 2**: Callback waits for PUBCOMP from broker

The library provides a unified callback interface, but the meaning differs by QoS level!

---

## 15. PUBACK Means Broker Receipt, Not Subscriber Delivery

**Q: When a client sends a publish message to broker with qos > 0, the PUBACK or PUBCOMP only means that is got by the broker, not sent to the subscribers, right?**

**Absolutely correct!** This is a very important distinction.

### What PUBACK/PUBCOMP Actually Means

When a **publisher** sends a message to the broker:

- **PUBACK (QoS 1)**: Only confirms the broker **received and accepted** the message
- **PUBCOMP (QoS 2)**: Only confirms the broker **completed the exactly-once delivery protocol**

**It does NOT mean:**
- ❌ Subscribers have received the message
- ❌ Subscribers even exist
- ❌ Message was successfully delivered to anyone
- ❌ Message will ever be delivered

### The Two-Stage Delivery Process

MQTT has **two independent delivery flows**:

#### Stage 1: Publisher → Broker
```
Publisher → Broker: PUBLISH (QoS 1)
Broker → Publisher: PUBACK
✅ Publisher's job is done
```

#### Stage 2: Broker → Subscribers (separate, independent)
```
Broker → Subscriber1: PUBLISH (QoS 1)
Subscriber1 → Broker: PUBACK

Broker → Subscriber2: PUBLISH (QoS 1)
Subscriber2 → Broker: PUBACK
```

These are **completely decoupled**. The broker sends PUBACK to the publisher as soon as it accepts the message, regardless of subscriber status.

### Scenarios That Illustrate This

#### Scenario 1: No subscribers exist
```
Publisher → Broker: PUBLISH "sensors/temp" (QoS 1)
Broker → Publisher: PUBACK ✅
(No subscribers exist - message is discarded)
```

#### Scenario 2: Subscriber offline with persistent session
```
Publisher → Broker: PUBLISH "sensors/temp" (QoS 1)
Broker → Publisher: PUBACK ✅
(Subscriber offline - message queued, will deliver later)
```

#### Scenario 3: Subscriber fails to process
```
Publisher → Broker: PUBLISH "sensors/temp" (QoS 1)
Broker → Publisher: PUBACK ✅

Broker → Subscriber: PUBLISH (QoS 1)
Subscriber → Broker: PUBACK ✅
(But subscriber's application crashes while processing)
```

### Why This Design?

**Scalability**: The publisher doesn't need to wait for all subscribers

**Decoupling**: Publisher and subscribers are independent

**Broker responsibility**: Once the broker accepts the message, it's the broker's job to deliver to subscribers according to their QoS subscriptions

**Publisher concern**: The publisher only cares that the broker got the message reliably

### If You Need End-to-End Confirmation

If you need to know that subscribers actually received and processed your message, you need to implement **application-level acknowledgments**:

```
Publisher publishes: "command/device123" → "reboot"

Device (subscriber):
  1. Receives message
  2. Processes command
  3. Publishes response: "response/device123" → "rebooted successfully"

Publisher:
  - Subscribes to "response/device123"
  - Waits for confirmation
  - Implements timeout if no response
```

### Summary

| Acknowledgment | Meaning |
|----------------|---------|
| PUBACK from broker to publisher | Broker received the message |
| PUBACK from subscriber to broker | Subscriber received the message |
| Application-level ACK | Subscriber processed the message successfully |

So yes, you're 100% right: **PUBACK/PUBCOMP only confirms broker receipt, NOT delivery to subscribers**. The broker handles subscriber delivery separately and independently.

---

## 16. Publisher Clean Session and Subscriber Queues

**Q: If a publisher publish messages with qos > 0, and there are subscribers offline, the messages are queued, right?. What happens if the publisher reconnect with clean session 1? those enqueued messages are cleaned or are sent when subscribers reconnect?**

Excellent question! This highlights an important distinction: **publisher session state** and **subscriber session state** are **completely independent**.

### The Answer

**The queued messages for subscribers are NOT cleaned** when the publisher reconnects with Clean Session = 1.

The publisher's Clean Session flag only affects the **publisher's own session state**, not the messages already accepted by the broker or the subscribers' queues.

### Why They're Independent

When a publisher publishes a message with QoS > 0:

1. **Publisher → Broker**: Message is sent with QoS 1 or 2
2. **Broker sends PUBACK/PUBCOMP**: Publisher's responsibility ends here
3. **Broker queues for offline subscribers**: This is now the **broker's responsibility**, tied to **subscriber sessions**, not publisher sessions

Once the broker sends PUBACK/PUBCOMP to the publisher, the message is "owned" by the broker and managed according to **subscriber session policies**.

### Step-by-Step Example

```
Time 1: Subscriber connects with Clean Session = 0
        Subscribes to "sensors/temp" with QoS 1

Time 2: Subscriber disconnects (still has persistent session)

Time 3: Publisher connects and publishes:
        "sensors/temp: 25°C" (QoS 1)
        Broker → Publisher: PUBACK ✅
        Broker queues message for offline subscriber

Time 4: Publisher disconnects

Time 5: Publisher reconnects with Clean Session = 1
        (Publisher's session is cleared)

        ❓ What happens to queued message?
        ✅ Message remains queued for subscriber!

Time 6: Subscriber reconnects
        ✅ Receives the queued "25°C" message
```

### What Clean Session = 1 for Publisher Does

When the **publisher** reconnects with Clean Session = 1:

**Clears publisher's session state:**
- ✅ Discards any unacknowledged PUBLISH messages the publisher sent
- ✅ Discards any pending PUBACK/PUBREC/PUBREL/PUBCOMP
- ✅ Clears publisher's subscriptions (if any)

**Does NOT affect:**
- ❌ Messages already accepted by broker (already PUBACK'd)
- ❌ Subscriber queues
- ❌ Subscriber session states
- ❌ Messages in transit to subscribers

### What Determines Message Lifetime

Messages queued for subscribers depend on **subscriber session settings**:

#### Subscriber with Clean Session = 0:
- Messages remain queued until subscriber reconnects
- Or until session expires (broker configuration)
- Or until queue limit reached

#### Subscriber with Clean Session = 1:
- No queuing happens at all
- Messages are lost if subscriber is offline

### Another Important Scenario

```
Time 1: Publisher connects with Clean Session = 0
        Publishes "topic: msg1" (QoS 1)
        Network fails before receiving PUBACK

Time 2: Publisher reconnects with Clean Session = 1

        Result: msg1 is discarded from publisher's pending messages
                BUT if broker already received msg1 and queued it
                for subscribers, it remains queued!
```

### Summary Table

| Action | Affects Publisher Session | Affects Subscriber Queues | Affects Messages Already PUBACK'd |
|--------|--------------------------|---------------------------|----------------------------------|
| Publisher reconnects Clean Session = 1 | ✅ Cleared | ❌ No effect | ❌ No effect |
| Subscriber reconnects Clean Session = 1 | ❌ No effect | ✅ Cleared | ❌ No effect (messages lost) |
| Broker restarts | Depends on broker persistence | Depends on broker persistence | Depends on broker persistence |

### Key Principle

**Once the broker sends PUBACK/PUBCOMP to the publisher, the message belongs to the broker and is managed according to SUBSCRIBER session policies, completely independent of what the publisher does.**

So to answer directly: **No, subscriber queues are NOT affected by the publisher's Clean Session flag. Messages remain queued for subscribers according to their own session settings.**

---

## 17. One Client ID = One Session

**Q: I understand that a client can have only one session, right?**

**Yes, exactly right!** A client can have only **one session** at a time, identified by its **Client ID**.

### How Sessions Work

#### Client ID is the Session Identifier

- Each client connects with a unique **Client ID**
- The Client ID identifies the session on the broker
- **One Client ID = One Session**

#### What Happens with Multiple Connections

If a client (or different clients using the same Client ID) tries to connect while a session already exists:

**The new connection takes over:**
1. New client connects with existing Client ID
2. Broker disconnects the old connection (if still active)
3. New connection assumes the session
4. Session state is transferred to new connection (if Clean Session = 0)

### Example Scenarios

#### Scenario 1: Same client reconnects
```
Time 1: Client "device123" connects with Clean Session = 0
Time 2: Client disconnects (session persists on broker)
Time 3: Client "device123" reconnects with Clean Session = 0
        ✅ Resumes existing session
        ✅ Receives queued messages
```

#### Scenario 2: Client takeover
```
Time 1: Client connects with ID "device123"
Time 2: Another process connects with SAME ID "device123"
        ❗ Broker disconnects first connection
        ✅ Second connection takes over the session
```

#### Scenario 3: Clean Session behavior
```
Client ID "device123" has persistent session with queued messages

Time 1: Client reconnects with Clean Session = 1
        ✅ Old session is DESTROYED
        ✅ New clean session created
        ❌ Queued messages are LOST
```

### Important Implications

#### You cannot have two active connections with the same Client ID
- Broker will always disconnect the old connection
- This is by design - prevents session conflicts
- Often called "client takeover"

#### Session belongs to the Client ID, not the TCP connection
- TCP connection can come and go
- Session persists on broker (if Clean Session = 0)
- Same Client ID always accesses same session

#### Use unique Client IDs for multiple simultaneous connections
```
✅ Device A: Client ID "device_A"
✅ Device B: Client ID "device_B"
(Two separate sessions, both can be connected simultaneously)

❌ Device A: Client ID "device_1"
❌ Device B: Client ID "device_1"
(Second connection will kick out the first)
```

### Shared Subscriptions (MQTT 5.0)

There's one exception-like feature in MQTT 5.0: **Shared Subscriptions**

- Multiple clients can subscribe to the same shared subscription group
- Messages are load-balanced between them
- But each client still has its own unique Client ID and session
- This is application-level load balancing, not multiple sessions per client

```
Client "worker1" subscribes to "$share/group/work/queue"
Client "worker2" subscribes to "$share/group/work/queue"
(Two separate clients, two separate sessions, sharing work)
```

### Summary

- **One Client ID = One Session** (fundamental rule)
- Multiple connections with same Client ID → last one wins (takeover)
- Want multiple simultaneous connections → use different Client IDs
- Session state tied to Client ID, not TCP connection
- Clean Session = 1 destroys old session and creates new one

So yes, your understanding is correct: **a client (identified by Client ID) has exactly one session at any given time**.

---

## 18. Protocol-Defined $ Topics

**Q: What other "$" topics has the protocol defined?**

The MQTT protocol defines **very few standard `$` topics** - most are **broker-specific implementations**.

### Protocol-Defined `$` Topics

#### `$SYS/` - Broker Statistics (MQTT 3.1.1)

This is the only widely recognized standard, though **not formally part of the MQTT spec**. It's a de-facto standard from Mosquitto:

```
$SYS/broker/version
$SYS/broker/uptime
$SYS/broker/clients/connected
$SYS/broker/clients/disconnected
$SYS/broker/clients/total
$SYS/broker/messages/received
$SYS/broker/messages/sent
$SYS/broker/subscriptions/count
$SYS/broker/load/messages/received/1min
$SYS/broker/load/messages/sent/1min
```

**Important notes:**
- Not formally standardized in MQTT specification
- Implementation varies by broker
- Read-only (clients subscribe, don't publish)
- Used for monitoring and diagnostics

#### `$share/` - Shared Subscriptions (MQTT 5.0 only)

This IS part of the **MQTT 5.0 specification**:

```
$share/{ShareName}/{TopicFilter}

Examples:
$share/workers/work/queue
$share/group1/sensors/+/temperature
```

**How it works:**
- Multiple subscribers in same share group
- Messages load-balanced across group members
- Each message delivered to only one subscriber in the group

#### `$delayed/` - NOT standard

Some brokers implement this (like EMQX), but it's **not in the MQTT specification**.

### What `$` Prefix Means

According to the MQTT specification:

> **Topic names beginning with `$` are reserved** for broker/server-specific purposes and should not be used by normal application topics.

**Rules:**
- Clients should not publish to `$` topics (unless broker-specific feature)
- Topics starting with `$` don't match wildcards `+` and `#`
- `$` topics are for broker control/metadata, not application data

#### Wildcard Behavior

```
Subscribe to "#"
  → Does NOT match "$SYS/broker/uptime"
  → Does NOT match "$share/group/topic"

Subscribe to "$SYS/#"
  → Matches "$SYS/broker/uptime" ✅
  → Matches "$SYS/broker/clients/connected" ✅
```

### Broker-Specific `$` Topics

Different brokers implement their own `$` topics:

#### Mosquitto
- `$SYS/*` - comprehensive system topics

#### EMQX
- `$SYS/*` - system topics
- `$delayed/{DelayInterval}/{Topic}` - delayed message publishing
- `$queue/{Topic}` - queue subscriptions (deprecated, use `$share`)

#### HiveMQ
- `$SYS/*` - system topics (limited)
- Custom extensions via plugins

#### AWS IoT Core
- `$aws/things/{thingName}/shadow/*` - device shadow topics
- `$aws/events/*` - lifecycle events
- `$aws/rules/{ruleName}` - rules engine

#### Azure IoT Hub
- `$iothub/*` - Azure-specific topics
- Device twins and direct methods

### Summary of Standard Topics

| Topic Prefix | MQTT Version | Status | Purpose |
|--------------|--------------|--------|---------|
| `$SYS/` | 3.1.1+ | De-facto standard (not in spec) | Broker statistics |
| `$share/` | 5.0 only | Official spec | Shared subscriptions |
| Others | N/A | Broker-specific | Vendor features |

### Key Takeaways

1. **Only `$share/` is truly standardized** (MQTT 5.0)
2. **`$SYS/` is widely used** but not in the official spec
3. **`$` prefix is reserved** - don't use for application topics
4. **Wildcards don't match `$` topics** (important for security)
5. **Most `$` topics are broker-specific** - check your broker's documentation

So to answer directly: The protocol formally defines very little - mainly the `$` reservation and `$share/` in MQTT 5.0. Everything else is convention or broker-specific implementation.

---

## 19. Client Storage Requirements for QoS > 0

**Q: Being client managing the protocol, the messages with qos > 0 don't need to be stored, right?**

**Not quite - clients DO need to store QoS > 0 messages!** This is a critical requirement for reliable delivery.

### Why Clients Must Store QoS > 0 Messages

Both **publishers** and **subscribers** need to store messages until the protocol handshake completes.

#### For Publishing (Client → Broker)

**QoS 1:**
```
Client: Stores PUBLISH message
Client → Broker: PUBLISH (QoS 1, PacketID=123)
[Wait for acknowledgment]
Client ← Broker: PUBACK (PacketID=123)
Client: Now safe to delete message ✅
```

**Why storage is needed:**
- Network might fail before PUBACK arrives
- Client must be able to **resend** if no PUBACK received
- Must store until PUBACK confirms broker received it

**QoS 2:**
```
Client: Stores PUBLISH message
Client → Broker: PUBLISH (QoS 2, PacketID=456)
Client ← Broker: PUBREC (PacketID=456)
Client: Stores PUBREC state, can delete original message
Client → Broker: PUBREL (PacketID=456)
Client ← Broker: PUBCOMP (PacketID=456)
Client: Now safe to delete all state ✅
```

**Why storage is needed:**
- Four-step handshake can be interrupted at any point
- Must track which phase we're in
- Must be able to resume after reconnection

#### For Receiving (Broker → Client)

**QoS 1:**
```
Client ← Broker: PUBLISH (QoS 1, PacketID=789)
Client: Stores message, processes it
Client → Broker: PUBACK (PacketID=789)
Client: Can delete after sending PUBACK
```

**QoS 2:**
```
Client ← Broker: PUBLISH (QoS 2, PacketID=999)
Client: Stores message and state
Client → Broker: PUBREC (PacketID=999)
Client ← Broker: PUBREL (PacketID=999)
Client: NOW processes message (exactly once guaranteed)
Client → Broker: PUBCOMP (PacketID=999)
Client: Can delete state ✅
```

**Why storage is needed:**
- Must prevent duplicate processing (QoS 2)
- Must track protocol state through disconnections
- Must handle retransmissions correctly

### What Happens Without Storage

#### Scenario: Client doesn't store QoS 1 message

```
Client → Broker: PUBLISH (QoS 1) "temperature: 25°C"
[Network failure - no PUBACK received]
Client reconnects
❌ Client has no record of the message
❌ Message is LOST - violates QoS 1 guarantee!
```

#### Scenario: Client doesn't store QoS 2 state

```
Client ← Broker: PUBLISH (QoS 2) "command: reboot"
Client → Broker: PUBREC
[Client processes: reboots device]
[Network failure]
Client reconnects
Broker → Client: PUBLISH (QoS 2) "command: reboot" [retransmission]
❌ Client has no record it already received this
❌ Device reboots AGAIN - violates exactly-once guarantee!
```

### With Persistent Sessions (Clean Session = 0)

Storage requirements are even more critical:

**Client must persist to disk (not just RAM):**
- Store unacknowledged PUBLISH messages
- Store protocol state (PUBREC/PUBREL state)
- Survive client crashes and restarts
- Resume protocol flows after reconnection

**Example:**
```
Client publishes with QoS 2, stores to disk
Client crashes before PUBCOMP
Client restarts and reconnects
Client reads stored state from disk
✅ Resumes protocol from where it left off
```

### Minimum Storage Requirements

#### For Publishers:

**QoS 1:**
- Store PUBLISH message until PUBACK received
- Store Packet ID to match acknowledgment

**QoS 2:**
- Store PUBLISH until PUBREC received
- Store PUBREC state until PUBCOMP received
- Store Packet ID through entire handshake

#### For Subscribers:

**QoS 1:**
- Brief storage while processing (can be RAM only)
- Send PUBACK quickly

**QoS 2:**
- Store message ID when PUBLISH received
- Must remember until PUBCOMP sent
- Prevents duplicate processing

### Implementation Considerations

**RAM vs Disk:**
- **Clean Session = 1**: Can use RAM only (lost on crash is acceptable)
- **Clean Session = 0**: MUST persist to disk for true reliability

**Typical client library behavior:**
```c
// Publishing with QoS 1
mosquitto_publish(mosq, &mid, "topic", len, payload, 1, false);
// Library internally:
// 1. Stores message with Packet ID
// 2. Sends PUBLISH
// 3. Waits for PUBACK
// 4. Deletes stored message when PUBACK arrives
// 5. Calls on_publish callback
```

### Summary

| QoS Level | Client Must Store? | Until When? |
|-----------|-------------------|-------------|
| QoS 0 | ❌ No | N/A - fire and forget |
| QoS 1 | ✅ Yes | Until PUBACK received |
| QoS 2 | ✅ Yes | Until PUBCOMP received (full handshake) |

**Critical point:** The QoS guarantees (at-least-once, exactly-once) are only possible BECAUSE clients store messages. Without storage, QoS > 0 cannot work reliably.

So no, clients definitely need to store QoS > 0 messages - it's fundamental to how the protocol provides reliable delivery!
