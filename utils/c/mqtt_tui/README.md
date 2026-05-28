# mqtt_tui

Terminal-UI MQTT client built on Yuneta. Useful for interactive
broker exploration, manual subscribe/publish, MQTT v5 property
inspection, and OAuth2-authenticated MQTT testing.

Speaks MQTT v3.1 / v3.1.1 / v5.0 via the shared
[MQTT module](../../../modules/c/mqtt/) (`C_PROT_MQTT2`). Drives the
session itself (one yuno = one client). Optional OIDC integration to
fetch a JWT from a Keycloak / Auth0 / Azure-AD realm and present it as
the MQTT password.

## Usage

```bash
mqtt_tui [OPTIONS]
```

Run `mqtt_tui --help` for the full option list. The non-trivial groups:

### MQTT keys

| Option                              | Description                                                                 |
|-------------------------------------|-----------------------------------------------------------------------------|
| `-c`, `--mqtt-persistent-session`   | Persistent session (`clean_session=0`). v5 clients can refine with `-x`.   |
| `-d`, `--mqtt-persistent-db`        | Use persistent DB for inflight / queued messages on the client side.       |
| `-i`, `--id=CLIENT_ID`              | MQTT Client ID.                                                            |
| `-q`, `--mqtt_protocol=PROTOCOL`    | `mqttv5` \| `mqttv311` \| `mqttv31`. Default: `v5`.                        |
| `-e`, `--mqtt_connect_properties='{...}'` | v5 CONNECT properties as JSON (e.g. `{"receive-maximum":100}`).      |
| `-x`, `--mqtt_session_expiry_interval=SECONDS` | v5 session-expiry-interval (0–4294967294, or -1 / ∞ for never).  |
| `-a`, `--mqtt_keepalive=SECONDS`    | Keepalive (default 60).                                                    |
| `--will-topic=TOPIC`                | MQTT Will topic.                                                           |
| `--will-payload=PAYLOAD`            | MQTT Will payload.                                                         |
| `--will-qos=QOS` / `--will-retain=N` | Will QoS and retain flag.                                                 |
| `--will-properties='{...}'`         | v5 will properties JSON.                                                   |

### OAuth2 keys

| Option                              | Description                                                                 |
|-------------------------------------|-----------------------------------------------------------------------------|
| `-I`, `--issuer=ISSUER`             | OIDC issuer URL (`https://auth.example.com/realms/foo/`). Triggers discovery. Leave empty for plain MQTT username/password. |
| `-T`, `--token-endpoint=URL`        | Explicit token endpoint. Skips discovery when paired with `-E`.            |
| `-E`, `--end-session-endpoint=URL`  | Explicit OIDC end_session endpoint.                                        |
| `-Z`, `--client-id=CLIENT_ID`       | OAuth2 client_id.                                                          |
| `-u`, `--user_id=USER_ID`           | MQTT username or OAuth2 user id.                                           |
| `-U`, `--user_passw=USER_PASSW`     | OAuth2 user password (for the token request).                              |
| `-P`, `--mqtt_passw=MQTT_PASSW`     | Plain MQTT password (no JWT path).                                         |
| `-j`, `--jwt=JWT`                   | Pre-obtained JWT (skip the auth dance).                                    |

### Connection keys

| Option                              | Description                                                                 |
|-------------------------------------|-----------------------------------------------------------------------------|
| `-h`, `--url-mqtt=URL-MQTT`         | MQTT broker URL. Default `mqtt://127.0.0.1:1810`.                          |
| `-b`, `--url-broker=URL-BROKER`     | Broker WS URL. Default `ws://127.0.0.1:1800`.                              |

### Local keys

| Option                              | Description                                                                 |
|-------------------------------------|-----------------------------------------------------------------------------|
| `-p`, `--print`                     | Print final merged configuration and exit.                                 |
| `-r`, `--print-role`                | Print basic yuno information (role + version).                             |
| `-l`, `--verbose=LEVEL`             | Verbose level.                                                             |
| `-v`, `--version`                   | Print yuno version.                                                        |
| `-V`, `--yuneta-version`            | Print Yuneta version.                                                      |
| `-m`, `--with-metadata`             | Print with metadata.                                                       |
| `--list-mqtt-properties`            | List every MQTT v5 property name accepted by `--mqtt_connect_properties` / `--will-properties` and exit. |

## How auth works (with OIDC)

```
mqtt_tui --issuer=<URL> --client-id=<ID> -u <USER> -U <PASS>
                          │
                          ▼
                  GET <issuer>.well-known/openid-configuration
                          │
                          ▼
                  POST <token_endpoint>
                  grant_type=password
                  client_id=<ID> username=<USER> password=<PASS>
                          │
                          ▼
                  JWT comes back  →  used as the MQTT password
                          │
                          ▼
                  CONNECT to the broker with username + JWT-as-password
```

`--jwt=<token>` skips the dance — useful when you already obtained a
token elsewhere.

## Build & install

```bash
cd utils/c/mqtt_tui/build
make install
```

Installed into `/yuneta/bin`. Not deployed via the agent in the usual
flow — it's an interactive CLI you invoke from your shell.
