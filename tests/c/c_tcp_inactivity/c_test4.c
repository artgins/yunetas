/***********************************************************************
 *          C_TEST4.C
 *
 *          Test C_TCP reconnect-on-tx keeps dl_tx across FAILED reconnects
 *          (inactivity model, clear traffic, no TLS)
 *
 *          Regression for the dl_tx-loss fixed in c_tcp.c. set_disconnected()
 *          used to flush dl_tx on every disconnect, so bytes queued while
 *          disconnected (to be sent on reconnect) were lost if the connect
 *          failed before succeeding. They must instead survive the failed
 *          retries and be delivered once the server is up.
 *
 *          Flow (event-driven)
 *            1) play -> start the client while NO server listens (connect
 *               refused, client retrying).
 *            2) while disconnected, send data: it is queued in dl_tx and a
 *               (failing) reconnect is attempted; the bytes were never put on
 *               the wire and must NOT be dropped.
 *            3) bring the echo server up; the next retry connects and the
 *               queued bytes are flushed and echoed back -> EV_RX_DATA.
 *            4) verify the echo. Pre-fix: dl_tx was flushed on the failed
 *               connect, nothing is sent, no echo, "Echo received" never fires.
 *
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <c_pepon.h>
#include "c_test4.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CLIENT_URL          "tcp://127.0.0.1:7782"
#define TIMEOUT_INACTIVITY  20000   // ms (inactivity model; the value is irrelevant here)
#define TX_DELAY_MS         1500    // send while the client is retrying a failed connect
#define SERVER_DELAY_MS     4000    // bring the server up after more failed retries
#define MESSAGE             "PING-KEPT-TX-RECONNECT"

typedef enum {
    PHASE_INIT = 0,         // client started, server down, retrying connect
    PHASE_TX_QUEUED,        // data sent while disconnected (queued in dl_tx)
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

    gobj_start(priv->timer);    // server brought up LATER (ac_timeout)

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
     *  Start the client with NO server listening: connect refused, retrying.
     */
    priv->phase = PHASE_INIT;
    gobj_start(priv->gobj_clitcp);

    set_timeout(priv->timer, TX_DELAY_MS);  // send data while still disconnected

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
 *  Timer, two uses:
 *    PHASE_INIT      -> send data while the client is disconnected/retrying
 *    PHASE_TX_QUEUED -> bring the echo server up
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase == PHASE_INIT) {
        priv->phase = PHASE_TX_QUEUED;
        gbuffer_t *gbuf = gbuffer_create(64, 64);
        gbuffer_printf(gbuf, "%s", MESSAGE);
        json_t *kw_tx = json_pack("{s:I}",
            "gbuffer", (json_int_t)(uintptr_t)gbuf
        );
        gobj_send_event(priv->gobj_clitcp, EV_TX_DATA, kw_tx, gobj);
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Tx queued while disconnected",
            NULL
        );
        set_timeout(priv->timer, SERVER_DELAY_MS);

    } else if(priv->phase == PHASE_TX_QUEUED) {
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
 *  Client connected (after the server came up and the client kept retrying).
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Echo of the bytes that were queued while disconnected. Their arrival proves
 *  dl_tx survived the failed reconnects.
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    if(priv->phase == PHASE_SERVER_UP) {
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
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)
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
GOBJ_DEFINE_GCLASS(C_TEST4);

/*------------------------*
 *      States / Events
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
PUBLIC int register_c_test4(void)
{
    return create_gclass(C_TEST4);
}
