# MQTT Mosquitto - Persistence and Client Management Discussion

## Table of Contents

### 📑 Quick Navigation

#### I. Core Concepts & Architecture
- [Q1: Mosquitto Bug - Payload Size Check with Empty Topic](#q1-mosquitto-bug---payload-size-check-with-empty-topic)
- [Q2: Difference Between Inflight and Queued Messages](#q2-difference-between-inflight-and-queued-messages)
- [Q3: When Does Mosquitto Store Messages to Disk?](#q3-when-does-mosquitto-store-messages-to-disk)
- [Q4: Client-Side Persistence Management](#q4-client-side-persistence-management)

#### II. MQTT Specification & Compliance
- [Q5: MQTT Spec Compliance - Does Mosquitto Violate Requirements?](#q5-mqtt-spec-compliance---does-mosquitto-violate-requirements)
- [Q6: MQTT Client Libraries in C with Persistence](#q6-mqtt-client-libraries-in-c-with-persistence)

#### III. Client Libraries Comparison
- [Q7: Other Libraries, Languages and State](#q7-other-libraries-language-and-state)
- [Q8: What Does AWS Use?](#q8-what-does-aws-use)
- [Q9: Who Offers Client Persistence?](#q9-who-offers-client-persistence)
- [Q10: Entities and Services - Open Source vs Commercial](#q10-entities-and-services---commercials-opens)

#### IV. Cloud Services & Integration
- [Q11: Can You Use Any Client with Any Broker?](#q11-can-you-use-any-client-with-any-broker)
- [Q12: Pricing of Commercial MQTT Services](#q12-pricing-of-commercial-mqtt-services)
- [Q13: Client ID Limits by Service](#q13-client-id-limits-by-service)
- [Q14: Final Pricing - Real-World Scenarios](#q14-final-pricing---real-world-scenarios)

#### V. Summary
- [Key Takeaways](#key-takeaways)

---

## 📊 Index by Topic

### Message Management
| Topic | Question | Key Points |
|-------|----------|------------|
| **Inflight vs Queued** | Q2 | Messages in transit vs waiting to send; Limits: 20/1000 |
| **Broker Persistence** | Q3 | Custom binary format; 30min auto-save; `mosquitto.db` |
| **Client Persistence** | Q4, Q5, Q6 | Memory-only by default; Spec compliance issues |

### Client Libraries
| Library | Language | Persistence | Question |
|---------|----------|-------------|----------|
| **Mosquitto** | C | ❌ No | Q5, Q6 |
| **Eclipse Paho C/C++** | C/C++ | ✅ File-based | Q6, Q7 |
| **Eclipse Paho Java** | Java | ✅ File/Custom | Q7, Q9 |
| **Eclipse Paho Python** | Python | ✅ File-based | Q7, Q9 |
| **Eclipse Paho Go** | Go | ✅ File-based | Q7, Q9 |
| **HiveMQ Client** | Java | ✅ Automatic | Q7, Q9 |
| **AWS IoT SDKs** | Multiple | ❌ No | Q8 |
| **MQTT.js** | JavaScript | ❌ No | Q7 |

### Cloud Services Pricing
| Scale | Service | Monthly Cost | Question |
|-------|---------|--------------|----------|
| **Hobby (10 devices)** | Free tiers | $0-5 | Q14 |
| **Small (100 devices)** | AWS IoT Core | $10-12 | Q12, Q14 |
| **Medium (1K devices)** | AWS/Azure | $250-500 | Q14 |
| **Large (10K devices)** | Self-hosted | $200-500 | Q14 |

### Service Limits
| Provider | Client ID Limit | Concurrent Connections | Question |
|----------|----------------|------------------------|----------|
| **AWS IoT Core** | Unlimited | 500K default | Q13 |
| **Azure IoT Hub** | 500-400K (by tier) | Tied to tier | Q13 |
| **HiveMQ Cloud** | Unlimited | 100-1K by plan | Q13 |
| **Mosquitto** | Unlimited | Hardware limited | Q13 |

---

## 🔍 Quick Reference Guide

### Need Persistence?
- ✅ **Eclipse Paho** (C/C++, Java, Python, Go, Rust)
- ✅ **HiveMQ Client** (Java only)
- ❌ Mosquitto client, AWS SDKs, MQTT.js

### Best Choice by Use Case
| Use Case | Recommendation | See Question |
|----------|----------------|--------------|
| **Strict MQTT compliance** | Eclipse Paho C + persistence | Q5, Q6 |
| **AWS ecosystem** | AWS SDK (convenience) or Paho C (persistence) | Q8, Q11 |
| **Java development** | HiveMQ Client or Paho Java | Q7, Q9 |
| **Cost optimization** | AWS IoT (small) / Self-hosted (large) | Q12, Q14 |
| **Quick prototyping** | Mosquitto client (accept restart risk) | Q4, Q5 |

### Critical Issues & Bugs
| Issue | Location | Impact | Question |
|-------|----------|--------|----------|
| **Payload validation bypass** | `mosquitto_publish_v5` | Security risk with Topic Aliases | Q1 |
| **No client persistence** | Mosquitto library | QoS guarantees lost on restart | Q5 |

### Configuration Quick Reference
```conf
# Mosquitto Broker Configuration (Q3)
persistence true                          # Enable disk storage
persistence_location /var/lib/mosquitto/  # Storage path
autosave_interval 300                     # Save every 5 minutes
max_inflight_messages 20                  # Inflight limit (Q2)
max_queued_messages 1000                  # Queue limit (Q2)
```

---

## Q1: Mosquitto Bug - Payload Size Check with Empty Topic

**Question:** In mosquitto actions.c, in mosquitto_publish_v5 function, the payload > MQTT_MAX_PAYLOAD only is checked when topic name is not empty. Is that a bug?

**Answer:**

Yes, this appears to be a potential issue. Looking at the code snippet from `mosquitto_publish_v5`, the payload size check appears inside a conditional block that also validates the topic:

```c
if(mosquitto_validate_utf8(topic, (int)tlen)) return MOSQ_ERR_MALFORMED_UTF8;
if(payloadlen < 0 || payloadlen > (int)MQTT_MAX_PAYLOAD) return MOSQ_ERR_PAYLOAD_SIZE;
if(mosquitto_pub_topic_check(topic) != MOSQ_ERR_SUCCESS){
    return MOSQ_ERR_INVAL;
}
```

**The Problem:**

The payload size validation (`payloadlen > MQTT_MAX_PAYLOAD`) is only executed when the topic validation passes. However, in MQTT v5, there's a feature called **Topic Aliases** where a client can send a message with an empty topic string if it has previously established a topic alias. This means:

1. If `topic` is empty (or NULL), the UTF-8 validation might fail or `tlen` might be 0
2. The code might skip the entire validation block
3. The payload size check would then be bypassed
4. An oversized payload could be processed without validation

**Why this matters:**
- Empty topics are valid when using Topic Aliases in MQTT v5
- The payload size limit should be enforced regardless of whether a topic alias is being used
- This could lead to buffer overflows or other issues if very large payloads are sent with topic aliases

**The fix would be to move the payload size check outside the topic validation block:**

```c
if(payloadlen < 0 || payloadlen > (int)MQTT_MAX_PAYLOAD) return MOSQ_ERR_PAYLOAD_SIZE;

if(topic && tlen > 0) {
    if(mosquitto_validate_utf8(topic, (int)tlen)) return MOSQ_ERR_MALFORMED_UTF8;
    if(mosquitto_pub_topic_check(topic) != MOSQ_ERR_SUCCESS){
        return MOSQ_ERR_INVAL;
    }
}
```

---

## Q2: Difference Between Inflight and Queued Messages

**Question:** Explain me the difference between inflight and queued messages.

**Answer:**

### Inflight Messages

**Inflight messages** are messages that have been sent but are waiting for acknowledgment to complete the QoS (Quality of Service) flow.

**Characteristics:**
- **Active in transit**: These messages are currently "in flight" between client and broker
- **QoS dependent**: Only relevant for QoS 1 and QoS 2 messages (QoS 0 has no acknowledgment)
- **Limited by protocol**: There's a limit on how many can be in-flight simultaneously
- **Waiting for ACK**:
    - QoS 1: Waiting for PUBACK
    - QoS 2: Waiting for PUBREC, PUBREL, or PUBCOMP (4-way handshake)

**Example flow (QoS 1):**
```
Client → PUBLISH → Broker  [Message is now INFLIGHT]
Client ← PUBACK  ← Broker  [Message completes, no longer inflight]
```

**Configuration:**
- Mosquitto: `max_inflight_messages` - default: 20
- Controls how many unacknowledged messages can be sent at once

### Queued Messages

**Queued messages** are messages that are stored and waiting to be sent or delivered.

**Characteristics:**
- **Waiting to be processed**: Not yet transmitted or being held for later delivery
- **Storage-based**: Kept in broker's message store or client's internal queue
- **Persistent sessions**: Especially important for clients with clean session = false
- **Different types**:
    - Messages queued for disconnected subscribers
    - Messages waiting to be sent because inflight limit is reached
    - Retained messages stored by the broker

**When messages get queued:**
1. **Subscriber is offline** (with persistent session)
2. **Inflight limit reached** - new messages queue until slots free up
3. **Network congestion** - client can't send fast enough
4. **Broker buffering** - holding messages for slow consumers

**Configuration:**
- Mosquitto: `max_queued_messages` - default: 1000
- Controls maximum queued messages per client

### Visual Comparison

```
Publisher Side:
┌─────────────────────────────────────────────────┐
│  Application generates messages                  │
└─────────────────┬───────────────────────────────┘
                  ↓
         ┌────────────────┐
         │  QUEUED        │ ← Messages waiting to be sent
         │  (in buffer)   │   (maybe inflight limit reached)
         └────────┬───────┘
                  ↓
         ┌────────────────┐
         │  INFLIGHT      │ ← Sent, waiting for ACK
         │  (on wire)     │   (max 20 by default)
         └────────┬───────┘
                  ↓
              [ACK received]
                  ↓
              [Complete]


Subscriber Side (Broker perspective):
┌─────────────────────────────────────────────────┐
│  Broker receives PUBLISH                         │
└─────────────────┬───────────────────────────────┘
                  ↓
         ┌────────────────┐
         │  QUEUED        │ ← Subscriber offline or slow
         │  (in store)    │   Messages accumulate here
         └────────┬───────┘
                  ↓
    [Subscriber connects/catches up]
                  ↓
         ┌────────────────┐
         │  INFLIGHT      │ ← Sending to subscriber
         │  (to client)   │   Waiting for client's ACK
         └────────┬───────┘
                  ↓
          [Client sends ACK]
                  ↓
              [Complete]
```

### Key Differences Summary

| Aspect | Inflight Messages | Queued Messages |
|--------|------------------|-----------------|
| **State** | Actively transmitted | Waiting to be sent |
| **Location** | On the network/in transit | In memory/storage buffer |
| **Limit Purpose** | Prevent network congestion | Prevent memory overflow |
| **QoS** | Only QoS 1 & 2 | All QoS levels |
| **Typical Limit** | Small (20-100) | Larger (1000+) |
| **What happens when full?** | New messages queue | New messages rejected/dropped |

### Practical Example

Imagine you're publishing 1000 messages with QoS 1:

1. **First 20 messages**: Sent immediately → **INFLIGHT** (waiting for PUBACK)
2. **Remaining 980 messages**: Can't send yet → **QUEUED** (waiting for inflight slots)
3. As each PUBACK arrives, inflight count decreases
4. Next queued message moves to inflight
5. Process continues until all 1000 are sent and acknowledged

If the subscriber is offline with a persistent session:
- Messages accumulate in the broker's **queue** for that client
- When client reconnects, messages move to **inflight** as they're delivered
- Client acknowledges each, freeing up inflight slots for more queued messages

---

## Q3: When Does Mosquitto Store Messages to Disk?

**Question:** In what moment mosquitto stores the messages in disk, and what kind of database is using?

**Answer:**

### When Mosquitto Stores Messages to Disk

Mosquitto stores messages to disk based on these triggers:

#### 1. Periodic Auto-save (Most Common)
Data is written to disk at periodic intervals defined by `autosave_interval` - default: 1800 seconds (30 minutes)

**Configuration:**
```conf
autosave_interval 1800  # Seconds between saves - default: 1800
```

#### 2. On Change-Based Threshold
If `autosave_on_changes` is enabled, the database saves when the number of subscription changes, retained messages, and queued messages exceeds `autosave_interval` (interpreted as count, not seconds)

**Configuration:**
```conf
autosave_on_changes false  # default: false
autosave_interval 100      # Now means 100 changes, not seconds
```

#### 3. Broker Shutdown
When Mosquitto closes gracefully, it writes all data to disk

#### 4. Manual Trigger
You can force a save by sending the SIGUSR1 signal to the Mosquitto process

```bash
sudo kill -SIGUSR1 $(pidof mosquitto)
# or
sudo killall -SIGUSR1 mosquitto
```

### What Gets Stored to Disk

When persistence is enabled, Mosquitto stores:
1. **Connection data** - Client subscriptions and session information
2. **QoS 1 and QoS 2 messages** that are in-flight or queued for offline clients
3. **Retained messages**
4. **QoS 0 messages** (only if `queue_qos0_messages` is enabled)

**Important:** The persistence file is NOT a message history database. It only stores messages that need to survive a broker restart (inflight messages, queued messages for offline clients, and retained messages)

### Database Type and Format

#### NOT a Traditional Database!

Mosquitto uses a **custom binary file format** - it's NOT SQLite, MySQL, or any standard database. The file is called `mosquitto.db` by default.

#### Custom Binary Format Details

The database structure consists of:
- **DBHeader** with magic bytes, CRC, and version number (currently version 6)
- **Chunk headers** followed by chunk data
- Different chunk types for different data (DB_CHUNK_CFG, DB_CHUNK_MSG_STORE, DB_CHUNK_CLIENT, DB_CHUNK_RETAIN, DB_CHUNK_SUB)

#### Reading the Database

You cannot open mosquitto.db with Excel or SQLite. To view its contents, you need to use the `mosquitto_db_dump` utility provided in the Mosquitto source code:

```bash
# Compile and use the dump tool
git clone https://github.com/eclipse-mosquitto/mosquitto
cd mosquitto/apps/db_dump
make
sudo ./mosquitto_db_dump /var/lib/mosquitto/mosquitto.db
```

**Output example:**
```
DB_CHUNK_SUB: Length: 54
  Client ID: myClient
  Topic: home/temperature
  QoS: 1

DB_CHUNK_MSG_STORE: Length: 285
  Store ID: 28935676
  Topic: sensors/data
  QoS: 1
  Retain: 1
  Payload Length: 190
  Payload: {json data here}
```

### Memory vs Disk Behavior

**Critical Understanding:**
Mosquitto ALWAYS keeps all messages in memory. The persistence file is just a snapshot written periodically. The file doesn't replace memory - it's a backup that gets reloaded on restart.

```
┌──────────────────────────────────────┐
│   IN-MEMORY DATABASE (Primary)       │
│   - All active messages               │
│   - Subscriptions                     │
│   - Client sessions                   │
│   - Retained messages                 │
└──────────┬───────────────────────────┘
           │
           │ Periodic sync (autosave_interval)
           │ OR on shutdown
           │ OR on SIGUSR1
           ↓
┌──────────────────────────────────────┐
│   mosquitto.db (Disk backup)         │
│   - Custom binary format             │
│   - Loaded on startup                │
└──────────────────────────────────────┘
```

### Configuration Example

```conf
# Enable persistence - default: false
persistence true

# Where to store the database - default: "" (current directory)
persistence_location /var/lib/mosquitto/

# Custom filename (optional) - default: mosquitto.db
persistence_file mosquitto.db

# Auto-save every 5 minutes - default: 1800
autosave_interval 300

# Or use change-based saving
# autosave_on_changes true
# autosave_interval 100

# Also persist QoS 0 messages (optional, usually not needed) - default: false
queue_qos0_messages false

# Expire old persistent clients after 1 month - default: never
persistent_client_expiration 1m
```

### Why This Design?

**Performance:** Memory operations are thousands of times faster than disk I/O. By keeping everything in memory and only periodically syncing to disk, Mosquitto maintains high message throughput.

**Reliability:** The periodic disk sync ensures that even if the broker crashes, you only lose messages since the last save (maximum 30 minutes by default).

**Simplicity:** A custom binary format is simpler and more efficient than integrating a full database system for what is essentially a checkpoint/restore mechanism.

---

## Q4: Client-Side Persistence Management

**Question:** Give me firstly a summary of client side management of persistence.

**Answer:**

### Client-Side Persistence Summary

#### Key Point: **Clients DO NOT persist to disk by default**

```c
// ================================================================
// Client-Side Storage (ALL IN MEMORY)
// ================================================================

// Maximum inflight messages - default: 20
unsigned int max_inflight = 20;
mosquitto_max_inflight_messages_set(mosq, max_inflight);

// Messages waiting to be sent: stored in RAM, no disk
// Messages sent waiting for ACK: stored in RAM, no disk
// If client crashes: ALL queued/inflight messages are LOST
```

#### Clean Session Flag (Server-Side Storage)

```c
// Clean session - default: true
bool clean_session = true;
mosquitto_new(client_id, clean_session, userdata);

// clean_session = true (default):
//   - Broker does NOT queue messages when client disconnects
//   - Fresh session each connection

// clean_session = false:
//   - Broker DOES queue messages when client disconnects
//   - Client must use same client_id to retrieve queued messages
//   - This is broker-side persistence, not client-side!
```

#### Bottom Line

- **Client library**: No disk storage, everything in RAM
- **Broker queuing**: Controlled by clean_session flag
- **Want client persistence?**: You must implement it yourself in your application

---

## Q5: MQTT Spec Compliance - Does Mosquitto Violate Requirements?

**Question:** You told me that the client must store the messages with qos>0 to comply mqtt specifications, then mosquitto is not a sure tool?

**Answer:**

### MQTT Specification Requirement

**Yes, the MQTT spec DOES require** that clients store QoS 1 and QoS 2 messages to ensure delivery guarantees survive restarts.

### What Mosquitto Client Library Actually Does

**Mosquitto client library stores messages ONLY in memory**, which means:

- ✅ **Compliant during normal operation** - messages are retried, ACKs are tracked
- ❌ **NOT compliant across client restarts** - if the client process crashes/restarts, unacknowledged messages are lost

### Is Mosquitto Unsafe?

**Not exactly - it depends on your use case:**

#### For Many Applications: **Safe Enough**
- If your client rarely crashes
- If occasional message loss on crash is acceptable
- If your application handles retries at a higher level
- **Most IoT devices and simple clients fall here**

#### For Critical Applications: **Not Safe Enough**
- Financial transactions
- Medical data
- Any scenario where QoS guarantees must survive restarts
- **You need a client library with true persistence**

### The Trade-off

Mosquitto chose **simplicity and performance** over **strict spec compliance**:
- Faster (no disk I/O)
- Simpler code
- Lower resource usage
- **BUT**: loses QoS guarantees on client restart

You're correct that this is technically a spec violation for the strictest interpretation of QoS guarantees.

---

## Q6: MQTT Client Libraries in C with Persistence

**Question:** Those another libraries are in C?

**Answer:**

### Eclipse Paho C - Has Persistence Option

```c
// Paho C supports persistence - default: memory only
MQTTClient client;
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

// Enable file-based persistence - default: MQTTCLIENT_PERSISTENCE_NONE
MQTTClient_create(&client, "tcp://localhost:1883", "clientID",
                  MQTTCLIENT_PERSISTENCE_DEFAULT,  // Uses files in /tmp
                  NULL);

// Other persistence options:
// MQTTCLIENT_PERSISTENCE_NONE      - default: no persistence (like Mosquitto)
// MQTTCLIENT_PERSISTENCE_DEFAULT   - file-based in default location
// MQTTCLIENT_PERSISTENCE_USER      - custom persistence implementation

// Example with custom directory:
MQTTClient_create(&client, "tcp://localhost:1883", "clientID",
                  MQTTCLIENT_PERSISTENCE_DEFAULT,
                  "/var/mqtt/persistence");  // Custom directory
```

### Summary

**Yes**, Eclipse Paho C is in C and has real persistence. If you need strict MQTT spec compliance with QoS guarantees across restarts, **use Paho C with persistence enabled**, not Mosquitto client library.

---

## Q7: Other Libraries with Persistence

**Question:** Other libraries, language and state.

**Answer:**

### MQTT Client Libraries with Persistence

| Library | Language | Persistence | Notes |
|---------|----------|-------------|-------|
| **Eclipse Paho C** | C | ✅ Optional | File-based |
| **Eclipse Paho C++** | C++ | ✅ Optional | File-based |
| **Eclipse Paho Java** | Java | ✅ Optional | File or memory |
| **Eclipse Paho Python** | Python | ✅ Optional | File-based |
| **Eclipse Paho JavaScript** | JavaScript | ❌ No | Browser/Node.js |
| **HiveMQ Client** | Java | ✅ Built-in | Advanced features |
| **MQTT.js** | JavaScript | ❌ No | Node.js/browser |
| **Mosquitto (libmosquitto)** | C | ❌ No | Memory only |
| **Paho Rust** | Rust | ✅ Optional | File-based |
| **Eclipse Paho Go** | Go | ✅ Optional | File-based |

**Summary**: Most "Eclipse Paho" clients have persistence. Mosquitto client library does not.

---

## Q8: What Does AWS Use?

**Question:** What AWS use?

**Answer:**

### AWS IoT Core - MQTT Client Libraries

AWS provides their own SDKs that wrap underlying MQTT clients:

#### AWS IoT Device SDK for Embedded C
- Based on: **Custom implementation** (not Mosquitto or Paho)
- Persistence: **❌ No built-in client persistence**
- Storage: Memory only by default

#### AWS IoT Device SDK v2 (C++)
- Based on: **aws-crt-cpp** (AWS Common Runtime)
- Persistence: **❌ No built-in client persistence**
- Storage: Memory only

#### AWS IoT Device SDK (Python)
- Based on: **aws-crt-python** wrapping C runtime
- Persistence: **❌ No built-in client persistence**

#### AWS IoT Device SDK (Java)
- Based on: **AWS CRT Java**
- Persistence: **❌ No built-in client persistence**

#### AWS IoT Device SDK (JavaScript)
- Based on: **MQTT.js** or AWS CRT
- Persistence: **❌ No**

### Key Point

**AWS IoT SDKs do NOT provide client-side message persistence.**

They rely on:
1. **AWS IoT Core** (the broker) to queue messages when devices are offline
2. **Your application** to handle persistence if needed
3. QoS guarantees work during runtime, but NOT across client restarts

Same issue as Mosquitto client library - not fully spec compliant for restarts.

---

## Q9: Who Offers Client Persistence?

**Question:** Who offer client persistence?

**Answer:**

### MQTT Clients with TRUE Persistence

#### Confirmed Working Persistence:

| Library | Language | Persistence Type |
|---------|----------|-----------------|
| **Eclipse Paho Java** | Java | File or custom store |
| **Eclipse Paho C/C++** | C/C++ | File-based |
| **Eclipse Paho Python** | Python | File-based |
| **Eclipse Paho Go** | Go | File-based |
| **HiveMQ Client** | Java | File-based (automatic) |
| **Paho Rust** | Rust | File-based |
| **Eclipse Paho Android** | Java/Android | SQLite database |

#### Don't Have Persistence:

- Mosquitto client library (C)
- AWS IoT SDKs (all languages)
- MQTT.js (JavaScript)
- Most lightweight/embedded clients

### Best Options by Language

- **Java**: HiveMQ Client or Paho Java
- **C/C++**: Eclipse Paho C/C++
- **Python**: Eclipse Paho Python
- **Go**: Eclipse Paho Go

**Pattern**: If it says "Eclipse Paho" and runs on a platform with a filesystem, it probably has persistence.

---

## Q10: Entities and Services - Open Source vs Commercial

**Question:** Entities or services, commercials, opens.

**Answer:**

### MQTT Client Persistence - By Provider

#### Open Source (Free)

| Provider | Library | Language | Persistence |
|----------|---------|----------|-------------|
| **Eclipse Foundation** | Paho C/C++ | C/C++ | ✅ File |
| **Eclipse Foundation** | Paho Java | Java | ✅ File/Custom |
| **Eclipse Foundation** | Paho Python | Python | ✅ File |
| **Eclipse Foundation** | Paho Go | Go | ✅ File |
| **Eclipse Foundation** | Mosquitto client | C | ❌ No |
| **Eclipse Foundation** | Paho Rust | Rust | ✅ File |

#### Commercial (Free with Pro Options)

| Provider | Library | Language | Persistence |
|----------|---------|----------|-------------|
| **HiveMQ** | HiveMQ Client | Java | ✅ File (automatic) |
| **AWS** | IoT Device SDKs | Multiple | ❌ No |
| **Azure** | IoT SDKs | Multiple | ❌ No |
| **Google Cloud** | IoT Core SDKs | Multiple | ❌ No |

#### Broker Services (Server-Side Only)

| Provider | Type | Client Library Persistence |
|----------|------|---------------------------|
| **HiveMQ Cloud** | Commercial | Only if you use HiveMQ client |
| **AWS IoT Core** | Commercial | ❌ No |
| **Azure IoT Hub** | Commercial | ❌ No |
| **CloudMQTT** | Commercial | Depends on client used |
| **Mosquitto** | Open Source | ❌ No (broker persists, not client) |

### Summary

**Open Source with persistence**: Eclipse Paho family (all languages except JavaScript)

**Commercial with persistence**: HiveMQ Client (Java only)

**Cloud providers**: Don't provide client persistence in their SDKs

---

## Q11: Can You Use Any Client with Any Broker?

**Question:** You always can to use a client library with persistence against AWS, Azure.. no?

**Answer:**

### Yes, Absolutely Correct!

You can use **any MQTT client library** with **any MQTT broker** (AWS, Azure, etc.)

```
┌─────────────────────────┐
│  Your Choice of Client  │
│  (e.g., Paho C with     │ ←─ You control this
│   persistence enabled)  │
└───────────┬─────────────┘
            │
            │ Standard MQTT Protocol
            │
            ↓
┌─────────────────────────┐
│  Any MQTT Broker        │
│  - AWS IoT Core         │ ←─ Doesn't matter which
│  - Azure IoT Hub        │
│  - HiveMQ Cloud         │
│  - Mosquitto            │
└─────────────────────────┘
```

#### Example: Paho C + AWS IoT Core

```c
// Use Eclipse Paho C (with persistence) against AWS IoT Core
MQTTClient client;

// Enable persistence - saves to disk
MQTTClient_create(&client, 
                  "ssl://your-endpoint.iot.us-east-1.amazonaws.com:8883",
                  "myClientID",
                  MQTTCLIENT_PERSISTENCE_DEFAULT,  // ✅ Disk persistence
                  "/var/mqtt/persist");

// Works perfectly with AWS IoT Core!
```

#### Why Use AWS SDK Then?

AWS SDKs add convenience features:
- Easier certificate handling
- Shadow/Jobs integration
- AWS-specific optimizations

**But you don't have to use them.** Standard MQTT client + AWS broker = works fine.

#### Reality

Most people use AWS SDK for convenience, **sacrificing client persistence**. But you're right - using Paho C with AWS gives you both persistence AND AWS compatibility.

---

## Q12: Pricing of Commercial MQTT Services

**Question:** Prices of those commercial services.

**Answer:**

### Commercial MQTT Broker Services - Pricing

#### AWS IoT Core
- **Free tier**: 250,000 messages/month (first 12 months)
- **After free tier**:
    - $1.00 per million messages
    - $0.08 per million connection minutes
- **Example**: 10M messages/month ≈ $10-15/month

#### Azure IoT Hub
- **Free tier**: 8,000 messages/day (permanent)
- **Basic tier**: Starts at $10/month (400K messages/day)
- **Standard tier**: Starts at $25/month (6M messages/day)
- **Example**: 10M messages/month ≈ $25-50/month

#### HiveMQ Cloud
- **Free tier**: 100 concurrent connections, 10 GB data/month
- **Starter**: $49/month (100 connections)
- **Professional**: $349/month (1,000 connections)
- **Enterprise**: Custom pricing (10,000+ connections)

#### CloudMQTT (by 84codes)
- **Free**: 5 connections, 10 KB message size - **$0/month**
- **Cat**: 10 connections - **$5/month**
- **Tiger**: 100 connections - **$49/month**
- **Lion**: 500 connections - **$249/month**

#### EMQ X Cloud (EMQX)
- **Free**: 25 connections, 1M messages/month - **$0/month**
- **Basic**: 100 connections - **$49/month**
- **Professional**: 1,000 connections - **$399/month**
- **Enterprise**: Custom pricing

#### Self-Hosted Mosquitto
- **Cost**: $0 (open source)
- **But you pay for**:
    - Server hosting (AWS EC2, DigitalOcean, etc.) ≈ $5-50/month
    - Your time for maintenance
    - SSL certificates (Let's Encrypt = free)

### Comparison Table

| Service | Free Tier | Starting Price | Best For |
|---------|-----------|----------------|----------|
| **AWS IoT Core** | ✅ 250K msg/month | ~$10/month | AWS ecosystem users |
| **Azure IoT Hub** | ✅ 8K msg/day | $10/month | Azure ecosystem users |
| **HiveMQ Cloud** | ✅ 100 connections | $49/month | Enterprise features |
| **CloudMQTT** | ✅ 5 connections | $5/month | Small projects |
| **EMQX Cloud** | ✅ 25 connections | $49/month | High scalability |
| **Self-hosted Mosquitto** | ✅ Unlimited | $5-50/month (hosting) | Full control, budget |

---

## Q13: Client ID Limits by Service

**Question:** Have they limit of client Id's?

**Answer:**

### Client ID Limits by Service

#### AWS IoT Core
- **Limit**: No hard limit on number of unique client IDs
- **BUT**: Limits on concurrent connections per account - default: 500,000
- **Billing**: Pay per connection minute, not per client ID
- **Note**: Can increase limits via support request

#### Azure IoT Hub
- **Limit**: Tied to device registry
    - Free tier: 500 devices (client IDs)
    - Basic S1: 400,000 devices
    - Standard S1: 400,000 devices
- **Each tier has fixed device limits**

#### HiveMQ Cloud
- **Limit**: Based on plan's concurrent connections
    - Free: 100 concurrent connections
    - Starter: 100 concurrent
    - Professional: 1,000 concurrent
- **No limit on total client IDs**, only simultaneous connections

#### CloudMQTT
- **Limit**: Concurrent connections only
    - Free: 5 concurrent
    - Cat: 10 concurrent
    - Tiger: 100 concurrent
- **Unlimited unique client IDs**, just limited concurrent connections

#### EMQX Cloud
- **Limit**: Concurrent connections
    - Free: 25 concurrent
    - Basic: 100 concurrent
    - Professional: 1,000 concurrent
- **No limit on unique client IDs**

#### Self-Hosted Mosquitto
- **Limit**: None by default - max_connections: -1 (unlimited)
- **Only limited by**:
    - Server RAM/CPU
    - Config: `max_connections -1` (unlimited, default)
- **Can track millions of client IDs** if hardware supports it

### Key Distinction

| Metric | What It Means |
|--------|---------------|
| **Concurrent connections** | How many clients connected at same time |
| **Unique client IDs** | Total different client IDs that can exist |
| **Device registry** | Pre-registered client IDs (Azure concept) |

#### Example Scenario

```
Service with 100 concurrent connection limit:

✅ Allowed:
- 1 million different client IDs over time
- But only 100 connected simultaneously
- Client A connects, disconnects
- Client B connects (different ID)
- Repeat with different IDs infinitely

❌ Not allowed:
- 101 clients connected at the same time
```

### Azure is Different

Azure requires **pre-registering devices** in their device registry:
- Must create device identity before connection
- Each tier has fixed device limit
- More restrictive than pure MQTT brokers

### Summary

- **Most services**: Unlimited client IDs, limited concurrent connections
- **Azure**: Limited device registry entries per tier
- **Mosquitto self-hosted**: Truly unlimited (hardware dependent)

---

## Q14: Final Pricing - Real-World Scenarios

**Question:** What is the final prices?

**Answer:**

### Final Prices - Real-World Scenarios

#### Small IoT Project (100 devices, always connected, 1 msg/min each)
- **Messages**: 100 devices × 60 msg/hr × 24hr × 30 days = **4.3M messages/month**
- **Connection time**: 100 devices × 24hr × 30 days = **72,000 connection hours**

| Service | Monthly Cost |
|---------|--------------|
| **AWS IoT Core** | ~$10-12 |
| **Azure IoT Hub** | $10 (Basic S1) |
| **HiveMQ Cloud** | $49 (Starter plan) |
| **CloudMQTT** | $49 (Tiger, 100 conn) |
| **EMQX Cloud** | $49 (Basic, 100 conn) |
| **Mosquitto (self-hosted)** | $10-20 (small VPS) |

#### Medium Project (1,000 devices, 10 msg/min each)
- **Messages**: ~432M messages/month
- **Connections**: 1,000 concurrent

| Service | Monthly Cost |
|---------|--------------|
| **AWS IoT Core** | ~$450-500 |
| **Azure IoT Hub** | $200-400 (Standard S2) |
| **HiveMQ Cloud** | $349 (Professional) |
| **CloudMQTT** | $249-449 (Lion/Leopard) |
| **EMQX Cloud** | $399 (Professional) |
| **Mosquitto (self-hosted)** | $50-100 (medium VPS) |

#### Large Project (10,000 devices, 1 msg/min each)
- **Messages**: ~432M messages/month
- **Connections**: 10,000 concurrent

| Service | Monthly Cost |
|---------|--------------|
| **AWS IoT Core** | ~$500-600 |
| **Azure IoT Hub** | $2,500+ (Standard S3) |
| **HiveMQ Cloud** | Custom (Enterprise) - $2,000+ estimate |
| **CloudMQTT** | Custom pricing |
| **EMQX Cloud** | Custom (Enterprise) - $1,500+ estimate |
| **Mosquitto (self-hosted)** | $200-500 (large VPS/cluster) |

#### Hobby/Development (10 devices, testing)
- **Messages**: ~432K messages/month
- **Connections**: 10 concurrent

| Service | Monthly Cost |
|---------|--------------|
| **AWS IoT Core** | **$0** (free tier) |
| **Azure IoT Hub** | **$0** (free tier) |
| **HiveMQ Cloud** | **$0** (free tier) |
| **CloudMQTT** | **$5** (Cat plan) |
| **EMQX Cloud** | **$0** (free tier) |
| **Mosquitto (self-hosted)** | **$5** (tiny VPS) |

### Cost Breakdown Summary

**For 100 devices**: $10-50/month  
**For 1,000 devices**: $250-500/month  
**For 10,000 devices**: $500-2,500/month

### Cheapest Options by Scale

- **<100 devices**: Free tiers or self-hosted ($0-10)
- **100-1000 devices**: AWS IoT Core or self-hosted ($10-100)
- **1000+ devices**: AWS IoT Core or self-hosted ($100-500)
- **10,000+ devices**: Self-hosted cluster ($200-500)

### Hidden Costs to Consider

- **Data transfer**: AWS charges for internet egress
- **Storage**: If using device shadows, rule engines, etc.
- **Support**: Enterprise plans add $$$
- **Certificates**: Usually free (Let's Encrypt)
- **Your time**: Self-hosted requires maintenance

**Bottom line**: AWS/Azure are cheapest for small-medium scale. Self-hosted Mosquitto is cheapest at large scale if you have the expertise.

---

## Key Takeaways

1. **Mosquitto broker** has excellent disk persistence, but **Mosquitto client library** does not
2. **Inflight** = messages sent waiting for ACK, **Queued** = messages waiting to be sent
3. Most MQTT clients (including Mosquitto, AWS SDKs) don't persist to disk
4. **Eclipse Paho** clients (C/C++, Java, Python, Go) DO have optional persistence
5. You can use any MQTT client with any MQTT broker (standard protocol)
6. For strict MQTT spec compliance: use Paho C with persistence enabled
7. AWS/Azure are cost-effective for small-medium deployments, self-hosted for large scale

---

*Conversation Date: December 2025*
