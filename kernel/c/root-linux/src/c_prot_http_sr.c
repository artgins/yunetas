/***********************************************************************
 *          C_PROT_HTTP_SRV.C
 *          Prot_http_srv GClass.
 *
 *          Protocol http as server
 *
 *          Copyright (c) 2018-2021 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <helpers.h>
#include <kwid.h>
#include <ghttp_parser.h>
#include <msg_ievent.h>
#include "c_timer.h"
#include "c_prot_http_sr.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag------------default---------description---------- */
SDATA (DTP_INTEGER,     "timeout_inactivity",SDF_WR,        "300",          "Timeout inactivity, in seconds"),
SDATA (DTP_BOOLEAN,     "connected",        SDF_RD|SDF_STATS,0,             "Connection state. Important filter!"),
SDATA (DTP_STRING,   "on_open_event_name",SDF_RD,        "EV_ON_OPEN",   "Must be empty if you don't want receive this event"),
SDATA (DTP_STRING,   "on_close_event_name",SDF_RD,       "EV_ON_CLOSE",  "Must be empty if you don't want receive this event"),
SDATA (DTP_STRING,   "on_message_event_name",SDF_RD,     "EV_ON_MESSAGE","Must be empty if you don't want receive this event"),
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
    const char *on_open_event_name;
    const char *on_close_event_name;
    const char *on_message_event_name;
} PRIVATE_DATA;

PRIVATE hgclass __gclass__ = 0;




            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create("", C_TIMER, 0, gobj);
    priv->parsing_request = ghttp_parser_create(
        gobj,
        HTTP_REQUEST,
        "",
        "",
        "EV_ON_MESSAGE",
        FALSE
    );

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber)
        subscriber = gobj_parent(gobj);
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(on_open_event_name,    gobj_read_str_attr)
    SET_PRIV(on_close_event_name,   gobj_read_str_attr)
    SET_PRIV(on_message_event_name, gobj_read_str_attr)
    SET_PRIV(timeout_inactivity,    gobj_read_integer_attr)
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
    gobj_start_childs(gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    gobj_stop_childs(gobj);

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
 *  Parse a http message
 *  Return -1 if error: you must close the socket.
 *  Return 0 if no new request.
 *  Return 1 if new request available in `request`.
 ***************************************************************************/
PRIVATE int parse_message(hgobj gobj, gbuffer_t *gbuf, GHTTP_PARSER *parser)
{
    size_t ln;
    while((ln=gbuffer_leftbytes(gbuf))>0) {
        char *bf = gbuffer_cur_rd_pointer(gbuf);
        int n = ghttp_parser_received(parser, bf, ln);
        if (n == -1) {
            // Some error in parsing
            ghttp_parser_reset(parser);
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
PRIVATE int ac_connected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_bool_attr(gobj, "connected", TRUE);

    ghttp_parser_reset(priv->parsing_request);

    if(!empty_string(priv->on_open_event_name)) {
        gobj_publish_event(gobj, priv->on_open_event_name, 0);
    }

    set_timeout(priv->timer, priv->timeout_inactivity*1000);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    if(gobj_is_volatil(src)) {
        gobj_set_bottom_gobj(gobj, 0);
    }
    gobj_write_bool_attr(gobj, "connected", FALSE);

    if(!empty_string(priv->on_close_event_name)) {
        gobj_publish_event(gobj, priv->on_close_event_name, 0);
    }

    KW_DECREF(kw)
    return 0;
}

/********************************************************************
 *
 ********************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);

    if(gobj_trace_level(gobj) & TRAFFIC) {
        //log_debug_gbuf(LOG_DUMP_INPUT, gbuf, "%s", gobj_short_name(gobj));
        gobj_trace_dump_gbuf(gobj, gbuf, "%s", gobj_short_name(gobj));
    }

    set_timeout(priv->timer, priv->timeout_inactivity*1000);

    int result = parse_message(gobj, gbuf, priv->parsing_request);
    if (result < 0) {
        gobj_send_event(gobj_bottom_gobj(gobj), "EV_DROP", 0, gobj);
    }

    KW_DECREF(kw)
    return 0;
}

/********************************************************************
 *
 ********************************************************************/
PRIVATE int ac_send_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gbuffer_t *gbuf = 0;

    if(kw_has_key(kw, "body")) {
        // New method
        const char *code = kw_get_str(gobj, kw, "code", "200 OK", 0);
        const char *headers = kw_get_str(gobj, kw, "headers", "", 0);
        json_t *jn_body = kw_duplicate(gobj, kw_get_dict_value(gobj, kw, "body", json_object(), KW_REQUIRED));
        char *resp = json2uglystr(jn_body);
        int len = strlen(resp) + strlen(headers);
        kw_decref(jn_body);

        gbuf = gbuffer_create(256+len, 256+len);
        gbuffer_printf(gbuf,
            "HTTP/1.1 %s\r\n"
            "%s"
            "Content-Type: application/json; charset=utf-8\r\n"
            "Content-Length: %d\r\n\r\n",
            code,
            headers,
            len
        );
        gbuffer_append(gbuf, resp, len);
        GBMEM_FREE(resp)
    } else  {
        // Old method
        json_t *kw_resp = msg_iev_pure_clone(
            kw  // NO owned
        );

        char *resp = json2uglystr(kw_resp);
        int len = (int)strlen(resp);
        kw_decref(kw_resp);

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
        "gbuffer", (json_int_t)(size_t)gbuf
    );
    KW_DECREF(kw)
    return gobj_send_event(gobj_bottom_gobj(gobj), "EV_TX_DATA", kw_response, gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_send_event(gobj_bottom_gobj(gobj), "EV_DROP", 0, gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_inactivity(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_send_event(gobj_bottom_gobj(gobj), "EV_DROP", 0, gobj);
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Child stopped
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
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
GOBJ_DEFINE_STATE(ST_DISCONNECTED);
GOBJ_DEFINE_STATE(ST_CONNECTED);

/*------------------------*
 *      Events
 *------------------------*/
//GOBJ_DEFINE_EVENT();

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
        {EV_CONNECTED,        ac_connected,               ST_CONNECTED},
        {EV_DISCONNECTED,     ac_disconnected,            0},
        {EV_STOPPED,          ac_stopped,                 0},
        {0,0,0}
    };
    ev_action_t st_connected[] = {
        {EV_RX_DATA,          ac_rx_data,                 0},
        {EV_SEND_MESSAGE,     ac_send_message,            0},
        {EV_TIMEOUT,          ac_timeout_inactivity,      0},
        {EV_TX_READY,         0,                          0},
        {EV_DROP,             ac_drop,                    0},
        {EV_DISCONNECTED,     ac_disconnected,            ST_DISCONNECTED},
        {EV_STOPPED,          ac_stopped,                 0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_DISCONNECTED,       st_disconnected},
        {ST_CONNECTED,          st_connected},
        {0, 0}
    };

    event_type_t event_types[] = { // HACK System gclass, not public events
        {EV_RX_DATA,            0}, // TODO is necessary to define the internal or input events?
        {EV_SEND_MESSAGE,       EVF_PUBLIC_EVENT},
        {EV_RX_DATA,            0},
        {EV_SEND_MESSAGE,       EVF_PUBLIC_EVENT},
        {EV_CONNECTED,          0},
        {EV_DISCONNECTED,       0},
        {EV_DROP,               EVF_PUBLIC_EVENT},
        {EV_TIMEOUT,            0},
        {EV_TX_READY,           0},
        {EV_STOPPED,            0},
        {EV_ON_OPEN,            EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,           EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT},
        {EV_ON_MESSAGE,         EVF_PUBLIC_EVENT|EVF_OUTPUT_EVENT},
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
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        0,  // s_user_trace_level
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
