# Built-in Yunos

Deployable services (daemons) shipped with the Yuneta kernel.
Binaries are installed into `$YUNETAS_OUTPUTS/yunos/` by `yunetas build`.

**Source:** `yunos/c/`

## Agent & Management

| Binary | Description |
|--------|-------------|
| **`yuneta_agent`** | Primary Yuneta agent — manages the lifecycle of yunos on the local machine (start/stop/update), exposes a WebSocket control interface, handles realms and authentication, and coordinates inter-yuno communication. This is the yuno that `ycommand`, `ystats`, and `ybatch` talk to by default. |
| **`yuneta_agent22`** | Secondary agent (backdoor management channel) controlled by `controlcenter`. Uses a PTY for advanced remote administration. Enable only on hosts reachable from a control center. Also builds `yuneta_agent44`. |
| **`controlcenter`** | Central management console for distributed Yuneta systems — connects to running yunos, shows real-time status, forwards events. Pairs with `yuneta_agent22` for remote management. |
| **`logcenter`** | Centralized logging yuno — aggregates logs from other yunos via UDP, applies rotation and filtering. Typically paired with the `to_udp` log handler (`udp://127.0.0.1:1992`). |

## Interactive Clients

| Binary | Description |
|--------|-------------|
| **`ycli`** | Interactive ncurses-based terminal UI for browsing yunos, inspecting state, and sending commands — a friendlier alternative to raw `ycommand`. |
| **`mqtt_tui`** | MQTT client with a text-based UI. |

## Messaging & Protocol

| Binary | Description |
|--------|-------------|
| **`mqtt_broker`** | Full MQTT v3.1.1 + v5.0 broker with persistence backed by timeranger2 for sessions, subscriptions, and queues. |
| **`sgateway`** | Simple bidirectional gateway — forwards data between an input URL and an output URL (TCP or WebSocket) with optional TLS. Useful for protocol bridging, tunnelling, or fan-in/fan-out. |

## Authentication

| Binary | Description |
|--------|-------------|
| **`auth_bff`** | OAuth 2 / OpenID Connect Backend-For-Frontend yuno — mediates between browser SPAs and an identity provider (Keycloak, Google, …) implementing authorization-code flow with PKCE. Tokens are stored in httpOnly cookies (SEC-04/06/07/09 compliant). |

## Data & Infrastructure

| Binary | Description |
|--------|-------------|
| **`dba_postgres`** | PostgreSQL database administrator yuno — provides connection pooling, query execution, and data-access services. Requires `CONFIG_MODULE_POSTGRES=y` and `libpq`. |
| **`emailsender`** | E-mail sending yuno — uses libcurl for SMTP delivery on behalf of other services. **Cannot** be built as a fully-static binary. |

## Development & Testing

| Binary | Description |
|--------|-------------|
| **`watchfs`** | Directory watcher yuno — executes a shell command when files matching configurable regex patterns are modified. Ideal for auto-rebuild loops during development. |
| **`emu_device`** | Device/sensor emulator — simulates device-gate connections for testing ingest pipelines and device-facing GClasses without real hardware. |
