(module-mqtt-util)=
# MQTT utilities

The helpers of the [MQTT module](#module-mqtt) that `C_PROT_MQTT`,
`C_PROT_MQTT2` and `C_MQTT_BROKER` share: names for the protocol's codes,
validation of topics and UTF-8 strings as the MQTT specification asks for
them, and the record of a message. A client gclass of its own (one built on
top of `C_PROT_MQTT2`) gets them by including `c_prot_mqtt2.h`, which includes
`mqtt_util.h`.

Source code:

- [`mqtt_util.h`](https://github.com/artgins/yunetas/blob/7.26.6/modules/c/mqtt/src/mqtt_util.h)
- [`mqtt_util.c`](https://github.com/artgins/yunetas/blob/7.26.6/modules/c/mqtt/src/mqtt_util.c)

Part of the code comes from the [Mosquitto](https://mosquitto.org/) project
(EPL-2.0 OR BSD-3-Clause), which is why some names keep its prefix.

---

(mqtt_connack_string)=
## `mqtt_connack_string()`

`mqtt_connack_string()` returns the text of an MQTT 3.1.1 CONNACK return code.

```C
const char *mqtt_connack_string(mqtt311_connack_codes_t connack_code);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `connack_code` | `mqtt311_connack_codes_t` | `CONNACK_ACCEPTED` (0) to `CONNACK_REFUSED_NOT_AUTHORIZED` (5). |

**Returns**

A static string, *"Connection Accepted."* or *"Connection Refused: ..."*
(*"Connection Refused: unknown reason."* for a code out of the table).

**Example**

```C
gobj_log_warning(gobj, 0,
    "function",     "%s", __FUNCTION__,
    "msgset",       "%s", MSGSET_MQTT,
    "msg",          "%s", "Mqtt connection refused",
    "reason",       "%s", mqtt_connack_string(CONNACK_REFUSED_BAD_USERNAME_PASSWORD),
    NULL
);
// "reason": "Connection Refused: bad user name or password."
```

---

(mqtt_reason_string)=
## `mqtt_reason_string()`

`mqtt_reason_string()` returns the text of an MQTT 5 reason code, the code
CONNACK, PUBACK, PUBREC, PUBREL, PUBCOMP, SUBACK, UNSUBACK, DISCONNECT and
AUTH carry.

```C
const char *mqtt_reason_string(mqtt5_reason_codes_t reason_code);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `reason_code` | `mqtt5_reason_codes_t` | An `MQTT_RC_*` value (`MQTT_RC_SUCCESS` = 0, `MQTT_RC_NOT_AUTHORIZED` = 135, ...). |

**Returns**

A static string with the name the specification gives the code (*"Not
authorized"*, *"Session taken over"*, ...), or *"Unknown reason"*.

**Example**

```C
printf("disconnected: %s\n", mqtt_reason_string(MQTT_RC_KEEP_ALIVE_TIMEOUT));
// disconnected: Keep Alive timeout
```

---

(mqtt_command_string)=
## `mqtt_command_string()`

`mqtt_command_string()` returns the name of an MQTT control packet type, the
form the traces and the *"MQTT malformed packet: <command>"* warnings print.

```C
const char *mqtt_command_string(mqtt_message_t command);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `command` | `mqtt_message_t` | The packet type with its flags masked out: `CMD_CONNECT` (`0x10`) to `CMD_AUTH` (`0xF0`). |

**Returns**

A static string, `"CMD_CONNECT"`, `"CMD_PUBLISH"`, ... or `"Unknown command"`
(also for `CMD_RESERVED`).

**Example** — name the packet whose first byte has just been read:

```C
uint8_t first_byte = 0x32;     // PUBLISH, QoS 1
const char *name = mqtt_command_string((mqtt_message_t)(first_byte & 0xF0));
// "CMD_PUBLISH"
```

---

(mqtt_property_identifier_to_string)=
## `mqtt_property_identifier_to_string()`

`mqtt_property_identifier_to_string()` returns the name of an MQTT 5 property,
in the kebab-case form the protocol uses as the key of that property in the
`properties` dict of a message.

```C
const char *mqtt_property_identifier_to_string(uint32_t identifier);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `identifier` | `uint32_t` | An `MQTT_PROP_*` value (`MQTT_PROP_CONTENT_TYPE` = 3, ...). |

**Returns**

A static string (`"content-type"`, `"message-expiry-interval"`,
`"user-property"`, ...). `NULL` for an unknown identifier, logged as an ERROR
*"Mqtt unknown property"* with the `identifier`: check it before using the
name.

**Example**

```C
const char *name = mqtt_property_identifier_to_string(identifier);
if(!name) {
    return -1;  // Error already logged
}
json_object_set_new(jn_properties, name, json_string(value));
```

---

(mqtt_pub_topic_check2)=
## `mqtt_pub_topic_check2()`

`mqtt_pub_topic_check2()` says whether a topic may be PUBLISHED to: it must
not be empty, nor longer than 65535 bytes, nor contain the wildcards `+` or
`#`, nor have more than `TOPIC_HIERARCHY_LIMIT` (200) `/` separators.

```C
int mqtt_pub_topic_check2(const char *topic, size_t topiclen);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `topic` | `const char *` | The topic, not necessarily null-terminated. |
| `topiclen` | `size_t` | Its length in bytes. |

**Returns**

`0` for a valid topic, `-1` for an invalid one. Nothing is logged: the caller
decides what an invalid topic means (a malformed packet from a peer, a bad
argument of a local call). It does not check UTF-8: call
[`mqtt_validate_utf8()`](#mqtt_validate_utf8) as well.

**Example**

```C
mqtt_pub_topic_check2("sensors/room1/temp", 18);    // 0
mqtt_pub_topic_check2("sensors/+/temp", 14);        // -1, a wildcard
mqtt_pub_topic_check2("", 0);                       // -1, empty
```

---

(mqtt_sub_topic_check2)=
## `mqtt_sub_topic_check2()`

`mqtt_sub_topic_check2()` says whether a topic filter may be SUBSCRIBED to.
The wildcards are allowed where the specification puts them: `+` must fill a
whole level (between `/`, or at either end), and `#` must be the whole last
level. The limits of length and hierarchy are the same as for
[`mqtt_pub_topic_check2()`](#mqtt_pub_topic_check2).

```C
int mqtt_sub_topic_check2(const char *topic, size_t topiclen);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `topic` | `const char *` | The topic filter, not necessarily null-terminated. |
| `topiclen` | `size_t` | Its length in bytes. |

**Returns**

`0` for a valid filter, `-1` for an invalid one, with nothing logged.

**Example**

```C
mqtt_sub_topic_check2("sensors/+/temp", 14);    // 0
mqtt_sub_topic_check2("sensors/#", 9);          // 0
mqtt_sub_topic_check2("sensors/ro+", 11);       // -1, '+' does not fill the level
mqtt_sub_topic_check2("sensors/#/temp", 14);    // -1, '#' is not the last level
```

---

(mqtt_validate_utf8)=
## `mqtt_validate_utf8()`

`mqtt_validate_utf8()` checks a string the way the MQTT specification asks of
every UTF-8 string of a packet: well-formed UTF-8 (no overlong or truncated
sequence, nothing above U+10FFFF, no UTF-16 surrogate), and none of the code
points it forbids -- U+0000, the control characters U+0001-U+001F and
U+007F-U+009F, and the non-characters (U+FDD0-U+FDEF and every U+xFFFE /
U+xFFFF).

```C
int mqtt_validate_utf8(const char *str, int len);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `str` | `const char *` | The bytes to check, not necessarily null-terminated. |
| `len` | `int` | Their number, `0` to `65536`. |

**Returns**

`0` for a valid string, `-1` for an invalid one (also for `str == NULL` or a
`len` out of range), with nothing logged.

**Example** — a topic received from a peer is checked twice. A bad one is
the peer's fault, so it is a warning with the peer's name, not an error:

```C
if(mqtt_validate_utf8(topic, (int)tlen) < 0 ||
        mqtt_pub_topic_check2(topic, tlen) < 0) {
    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_MQTT,
        "msg",          "%s", "Mqtt invalid topic",
        "peername",     "%s", gobj_read_str_attr(gobj_bottom_gobj(gobj), "peername"),
        NULL
    );
    return -1;
}
```

---

(new_mqtt_message)=
## `new_mqtt_message()`

`new_mqtt_message()` builds the record of a PUBLISH message, the json the
protocol hands to its subscriber with `EV_MQTT_MESSAGE` and the broker stores
in the session queues ([tr2q](#module-mqtt-tr2q)). It is used on both sides: a
client publishing, a broker receiving, a client receiving.

```C
json_t *new_mqtt_message(
    hgobj gobj,
    const char *client_id,
    const char *topic,
    gbuffer_t *gbuf_payload,    // owned
    uint8_t qos,
    uint16_t mid,
    BOOL retain,
    BOOL dup,
    json_t *properties,         // owned
    uint32_t expiry_interval,
    json_int_t t
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The gobj that logs on failure. |
| `client_id` | `const char *` | The client id of the session. |
| `topic` | `const char *` | The topic. |
| `gbuf_payload` | `gbuffer_t *` | The payload (owned), or `NULL` for an empty one. |
| `qos` | `uint8_t` | `0`, `1` or `2`. |
| `mid` | `uint16_t` | The packet identifier (`0` for QoS 0). |
| `retain` | `BOOL` | The retain flag. |
| `dup` | `BOOL` | The DUP flag of the PUBLISH, written as `dup`. It informs whoever reads the record; what goes on the wire when a message is sent again is the dup bit of its queue entry ([`msg_flag_get_dup()`](#module-mqtt-tr2q)), and the broker builds each subscriber's copy with `FALSE`, since the DUP of an incoming PUBLISH is not propagated [MQTT-3.3.1-3]. Up to 7.26.4 it was not written, and `dup` was always `false`. |
| `properties` | `json_t *` | The MQTT 5 properties as a dict (owned), or `NULL`. |
| `expiry_interval` | `uint32_t` | The message expiry interval, in seconds. |
| `t` | `json_int_t` | The time of the message, written as `tm`; the protocol passes [`mosquitto_time()`](#mosquitto_time). |

**Returns**

A new json, yours:

```json
{
    "topic": "sensors/room1/temp",
    "tm": 1759750000,
    "client_id": "sensor-17",
    "mid": 42,
    "qos": 1,
    "expiry_interval": 0,
    "retain": false,
    "properties": {},
    "gbuffer": 94418432102400,
    "dup": false,
    "state": 0
}
```

`gbuffer` is the payload's pointer as an integer, and the record owns it from
then on: decref the record with `KW_DECREF()`, which releases the gbuffer
with it (see *"`kw["gbuffer"]` is auto-decref'd"* in the framework rules).
`NULL` when the record cannot be created, logged as *"Mqtt publish: cannot
create the message"*; the payload and the properties are released then, as
anything owned is (up to 7.26.4 they leaked on that path).

**Example** — a client that publishes a reading:

```C
gbuffer_t *gbuf = gbuffer_create(64, 64);
gbuffer_printf(gbuf, "{\"temp\": %.1f}", 21.5);

json_t *kw_mqtt_msg = new_mqtt_message(
    gobj,
    client_id,
    "sensors/room1/temp",
    gbuf,       // owned
    1,          // qos
    0,          // mid, given by the protocol when sent
    FALSE,      // retain
    FALSE,      // dup
    NULL,       // properties
    0,          // expiry_interval
    mosquitto_time()
);
if(!kw_mqtt_msg) {
    return -1;  // Error already logged
}
```

---

(mosquitto_time)=
## `mosquitto_time()`

`mosquitto_time()` returns the time, in seconds, that the module stamps on
messages (`tm`) and compares with their expiry interval.

```C
time_t mosquitto_time(void);
```

**Parameters** — none.

**Returns**

`time(NULL)`: the seconds of the WALL clock, on purpose. What it returns is
persisted and compared after a restart -- the `tm` of a message, which is
also the `__t__` of its [tr2q](#module-mqtt-tr2q) record, the `tm` of a
retained message, the `will_delay_time` of a disconnected session -- and a
monotonic clock starts again at every boot. The price is that a clock set
back or forward moves the expiries computed with it by the same amount.

Up to 7.26.4 the function carried Mosquitto's monotonic branch
(`clock_gettime()`), and branches for Windows and macOS, under a guard that
was never true here (`_POSIX_TIMERS` comes from `<unistd.h>`, which the file
does not include, and the `time_clock` it used was defined nowhere): it
looked monotonic and was not. The value it returns has not changed.

**Example** — a queued message whose expiry passed while it waited is
dropped, as `C_PROT_MQTT2` does before sending it [MQTT-3.3.2-18]:

```C
uint32_t expiry_interval = (uint32_t)kw_get_int(gobj, kw_msg, "expiry_interval", 0, 0);
if(expiry_interval > 0) {
    time_t msg_time = (time_t)kw_get_int(gobj, kw_msg, "tm", 0, 0);
    time_t elapsed = mosquitto_time() - msg_time;
    if(elapsed >= (time_t)expiry_interval) {
        tr2q_unload_msg(qmsg, 0);   // expired: out of the queue
    } else {
        expiry_interval -= (uint32_t)elapsed;   // what is left goes on the wire
    }
}
```
