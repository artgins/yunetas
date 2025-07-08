/****************************************************************************
 *          GHTTP_PARSER.H
 *          Mixin http_parser - gobj-ecosistema
 *
 *          Copyright (c) 2013 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

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

PUBLIC GHTTP_PARSER *ghttp_parser_create(
    hgobj gobj,
    enum http_parser_type type,
    gobj_event_t on_header_event,       // Event to publish or send when the header is completed
        /* kw of event:
            {
                "http_parser_type":     (int) http_parser_type,
                "url":                  (string) "url",
                "response_status_code": (int) status_code,
                "request_method":       (int) method,
                "headers":              (json_object) jn_headers
            }
        */
    gobj_event_t on_body_event,         // Event to publish or send when the body is receiving
        /*
           kw of event:
            {
                "__pbf__":              (uint8_t *)(size_t) (int) pointer to buffer with the partial body received,
                "__pbf_size__":         (size_t) (int) size of buffer
            }
            HACK: The last event without "__pbf__" key will indicate that all message is completed
        */

    gobj_event_t on_message_event,      // Event to publish or send when the message is completed
        /* kw of event:
            {
                "http_parser_type":     (int) http_parser_type,
                "url":                  (string) "url",
                "response_status_code": (int) status_code,
                "request_method":       (int) method,
                "headers":              (json_object) jn_headers,

                if content-type == application/json
                    "body":             (anystring2json)
                else
                    "gbuffer":          gbuffer with full body
            }
        */

    BOOL send_event  // TRUE: use gobj_send_event() to parent, FALSE: use gobj_publish_event()
);
PUBLIC int ghttp_parser_received( /* Return bytes consumed or -1 if error */
    GHTTP_PARSER *parser,
    char *bf,
    size_t len
);
PUBLIC void ghttp_parser_destroy(GHTTP_PARSER *parser);

PUBLIC void ghttp_parser_reset(GHTTP_PARSER *parser);

#ifdef __cplusplus
}
#endif
