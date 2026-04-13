# HTTP Parser

Incremental HTTP/1.x parser used by the HTTP gclasses. Feeds bytes from a `gbuffer_t` and invokes callbacks on each complete header, body chunk, and message.

Source code:

- [`helpers.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [`helpers.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(ghttp_parser_create)=
## `ghttp_parser_create()`

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
| `send_event` | `BOOL` | If `TRUE`, events are sent using [`gobj_send_event()`](<#gobj_send_event>); otherwise, they are published using [`gobj_publish_event()`](<#gobj_publish_event>). |

**Returns**

A pointer to the newly created `GHTTP_PARSER` instance, or `NULL` if memory allocation fails.

**Notes**

The returned `GHTTP_PARSER` instance must be destroyed using [`ghttp_parser_destroy()`](<#ghttp_parser_destroy>) when no longer needed.

---

(ghttp_parser_destroy)=
## `ghttp_parser_destroy()`

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

After calling `ghttp_parser_destroy()`, the `parser` pointer should not be used as it becomes invalid.

---

(ghttp_parser_received)=
## `ghttp_parser_received()`

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

Returns the number of bytes successfully parsed. Returns `-1` if an error occurs during parsing.

**Notes**

['This function uses `http_parser_execute()` to process the HTTP message.', 'If the parser encounters an error, it logs the issue and returns `-1`.', 'The function checks for upgrade requests and handles them accordingly.']

---

(ghttp_parser_finish)=
## `ghttp_parser_finish()`

Signals end-of-stream to the underlying llhttp parser (the TCP peer closed the socket). Required to complete HTTP messages whose terminator is the connection close itself — HTTP/1.0 responses without `Content-Length`, and HTTP/1.1 responses with neither `Content-Length` nor `Transfer-Encoding: chunked`. Without this call, `on_message_complete` would never fire for such messages and the subscriber would miss `EV_ON_MESSAGE`.

```C
int ghttp_parser_finish(GHTTP_PARSER *parser);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `parser` | `GHTTP_PARSER *` | Pointer to the `GHTTP_PARSER` instance. |

**Returns**

`0` on success, `-1` on protocol error (e.g. a partial message was in flight when the peer closed).

**Notes**

Call it from the gobj's disconnect handler (`ac_disconnected` / `mt_stop`). Never call it from inside an llhttp callback.

---
