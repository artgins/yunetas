# Inter-Event GClasses

RPC-like communication between yunos over the network. A local gobj
subscribes to a remote service as if it were local.

**Source:** `kernel/c/root-linux/src/c_ievent_cli.c`, `c_ievent_srv.c`

---

(gclass-c-ievent-cli)=
## C_IEVENT_CLI

Inter-event client — connects to a remote yuno and simulates its service
as a local gobj. Handles identity-card exchange, authentication, and
subscription management.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_DISCONNECTED`, `ST_WAIT_CONNECTED`, `ST_WAIT_IDENTITY_CARD`, `ST_SESSION`, `ST_SUBSCRIBED` |
| **Input events** | `EV_SEND_MESSAGE`, `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE`, `EV_TIMEOUT`, `EV_DROP`, `EV_STOPPED` |
| **Output events** | `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `wanted_yuno_role` | `string` | Role of the remote yuno to connect to. |
| `wanted_yuno_name` | `string` | Name of the remote yuno. |
| `wanted_yuno_service` | `string` | Service name on the remote yuno. |
| `url` | `string` | Connection URL. |
| `jwt` | `string` | JSON Web Token for authentication. |
| `timeout_idack` | `integer` | Identity-card ack timeout in seconds. |

---

(gclass-c-ievent-srv)=
## C_IEVENT_SRV

Inter-event server — entry gate for authenticated service access.
Manages incoming connections, routes inter-event messages to local
services, and handles WebSocket upgrade.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_DISCONNECTED`, `ST_WAIT_IDENTITY_CARD`, `ST_SESSION` |
| **Input events** | `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE`, `EV_TIMEOUT`, `EV_DROP`, `EV_STOPPED` |
| **Output events** | `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `__username__` | `string` | Authenticated username (read-only). |
| `__session_id__` | `string` | Session identifier (read-only). |
| `jwt_payload` | `json` | Decoded JWT payload. |
| `client_yuno_role` | `string` | Role of the connected client yuno. |
| `client_yuno_name` | `string` | Name of the connected client yuno. |
| `this_service` | `string` | Local service name this gate serves. |
| `authenticated` | `bool` | Whether the connection is authenticated. |
