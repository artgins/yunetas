/***********************************************************************
 *          GHTTP_PARSER.C
 *          Mixin http_parser - gobj-ecosistema
 *          Migrated from http_parser (Joyent) to llhttp (nodejs/llhttp)
 *
 *          Copyright (c) 2013 Niyamaka.
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <ctype.h>

#include "ghttp_parser.h"

/****************************************************************
 *         Constants
 ****************************************************************/
#define RX_GBUFFER_SIZE (4*1024)
#define MAX_LOG_DUMP_SIZE (256)     // Cap the data dump added to logs, for very large packets

/****************************************************************
 *         Structures
 ****************************************************************/

/****************************************************************
 *         Prototypes
 ****************************************************************/
PRIVATE int on_message_begin(llhttp_t* llhttp);
PRIVATE int on_headers_complete(llhttp_t* llhttp);
PRIVATE int on_message_complete(llhttp_t* llhttp);
PRIVATE int on_url(llhttp_t* llhttp, const char* at, size_t length);
PRIVATE int on_url_complete(llhttp_t* llhttp);
PRIVATE int on_status(llhttp_t* llhttp, const char* at, size_t length);
PRIVATE int on_header_field(llhttp_t* llhttp, const char* at, size_t length);
PRIVATE int on_header_value(llhttp_t* llhttp, const char* at, size_t length);
PRIVATE int on_header_value_complete(llhttp_t* llhttp);
PRIVATE int on_body(llhttp_t* llhttp, const char* at, size_t length);
PRIVATE json_t *build_message_kw(GHTTP_PARSER *parser);
PRIVATE void reset_message(GHTTP_PARSER *parser);
PRIVATE void publish_message(GHTTP_PARSER *parser, json_t *kw_http);
PRIVATE int refuse_body(GHTTP_PARSER *parser, size_t length, size_t limit);
PRIVATE size_t body_size_limit(GHTTP_PARSER *parser);
PRIVATE size_t header_size_limit(GHTTP_PARSER *parser);
PRIVATE int add_to_head(GHTTP_PARSER *parser, gbuffer_t **pgbuf, const char *at, size_t length);
PRIVATE void publish_refused_response(GHTTP_PARSER *parser, const char *key, json_t *jn_value);

/****************************************************************
 *         Data
 *
 *  The settings table is a read-only vtable shared by every
 *  parser instance.  It is initialised once, lazily, on the
 *  first ghttp_parser_create() call via llhttp_settings_init().
 ****************************************************************/
PRIVATE BOOL settings_ready = FALSE;
PRIVATE llhttp_settings_t settings;

PRIVATE void ensure_settings_initialized(void)
{
    if(!settings_ready) {
        llhttp_settings_init(&settings);
        settings.on_message_begin    = on_message_begin;
        settings.on_url              = on_url;
        settings.on_url_complete     = on_url_complete;
        settings.on_status           = on_status;
        settings.on_header_field     = on_header_field;
        settings.on_header_value     = on_header_value;
        settings.on_header_value_complete = on_header_value_complete;
        settings.on_headers_complete = on_headers_complete;
        settings.on_body             = on_body;
        settings.on_message_complete = on_message_complete;
        settings_ready = TRUE;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC GHTTP_PARSER *ghttp_parser_create(
    hgobj gobj,
    llhttp_type_t type,
    gobj_event_t on_header_event,       // Event to publish or send when the header is completed
    gobj_event_t on_body_event,         // Event to publish or send when the body is receiving
    gobj_event_t on_message_event,      // Event to publish or send when the message is completed
    BOOL send_event  // TRUE: use gobj_send_event(), FALSE: use gobj_publish_event()
)
{
    GHTTP_PARSER *parser;

    ensure_settings_initialized();

    /*--------------------------------*
     *      Alloc memory
     *--------------------------------*/
    parser = GBMEM_MALLOC(sizeof(GHTTP_PARSER));
    if(!parser) {
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY,
            "msg",                  "%s", "no memory for sizeof(GHTTP_PARSER)",
            "sizeof(GHTTP_PARSER)", "%d", sizeof(GHTTP_PARSER),
            NULL);
        return 0;
    }
    parser->gobj = gobj;
    parser->type = type;
    parser->on_header_event = empty_string(on_header_event)?NULL:on_header_event;
    parser->on_body_event = empty_string(on_body_event)?NULL:on_body_event;
    parser->on_message_event = empty_string(on_message_event)?NULL:on_message_event;
    parser->send_event = send_event;

    llhttp_init(&parser->llhttp, type, &settings);
    parser->llhttp.data = parser;
    llhttp_set_lenient_headers(&parser->llhttp, 1);
    llhttp_set_lenient_data_after_close(&parser->llhttp, 1);

    return parser;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void ghttp_parser_destroy(GHTTP_PARSER *parser)
{
    if(!parser) {
        return;
    }
    GBMEM_FREE(parser->url);
    GBUFFER_DECREF(parser->gbuf_body)
    JSON_DECREF(parser->jn_headers);
    GBUFFER_DECREF(parser->gbuf_url)
    GBUFFER_DECREF(parser->gbuf_field)
    GBUFFER_DECREF(parser->gbuf_value)
    GBMEM_FREE(parser);
}

/***************************************************************************
 *  The connection ends: nothing more is delivered
 ***************************************************************************/
PUBLIC void ghttp_parser_stop(GHTTP_PARSER *parser)
{
    if(!parser) {
        return;
    }
    parser->stopped = TRUE;
}

/***************************************************************************
 *  Feed data received on the underlying TCP connection to the parser.
 *
 *  Return value:
 *    > 0   Bytes consumed from `buf`.
 *              - HPE_OK            : equals `received` (llhttp consumes the
 *                                     whole buffer in the success case).
 *              - HPE_PAUSED_UPGRADE: bytes consumed up to (and including)
 *                                     the blank line of the upgrade request.
 *                                     The tail (if any) belongs to the new
 *                                     protocol and must be routed by the
 *                                     caller to the next handler (e.g. the
 *                                     first WebSocket frame piggybacked on
 *                                     the handshake TCP segment).
 *              - stopped           : equals `received`, none parsed: the
 *                                     connection ends (ghttp_parser_stop()).
 *     -1   Protocol error, or a head or a body over its limit
 *          (`headers_refused`, `body_refused`).  The caller should close
 *          the connection.
 ***************************************************************************/
PUBLIC int ghttp_parser_received(
    GHTTP_PARSER *parser,
    char *buf,
    size_t received
) {
    hgobj gobj = parser->gobj;

    if(parser->stopped) {
        return (int)received;
    }

    parser->executing = TRUE;
    llhttp_errno_t err = llhttp_execute(&parser->llhttp, buf, received);
    parser->executing = FALSE;

    if(parser->stopped) {
        return (int)received;   // what is left of the bytes is not of anybody any more
    }
    if (err == HPE_PAUSED_UPGRADE) {
        /*
         *  Handle new protocol (upgrade).
         *  llhttp stopped exactly after the blank line of the HTTP
         *  upgrade request; report how many bytes of `buf` were actually
         *  consumed so the caller can re-route the remainder to the
         *  next protocol handler.
         */
        const char *stop = llhttp_get_error_pos(&parser->llhttp);
        size_t consumed = (stop && stop >= buf) ? (size_t)(stop - buf) : received;
        if(consumed > received) {
            consumed = received;
        }
        llhttp_resume_after_upgrade(&parser->llhttp);
        return (int)consumed;
    } else if (err != HPE_OK) {
        if(parser->body_refused || parser->headers_refused) {
            return -1;  // Warning already logged (refuse_body, add_to_head)
        }
        /* Handle error. Usually just close the connection. */
        const char *peername = gobj_read_str_attr(gobj, "peername");
        const char *sockname = gobj_read_str_attr(gobj, "sockname");
        gobj_log_warning(gobj,0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "Protocol violation: non-HTTP data received",
            "peername",     "%s", peername?peername:"",
            "sockname",     "%s", sockname?sockname:"",
            "error",        "%s", llhttp_errno_name(err),
            "desc",         "%s", llhttp_get_error_reason(&parser->llhttp),
            NULL
        );
        gobj_trace_dump(gobj, buf, MIN(received, MAX_LOG_DUMP_SIZE),
            "Protocol violation: non-HTTP data received"
        );
        if(parser->type == HTTP_RESPONSE && parser->message_begun) {
            /*
             *  The request waiting for this response is answered: it never
             *  goes again by itself on the next connection
             */
            const char *reason = llhttp_get_error_reason(&parser->llhttp);
            publish_refused_response(parser, "protocol_error",
                json_string(reason? reason : llhttp_errno_name(err))
            );
        }
        return -1;
    }
    return (int)received;
}

/***************************************************************************
 *  Signal end-of-stream to llhttp (the TCP peer closed the socket).
 *
 *  Needed to complete HTTP messages whose terminator is the connection
 *  close — HTTP/1.0 responses without Content-Length, or HTTP/1.1
 *  responses with neither Content-Length nor Transfer-Encoding: chunked.
 *  Without this call llhttp would never fire on_message_complete for
 *  those messages and the caller would hang waiting for EV_ON_MESSAGE.
 *
 *  Call it from the gobj's disconnect handler (ac_disconnected / mt_stop),
 *  not from inside an llhttp callback.
 *
 *  Returns 0 on success, -1 on protocol error (incomplete message in
 *  flight — the caller can ignore this when the peer was expected to
 *  drop the socket, e.g. after a successful response was already
 *  delivered).
 ***************************************************************************/
PUBLIC int ghttp_parser_finish(GHTTP_PARSER *parser)
{
    if(!parser) {
        return 0;
    }
    if(parser->executing) {
        ghttp_parser_stop(parser);  // the close came inside an event of the parser
        return 0;
    }
    if(parser->stopped) {
        return 0;
    }

    llhttp_errno_t err = llhttp_finish(&parser->llhttp);
    if (err != HPE_OK) {
        // Silence please, check the return if necessary
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Callbacks must return 0 on success.
 *  Returning a non-zero value indicates error to the parser,
 *  making it exit immediately.
 ***************************************************************************/
PRIVATE int on_message_begin(llhttp_t* llhttp)
{
    GHTTP_PARSER *parser = llhttp->data;

    if(parser->stopped) {
        return -1;
    }
    parser->message_begun = 1;
    parser->header_size = 0;
    parser->body_size = 0;
    return 0;
}

/***************************************************************************
 *  Callbacks must return 0 on success.
 *  Returning a non-zero value indicates error to the parser,
 *  making it exit immediately.
 ***************************************************************************/
PRIVATE int on_headers_complete(llhttp_t* llhttp)
{
    GHTTP_PARSER *parser = llhttp->data;
    hgobj gobj = parser->gobj;

    if(parser->stopped) {
        return -1;
    }
    parser->headers_completed = 1;
    if(!parser->jn_headers) {
        parser->jn_headers = json_object();
    }
    if(parser->on_header_event) {
        json_t *kw_http = json_pack("{s:i, s:s, s:i, s:i, s:O}",
            "http_parser_type",     (int)parser->type,
            "url",                  parser->url?parser->url:"",
            "response_status_code", (int)llhttp_get_status_code(&parser->llhttp),
            "request_method",       (int)llhttp_get_method(&parser->llhttp),
            "headers",              parser->jn_headers
        );
        if(parser->send_event) {
            gobj_send_event(gobj, parser->on_header_event, kw_http, gobj);
        } else {
            gobj_publish_event(gobj, parser->on_header_event, kw_http);
        }
    }
    return parser->stopped? -1 : 0;
}

/***************************************************************************
 *  Callbacks must return 0 on success.
 *  Returning a non-zero value indicates error to the parser,
 *  making it exit immediately.
 ***************************************************************************/
PRIVATE int on_body(llhttp_t* llhttp, const char* at, size_t length)
{
    GHTTP_PARSER *parser = llhttp->data;
    hgobj gobj = parser->gobj;

    if(parser->stopped) {
        return -1;
    }

    size_t limit = body_size_limit(parser);
    if(limit > 0 && parser->body_size + length > limit) {
        return refuse_body(parser, length, limit);
    }

    if(parser->on_body_event) {
        json_t *kw_http = json_pack("{s:i, s:I, s:I}",
            "response_status_code", (int)llhttp_get_status_code(&parser->llhttp),
            "__pbf__", (json_int_t)(uintptr_t)at,
            "__pbf_size__", (json_int_t)length
        );
        if(parser->send_event) {
            gobj_send_event(gobj, parser->on_body_event, kw_http, gobj);
        } else {
            gobj_publish_event(gobj, parser->on_body_event, kw_http);
        }
        if(parser->stopped) {
            return -1;
        }
    }

    if(parser->on_message_event) {
        /*
         *  If on_message_event is defined, then accumulate data in body variable
         */
        if(!parser->gbuf_body) {
            /*
             *  Of the size of the first part of the body, up to
             *  RX_GBUFFER_SIZE: it grows with the rest
             */
            size_t initial_size = (length < RX_GBUFFER_SIZE)? length : RX_GBUFFER_SIZE;
            parser->gbuf_body = gbuffer_create(initial_size, limit + 1);
            if(!parser->gbuf_body) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MEMORY,
                    "msg",          "%s", "no memory for body",
                    "body_size",    "%d", parser->body_size,
                    "more length",  "%d", length,
                    NULL
                );
                return -1;
            }
        }
        if(gbuffer_append(parser->gbuf_body, (void *)at, length) != length) {
            // Error already logged (gbuffer)
            return refuse_body(parser, length, limit);
        }
    }
    parser->body_size += length;

    return 0;
}

/***************************************************************************
 *  The longest body: `max_body_size`. With on_message_event, that holds
 *  the whole body, never more than half the largest memory block, so the
 *  buffer that holds it, grown by doubling, is always one block that can
 *  be allocated; with the body only streamed (on_body_event), 0 is no
 *  limit, because the parser holds none of it.
 ***************************************************************************/
PRIVATE size_t body_size_limit(GHTTP_PARSER *parser)
{
    if(!parser->on_message_event) {
        return parser->max_body_size;
    }
    size_t limit = gbmem_get_maximum_block() / 2;
    if(parser->max_body_size > 0 && parser->max_body_size < limit) {
        limit = parser->max_body_size;
    }
    return limit;
}

/***************************************************************************
 *  The longest head: `max_header_size`, 0 is GHTTP_MAX_HEADER_SIZE, and
 *  never more than half the largest memory block
 ***************************************************************************/
PRIVATE size_t header_size_limit(GHTTP_PARSER *parser)
{
    size_t limit = parser->max_header_size? parser->max_header_size : GHTTP_MAX_HEADER_SIZE;
    size_t max_limit = gbmem_get_maximum_block() / 2;
    if(limit > max_limit) {
        limit = max_limit;
    }
    return limit;
}

/***************************************************************************
 *  A RESPONSE refused, published without its body, so the request waiting
 *  for it is answered. Owns jn_value.
 ***************************************************************************/
PRIVATE void publish_refused_response(GHTTP_PARSER *parser, const char *key, json_t *jn_value)
{
    if(!parser->on_message_event) {
        JSON_DECREF(jn_value)
        return;
    }
    if(!parser->jn_headers) {
        parser->jn_headers = json_object();
    }
    json_t *kw_http = build_message_kw(parser);
    json_object_set_new(kw_http, key, jn_value);
    reset_message(parser);
    publish_message(parser, kw_http);
}

/***************************************************************************
 *  A body that does not fit: ONE warning, the message of a RESPONSE
 *  published without its body (`body_too_large`), and the parser stopped
 *  (llhttp keeps answering the error, and ghttp_parser_received() keeps
 *  quiet about it). Returns the error for llhttp.
 ***************************************************************************/
PRIVATE int refuse_body(GHTTP_PARSER *parser, size_t length, size_t limit)
{
    hgobj gobj = parser->gobj;

    const char *peername = gobj_read_str_attr(gobj, "peername");
    const char *sockname = gobj_read_str_attr(gobj, "sockname");
    gobj_log_warning(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_PROTOCOL,
        "msg",              "%s", "HTTP body larger than its limit, message refused",
        "http_parser_type", "%s", parser->type == HTTP_RESPONSE? "response" : "request",
        "body_received",    "%lu", (unsigned long)(parser->body_size + length),
        "max_body_size",    "%lu", (unsigned long)limit,
        "peername",         "%s", peername? peername : "",
        "sockname",         "%s", sockname? sockname : "",
        NULL
    );
    parser->body_refused = TRUE;
    parser->body_size += length;

    if(parser->type == HTTP_RESPONSE) {
        publish_refused_response(parser, "body_too_large", json_integer((json_int_t)limit));
    } else {
        reset_message(parser);
    }
    return -1;
}

/***************************************************************************
 *  Append a piece of the head (the target, a name, a value) to its buffer.
 *  A head that passes its limit: ONE warning, the message of a RESPONSE
 *  published without its body (`headers_too_large`), the parser stopped.
 *  Linear: each byte is copied once, into a buffer that grows by doubling.
 ***************************************************************************/
PRIVATE int add_to_head(GHTTP_PARSER *parser, gbuffer_t **pgbuf, const char *at, size_t length)
{
    hgobj gobj = parser->gobj;

    if(parser->stopped) {
        return -1;
    }
    if(length == 0) {
        return 0;
    }

    size_t limit = header_size_limit(parser);
    if(parser->header_size + length > limit) {
        const char *peername = gobj_read_str_attr(gobj, "peername");
        const char *sockname = gobj_read_str_attr(gobj, "sockname");
        const char *field = parser->gbuf_field? gbuffer_cur_rd_pointer(parser->gbuf_field) : "";
        gobj_log_warning(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_PROTOCOL,
            "msg",              "%s", "HTTP headers larger than their limit, message refused",
            "http_parser_type", "%s", parser->type == HTTP_RESPONSE? "response" : "request",
            "header_received",  "%lu", (unsigned long)(parser->header_size + length),
            "max_header_size",  "%lu", (unsigned long)limit,
            "header",           "%.64s", (pgbuf == &parser->gbuf_url)? "" : field,
            "peername",         "%s", peername? peername : "",
            "sockname",         "%s", sockname? sockname : "",
            NULL
        );
        parser->headers_refused = TRUE;
        if(parser->type == HTTP_RESPONSE) {
            publish_refused_response(parser, "headers_too_large", json_integer((json_int_t)limit));
        } else {
            reset_message(parser);
        }
        return -1;
    }
    parser->header_size += length;

    if(!pgbuf) {
        return 0;   // counted only
    }
    if(!*pgbuf) {
        *pgbuf = gbuffer_create(MIN(length, (size_t)256), limit + 1);
        if(!*pgbuf) {
            // Error already logged
            return -1;
        }
    }
    if(gbuffer_append(*pgbuf, (void *)at, length) != length) {
        // Error already logged
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  The kw of on_message_event, without its body
 ***************************************************************************/
PRIVATE json_t *build_message_kw(GHTTP_PARSER *parser)
{
    return json_pack("{s:i, s:s, s:i, s:i, s:O, s:I}",
        "http_parser_type",     (int)parser->type,
        "url",                  parser->url?parser->url:"",
        "response_status_code", (int)llhttp_get_status_code(&parser->llhttp),
        "request_method",       (int)llhttp_get_method(&parser->llhttp),
        "headers",              parser->jn_headers,
        "body_size",            (json_int_t)parser->body_size
    );
}

/***************************************************************************
 *  Clear the per-message application state, so the next pipelined
 *  message starts clean.
 *
 *  CAREFUL: do NOT re-init llhttp here.  We are still inside
 *  llhttp_execute, about to return from its callback so llhttp can keep
 *  parsing the rest of the buffer.  Re-initing llhttp from inside its own
 *  callback corrupts its internal state (cs/sp/return-stack) and llhttp
 *  silently swallows every subsequent message in the buffer
 *  (test8_queue_full pipelines four POSTs and only the first one ever
 *  fired EV_ON_MESSAGE).  llhttp handles the message-to-message
 *  transition internally for keep-alive, so we just clear our own
 *  per-request fields.
 ***************************************************************************/
PRIVATE void reset_message(GHTTP_PARSER *parser)
{
    parser->headers_completed = 0;
    parser->message_completed = 0;
    parser->message_begun = 0;
    parser->body_size = 0;
    parser->header_size = 0;
    GBMEM_FREE(parser->url);
    GBUFFER_DECREF(parser->gbuf_body)
    JSON_DECREF(parser->jn_headers);
    GBUFFER_DECREF(parser->gbuf_url)
    GBUFFER_DECREF(parser->gbuf_field)
    GBUFFER_DECREF(parser->gbuf_value)
}

/***************************************************************************
 *  Publish or send on_message_event
 ***************************************************************************/
PRIVATE void publish_message(GHTTP_PARSER *parser, json_t *kw_http)
{
    if(parser->send_event) {
        gobj_send_event(parser->gobj, parser->on_message_event, kw_http, parser->gobj);
    } else {
        gobj_publish_event(parser->gobj, parser->on_message_event, kw_http);
    }
}

/***************************************************************************
 *  Callbacks must return 0 on success.
 *  Returning a non-zero value indicates error to the parser,
 *  making it exit immediately.
 ***************************************************************************/
PRIVATE int on_message_complete(llhttp_t* llhttp)
{
    GHTTP_PARSER *parser = llhttp->data;
    hgobj gobj = parser->gobj;

    if(parser->stopped) {
        return -1;
    }
    parser->message_completed = 1;
    parser->message_begun = 0;
    if(!parser->jn_headers) {
        parser->jn_headers = json_object();
    }

    if(parser->on_body_event) {
        /*
         *  HACK: The last event without "__buffer__" key will indicate that all message is completed
         */
        json_t *kw_http = json_pack("{s:i}",
            "response_status_code", (int)llhttp_get_status_code(&parser->llhttp)
        );
        if(parser->send_event) {
            gobj_send_event(gobj, parser->on_body_event, kw_http, gobj);
        } else {
            gobj_publish_event(gobj, parser->on_body_event, kw_http);
        }
        if(parser->stopped) {
            return -1;
        }
    }

    if(parser->on_message_event) {
        /*
         *  The cur_request (with the body) is ready to use.
         */
        json_t *kw_http = build_message_kw(parser);
        const char *content_type = kw_get_str(gobj, parser->jn_headers, "CONTENT-TYPE", "", 0);
        if(!parser->gbuf_body) {
            json_object_set_new(kw_http, "body", json_string(""));
        } else {
            char *body = gbuffer_cur_rd_pointer(parser->gbuf_body);
            size_t body_len = gbuffer_leftbytes(parser->gbuf_body);
            if(body_len != parser->body_size) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MEMORY,
                    "msg",          "%s", "NO MATCH in body size",
                    "body_size",    "%d", (int)parser->body_size,
                    "body_len",     "%d", (int)body_len,
                    NULL
                );
            }
            if(strcasestr(content_type, "application/json")) {
                json_t *jn_body = anystring2json_from_peer(gobj, body, body_len, gobj);
                json_object_set_new(kw_http, "body", jn_body);
            } else {
                json_object_set_new(kw_http, "gbuffer", json_integer((json_int_t)(uintptr_t)parser->gbuf_body));
                parser->gbuf_body = 0;
            }
        }

        reset_message(parser);
        publish_message(parser, kw_http);
    }
    return parser->stopped? -1 : 0;
}

/***************************************************************************
 *  Callbacks must return 0 on success.
 *  Returning a non-zero value indicates error to the parser,
 *  making it exit immediately.
 ***************************************************************************/
PRIVATE int on_url(llhttp_t* llhttp, const char* at, size_t length)
{
    GHTTP_PARSER *parser = llhttp->data;
    return add_to_head(parser, &parser->gbuf_url, at, length);
}

/***************************************************************************
 *  The target is whole: it is the url of the message
 ***************************************************************************/
PRIVATE int on_url_complete(llhttp_t* llhttp)
{
    GHTTP_PARSER *parser = llhttp->data;

    if(parser->stopped) {
        return -1;
    }
    GBMEM_FREE(parser->url);
    if(parser->gbuf_url) {
        parser->url = gbmem_strndup(
            gbuffer_cur_rd_pointer(parser->gbuf_url),
            gbuffer_leftbytes(parser->gbuf_url)
        );
        GBUFFER_DECREF(parser->gbuf_url)
    } else {
        parser->url = gbmem_strdup("");
    }
    if(!parser->url) {
        // Error already logged
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  The status text of a response is not kept, but it is part of the head
 ***************************************************************************/
PRIVATE int on_status(llhttp_t* llhttp, const char* at, size_t length)
{
    GHTTP_PARSER *parser = llhttp->data;
    return add_to_head(parser, NULL, at, length);
}

/***************************************************************************
 *  Callbacks must return 0 on success.
 *  Returning a non-zero value indicates error to the parser,
 *  making it exit immediately.
 ***************************************************************************/
PRIVATE int on_header_field(llhttp_t* llhttp, const char* at, size_t length)
{
    GHTTP_PARSER *parser = llhttp->data;
    return add_to_head(parser, &parser->gbuf_field, at, length);
}

/***************************************************************************
 *  Callbacks must return 0 on success.
 *  Returning a non-zero value indicates error to the parser,
 *  making it exit immediately.
 ***************************************************************************/
PRIVATE int on_header_value(llhttp_t* llhttp, const char* at, size_t length)
{
    GHTTP_PARSER *parser = llhttp->data;
    return add_to_head(parser, &parser->gbuf_value, at, length);
}

/***************************************************************************
 *  A header is whole: its name, in upper case, and its value go into
 *  jn_headers, once (a name that comes again keeps its last value)
 ***************************************************************************/
PRIVATE int on_header_value_complete(llhttp_t* llhttp)
{
    GHTTP_PARSER *parser = llhttp->data;
    hgobj gobj = parser->gobj;

    if(parser->stopped) {
        return -1;
    }
    if(!parser->jn_headers) {
        parser->jn_headers = json_object();
        if(!parser->jn_headers) {
            // Error already logged
            return -1;
        }
    }

    char *key = parser->gbuf_field? gbuffer_cur_rd_pointer(parser->gbuf_field) : "";
    size_t key_len = parser->gbuf_field? gbuffer_leftbytes(parser->gbuf_field) : 0;
    const char *value = parser->gbuf_value? gbuffer_cur_rd_pointer(parser->gbuf_value) : "";
    size_t value_len = parser->gbuf_value? gbuffer_leftbytes(parser->gbuf_value) : 0;

    if(key_len > 0) {
        strntoupper(key, key_len);
        json_t *jn_value = json_stringn(value, value_len);
        if(!jn_value || json_object_set_new(parser->jn_headers, key, jn_value) < 0) {
            // Not utf-8: the header is left out, and said
            const char *peername = gobj_read_str_attr(gobj, "peername");
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL,
                "msg",          "%s", "cannot store header value (invalid utf-8?)",
                "peername",     "%s", peername? peername : "",
                "key",          "%.64s", key,
                NULL
            );
            gobj_trace_dump(gobj, value, MIN(value_len, MAX_LOG_DUMP_SIZE),
                "cannot store header value (invalid utf-8?)"
            );
        }
    }

    if(parser->gbuf_field) {
        gbuffer_reset_wr(parser->gbuf_field);
    }
    if(parser->gbuf_value) {
        gbuffer_reset_wr(parser->gbuf_value);
    }
    return 0;
}
