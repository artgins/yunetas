# HTTP Parser Functions

Source code in:

- [helpers.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [helpers.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(GHTTP_PARSER)=
## GHTTP_PARSER

```C
    typedef struct _GHTTP_PARSER {
        http_parser http_parser;
        hgobj gobj;
        gobj_event_t on_header_event;
        gobj_event_t on_body_event;
        gobj_event_t on_message_event;
        BOOL send_event;

        enum http_parser_type type;
        char message_completed;
        char headers_completed;

        char *url;
        json_t *jn_headers;
        //char *body;
        size_t body_size;
        gbuffer_t *gbuf_body;

        char *cur_key;  // key can arrive in several callbacks
        char *last_key; // save last key for the case value arriving in several callbacks
    } GHTTP_PARSER;
```

(date_mode)=
## date_mode

```C
    struct date_mode {
        enum date_mode_type {
            DATE_NORMAL = 0,
            DATE_RELATIVE,
            DATE_SHORT,
            DATE_ISO8601,
            DATE_ISO8601_STRICT,
            DATE_RFC2822,
            DATE_STRFTIME,
            DATE_RAW,
            DATE_UNIX
        } type;
        char strftime_fmt[256];
        int local;
    };
```

(ghttp_parser_create)=
## `ghttp_parser_create()`

Creates and initializes a new `GHTTP_PARSER` instance for parsing HTTP messages. The parser processes incoming HTTP data and triggers events when headers, body, or complete messages are received.

```C
GHTTP_PARSER *ghttp_parser_create(
    hgobj gobj,
    enum http_parser_type type,
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
    char *buf,
    size_t received
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `parser` | `GHTTP_PARSER *` | Pointer to the [`GHTTP_PARSER`](#GHTTP_PARSER) instance handling the HTTP parsing. |
| `buf` | `char *` | Pointer to the buffer containing the HTTP message data. |
| `received` | `size_t` | Number of bytes available in `buf` for parsing. |

**Returns**

Returns the number of bytes successfully parsed. Returns `-1` if an error occurs during parsing.

**Notes**

['This function uses `http_parser_execute()` to process the HTTP message.', 'If the parser encounters an error, it logs the issue and returns `-1`.', 'The function checks for upgrade requests and handles them accordingly.']

---

(ghttp_parser_reset)=
## `ghttp_parser_reset()`

Resets the internal state of a [`GHTTP_PARSER`](#GHTTP_PARSER) structure, clearing accumulated data such as headers, body, and URL.

```C
void ghttp_parser_reset(GHTTP_PARSER *parser);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `parser` | `GHTTP_PARSER *` | Pointer to the [`GHTTP_PARSER`](#GHTTP_PARSER) structure to reset. |

**Returns**

This function does not return a value.

**Notes**

This function should be called before reusing a [`GHTTP_PARSER`](#GHTTP_PARSER) instance to ensure a clean state.

---
