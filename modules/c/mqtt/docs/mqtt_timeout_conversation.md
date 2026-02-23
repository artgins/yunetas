# MQTT Timeout and Retry Behavior - Conversation

## Table of Contents

- [Question 1: What does Mosquitto do when it doesn't receive PUBACK, PUBREC, or PUBCOMP?](#question-1-what-does-mosquitto-do-when-it-doesnt-receive-puback-pubrec-or-pubcomp)
  - [Answer Summary](#answer-summary)
  - [Mosquitto Broker Behavior](#mosquitto-broker-behavior)
  - [The retry_interval Configuration (Deprecated/Non-standard)](#the-retry_interval-configuration-deprecatednon-standard)
  - [Client Library Behavior](#client-library-behavior)
  - [Practical Implementation for Yunetas](#practical-implementation-for-yunetas)
- [Question 2: Does the MQTT specification have NO treatment of timeouts or no responses?](#question-2-does-the-mqtt-specification-have-no-treatment-of-timeouts-or-no-responses)
  - [Answer: Correct - The Spec Has NO Timeout for Acknowledgments](#answer-correct---the-spec-has-no-timeout-for-acknowledgments)
  - [What the MQTT Specification Actually Says](#what-the-mqtt-specification-actually-says)
  - [What This Means](#what-this-means)
  - [Why This Design?](#why-this-design)
  - [What About Real Implementations?](#what-about-real-implementations)
  - [The Only "Timeout" in MQTT: Keep-Alive](#the-only-timeout-in-mqtt-keep-alive)
  - [For Yunetas Implementation](#for-yunetas-implementation)
- [Key Takeaways](#key-takeaways)
- [References](#references)

---

## Question 1: What does Mosquitto do when it doesn't receive PUBACK, PUBREC, or PUBCOMP?

**User Question:**
What does Mosquitto do when it doesn't receive a PUBACK, PUBREC, or PUBCOMP? Does it have a timeout? What about the broker? Do they have retries of messages on the same timeout without response? What logic do they have?

### Answer Summary

**Key Point: No Timeout During Active Session**

According to the MQTT specification (especially v5.0), there is **NO timeout** for missing acknowledgments during an active session. The retry logic only triggers on **reconnection**, not during ongoing connections.

### Mosquitto Broker Behavior

#### During Active Connection:
- **No automatic retry** - Mosquitto waits indefinitely for PUBACK/PUBREC/PUBCOMP
- Messages stay in the in-flight queue
- No timeout mechanism triggers retransmission

#### On Reconnection:
- Automatically resends all unacknowledged QoS 1/2 messages
- Messages marked with DUP flag (duplicate) set to 1
- This is the ONLY time retransmission is required by spec

### The `retry_interval` Configuration (Deprecated/Non-standard)

Mosquitto has a `retry_interval` parameter (default: **20 seconds**), but:

**IMPORTANT**: This setting is **deprecated** and **non-standard** for MQTT v5.0. The MQTT v5.0 spec explicitly states:

> "Clients and Servers MUST NOT resend messages at any other time [than on reconnects]"

However, in older Mosquitto versions or MQTT v3.1.1 mode:
- `retry_interval` sets seconds to wait before retrying unacknowledged QoS 1/2 messages
- Default: 20 seconds
- Continuously retries every `retry_interval` if no ack received
- Can cause out-of-order delivery issues (especially with `max_inflight_messages = 1`)

### Client Library Behavior

Most MQTT client libraries (including libmosquitto):
- **Don't have built-in retry timers** during active sessions
- Rely on TCP reliability to deliver packets
- Send messages and wait for broker response
- Only resend on reconnection

### Practical Implementation for Yunetas

For your MQTT implementation in Yunetas, you should:

1. **No retry timer needed during active sessions** - trust TCP
2. **On reconnection**: resend all unacknowledged messages with DUP flag
3. **Track message state** properly:
   - QoS 1: sent → wait for PUBACK
   - QoS 2: sent → wait for PUBREC → send PUBREL → wait for PUBCOMP
4. **Persistence**: save unacknowledged messages to handle reconnection

---

## Question 2: Does the MQTT specification have NO treatment of timeouts or no responses?

**User Question:**
The MQTT specification has no treatment of timeouts or no responses?

### Answer: Correct - The Spec Has NO Timeout for Acknowledgments

You're absolutely right to question this. The MQTT specification (both v3.1.1 and v5.0) **does NOT define any timeout** for missing acknowledgments (PUBACK, PUBREC, PUBREL, PUBCOMP) during an active session.

### What the MQTT Specification Actually Says

#### MQTT v3.1.1:
"When a Client reconnects with CleanSession set to 0, both the Client and Server MUST re-send any unacknowledged PUBLISH Packets (where QoS > 0) and PUBREL Packets using their original Packet Identifiers. **This is the only circumstance where a Client or Server is REQUIRED to redeliver messages.**"

#### MQTT v5.0 (even stricter):
"Clients and Servers **MUST NOT resend messages at any other time [than on reconnects]**" [MQTT-4.4.0-1]

### What This Means

1. **No timeout mechanism** - During an active TCP connection, there's no specified timeout for waiting for ACKs
2. **No retransmission** - Messages are NOT retransmitted during an active session
3. **Trust TCP** - MQTT relies on TCP's reliability for packet delivery
4. **Only reconnect triggers retransmission** - Unacknowledged messages are resent only when the client reconnects (with clean_session=0 / clean_start=0)

### Why This Design?

The spec assumes:
- **TCP is reliable** - If TCP connection is alive, packets will arrive
- **Application-level timeouts would interfere** - A slow client processing a message shouldn't trigger retransmission
- **Network failure detection** - Use Keep-Alive (PINGREQ/PINGRESP) to detect broken connections, not message ACK timeouts

### What About Real Implementations?

Some implementations (like Mosquitto's `retry_interval`) added timeout-based retransmission as **non-standard extensions**, but:
- This is **deprecated** in modern MQTT
- MQTT v5.0 **explicitly forbids** it
- It can cause problems (out-of-order delivery, duplicate messages)

### The Only "Timeout" in MQTT: Keep-Alive

The only timeout mechanism in MQTT is the **Keep-Alive timeout**:
- Client sends PINGREQ if no packets sent within keep-alive interval
- Broker must receive some packet within 1.5x keep-alive interval
- If not received, broker disconnects the client
- Default keep-alive: often 60 seconds (so timeout = 90 seconds)

This detects **connection failure**, not slow message processing.

### For Yunetas Implementation

Your MQTT implementation should:

1. **Send message** → wait indefinitely for ACK (trust TCP)
2. **Keep-Alive mechanism** → detect connection failure (PINGREQ/PINGRESP timeout)
3. **On reconnection** → resend all unacknowledged messages
4. **No application-level retry timer** for ACKs during active session

---

## Key Takeaways

1. **MQTT specification defines NO timeout for message acknowledgments during active sessions**
2. **Retransmission only happens on reconnection** (with persistent sessions)
3. **TCP reliability is trusted** - if TCP is working, messages arrive
4. **Keep-Alive is the only timeout** - used to detect dead connections, not slow processing
5. **Mosquitto's `retry_interval` is non-standard** and deprecated for MQTT v5.0
6. **For Yunetas**: Implement reconnection-based retransmission, not timeout-based

## References

- MQTT v3.1.1 Specification: Section 4.4 (Message delivery retry)
- MQTT v5.0 Specification: [MQTT-4.4.0-1] (Message delivery on reconnect only)
- HiveMQ Blog: "Debunking Common MQTT QoS Misconceptions"
- Mosquitto configuration: `retry_interval` parameter (deprecated)
