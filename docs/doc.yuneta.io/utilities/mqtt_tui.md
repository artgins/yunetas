(util-mqtt_tui)=
# `mqtt_tui`

Interactive MQTT client with a text-based UI — connects to an MQTT broker and a
Yuneta broker simultaneously, with interactive publish / subscribe / unsubscribe,
MQTT v5 property inspection, and optional OIDC auth (fetches a JWT and presents
it as the MQTT password). Speaks v3.1 / v3.1.1 / v5.0 via `C_PROT_MQTT2`.
