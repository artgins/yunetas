# MQTT QoS Message Persistence Guide

## What happens if the broker or client disconnect unexpectedly with QoS 1/2 messages?

The answer depends on whether you're using **persistent sessions** and the specific scenario.

## With Persistent Sessions (Clean Session = false)

**Messages are NOT lost** - the MQTT protocol is designed to handle this:

### If the CLIENT disconnects unexpectedly:

**QoS 1 messages:**
- Messages the client was sending to broker: Stored by client, will be resent on reconnect
- Messages broker was sending to client: Queued by broker, delivered when client reconnects
- If client hasn't sent PUBACK, broker will resend on reconnect

**QoS 2 messages:**
- The multi-step handshake (PUBLISH → PUBREC → PUBREL → PUBCOMP) resumes where it left off
- Both client and broker maintain state of in-flight messages
- Ensures exactly-once delivery even across disconnections

### If the BROKER crashes/restarts:

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

## Without Persistent Sessions (Clean Session = true)

**Messages ARE lost:**
- No session state is maintained
- No message queuing
- When client reconnects, it starts with a clean slate

## Example scenarios

### Scenario 1: Client crash with persistent session
```
1. Client subscribed to "sensors/temp" with QoS 1, persistent session
2. Client crashes
3. Broker receives 10 messages on "sensors/temp"
4. Broker queues all 10 messages
5. Client reconnects with same Client ID
6. ✅ Client receives all 10 queued messages
```

### Scenario 2: Broker crash WITHOUT disk persistence
```
1. Client disconnected, has persistent session
2. Broker queuing 100 messages for that client (in RAM only)
3. Broker crashes
4. Broker restarts
5. ❌ All 100 queued messages are lost
6. Client rec