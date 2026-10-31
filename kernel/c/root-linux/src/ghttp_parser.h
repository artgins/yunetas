/****************************************************************************
 *          GHTTP_PARSER.H
 *          Mixin http_parser - gobj-ecosistema
 *
 *          Copyright (c) 2013 Niyamaka.
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/*
 *  The head of a message (the request target or the status text, and the
 *  names and values of its headers) longer than this is refused, when
 *  `max_header_size` is 0. Two cookies of 4096 bytes (the largest cookie a
 *  browser keeps) fit in it, with room for the rest of a site's cookies.
 */
#define GHTTP_MAX_HEADER_SIZE   (64*1024)

typedef struct _GHTTP_PARSER {
    llhttp_t llhttp;
    hgobj gobj;
    gobj_event_t on_header_event;
    gobj_event_t on_body_event;
    gobj_event_t on_message_event;
    BOOL send_event;

    llhttp_type_t type;
    char message_completed;
    char headers_completed;
    char message_begun;     // a message is being parsed: its first byte came, it did not end

    char *url;
    json_t *jn_headers;
    size_t body_size;
    gbuffer_t *gbuf_body;
    size_t max_body_size;   // largest body; 0: half the largest memory block (on_message_event) or no limit (on_body_event alone); never more than that half with on_message_event
    size_t max_header_size; // largest head (target or status text, names, values); 0: GHTTP_MAX_HEADER_SIZE; never more than half the largest memory block
    size_t header_size;     // bytes of the head of the message so far
    BOOL body_refused;      // a body passed its limit: the parser is stopped, already said
    BOOL headers_refused;   // a head passed its limit: the parser is stopped, already said
    BOOL stopped;           // ghttp_parser_stop(): nothing more is delivered
    BOOL executing;         // inside llhttp_execute()

    gbuffer_t *gbuf_url;    // the target, while it arrives
    gbuffer_t *gbuf_field;  // the name of the header that arrives
    gbuffer_t *gbuf_value;  // its value
} GHTTP_PARSER;

PUBLIC GHTTP_PARSER *ghttp_parser_create(
    hgobj gobj,
    llhttp_type_t type,
    gobj_event_t on_header_event,       // Event to publish or send when the header is completed
        /* kw of event:
            {
                "http_parser_type":     (int) llhttp_type_t,
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
                "__pbf__":              (uint8_t *)(uintptr_t) (int) pointer to buffer with the partial body received,
                "__pbf_size__":         (size_t) (int) size of buffer
            }
            HACK: The last event without "__pbf__" key will indicate that all message is completed
        */

    gobj_event_t on_message_event,      // Event to publish or send when the message is completed
        /* kw of event:
            {
                "http_parser_type":     (int) llhttp_type_t,
                "url":                  (string) "url",
                "response_status_code": (int) status_code,
                "request_method":       (int) method,
                "headers":              (json_object) jn_headers,
                "body_size":            (int) bytes of the body,

                if content-type == application/json
                    "body":             (anystring2json)
                else
                    "gbuffer":          gbuffer with full body
            }
            A body longer than its limit (`max_body_size`, see the struct),
            or a head longer than its limit (`max_header_size`), is not
            kept: the parser logs ONE warning and stops
            (ghttp_parser_received() returns -1, `body_refused` or
            `headers_refused` say which, the caller drops the connection).
            A RESPONSE parser publishes the message first, without "body"
            nor "gbuffer", with "body_too_large" or "headers_too_large":
            (int) the limit that was passed, so the request waiting for it
            is answered; a REQUEST parser publishes nothing (the server
            answers 413 or 431). A response that breaks the protocol once
            begun is published the same way, with "protocol_error":
            (string) the reason of llhttp.
        */

    BOOL send_event  // TRUE: use gobj_send_event() to parent, FALSE: use gobj_publish_event()
);
PUBLIC int ghttp_parser_received( /* Return bytes consumed or -1 if error */
    GHTTP_PARSER *parser,
    char *bf,
    size_t len
);

/*
 *  Signal end-of-stream to the parser (TCP peer closed the socket).
 *  Required to complete messages whose terminator is the connection
 *  close (HTTP/1.0 responses without Content-Length, or HTTP/1.1
 *  responses with neither Content-Length nor Transfer-Encoding:
 *  chunked).  Call it from the gobj's disconnect handler. Inside an
 *  llhttp callback (the disconnect came inside an event published by the
 *  parser) it is ghttp_parser_stop().
 *  Returns 0 on success, -1 on protocol error.
 */
PUBLIC int ghttp_parser_finish(GHTTP_PARSER *parser);

/*
 *  The connection of the parser ends (it is dropped, or it closed): nothing
 *  more is delivered, not even the rest of the bytes being parsed now, and
 *  ghttp_parser_received() takes every byte given to it and parses none.
 *  Call it from the drop or disconnect handler; it is safe inside an llhttp
 *  callback (an event published by the parser), where ghttp_parser_finish()
 *  does the same. A new connection needs a new parser.
 */
PUBLIC void ghttp_parser_stop(GHTTP_PARSER *parser);

PUBLIC void ghttp_parser_destroy(GHTTP_PARSER *parser);

#ifdef __cplusplus
}
#endif
