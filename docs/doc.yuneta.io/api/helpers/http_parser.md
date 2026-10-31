# HTTP Parser

Incremental HTTP/1.x parser used by the HTTP gclasses. Feeds bytes from a `gbuffer_t` and invokes callbacks on each complete header, body chunk, and message.

Source code:

- [`ghttp_parser.h`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ghttp_parser.h)
- [`ghttp_parser.c`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ghttp_parser.c)

### Limits

A parser holds the head of a message (the request target or the status text,
and the names and values of its headers) and, with `on_message_event`, its
body. Both have a limit, set in the struct after
[`ghttp_parser_create()`](#ghttp_parser_create):

| Field | Default (`0`) | What it limits |
|---|---|---|
| `max_header_size` | `GHTTP_MAX_HEADER_SIZE`, 64 KB | The bytes of the target or status text, the header names and the values of one message. Never more than half the max block. |
| `max_body_size` | half the max block (with `on_message_event`); no limit (with `on_body_event` alone, which holds nothing) | The bytes of the body of one message. Never more than half the max block with `on_message_event`. |

A message over a limit is refused: ONE warning (*"HTTP headers larger than
their limit, message refused"* with `header_received`, `max_header_size` and
the name of the `header`; or *"HTTP body larger than its limit, message
refused"* with `body_received`, `max_body_size`), `headers_refused` or
`body_refused` set, and [`ghttp_parser_received()`](#ghttp_parser_received)
answers `-1`. What then:

- a **request** parser publishes nothing; the server answers `431 Request
  Header Fields Too Large` or `413 Content Too Large` and drops the
  connection ([`C_PROT_HTTP_SR`](#gclass-c-prot-http-sr));
- a **response** parser publishes `on_message_event` without `body` nor
  `gbuffer`, with `"headers_too_large"` or `"body_too_large"`: the limit
  passed, so the request waiting for it is answered
  ([`C_PROT_HTTP_CL`](#gclass-c-prot-http-cl)). A response that breaks the
  protocol once begun is published the same way, with `"protocol_error"`:
  the reason of llhttp.

The head is put together in linear time: each byte is copied once, into a
buffer that grows by doubling, and a header goes into `jn_headers` once,
when its value ends. A header with an empty value is kept as `""`. A name
that comes again is one field, its values joined in their order with `", "`
(RFC 9110 §5.3): two `Retry-After: 120` and `Retry-After: 60` are
`"120, 60"`, which a reader of one value refuses instead of taking the last.
Those of `COOKIE` are joined with `"; "`, what a cookie parser splits on.
`SET-COOKIE` is never joined (its values hold commas, RFC 6265): the last one
is kept.

```C
GHTTP_PARSER *parser = ghttp_parser_create(gobj, HTTP_REQUEST, 0, 0, EV_ON_MESSAGE, TRUE);
parser->max_header_size = 16*1024;     // a head of 16 KB at most
parser->max_body_size = 1024*1024;     // a body of 1 MB at most

if(ghttp_parser_received(parser, bf, len) < 0) {
    if(parser->headers_refused) {
        /* answer 431, then drop */
    } else if(parser->body_refused) {
        /* answer 413, then drop */
    } else {
        /* a protocol violation: drop */
    }
}
```

The kw of `on_message_event`:

| Key | Type | Description |
|---|---|---|
| `http_parser_type` | `int` | `HTTP_REQUEST` or `HTTP_RESPONSE`. |
| `url` | `str` | The target of a request. |
| `response_status_code` | `int` | The status of a response. |
| `request_method` | `int` | The method of a request (`llhttp_method_t`). |
| `headers` | `dict` | The headers, the names in upper case (`CONTENT-TYPE`). |
| `body_size` | `int` | The bytes of the body. |
| `body` | json | The body, for `application/json`; `""` when there is no body. |
| `gbuffer` | `int` | The body in a `gbuffer_t *`, for any other type. |
| `body_too_large`, `headers_too_large` | `int` | A refused response: the limit it passed (no `body` nor `gbuffer`). |
| `protocol_error` | `str` | A response that broke the protocol (no `body` nor `gbuffer`). |

(ghttp_parser_create)=
## [`ghttp_parser_create()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ghttp_parser.c#L63)

Creates and initializes a new `GHTTP_PARSER` instance for parsing HTTP messages. The parser processes incoming HTTP data and triggers events when headers, body, or complete messages are received.

```C
GHTTP_PARSER *ghttp_parser_create(
    hgobj gobj,
    llhttp_type_t type,
    gobj_event_t on_header_event,
    gobj_event_t on_body_event,
    gobj_event_t on_message_event,
    BOOL send_event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The parent GObj that will handle the parsed HTTP events. |
| `type` | `enum http_parser_type` | The type of HTTP parser (`HTTP_REQUEST`, `HTTP_RESPONSE`, or `HTTP_BOTH`). |
| `on_header_event` | `gobj_event_t` | The event triggered when HTTP headers are fully received. |
| `on_body_event` | `gobj_event_t` | The event triggered when a portion of the HTTP body is received. |
| `on_message_event` | `gobj_event_t` | The event triggered when the entire HTTP message is received. |
| `send_event` | `BOOL` | If `TRUE`, events are sent using [`gobj_send_event()`](<#gobj_send_event>). Otherwise, they are published using [`gobj_publish_event()`](<#gobj_publish_event>). |

**Returns**

A pointer to the newly created `GHTTP_PARSER` instance, or `NULL` if memory allocation fails.

**Notes**

The returned `GHTTP_PARSER` instance must be destroyed using [`ghttp_parser_destroy()`](<#ghttp_parser_destroy>) when no longer needed.

---

**API break (7.3.0):** `ghttp_parser_reset()` was **removed** from the public API. Calling it from inside an llhttp callback (for example, `on_message_complete`) corrupted llhttp's internal state machine and silently swallowed pipelined HTTP/1.1 messages. Callers that need a fresh parser for a new connection must destroy the current parser and create a new one via [`ghttp_parser_create()`](#ghttp_parser_create) — see `c_prot_http_sr::ac_connected`, `c_prot_http_cl::ac_connected` and `c_websocket::ac_connected` for the canonical pattern.

---

(ghttp_parser_destroy)=
## [`ghttp_parser_destroy()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ghttp_parser.c#L107)

Releases all resources associated with a `GHTTP_PARSER` instance, including allocated memory and internal buffers.

```C
void ghttp_parser_destroy(GHTTP_PARSER *parser);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `parser` | `GHTTP_PARSER *` | Pointer to the `GHTTP_PARSER` instance to be destroyed. |

**Returns**

This function does not return a value.

**Notes**

After calling `ghttp_parser_destroy()`, the `parser` pointer must not be used as it becomes invalid.

---

(ghttp_parser_received)=
## [`ghttp_parser_received()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ghttp_parser.c#L136)

Parses an HTTP message from the given buffer using the [`ghttp_parser_create()`](#ghttp_parser_create) instance.

```C
int ghttp_parser_received(
    GHTTP_PARSER *parser,
    char *bf,
    size_t len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `parser` | `GHTTP_PARSER *` | Pointer to the `GHTTP_PARSER` instance handling the HTTP parsing. |
| `bf` | `char *` | Pointer to the buffer containing the HTTP message data. |
| `len` | `size_t` | Number of bytes available in `bf` for parsing. |

**Returns**

The bytes consumed: all of `len`, or with an upgrade request (`HPE_PAUSED_UPGRADE`) the bytes up to its blank line (the rest belongs to the new protocol). A stopped parser ([`ghttp_parser_stop()`](#ghttp_parser_stop)) answers `len` and parses none. `-1` on a protocol violation (one warning, *"Protocol violation: non-HTTP data received"*, with a dump of at most 256 bytes) or a message over a limit (`headers_refused`, `body_refused`, see *Limits*): the caller drops the connection.

**Notes**

It runs `llhttp_execute()` over the bytes, and the events of the parser are published from inside it.

```C
int n = ghttp_parser_received(parser, gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf));
if(n < 0) {
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);  // already logged
}
```

---

(ghttp_parser_finish)=
## [`ghttp_parser_finish()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ghttp_parser.c#L198)

Signals end-of-stream to the underlying llhttp parser (the TCP peer closed the socket). Required to complete HTTP messages whose terminator is the connection close itself — HTTP/1.0 responses without `Content-Length`, and HTTP/1.1 responses with neither `Content-Length` nor `Transfer-Encoding: chunked`. Without this call, `on_message_complete` will never fire for such messages and the subscriber will miss `EV_ON_MESSAGE`.

```C
int ghttp_parser_finish(GHTTP_PARSER *parser);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `parser` | `GHTTP_PARSER *` | Pointer to the `GHTTP_PARSER` instance. |

**Returns**

`0` on success, `-1` on protocol error (for example a partial message was in flight when the peer closed).

**Notes**

Call it from the gobj's disconnect handler (`ac_disconnected` / `mt_stop`). Called inside an event published by the parser (the subscriber dropped the connection, and the disconnection came at once), it is [`ghttp_parser_stop()`](#ghttp_parser_stop) and answers `0`.

```C
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    (void)ghttp_parser_finish(priv->parsing_response);  // an EOF-terminated response ends here
    ...
}
```

---

(ghttp_parser_stop)=
## [`ghttp_parser_stop()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/ghttp_parser.c#L139)

The connection of the parser ends (it is dropped, or it closed): nothing more is delivered, not even the rest of the bytes being parsed now. Requests pipelined in one read after the one whose subscriber dropped the connection are not published.

```C
void ghttp_parser_stop(GHTTP_PARSER *parser);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `parser` | `GHTTP_PARSER *` | The parser; `NULL` is ignored. |

**Returns**

This function does not return a value.

**Notes**

Safe inside an llhttp callback. After it, [`ghttp_parser_received()`](#ghttp_parser_received) takes every byte and parses none, and [`ghttp_parser_finish()`](#ghttp_parser_finish) does nothing. A new connection needs a new parser.

```C
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    ghttp_parser_stop(priv->parsing_request);   // what is left of the read is of nobody
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
    KW_DECREF(kw)
    return 0;
}
```

---
