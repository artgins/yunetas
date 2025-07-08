/***********************************************************************
 *          GHTTP_PARSER.C
 *          Mixin http_parser - gobj-ecosistema
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
PRIVATE int on_message_begin(http_parser* http_parser);
PRIVATE int on_headers_complete(http_parser* http_parser);
PRIVATE int on_message_complete(http_parser* http_parser);
PRIVATE int on_url(http_parser* http_parser, const char* at, size_t length);
PRIVATE int on_header_field(http_parser* http_parser, const char* at, size_t length);
PRIVATE int on_header_value(http_parser* http_parser, const char* at, size_t length);
PRIVATE int on_body(http_parser* http_parser, const char* at, size_t length);

/****************************************************************
 *         Data
 ****************************************************************/
PRIVATE http_parser_settings settings = {
  .on_message_begin = on_message_begin,
  .on_url = on_url,
  .on_header_field = on_header_field,
  .on_header_value = on_header_value,
  .on_headers_complete = on_headers_complete,
  .on_body = on_body,
  .on_message_complete = on_message_complete,
};

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC GHTTP_PARSER *ghttp_parser_create(
    hgobj gobj,
    enum http_parser_type type,
    gobj_event_t on_header_event,       // Event to publish or send when the header is completed
    gobj_event_t on_body_event,         // Event to publish or send when the body is receiving
    gobj_event_t on_message_event,      // Event to publish or send when the message is completed
    BOOL send_event  // TRUE: use gobj_send_event(), FALSE: use gobj_publish_event()
)
{
    GHTTP_PARSER *parser;

    /*--------------------------------*
     *      Alloc memory
     *--------------------------------*/
    parser = GBMEM_MALLOC(sizeof(GHTTP_PARSER));
    if(!parser) {
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY_ERROR,
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

    http_parser_init(&parser->http_parser, type);
    parser->http_parser.data = parser;

    return parser;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void ghttp_parser_destroy(GHTTP_PARSER *parser)
{
    ghttp_parser_reset(parser);
    GBMEM_FREE(parser);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void ghttp_parser_reset(GHTTP_PARSER *parser)
{
    parser->headers_completed = 0;
    parser->message_completed = 0;
    parser->body_size = 0;
    GBMEM_FREE(parser->url);
    //GBMEM_FREE(parser->body);
    GBUFFER_DECREF(parser->gbuf_body)
    JSON_DECREF(parser->jn_headers);
    GBMEM_FREE(parser->cur_key);
    GBMEM_FREE(parser->last_key);
    http_parser_init(&parser->http_parser, parser->type);
}

/***************************************************************************
 *  Return bytes consumed or -1 if error
 ***************************************************************************/
PUBLIC int ghttp_parser_received(
    GHTTP_PARSER *parser,
    char *buf,
    size_t received
) {
    hgobj gobj = parser->gobj;

    size_t nparsed = http_parser_execute(&parser->http_parser, &settings, buf, received);
    if (parser->http_parser.upgrade) {
        /* handle new protocol */
    } else if (nparsed != received) {
        /* Handle error. Usually just close the connection. */
        gobj_log_error(gobj,0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "http_parser_execute() FAILED 1",
            "error",        "%s", http_errno_name(HTTP_PARSER_ERRNO(&parser->http_parser)),
            "desc",         "%s", http_errno_description(HTTP_PARSER_ERRNO(&parser->http_parser)),
            NULL
        );
        gobj_trace_dump(gobj, buf, received, "http_parser_execute() FAILED 1");
        return -1;
    } else {
        if(HTTP_PARSER_ERRNO(&parser->http_parser) != HPE_OK) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
                "msg",          "%s", "http_parser_execute() FAILED 2",
                "error",        "%s", http_errno_name(HTTP_PARSER_ERRNO(&parser->http_parser)),
                "desc",         "%s", http_errno_description(HTTP_PARSER_ERRNO(&parser->http_parser)),
                NULL
            );
            gobj_trace_dump(gobj, buf, received, "http_parser_execute() FAILED 2");
            return -1;
        }
    }
    return (int)nparsed;
}

/***************************************************************************
 *  Callbacks must return 0 on success.
 *  Returning a non-zero value indicates error to the parser,
 *  making it exit immediately.
 ***************************************************************************/
PRIVATE int on_message_begin(http_parser* http_parser)
{
//     GHTTP_PARSER *parser = http_parser->data;
//     ghttp_parser_reset(parser);
    return 0;
}

/***************************************************************************
 *  Callbacks must return 0 on success.
 *  Returning a non-zero value indicates error to the parser,
 *  making it exit immediately.
 ***************************************************************************/
PRIVATE int on_headers_complete(http_parser* http_parser)
{
    GHTTP_PARSER *parser = http_parser->data;
    hgobj gobj = parser->gobj;

    parser->headers_completed = 1;
    if(!parser->jn_headers) {
        parser->jn_headers = json_object();
    }
    if(parser->on_header_event) {
        json_t *kw_http = json_pack("{s:i, s:s, s:i, s:i, s:O}",
            "http_parser_type",     (int)parser->type,
            "url",                  parser->url?parser->url:"",
            "response_status_code", (int)parser->http_parser.status_code,
            "request_method",       (int)parser->http_parser.method,
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
PRIVATE int on_body(http_parser* http_parser, const char* at, size_t length)
{
    GHTTP_PARSER *parser = http_parser->data;
    hgobj gobj = parser->gobj;

    if(parser->on_body_event) {
        json_t *kw_http = json_pack("{s:i, s:I, s:I}",
            "response_status_code", (int)parser->http_parser.status_code,
            "__pbf__", (json_int_t)(size_t)at,
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

// Old code
//        if(parser->body) {
//            parser->body = gobj_realloc_func()(
//                parser->body,
//                parser->body_size + length + 1
//            );
//        } else {
//            parser->body = gobj_malloc_func()(length + 1);
//        }
//
//        if(!parser->body) {
//            gobj_log_error(gobj, 0,
//                "function",     "%s", __FUNCTION__,
//                "msgset",       "%s", MSGSET_MEMORY_ERROR,
//                "msg",          "%s", "no memory for body",
//                "body_size",    "%d", parser->body_size,
//                "length",       "%d", length,
//                NULL);
//            return -1;
//        }
//        memcpy(parser->body + parser->body_size, at, length);

        if(!parser->gbuf_body) {
            parser->gbuf_body = gbuffer_create(RX_GBUFFER_SIZE, gobj_get_maximum_block());
            if(!parser->gbuf_body) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MEMORY_ERROR,
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
PRIVATE int on_message_complete(http_parser* http_parser)
{
    GHTTP_PARSER *parser = http_parser->data;
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
            "response_status_code", (int)parser->http_parser.status_code
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
            "response_status_code", (int)parser->http_parser.status_code,
            "request_method",       (int)parser->http_parser.method,
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
                    "msgset",       "%s", MSGSET_MEMORY_ERROR,
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
                json_object_set_new(kw_http, "gbuffer", json_integer((json_int_t)(size_t)parser->gbuf_body));
                parser->gbuf_body = 0;
            }
        }
        ghttp_parser_reset(parser);
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
PRIVATE int on_url(http_parser* http_parser, const char* at, size_t length)
{
    GHTTP_PARSER *parser = http_parser->data;
    hgobj gobj = parser->gobj;

    size_t pos = 0;
    if(parser->url) {
        pos = strlen(parser->url);
        parser->url = gobj_realloc_func()(
            parser->url,
            pos + length + 1
        );
    } else {
        parser->url = gobj_malloc_func()(length+1);
    }

    if(!parser->url) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
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
PRIVATE int on_header_field(http_parser* http_parser, const char* at, size_t length)
{
    GHTTP_PARSER *parser = http_parser->data;
    hgobj gobj = parser->gobj;

    if(!parser->jn_headers) {
        parser->jn_headers = json_object();
    }
    if(!parser->jn_headers) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "no memory for url",
            "length",       "%d", length,
            NULL
        );
        return -1;
    }

    size_t pos=0;
    if(parser->cur_key) {
        pos = strlen(parser->cur_key);
        parser->cur_key = gobj_realloc_func()(
            parser->cur_key,
            pos + length + 1
        );
    } else {
        parser->cur_key = gobj_malloc_func()(length+1);
    }
    if(!parser->cur_key) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
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
PRIVATE int on_header_value(http_parser* http_parser, const char* at, size_t length)
{
    GHTTP_PARSER *parser = http_parser->data;
    hgobj gobj = parser->gobj;

    if(!parser->jn_headers) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
        value = gobj_malloc_func()(pos + length + 1);
        if(value) {
            memcpy(value, partial_value, pos);
        }
    } else {
        value = gobj_malloc_func()(length + 1);
    }
    if(!value) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
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
