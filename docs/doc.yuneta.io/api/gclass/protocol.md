# Protocol GClasses

Message framing and protocol parsing. Protocol GClasses sit between a
transport (C_TCP) and the application logic, translating raw bytes into
structured events.

**Source:** `kernel/c/root-linux/src/c_prot_http_cl.c`, `c_prot_http_sr.c`,
`c_prot_tcp4h.c`, `c_prot_raw.c`, `c_websocket.c`

---

(gclass-c-prot-http-cl)=
## C_PROT_HTTP_CL

HTTP client protocol — sends requests and parses responses.
Can deliver the body incrementally (`raw_body_data`) or as a complete
message.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_DISCONNECTED`, `ST_WAIT_CONNECTED`, `ST_CONNECTED` |
| **Input events** | `EV_SEND_MESSAGE`, `EV_CONNECTED`, `EV_DISCONNECTED`, `EV_RX_DATA`, `EV_TX_READY`, `EV_TIMEOUT` |
| **Output events** | `EV_ON_MESSAGE`, `EV_ON_HEADER`, `EV_ON_OPEN`, `EV_ON_CLOSE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | HTTP endpoint URL. |
| `timeout_inactivity` | `integer` | Inactivity timeout in seconds. |
| `cert_pem` | `string` | TLS certificate (PEM). |
| `raw_body_data` | `bool` | `TRUE` to deliver body chunks incrementally. |
| `subscriber` | `pointer` | Gobj receiving output events. |

---

(gclass-c-prot-http-sr)=
## C_PROT_HTTP_SR

HTTP server protocol — parses incoming HTTP requests and produces
structured events for the application.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_DISCONNECTED`, `ST_CONNECTED` |
| **Input events** | `EV_SEND_MESSAGE`, `EV_CONNECTED`, `EV_DISCONNECTED`, `EV_RX_DATA`, `EV_TX_READY` |
| **Output events** | `EV_ON_MESSAGE`, `EV_ON_HEADER`, `EV_ON_OPEN`, `EV_ON_CLOSE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `timeout_inactivity` | `integer` | Inactivity timeout in seconds. |
| `subscriber` | `pointer` | Gobj receiving output events. |

---

(gclass-c-prot-tcp4h)=
## C_PROT_TCP4H

Frame protocol with a 4-byte binary header containing the payload length.
Works in both client and server modes.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_DISCONNECTED`, `ST_WAIT_CONNECTED`, `ST_WAIT_HANDSHAKE`, `ST_CONNECTED` |
| **Input events** | `EV_SEND_MESSAGE`, `EV_CONNECTED`, `EV_DISCONNECTED`, `EV_RX_DATA`, `EV_TX_READY`, `EV_DROP`, `EV_TIMEOUT` |
| **Output events** | `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | Connection URL. |
| `iamServer` | `bool` | `TRUE` for server mode. |
| `max_pkt_size` | `integer` | Maximum allowed packet size. |
| `timeout_handshake` | `integer` | Handshake timeout in seconds. |
| `timeout_payload` | `integer` | Payload reception timeout. |
| `cert_pem` | `string` | TLS certificate (PEM). |

---

(gclass-c-prot-raw)=
## C_PROT_RAW

Raw pass-through protocol — forwards data without adding or removing
headers. Useful for transparent proxying.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_DISCONNECTED`, `ST_CONNECTED` |
| **Input events** | `EV_SEND_MESSAGE`, `EV_CONNECTED`, `EV_DISCONNECTED`, `EV_RX_DATA`, `EV_TX_READY` |
| **Output events** | `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `subscriber` | `pointer` | Gobj receiving output events. |

---

(gclass-c-websocket)=
## C_WEBSOCKET

WebSocket protocol (RFC 6455) — supports both client and server roles
with frame masking, ping/pong, and graceful close handshake.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_DISCONNECTED`, `ST_WAIT_CONNECTED`, `ST_WAIT_HANDSHAKE`, `ST_CONNECTED` |
| **Input events** | `EV_SEND_MESSAGE`, `EV_CONNECTED`, `EV_DISCONNECTED`, `EV_RX_DATA`, `EV_TX_READY`, `EV_DROP`, `EV_TIMEOUT` |
| **Output events** | `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | WebSocket URL (`ws://` or `wss://`). |
| `iamServer` | `bool` | `TRUE` for server mode. |
| `cert_pem` | `string` | TLS certificate (PEM). |
| `timeout_handshake` | `integer` | Handshake timeout in seconds. |
| `timeout_payload` | `integer` | Payload reception timeout. |
| `pingT` | `integer` | Ping interval in seconds (`0` = disabled). |
