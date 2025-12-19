# Complete Mosquitto Message Flow and Queue Management

## Message State Management Architecture

### Core Data Flow Overview

```
[PUBLISH arrives] → [Store] → [Queue] → [In-flight] → [ACK] → [Delete]
       ↓              ↓          ↓           ↓          ↓         ↓
   validate    db__message   ready for  send packet  update   cleanup
   + auth        _store      flight?                  state
```

## Complete Function Reference by Category

### 1. Flow Control & Capacity Management

#### `db__ready_for_flight()`
```c
BOOL db__ready_for_flight(struct mosquitto *context, 
                          enum mosquitto_msg_direction dir, 
                          int qos);
```
**Purpose**: Determines if client can accept more in-flight messages

**Logic**:
- Checks `context->msgs_in.inflight_quota` (for incoming) or `context->msgs_out.inflight_quota` (for outgoing)
- QoS 0: Always returns TRUE (no tracking needed)
- QoS 1/2: Returns TRUE if `inflight_count < max_inflight_messages`
- Used before sending new PUBLISH packets

**Called from**: 
- `handle__publish()` - Before accepting incoming QoS 1/2 message
- Message send loop - Before transmitting outgoing message

**Flow Impact**: Acts as gate-keeper preventing queue overflow

---

### 2. Message Storage Functions

#### `db__message_store()`
```c
int db__message_store(const struct mosquitto *source, 
                      struct mosquitto_msg_store *stored, 
                      uint32_t message_expiry_interval, 
                      int store_id, 
                      enum mosquitto_msg_origin origin);
```
**Purpose**: Central message storage - creates the canonical message object

**Logic**:
1. Allocates `mosquitto_msg_store` structure
2. Assigns unique `db_id` (if store_id == 0)
3. Copies topic, payload, properties
4. Sets expiry time = now + message_expiry_interval
5. Initializes `ref_count = 0`
6. Adds to global message store hash table
7. Calls `persist__message_store_write()` if persistence enabled

**Memory Model**: 
- Single stored message can be referenced by multiple clients
- Reference counting prevents premature deletion
- Ownership of topic/payload/properties transferred to store

**Called from**:
- `handle__publish()` - When broker receives PUBLISH
- `persist_read()` - When restoring from disk
- `db__messages_easy_queue()` - Helper function

---

#### `db__msg_store_free()`
```c
void db__msg_store_free(struct mosquitto_msg_store *store);
```
**Purpose**: Frees stored message when ref_count reaches 0

**Logic**:
1. Decrements `ref_count`
2. If `ref_count == 0`:
   - Frees topic string
   - Frees payload
   - Frees properties
   - Removes from hash table
   - Frees structure

**Called from**:
- `handle__publish()` - After message rejected/handled
- Message deletion functions - When last reference removed
- Cleanup routines

---

### 3. Message Insertion & Queueing

#### `db__message_insert()`
```c
int db__message_insert(struct mosquitto *context, 
                       uint16_t mid, 
                       enum mosquitto_msg_direction dir, 
                       uint8_t qos, 
                       BOOL retain, 
                       struct mosquitto_msg_store *stored, 
                       json_t *properties, 
                       BOOL update);
```
**Purpose**: Links a stored message to a specific client's queue

**Logic**:
1. Allocates `mosquitto_client_msg` wrapper
2. Sets `msg->store = stored` (increments ref_count)
3. Sets `msg->mid` for this client
4. Sets initial state:
   - Outgoing QoS 0: `mosq_ms_publish_qos0`
   - Outgoing QoS 1: `mosq_ms_publish_qos1`
   - Outgoing QoS 2: `mosq_ms_publish_qos2`
   - Incoming: `mosq_ms_wait_for_pubrel` (QoS 2) or queued
5. Adds to appropriate queue:
   - `context->msgs_out` (outgoing)
   - `context->msgs_in` (incoming)
6. Updates statistics via `db__msg_add_to_*_stats()`
7. If `update == TRUE`, calls persistence functions

**Queueing Decision**:
- If `db__ready_for_flight()` returns TRUE → state allows immediate send
- Otherwise → state = `mosq_ms_queued`, waits in queue

**Called from**:
- `handle__publish()` - Queue incoming message for subscribers
- Loop sending logic - Queue outgoing messages
- `persist_read()` - Restore queued messages

---

#### `db__messages_easy_queue()`
```c
int db__messages_easy_queue(struct mosquitto *context, 
                            const char *topic, 
                            uint8_t qos, 
                            uint32_t payloadlen, 
                            const void *payload, 
                            int retain, 
                            uint32_t message_expiry_interval, 
                            json_t **properties);
```
**Purpose**: High-level convenience function combining store + insert

**Logic**:
1. Creates `mosquitto_msg_store` structure with provided data
2. Calls `db__message_store()` 
3. Calls `db__message_insert()`
4. Returns combined result

**Used for**:
- Will message delivery (`context__send_will()`)
- Internal system messages
- Simple publish operations in loop

**Ownership**: Takes ownership of topic, payload, properties (will free on error)

---

### 4. Message Transmission Functions

#### `db__message_write_queued_out()`
```c
int db__message_write_queued_out(struct mosquitto *context);
```
**Purpose**: Sends all queued outgoing messages that are ready

**Logic**:
1. Iterates through `context->msgs_out` queue
2. For each message in `mosq_ms_queued` state:
   - Checks `db__ready_for_flight()`
   - If ready:
     - Changes state to appropriate in-flight state
     - Sends PUBLISH packet
     - Updates inflight count
     - Updates statistics
   - If not ready: stops (respects max_inflight limit)
3. Continues until queue empty or inflight limit reached

**Called from**:
- `handle__connack()` - After successful connection
- `handle__connect()` - Session restoration
- `handle__subscribe()` - After subscription, send retained messages
- Websocket handlers - After protocol upgrade
- After receiving PUBACK/PUBCOMP - Frees inflight slot

**Critical for**: Flow control and QoS guarantee delivery

---

#### `db__message_write_inflight_out_all()`
```c
int db__message_write_inflight_out_all(struct mosquitto *context);
```
**Purpose**: Retransmits ALL in-flight outgoing messages (reconnection scenario)

**Logic**:
1. Iterates `context->msgs_out` queue
2. For messages in in-flight states:
   - `mosq_ms_wait_for_puback`
   - `mosq_ms_wait_for_pubrec`
   - `mosq_ms_wait_for_pubcomp`
3. Resends PUBLISH or PUBREL packets
4. Sets DUP flag = 1 (duplicate delivery indicator)
5. Maintains original message ID

**Called from**:
- `handle__connect()` - Client reconnects with clean_session=false
- `handle__connack()` - After broker confirms connection

**MQTT Spec Compliance**: Required for persistent session QoS 1/2 delivery guarantee

---

#### `db__message_write_inflight_out_latest()`
```c
int db__message_write_inflight_out_latest(struct mosquitto *context);
```
**Purpose**: Sends only the LATEST queued message immediately (bypasses normal queue order)

**Logic**:
1. Finds last message in `context->msgs_out` with `state == mosq_ms_queued`
2. If `db__ready_for_flight()`:
   - Moves to in-flight state
   - Sends PUBLISH immediately
3. Does NOT process entire queue

**Called from**:
- `handle__subscribe()` - Send matching retained messages immediately
- Special priority scenarios

**Use Case**: Retained messages should be delivered immediately after SUBSCRIBE, not queued behind other messages

---

#### `db__message_write_queued_in()`
```c
int db__message_write_queued_in(struct mosquitto *context);
```
**Purpose**: Delivers queued incoming messages to local subscribers/bridge

**Logic**:
1. Iterates `context->msgs_in` queue
2. For messages in `mosq_ms_queued` state:
   - Delivers to local application/plugin
   - Removes from queue after delivery
3. Only processes messages that have completed QoS 2 handshake

**Called from**:
- `handle__subscribe()` - Deliver queued messages matching new subscription

**Context**: Used for messages that arrived but couldn't be delivered (no matching subscription yet)

---

### 5. Message State Update Functions

#### `db__message_update_outgoing()`
```c
int db__message_update_outgoing(struct mosquitto *context, 
                                uint16_t mid, 
                                enum mosquitto_msg_state state, 
                                int qos);
```
**Purpose**: Updates message state during QoS handshake progression

**Logic**:
1. Finds message by `mid` in `context->msgs_out`
2. Validates state transition is valid for QoS level
3. Updates `msg->state` to new state
4. Updates timestamp for timeout tracking
5. Calls persistence update if enabled

**State Transitions**:
- QoS 1: `publish_qos1` → `wait_for_puback` → (deleted)
- QoS 2: `publish_qos2` → `wait_for_pubrec` → `wait_for_pubcomp` → (deleted)

**Called from**:
- `handle__pubrec()` - After PUBREC received, before sending PUBREL
- Message send functions - After packet transmitted

---

#### `db__message_reconnect_reset()`
```c
int db__message_reconnect_reset(struct mosquitto *context);
```
**Purpose**: Resets message states when client reconnects

**Logic**:
1. For persistent session (clean_session=false):
   - Keeps messages in queue
   - Resets in-flight messages to queued state
   - Clears message IDs (will be reassigned)
   - Resets DUP flags
2. For clean session (clean_session=true):
   - Deletes all messages via `db__messages_delete()`
   - Clears both incoming and outgoing queues

**Called from**:
- `handle__connect()` - When client connects
- `handle__connack()` - After broker acknowledges connection

**Critical Decision Point**: Clean session vs persistent session behavior

---

### 6. Message Deletion Functions

#### `db__message_delete_outgoing()`
```c
int db__message_delete_outgoing(struct mosquitto *context, 
                                uint16_t mid, 
                                enum mosquitto_msg_state expect_state, 
                                int qos);
```
**Purpose**: Removes message from outgoing queue after successful delivery

**Logic**:
1. Finds message by `mid` in `context->msgs_out`
2. Validates state matches `expect_state` (security check)
3. Removes from linked list
4. Decrements inflight count
5. Calls `db__msg_store_free()` (decrements ref_count)
6. Frees `mosquitto_client_msg` wrapper
7. Updates statistics
8. Calls persistence delete
9. **Triggers**: `db__message_write_queued_out()` to send next queued message

**Called from**:
- `handle__puback()` - QoS 1 complete
- `handle__pubcomp()` - QoS 2 complete
- `handle__pubrec()` - After state update (via expect_state validation)

**Flow Control Impact**: Frees inflight slot, allowing queued messages to move forward

---

#### `db__message_remove_incoming()`
```c
int db__message_remove_incoming(struct mosquitto* context, uint16_t mid);
```
**Purpose**: Removes incoming message (typically for error/duplicate handling)

**Logic**:
1. Finds message by `mid` in `context->msgs_in`
2. Removes from queue
3. Decrements ref_count
4. Frees wrapper
5. Does NOT complete QoS handshake

**Called from**:
- `handle__publish()` - When duplicate QoS 2 PUBLISH detected
- Error handling paths

**Difference from release**: This is deletion, not completion

---

#### `db__message_release_incoming()`
```c
int db__message_release_incoming(struct mosquitto *context, uint16_t mid);
```
**Purpose**: Releases incoming QoS 2 message for delivery (completes handshake)

**Logic**:
1. Finds message in `context->msgs_in` with state `mosq_ms_wait_for_pubrel`
2. Changes state to `mosq_ms_queued`
3. Message now ready for local delivery
4. Sends PUBCOMP to publisher

**Called from**:
- `handle__pubrel()` - Third step of QoS 2 handshake

**QoS 2 Flow**:
```
PUBLISH → PUBREC → (wait) → PUBREL → release() → PUBCOMP → deliver
```

---

#### `db__messages_delete()`
```c
int db__messages_delete(struct mosquitto *context, BOOL force_free);
```
**Purpose**: Deletes ALL messages for a client (cleanup/disconnect)

**Logic**:
1. Iterates both `msgs_out` and `msgs_in` queues
2. For each message:
   - Decrements ref_count via `db__msg_store_free()`
   - Frees wrapper structure
   - Removes from queue
3. If `force_free == TRUE`: Immediate cleanup regardless of state
4. If `force_free == FALSE`: May preserve persistent session messages

**Called from**:
- `context__cleanup()` - Client disconnects
- `db__message_reconnect_reset()` - Clean session connection
- Broker shutdown

---

### 7. Statistics & Monitoring

#### `db__msg_add_to_inflight_stats()`
```c
void db__msg_add_to_inflight_stats(struct mosquitto_msg_data *msg_data, 
                                   struct mosquitto_client_msg *msg);
```
**Purpose**: Updates inflight message counters

**Logic**:
1. Increments `msg_data->msg_count` (total messages)
2. Increments `msg_data->inflight_count`
3. Adds `msg->payloadlen` to `msg_data->msg_bytes`
4. Updates inflight quota tracking

**Called from**:
- `persist_read()` - When restoring inflight messages
- Message state transitions to inflight

---

#### `db__msg_add_to_queued_stats()`
```c
void db__msg_add_to_queued_stats(struct mosquitto_msg_data *msg_data, 
                                 struct mosquitto_client_msg *msg);
```
**Purpose**: Updates queued message counters

**Logic**:
1. Increments `msg_data->msg_count`
2. Adds `msg->payloadlen` to `msg_data->msg_bytes`
3. Does NOT increment inflight_count

**Called from**:
- `persist_read()` - When restoring queued messages
- `db__message_insert()` - When queueing new messages

---

### 8. Message Expiration

#### `db__expire_all_messages()`
```c
void db__expire_all_messages(struct mosquitto *context);
```
**Purpose**: Removes expired messages for a specific client

**Logic**:
1. Current time = `time(NULL)`
2. Iterates `context->msgs_out` queue
3. For each message:
   - Checks `msg->store->expiry_time`
   - If `expiry_time > 0 && expiry_time < now`:
     - Removes message via `db__message_delete_outgoing()`
     - Continues iteration
4. Also processes `msgs_in` queue

**MQTT 5 Feature**: Message Expiry Interval property

**Called from**:
- `handle__connect()` - Check expiry on connection
- Periodic timer in main loop

---

### 9. Context Cleanup

#### `context__cleanup()`
```c
void context__cleanup(struct mosquitto *context, BOOL force_free);
```
**Purpose**: Complete client context cleanup on disconnect

**Logic**:
1. Calls `db__messages_delete(context, force_free)`
2. Frees subscriptions
3. Frees session state
4. If persistent session: Saves state to disk
5. If clean session or force_free: Complete deletion
6. Frees context structure

**Called from**:
- Client disconnect handlers
- Connection error paths
- Broker shutdown

**Warning**: Any unsent packets are lost (mentioned in code comment)

---

## Complete Message Flow Diagrams

### Flow 1: QoS 0 Message (Outgoing)

```
┌─────────────────┐
│ Message arrives │
└────────┬────────┘
         │
         ▼
┌─────────────────────────┐
│ db__message_store()     │ ← Create stored message
└────────┬────────────────┘
         │
         ▼
┌─────────────────────────┐
│ db__message_insert()    │ ← Link to client queue
│ state = publish_qos0    │
└────────┬────────────────┘
         │
         ▼
┌─────────────────────────┐
│db__message_write        │ ← Send immediately
│_queued_out()            │
└────────┬────────────────┘
         │
         ▼
┌─────────────────────────┐
│ Send PUBLISH packet     │
└────────┬────────────────┘
         │
         ▼
┌─────────────────────────┐
│ db__message_delete_     │ ← Remove immediately
│ outgoing()              │
└────────┬────────────────┘
         │
         ▼
┌─────────────────────────┐
│ db__msg_store_free()    │ ← Decrement ref_count
└─────────────────────────┘
```

### Flow 2: QoS 1 Message (Outgoing)

```
┌─────────────────┐
│ Message arrives │
└────────┬────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_store()      │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_insert()     │
│ state = publish_qos1     │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐     NO    ┌─────────────────┐
│ db__ready_for_flight()   ├──────────→│ state = queued  │
└────────┬─────────────────┘           └────────┬────────┘
         │ YES                                   │
         ▼                                       │
┌──────────────────────────┐                    │
│ Send PUBLISH             │                    │
│ state = wait_for_puback  │◄───────────────────┘
│ inflight_count++         │          (when slot available)
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ ◄─── PUBACK received     │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ handle__puback()         │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_delete_      │
│ outgoing()               │
│ inflight_count--         │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__msg_store_free()     │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_write_       │ ← Send next queued
│ queued_out()             │
└──────────────────────────┘
```

### Flow 3: QoS 2 Message (Outgoing - Complete)

```
┌─────────────────┐
│ Message arrives │
└────────┬────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_store()      │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_insert()     │
│ state = publish_qos2     │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐     NO    ┌─────────────────┐
│ db__ready_for_flight()   ├──────────→│ state = queued  │
└────────┬─────────────────┘           └────────┬────────┘
         │ YES                                   │
         ▼                                       │
┌──────────────────────────┐                    │
│ Send PUBLISH             │                    │
│ state = wait_for_pubrec  │◄───────────────────┘
│ inflight_count++         │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ ◄─── PUBREC received     │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ handle__pubrec()         │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_update_      │
│ outgoing()               │
│ state = wait_for_pubcomp │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ Send PUBREL              │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ ◄─── PUBCOMP received    │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ handle__pubcomp()        │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_delete_      │
│ outgoing()               │
│ expect_state =           │
│   wait_for_pubcomp       │
│ inflight_count--         │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__msg_store_free()     │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_write_       │
│ queued_out()             │
└──────────────────────────┘
```

### Flow 4: QoS 2 Message (Incoming)

```
┌──────────────────────────┐
│ ◄─── PUBLISH received    │
│      QoS = 2             │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ handle__publish()        │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐     NO    ┌──────────────────┐
│ db__ready_for_flight()   ├──────────→│ Reject/Error     │
└────────┬─────────────────┘           └──────────────────┘
         │ YES
         ▼
┌──────────────────────────┐
│ db__message_store()      │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_insert()     │
│ direction = in           │
│ state = wait_for_pubrel  │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ Send PUBREC ───►         │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ ◄─── PUBREL received     │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ handle__pubrel()         │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_release_     │
│ incoming()               │
│ state = queued           │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ Send PUBCOMP ───►        │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ Deliver to application   │
│ or local subscribers     │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_remove_      │
│ incoming()               │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__msg_store_free()     │
└──────────────────────────┘
```

### Flow 5: Client Reconnection (Persistent Session)

```
┌──────────────────────────┐
│ Client reconnects        │
│ clean_session = false    │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ handle__connect()        │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_reconnect_   │
│ reset()                  │
│ - Keep messages          │
│ - Reset states           │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__expire_all_          │
│ messages()               │
│ - Remove expired         │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_write_       │
│ inflight_out_all()       │
│ - Retransmit in-flight   │
│ - Set DUP = 1            │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_write_       │
│ queued_out()             │
│ - Send queued messages   │
└──────────────────────────┘
```

### Flow 6: Subscription and Retained Messages

```
┌──────────────────────────┐
│ ◄─── SUBSCRIBE received  │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ handle__subscribe()      │
│ - Add subscription       │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ For each matching        │
│ retained message:        │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_insert()     │
│ - Queue retained msg     │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_write_       │
│ inflight_out_latest()    │
│ - Send immediately       │
│ - Bypass queue order     │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_write_       │
│ queued_out()             │
│ - Send other queued      │
└────────┬─────────────────┘
         │
         ▼
┌──────────────────────────┐
│ db__message_write_       │
│ queued_in()              │
│ - Deliver queued incoming│
└──────────────────────────┘
```

## Key Architectural Patterns

### 1. Two-Level Message Storage
```
Global Store (shared):          Per-Client Queue:
┌─────────────────┐            ┌──────────────────┐
│ msg_store       │◄───────────┤ client_msg       │
│ - db_id         │            │ - store ptr      │
│ - topic         │            │ - mid            │
│ - payload       │            │ - state          │
│ - ref_count     │            │ - direction      │
└─────────────────┘            └──────────────────┘
      1 stored msg                 N client refs
```

### 2. Flow Control via Inflight Quota
```
max_inflight_messages = 20

Current state:
┌────────┬────────┬────────┬────────┬─────────────┐
│ In-    │ In-    │ In-    │ Queued │   Queued    │
│ flight │ flight │ flight │  (17)  │    (18)     │
│  (1)   │  (2)   │  (3)   │ ◄──────┴─────────────┘
└────────┴────────┴────────┘  Waiting for slot
         ▲
         │
  db__ready_for_flight() checks this
```

### 3. State Machine Per Message
```
QoS 1 States:
  queued → publish_qos1 → wait_for_puback → [deleted]

QoS 2 States (outgoing):
  queued → publish_qos2 → wait_for_pubrec → wait_for_pubcomp → [deleted]

QoS 2 States (incoming):
  wait_for_pubrel → queued → [delivered] → [deleted]
```

### 4. Reference Counting for Memory Management
```
Message Store ref_count tracking:

Initial: ref_count = 0
         │
         ▼
Client A subscribes: ref_count = 1
         │
         ▼
Client B subscribes: ref_count = 2
         │
         ▼
Client A receives: ref_count = 1
         │
         ▼
Client B receives: ref_count = 0
         │
         ▼
[Message freed by db__msg_store_free()]
```

## Critical Timing and Interactions

### Inflight Slot Release Triggers Next Send
```c
// In db__message_delete_outgoing():
1. Remove message from queue
2. inflight_count--
3. Call db__message_write_queued_out()  ← Immediately try to send next
```

### Persistence Save Points
Every state change calls:
```c
persist__client_msg_write()  // Save to disk
```
Saved at:
- Message insert
- State update
- Message delete
- Context disconnect

### Timeout and Retry Logic
```c
// In main loop (periodic check):
for each client:
    for each in-flight message:
        if (now - msg->timestamp > retry_interval):
            resend_with_dup_flag()
            msg->timestamp = now
```

## Integration Points for Yunetas Implementation

When implementing similar functionality in Yunetas:

1. **Message Storage Strategy**
   - Consider using `treedb` for message store (key = db_id)
   - Use `tranger` for time-series persistence
   - Implement ref_count in message metadata

2. **Queue Management**
   - Use Yunetas double-linked lists for msgs_in/msgs_out
   - Leverage Yunetas event-driven architecture for state transitions
   - Use timers for retry/timeout logic

3. **Flow Control**
   - Implement `ready_for_flight` check in publishing event handler
   - Use Yunetas quota system for max_inflight tracking

4. **Persistence**
   - Use `treedb` save_node() after each state change
   - Implement recovery in gclass initialization

5. **State Machine**
   - Define message states as Yunetas events
   - Use gclass FSM for state transitions
   - Each state change = event emission

---

## Summary

This comprehensive analysis documents Mosquitto's complete message queue and database management system, covering:

- **20+ core functions** for message lifecycle management
- **6 complete message flow diagrams** for different QoS levels and scenarios
- **Key architectural patterns**: two-level storage, reference counting, flow control
- **State machines** for QoS 1 and QoS 2 protocols
- **Critical interactions** between functions
- **Integration guidance** for Yunetas implementation

The system ensures reliable message delivery through careful state management, flow control, persistence, and reference-counted memory management.
