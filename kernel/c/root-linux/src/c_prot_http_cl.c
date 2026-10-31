/****************************************************************************
 *          c_prot_http_cli.c
 *          Prot_http_cli GClass.
 *
 *          Protocol http as client
 *
 *          Limits, each said with ONE warning:
 *          - a response whose head (status text, header names and values)
 *            passes `max_header_size`, or whose body passes
 *            `max_body_size`, is published without its body
 *            (`headers_too_large`, `body_too_large`) and the connection
 *            dropped; so is a response that breaks the protocol once begun
 *            (`protocol_error`): the request waiting for it is answered,
 *            and it is never sent again by this gclass;
 *          - `max_tx_queue` requests waiting in the transport and no write
 *            ended in `timeout_tx_stall` milliseconds, a server that does
 *            not read: the next request is refused and the connection
 *            dropped. The queue alone says nothing of the server: it moves
 *            only at the end of a write, in the event loop, so a burst sent
 *            in one turn fills it whether the server reads or not.
 *          Once the connection is dropped nothing more of what was
 *          received is delivered.
 *
 *          Copyright (c) 2017-2023 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <strings.h>
#include <limits.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include <yuneta_version.h>

#ifdef ESP_PLATFORM
    #include "c_esp_transport.h"
#endif
#ifdef __linux__
    #include "c_tcp.h"
#endif

#include "ghttp_parser.h"
#include "c_timer.h"
#include "c_prot_http_cl.h"

/***************************************************************
 *              Constants
 ***************************************************************/
#define DEFAULT_MAX_TX_QUEUE    64
#define DEFAULT_TIMEOUT_TX_STALL 5000
#define TEXT_OF(x)              TEXT_OF_(x)
#define TEXT_OF_(x)             #x

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int check_url(hgobj gobj);
PRIVATE GHTTP_PARSER *create_response_parser(hgobj gobj);
PRIVATE int append_form_text(gbuffer_t *gbuf, const char *text, size_t len);
PRIVATE size_t form_text_size(const char *text, size_t len);
PRIVATE const char *form_value_text(json_t *v, size_t *len, char **to_free);
PRIVATE gbuffer_t *build_form(hgobj gobj, json_t *jn_data);
PRIVATE BOOL server_does_not_read(hgobj gobj);

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE const sdata_desc_t attrs_table[] = {
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_STRING,  "url",              SDF_PERSIST,    "",         "Url to connect"),
SDATA (DTP_INTEGER, "timeout_inactivity",SDF_WR,        "60000",    "Timeout inactivity, in milliseconds"),
SDATA (DTP_STRING,  "cert_pem",         SDF_PERSIST,    "",         "SSL server certificate, PEM format"),
SDATA (DTP_DICT,    "crypto",           SDF_RD,         "{\"ssl_use_system_ca\": true, \"ssl_verify_mode\": \"required\"}", "TLS crypto forwarded to the bottom C_TCP for https:// urls. Verifies the server cert against the system CA by default; override with ssl_trusted_certificate for a private CA, or ssl_allow_insecure_client=true to skip (MITM risk)."),
SDATA (DTP_BOOLEAN, "raw_body_data",    SDF_RD,         "FALSE",    "Publish raw partial data of body or full body at the end"),
SDATA (DTP_INTEGER, "max_body_size",    SDF_RD,         "0",        "Largest body of a response, in bytes; 0, or more than half the largest memory block (gbmem): that half (with raw_body_data, 0 is no limit). A longer one is refused with ONE warning: EV_ON_MESSAGE carries `body_too_large` (the limit) and no body (with raw_body_data nothing more is published), and the connection is dropped"),
SDATA (DTP_INTEGER, "max_header_size",  SDF_RD,         "65536",    "Largest head of a response (status text, header names and values), in bytes; 0: 65536; never more than half the largest memory block (gbmem). Each header counts GHTTP_HEADER_COST bytes more. A longer one is refused with ONE warning: EV_ON_MESSAGE carries `headers_too_large` (the limit) and no body (with raw_body_data nothing is published), and the connection is dropped"),
SDATA (DTP_INTEGER, "max_tx_queue",     SDF_RD,         TEXT_OF(DEFAULT_MAX_TX_QUEUE), "Requests waiting in the transport from which a server that writes nothing is one that does not read. 0: no limit"),
SDATA (DTP_INTEGER, "timeout_tx_stall", SDF_RD,         TEXT_OF(DEFAULT_TIMEOUT_TX_STALL), "Milliseconds with max_tx_queue requests waiting and no write ended: the server does not read, the next request is refused and drops the connection, with ONE warning. 0: never"),
SDATA (DTP_POINTER, "user_data",        0,              0,          "user data"),
SDATA (DTP_POINTER, "user_data2",       0,              0,          "more user data"),
SDATA (DTP_POINTER, "subscriber",       0,              0,          "subscriber of output-events. If it's null then subscriber is the parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendant value!
 *  required paired correlative strings
 *  in s_user_trace_level
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
    int timeout_inactivity;
    hgobj timer;
    GHTTP_PARSER *parsing_response;
    const char *url;
    json_int_t max_body_size;
    json_int_t max_header_size;
    json_int_t max_tx_queue;
    json_int_t timeout_tx_stall;
    uint64_t tx_stall_timer;        // max_tx_queue requests wait: no write ended until this, the server does not read
    json_int_t tx_stall_written;    // the bytes written when it started
    char schema[64];
    char host[NAME_MAX];
    int port;
} PRIVATE_DATA;





                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    SET_PRIV(max_body_size,         gobj_read_integer_attr)
    SET_PRIV(max_header_size,       gobj_read_integer_attr)
    SET_PRIV(max_tx_queue,          gobj_read_integer_attr)
    SET_PRIV(timeout_tx_stall,      gobj_read_integer_attr)
    if(priv->max_body_size < 0 || priv->max_header_size < 0 || priv->max_tx_queue < 0 ||
            priv->timeout_tx_stall < 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_PARAMETER,
            "msg",              "%s", "A limit of C_PROT_HTTP_CL is negative: its default taken",
            "max_body_size",    "%d", (int)priv->max_body_size,
            "max_header_size",  "%d", (int)priv->max_header_size,
            "max_tx_queue",     "%d", (int)priv->max_tx_queue,
            "timeout_tx_stall", "%d", (int)priv->timeout_tx_stall,
            "url",              "%s", gobj_read_str_attr(gobj, "url"),
            NULL
        );
        if(priv->max_body_size < 0) {
            priv->max_body_size = 0;
        }
        if(priv->max_header_size < 0) {
            priv->max_header_size = 0;
        }
        if(priv->max_tx_queue < 0) {
            priv->max_tx_queue = DEFAULT_MAX_TX_QUEUE;
        }
        if(priv->timeout_tx_stall < 0) {
            priv->timeout_tx_stall = DEFAULT_TIMEOUT_TX_STALL;
        }
    }

    priv->parsing_response = create_response_parser(gobj);

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
    SET_PRIV(url,                   gobj_read_str_attr)
    SET_PRIV(timeout_inactivity,    gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout_inactivity,          gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(url,                       gobj_read_str_attr)
        check_url(gobj);
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    check_url(gobj);

    /*
     *  The bottom must be a C_TCP (it has manual start/stop!).
     *  If it's a client then start to begin the connection.
     *  If it's a server, wait to give the connection done by C_TCP_S.
     */
    hgobj bottom_gobj = gobj_bottom_gobj(gobj);
    if(!bottom_gobj) {
        /*
         *  This gobj is always client!
         */
        json_t *kw = json_pack("{s:s, s:s, s:O}",
            "cert_pem", gobj_read_str_attr(gobj, "cert_pem"),
            "url", gobj_read_str_attr(gobj, "url"),
            "crypto", gobj_read_json_attr(gobj, "crypto")
        );

        #ifdef ESP_PLATFORM
            bottom_gobj = gobj_create_pure_child(gobj_name(gobj), C_ESP_TRANSPORT, kw, gobj);
        #endif
        #ifdef __linux__
            bottom_gobj = gobj_create_pure_child(gobj_name(gobj), C_TCP, kw, gobj);
        #endif
        gobj_set_bottom_gobj(gobj, bottom_gobj);
    }

    gobj_start(bottom_gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(gobj_bottom_gobj(gobj));
    clear_timeout(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    EXEC_AND_RESET(ghttp_parser_destroy, priv->parsing_response)
}




                    /***************************
                     *      Local methods
                     ***************************/




/***************************************************************************
 *  Check url
 ***************************************************************************/
PRIVATE int check_url(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(empty_string(priv->url)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "url EMPTY",
            NULL
        );
    } else {
        char port[64];
        if(parse_url(
            gobj,
            priv->url,
            priv->schema, sizeof(priv->schema),
            priv->host, sizeof(priv->host),
            port, sizeof(port),
            0, 0,
            0, 0,
            FALSE
        )<0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "parse_http_url() FAILED",
                "url",          "%s", priv->url,
                NULL
            );
        }
        priv->port = atoi(port);
        if(!priv->port) {
            if(strcmp(priv->schema, "https") == 0 || strcmp(priv->schema, "wss") == 0) {
                priv->port = 443;
            } else if(strcmp(priv->schema, "http") == 0 || strcmp(priv->schema, "ws") == 0) {
                priv->port = 80;
            }
        }
    }

    if(empty_string(priv->host)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "host EMPTY",
            NULL
        );
        return -1;
    }
    if(!priv->port) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "port EMPTY",
            NULL
        );
        return -1;
    }

    return 0;
}

/***************************************************************************
 *  The parser of the responses of a connection
 ***************************************************************************/
PRIVATE GHTTP_PARSER *create_response_parser(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    GHTTP_PARSER *parser;
    BOOL raw_body_data = gobj_read_bool_attr(gobj, "raw_body_data");
    if(!raw_body_data) {
        parser = ghttp_parser_create(
            gobj,
            HTTP_RESPONSE,  // http_parser_type
            NULL,           // on_header_event
            NULL,           // on_body_event
            EV_ON_MESSAGE,  // on_message_event ==> publish the full message in a gbuffer
            FALSE           // TRUE: use gobj_send_event, FALSE: use gobj_publish_event
        );
    } else {
        parser = ghttp_parser_create(
            gobj,
            HTTP_RESPONSE,  // http_parser_type
            EV_ON_HEADER,   // on_header_event
            EV_ON_MESSAGE,  // on_body_event  ==> publish the partial message with original buffer pointer
            NULL,           // on_message_event
            FALSE           // TRUE: use gobj_send_event, FALSE: use gobj_publish_event
        );
    }
    if(parser) {
        parser->max_body_size = (size_t)priv->max_body_size;
        parser->max_header_size = (size_t)priv->max_header_size;
    }
    return parser;
}

/***************************************************************************
 *  The bytes a text takes once encoded
 ***************************************************************************/
PRIVATE size_t form_text_size(const char *text, size_t len)
{
    size_t size = 0;
    for(size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                c == '-' || c == '.' || c == '_' || c == '~' || c == ' ') {
            size += 1;
        } else {
            size += 3;
        }
    }
    return size;
}

/***************************************************************************
 *  Append a key or a value of a form, application/x-www-form-urlencoded:
 *  the unreserved bytes of RFC 3986 (ALPHA, DIGIT, "-", ".", "_", "~")
 *  as they are, a space as "+", every other byte (a NUL too) as %XX. So
 *  a "+", a "%", a "&" or a "=" of a value is that byte for the server,
 *  never a space, an escape, or the start of another field.
 ***************************************************************************/
PRIVATE int append_form_text(gbuffer_t *gbuf, const char *text, size_t len)
{
    static const char hex[] = "0123456789ABCDEF";
    for(size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[i];
        char enc[3];
        size_t n = 1;
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                c == '-' || c == '.' || c == '_' || c == '~') {
            enc[0] = (char)c;
        } else if(c == ' ') {
            enc[0] = '+';
        } else {
            enc[0] = '%';
            enc[1] = hex[c >> 4];
            enc[2] = hex[c & 0x0F];
            n = 3;
        }
        if(gbuffer_append(gbuf, enc, n) != n) {
            return -1;  // Error already logged
        }
    }
    return 0;
}

/***************************************************************************
 *  The text of a value of a form: a string as it is (all its bytes), a
 *  number or a boolean as its json text (made here: free *to_free).
 *  NULL for a null, an object or an array, which have no text in a form.
 ***************************************************************************/
PRIVATE const char *form_value_text(json_t *v, size_t *len, char **to_free)
{
    *to_free = NULL;
    *len = 0;
    if(json_is_string(v)) {
        *len = json_string_length(v);
        return json_string_value(v);
    }
    if(json_is_number(v) || json_is_boolean(v)) {
        *to_free = json_dumps(v, JSON_ENCODE_ANY|JSON_COMPACT);
        if(*to_free) {
            *len = strlen(*to_free);
        }
        return *to_free;
    }
    return NULL;
}

/***************************************************************************
 *  The form of a dict `data`, k1=v1&k2=v2, each key and value encoded, in
 *  a buffer of its exact size. An empty value is sent as `key=`.
 *  NULL if a value has no text (a null, an object, an array): the request
 *  cannot be built, and it is said.
 ***************************************************************************/
PRIVATE gbuffer_t *build_form(hgobj gobj, json_t *jn_data)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *key;
    json_t *v;

    if(jn_data && !json_is_object(jn_data)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "The data of a form must be a dict",
            "url",          "%s", priv->url,
            NULL
        );
        return NULL;
    }

    size_t size = 0;
    json_object_foreach(jn_data, key, v) {
        size_t len;
        char *to_free;
        const char *text = form_value_text(v, &len, &to_free);
        if(!text) {
            /*  The `url` and not a `peername`: this is an OUTBOUND client,
             *  and what an operator needs of a bad request is WHICH one */
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "A value of a form must be a string, a number or a boolean",
                "key",          "%s", key,
                "type",         "%s", json_is_null(v)? "null" : json_is_object(v)? "object" : "array",
                "url",          "%s", priv->url,
                NULL
            );
            return NULL;
        }
        size += (size? 1 : 0) + form_text_size(key, strlen(key)) + 1 + form_text_size(text, len);
        GBMEM_FREE(to_free)
    }

    gbuffer_t *gbuf = gbuffer_create(size, size + 1);
    if(!gbuf) {
        return NULL;    // Error already logged
    }
    int fields = 0;
    json_object_foreach(jn_data, key, v) {
        size_t len;
        char *to_free;
        const char *text = form_value_text(v, &len, &to_free);
        int ret = 0;
        if(fields > 0 && gbuffer_append(gbuf, "&", 1) != 1) {
            ret = -1;
        }
        if(ret == 0 && (append_form_text(gbuf, key, strlen(key)) < 0 ||
                gbuffer_append(gbuf, "=", 1) != 1 ||
                append_form_text(gbuf, text, len) < 0)) {
            ret = -1;
        }
        GBMEM_FREE(to_free)
        if(ret < 0) {
            GBUFFER_DECREF(gbuf)
            return NULL;    // Error already logged
        }
        fields++;
    }
    return gbuf;
}

/***************************************************************************
 *  The transport holds `max_tx_queue` requests and no write ended in
 *  `timeout_tx_stall` milliseconds: the only thing that says the server does
 *  not read, because the queue moves only at the end of a write, out of
 *  this turn of the loop. Asked at each request: the wait starts at the
 *  first time it finds the queue full, and any write that ends starts it
 *  again. ONE warning: after it the parser is stopped.
 ***************************************************************************/
PRIVATE BOOL server_does_not_read(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj bottom = gobj_bottom_gobj(gobj);
    if(priv->max_tx_queue <= 0 || priv->timeout_tx_stall == 0 ||
            priv->parsing_response->stopped ||
            !bottom || !gobj_has_attr(bottom, "cur_tx_queue")) {
        return FALSE;
    }
    json_int_t queued = gobj_read_integer_attr(bottom, "cur_tx_queue");
    if(queued < priv->max_tx_queue) {
        return FALSE;   // a queue that shrank is a write that ended: the wait starts again below
    }
    json_int_t written = gobj_read_integer_attr(bottom, "txBytes");
    if(priv->tx_stall_timer == 0 || written != priv->tx_stall_written) {
        priv->tx_stall_written = written;
        priv->tx_stall_timer = start_msectimer((uint64_t)priv->timeout_tx_stall);
        return FALSE;
    }
    if(!test_msectimer(priv->tx_stall_timer)) {
        return FALSE;
    }
    gobj_log_warning(gobj, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_PROTOCOL,
        "msg",              "%s", "HTTP server does not read the requests, request refused and connection dropped",
        "cur_tx_queue",     "%d", (int)queued,
        "max_tx_queue",     "%d", (int)priv->max_tx_queue,
        "timeout_tx_stall", "%d", (int)priv->timeout_tx_stall,
        "url",              "%s", priv->url,
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
            gbuffer_get(gbuf, (size_t)n);  // take out the bytes consumed
        }
    }
    return 0;
}

/***************************************************************************
 *  WARNING size for ugly json string (without indent)
 ***************************************************************************/
PRIVATE size_t kw_content_size(json_t *kw)
{
    char *sjn = json_dumps(kw, JSON_INDENT(0));
    size_t ln = sjn?strlen(sjn):0;
    GBMEM_FREE(sjn)
    ln += 2 * json_object_size(kw);
    return ln;
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
     *  Rebuild the response parser for the new connection.
     *  A plain re-init of llhttp is not exposed any more; we use the
     *  destroy+create cycle so every connection starts with a pristine
     *  state machine.
     */
    if(priv->parsing_response) {
        ghttp_parser_destroy(priv->parsing_response);
        priv->parsing_response = 0;
    }
    priv->parsing_response = create_response_parser(gobj);
    priv->tx_stall_timer = 0;

    set_timeout(priv->timer, priv->timeout_inactivity);

    return gobj_publish_event(gobj, EV_ON_OPEN, kw); // use the same kw
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    clear_timeout(priv->timer);

    /*
     *  Signal end-of-stream to llhttp.  Critical for HTTP/1.0 responses
     *  and Connection: close responses without Content-Length, where the
     *  peer close is the only message terminator: llhttp will fire
     *  on_message_complete -> EV_ON_MESSAGE right here.  Error return
     *  is non-fatal (partial message) and the subscriber sees EV_ON_CLOSE.
     */
    if(priv->parsing_response) {
        (void)ghttp_parser_finish(priv->parsing_response);
    }

    return gobj_publish_event(gobj, EV_ON_CLOSE, kw); // use the same kw
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s ⏪ %s",
             gobj_short_name(gobj),
             gobj_short_name(gobj_bottom_gobj(gobj))
        );
    }

    set_timeout(priv->timer, priv->timeout_inactivity);

    int result = parse_http_message(gobj, gbuf, priv->parsing_response);
    if (result < 0) {
        gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(server_does_not_read(gobj)) {
        // Warning already logged
        ghttp_parser_stop(priv->parsing_response);
        gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
        KW_DECREF(kw)
        return -1;
    }

    const char *method = kw_get_str(gobj, kw, "method", "GET", 0);
    char *resource = gbmem_strdup(kw_get_str(gobj, kw, "resource", "/", 0));
    if(!resource) {
        // Error already logged
        KW_DECREF(kw)
        return -1;
    }
    const char *query = kw_get_str(gobj, kw, "query", "", 0);
    json_t *jn_headers_ = kw_get_dict(gobj, kw, "headers", 0, 0);
    /*
     *  `data` may be any JSON value, not only an object: a JSON array
     *  body is required by some REST APIs (e.g. Keycloak's
     *  execute-actions-email).  kw_get_dict_value returns the value
     *  whatever its type; the application/json branch below json_dumps()
     *  it verbatim.  The x-www-form-urlencoded branch only iterates it
     *  when it is an object (guarded there).
     */
    json_t *jn_data_ = kw_get_dict_value(gobj, kw, "data", 0, 0);
    const char *http_version = "1.1";
    size_t content_length = 0;
    char *content = 0;
    const char *key; json_t *v;

    json_t *jn_headers = json_object();
    json_object_set_new(jn_headers,
        "User-Agent", json_sprintf("yuneta-%s", YUNETA_VERSION)
    );
    if(jn_data_) {
        json_object_set_new(
            jn_headers, "Content-Type", json_string("application/json; charset=utf-8")
        );
    }
    json_object_set_new(jn_headers, "Connection", json_string("keep-alive"));
    json_object_set_new(jn_headers, "Accept", json_string("*/*"));

    if(priv->port == 80 || priv->port == 443) {
        json_object_set_new(jn_headers, "Host", json_string(priv->host));
    } else {
        json_object_set_new(jn_headers, "Host", json_sprintf("%s:%d", priv->host, priv->port));
    }

    if(jn_headers_) {
        json_object_update(jn_headers, jn_headers_);
    }

    const char *content_type = kw_get_str(gobj, jn_headers, "Content-Type", "", 0);
    BOOL set_form_urlencoded = strcmp(content_type, "application/x-www-form-urlencoded")==0?1:0;
    if(set_form_urlencoded) {
        if(!empty_string(query)) {
            content = gbmem_strdup(query);
        } else {
            gbuffer_t *gbuf_form = build_form(gobj, jn_data_);
            if(gbuf_form) {
                content = gbmem_strndup(gbuffer_cur_rd_pointer(gbuf_form), gbuffer_leftbytes(gbuf_form));
                GBUFFER_DECREF(gbuf_form)
            }
        }
        if(!content) {
            // Error already logged
            GBMEM_FREE(resource)
            JSON_DECREF(jn_headers)
            KW_DECREF(kw)
            return -1;
        }
        content_length = strlen(content);
        if (strcasecmp(method, "GET")==0) {
            // parameters must be in resource: room for the '?' and the NUL
            if(content_length > 0) {
                size_t l = strlen(resource) + 1 + content_length + 1;
                char *new_resource = GBMEM_MALLOC(l);
                if(!new_resource) {
                    // Error already logged
                    GBMEM_FREE(resource)
                    JSON_DECREF(jn_headers)
                    GBMEM_FREE(content)
                    KW_DECREF(kw)
                    return -1;
                }
                snprintf(new_resource, l, "%s?%s", resource, content);
                GBMEM_FREE(resource)
                resource = new_resource;
            }
            GBMEM_FREE(content)
            content_length = 0;
        }

    } else {
        if(jn_data_) {
            content = json_dumps(jn_data_, JSON_COMPACT|JSON_INDENT(0)|JSON_ENCODE_ANY);
            if(!content) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "json_dumps() of the data FAILED",
                    "url",          "%s", priv->url,
                    NULL
                );
                GBMEM_FREE(resource)
                JSON_DECREF(jn_headers)
                KW_DECREF(kw)
                return -1;
            }
            content_length = strlen(content);
        }
    }

    size_t len = strlen(method) + strlen(resource) + kw_content_size(jn_headers) + content_length + 256;
    gbuffer_t *gbuf = gbuffer_create(len, len);
    if(!gbuf) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "gbuf_create() FAILED",
            NULL
        );
        GBMEM_FREE(resource)
        JSON_DECREF(jn_headers)
        GBMEM_FREE(content)
        KW_DECREF(kw)
        return -1;
    }
    gbuffer_printf(gbuf, "%s %s HTTP/%s\r\n", method, resource, http_version);
    json_object_foreach(jn_headers, key, v) {
        gbuffer_printf(gbuf, "%s:%s\r\n", key, json_string_value(v));
    }

    if(content) {
        gbuffer_printf(gbuf, "Content-Length: %d\r\n", (int)content_length);
    }
    gbuffer_printf(gbuf, "\r\n");
    if(content && content_length > 0) {
        gbuffer_append(gbuf, content, content_length);
    }

    if(gobj_trace_level(gobj) & TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s ⏩ %s",
             gobj_short_name(gobj),
             gobj_short_name(gobj_bottom_gobj(gobj))
        );
    }

    GBMEM_FREE(resource)
    JSON_DECREF(jn_headers)
    GBMEM_FREE(content)
    KW_DECREF(kw)

    json_t *kw_response = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    return gobj_send_event(gobj_bottom_gobj(gobj), EV_TX_DATA, kw_response, gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    ghttp_parser_stop(priv->parsing_response);
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_inactivity(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    ghttp_parser_stop(priv->parsing_response);
    /*
     *  EV_DROP, the interned symbol — NOT the string literal "EV_DROP".
     *  Events are matched by POINTER identity (_find_event_action), so a
     *  literal is a different address that never matches the bottom
     *  gobj's state table: the drop is silently refused with
     *  "Event NOT DEFINED in state", naming an event the table clearly
     *  declares, and the connection is never torn down.
     */
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
    KW_DECREF(kw);
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
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_PROT_HTTP_CL);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

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
        {EV_CONNECTED,          ac_connected,           ST_CONNECTED},
        {EV_STOPPED,            ac_stopped,             0},
        {0,0,0}
    };
    ev_action_t st_connected[] = {
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_SEND_MESSAGE,       ac_send_message,        0},
        {EV_TIMEOUT,            ac_timeout_inactivity,  0},
        {EV_TX_READY,           0,                      0},
        {EV_DISCONNECTED,       ac_disconnected,        ST_DISCONNECTED},
        {EV_DROP,               ac_drop,                0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,   st_disconnected},
        {ST_CONNECTED,      st_connected},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_MESSAGE,     EVF_OUTPUT_EVENT},
        {EV_ON_HEADER,      EVF_OUTPUT_EVENT},
        {EV_RX_DATA,        0},
        {EV_ON_OPEN,        EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,       EVF_OUTPUT_EVENT},

        {EV_TIMEOUT,        0},
        {EV_CONNECTED,      0},
        {EV_DISCONNECTED,   0},
        {EV_STOPPED,        0},
        {EV_TX_READY,       0},
        {EV_SEND_MESSAGE,   0},
        {EV_DROP,           0},

        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,
        0   // gcflag_t
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_prot_http_cl(void)
{
    return create_gclass(C_PROT_HTTP_CL);
}
