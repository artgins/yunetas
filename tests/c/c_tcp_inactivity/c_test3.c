/***********************************************************************
 *          C_TEST3.C
 *
 *          Test C_TCP reconnect-with-backoff after a FAILED connect
 *          (inactivity model, clear traffic, no TLS)
 *
 *          Regression for the inactivity-model stall fixed in c_tcp.c: a
 *          pure C_TCP client with timeout_inactivity > 0 used to clear its
 *          timer on ANY disconnect, so a failed connect left it stalled (no
 *          retry, no EV_DISCONNECTED). It must instead retry at
 *          timeout_between_connections until it succeeds.
 *
 *          Flow (event-driven)
 *            1) play -> start the client while NO server listens:
 *               the connect is refused and must keep retrying (~2s apart).
 *            2) after a few seconds, bring the echo server up.
 *            3) the next retry connects                       -> EV_CONNECTED
 *            4) verify and shut down. If the client had stalled on the first
 *               failure (pre-fix), EV_CONNECTED never arrives and the
 *               expected-log FIFO fails.
 *
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <c_pepon.h>
#include "c_test3.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CLIENT_URL          "tcp://127.0.0.1:7781"
/*
 *  Inactivity model (the fix only applies when timeout_inactivity > 0). The
 *  value itself is irrelevant here: the client never stays connected long
 *  enough to idle-close. What matters is the retry cadence on a failed
 *  connect, which is timeout_between_connections (~2s, the default).
 */
#define TIMEOUT_INACTIVITY  20000   // ms
#define SERVER_DELAY_MS     5000    // bring the server up after ~2-3 failed retries

typedef enum {
    PHASE_INIT = 0,         // client started, server down, retrying connect
    PHASE_SERVER_UP,        // server brought up
    PHASE_DONE,
} test_phase_t;

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
     *  The client under test: a pure C_TCP in the inactivity model.
     */
    json_t *kw_cli = json_pack("{s:s, s:I, s:b}",
        "url", CLIENT_URL,
        "timeout_inactivity", (json_int_t)TIMEOUT_INACTIVITY,
        "no_tx_ready_event", 1
    );
    priv->gobj_clitcp = gobj_create_pure_child("clitcp", C_TCP, kw_cli, gobj);

    SET_PRIV(timeout,               gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Only the timer here. The echo server is brought up LATER (ac_timeout)
     *  so the client's first connects fail and exercise the retry path.
     */
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
    if(gobj_is_running(priv->gobj_clitcp)) {
        gobj_stop(priv->gobj_clitcp);
    }
    if(gobj_is_running(priv->pepon)) {
        gobj_stop(priv->pepon);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Start the client with NO server listening: the connect is refused and
     *  the inactivity-model client must keep retrying (timeout_between_connections).
     */
    priv->phase = PHASE_INIT;
    gobj_start(priv->gobj_clitcp);

    /*
     *  Bring the server up after a few failed retries.
     */
    set_timeout(priv->timer, SERVER_DELAY_MS);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    if(gobj_is_playing(priv->pepon)) {
        gobj_pause(priv->pepon);
    }

    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Timer fired: bring the echo server up. By now the client has failed to
 *  connect a few times and must still be retrying.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase == PHASE_INIT) {
        priv->phase = PHASE_SERVER_UP;
        if(!gobj_is_running(priv->pepon)) {
            gobj_start(priv->pepon);
        }
        gobj_play(priv->pepon);
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Server up after retries",
            NULL
        );
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Client connected. This only happens once the server is up AND the client
 *  kept retrying after its earlier failed connects (the regression).
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase == PHASE_SERVER_UP) {
        priv->phase = PHASE_DONE;
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Client connected after retries",
            NULL
        );
        set_yuno_must_die();
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A connect failure does NOT publish EV_DISCONNECTED (the socket never
 *  connected), so this normally won't fire for the retries. Kept as a no-op.
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Optional/teardown events
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

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
GOBJ_DEFINE_GCLASS(C_TEST3);

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
PUBLIC int register_c_test3(void)
{
    return create_gclass(C_TEST3);
}
