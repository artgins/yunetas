(module-mqtt)=
# MQTT

**Kconfig:** `CONFIG_MODULE_MQTT` — **GClasses:** `C_PROT_MQTT`, `C_PROT_MQTT2`, `C_MQTT_BROKER`

Full MQTT protocol implementation (v3.1.1 + v5.0) with a persistent message
broker backed by TreeDB.

## C_PROT_MQTT / C_PROT_MQTT2

MQTT protocol with publish, subscribe, QoS levels, and event-driven
message handling. `C_PROT_MQTT2` is the current implementation (and works as
both the broker's per-connection protocol and a standalone client — see
[`mqtt_tui`](../utilities/mqtt_tui.md)). `C_PROT_MQTT` is the legacy version
kept for backward compatibility.

**Trace levels (`C_PROT_MQTT2`):** `traffic` (packets, no payload),
`traffic-payload`, `show-decode` (decoded packet structure), `messages2`.

**Secrets:** `user_passw`, `jwt` and `auth_data` (the MQTT 5 AUTH data of the
connection, `C_PROT_MQTT` too) are `SDF_SECRET`: `view-attrs`, `view-gobj` and
`list-persistent-attrs` show them as `********` (an empty one stays empty).
Up to 7.25.20 `auth_data` showed in clear.

A frame the protocol refuses is dumped in the log, at every trace level
(*"MQTT malformed packet: <command>"*, length-capped). Of a CONNECT only the
head is dumped, the protocol name, level, flags and keep alive, and of an AUTH
only its reason code: the properties and the payload carry the credentials.
Up to 7.25.20 the whole CONNECT was dumped, password included.

```text
"msg": "MQTT malformed packet: CMD_CONNECT (credentials not dumped)",
"data": {"0000": "00 04 4D 51 54 54 05 C2 00 3C     ..MQTT...<"}
```

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=view-attrs gobj=<C_PROT_MQTT2 full name> attribute=auth_data'
# {"C_PROT_MQTT2^<name>": "********"}
```

## C_MQTT_BROKER

Full MQTT message broker — subscriber management, message routing,
session handling, and persistent storage using TreeDB on timeranger2.

**Trace levels:** `messages`, `messages2`.

Driven by the [`mqtt_broker`](../yunos/mqtt_broker.md) yuno — see that page for
the protocol/persistence/configuration details.

## C API

The helpers the three gclasses share have pages of their own:

- [MQTT utilities](#module-mqtt-util) (`mqtt_util.h`) -- names of the
  protocol's codes, validation of topics and UTF-8, the record of a message.
- [tr2q](#module-mqtt-tr2q) (`tr2q_mqtt.h`) -- the persistent queues of a
  session, on timeranger2.

## Registration

Register the gclasses a yuno uses once, in `register_yuno_and_more()`, before
creating them; a second call of the same one fails with *"GClass ALREADY
created"*. Each one returns `0`, or `-1` (logged) when the gclass cannot be
created.

```C
int register_c_prot_mqtt(void);
int register_c_prot_mqtt2(void);
int register_c_mqtt_broker(void);
```

The `mqtt_broker` yuno registers the protocol and the broker:

```C
static int register_yuno_and_more(void)
{
    register_c_prot_mqtt2();
    register_c_mqtt_broker();
    return 0;
}
```

(register_c_prot_mqtt)=
### `register_c_prot_mqtt()` — `C_PROT_MQTT`

The deprecated protocol, kept until the remaining gates migrate to
`C_PROT_MQTT2`.

(register_c_prot_mqtt2)=
### `register_c_prot_mqtt2()` — `C_PROT_MQTT2`

The MQTT protocol, broker side and client side (`mqtt_tui` registers it with
its own client gclass and `C_EDITLINE`, without the broker).

(register_c_mqtt_broker)=
### `register_c_mqtt_broker()` — `C_MQTT_BROKER`

The broker.
