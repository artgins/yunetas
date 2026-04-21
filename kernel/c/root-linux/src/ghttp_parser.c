/***********************************************************************
 *          GHTTP_PARSER.C
 *          Mixin http_parser - gobj-ecosistema
 *          Migrated from http_parser (Joyent) to llhttp (nodejs/llhttp)
 *
 *          Copyright (c) 2013 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <ctype.h>

#include "ghttp_parser.h"

/****************************************************************
 *         Constants
 ****************************************************************/
#define RX_GBUFFER_SIZE (4*1024)

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
PRIVATE int on_header_field(llhttp_t* llhttp, const char* at, size_t length);
PRIVATE int on_header_value(llhttp_t* llhttp, const char* at, size_t length);
PRIVATE int on_body(llhttp_t* llhttp, const char* at, size_t length);

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
        settings.on_header_field     = on_header_field;
        settings.on_header_value     = on_header_value;
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
    GBMEM_FREE(parser->cur_key);
    GBMEM_FREE(parser->last_key);
    GBMEM_FREE(parser);
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
 *     -1   Protocol error.  The caller should close the connection.
 ***************************************************************************/
PUBLIC int ghttp_parser_received(
    GHTTP_PARSER *parser,
    char *buf,
    size_t received
) {
    hgobj gobj = parser->gobj;

    llhttp_errno_t err = llhttp_execute(&parser->llhttp, buf, received);
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
        gobj_trace_dump(gobj, buf, received, "Protocol violation: non-HTTP data received");
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
    return 0;
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
    }

    if(parser->on_message_event) {
        /*
         *  If on_message_event is defined, then accumulate data in body variable
         */
        if(!parser->gbuf_body) {
            parser->gbuf_body = gbuffer_create(RX_GBUFFER_SIZE, gbmem_get_maximum_block());
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
        gbuffer_append(parser->gbuf_body, (void *)at, length);
        parser->body_size += length;
    }

    return 0;
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

    parser->message_completed = 1;
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
    }

    if(parser->on_message_event) {
        /*
         *  The cur_request (with the body) is ready to use.
         */
        json_t *kw_http = json_pack("{s:i, s:s, s:i, s:i, s:O}",
            "http_parser_type",     (int)parser->type,
            "url",                  parser->url?parser->url:"",
            "response_status_code", (int)llhttp_get_status_code(&parser->llhttp),
            "request_method",       (int)llhttp_get_method(&parser->llhttp),
            "headers",              parser->jn_headers
        );
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
                json_t *jn_body = anystring2json(body, body_len, TRUE);
                json_object_set_new(kw_http, "body", jn_body);
            } else {
                json_object_set_new(kw_http, "gbuffer", json_integer((json_int_t)(uintptr_t)parser->gbuf_body));
                parser->gbuf_body = 0;
            }
        }

        /*
         *  Reset the per-message application state so the next pipelined
         *  request starts clean.
         *
         *  CAREFUL: do NOT re-init llhttp here.  We are still inside
         *  llhttp_execute, about to return 0 from this callback so llhttp
         *  can keep parsing the rest of the buffer.  Re-initing llhttp
         *  from inside its own callback corrupts its internal state
         *  (cs/sp/return-stack) and llhttp silently swallows every
         *  subsequent message in the buffer (test8_queue_full pipelines
         *  four POSTs and only the first one ever fired EV_ON_MESSAGE).
         *  llhttp handles the message-to-message transition internally
         *  for keep-alive, so we just clear our own per-request fields.
         */
        parser->headers_completed = 0;
        parser->message_completed = 0;
        parser->body_size = 0;
        GBMEM_FREE(parser->url);
        GBUFFER_DECREF(parser->gbuf_body)
        JSON_DECREF(parser->jn_headers);
        GBMEM_FREE(parser->cur_key);
        GBMEM_FREE(parser->last_key);

        if(parser->send_event) {
            gobj_send_event(gobj, parser->on_message_event, kw_http, gobj);
        } else {
            gobj_publish_event(gobj, parser->on_message_event, kw_http);
        }
    }
    return 0;
}

/***************************************************************************
 *  Callbacks must return 0 on success.
 *  Returning a non-zero value indicates error to the parser,
 *  making it exit immediately.
 ***************************************************************************/
PRIVATE int on_url(llhttp_t* llhttp, const char* at, size_t length)
{
    GHTTP_PARSER *parser = llhttp->data;
    hgobj gobj = parser->gobj;

    size_t pos = 0;
    if(parser->url) {
        pos = strlen(parser->url);
        parser->url = gbmem_realloc(
            parser->url,
            pos + length + 1
        );
    } else {
        parser->url = gbmem_malloc(length+1);
    }

    if(!parser->url) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "no memory for url",
            "length",       "%d", length,
            NULL
        );
        return -1;
    }
    memcpy(parser->url + pos, at, length);
    return 0;
}

/***************************************************************************
 *  Callbacks must return 0 on success.
 *  Returning a non-zero value indicates error to the parser,
 *  making it exit immediately.
 ***************************************************************************/
PRIVATE int on_header_field(llhttp_t* llhttp, const char* at, size_t length)
{
    GHTTP_PARSER *parser = llhttp->data;
    hgobj gobj = parser->gobj;

    if(!parser->jn_headers) {
        parser->jn_headers = json_object();
    }
    if(!parser->jn_headers) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "no memory for url",
            "length",       "%d", length,
            NULL
        );
        return -1;
    }

    size_t pos=0;
    if(parser->cur_key) {
        pos = strlen(parser->cur_key);
        parser->cur_key = gbmem_realloc(
            parser->cur_key,
            pos + length + 1
        );
    } else {
        parser->cur_key = gbmem_malloc(length+1);
    }
    if(!parser->cur_key) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "no memory for header field",
            "length",       "%d", length,
            NULL);
        return -1;
    }
    memcpy(parser->cur_key + pos, at, length);
    strntoupper(parser->cur_key + pos, length);
    return 0;
}

/***************************************************************************
 *  Callbacks must return 0 on success.
 *  Returning a non-zero value indicates error to the parser,
 *  making it exit immediately.
 ***************************************************************************/
PRIVATE int on_header_value(llhttp_t* llhttp, const char* at, size_t length)
{
    GHTTP_PARSER *parser = llhttp->data;
    hgobj gobj = parser->gobj;

    if(!parser->jn_headers) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "jn_headers NULL",
            "length",       "%d", length,
            NULL);
        return -1;
    }
    char add_value = 0;
    if(!parser->cur_key) {
        if(parser->last_key) {
            parser->cur_key = parser->last_key;
            parser->last_key = 0;
            add_value = 1;
        }
        if(!parser->cur_key) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "cur_key NULL",
                NULL);
            return -1;
        }
    }

    char *value;
    size_t pos = 0;
    if(add_value) {
        json_t *jn_partial_value = json_object_get(
            parser->jn_headers,
            parser->cur_key
        );
        const char *partial_value = json_string_value(jn_partial_value);
        pos = strlen(partial_value);
        value = gbmem_malloc(pos + length + 1);
        if(value) {
            memcpy(value, partial_value, pos);
        }
    } else {
        value = gbmem_malloc(length + 1);
    }
    if(!value) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "no memory for header value",
            "length",       "%d", length,
            NULL
        );
        return -1;
    }
    memcpy(value+pos, at, length);
    json_object_set_new(parser->jn_headers, parser->cur_key, json_string(value));
    GBMEM_FREE(value);

    if(parser->last_key) {
        GBMEM_FREE(parser->last_key);
        parser->last_key = 0;
    }
    parser->last_key = parser->cur_key;
    parser->cur_key = 0;
    return 0;
}
