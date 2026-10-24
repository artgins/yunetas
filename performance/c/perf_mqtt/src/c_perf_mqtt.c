/***********************************************************************
 *          c_perf_mqtt.c
 *          Perf_mqtt GClass.
 *
 *          The client of perf_mqtt (from the test c_mqtt/test1):
 *            1. The timer fires: start the MQTT client (`__output_side__`)
 *            2. EV_ON_OPEN (CONNACK): subscribe to "perf/topic"
 *            3. EV_MQTT_SUBSCRIBE (SUBACK): publish the first message, QoS 0
 *            4. EV_MQTT_MESSAGE: publish the next one, MAX_MESSAGES times,
 *               timed; then disconnect and die
 *          A connection closed before the end is an error.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include <c_mqtt_broker.h>
#include <c_prot_mqtt2.h>
#include "c_perf_mqtt.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define MAX_MESSAGES 100000
#define TOPIC "perf/topic"

/*
 *  99 bytes of json, the payload of perf_ievent
 */
#define PAYLOAD "{\"device\": \"0000000001\", \"t\": 1729600000, " \
    "\"v\": [230.1, 230.4, 229.8], \"i\": [1.2, 1.1, 0.9], \"q\": 1}"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int publish_one(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
extern time_measure_t time_measure;

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type--------name----------------flag----------------default-----description---------- */
SDATA (DTP_POINTER, "subscriber",       0,                  0,          "Subscriber of output-events"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj gobj_output_side;
    json_int_t messages;
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
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  One QoS 0 publication
 ***************************************************************************/
PRIVATE int publish_one(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t ln = strlen(PAYLOAD);
    gbuffer_t *gbuf = gbuffer_create(ln, ln);
    gbuffer_append_string(gbuf, PAYLOAD);

    json_t *kw_pub = json_pack("{s:s, s:i, s:i, s:b, s:I}",
        "topic",            TOPIC,
        "qos",              0,
        "expiry_interval",  0,
        "retain",           0,
        "gbuffer",          (json_int_t)(uintptr_t)gbuf
    );
    json_t *kw_iev = iev_create(gobj, EV_MQTT_PUBLISH, kw_pub);
    return gobj_send_event(priv->gobj_output_side, EV_SEND_IEV, kw_iev, gobj);
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The timer fired: start the MQTT client tree (initiate the connection)
 ***************************************************************************/
PRIVATE int ac_connect(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start_tree(priv->gobj_output_side);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Connected (CONNACK): subscribe the topic
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw_sub = json_pack("{s:[s], s:i, s:i}",
        "subs",     TOPIC,
        "qos",      0,
        "options",  0
    );
    json_t *kw_iev = iev_create(gobj, EV_MQTT_SUBSCRIBE, kw_sub);
    gobj_send_event(priv->gobj_output_side, EV_SEND_IEV, kw_iev, gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Subscribed (SUBACK): the first publication
 ***************************************************************************/
PRIVATE int ac_suback(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->messages = 0;
    MT_START_TIME(time_measure)
    publish_one(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A publication came back: the next one, or the end
 ***************************************************************************/
PRIVATE int ac_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->messages++;
    if(priv->messages < MAX_MESSAGES) {
        publish_one(gobj);
    } else {
        MT_INCREMENT_COUNT(time_measure, MAX_MESSAGES)
        MT_PRINT_TIME(time_measure, "mqtt qos0 round trips")
        gobj_send_event(priv->gobj_output_side, EV_DROP, json_object(), gobj);
        set_yuno_must_die();
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The connection closed: an error before the last message
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->messages < MAX_MESSAGES) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "perf_mqtt FAILED: connection closed before the last message",
            "messages",     "%ld", (long)priv->messages,
            NULL
        );
        set_yuno_must_die();
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  QoS 0 publication sent: nothing to do
 ***************************************************************************/
PRIVATE int ac_publish_sent(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A volatil child stopped: destroy it
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
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_play    = mt_play,
    .mt_pause   = mt_pause,
    .mt_writing = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_PERF_MQTT);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    /*
     *  ST_IDLE: the timer starts the client, then CONNACK (EV_ON_OPEN)
     */
    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_connect,             0},
        {EV_ON_OPEN,                ac_on_open,             ST_CONNECTED},
        {EV_ON_CLOSE,               ac_on_close,            0},
        {EV_STOPPED,                ac_stopped,             0},
        {0,0,0}
    };
    /*
     *  ST_CONNECTED: subscribe, publish, receive
     */
    ev_action_t st_connected[] = {
        {EV_MQTT_SUBSCRIBE,         ac_suback,              0},
        {EV_MQTT_PUBLISH,           ac_publish_sent,        0},
        {EV_MQTT_MESSAGE,           ac_message,             0},
        {EV_ON_CLOSE,               ac_on_close,            ST_IDLE},
        {EV_STOPPED,                ac_stopped,             0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {ST_CONNECTED,              st_connected},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_MQTT_SUBSCRIBE,         0},
        {EV_MQTT_PUBLISH,           0},
        {EV_MQTT_MESSAGE,           0},
        {EV_STOPPED,                0},
        {NULL, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0, // local methods
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        0, // command_table
        s_user_trace_level,
        0 // gcflags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_perf_mqtt(void)
{
    return create_gclass(C_PERF_MQTT);
}
