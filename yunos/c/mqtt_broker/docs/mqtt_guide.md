# MQTT Core Concepts Guide

## The Retain Flag in MQTT

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

## Retain Flag vs Last Will and Testament

They're related but serve different purposes!

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

## MQTT Subscriptions and Reconnection

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

## Re-subscribing is Idempotent

**Re-subscribing is idempotent** - you can subscribe to the same topic multiple times without any negative effects.

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

---

## Messages During Disconnection

What happens to messages published when a client was disconnected depends on the **QoS level** and whether you're using **persistent sessions**.

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

## Broker Resource Exhaustion with Persistent Sessions

**Yes!** This is a real operational concern with MQTT brokers and persistent sessions.

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

## The AUTH Command in MQTT 5.0

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

## Standard AUTH Methods

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