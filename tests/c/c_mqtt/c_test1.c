/****************************************************************************
 *          C_TEST1.C
 *
 *          Self-contained MQTT QoS 0 pub/sub test GClass.
 *
 *          Test sequence:
 *            1. Timer fires (500 ms) → start MQTT client (output_side)
 *            2. EV_ON_OPEN (CONNACK)  → subscribe to "test/topic"
 *            3. EV_MQTT_SUBSCRIBE (SUBACK) → publish "Hello MQTT" to "test/topic"
 *            4. EV_MQTT_MESSAGE       → verify topic + payload, disconnect, die
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include <c_mqtt_broker.h>
#include <c_prot_mqtt2.h>
#include "c_test1.h"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type----------name-----------flag----default-----description---------*/
SDATA (DTP_POINTER,   "user_data",   0,      0,          "user data"),
SDATA (DTP_POINTER,   "subscriber",  0,      0,          "subscriber of output-events"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
    {"messages",    "Trace messages"},
    {0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj gobj_output_side;
    BOOL message_received;
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

    gobj_stop(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_output_side = gobj_find_service("__output_side__", TRUE);
    gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);

    set_timeout(priv->timer, 500);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Timer fired: start the MQTT client tree (initiate TCP connection)
 ***************************************************************************/
PRIVATE int ac_connect(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start_tree(priv->gobj_output_side);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  MQTT connected (CONNACK received): subscribe to "test/topic"
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw_sub = json_pack("{s:[s], s:i, s:i}",
        "subs",     "test/topic",
        "qos",      0,
        "options",  0
    );
    json_t *kw_iev = iev_create(gobj, EV_MQTT_SUBSCRIBE, kw_sub);
    gobj_send_event(priv->gobj_output_side, EV_SEND_IEV, kw_iev, gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  SUBACK received: publish "Hello MQTT" to "test/topic"
 ***************************************************************************/
PRIVATE int ac_suback(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *msg = "Hello MQTT";
    size_t msglen = strlen(msg);
    gbuffer_t *gbuf = gbuffer_create(msglen, msglen);
    gbuffer_append_string(gbuf, msg);

    json_t *kw_pub = json_pack("{s:s, s:i, s:i, s:b, s:I}",
        "topic",            "test/topic",
        "qos",              0,
        "expiry_interval",  0,
        "retain",           0,
        "gbuffer",          (json_int_t)(uintptr_t)gbuf
    );
    json_t *kw_iev = iev_create(gobj, EV_MQTT_PUBLISH, kw_pub);
    gobj_send_event(priv->gobj_output_side, EV_SEND_IEV, kw_iev, gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  MQTT message received: verify topic + payload, then disconnect and die
 ***************************************************************************/
PRIVATE int ac_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *topic = kw_get_str(gobj, kw, "topic", "", 0);
    if(strcmp(topic, "test/topic") != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "MQTT test FAILED: wrong topic",
            "expected",     "%s", "test/topic",
            "got",          "%s", topic,
            NULL
        );
    } else {
        gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
        if(!gbuf) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_APP,
                "msg",          "%s", "MQTT test FAILED: NULL gbuffer in message",
                NULL
            );
        } else {
            const char *payload = gbuffer_cur_rd_pointer(gbuf);
            size_t payloadlen = gbuffer_chunk(gbuf);
            const char *expected = "Hello MQTT";
            size_t expected_len = strlen(expected);

            if(payloadlen != expected_len || memcmp(payload, expected, expected_len) != 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_APP,
                    "msg",          "%s", "MQTT test FAILED: wrong payload",
                    "expected",     "%s", expected,
                    NULL
                );
            } else {
                priv->message_received = TRUE;
            }
        }
    }

    /*
     *  Disconnect and finish
     */
    gobj_send_event(priv->gobj_output_side, EV_DROP, json_object(), gobj);
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  MQTT connection closed unexpectedly (before message received)
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->message_received) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "MQTT test FAILED: connection closed before message received",
            NULL
        );
        set_yuno_must_die();
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  QoS 0 publish-sent confirmation: ignore silently
 ***************************************************************************/
PRIVATE int ac_publish_sent(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Volatil child stopped: destroy it
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }

    JSON_DECREF(kw)
    return 0;
}




                    /***************************
                     *      FSM
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST1);

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
    /*
     *  ST_IDLE: waiting for timer to fire (start MQTT connection)
     *           and then waiting for CONNACK (EV_ON_OPEN)
     */
    ev_action_t st_idle[] = {
        {EV_TIMEOUT,            ac_connect,         0},             // start client
        {EV_ON_OPEN,            ac_on_open,         ST_CONNECTED},  // CONNACK → subscribe
        {EV_ON_CLOSE,           ac_on_close,        0},             // unexpected close
        {EV_STOPPED,            ac_stopped,         0},
        {0, 0, 0}
    };

    /*
     *  ST_CONNECTED: MQTT CONNACK received, do subscribe/publish/receive
     */
    ev_action_t st_connected[] = {
        {EV_MQTT_SUBSCRIBE,     ac_suback,          0},             // SUBACK → publish
        {EV_MQTT_PUBLISH,       ac_publish_sent,    0},             // QoS 0 send confirmation
        {EV_MQTT_MESSAGE,       ac_message,         0},             // message → verify + die
        {EV_ON_CLOSE,           ac_on_close,        ST_IDLE},       // unexpected close
        {EV_STOPPED,            ac_stopped,         0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {ST_CONNECTED,          st_connected},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            0},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
        {EV_MQTT_SUBSCRIBE,     0},
        {EV_MQTT_PUBLISH,       0},
        {EV_MQTT_MESSAGE,       0},
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
        0,              // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,              // authz_table
        0,              // command_table
        s_user_trace_level,
        0               // gcflag_t
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
PUBLIC int register_c_test1(void)
{
    return create_gclass(C_TEST1);
}
