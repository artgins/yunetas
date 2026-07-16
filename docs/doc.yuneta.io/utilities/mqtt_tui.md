(util-mqtt_tui)=
# `mqtt_tui`

`mqtt_tui` is a terminal-UI MQTT client built on Yuneta's MQTT module
(`C_PROT_MQTT2`). It drives the session itself — one yuno = one client — and is
handy for interactive broker exploration, manual subscribe/publish, MQTT v5
property inspection, and OAuth2-authenticated MQTT testing. It speaks
**v3.1 / v3.1.1 / v5.0**.

## Usage

```bash
mqtt_tui [OPTIONS]
```

Run `mqtt_tui --help` for the full list. The meaningful groups:

### MQTT session

| Option | Short | Purpose |
|--------|-------|---------|
| `--id=<CLIENT_ID>` | `-i` | MQTT client id |
| `--mqtt_protocol=<p>` | `-q` | `mqttv5` \| `mqttv311` \| `mqttv31` (default `v5`) |
| `--mqtt-persistent-session` | `-c` | Persistent session (`clean_session=0`) |
| `--mqtt-persistent-db` | `-d` | Persist inflight/queued messages client-side |
| `--mqtt_session_expiry_interval=<s>` | `-x` | v5 session expiry (`-1`/`∞` = never) |
| `--mqtt_keepalive=<s>` | `-a` | Keepalive seconds (default 60) |
| `--mqtt_connect_properties='{…}'` | `-e` | v5 CONNECT properties as JSON |
| `--will-topic` / `--will-payload` / `--will-qos` / `--will-retain` / `--will-properties` | | Last Will & Testament (v5 will properties as JSON) |

### Connection & auth

`--url`/`-u`, plus the same OAuth2 / OIDC flags as [`ycommand`](#util-ycommand) (`--issuer`/`-I`,
`--token-endpoint`/`-T`, `--client-id`/`-Z`, …). With OIDC, `mqtt_tui` fetches a
JWT from the realm and presents it as the MQTT password; otherwise plain MQTT
username/password is used. See [Authentication (OAuth2 / OIDC)](#ycommand-auth).

## See also

- [`mqtt_broker`](../yunos/mqtt_broker.md) — the broker side.
- [`utils/c/mqtt_tui/README.md`](https://github.com/artgins/yunetas/blob/7.8.0/utils/c/mqtt_tui/README.md).
