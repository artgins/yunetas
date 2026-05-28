(module-mqtt)=
# MQTT

**Kconfig:** `CONFIG_MODULE_MQTT` — **GClasses:** `C_PROT_MQTT`, `C_PROT_MQTT2`, `C_MQTT_BROKER`

Full MQTT protocol implementation (v3.1.1 + v5.0) with a persistent message
broker backed by TreeDB.

## C_PROT_MQTT / C_PROT_MQTT2

MQTT protocol with publish, subscribe, QoS levels, and event-driven
message handling. `C_PROT_MQTT2` is the current implementation;
`C_PROT_MQTT` is the legacy version kept for backward compatibility.

## C_MQTT_BROKER

Full MQTT message broker — subscriber management, message routing,
session handling, and persistent storage using TreeDB on timeranger2.
Used by the `mqtt_broker` yuno.
