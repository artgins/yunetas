/***********************************************************************
 *          C_PROT_HTTP_SRV.C
 *          Prot_http_srv GClass.
 *
 *          Protocol http as server
 *
 *          Limits:
 *          - a head (target and headers) over `max_header_size`: 431;
 *          - a body over `max_body_size`: 413.
 *          A refused request gets its answer, after the answers of the
 *          requests before it (an application may answer later, and HTTP
 *          answers go in the order of the requests), and then
 *          `timeout_linger` milliseconds to go (the bytes it still sends are
 *          read and thrown away): a socket closed with unread bytes resets
 *          the connection, and the peer could lose the answer.
 *
 *          The answers a peer leaves unread have a limit too. The queue of
 *          the transport moves only at the end of a write, in the event
 *          loop, so while one read is parsed no answer leaves, whether the
 *          peer reads or not: the length of the queue alone says nothing of
 *          the peer. With `max_tx_queue` answers waiting:
 *          - what the peer sends is held, not parsed, until the queue is
 *            empty (EV_TX_READY), so a pipeline of any length is served in
 *            pieces and the answers never pile up by more than one read;
 *          - a peer that passes `max_rx_held` bytes held is dropped;
 *          - a peer with no write ended in `timeout_tx_stall` milliseconds
 *            is dropped.
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
#define DEFAULT_TIMEOUT_TX_STALL 5000
#define DEFAULT_MAX_RX_HELD     1048576
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
PRIVATE int send_refusal(hgobj gobj, const char *code);
PRIVATE void drop_connection(hgobj gobj);
PRIVATE BOOL tx_queue_is_full(hgobj gobj);
PRIVATE BOOL peer_does_not_read(hgobj gobj);
PRIVATE int hold_received_bytes(hgobj gobj, gbuffer_t *gbuf);
PRIVATE void parse_held_requests(hgobj gobj);
PRIVATE void parse_requests(hgobj gobj, gbuffer_t *gbuf);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag------------default---------description---------- */
SDATA (DTP_INTEGER,     "timeout_inactivity",SDF_WR,        "300",          "Timeout inactivity, in seconds"),
SDATA (DTP_INTEGER,     "max_header_size",  SDF_RD,         "65536",        "Largest head of a request (target, header names and values), in bytes; 0: 65536; never more than half the largest memory block (gbmem). Each header counts GHTTP_HEADER_COST bytes more. A longer one is answered 431 and the connection dropped, with ONE warning"),
SDATA (DTP_INTEGER,     "max_body_size",    SDF_RD,         "0",            "Largest body of a request, in bytes; 0, or more than half the largest memory block (gbmem): that half. A longer one is answered 413 and the connection dropped, with ONE warning"),
SDATA (DTP_INTEGER,     "max_tx_queue",     SDF_RD,         TEXT_OF(DEFAULT_MAX_TX_QUEUE),         "Answers waiting in the transport from which what the peer sends is held, not parsed, until they leave. 0: no limit"),
SDATA (DTP_INTEGER,     "timeout_tx_stall", SDF_RD,         TEXT_OF(DEFAULT_TIMEOUT_TX_STALL),     "Milliseconds with max_tx_queue answers waiting and no write ended: the peer does not read, the connection is dropped, with ONE warning. 0: never"),
SDATA (DTP_INTEGER,     "max_rx_held",      SDF_RD,         TEXT_OF(DEFAULT_MAX_RX_HELD),          "Most bytes held behind max_tx_queue answers; one more drops the connection, with ONE warning. 0: no limit"),
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
    json_int_t timeout_tx_stall;
    json_int_t max_rx_held;
    json_int_t timeout_linger;
    BOOL lingering;                     // a request was refused: its bytes are thrown away
    const char *refusal;                // a refused request, waiting for the answers before it
    size_t answers_sent;                // of the requests published by the parser of this connection

    uint64_t tx_stall_timer;            // max_tx_queue answers wait: no write ended until this, the peer does not read
    json_int_t tx_stall_written;        // the bytes written when it started
    dl_list_t dl_held;                  // what the peer sent behind max_tx_queue answers, not parsed
    size_t rx_held;                     // its bytes
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
    dl_init(&priv->dl_held, gobj);

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
    SET_PRIV(timeout_tx_stall,      gobj_read_integer_attr)
    SET_PRIV(max_rx_held,           gobj_read_integer_attr)
    SET_PRIV(timeout_linger,        gobj_read_integer_attr)

    if(priv->max_header_size < 0 || priv->max_body_size < 0 || priv->max_tx_queue < 0 ||
            priv->timeout_tx_stall < 0 || priv->max_rx_held < 0 || priv->timeout_linger < 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_PARAMETER,
            "msg",              "%s", "A limit of C_PROT_HTTP_SR is negative: its default taken",
            "max_header_size",  "%d", (int)priv->max_header_size,
            "max_body_size",    "%d", (int)priv->max_body_size,
            "max_tx_queue",     "%d", (int)priv->max_tx_queue,
            "timeout_tx_stall", "%d", (int)priv->timeout_tx_stall,
            "max_rx_held",      "%d", (int)priv->max_rx_held,
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
        if(priv->timeout_tx_stall < 0) {
            priv->timeout_tx_stall = DEFAULT_TIMEOUT_TX_STALL;
        }
        if(priv->max_rx_held < 0) {
            priv->max_rx_held = DEFAULT_MAX_RX_HELD;
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
    dl_flush(&priv->dl_held, (fnfree)gbuffer_decref);
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
 *  thrown away until it goes, or `timeout_linger` ends. With answers still
 *  due to the requests before it (an application that answers later), the
 *  refusal waits for them in `refusal`: HTTP answers go in order.
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

    priv->lingering = TRUE;
    dl_flush(&priv->dl_held, (fnfree)gbuffer_decref);
    priv->rx_held = 0;
    priv->tx_stall_timer = 0;

    if(priv->answers_sent < parser->messages_published) {
        priv->refusal = code;
        return 0;
    }
    return send_refusal(gobj, code);
}

/***************************************************************************
 *  The answer of a refused request, the last one of the connection.
 *  Returns -1 when the connection must be dropped now: no `timeout_linger`.
 ***************************************************************************/
PRIVATE int send_refusal(hgobj gobj, const char *code)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

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
    set_timeout(priv->timer, priv->timeout_linger);
    return 0;
}

/***************************************************************************
 *  The connection ends: nothing more is delivered
 ***************************************************************************/
PRIVATE void drop_connection(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    ghttp_parser_stop(priv->parsing_request);
    dl_flush(&priv->dl_held, (fnfree)gbuffer_decref);
    priv->rx_held = 0;
    priv->tx_stall_timer = 0;
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
}

/***************************************************************************
 *  The transport holds `max_tx_queue` answers. It says nothing of the peer:
 *  the queue moves only at the end of a write, out of this turn of the loop.
 ***************************************************************************/
PRIVATE BOOL tx_queue_is_full(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj bottom = gobj_bottom_gobj(gobj);
    if(priv->max_tx_queue <= 0 || !bottom || !gobj_has_attr(bottom, "cur_tx_queue")) {
        return FALSE;
    }
    return (gobj_read_integer_attr(bottom, "cur_tx_queue") >= priv->max_tx_queue)? TRUE : FALSE;
}

/***************************************************************************
 *  The queue is full and no write ended in `timeout_tx_stall` milliseconds:
 *  the only thing that says the peer does not read. Asked at each answer
 *  and at each read, and by the timer when the peer goes silent. No write
 *  can end inside a turn of the loop, so the first time it is asked only
 *  starts the wait, and any write that ends starts it again. ONE warning:
 *  after it the parser is stopped.
 ***************************************************************************/
PRIVATE BOOL peer_does_not_read(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->parsing_request->stopped || priv->lingering || priv->timeout_tx_stall == 0) {
        return FALSE;
    }
    if(!tx_queue_is_full(gobj)) {
        priv->tx_stall_timer = 0;
        return FALSE;
    }

    hgobj bottom = gobj_bottom_gobj(gobj);
    json_int_t written = gobj_read_integer_attr(bottom, "txBytes");
    if(priv->tx_stall_timer == 0 || written != priv->tx_stall_written) {
        priv->tx_stall_written = written;
        priv->tx_stall_timer = start_msectimer((uint64_t)priv->timeout_tx_stall);
        set_timeout(priv->timer, priv->timeout_tx_stall);
        return FALSE;
    }
    if(!test_msectimer(priv->tx_stall_timer)) {
        set_timeout(priv->timer, priv->timeout_tx_stall);
        return FALSE;
    }

    const char *peername = gobj_read_str_attr(bottom, "peername");
    gobj_log_warning(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_PROTOCOL,
        "msg",              "%s", "HTTP peer does not read its answers, connection dropped",
        "cur_tx_queue",     "%d", (int)gobj_read_integer_attr(bottom, "cur_tx_queue"),
        "max_tx_queue",     "%d", (int)priv->max_tx_queue,
        "timeout_tx_stall", "%d", (int)priv->timeout_tx_stall,
        "peername",         "%s", peername? peername : "",
        NULL
    );
    return TRUE;
}

/***************************************************************************
 *  What the peer sent behind a full queue waits, in the pieces it came:
 *  those pieces are what limits the answers that each turn adds.
 *  -1: it passed `max_rx_held`, ONE warning, the caller drops the connection.
 ***************************************************************************/
PRIVATE int hold_received_bytes(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t len = gbuffer_leftbytes(gbuf);
    if(priv->max_rx_held > 0 && priv->rx_held + len > (size_t)priv->max_rx_held) {
        hgobj bottom = gobj_bottom_gobj(gobj);
        const char *peername = gobj_read_str_attr(bottom, "peername");
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "HTTP peer does not read its answers and keeps sending, connection dropped",
            "cur_tx_queue", "%d", (int)gobj_read_integer_attr(bottom, "cur_tx_queue"),
            "max_tx_queue", "%d", (int)priv->max_tx_queue,
            "rx_held",      "%lu", (unsigned long)(priv->rx_held + len),
            "max_rx_held",  "%d", (int)priv->max_rx_held,
            "peername",     "%s", peername? peername : "",
            NULL
        );
        return -1;
    }

    gbuffer_t *gbuf_held = gbuffer_create(len, len);
    if(!gbuf_held) {
        return -1;  // Error already logged
    }
    if(gbuffer_append(gbuf_held, gbuffer_cur_rd_pointer(gbuf), len) != len) {
        GBUFFER_DECREF(gbuf_held)
        return -1;  // Error already logged
    }
    dl_add(&priv->dl_held, gbuf_held);
    priv->rx_held += len;
    return 0;
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

/***************************************************************************
 *  Parse what came: a refused request is answered, what is left of the
 *  connection after a violation is dropped.
 ***************************************************************************/
PRIVATE void parse_requests(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(parse_http_message(gobj, gbuf, priv->parsing_request) < 0) {
        if(answer_refused_request(gobj, priv->parsing_request) < 0) {
            drop_connection(gobj);
        }
    }
}

/***************************************************************************
 *  Parse what was held, while the queue has room. A drop inside the parse
 *  flushes the list, so each piece is out of it first.
 ***************************************************************************/
PRIVATE void parse_held_requests(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf;
    while(!tx_queue_is_full(gobj) && (gbuf = dl_first(&priv->dl_held))) {
        dl_delete(&priv->dl_held, gbuf, 0);
        priv->rx_held -= gbuffer_leftbytes(gbuf);
        parse_requests(gobj, gbuf);
        GBUFFER_DECREF(gbuf)
    }
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
    priv->refusal = NULL;
    priv->answers_sent = 0;
    priv->tx_stall_timer = 0;
    dl_flush(&priv->dl_held, (fnfree)gbuffer_decref);
    priv->rx_held = 0;

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
    priv->refusal = NULL;
    priv->tx_stall_timer = 0;
    dl_flush(&priv->dl_held, (fnfree)gbuffer_decref);
    priv->rx_held = 0;

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

    if(dl_size(&priv->dl_held) > 0 || tx_queue_is_full(gobj)) {
        if(hold_received_bytes(gobj, gbuf) < 0 || peer_does_not_read(gobj)) {
            drop_connection(gobj);  // Warning already logged
        }
    } else {
        parse_requests(gobj, gbuf);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The queue is empty: the peer read it all
 ***************************************************************************/
PRIVATE int ac_tx_ready(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    parse_held_requests(gobj);

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

    if(peer_does_not_read(gobj)) {
        // Warning already logged
        drop_connection(gobj);
        KW_DECREF(kw)
        return -1;
    }
    if(priv->answers_sent < priv->parsing_request->messages_published) {
        priv->answers_sent++;
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
    int ret = gobj_send_event(gobj_bottom_gobj(gobj), EV_TX_DATA, kw_response, gobj);

    if(priv->refusal && priv->answers_sent == priv->parsing_request->messages_published) {
        const char *code = priv->refusal;
        priv->refusal = NULL;
        if(send_refusal(gobj, code) < 0) {
            drop_connection(gobj);
        }
    }
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    drop_connection(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The timer: the inactivity, or the end of `timeout_linger`, or, with
 *  `tx_stall_timer`, the wait of a queue that is full. A peer that reads
 *  goes on with the inactivity again.
 ***************************************************************************/
PRIVATE int ac_timeout_inactivity(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->tx_stall_timer && !peer_does_not_read(gobj)) {
        if(!priv->tx_stall_timer) {
            set_timeout(priv->timer, priv->timeout_inactivity*1000);
            parse_held_requests(gobj);
        }
        KW_DECREF(kw)
        return 0;
    }

    drop_connection(gobj);
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
        {EV_TX_READY,           ac_tx_ready,                0},
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
