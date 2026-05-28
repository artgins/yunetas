(module-mqtt)=
# MQTT

**Kconfig:** `CONFIG_MODULE_MQTT` — **GClasses:** `C_PROT_MQTT`, `C_PROT_MQTT2`, `C_MQTT_BROKER`

Full MQTT protocol implementation (v3.1.1 + v5.0) with a persistent message
broker backed by TreeDB.

## C_PROT_MQTT / C_PROT_MQTT2

MQTT protocol with publish, subscribe, QoS levels, and event-driven
message handling. `C_PROT_MQTT2` is the current implementation (and works as
both the broker's per-connection protocol and a standalone client — see
[`mqtt_tui`](../utilities/mqtt_tui.md)); `C_PROT_MQTT` is the legacy version
kept for backward compatibility.

**Trace levels (`C_PROT_MQTT2`):** `traffic` (packets, no payload),
`traffic-payload`, `show-decode` (decoded packet structure), `messages2`.

## C_MQTT_BROKER

Full MQTT message broker — subscriber management, message routing,
session handling, and persistent storage using TreeDB on timeranger2.

**Trace levels:** `messages`, `messages2`.

Driven by the [`mqtt_broker`](../yunos/mqtt_broker.md) yuno — see that page for
the protocol/persistence/configuration details.
