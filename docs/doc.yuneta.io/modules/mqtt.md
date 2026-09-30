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
