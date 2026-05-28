# Built-in Yunos

Deployable services (daemons) shipped with the Yuneta kernel.
Binaries are installed into `$YUNETAS_OUTPUTS/yunos/` by `yunetas build`.

**Source:** `yunos/c/`

Each yuno below has its own anchored entry, so it can be linked directly
(e.g. `[](#yuno-mqtt_broker)`) and shows up individually in the page outline.

## Agent & Management

(yuno-yuneta_agent)=
### `yuneta_agent`

Primary Yuneta agent — manages the lifecycle of yunos on the local machine
(start/stop/update), exposes a WebSocket control interface, handles realms and
authentication, and coordinates inter-yuno communication. This is the yuno that
`ycommand`, `ystats`, and `ybatch` talk to by default.

(yuno-yuneta_agent22)=
### `yuneta_agent22`

Secondary agent (backdoor management channel) controlled by `controlcenter`.
Uses a PTY for advanced remote administration. Enable only on hosts reachable
from a control center. Also builds `yuneta_agent44`.

(yuno-controlcenter)=
### `controlcenter`

Central management console for distributed Yuneta systems — connects to running
yunos, shows real-time status, forwards events. Pairs with `yuneta_agent22` for
remote management.

(yuno-logcenter)=
### `logcenter`

Centralized logging yuno — aggregates logs from other yunos via UDP, applies
rotation and filtering. Typically paired with the `to_udp` log handler
(`udp://127.0.0.1:1992`).

## Messaging & Protocol

(yuno-mqtt_broker)=
### `mqtt_broker`

Full MQTT v3.1.1 + v5.0 broker with persistence backed by timeranger2 for
sessions, subscriptions, and queues.

(yuno-sgateway)=
### `sgateway`

Simple bidirectional gateway — forwards data between an input URL and an output
URL (TCP or WebSocket) with optional TLS. Useful for protocol bridging,
tunnelling, or fan-in/fan-out.

## Authentication

(yuno-auth_bff)=
### `auth_bff`

OAuth 2 / OpenID Connect Backend-For-Frontend yuno — mediates between browser
SPAs and an identity provider (Keycloak, Google, …) implementing
authorization-code flow with PKCE. Tokens are stored in httpOnly cookies
(SEC-04/06/07/09 compliant).

## Data & Infrastructure

(yuno-dba_postgres)=
### `dba_postgres`

PostgreSQL database administrator yuno — provides connection pooling, query
execution, and data-access services. Requires `CONFIG_MODULE_POSTGRES=y` and
`libpq`.

(yuno-emailsender)=
### `emailsender`

E-mail sending yuno — speaks native SMTPS (implicit TLS, RFC 8314) on top of
`C_SMTP_SESSION` + `C_TCP`, with its own MIME encoder. Queues outgoing messages
with TimeRanger2 persistence. Builds fully static since 7.4.3.

## Development & Testing

(yuno-watchfs)=
### `watchfs`

Directory watcher yuno — executes a shell command when files matching
configurable regex patterns are modified. Ideal for auto-rebuild loops during
development.

(yuno-emu_device)=
### `emu_device`

Device/sensor emulator — simulates device-gate connections for testing ingest
pipelines and device-facing GClasses without real hardware.
