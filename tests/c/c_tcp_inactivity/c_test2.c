/***********************************************************************
 *          C_TEST2.C
 *
 *          Test C_TCP timeout_inactivity (secure traffic, TLS)
 *
 *          Identical flow to C_TEST1, but the client under test connects
 *          with tcps:// and a 'crypto' config. This exercises the inactivity
 *          close and on-demand reconnect across the TLS handshake (the
 *          inactivity timer is armed in set_secure_connected, after the
 *          handshake completes).
 *
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <c_pepon.h>
#include "c_test2.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CLIENT_URL          "tcps://127.0.0.1:7780"
#define TIMEOUT_INACTIVITY  1500    // milliseconds of silence before closing
#define MESSAGE             "PING-INACTIVITY-RECONNECT"

typedef enum {
    PHASE_INIT = 0,
    PHASE_FIRST_CONNECTED,
    PHASE_INACTIVITY_CLOSED,
    PHASE_RECONNECTED,
    PHASE_DONE,
} test_phase_t;

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
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_INTEGER,     "timeout",          SDF_RD,             "1000",     "Timeout"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    json_int_t timeout;
    hgobj timer;

    hgobj pepon;
    hgobj gobj_clitcp;          // pure tcp client under test

    test_phase_t phase;
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

    json_t *kw_pepon = json_pack("{s:b}",
        "do_echo", 1
    );
    priv->pepon = gobj_create_pure_child("server", C_PEPON, kw_pepon, gobj);

    /*
     *  The client under test: a pure secure C_TCP with the inactivity timeout.
     */
    json_t *kw_cli = json_pack("{s:s, s:I, s:b, s:{s:s, s:b}}",
        "url", CLIENT_URL,
        "timeout_inactivity", (json_int_t)TIMEOUT_INACTIVITY,
        "no_tx_ready_event", 1,
        "crypto",
            "library", TLS_LIBRARY_NAME,
            "trace", 0
    );
    priv->gobj_clitcp = gobj_create_pure_child("clitcp", C_TCP, kw_cli, gobj);

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,               gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    if(!gobj_is_running(priv->pepon)) {
        gobj_start(priv->pepon);
    }

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
    if(gobj_is_running(priv->gobj_clitcp)) {
        gobj_stop(priv->gobj_clitcp);
    }
    gobj_stop(priv->pepon);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Bring up the echo server, then give it a moment to listen before
     *  starting the client (with the inactivity model a failed first
     *  connect would not auto-retry).
     */
    gobj_play(priv->pepon);

    /*
     *  The client is a pure child, so C_TCP's mt_create already subscribed us
     *  (the parent) to its output events. Do NOT subscribe again here.
     */

    set_timeout(priv->timer, 1000); // timeout to start the client

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_pause(priv->pepon);

    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Test timer, two uses (both deferred to avoid re-entering the client
 *  from inside its own published-event callbacks):
 *    - PHASE_INIT: start the client once the server is listening
 *    - PHASE_INACTIVITY_CLOSED: now that the client settled in ST_DISCONNECTED,
 *      send data to trigger the on-demand reconnection
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase == PHASE_INIT) {
        if(!gobj_is_running(priv->gobj_clitcp)) {
            gobj_start(priv->gobj_clitcp);
        }
    } else if(priv->phase == PHASE_INACTIVITY_CLOSED) {
        gbuffer_t *gbuf = gbuffer_create(64, 64);
        gbuffer_printf(gbuf, "%s", MESSAGE);
        json_t *kw_tx = json_pack("{s:I}",
            "gbuffer", (json_int_t)(uintptr_t)gbuf
        );
        gobj_send_event(priv->gobj_clitcp, EV_TX_DATA, kw_tx, gobj);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Client connected (first time and after the on-demand reconnection)
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase == PHASE_INIT) {
        priv->phase = PHASE_FIRST_CONNECTED;
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Client connected",
            NULL
        );
        /*
         *  Do nothing: stay silent so the client's inactivity timer fires.
         */
    } else if(priv->phase == PHASE_INACTIVITY_CLOSED) {
        priv->phase = PHASE_RECONNECTED;
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Client reconnected",
            NULL
        );
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Client disconnected: the first one is the inactivity close.
 *  React by sending data while disconnected, which must reconnect on demand.
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase == PHASE_FIRST_CONNECTED) {
        priv->phase = PHASE_INACTIVITY_CLOSED;
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Inactivity disconnected",
            NULL
        );

        /*
         *  Defer the tx: EV_DISCONNECTED is published while the client is
         *  still finishing set_disconnected (not yet in ST_DISCONNECTED).
         *  Sending now would re-enter the client mid-teardown. Let the loop
         *  settle, then send from ac_timeout.
         */
        set_timeout(priv->timer, 200);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Echo received after the on-demand reconnection
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    if(priv->phase == PHASE_RECONNECTED) {
        size_t len = gbuf? gbuffer_leftbytes(gbuf) : 0;
        char *p = gbuf? gbuffer_cur_rd_pointer(gbuf) : NULL;

        if(p && len == strlen(MESSAGE) && memcmp(p, MESSAGE, len)==0) {
            gobj_log_info(gobj, 0,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "Echo received",
                NULL
            );
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Echo mismatch",
                NULL
            );
        }
        priv->phase = PHASE_DONE;
        set_yuno_must_die();
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Optional/teardown events
 ***************************************************************************/
PRIVATE int ac_tx_ready(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
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
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST2);

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
    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_timeout,                 0},
        {EV_CONNECTED,              ac_connected,               0},
        {EV_DISCONNECTED,           ac_disconnected,            0},
        {EV_RX_DATA,                ac_rx_data,                 0},
        {EV_TX_READY,               ac_tx_ready,                0},
        {EV_STOPPED,                ac_stopped,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_CONNECTED,      0},
        {EV_DISCONNECTED,   0},
        {EV_RX_DATA,        0},
        {EV_TX_READY,       0},
        {EV_STOPPED,        0},
        {EV_TIMEOUT,        0},
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
PUBLIC int register_c_test2(void)
{
    return create_gclass(C_TEST2);
}
