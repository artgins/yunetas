# HTTP Parser functions

Source code in:

- [helpers.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [helpers.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(GHTTP_PARSER())=
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

(date_mode())=
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



:::{toctree}
:caption: HTTP Parser functions
:maxdepth: 1

http-parser/ghttp_parser_create
http-parser/ghttp_parser_destroy
http-parser/ghttp_parser_received
http-parser/ghttp_parser_reset

:::
