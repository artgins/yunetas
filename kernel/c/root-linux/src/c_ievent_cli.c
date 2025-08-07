/****************************************************************************
 *          c_ievent_cli.c
 *          Ievent_cli GClass.
 *
 *          Inter-event (client side)
 *          Simulate a remote service like a local gobj.
 *
 *          Copyright (c) 2016-2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include <yuneta_version.h>

#include "c_timer.h"
#include "msg_ievent.h"
#include "c_ievent_cli.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int send_static_iev(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw, // owned and serialized,
    hgobj src
);
PRIVATE json_t *build_cli_ievent_request(
    hgobj gobj,
    const char *src_service,
    const char *dst_service
);
PRIVATE int send_remote_subscription(
    hgobj gobj,
    json_t *subs
);

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE const sdata_desc_t attrs_table[] = {
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_STRING,  "wanted_yuno_role", SDF_RD,         "",         "wanted yuno role"),
SDATA (DTP_STRING,  "wanted_yuno_name", SDF_RD,         "",         "wanted yuno name"),
SDATA (DTP_STRING,  "wanted_yuno_service",SDF_RD,       "",         "wanted yuno service"),
SDATA (DTP_STRING,  "remote_yuno_role", SDF_RD,         "",         "confirmed remote yuno role"),
SDATA (DTP_STRING,  "remote_yuno_name", SDF_RD,         "",         "confirmed remote yuno name"),
SDATA (DTP_STRING,  "remote_yuno_service",SDF_RD,       "",         "confirmed remote yuno service"),
SDATA (DTP_STRING,  "url",              SDF_PERSIST,    "",         "Url to connect"),
SDATA (DTP_STRING,  "jwt",              SDF_PERSIST,    "",         "JWT"),
SDATA (DTP_STRING,  "cert_pem",         SDF_PERSIST,    "",         "SSL server certification, PEM str format"),
SDATA (DTP_JSON,    "extra_info",       SDF_RD,         "{}",       "dict data set by user, added to the identity card msg."),
SDATA (DTP_INTEGER, "timeout_idack",    SDF_RD,         "5000",     "timeout waiting idAck"),
SDATA (DTP_POINTER, "subscriber",       0,              0,          "subscriber of output-events. If null then subscriber is the parent"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendant value!
 *  required paired correlative strings
 *  in s_user_trace_level
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
    const char *remote_yuno_name;
    const char *remote_yuno_role;
    const char *remote_yuno_service;
    hgobj gobj_timer;
    BOOL inform_on_close;
    BOOL inside_on_open;    // avoid duplicates, no subscriptions while in on_open,
                            // they will send in resend_subscriptions

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

    priv->gobj_timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    SET_PRIV(remote_yuno_name,          gobj_read_str_attr)
    SET_PRIV(remote_yuno_role,          gobj_read_str_attr)
    SET_PRIV(remote_yuno_service,       gobj_read_str_attr)
    gobj_write_str_attr(gobj, "wanted_yuno_name", priv->remote_yuno_name?priv->remote_yuno_name:"");
    gobj_write_str_attr(gobj, "wanted_yuno_role", priv->remote_yuno_role?priv->remote_yuno_role:"");
    gobj_write_str_attr(gobj, "wanted_yuno_service", priv->remote_yuno_service?priv->remote_yuno_service:"");

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(remote_yuno_name,        gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(remote_yuno_role,      gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(remote_yuno_service,   gobj_read_str_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->gobj_timer);

    hgobj bottom_gobj = gobj_bottom_gobj(gobj);
    if(!bottom_gobj) {
        char schema[32];
        get_url_schema(gobj, gobj_read_str_attr(gobj, "url"), schema, sizeof(schema));
        gclass_name_t gclass_name = comm_prot_get_gclass(schema);

        json_t *kw_prot = json_pack("{s:s, s:s, s:s}",
            "cert_pem", gobj_read_str_attr(gobj, "cert_pem"),
            "url", gobj_read_str_attr(gobj, "url"),
            "jwt", gobj_read_str_attr(gobj, "jwt")
        );

        #ifdef ESP_PLATFORM
            hgobj gobj_bottom = gobj_create_pure_child(
                gobj_name(gobj),
                gclass_name, // C_ESP_TRANSPORT,
                kw_prot,
                gobj
            );
        #endif
        #ifdef __linux__
            hgobj gobj_bottom = gobj_create_pure_child(
                gobj_name(gobj),
                gclass_name,
                kw_prot,
                gobj
            );
        #endif
        gobj_set_bottom_gobj(gobj, gobj_bottom);
    }

    gobj_start(gobj_bottom_gobj(gobj));

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->gobj_timer);

    gobj_stop(gobj_bottom_gobj(gobj));

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method stats
 ***************************************************************************/
PRIVATE json_t *mt_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src)
{
    if(gobj_current_state(gobj) != ST_SESSION) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Not in session",
            "stats",        "%s", stats?stats:"",
            NULL
        );
        KW_DECREF(kw)
        return NULL;
    }

    if(!kw) {
        kw = json_object();
    }

    /*
     *      __REQUEST__ __MESSAGE__
     */
    json_t *jn_ievent_id = build_cli_ievent_request(
        gobj,
        gobj_name(src),
        kw_get_str(gobj, kw, "service", "", 0)
    );

    json_t *__md_stats__ = kw_get_dict_value(gobj, kw, "__md_stats__", NULL, KW_EXTRACT);
    if(!__md_stats__) {
        __md_stats__ = json_object();
    }

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        STATS_STACK_ID,
        json_pack("{s:s, s:o}",   // owned
            "stats", stats,
            "kw", __md_stats__
        )
    );

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        IEVENT_STACK_ID,
        jn_ievent_id   // owned
    );

    json_object_set_new(kw, "__stats__", json_string(stats));
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
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Not in session",
            "command",      "%s", command,
            NULL
        );
        KW_DECREF(kw)
        return NULL;
    }

    if(!kw) {
        kw = json_object();
    }

    /*
     *      __REQUEST__ __MESSAGE__
     */
    json_t *jn_ievent_id = build_cli_ievent_request(
        gobj,
        gobj_name(src),
        kw_get_str(gobj, kw, "service", "", 0)
    );

    json_t *__md_command__ = kw_get_dict_value(gobj, kw, "__md_command__", NULL, KW_EXTRACT);
    if(!__md_command__) {
        __md_command__ = json_object();
    }

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        COMMAND_STACK_ID,
        json_pack("{s:s, s:o}",   // owned
            "command", command,
            "kw", __md_command__
        )
    );

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        IEVENT_STACK_ID,
        jn_ievent_id   // owned
    );

    json_object_set_new(kw, "__command__", json_string(command));
    msg_iev_set_msg_type(gobj, kw, "__command__");

    send_static_iev(gobj, EV_MT_COMMAND, kw, src);

    return NULL;   // return NULL on asynchronous response.
}

/***************************************************************************
 *      Framework Method inject_event
 ***************************************************************************/
PRIVATE int mt_inject_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_current_state(gobj) != ST_SESSION) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
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
     *  Put the ievent if it doesn't come with it,
     *  if it does come with it, it's because it will be a response
     */
    json_t *jn_request = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, FALSE);
    if(jn_request) {
        /*
         *      __RESPONSE__
         */
    } else {
        /*
         *      __REQUEST__
         */
        json_t *jn_ievent_id = build_cli_ievent_request(
            gobj,
            gobj_name(src),
            kw_get_str(gobj, kw, "__service__", 0, 0)
        );
        json_object_del(kw, "__service__");

        msg_iev_push_stack(
            gobj,
            kw,         // not owned
            IEVENT_STACK_ID,
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

/***************************************************************************
 *      Framework Method subscription_added
 ***************************************************************************/
PRIVATE int mt_subscription_added(
    hgobj gobj,
    json_t *subs)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    // return 0; // TODO hay algo mal, las subscripciones locales se interpretan como remotas

    if(gobj_current_state(gobj) != ST_SESSION) {
        // on_open will send all subscriptions
        return 0;
    }

    if(priv->inside_on_open) {
        // avoid duplicates of subscriptions
        return 0;
    }

    return send_remote_subscription(gobj, subs);
}

/***************************************************************************
 *      Framework Method subscription_deleted
 ***************************************************************************/
PRIVATE int mt_subscription_deleted(
    hgobj gobj,
    json_t *subs)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_current_state(gobj) != ST_SESSION) {
        // Nothing to do. On open this subscription will be not sent.
        return 0;
    }

    if(priv->inside_on_open) {
        // avoid duplicates of subscriptions
        return 0;
    }

    gobj_event_t event = (gobj_event_t)(size_t)kw_get_int(gobj, subs, "event", 0, KW_REQUIRED);
    if(empty_string(event)) {
        // HACK only resend explicit subscriptions
        return -1;
    }

    json_t *__config__ = kw_get_dict(gobj, subs, "__config__", 0, KW_REQUIRED);
    json_t *__global__ = kw_get_dict(gobj, subs, "__global__", 0, KW_REQUIRED);
    json_t *__filter__ = kw_get_dict(gobj, subs, "__filter__", 0, KW_REQUIRED);
    const char *__service__ = kw_get_str(gobj, subs, "__service__", "", KW_REQUIRED);
    hgobj subscriber = (hgobj)(size_t)kw_get_int(gobj, subs, "subscriber", 0, KW_REQUIRED);

    /*
     *      __REQUEST__ __MESSAGE__
     */
    json_t *kw = json_object();
    if(__config__) {
        json_object_set(kw, "__config__", __config__);
    }
    if(__global__) {
        json_object_set(kw, "__global__", __global__);
    }
    if(__filter__) {
        json_object_set(kw, "__filter__", __filter__);
    }
    json_t *jn_ievent_id = build_cli_ievent_request(
        gobj,
        gobj_name(subscriber),
        __service__
    );

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        IEVENT_STACK_ID,
        jn_ievent_id   // owned
    );

    msg_iev_set_msg_type(gobj, kw, "__unsubscribing__");

    return send_static_iev(gobj, event, kw, gobj);
}




                    /***************************
                     *      Local methods
                     ***************************/




/***************************************************************************
 *  __MESSAGE__
 ***************************************************************************/
PRIVATE json_t *build_cli_ievent_request(
    hgobj gobj,
    const char *src_service,
    const char *dst_service
)
{
    if(empty_string(dst_service)) {
        dst_service = gobj_read_str_attr(gobj, "wanted_yuno_service");
    }
    json_t *jn_ievent_chain = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
        "dst_yuno", gobj_read_str_attr(gobj, "wanted_yuno_name"),
        "dst_role", gobj_read_str_attr(gobj, "wanted_yuno_role"),
        "dst_service", dst_service,

        "src_yuno", gobj_yuno_name(),
        "src_role", gobj_yuno_role(),
        "src_service", src_service,

        "user", gobj_read_str_attr(gobj_yuno(), "__username__"),
        "host", get_hostname()
    );
    return jn_ievent_chain;
}

/***************************************************************************
 *  send our identity card if iam client
 *  we only send the identity card in static routes!
 ***************************************************************************/
PRIVATE int send_identity_card(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL playing = gobj_is_playing(gobj_yuno());
    const char *yuno_version = gobj_read_str_attr(gobj_yuno(), "yuno_version");
    const char *yuno_release = gobj_read_str_attr(gobj_yuno(), "yuno_release");
    const char *yuno_tag = gobj_read_str_attr(gobj_yuno(), "yuno_tag");
    json_int_t launch_id = gobj_read_integer_attr(gobj_yuno(), "launch_id");
    json_t *kw_identity_card = json_pack(
        "{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:b, s:i, s:i, s:s, s:s, s:I, s:s, s:s}",
        "yuno_role", gobj_yuno_role(),
        "yuno_id", gobj_yuno_id(),
        "yuno_name", gobj_yuno_name(),
        "yuno_tag", yuno_tag?yuno_tag:"",
        "yuno_version", yuno_version?yuno_version:"",
        "yuno_release", yuno_release?yuno_release:"",
        "yuneta_version", YUNETA_VERSION,
        "playing", playing,
        "pid", (int)getpid(),
        "watcher_pid", (int)gobj_read_integer_attr(gobj_yuno(), "watcher_pid"),
        "jwt", gobj_read_str_attr(gobj, "jwt"),
        "username", gobj_read_str_attr(gobj_yuno(), "__username__"),
        "launch_id", launch_id,
        "yuno_startdate", gobj_read_str_attr(gobj_yuno(), "start_date"),
        "id", node_uuid()
    );

    json_t *jn_required_services = gobj_read_json_attr(gobj_yuno(), "required_services");
    if(jn_required_services) {
        json_object_set(kw_identity_card, "required_services", jn_required_services);
    } else {
        json_object_set_new(kw_identity_card, "required_services", json_array());
    }

    json_t *jn_extra_info = gobj_read_json_attr(gobj, "extra_info");
    if(jn_extra_info) {
        // Additional information that can be added by the user,
        // to attach to the business card.
        // I am not going to allow modifying the yuno's own data
        // Only update missing
        json_object_update_missing(kw_identity_card, jn_extra_info);
    }

    /*
     *      __REQUEST__ __MESSAGE__
     */
    json_t *jn_ievent_id = build_cli_ievent_request(
        gobj,
        gobj_name(gobj),
        0
    );
    msg_iev_push_stack(
        gobj,
        kw_identity_card,         // not owned
        IEVENT_STACK_ID,
        jn_ievent_id   // owned
    );

    msg_iev_set_msg_type(gobj, kw_identity_card, "__identity__");

    set_timeout(priv->gobj_timer, gobj_read_integer_attr(gobj, "timeout_idack"));

    return send_static_iev(
        gobj,
        EV_IDENTITY_CARD,
        kw_identity_card,
        gobj
    );
}

/***************************************************************************
 *  Route must be connected
 ***************************************************************************/
PRIVATE int send_static_iev(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw, // owned and serialized,
    hgobj src
)
{
    hgobj below_gobj = gobj_bottom_gobj(gobj);
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
        kw  // owned and serialized
    );
    if(!gbuf) {
        // error already logged
        return -1;
    }
    json_t *kw_send = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    return gobj_send_event(below_gobj,
        EV_SEND_MESSAGE,
        kw_send,
        gobj
    );
}

/***************************************************************************
 *  send subscription
 ***************************************************************************/
PRIVATE int send_remote_subscription(
    hgobj gobj,
    json_t *subs
)
{
    // const char *event
    gobj_event_t event = (gobj_event_t)(size_t)kw_get_int(gobj, subs, "event", 0, KW_REQUIRED);
    if(empty_string(event)) {
        // HACK only resend explicit subscriptions
        return -1;
    }
    json_t *__config__ = kw_get_dict(gobj, subs, "__config__", 0, KW_REQUIRED);
    json_t *__global__ = kw_get_dict(gobj, subs, "__global__", 0, KW_REQUIRED);
    json_t *__filter__ = kw_get_dict(gobj, subs, "__filter__", 0, KW_REQUIRED);
    const char *__service__ = kw_get_str(gobj, subs, "__service__", "", KW_REQUIRED);
    hgobj subscriber = (hgobj)(size_t)kw_get_int(gobj, subs, "subscriber", 0, KW_REQUIRED);

    /*
     *      __REQUEST__ __MESSAGE__
     */
    json_t *kw = json_object();
    if(__config__) {
        json_object_set(kw, "__config__", __config__);
    }
    if(__global__) {
        json_object_set(kw, "__global__", __global__);
    }
    if(__filter__) {
        json_object_set(kw, "__filter__", __filter__);
    }
    json_t *jn_ievent_id = build_cli_ievent_request(
        gobj,
        gobj_name(subscriber),
        __service__
    );

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        IEVENT_STACK_ID,
        jn_ievent_id   // owned
    );

    msg_iev_set_msg_type(gobj, kw, "__subscribing__");

    return send_static_iev(gobj, event, kw, gobj);
}

/***************************************************************************
 *  resend subscriptions
 ***************************************************************************/
PRIVATE int resend_subscriptions(hgobj gobj)
{
    json_t *dl_subs = gobj_find_subscriptions(gobj, NULL, NULL, NULL);

    size_t idx; json_t *subs;
    json_array_foreach(dl_subs, idx, subs) {
        send_remote_subscription(gobj, subs);
    }
    json_decref(dl_subs);
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *  Static route
     *  Send our identity card as iam client
     */
    send_identity_card(gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    /*
     *  Route (channel) close.
     */
    if(priv->inform_on_close) {
        priv->inform_on_close = FALSE;
        json_t *kw_on_close = json_pack("{s:s, s:s, s:s}",
            "remote_yuno_name", gobj_read_str_attr(gobj, "remote_yuno_name"),
            "remote_yuno_role", gobj_read_str_attr(gobj, "remote_yuno_role"),
            "remote_yuno_service", gobj_read_str_attr(gobj, "remote_yuno_service")
        );

        /*
         *  SERVICE subscription model
         */
        if(gobj_is_pure_child(gobj)) {
            gobj_send_event(gobj_parent(gobj), EV_ON_CLOSE, kw_on_close, gobj);
        } else {
            gobj_publish_event(gobj, EV_ON_CLOSE, kw_on_close);
        }
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_wait_idAck(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    // TODO by here it fails
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_identity_card_ack(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*---------------------------------------*
     *  Clear timeout
     *---------------------------------------*/
    clear_timeout(priv->gobj_timer);

    /*---------------------------------------*
     *  Update remote values
     *---------------------------------------*/
    /*
     *  Here is the end point of the request.
     *  Don't pop the request, because in the
     *  the event can be publishing to more users.
     */
    /*
     *      __RESPONSE__ __MESSAGE__
     */
    json_t *jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, TRUE);
    const char *src_yuno = kw_get_str(gobj, jn_ievent_id, "src_yuno", "", 0);
    const char *src_role = kw_get_str(gobj, jn_ievent_id, "src_role", "", 0);
    const char *src_service = kw_get_str(gobj, jn_ievent_id, "src_service", "", 0);
    gobj_write_str_attr(gobj, "remote_yuno_name", src_yuno);
    gobj_write_str_attr(gobj, "remote_yuno_role", src_role);
    gobj_write_str_attr(gobj, "remote_yuno_service", src_service);

    // WARNING comprueba result, ahora puede venir negativo
    int result = kw_get_int(gobj, kw, "result", -1, 0);
    if(result < 0) {
        gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);

        /*
         *  SERVICE subscription model
         */
        if(gobj_is_pure_child(gobj)) {
            gobj_send_event(gobj_parent(gobj), EV_ON_ID_NAK, json_incref(kw), gobj);
        } else {
            gobj_publish_event(gobj, EV_ON_ID_NAK, json_incref(kw));
        }

    } else {
        json_t *jn_data = kw_get_dict_value(gobj, kw, "data", 0, 0);

        gobj_change_state(gobj, ST_SESSION);

        if(!priv->inform_on_close) {
            priv->inform_on_close = TRUE;
            json_t *kw_on_open = json_pack("{s:s, s:s, s:s, s:O}",
                "remote_yuno_name", gobj_read_str_attr(gobj, "remote_yuno_name"),
                "remote_yuno_role", gobj_read_str_attr(gobj, "remote_yuno_role"),
                "remote_yuno_service", gobj_read_str_attr(gobj, "remote_yuno_service"),
                "data", jn_data?jn_data:json_null()
            );

            /*
             *  SERVICE subscription model
             */
            if(gobj_is_pure_child(gobj)) {
                gobj_send_event(gobj_parent(gobj), EV_ON_OPEN, kw_on_open, gobj);
            } else {
                gobj_publish_event(gobj, EV_ON_OPEN, kw_on_open);
            }
        }

        /*
         *  Resend subscriptions
         */
        resend_subscriptions(gobj);
    }

    JSON_DECREF(jn_ievent_id)
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);

    /*---------------------------------------*
     *  Create inter_event from gbuf
     *---------------------------------------*/
    gbuffer_incref(gbuf);

    gobj_event_t iev_event;
    json_t *iev_kw = iev_create_from_gbuffer(gobj, &iev_event, gbuf, TRUE);
    if(!iev_kw) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "iev_create_from_gbuffer() FAILED",
            NULL
        );
        gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
        KW_DECREF(kw)
        return -1;
    }
    if(empty_string(iev_event)) {
        // Error already logged
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "iev_event NULL",
            NULL
        );
        gobj_trace_json(gobj, iev_kw, "iev_event NULL");
        gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
        KW_DECREF(iev_kw)
        KW_DECREF(kw)
        return -1;
    }

    /*---------------------------------------*
     *          trace inter_event
     *---------------------------------------*/
    uint32_t trace_level = gobj_trace_level(gobj);
    char prefix[256];
    if(trace_level) {
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
     *  If state is not in SESSION, send self.
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
        trace_inter_event(gobj, prefix, iev_event, iev_kw);
        gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
        KW_DECREF(iev_kw)
        KW_DECREF(kw)
        return ret;
    }

    /*------------------------------------*
     *   Analyze inter_event
     *------------------------------------*/
    const char *msg_type = msg_iev_get_msg_type(gobj, iev_kw);

    /*----------------------------------------*
     *  Get inter-event routing information.
     *----------------------------------------*/
    json_t *jn_ievent_id = msg_iev_get_stack(gobj, iev_kw, IEVENT_STACK_ID, TRUE);

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
            trace_inter_event(gobj, prefix, iev_event, iev_kw);
            gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
            KW_DECREF(iev_kw)
            KW_DECREF(kw)
            return -1;
        }
    }
    const char *iev_dst_yuno = kw_get_str(gobj, jn_ievent_id, "dst_yuno", "", 0);
    if(!empty_string(iev_dst_yuno)) {
        char *px = strchr(iev_dst_yuno, '^'); // TODO review, not used in ievent_srv, review
        if(px) {
            *px = 0;
        }
        int ret = strcmp(iev_dst_yuno, gobj_yuno_name());
        if(px) {
            *px = '^';
        }
        if(ret!=0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "It's not my name",
                "yuno_name",    "%s", iev_dst_yuno,
                "my_name",      "%s", gobj_yuno_name(),
                NULL
            );
            trace_inter_event(gobj, prefix, iev_event, iev_kw);
            gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
            KW_DECREF(iev_kw)
            KW_DECREF(kw)
            return -1;
        }
    }

    /*----------------------------------------*
     *  Check dst service
     *----------------------------------------*/
    const char *iev_dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
    // TODO de momento pasa todo, multi-servicio.
    // Obligado al servicio acordado en el identity_card.
    // (priv->gobj_service)
    // Puede venir empty si se autoriza a buscar el evento publico en otros servicios

    /*------------------------------------*
     *   Set channel info
     *------------------------------------*/
    json_object_set_new(
        jn_ievent_id,
        "input_service",
        json_string(gobj_name(gobj_parent(gobj)))
    );
    json_object_set_new(
        jn_ievent_id,
        "input_channel",
        json_string(gobj_name(gobj))
    );

    /*---------------------------------------*
     *          trace inter_event
     *---------------------------------------*/
    if(trace_level) {
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

    /*------------------------------------*
     *   Dispatch event
     *------------------------------------*/
    if(strcasecmp(msg_type, "__subscribing__")==0) {
        /*-----------------------------------*
         *  It's a external subscription
         *-----------------------------------*/
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "subscription event ignored, I'm client",
            "service",      "%s", iev_dst_service,
            "event",        "%s", iev_event,
            NULL
        );
        gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
        KW_DECREF(iev_kw)
        KW_DECREF(kw)
        return -1;

    } else if(strcasecmp(msg_type, "__unsubscribing__")==0) {
        /*-----------------------------------*
         *  It's a external unsubscription
         *-----------------------------------*/
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "subscription event ignored, I'm client",
            "service",      "%s", iev_dst_service,
            "event",        "%s", iev_event,
            NULL
        );
        gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);
        KW_DECREF(iev_kw)
        KW_DECREF(kw)
        return -1;
    }

    /*-------------------------------------------------------*
     *  Filter public events of this gobj
     *-------------------------------------------------------*/
    if(gobj_has_event(gobj, iev_event, EVF_PUBLIC_EVENT)) {
        /*
         *  It's mine (I manage inter-command and inter-stats)
         */
        gobj_send_event(
            gobj,
            iev_event,
            iev_kw,
            gobj
        );
        KW_DECREF(kw)
        return 0;
    }

    /*-------------------------*
     *  Dispatch the event
     *-------------------------*/
    hgobj gobj_service = gobj_find_service(iev_dst_service, TRUE);
    if(gobj_service) {
        if(gobj_has_event(gobj_service, iev_event, EVF_PUBLIC_EVENT)) {
            gobj_send_event(gobj_service, iev_event, iev_kw, gobj);
        } else {
            // TODO by now publish event, agent changing the destine (perhaps in ac_final_count)
            // TODO in future restore the gobj_log_error
            gobj_publish_event( /* NOTE original behavior */
                gobj,
                iev_event,
                iev_kw
            );
            // gobj_log_error(gobj, 0,
            //     "function",     "%s", __FUNCTION__,
            //     "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            //     "msg",          "%s", "Service found but no event or not public",
            //     "service",      "%s", gobj_short_name(gobj_service),
            //     "event",        "%s", iev_event,
            //     NULL
            // );
            // KW_DECREF(iev_kw)
            // KW_DECREF(kw)
            // return -1;
        }
    } else {
        /*
         *  SERVICE subscription model
         *  TODO Shouldn't this event be rejected? It may make sense in routing task.
         */
        if(gobj_is_pure_child(gobj)) {
            gobj_send_event(gobj_parent(gobj), iev_event, iev_kw, gobj);
        } else {
            gobj_publish_event(gobj, iev_event, iev_kw);
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  remote asking for stats
 ***************************************************************************/
PRIVATE int ac_mt_stats(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *      __RESPONSE__ __MESSAGE__
     */

    /*----------------------------------*
     *  Check AUTHZ
     *----------------------------------*/
    /*------------------------------------*
     *   Analyze inter_event
     *------------------------------------*/
    /*-----------------------------------------------------------*
     *  Get inter-event routing information.
     *-----------------------------------------------------------*/
    // json_t *jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, FALSE);

    /*----------------------------------------*
     *  Check dst role^name
     *----------------------------------------*/
    /*----------------------------------------*
     *  Check dst service
     *----------------------------------------*/
    const char *service = kw_get_str(gobj, kw, "service", "", 0);

    hgobj gobj_service;
    if(empty_string(service)) {
        gobj_service = gobj_default_service();
    } else {
        gobj_service = gobj_find_service(service, FALSE);
        if(!gobj_service) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Service not found",
                "service",      "%s", service,
                "event",        "%s", event,
                NULL
            );
            json_t *kw_response = build_command_response(
                gobj,
                -1,     // result
                json_sprintf("Service not found: '%s'", service),   // jn_comment
                0,      // jn_schema
                0       // jn_data
            );
            kw_response = msg_iev_set_back_metadata(
                gobj,
                kw,             // owned, kw request, used to extract ONLY __md_iev__
                kw_response,    // like owned, is returned!, created if null, the body of answer message
                TRUE            // reverse_dst
            );

            return send_static_iev(gobj,
                EV_MT_STATS_ANSWER,
                kw_response,
                src
            );
        }
    }

    /*------------------------------------*
     *   Dispatch stats
     *------------------------------------*/
    const char *stats = kw_get_str(gobj, kw, "__stats__", 0, 0); // v6
    if(!stats) {
        // v7
        json_t *__stats__ = msg_iev_get_stack(gobj, kw, "__stats__", TRUE);
        stats = kw_get_str(gobj, __stats__, "stats", "", KW_REQUIRED);
    }

    KW_INCREF(kw);
    json_t *kw_response = gobj_stats(
        gobj_service,
        stats,
        kw,
        src
    );
    if(!kw_response) {
       // Asynchronous response
    } else {
        kw_response = msg_iev_set_back_metadata(
            gobj,
            kw,             // owned, kw request, used to extract ONLY __md_iev__
            kw_response,    // like owned, is returned!, created if null, the body of answer message
            TRUE            // reverse_dst
        );

        return send_static_iev(gobj,
            EV_MT_STATS_ANSWER,
            kw_response,
            src
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  remote asking for command
 ***************************************************************************/
PRIVATE int ac_mt_command(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *      __RESPONSE__ __MESSAGE__
     */

    /*----------------------------------*
     *  Check AUTHZ
     *----------------------------------*/
    /*------------------------------------*
     *   Analyze inter_event
     *------------------------------------*/
    /*-----------------------------------------------------------*
     *  Get inter-event routing information.
     *-----------------------------------------------------------*/
    // json_t *jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, FALSE);

    /*----------------------------------------*
     *  Check dst role^name
     *----------------------------------------*/
    /*----------------------------------------*
     *  Check dst service
     *----------------------------------------*/
    const char *service = kw_get_str(gobj, kw, "service", "", 0);

    hgobj gobj_service;
    if(empty_string(service)) {
        gobj_service = gobj_default_service();
    } else {
        gobj_service = gobj_find_service(service, FALSE);
        if(!gobj_service) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Service not found",
                "service",      "%s", service,
                "event",        "%s", event,
                NULL
            );
            json_t *kw_response = build_command_response(
                gobj,
                -1,     // result
                json_sprintf("Service not found: '%s'", service),   // jn_comment
                0,      // jn_schema
                0       // jn_data
            );
            kw_response = msg_iev_set_back_metadata(
                gobj,
                kw,             // owned, kw request, used to extract ONLY __md_iev__
                kw_response,    // like owned, is returned!, created if null, the body of answer message
                TRUE            // reverse_dst
            );

            return send_static_iev(gobj,
                EV_MT_STATS_ANSWER,
                kw_response,
                src
            );
        }
    }

    /*------------------------------------*
     *   Dispatch command
     *------------------------------------*/
    const char *command = kw_get_str(gobj, kw, "__command__", 0, 0); // v6
    if(!command) {
        // v7
        json_t *__command__ = msg_iev_get_stack(gobj, kw, "__command__", TRUE);
        command = kw_get_str(gobj, __command__, "command", "", KW_REQUIRED);
    }

    KW_INCREF(kw);
    json_t *kw_response = gobj_command(
        gobj_service,
        command,
        kw,
        src
    );
    if(!kw_response) {
        // Asynchronous response
    } else {
        kw_response = msg_iev_set_back_metadata(
            gobj,
            kw,             // owned, kw request, used to extract ONLY __md_iev__
            kw_response,    // like owned, is returned!, created if null, the body of answer message
            TRUE            // reverse_dst
        );

        return send_static_iev(gobj,
            EV_MT_COMMAND_ANSWER,
            kw_response,
            src
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  remote asking for play yuno
 ***************************************************************************/
PRIVATE int ac_play_yuno(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *      __RESPONSE__ __MESSAGE__
     */
    int ret = gobj_play(gobj_yuno());
    json_t *jn_result = json_pack("{s:i}",
        "result",
        ret
    );
    json_t *kw2resp = msg_iev_set_back_metadata(
        gobj,
        kw,             // owned, kw request, used to extract ONLY __md_iev__
        jn_result,      // like owned, is returned!, created if null, the body of answer message
        TRUE            // reverse_dst
    );

    return send_static_iev(gobj,
        EV_PLAY_YUNO_ACK,
        kw2resp,
        src
    );
}

/***************************************************************************
 *  remote asking for pause yuno
 ***************************************************************************/
PRIVATE int ac_pause_yuno(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *      __RESPONSE__ __MESSAGE__
     */

    int ret = gobj_pause(gobj_yuno());
    json_t *jn_result = json_pack("{s:i}",
        "result",
        ret
    );
    json_t *kw2resp = msg_iev_set_back_metadata(
        gobj,
        kw,
        jn_result,
        TRUE            // reverse_dst
    );

    return send_static_iev(gobj,
        EV_PAUSE_YUNO_ACK,
        kw2resp,
        src
    );
}

/***************************************************************************
 *  For asynchronous responses
 ***************************************************************************/
PRIVATE int ac_send_command_answer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *      __RESPONSE__ __MESSAGE__
     */
    return send_static_iev(gobj,
        EV_MT_COMMAND_ANSWER,
        kw,
        src
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_send_event(gobj_bottom_gobj(gobj), EV_DROP, 0, gobj);

    JSON_DECREF(kw)
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
    .mt_create = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_subscription_added = mt_subscription_added,
    .mt_subscription_deleted = mt_subscription_deleted,
    .mt_stats = mt_stats,
    .mt_command_parser = mt_command,
    .mt_inject_event = mt_inject_event,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_IEVENT_CLI);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DEFINE_STATE(ST_WAIT_IDENTITY_CARD_ACK);

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
        {EV_ON_OPEN,            ac_on_open,             ST_WAIT_IDENTITY_CARD_ACK},
        {EV_STOPPED,            ac_stopped,             0},
        {0,0,0}
    };
    ev_action_t st_wait_identity_card_ack[] = {
        {EV_ON_MESSAGE,         ac_on_message,          0},
        {EV_IDENTITY_CARD_ACK,  ac_identity_card_ack,   0},
        {EV_ON_CLOSE,           ac_on_close,            ST_DISCONNECTED},
        {EV_TIMEOUT,            ac_timeout_wait_idAck,  0},
        {0,0,0}
    };
    ev_action_t st_session[] = {
        {EV_ON_MESSAGE,         ac_on_message,          0},
        {EV_MT_COMMAND,         ac_mt_command,          0},
        {EV_MT_STATS,           ac_mt_stats,            0},
        {EV_SEND_COMMAND_ANSWER,ac_send_command_answer, 0},
        {EV_PLAY_YUNO,          ac_play_yuno,           0},
        {EV_PAUSE_YUNO,         ac_pause_yuno,          0},
        {EV_ON_CLOSE,           ac_on_close,            ST_DISCONNECTED},
        {EV_DROP,               ac_drop,                0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,           st_disconnected},
        {ST_WAIT_IDENTITY_CARD_ACK, st_wait_identity_card_ack},
        {ST_SESSION,                st_session},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_MESSAGE,             0},
        {EV_MT_COMMAND,             EVF_PUBLIC_EVENT},
        {EV_MT_STATS,               EVF_PUBLIC_EVENT},
        {EV_SEND_COMMAND_ANSWER,    EVF_PUBLIC_EVENT},
        {EV_IDENTITY_CARD_ACK,      EVF_PUBLIC_EVENT},
        {EV_PLAY_YUNO,              EVF_PUBLIC_EVENT},  // Extra events to let agent
        {EV_PAUSE_YUNO,             EVF_PUBLIC_EVENT},  // request clients

        {EV_ON_OPEN,                EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_ON_CLOSE,               EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS},
        {EV_ON_ID_NAK,              EVF_OUTPUT_EVENT},

        {EV_TIMEOUT,                0},
        {EV_DROP,                   0},
        {EV_STOPPED,                0},

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
PUBLIC int register_c_ievent_cli(void)
{
    return create_gclass(C_IEVENT_CLI);
}
