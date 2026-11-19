# mqtt_tui

`mqtt_tui` is an MQTT client with a terminal UI, built on Yuneta. Use it to
explore a broker, to subscribe and publish by hand, to read MQTT v5
properties, and to test MQTT with OAuth2 authentication.

It speaks MQTT v3.1, v3.1.1 and v5.0 through the shared
[MQTT module](../../../modules/c/mqtt/) (`C_PROT_MQTT2`). It drives the
session itself (one yuno is one client). As an option, it can get a JWT
from a Keycloak, Auth0 or Azure-AD realm with OIDC, and send it as
the MQTT password.

## Usage

```bash
mqtt_tui [OPTIONS]
```

Run `mqtt_tui --help` for the full option list. These groups need an explanation:

### MQTT keys

| Option                              | Description                                                                 |
|-------------------------------------|-----------------------------------------------------------------------------|
| `-c`, `--mqtt-persistent-session`   | Persistent session (`clean_session=0`). A v5 client can tune it with `-x`. |
| `-d`, `--mqtt-persistent-db`        | Use a persistent DB for inflight and queued messages on the client side.   |
| `-i`, `--id=CLIENT_ID`              | MQTT Client ID.                                                            |
| `-q`, `--mqtt_protocol=PROTOCOL`    | `mqttv5` \| `mqttv311` \| `mqttv31`. Default: `v5`.                        |
| `-e`, `--mqtt_connect_properties='{...}'` | v5 CONNECT properties as JSON (for example `{"receive-maximum":100}`). |
| `-x`, `--mqtt_session_expiry_interval=SECONDS` | v5 session-expiry-interval (0–4294967294, or -1 / ∞ for never).  |
| `-a`, `--mqtt_keepalive=SECONDS`    | Keepalive (default 60).                                                    |
| `--will-topic=TOPIC`                | MQTT Will topic.                                                           |
| `--will-payload=PAYLOAD`            | MQTT Will payload.                                                         |
| `--will-qos=QOS` / `--will-retain=N` | Will QoS and retain flag.                                                 |
| `--will-properties='{...}'`         | v5 will properties JSON.                                                   |

### OAuth2 keys

| Option                              | Description                                                                 |
|-------------------------------------|-----------------------------------------------------------------------------|
| `-I`, `--issuer=ISSUER`             | OIDC issuer URL (`https://auth.example.com/realms/foo/`). It starts the discovery. For a plain MQTT username and password, leave it empty. |
| `-T`, `--token-endpoint=URL`        | Explicit token endpoint. With `-E`, the tool skips the discovery.          |
| `-E`, `--end-session-endpoint=URL`  | Explicit OIDC end_session endpoint.                                        |
| `-Z`, `--client-id=CLIENT_ID`       | OAuth2 client_id.                                                          |
| `-u`, `--user_id=USER_ID`           | MQTT username or OAuth2 user id.                                           |
| `-U`, `--user_passw=USER_PASSW`     | OAuth2 user password (for the token request).                              |
| `-P`, `--mqtt_passw=MQTT_PASSW`     | Plain MQTT password (no JWT path).                                         |
| `-j`, `--jwt=JWT`                   | A JWT that you already have (no token request).                            |

### Connection keys

| Option                              | Description                                                                 |
|-------------------------------------|-----------------------------------------------------------------------------|
| `-h`, `--url-mqtt=URL-MQTT`         | MQTT broker URL. Default `mqtt://127.0.0.1:1810`.                          |
| `-b`, `--url-broker=URL-BROKER`     | Broker WS URL. Default `ws://127.0.0.1:1800`.                              |

### Local keys

| Option                              | Description                                                                 |
|-------------------------------------|-----------------------------------------------------------------------------|
| `-p`, `--print`                     | Print the final merged configuration and exit.                             |
| `-r`, `--print-role`                | Print basic yuno information (role + version).                             |
| `-l`, `--verbose=LEVEL`             | Verbose level.                                                             |
| `-v`, `--version`                   | Print yuno version.                                                        |
| `-V`, `--yuneta-version`            | Print Yuneta version.                                                      |
| `-m`, `--with-metadata`             | Print with metadata.                                                       |
| `--list-mqtt-properties`            | List every MQTT v5 property name that `--mqtt_connect_properties` and `--will-properties` accept, and exit. |

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

`--jwt=<token>` skips these steps. Use it when you already have a token
from another source.

## Build & install

```bash
cd utils/c/mqtt_tui/build
make install
```

`make install` copies it into `/yuneta/bin`. The agent does not deploy it in
the usual flow. It is an interactive CLI that you start from your shell.
