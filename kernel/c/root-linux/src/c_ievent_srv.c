/***********************************************************************
 *          C_IEVENT_SRV.C
 *          Ievent_srv GClass.
 *
 *          Inter-event server side
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>
#include <stdio.h>

#include <kwid.h>
#include <helpers.h>

#include "c_timer.h"
#include "msg_ievent.h"
#include "c_ievent_srv.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int send_static_iev(
    hgobj gobj,
    const char *event,
    json_t *kw, // owned and serialized,
    hgobj src
);
PRIVATE json_t *build_ievent_request(
    hgobj gobj,
    const char *src_service,
    const char *dst_service
);


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/


/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
SDATA (DTP_STRING,      "client_yuno_role",     SDF_RD, 0, "yuno role of connected client"),
SDATA (DTP_STRING,      "client_yuno_name",     SDF_RD, 0, "yuno name of connected client"),
SDATA (DTP_STRING,      "client_yuno_service",  SDF_RD, 0, "yuno service of connected client"),

SDATA (DTP_STRING,      "this_service",         SDF_RD, 0, "dst_service at identity_card"),
SDATA (DTP_POINTER,     "gobj_service",         0,      0, "gobj of identity_card dst_service"),

SDATA (DTP_BOOLEAN,     "authenticated",        SDF_RD, 0, "True if entry was authenticated"),

// HACK set by c_authz
SDATA (DTP_JSON,        "jwt_payload",          SDF_RD, 0, "JWT payload (decoded user data) of authenticated user, WARNING set by c_authz"),
SDATA (DTP_STRING,      "__username__",         SDF_RD, "", "Username, WARNING set by c_authz"),
SDATA (DTP_JSON,        "identity_card",        SDF_RD, "", "Identity Card of clisrv"),

// TODO available_services for this gate
// TODO available_services in this gate
// For now, only one service is available, the selected in identity_card exchange.

SDATA (DTP_INTEGER,     "timeout_idgot",        SDF_RD, "5000", "timeout waiting Identity Card"),

SDATA (DTP_POINTER,     "user_data",            0, 0, "user data"),
SDATA (DTP_POINTER,     "user_data2",           0, 0, "more user data"),
SDATA (DTP_POINTER,     "subscriber",           0, 0, "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_IEVENTS       = 0x0001,
    TRACE_IEVENTS2      = 0x0002,
    TRACE_IDENTITY_CARD = 0x0004,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"ievents",        "Trace inter-events with metadata of kw"},
{"ievents2",       "Trace inter-events with full kw"},
{"identity-card",  "Trace identity_card messages"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int32_t timeout; //TODO
    BOOL inform_on_close;

    const char *client_yuno_name;
    const char *client_yuno_role;
    const char *client_yuno_service;
    const char *this_service;
    hgobj gobj_service;
    hgobj subscriber;

    hgobj timer;
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

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    /*
     *  CHILD/SERVER subscription model
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
    SET_PRIV(client_yuno_name,          gobj_read_str_attr)
    SET_PRIV(client_yuno_role,          gobj_read_str_attr)
    SET_PRIV(client_yuno_service,       gobj_read_str_attr)
    SET_PRIV(this_service,              gobj_read_str_attr)
    SET_PRIV(subscriber,                gobj_read_pointer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(client_yuno_name,        gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(client_yuno_role,      gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(client_yuno_service,   gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(this_service,          gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(gobj_service,          gobj_read_pointer_attr)
    ELIF_EQ_SET_PRIV(subscriber,            gobj_read_pointer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
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
    return 0;
}

/***************************************************************************
 *      Framework Method stats
 ***************************************************************************/
PRIVATE json_t *mt_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src)
{
    if(gobj_current_state(gobj)!=ST_SESSION) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Not in session",
            "stats",        "%s", stats?stats:"",
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    if(!kw) {
        kw = json_object();
    }

    /*
     *      __REQUEST__ __MESSAGE__
     */
    json_t *jn_ievent_id = build_ievent_request(
        gobj,
        gobj_name(src),
        kw_get_str(gobj, kw, "__service__", 0, 0)
    );
    json_object_del(kw, "__service__");

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        "__stats__",
        json_pack("{s:s, s:O}",   // owned
            "stats", stats,
            "kw", kw
        )
    );

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        IEVENT_MESSAGE_AREA_ID,
        jn_ievent_id   // owned
    );

    json_object_set_new(kw, "__stats__", json_string(stats)); // TODO deprecated, used by v6
    msg_iev_set_msg_type(gobj, kw, "__stats__");

    send_static_iev(gobj, EV_MT_STATS, kw, src);

    return NULL;   // return NULL on asynchronous response.
}

/***************************************************************************
 *      Framework Method command
 ***************************************************************************/
PRIVATE json_t *mt_command(hgobj gobj, const char *command, json_t *kw, hgobj src)
{
    if(gobj_current_state(gobj) != ST_SESSION) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Not in session",
            "command",      "%s", command,
            NULL
        );
        KW_DECREF(kw)
        return 0;
    }

    if(!kw) {
        kw = json_object();
    }

    /*
     *      __REQUEST__ __MESSAGE__
     */
    json_t *jn_ievent_id = build_ievent_request(
        gobj,
        gobj_name(src),
        kw_get_str(gobj, kw, "__service__", 0, 0)
    );
    json_object_del(kw, "__service__");

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        "__command__",
        json_pack("{s:s, s:O}",   // owned
            "command", command,
            "kw", kw
        )
    );

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        IEVENT_MESSAGE_AREA_ID,
        jn_ievent_id   // owned
    );

    json_object_set_new(kw, "__command__", json_string(command)); // TODO deprecated, used by v6
    msg_iev_set_msg_type(gobj, kw, "__command__");

    send_static_iev(gobj, EV_MT_COMMAND, kw, src);

    return NULL;   // return NULL on asynchronous response.
}

/***************************************************************************
 *      Framework Method inject_event
 ***************************************************************************/
PRIVATE int mt_inject_event(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    if(gobj_current_state(gobj) != ST_SESSION) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Not in session",
            "event",        "%s", event,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }
    if(!kw) {
        kw = json_object();
    }

    /*
     *      __MESSAGE__
     */
    json_t *jn_request = msg_iev_get_stack(gobj, kw, IEVENT_MESSAGE_AREA_ID, false);
    if(!jn_request) {
        /*
         *  Put the ievent if it doesn't come with it,
         *  if it does come with it, it's because it will be a response
         */
        json_t *jn_ievent_id = build_ievent_request(
            gobj,
            gobj_name(src),
            kw_get_str(gobj, kw, "__service__", 0, 0)
        );
        json_object_del(kw, "__service__");

        msg_iev_push_stack(
            gobj,
            kw,         // not owned
            IEVENT_MESSAGE_AREA_ID,
            jn_ievent_id   // owned
        );

        msg_iev_push_stack(
            gobj,
            kw,         // not owned
            "__message__",
            json_string(event)   // owned
        );
    }

    msg_iev_set_msg_type(gobj, kw, "__message__");

    return send_static_iev(gobj, event, kw, src);
}




            /***************************
             *      Commands
             ***************************/




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  __MESSAGE__
 ***************************************************************************/
PRIVATE json_t *build_ievent_request(
    hgobj gobj,
    const char *src_service,
    const char *dst_service
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(empty_string(dst_service)) {
        dst_service = priv->client_yuno_service;
    }
    json_t *jn_ievent_chain = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
        "dst_yuno", priv->client_yuno_name,
        "dst_role", priv->client_yuno_role,
        "dst_service", dst_service,

        "src_yuno", gobj_yuno_name(),
        "src_role", gobj_yuno_role(),
        "src_service", src_service,

        "host", get_hostname()
    );
    return jn_ievent_chain;
}

/***************************************************************************
 *  Get bottom level gobj
 ***************************************************************************/
PRIVATE hgobj get_bottom_gobj(hgobj gobj)
{
    hgobj bottom_gobj = gobj_bottom_gobj(gobj);
    if(bottom_gobj) {
        return bottom_gobj;
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "NOT below gobj",
            NULL
        );
    }
    return 0;
}

/***************************************************************************
 *  Drop
 ***************************************************************************/
PRIVATE int drop(hgobj gobj)
{
    hgobj below_gobj = get_bottom_gobj(gobj);
    if(!below_gobj) {
        // Error already logged
        return -1;
    }

    gobj_send_event(below_gobj, EV_DROP, 0, gobj);
    return 0;
}

/***************************************************************************
 *  Route must be connected
 ***************************************************************************/
PRIVATE int send_static_iev(
    hgobj gobj,
    const char *event,
    json_t *kw, // owned and serialized,
    hgobj src
)
{
    hgobj below_gobj = get_bottom_gobj(gobj);
    if(!below_gobj) {
        // Error already logged
        return -1;
    }

    uint32_t trace_level = gobj_trace_level(gobj);
    if(trace_level) {
        char prefix[256];
        snprintf(prefix, sizeof(prefix),
            "INTRA-EVENT(%s^%s) %s ==> %s",
            gobj_yuno_role(),
            gobj_yuno_name(),
            gobj_short_name(src),
            gobj_short_name(below_gobj)
        );
        if((trace_level & TRACE_IEVENTS2)) {
            trace_inter_event2(gobj, prefix, event, kw);
        } else if((trace_level & TRACE_IEVENTS)) {
            trace_inter_event(gobj, prefix, event, kw);
        } else if((trace_level & TRACE_IDENTITY_CARD)) {
            if(event == EV_IDENTITY_CARD ||
                event == EV_IDENTITY_CARD_ACK ||
                event == EV_PLAY_YUNO ||
                event == EV_PLAY_YUNO_ACK ||
                event == EV_PAUSE_YUNO ||
                event == EV_PAUSE_YUNO_ACK
               ) {
                trace_inter_event2(gobj, prefix, event, kw);
            }
        }
    }

    gbuffer_t *gbuf = iev_create_to_gbuffer(
        gobj,
        event,
        kw     // own
    );
    if(!gbuf) {
        // error already logged
        return -1;
    }
    json_t *kw_send = json_pack("{s:I}",
        "gbuffer", (json_int_t)(size_t)gbuf
    );
    return gobj_send_event(below_gobj,
        EV_SEND_MESSAGE,
        kw_send,
        gobj
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Route (channel) open.
     *  Wait Identity card
     */
    set_timeout(priv->timer, gobj_read_integer_attr(gobj, "timeout_idgot"));

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*---------------------------------------*
     *  Clear timeout
     *---------------------------------------*/
    clear_timeout(priv->timer);

    /*
     *  Delete external subscriptions
     */
    json_t *kw2match = json_object();
    kw_set_dict_value(
        gobj,
        kw2match,
        "__local__`__subscription_reference__",
        json_integer((json_int_t)(size_t)gobj)
    );

    json_t *dl_s = gobj_find_subscribings(gobj, 0, kw2match, 0);
    gobj_unsubscribe_list(dl_s, false);

    /*
     *  Route (channel) close.
     */
    if(priv->inform_on_close) {
        priv->inform_on_close = false;
        json_t *kw_on_close = json_pack("{s:s, s:s, s:s, s:s}",
            "client_yuno_name", gobj_read_str_attr(gobj, "client_yuno_name"),
            "client_yuno_role", gobj_read_str_attr(gobj, "client_yuno_role"),
            "client_yuno_service", gobj_read_str_attr(gobj, "client_yuno_service"),
            "__username__", gobj_read_str_attr(gobj, "__username__")
        );
        json_object_update_missing(kw_on_close, kw);

        /*
         *  CHILD/SERVER subscription model
         */
        gobj_publish_event(gobj, EV_ON_CLOSE, kw_on_close);
    }

    /*----------------------------*
     *      Reset "client data"
     *----------------------------*/
    gobj_write_bool_attr(gobj, "authenticated", false);
    gobj_write_str_attr(gobj, "client_yuno_role", "");
    gobj_write_str_attr(gobj, "client_yuno_service", "");
    gobj_write_str_attr(gobj, "client_yuno_name", "");
    gobj_write_str_attr(gobj, "this_service", "");
    gobj_write_pointer_attr(gobj, "gobj_service", 0);
    gobj_write_str_attr(gobj, "__username__", "");
    gobj_write_json_attr(gobj, "identity_card", 0);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_wait_idGot(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
        "msg",          "%s", "Timeout waiting identity card",
        NULL
    );
    drop(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  remote ask
 *  Somebody wants our services.
 ***************************************************************************/
PRIVATE int ac_identity_card(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *      __MESSAGE__
     */
    /*
     *  Final point of the request
     */
    json_t *jn_request = msg_iev_get_stack(gobj, kw, IEVENT_MESSAGE_AREA_ID, true);
    if(!jn_request) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "no ievent_gate_stack",
            NULL
        );
        gobj_trace_json(gobj, kw, "no ievent_gate_stack");
        KW_DECREF(kw)
        return -1;
    }
    const char *iev_dst_yuno = kw_get_str(gobj, jn_request, "dst_yuno", "", 0);
    const char *iev_dst_role = kw_get_str(gobj, jn_request, "dst_role", "", 0);
    const char *iev_dst_service = kw_get_str(gobj, jn_request, "dst_service", "", 0);
    const char *iev_src_yuno = kw_get_str(gobj, jn_request, "src_yuno", "", 0);
    const char *iev_src_role = kw_get_str(gobj, jn_request, "src_role", "", 0);
    const char *iev_src_service = kw_get_str(gobj, jn_request, "src_service", "", 0);

    /*---------------------------------------*
     *  Clear timeout
     *---------------------------------------*/
    clear_timeout(priv->timer);

    /*------------------------------------*
     *  Analyze
     *------------------------------------*/

    /*------------------------------------*
     *  Match wanted yuno role. Required.
     *------------------------------------*/
    if(strcasecmp(iev_dst_role, gobj_yuno_role())!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "dst_role NOT MATCH",
            "my_role",      "%s", gobj_yuno_role(),
            "dst_role",     "%s", iev_dst_role,
            NULL
        );
        gobj_trace_json(gobj, kw, "dst_role NOT MATCH");
        KW_DECREF(kw)
        return -1;
    }

    /*--------------------------------*
     *  Match wanted yuno name
     *--------------------------------*/
    if(!empty_string(iev_dst_yuno)) {
        if(strcasecmp(iev_dst_yuno, gobj_yuno_name())!=0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
                "msg",          "%s", "dst_yuno NOT MATCH",
                "my_yuno",      "%s", gobj_yuno_name(),
                "dst_yuno",     "%s", iev_dst_yuno,
                NULL
            );
            gobj_trace_json(gobj, kw, "dst_yuno NOT MATCH");
            KW_DECREF(kw)
            return -1;
        }
    }

    /*------------------------------------*
     *  Save client yuno role. Required.
     *------------------------------------*/
    if(empty_string(iev_src_role)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "GOT identity card without yuno role",
            NULL
        );
        gobj_trace_json(gobj, kw, "GOT identity card without yuno role");
        KW_DECREF(kw)
        return -1;
    }

    /*---------------------------------------*
     *  Save client yuno service. Required.
     *---------------------------------------*/
    if(empty_string(iev_src_service)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "GOT identity card without yuno service",
            NULL
        );
        gobj_trace_json(gobj, kw, "GOT identity card without yuno service");
        KW_DECREF(kw)
        return -1;
    }

    /*-----------------------------------*
     *  Find wanted service. Required.
     *-----------------------------------*/
    hgobj gobj_service = gobj_find_service(iev_dst_service, false);
    if (!gobj_service) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "dst_srv NOT FOUND in this yuno",
            "dst_service",  "%s", iev_dst_service,
            NULL
        );
        gobj_trace_json(gobj, kw, "dst_srv NOT FOUND in this yuno");
        KW_DECREF(kw)
        return -1;
    }

    /*-------------------------*
     *  Do authentication
     *-------------------------*/
    /*
     *  WARNING if not a localhost connection the authentication must be required!
     *  See mt_authenticate of c_authz.c
     */
    KW_INCREF(kw)
    json_t *jn_resp = gobj_authenticate(gobj_service, kw, gobj);
    if(kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED|KW_CREATE)<0) {
        const char *comment = kw_get_str(gobj, jn_resp, "comment", "", 0);
        // TODO sacalo: const char *remote_addr = gobj_read_str_attr(get_bottom_gobj(src), "remote-addr");
        // TODO y en el cliente mete la ip de origen
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL_ERROR,
            "msg",          "%s", "Authentication rejected",
            "cause",        "%s", comment,
            "detail",       "%j", jn_resp,
            //"remote-addr",  "%s", remote_addr?remote_addr:"",
            "yuno_role",    "%s", kw_get_str(gobj, kw, "yuno_role", "", 0),
            "yuno_id",      "%s", kw_get_str(gobj, kw, "yuno_id", "", 0),
            "yuno_name",    "%s", kw_get_str(gobj, kw, "yuno_name", "", 0),
            "yuno_tag",     "%s", kw_get_str(gobj, kw, "yuno_tag", "", 0),
            "yuno_version", "%s", kw_get_str(gobj, kw, "yuno_version", "", 0),
            "src_yuno",     "%s", iev_src_yuno,
            "src_role",     "%s", iev_src_role,
            "src_service",  "%s", iev_src_service,
            NULL
        );

        /*
         *      __ANSWER__ __MESSAGE__
         */
        JSON_INCREF(kw)
        json_t *kw_answer = msg_iev_set_back_metadata(
            gobj,
            kw,
            jn_resp,
            true
        );

        // TODO why don't send a mark in the message, the when transmit is done, drop the connection?
        //      so we avoid create a timer with each ievent_srv (could it be millions)
        send_static_iev(
            gobj,
            EV_IDENTITY_CARD_ACK,
            kw_answer,      // own
            src
        );

        set_timeout(priv->timer, gobj_read_integer_attr(gobj, "timeout_idgot"));

        KW_DECREF(kw)
        return 0; // Don't return -1, don't drop connection, let send negative ack. Drop by timeout.
    }

    /*------------------------*
     *  Gate authenticated
     *------------------------*/
    /*----------------------------------------------*
     *      Save "client data"
     *      Set connection name if name is empty
     *----------------------------------------------*/
    gobj_write_bool_attr(gobj, "authenticated", true);
    gobj_write_str_attr(gobj, "client_yuno_role", iev_src_role);
    gobj_write_str_attr(gobj, "client_yuno_service", iev_src_service);
    if(empty_string(iev_src_yuno)) {
        const char *peername = gobj_read_str_attr(src, "peername");
        const char *sockname = gobj_read_str_attr(src, "sockname");
        // anonymous yuno name
        char temp[80];
        snprintf(temp, sizeof(temp), "^%s-%s", peername, sockname);
        gobj_write_str_attr(gobj, "client_yuno_name", temp);
    } else {
        gobj_write_str_attr(gobj, "client_yuno_name", iev_src_yuno);
    }

    gobj_write_str_attr(gobj, "this_service", iev_dst_service);
    gobj_write_pointer_attr(gobj, "gobj_service", gobj_service);

    // HACK comment next sentence, already set by gobj_authenticate(), 24-Oct-2023
    // gobj_write_str_attr(gobj, "__username__", kw_get_str(jn_resp, "username", "", KW_REQUIRED));

    // json_object_set(kw, "jwt", kw_get_dict_value(jn_resp, "jwt_payload", json_null(), KW_REQUIRED)); // HACK delete original jwt

    json_object_set(kw, "jwt", gobj_read_json_attr(gobj, "jwt_payload")); // HACK delete original jwt
    gobj_write_json_attr(gobj, "identity_card", kw);

    /*----------------------------------------------*
     *  Change to state SESSION,
     *  send answer and inform.
     *----------------------------------------------*/
    gobj_change_state(gobj, ST_SESSION);

    /*
     *      __ANSWER__ __MESSAGE__
     */
    JSON_INCREF(kw)
    json_t *kw_answer = msg_iev_set_back_metadata(
        gobj,
        kw,
        jn_resp,
        true
    );

    send_static_iev(
        gobj,
        EV_IDENTITY_CARD_ACK,
        kw_answer,      // own
        src
    );

    /*-----------------------------------------------------------*
     *  Publish the new client
     *-----------------------------------------------------------*/
    if(!priv->inform_on_close) {
        priv->inform_on_close = true;
        json_t *kw_on_open = json_pack("{s:s, s:s, s:s, s:O}",
            "client_yuno_name", gobj_read_str_attr(gobj, "client_yuno_name"),
            "client_yuno_role", gobj_read_str_attr(gobj, "client_yuno_role"),
            "client_yuno_service", gobj_read_str_attr(gobj, "client_yuno_service"),
            "identity_card", kw // REQUIRED for controlcenter, agent!!
        );
        kw_set_subdict_value(
            gobj,
            kw_on_open,
            "__temp__", "__username__",
            json_string(gobj_read_str_attr(gobj, "__username__"))
        );

        /*
         *  CHILD/SERVER subscription model
         */
        gobj_publish_event(gobj, EV_ON_OPEN, kw_on_open);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, false);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gbuffer NULL, expected gbuf with inter-event",
            NULL
        );
        drop(gobj);
        KW_DECREF(kw)
        return -1;
    }

    /*---------------------------------------*
     *  Create inter_event from gbuf
     *---------------------------------------*/
    gbuffer_incref(gbuf);

    gobj_event_t iev_event;
    json_t *iev_kw = iev_create_from_gbuffer(gobj, &iev_event, gbuf, true);
    if(!iev_kw) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "iev_create_from_gbuffer() FAILED",
            NULL
        );
        drop(gobj);
        KW_DECREF(kw)
        return -1;
    }
    if(empty_string(iev_event)) {
        // Error already logged
        drop(gobj);
        KW_DECREF(iev_kw)
        KW_DECREF(kw)
        return -1;
    }

    /*---------------------------------------*
     *          trace inter_event
     *---------------------------------------*/
    uint32_t trace_level = gobj_trace_level(gobj);
    if(trace_level) {
        char prefix[256];
        snprintf(prefix, sizeof(prefix),
            "INTRA-EVENT(%s^%s) %s <== %s",
            gobj_yuno_role(),
            gobj_yuno_name(),
            gobj_short_name(gobj),
            gobj_short_name(src)
        );
        if((trace_level & TRACE_IEVENTS2)) {
            trace_inter_event2(gobj, prefix, iev_event, iev_kw);
        } else if((trace_level & TRACE_IEVENTS)) {
            trace_inter_event(gobj, prefix, iev_event, iev_kw);
        } else if((trace_level & TRACE_IDENTITY_CARD)) {
            if(iev_event == EV_IDENTITY_CARD ||
                iev_event == EV_IDENTITY_CARD_ACK ||
                iev_event == EV_PLAY_YUNO ||
                iev_event == EV_PLAY_YUNO_ACK ||
                iev_event == EV_PAUSE_YUNO ||
                iev_event == EV_PAUSE_YUNO_ACK
               ) {
                trace_inter_event2(gobj, prefix, iev_event, iev_kw);
            }
        }
    }

    /*-----------------------------------------*
     *  If state is not SESSION send self.
     *  Mainly process EV_IDENTITY_CARD_ACK
     *-----------------------------------------*/
    if(gobj_current_state(gobj) != ST_SESSION) {
        int ret = -1;
        if(gobj_has_event(gobj, iev_event, EVF_PUBLIC_EVENT)) {
            kw_incref(iev_kw);
            ret = gobj_send_event(gobj, iev_event, iev_kw, gobj);
            if(ret==0) {
                KW_DECREF(iev_kw)
                KW_DECREF(kw)
                return 0;
            }
        }
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "event UNKNOWN in not-session state",
            "event",        "%s", iev_event,
            NULL
        );
        drop(gobj);
        KW_DECREF(iev_kw)
        KW_DECREF(kw)
        return ret;
    }

    /*------------------------------------*
     *   Analyze inter_event
     *------------------------------------*/
    const char *msg_type = msg_iev_get_msg_type(gobj, iev_kw);

    /*-----------------------------------------------------------*
     *  Get inter-event routing information.
     *  Version > 2.4.0
     *  Cambio msg_iev_get_stack(, , true->false)
     *  porque el yuno puede informar autónomamente
     *  de un cambio de play->pause, y entonces viene sin stack,
     *  porque no es una petición que salga del agente.
     *-----------------------------------------------------------*/
    json_t *jn_ievent_id = msg_iev_get_stack(gobj, iev_kw, IEVENT_MESSAGE_AREA_ID, false);
    // TODO en request_id tenemos el routing del inter-event

    /*----------------------------------------*
     *  Check dst role^name
     *----------------------------------------*/
    const char *iev_dst_role = kw_get_str(gobj, jn_ievent_id, "dst_role", "", 0);
    if(!empty_string(iev_dst_role)) {
        if(strcmp(iev_dst_role, gobj_yuno_role())!=0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "It's not my role",
                "yuno_role",    "%s", iev_dst_role,
                "my_role",      "%s", gobj_yuno_role(),
                NULL
            );
            drop(gobj);
            KW_DECREF(iev_kw)
            KW_DECREF(kw)
            return -1;
        }
    }
    const char *iev_dst_yuno = kw_get_str(gobj, jn_ievent_id, "dst_yuno", "", 0);
    if(!empty_string(iev_dst_yuno)) {
        if(strcmp(iev_dst_yuno, gobj_yuno_name())!=0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "It's not my name",
                "yuno_name",    "%s", iev_dst_yuno,
                "my_name",      "%s", gobj_yuno_name(),
                NULL
            );
            drop(gobj);
            KW_DECREF(iev_kw)
            KW_DECREF(kw)
            return -1;
        }
    }

    /*----------------------------------------*
     *  Check dst service
     *----------------------------------------*/
    const char *iev_dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
    // TODO check if it's a required service authorized
    hgobj gobj_service = gobj_find_service(iev_dst_service, false);
    if(!gobj_service) {
        gobj_service = priv->gobj_service;
    }
    if(!gobj_service) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "event ignored, service not found",
            "service",      "%s", iev_dst_service,
            "event",        "%s", iev_event,
            NULL
        );
        KW_DECREF(iev_kw)
        KW_DECREF(kw)
        return -1;
    }

    /*------------------------------------*
     *   Dispatch event
     *------------------------------------*/
    if(strcasecmp(msg_type, "__subscribing__")==0) {
        /*-----------------------------------*
         *  It's a external subscription
         *-----------------------------------*/

        /*-------------------------------------------------*
         *  Check AUTHZ
         *-------------------------------------------------*/
//         const EVENT *output_event = gobj_output_event(publisher);
//             if(output_event->authz & EV_AUTHZ_INJECT) {
//                 const char *event = output_event->event?output_event->event:"";
//                 /*
//                 *  AUTHZ Required
//                 */
//                 json_t *kw_authz = json_pack("{s:s}",
//                     "event", event
//                 );
//                 if(kw) {
//                     json_object_set(kw_authz, "kw", kw);
//                 } else {
//                     json_object_set_new(kw_authz, "kw", json_object());
//                 }
//                 if(!gobj_user_has_authz(
//                     publisher_,
//                     "__subscribe_event__",
//                     kw_authz,
//                     subscriber_
//                 )) {
//                     gobj_log_error(gobj, 0,
//                         "gobj",         "%s", gobj_full_name(publisher_),
//                         "function",     "%s", __FUNCTION__,
//                         "msgset",       "%s", MSGSET_AUTH_ERROR,
//                         "msg",          "%s", "No permission to subscribe event",
//                         //"user",         "%s", gobj_get_user(subscriber_),
//                         "gclass",       "%s", gobj_gclass_name(publisher_),
//                         "event",        "%s", event?event:"",
//                         NULL
//                     );
//                     KW_DECREF(kw)
//                     return 0;
//                 }
//             }
//             output_event_list++;
//         }

        /*
         *   Protect: only public events
         */
         if(!gobj_has_output_event(gobj_service, iev_event, EVF_PUBLIC_EVENT)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "SUBSCRIBING event ignored, not PUBLIC or PUBLIC event",
                "service",      "%s", iev_dst_service,
                "gobj_service", "%s", gobj_short_name(gobj_service),
                "event",        "%s", iev_event,
                NULL
            );
            KW_DECREF(iev_kw)
            KW_DECREF(kw)
            return -1;
        }

        // Set locals to remove on publishing
        kw_set_subdict_value(
            gobj,
            iev_kw,
            "__local__", "__subscription_reference__",
            json_integer((json_int_t)(size_t)gobj)
        );
        kw_set_subdict_value(gobj, iev_kw, "__local__", "__temp__", json_null());

        // Prepare the return of response
        json_t *__md_iev__ = kw_get_dict(gobj, iev_kw, "__md_iev__", 0, 0);
        if(__md_iev__) {
            KW_INCREF(iev_kw)
            json_t *kw3 = msg_iev_set_back_metadata(
                gobj,
                iev_kw,
                0,
                true
            );

            json_object_del(iev_kw, "__md_iev__");
            json_t *__global__ = kw_get_dict(gobj, iev_kw, "__global__", 0, 0);
            if(__global__) {
                json_object_update_new(__global__, kw3);
            } else {
                json_object_set_new(iev_kw, "__global__", kw3);
            }
        }

        gobj_subscribe_event(gobj_service, iev_event, iev_kw, gobj);

    } else if(strcasecmp(msg_type, "__unsubscribing__")==0) {

        /*-----------------------------------*
         *  It's a external unsubscription
         *-----------------------------------*/
        /*-------------------------------------------------*
         *  Check AUTHZ
         *-------------------------------------------------*/
        // TODO

        /*
         *   Protect: only public events
         */
        if(!gobj_has_output_event(gobj_service, iev_event, EVF_PUBLIC_EVENT)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "UNSUBSCRIBING event ignored, not PUBLIC or PUBLIC event",
                "service",      "%s", iev_dst_service,
                "gobj_service", "%s", gobj_short_name(gobj_service),
                "event",        "%s", iev_event,
                NULL
            );
            KW_DECREF(iev_kw)
            KW_DECREF(kw)
            return -1;
        }
        kw_delete(gobj, iev_kw, "__md_iev__");
        gobj_unsubscribe_event(gobj_service, iev_event, iev_kw, gobj);

    } else {
        /*----------------------*
         *      Inject event
         *----------------------*/
        /*-------------------------------------------------------*
         *  Filter public events of this gobj
         *-------------------------------------------------------*/

        /*----------------------------------*
         *  Check AUTHZ
         *----------------------------------*/
//         if(ev_desc->authz & EV_AUTHZ_INJECT) {
//             /*
//             *  AUTHZ Required
//             */
//             json_t *kw_authz = json_pack("{s:s}",
//                 "event", event
//             );
//             if(kw) {
//                 json_object_set(kw_authz, "kw", kw);
//             } else {
//                 json_object_set_new(kw_authz, "kw", json_object());
//             }
//             if(!gobj_user_has_authz(
//                     dst, "__inject_event__", kw_authz, src)
//             ) {
//                 gobj_log_error(gobj, 0,
//                     "gobj",         "%s", gobj_full_name(dst),
//                     "function",     "%s", __FUNCTION__,
//                     "msgset",       "%s", MSGSET_AUTH_ERROR,
//                     "msg",          "%s", "No permission to inject event",
//                     //"user",         "%s", gobj_get_user(src),
//                     "gclass",       "%s", gobj_gclass_name(dst),
//                     "event",        "%s", event?event:"",
//                     NULL
//                 );
//                 KW_DECREF(kw)
//                 return -403;
//             }
//         }

        if(gobj_has_event(gobj, iev_event, EVF_PUBLIC_EVENT)) {
            /*
            *  It's mine (I manage inter-command and inter-stats)
            */
            int ret = gobj_send_event(
                gobj,
                iev_event,
                iev_kw,
                gobj
            );
            KW_DECREF(kw)
            return ret;
        }

        /*
         *   Send inter-event to subscriber
         */
        kw_set_subdict_value(
            gobj,
            iev_kw,
            "__temp__", "__username__",
            json_string(gobj_read_str_attr(gobj, "__username__"))
        );

        json_t *jn_iev = json_object();
        json_object_set_new(jn_iev, "event", json_string(iev_event));
        json_object_set_new(jn_iev, "kw", iev_kw);

        /*
         *  HACK Obligatoriamente el destinatario tiene que ser un IOGate!!! ???
         *  Todos los mensajes (comandos, stats, subscribe/unsubscribe) van
         *  al dst_service del mensaje en curso,
         *  y si éste no existe van dst_service del identity_card,
         *  pero el mensaje directo va al subscriber directamente? Porqué?
         *  porque el IOGate es quien lo publica, como gobj unico que engloba a todos sus canales.
         *
         *  En el agente se responde al src directamente porque los mensajes vienen de commando!
         *  Por eso en los que no vienen de comando tienen que usar el __temp__`channel_gobj!
         *
         *  Es un metodo de información más directo que el comando, porque no pasa por un parser.
         *
         *  WARNING efectos colaterales si lo cambio? seguro que muchos
         */

        /*
         *  ~CHILD subscription model, send to subscriber
         */
        if(gobj_is_service(gobj)) {
            gobj_publish_event(gobj, EV_ON_IEV_MESSAGE, jn_iev);
        } else {
            gobj_send_event(
                priv->subscriber,
                EV_ON_IEV_MESSAGE, //EV_IEV_MESSAGE, TODO check
                jn_iev,
                gobj
            );
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  remote ask for stats
 ***************************************************************************/
PRIVATE int ac_mt_stats(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------*
     *  Check AUTHZ
     *----------------------------------*/
    if(!gobj_read_bool_attr(gobj, "authenticated")) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Only authenticated users can request stats",
            NULL
        );
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("Only authenticated users can request stats"),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        kw_response = msg_iev_set_back_metadata(
            gobj,
            kw,             // owned, kw request, used to extract ONLY __md_iev__
            kw_response,    // like owned, is returned!, created if null, the body of answer message
            true            // reverse_dst
        );

        return send_static_iev(gobj,
            EV_MT_STATS_ANSWER,
            kw_response,
            src
        );
    }

    /*------------------------------------*
     *   Analyze inter_event
     *------------------------------------*/

    /*-----------------------------------------------------------*
     *  Get inter-event routing information.
     *-----------------------------------------------------------*/
    json_t *jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_MESSAGE_AREA_ID, false);

    /*----------------------------------------*
     *  Check dst role^name
     *----------------------------------------*/

    /*----------------------------------------*
     *  Check dst service
     *----------------------------------------*/
    const char *iev_dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
    // TODO check if it's a required service authorized
    hgobj gobj_service = gobj_find_service(iev_dst_service, false);
    if(!gobj_service) {
        gobj_service = priv->gobj_service;
    }
    if(!gobj_service) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "stats ignored, service not found",
            "service",      "%s", iev_dst_service,
            "event",        "%s", event,
            NULL
        );

        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("Service not found: '%s'", iev_dst_service),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        kw_response = msg_iev_set_back_metadata(
            gobj,
            kw,             // owned, kw request, used to extract ONLY __md_iev__
            kw_response,    // like owned, is returned!, created if null, the body of answer message
            true            // reverse_dst
        );

        return send_static_iev(gobj,
            EV_MT_STATS_ANSWER,
            kw_response,
            src
        );
    }

    /*------------------------------------*
     *   Dispatch stats
     *------------------------------------*/
    const char *stats = kw_get_str(gobj, kw, "__stats__", 0, 0); // v6
    if(!stats) {
        // v7
        json_t *__stats__ = msg_iev_get_stack(gobj, kw, "__stats__", true);
        stats = kw_get_str(gobj, __stats__, "stats", "", KW_REQUIRED);
    }

    kw_set_subdict_value(
        gobj,
        kw,
        "__temp__", "__username__",
        json_string(gobj_read_str_attr(gobj, "__username__"))
    );

    KW_INCREF(kw)
    json_t *kw_response = gobj_stats(
        gobj_service,
        stats,
        kw,
        src
    );
    if(!kw_response) {
        // Asynchronous response
    } else {
        json_t *kw2 = msg_iev_set_back_metadata(
            gobj,
            kw,
            kw_response,
            true
        );

        return send_static_iev(gobj,
            EV_MT_STATS_ANSWER,
            kw2,
            src
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  remote ask for command
 ***************************************************************************/
PRIVATE int ac_mt_command(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*----------------------------------*
     *  Check AUTHZ
     *----------------------------------*/
    if(!gobj_read_bool_attr(gobj, "authenticated")) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Only authenticated users can request commands",
            NULL
        );

        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("Only authenticated users can request commands"),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        kw_response = msg_iev_set_back_metadata(
            gobj,
            kw,             // owned, kw request, used to extract ONLY __md_iev__
            kw_response,    // like owned, is returned!, created if null, the body of answer message
            true            // reverse_dst
        );

        return send_static_iev(gobj,
            EV_MT_COMMAND_ANSWER,
            kw_response,
            src
        );
    }

    /*------------------------------------*
     *   Analyze inter_event
     *------------------------------------*/

    /*-----------------------------------------------------------*
     *  Get inter-event routing information.
     *-----------------------------------------------------------*/
    json_t *jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_MESSAGE_AREA_ID, false);

    /*----------------------------------------*
     *  Check dst role^name
     *----------------------------------------*/

    /*----------------------------------------*
     *  Check dst service
     *----------------------------------------*/
    const char *iev_dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
    // TODO check if it's a required service authorized
    hgobj gobj_service = gobj_find_service(iev_dst_service, false);
    if(!gobj_service) {
        gobj_service = priv->gobj_service;
    }
    if(!gobj_service) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "command ignored, service not found",
            "service",      "%s", iev_dst_service,
            "event",        "%s", event,
            NULL
        );

        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("Service not found: '%s'", iev_dst_service),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        kw_response = msg_iev_set_back_metadata(
            gobj,
            kw,             // owned, kw request, used to extract ONLY __md_iev__
            kw_response,    // like owned, is returned!, created if null, the body of answer message
            true            // reverse_dst
        );

        return send_static_iev(gobj,
            EV_MT_COMMAND_ANSWER,
            kw_response,
            src
        );
    }

    /*------------------------------------*
     *   Dispatch command
     *------------------------------------*/
    const char *command = kw_get_str(gobj, kw, "__command__", 0, 0); // v6
    if(!command) {
        // v7
        json_t *__command__ = msg_iev_get_stack(gobj, kw, "__command__", true);
        command = kw_get_str(gobj, __command__, "command", "", KW_REQUIRED);
    }

    kw_set_subdict_value(
        gobj,
        kw,
        "__temp__", "__username__",
        json_string(gobj_read_str_attr(gobj, "__username__"))
    );

    KW_INCREF(kw)
    json_t *kw_response = gobj_command(
        gobj_service,
        command,
        kw,
        src
    );
    if(!kw_response) {
        // Asynchronous response
    } else {
        json_t *kw2 = msg_iev_set_back_metadata(
            gobj,
            kw,
            kw_response,
            true
        );

        return send_static_iev(gobj,
            EV_MT_COMMAND_ANSWER,
            kw2,
            src
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  remote ask
 *  Somebody wants exit
 ***************************************************************************/
PRIVATE int ac_goodbye(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    const char *cause = kw_get_str(gobj, kw, "cause", "", 0);

    uint32_t trace_level = gobj_trace_level(gobj);
    if((trace_level & TRACE_IDENTITY_CARD)) {
        char prefix[256];
        snprintf(prefix, sizeof(prefix),
            "INTRA-EVENT(%s^%s) %s <== %s (cause: %s)",
            gobj_yuno_role(),
            gobj_yuno_name(),
            gobj_short_name(gobj),
            gobj_short_name(src),
            cause
        );
        trace_inter_event2(gobj, prefix, event, kw);
    }
    drop(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    drop(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    /*
     *  someone or ... has stopped
     */
    KW_DECREF(kw)
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
    .mt_stats = mt_stats,
    .mt_command_parser = mt_command,
    .mt_inject_event = mt_inject_event,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_IEVENT_SRV);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DEFINE_STATE(ST_WAIT_IDENTITY_CARD);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_GOODBYE);


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
        {EV_ON_OPEN,            ac_on_open,             ST_WAIT_IDENTITY_CARD},
        {EV_STOPPED,            ac_stopped,             0},
        {0,0,0}
    };
    ev_action_t st_wait_identity_card[] = {
        {EV_ON_MESSAGE,         ac_on_message,          0},
        {EV_IDENTITY_CARD,      ac_identity_card,       0},
        {EV_GOODBYE,            ac_goodbye,             0},
        {EV_ON_CLOSE,           ac_on_close,            ST_DISCONNECTED},
        {EV_DROP,               ac_drop,                0},
        {EV_TIMEOUT,            ac_timeout_wait_idGot,  0},
        {0,0,0}
    };
    ev_action_t st_session[] = {
        {EV_ON_MESSAGE,         ac_on_message,          0},
        {EV_MT_STATS,           ac_mt_stats,            0},
        {EV_MT_COMMAND,         ac_mt_command,          0},
        {EV_IDENTITY_CARD,      ac_identity_card,       0},
        {EV_GOODBYE,            ac_goodbye,             0},
        {EV_ON_CLOSE,           ac_on_close,            ST_DISCONNECTED},
        {EV_DROP,               ac_drop,                0},
        {EV_STOPPED,            ac_stopped,             0}, // puede llegar por aquí en un gobj_stop_tree()
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,           st_disconnected},
        {ST_WAIT_IDENTITY_CARD,     st_wait_identity_card},
        {ST_SESSION,                st_session},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_MESSAGE,         0},
        {EV_ON_OPEN,            EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_ON_CLOSE,           EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_DROP,               0},
        // public
        {EV_IDENTITY_CARD,      EVF_PUBLIC_EVENT},
        {EV_GOODBYE,            EVF_PUBLIC_EVENT},
        {EV_MT_STATS,           EVF_PUBLIC_EVENT},
        {EV_MT_COMMAND,         EVF_PUBLIC_EVENT},
        // internal
        {EV_TIMEOUT,            0},
        {EV_STOPPED,            0},


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
        gcflag_no_check_output_events   // gcflag_t
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
PUBLIC int register_c_ievent_srv(void)
{
    return create_gclass(C_IEVENT_SRV);
}
