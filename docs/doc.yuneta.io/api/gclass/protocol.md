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
| `timeout_inactivity` | `integer` | Inactivity timeout in milliseconds (`60000`). |
| `cert_pem` | `string` | TLS certificate (PEM). |
| `crypto` | `json` | TLS config forwarded to the bottom C_TCP for `https://` URLs. Default `{"ssl_use_system_ca": true, "ssl_verify_mode": "required"}` — verifies the server cert against the system CA. Override with `ssl_trusted_certificate` for a private CA, or `ssl_allow_insecure_client: true` to skip verification (MITM risk). |
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
| **States** | `ST_DISCONNECTED`, `ST_WAIT_HANDSHAKE`, `ST_WAIT_FRAME_HEADER`, `ST_WAIT_PAYLOAD` |
| **Input events** | `EV_SEND_MESSAGE`, `EV_CONNECTED`, `EV_DISCONNECTED`, `EV_RX_DATA`, `EV_TX_READY`, `EV_DROP`, `EV_TIMEOUT` |
| **Output events** | `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | Connection URL. |
| `iamServer` | `bool` | `TRUE` for server mode. |
| `max_pkt_size` | `integer` | Largest payload of a frame, in bytes (default `0`: what a gbuffer holds, the max block of the yuno less one byte; a bigger value is capped to that). A frame announced bigger closes the connection, with a warning (*"tcp4h head too long"*, with `len` and `max`). Up to 7.25.22 the default was the max block itself, and a frame of exactly that length never ended. |
| `timeout_handshake` | `integer` | Handshake timeout in milliseconds (default `30000`). |
| `timeout_payload` | `integer` | Milliseconds to receive the rest of a frame whose header came (default `5000`); when it runs out the connection is closed. |
| `timeout_close` | `integer` | Milliseconds a closing client waits for the server to drop the connection before it drops it itself (default `3000`). |
| `cert_pem` | `string` | TLS certificate (PEM). |

The length of a frame is the peer's word, in the 4-byte header (big-endian,
counting the header itself) before any of its payload. The payload buffer
starts at 4 KB and grows with what arrives, up to what the frame needs; up to
7.25.21 the whole length was reserved at the header, so a peer that wrote only
headers announcing the max block reserved it each time, with no payload sent.

```C
uint32_t len = htonl(payload_len + 4);      // the header counts itself
gbuffer_append(gbuf, &len, sizeof(len));
gbuffer_append(gbuf, payload, payload_len);
```

`tests/c/c_prot_tcp4h` (`test1`): a frame of 60000 bytes in three parts
arrives whole, and the header of a frame of 10 MB with no payload does not
grow the yuno's memory.

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
| **States** | `ST_DISCONNECTED`, `ST_WAIT_HANDSHAKE`, `ST_WAIT_FRAME_HEADER`, `ST_WAIT_PAYLOAD` |
| **Input events** | `EV_SEND_MESSAGE`, `EV_CONNECTED`, `EV_DISCONNECTED`, `EV_RX_DATA`, `EV_TX_READY`, `EV_DROP`, `EV_TIMEOUT` |
| **Output events** | `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `url` | `string` | WebSocket URL (`ws://` or `wss://`). |
| `iamServer` | `bool` | `TRUE` for server mode. |
| `cert_pem` | `string` | TLS certificate (PEM). |
| `timeout_handshake` | `integer` | Handshake timeout in milliseconds (default `30000`). |
| `timeout_payload` | `integer` | Milliseconds to receive the rest of a frame whose header came (default `5000`); when it runs out the connection is closed. |
| `timeout_close` | `integer` | Milliseconds a closing client waits for the server to drop the connection before it drops it itself (default `3000`). |
| `pingT` | `integer` | Ping interval in milliseconds (`0` = disabled). |
| `max_payload_size` | `integer` | Largest payload of a frame, in bytes (default `0`: what a gbuffer holds, the max block of the yuno less one byte; a bigger value is capped to that -- up to 7.25.22 the default was the max block itself, and a frame of exactly that length never ended). A frame announced bigger is refused: a warning (*"Websocket frame bigger than the max payload, connection closed"*, with `frame_length`) and the connection closed with `1009`. |

The length of a frame is the peer's word, written in its header before any of
its payload. The payload buffer starts at 4 KB and grows with what arrives, up
to what the frame needs (a frame of exactly `max_payload_size` included: up to
7.25.21 it never ended, a gbuffer holding one byte less than its max); up to
7.25.21 the whole length was reserved at the header, so a few connections that
each announced the max block reserved it each with a few bytes sent.

```json
{
    "name": "input",
    "gclass": "C_WEBSOCKET",
    "kw": {
        "iamServer": true,
        "max_payload_size": 1048576
    }
}
```

### Closing

`ws_close()` -- a timeout, a protocol error, a received Close -- moves the
gobj to `ST_DISCONNECTED` at once and sends its Close frame. A **server** drops
the TCP connection there and then. A **client** keeps it, as RFC 6455 asks:
the server closes the TCP connection (7.1.1), and an endpoint that sent a
Close may go on receiving (5.5.1). The client drops it itself when
`timeout_close` runs out (warning *"Timeout waiting websocket
disconnected"*).

What arrives in between -- the rest of the frame the client gave up on, the
server's own Close -- belongs to a session that is over: it is discarded, and
nothing is published (the `debug` trace level logs it). Up to 7.25.10
`ST_DISCONNECTED` did not declare `EV_RX_DATA`, and each such read logged
*"Event NOT DEFINED in state"*; a controller too busy to read a frame in time
showed it. A client configured to give a slow peer more time:

```C
{
    "name": "output",
    "gclass": "C_WEBSOCKET",
    "kw": {
        "timeout_payload": 10000,
        "timeout_close": 5000
    }
}
```

Test: `tests/c/c_websocket` (`test_websocket_test1`).
