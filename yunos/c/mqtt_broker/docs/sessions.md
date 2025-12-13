# MQTT Client Identification and Session Management

## Question 1: How to know the client ID when receiving a subscribed message?

In MQTT, when you receive a message from a subscription, the message itself **does not include the client ID of the publisher** by default. This is a fundamental aspect of MQTT's publish/subscribe model - it provides decoupling between publishers and subscribers.

However, there are several approaches to identify the source:

### 1. Topic-based identification (Most common)
Structure your topic hierarchy to include the client/device identifier:
```
devices/client123/temperature
devices/client456/humidity
```

When you receive the message, parse the topic to extract the client ID.

### 2. Include client ID in the message payload
Have publishers include their identifier in the message payload:
```json
{
  "client_id": "client123",
  "temperature": 25.5,
  "timestamp": 1702468800
}
```

### 3. Use MQTT 5.0 User Properties
If using MQTT 5.0, you can add custom user properties to messages:
```c
// Publisher adds property
mosquitto_property_add_string_pair(&props, 
    MQTT_PROP_USER_PROPERTY, 
    "client_id", 
    "client123");

// Subscriber reads property
char *client_id = mosquitto_property_read_string(props, 
    MQTT_PROP_USER_PROPERTY, 
    "client_id", 
    NULL);
```

### In Yunetas context
When implementing this in your Yunetas MQTT gclass, you'd typically:
- Parse the topic in your message callback to extract identifiers
- Or decode the payload to get client information
- Store this information in your event data structure for processing

The topic-based approach is most widely used because it works with all MQTT versions and integrates naturally with subscription patterns.

---

## Question 2: Do published topics clear on a new client session?

No, **published topics do NOT automatically clear** when a client reconnects or starts a new session. Here's what actually happens:

### Retained Messages
Messages published with the **retained flag** will persist on the broker and be delivered to new subscribers regardless of publisher sessions:
- Retained messages stay until explicitly cleared (publish empty payload with retain=true)
- They're independent of the publisher's connection state
- New subscribers immediately get the last retained message for matching topics

### Regular (Non-Retained) Messages
- Only delivered to currently connected subscribers
- Once delivered, they're gone
- New sessions won't see old messages

### Session State (Clean Session Flag)
The `clean_session` flag affects **subscriber** state, not published topics:

**clean_session = false:**
- Broker maintains subscriptions
- Queues QoS 1/2 messages while client is offline
- Client receives missed messages on reconnect

**clean_session = true:**
- Subscriptions deleted on disconnect
- No message queuing
- Fresh start each session

### To Clear Published Topics
If you want to clear retained messages when a client disconnects, you must:

```c
// Explicitly clear retained message
mosquitto_publish(mosq, NULL, "topic/path", 
    0,           // payload length = 0
    NULL,        // empty payload
    0,           // qos
    true);       // retain = true
```

In your Yunetas MQTT gclass implementation, you'd typically do this in your disconnect/cleanup handler if you want topics cleared when clients go offline.

**Bottom line:** Topics and their retained messages persist independently of client sessions unless you explicitly clear them.

---

## Question 3: With clean_session = true, does the broker delete subscriptions on disconnect?

Yes, **absolutely correct**! With `clean_session = true` (MQTT 3.1.1) or `clean_start = true` (MQTT 5.0):

### What the Broker MUST Do on Disconnect:

1. **Delete all subscriptions** made by that client
2. **Discard the session state** entirely
3. **Drop any queued QoS 1 and QoS 2 messages** for that client

### On Reconnect with clean_session = true:

- Client starts with a **completely fresh session**
- **No subscriptions** exist (must re-subscribe to everything)
- **No pending messages** from previous session
- New session identifier assigned

### Example Flow:

```
Connect (clean_session=true, client_id="device123")
  → Broker creates new session

Subscribe to "sensors/#"
  → Broker stores subscription

Disconnect
  → Broker DELETES subscription to "sensors/#"
  → Broker DELETES entire session for "device123"

Reconnect (clean_session=true, client_id="device123")
  → Brand new session, NO subscriptions
  → Must subscribe to "sensors/#" again
```

### In Contrast: clean_session = false

- Broker **preserves** subscriptions across disconnects
- Broker **queues** QoS 1/2 messages while offline
- Session persists until client explicitly disconnects with clean_session=true

So yes, you're right - with `clean_session = true`, the broker is **required by the MQTT specification** to delete all subscriptions when the client disconnects.
