/***********************************************************************
 *          C_PROT_HTTP_SRV.C
 *          Prot_http_srv GClass.
 *
 *          Protocol http as server
 *
 *          Limits, said to the peer before the connection is dropped:
 *          - a head (target and headers) over `max_header_size`: 431;
 *          - a body over `max_body_size`: 413;
 *          - more than `max_tx_queue` answers waiting for a peer that does
 *            not read them: dropped.
 *          A refused request gets its answer and then `timeout_linger`
 *          milliseconds to go (the bytes it still sends are read and
 *          thrown away): a socket closed with unread bytes resets the
 *          connection, and the peer could lose the answer.
 *          Once the connection is dropped (EV_DROP, or the peer left)
 *          nothing more of what was received is delivered, not even the
 *          rest of the requests pipelined in the same read.
 *
 *          Copyright (c) 2018-2021 Niyamaka.
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include "msg_ievent.h"
#include "c_timer.h"
#include "ghttp_parser.h"
#include "c_prot_http_sr.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define HTTP_REFUSED_HEADERS    "431 Request Header Fields Too Large"
#define HTTP_REFUSED_BODY       "413 Content Too Large"
#define DEFAULT_MAX_TX_QUEUE    64
#define DEFAULT_TIMEOUT_LINGER  2000
#define TEXT_OF(x)              TEXT_OF_(x)
#define TEXT_OF_(x)             #x

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE GHTTP_PARSER *create_request_parser(hgobj gobj);
PRIVATE int answer_refused_request(hgobj gobj, GHTTP_PARSER *parser);
PRIVATE BOOL tx_queue_is_full(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag------------default---------description---------- */
SDATA (DTP_INTEGER,     "timeout_inactivity",SDF_WR,        "300",          "Timeout inactivity, in seconds"),
SDATA (DTP_INTEGER,     "max_header_size",  SDF_RD,         "65536",        "Largest head of a request (target, header names and values), in bytes; 0: 65536; never more than half the largest memory block (gbmem). A longer one is answered 431 and the connection dropped, with ONE warning"),
SDATA (DTP_INTEGER,     "max_body_size",    SDF_RD,         "0",            "Largest body of a request, in bytes; 0, or more than half the largest memory block (gbmem): that half. A longer one is answered 413 and the connection dropped, with ONE warning"),
SDATA (DTP_INTEGER,     "max_tx_queue",     SDF_RD,         TEXT_OF(DEFAULT_MAX_TX_QUEUE),         "Most answers waiting in the transport for a peer that does not read them; one more drops the connection, with ONE warning. 0: no limit"),
SDATA (DTP_INTEGER,     "timeout_linger",   SDF_RD,         TEXT_OF(DEFAULT_TIMEOUT_LINGER),         "Milliseconds a refused request (431, 413) has to go after its answer, before the connection is dropped"),
SDATA (DTP_POINTER,     "user_data",        0,              0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,              0,              "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,              0,              "subscriber of output-events. If it's null then subscriber is the parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRAFFIC         = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"traffic",         "Trace traffic"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    GHTTP_PARSER *parsing_request;      // A request parser instance

    int timeout_inactivity;
    json_int_t max_header_size;
    json_int_t max_body_size;
    json_int_t max_tx_queue;
    json_int_t timeout_linger;
    BOOL lingering;                     // a refused request was answered: its bytes are thrown away
} PRIVATE_DATA;





            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout_inactivity,    gobj_read_integer_attr)
    SET_PRIV(max_header_size,       gobj_read_integer_attr)
    SET_PRIV(max_body_size,         gobj_read_integer_attr)
    SET_PRIV(max_tx_queue,          gobj_read_integer_attr)
    SET_PRIV(timeout_linger,        gobj_read_integer_attr)

    if(priv->max_header_size < 0 || priv->max_body_size < 0 || priv->max_tx_queue < 0 ||
            priv->timeout_linger < 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_PARAMETER,
            "msg",              "%s", "A limit of C_PROT_HTTP_SR is negative: its default taken",
            "max_header_size",  "%d", (int)priv->max_header_size,
            "max_body_size",    "%d", (int)priv->max_body_size,
            "max_tx_queue",     "%d", (int)priv->max_tx_queue,
            "timeout_linger",   "%d", (int)priv->timeout_linger,
            NULL
        );
        if(priv->max_header_size < 0) {
            priv->max_header_size = 0;
        }
        if(priv->max_body_size < 0) {
            priv->max_body_size = 0;
        }
        if(priv->max_tx_queue < 0) {
            priv->max_tx_queue = DEFAULT_MAX_TX_QUEUE;
        }
        if(priv->timeout_linger < 0) {
            priv->timeout_linger = DEFAULT_TIMEOUT_LINGER;
        }
    }

    priv->parsing_request = create_request_parser(gobj);
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout_inactivity,          gobj_read_integer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gobj_start(priv->timer);
    gobj_start_children(gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop_children(gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->parsing_request) {
        ghttp_parser_destroy(priv->parsing_request);
        priv->parsing_request = 0;
    }
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  The parser of the requests of a connection
 ***************************************************************************/
PRIVATE GHTTP_PARSER *create_request_parser(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GHTTP_PARSER *parser = ghttp_parser_create(
        gobj,
        HTTP_REQUEST,
        NULL,                   // on_header_event
        NULL,                   // on_body_event
        EV_ON_MESSAGE,
        gobj_is_service(gobj)?FALSE:TRUE // TRUE: use gobj_send_event, FALSE: use gobj_publish_event
    );
    if(parser) {
        parser->max_header_size = (size_t)priv->max_header_size;
        parser->max_body_size = (size_t)priv->max_body_size;
    }
    return parser;
}

/***************************************************************************
 *  A request refused by the parser (its head or its body over the limit)
 *  is answered, and the connection lingers: what the peer still sends is
 *  thrown away until it goes, or `timeout_linger` ends.
 *  Returns -1 when the connection must be dropped now: a protocol
 *  violation (not a refusal), or no `timeout_linger`.
 ***************************************************************************/
PRIVATE int answer_refused_request(hgobj gobj, GHTTP_PARSER *parser)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *code = NULL;
    if(parser->headers_refused) {
        code = HTTP_REFUSED_HEADERS;
    } else if(parser->body_refused) {
        code = HTTP_REFUSED_BODY;
    }
    if(!code) {
        return -1;  // Warning already logged (a protocol violation)
    }

    gbuffer_t *gbuf = gbuffer_create(256, 256);
    if(!gbuf) {
        return -1;  // Error already logged
    }
    gbuffer_printf(gbuf,
        "HTTP/1.1 %s\r\n"
        "Connection: close\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        code
    );
    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s", gobj_short_name(gobj_bottom_gobj(gobj)));
    }
    gobj_send_event(gobj_bottom_gobj(gobj), EV_TX_DATA,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        gobj
    );

    if(priv->timeout_linger == 0) {
        return -1;
    }
    priv->lingering = TRUE;
    set_timeout(priv->timer, priv->timeout_linger);
    return 0;
}

/***************************************************************************
 *  The transport holds `max_tx_queue` answers for a peer that does not
 *  read them
 ***************************************************************************/
PRIVATE BOOL tx_queue_is_full(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj bottom = gobj_bottom_gobj(gobj);
    if(priv->max_tx_queue <= 0 || !bottom || !gobj_has_attr(bottom, "cur_tx_queue")) {
        return FALSE;
    }
    json_int_t queued = gobj_read_integer_attr(bottom, "cur_tx_queue");
    if(queued < priv->max_tx_queue) {
        return FALSE;
    }
    const char *peername = gobj_read_str_attr(bottom, "peername");
    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PROTOCOL,
        "msg",          "%s", "HTTP peer does not read its answers, connection dropped",
        "cur_tx_queue", "%d", (int)queued,
        "max_tx_queue", "%d", (int)priv->max_tx_queue,
        "peername",     "%s", peername? peername : "",
        NULL
    );
    return TRUE;
}

/***************************************************************************
 *  Parse a http message
 *  Return -1 if error: you must close the socket.
 *  Return 0 if no new request.
 *  Return 1 if new request available in `request`.
 ***************************************************************************/
PRIVATE int parse_http_message(hgobj gobj, gbuffer_t *gbuf, GHTTP_PARSER *parser)
{
    size_t ln;
    while((ln=gbuffer_leftbytes(gbuf))>0) {
        char *bf = gbuffer_cur_rd_pointer(gbuf);
        int n = ghttp_parser_received(parser, bf, ln);
        if (n == -1) {
            // Error already logged
            // Http protocol violation, caller must to close the socket.
            // No parser reset here: a fresh parser is built by
            // ac_connected() on the next connection.
            return -1;
        } else if (n > 0) {
            gbuffer_get(gbuf, n);  // take out the bytes consumed
        }
    }
    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Rebuild the request parser for the new connection.
     *  A plain re-init of llhttp is not exposed any more; we use the
     *  destroy+create cycle so every connection starts with a pristine
     *  state machine.
     */
    if(priv->parsing_request) {
        ghttp_parser_destroy(priv->parsing_request);
    }
    priv->parsing_request = create_request_parser(gobj);
    priv->lingering = FALSE;

    gobj_publish_event(gobj, EV_ON_OPEN, 0);

    set_timeout(priv->timer, priv->timeout_inactivity*1000);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    priv->lingering = FALSE;

    /*
     *  Let llhttp complete any message whose terminator is the
     *  connection close.  Errors here are non-fatal (they simply mean
     *  the peer hung up mid-request). Inside an event of the parser it
     *  stops it: nothing more of the bytes in hand is delivered.
     */
    if(priv->parsing_request) {
        (void)ghttp_parser_finish(priv->parsing_request);
    }

    if(gobj_is_volatil(src)) {
        gobj_set_bottom_gobj(gobj, 0);
    }
    gobj_publish_event(gobj, EV_ON_CLOSE, 0);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Message completed
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_publish_event(gobj, event, kw);
    return 0;
}

/********************************************************************
 *
 ********************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);

    if(gobj_trace_level(gobj) & TRAFFIC) {
        //log_debug_gbuf(LOG_DUMP_INPUT, gbuf, "%s", gobj_short_name(gobj));
        gobj_trace_dump_gbuf(gobj, gbuf, "%s", gobj_short_name(gobj));
    }

    if(priv->lingering) {
        KW_DECREF(kw)
        return 0;   // the bytes of a refused request, thrown away
    }

    set_timeout(priv->timer, priv->timeout_inactivity*1000);

    int result = parse_http_message(gobj, gbuf, priv->parsing_request);
    if (result < 0) {
        if(answer_refused_request(gobj, priv->parsing_request) < 0) {
            gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
        }
    }

    KW_DECREF(kw)
    return 0;
}

/********************************************************************
 *
 ********************************************************************/
PRIVATE int ac_send_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = 0;

    if(tx_queue_is_full(gobj)) {
        // Warning already logged
        ghttp_parser_stop(priv->parsing_request);
        gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
        KW_DECREF(kw)
        return -1;
    }

    if(kw_has_key(kw, "body")) {
        // New method
        const char *code = kw_get_str(gobj, kw, "code", "200 OK", 0);
        const char *headers = kw_get_str(gobj, kw, "headers", "", 0);
        char *resp = json2uglystr(kw_get_dict_value(gobj, kw, "body", 0, 0));
        if(!resp) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "The body of an answer cannot be dumped",
                NULL
            );
            KW_DECREF(kw)
            return -1;
        }
        int body_len = strlen(resp);
        int headers_len = strlen(headers);

        /*
         *  RFC 7230 §3.3.2/§3.3.3: 1xx, 204 and 304 responses MUST NOT
         *  carry a message body or Content-Length.  llhttp enforces this
         *  strictly on the receiving side (any trailing byte after the
         *  CRLF CRLF is parsed as the start of the next response and
         *  fails with HPE_INVALID_CONSTANT), so emit a body-less
         *  response regardless of what the caller passed in `body`.
         */
        BOOL no_body = (code[0] == '1') ||
            (strncmp(code, "204", 3) == 0) ||
            (strncmp(code, "304", 3) == 0);

        gbuf = gbuffer_create(256 + headers_len + body_len, 256 + headers_len + body_len);
        if(no_body) {
            gbuffer_printf(gbuf,
                "HTTP/1.1 %s\r\n"
                "%s"
                "\r\n",
                code,
                headers
            );
        } else {
            gbuffer_printf(gbuf,
                "HTTP/1.1 %s\r\n"
                "%s"
                "Content-Type: application/json; charset=utf-8\r\n"
                "Content-Length: %d\r\n\r\n",
                code,
                headers,
                body_len
            );
            gbuffer_append(gbuf, resp, body_len);
        }
        GBMEM_FREE(resp)
    } else  {
        // Old method
        msg_iev_clean_metadata(kw);

        char *resp = json2uglystr(kw);
        int len = (int)strlen(resp);

        gbuf = gbuffer_create(256+len, 256+len);
        gbuffer_printf(gbuf,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "Content-Length: %d\r\n\r\n",
            len
        );
        gbuffer_append(gbuf, resp, len);
        GBMEM_FREE(resp)
    }

    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(
            gobj,
            gbuf,
            "%s", gobj_short_name(gobj_bottom_gobj(gobj))
        );
    }

    json_t *kw_response = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    KW_DECREF(kw)
    return gobj_send_event(gobj_bottom_gobj(gobj), EV_TX_DATA, kw_response, gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    ghttp_parser_stop(priv->parsing_request);
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_inactivity(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    ghttp_parser_stop(priv->parsing_request);
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Child stopped
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    JSON_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_PROT_HTTP_SR);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
//GOBJ_DEFINE_EVENT();

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_disconnected[] = {
        {EV_CONNECTED,          ac_connected,               ST_CONNECTED},
        {EV_DISCONNECTED,       ac_disconnected,            0},
        {EV_STOPPED,            ac_stopped,                 0},
        {0,0,0}
    };
    ev_action_t st_connected[] = {
        {EV_RX_DATA,            ac_rx_data,                 0},
        {EV_ON_MESSAGE,         ac_on_message,              0},
        {EV_SEND_MESSAGE,       ac_send_message,            0},
        {EV_TIMEOUT,            ac_timeout_inactivity,      0},
        {EV_TX_READY,           0,                          0},
        {EV_DROP,               ac_drop,                    0},
        {EV_DISCONNECTED,       ac_disconnected,            ST_DISCONNECTED},
        {EV_STOPPED,            ac_stopped,                 0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_DISCONNECTED,       st_disconnected},
        {ST_CONNECTED,          st_connected},
        {0, 0}
    };

    event_type_t event_types[] = { // HACK System gclass, not public events
        {EV_ON_MESSAGE,         EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT},
        {EV_RX_DATA,            0}, // TODO is necessary to define the internal or input events?
        {EV_SEND_MESSAGE,       EVF_PUBLIC_EVENT},
        {EV_CONNECTED,          0},
        {EV_DISCONNECTED,       0},
        {EV_DROP,               EVF_PUBLIC_EVENT},
        {EV_TIMEOUT,            0},
        {EV_TX_READY,           0},
        {EV_STOPPED,            0},
        {EV_ON_OPEN,            EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,           EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT},
        {NULL, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  //lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,  // s_user_trace_level
        0   // gclass_flag
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_prot_http_sr(void)
{
    return create_gclass(C_PROT_HTTP_SR);
}
