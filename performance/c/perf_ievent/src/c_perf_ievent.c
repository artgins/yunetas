/***********************************************************************
 *          c_perf_ievent.c
 *          Perf_ievent GClass.
 *
 *          The client of the inter-yuno round trips of perf_ievent.
 *
 *          Subscribes EV_PERF_PONG of the remote service `echo` through its
 *          C_IEVENT_CLI, and starts its tree. When the session opens (the
 *          C_IEVENT_CLI sends its subscriptions after EV_ON_OPEN, so the first
 *          ping goes on the next cycle) it sends EV_PERF_PING, and sends the
 *          next one when the EV_PERF_PONG of the previous one comes back,
 *          MAX_ROUND_TRIPS times, timed. A session closed before the end is
 *          an error.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include "c_perf_ievent.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define MAX_ROUND_TRIPS 100000

/*
 *  99 bytes of json, of the size of the message of perf_c_tcp (132)
 */
#define PAYLOAD "{\"device\": \"0000000001\", \"t\": 1729600000, " \
    "\"v\": [230.1, 230.4, 229.8], \"i\": [1.2, 1.1, 0.9], \"q\": 1}"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int send_ping(hgobj gobj);
GOBJ_DECLARE_EVENT(EV_PERF_START);  // private: the first ping, on the next cycle

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
    hgobj cli;
    json_int_t round_trips;
} PRIVATE_DATA;





                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
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
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->cli = gobj_find_service("cli", TRUE);
    gobj_subscribe_event(priv->cli, NULL, 0, gobj);         // its own events: open, close
    gobj_subscribe_event(priv->cli, EV_PERF_PONG, 0, gobj); // the remote one
    gobj_start_tree(priv->cli);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    return 0;
}




                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The ping, to the remote service `echo`
 ***************************************************************************/
PRIVATE int send_ping(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return gobj_send_event(
        priv->cli,
        EV_PERF_PING,
        json_pack("{s:I, s:s, s:s}",
            "n", priv->round_trips,
            "topic", "measures",
            "payload", PAYLOAD
        ),
        gobj
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The session is open: start on the next cycle, after the subscriptions
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->round_trips = 0;
    gobj_post_event(gobj, EV_PERF_START, 0, gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->round_trips < MAX_ROUND_TRIPS) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_APP,
            "msg",          "%s", "perf_ievent FAILED: session closed before the last round trip",
            "round_trips",  "%ld", (long)priv->round_trips,
            NULL
        );
        set_yuno_must_die();
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_start(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    MT_START_TIME(time_measure)
    send_ping(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_pong(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->round_trips++;
    if(priv->round_trips < MAX_ROUND_TRIPS) {
        send_ping(gobj);
    } else {
        MT_INCREMENT_COUNT(time_measure, MAX_ROUND_TRIPS)
        MT_PRINT_TIME(time_measure, "ievent round trips")
        set_yuno_must_die();
    }

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
GOBJ_DEFINE_GCLASS(C_PERF_IEVENT);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_PERF_PING);
GOBJ_DEFINE_EVENT(EV_PERF_PONG);
GOBJ_DEFINE_EVENT(EV_PERF_START);

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
    ev_action_t st_idle[] = {
        {EV_ON_OPEN,                ac_on_open,             0},
        {EV_ON_CLOSE,               ac_on_close,            0},
        {EV_PERF_START,             ac_start,               0},
        {EV_PERF_PONG,              ac_pong,                0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_PERF_START,             0},
        {EV_PERF_PONG,              EVF_PUBLIC_EVENT},
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
PUBLIC int register_c_perf_ievent(void)
{
    return create_gclass(C_PERF_IEVENT);
}
