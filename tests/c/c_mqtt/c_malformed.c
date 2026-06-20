/****************************************************************************
 *          C_MALFORMED.C
 *
 *          Self-contained MQTT broker malformed-frame regression test GClass.
 *
 *          Regression for the NULL-gbuf crash (core.gate_mqtts): a control
 *          packet whose MQTT "remaining length" is 0 left frame_completed()
 *          with a NULL payload gbuffer, which it then handed to a handler
 *          that dereferenced it (gbuffer_leftbytes(NULL)) -> SIGSEGV. A
 *          remote, single-packet DoS of the whole broker process.
 *
 *          Test sequence:
 *            1. Timer fires (500 ms) -> start the MQTT client stack, which
 *               performs a normal CONNECT/CONNACK with the embedded broker.
 *            2. EV_ON_OPEN (session established) -> reach the client's bottom
 *               C_TCP and inject RAW bytes for a malformed SUBSCRIBE
 *               (0x82 0x00: SUBSCRIBE, flags=2, remaining length 0). This is
 *               the exact in-session shape that used to crash handle__subscribe.
 *            3. Survival timer (700 ms) -> the broker rejected the frame as
 *               malformed and is still alive (the event loop ran the timer):
 *               test PASSES. Before the fix the broker (and therefore this
 *               single-process test) segfaulted here.
 *
 *          The broker is expected to log exactly one error,
 *          "Mqtt malformed packet, payload required" (see main_malformed.c).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_malformed.h"

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
 *  Connect timer fired: start the MQTT client stack (CONNECT/CONNACK)
 ***************************************************************************/
PRIVATE int ac_connect(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start_tree(priv->gobj_output_side);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  MQTT session established (CONNACK): inject the malformed SUBSCRIBE and
 *  arm the survival timer.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Chase down to the client's bottom transport (C_TCP), bypassing the
     *  MQTT protocol layer so we can put malformed bytes on the wire.
     */
    hgobj transport = src;
    while(gobj_bottom_gobj(transport)) {
        transport = gobj_bottom_gobj(transport);
    }

    /*
     *  Malformed SUBSCRIBE: fixed header 0x82 (CMD_SUBSCRIBE | flags 2),
     *  remaining length 0. A valid command for an in-session client, but
     *  zero payload, the exact shape that left a NULL gbuffer reaching
     *  handle__subscribe before the fix.
     */
    static const uint8_t malformed_subscribe[] = {0x82, 0x00};

    gbuffer_t *gbuf = gbuffer_create(sizeof(malformed_subscribe), sizeof(malformed_subscribe));
    gbuffer_append(gbuf, (void *)malformed_subscribe, sizeof(malformed_subscribe));

    json_t *kw_tx = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    gobj_send_event(transport, EV_TX_DATA, kw_tx, gobj);

    /*
     *  If the broker survives the malformed frame, this timer fires and the
     *  test passes. A crash would take down the whole (single-process) test
     *  before the event loop could run it.
     */
    set_timeout(priv->timer, 700);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Survival timer fired: the broker rejected the malformed frame and lives
 ***************************************************************************/
PRIVATE int ac_survived(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    set_yuno_must_die();

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Broker dropped our connection after rejecting the malformed frame.
 *  Expected: do NOT fail. The survival timer is the authoritative signal.
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
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
GOBJ_DEFINE_GCLASS(C_MALFORMED);

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
     *  ST_IDLE: wait for the connect timer, then for CONNACK (EV_ON_OPEN);
     *  after injecting the malformed frame, the same timer proves liveness.
     */
    ev_action_t st_idle[] = {
        {EV_TIMEOUT,            ac_connect,         0},             // start client
        {EV_ON_OPEN,            ac_on_open,         ST_CONNECTED},  // session up -> inject
        {EV_ON_CLOSE,           ac_on_close,        0},
        {EV_STOPPED,            ac_stopped,         0},
        {0, 0, 0}
    };

    /*
     *  ST_CONNECTED: malformed frame injected, waiting to prove the broker lives
     */
    ev_action_t st_connected[] = {
        {EV_TIMEOUT,            ac_survived,        0},             // broker alive -> pass + die
        {EV_ON_CLOSE,           ac_on_close,        0},             // broker dropped us (expected)
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
PUBLIC int register_c_malformed(void)
{
    return create_gclass(C_MALFORMED);
}
