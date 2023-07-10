/****************************************************************************
 *          c_timer.c
 *
 *          GClass Timer
 *          Low level linux
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <time.h>
#include <gobj.h>
#include "c_linux_yuno.h"
#include "yunetas_ev_loop.h"
#include "c_timer.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int yev_callback(yev_event_t *yev_event);

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type--------name----------------flag------------default-----description---------- */
SDATA (DTP_INTEGER, "subscriber",       0,              0,          "subscriber of output-events. Default if null is parent."),
SDATA (DTP_BOOLEAN, "periodic",         SDF_RD,         "0",        "True for periodic timeouts"),
SDATA (DTP_INTEGER, "msec",             SDF_RD,         "0",        "Timeout in miliseconds"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    BOOL periodic;
    yev_event_t *yev_event;
} PRIVATE_DATA;

PRIVATE hgclass gclass = 0;





                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    SET_PRIV(periodic,          gobj_read_bool_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(periodic,    gobj_read_bool_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*--------------------------------*
     *      Create timer
     *--------------------------------*/
    priv->yev_event = yev_create_timer_event(yuno_event_loop(), yev_callback, gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->yev_event) {
        yev_stop_event(priv->yev_event);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    time_t msec = (time_t)gobj_read_integer_attr(gobj, "msec");
    BOOL periodic = gobj_read_bool_attr(gobj, "periodic");

    yev_start_timer_event(priv->yev_event, msec, periodic);

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    yev_start_timer_event(priv->yev_event, 0, 0);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->yev_event) {
        yev_destroy_event(priv->yev_event);
        priv->yev_event = NULL;
    }
}




                    /***************************
                     *      Local methods
                     ***************************/




/***************************************************************************
 *  Callback that will be executed when the timer period lapses.
 *  Posts the timer expiry event to the default event loop.
 ***************************************************************************/
PRIVATE int yev_callback(yev_event_t *yev_event)
{
    hgobj gobj = yev_event->gobj;
    BOOL stopped = (yev_event->flag & YEV_STOPPED_FLAG)?TRUE:FALSE;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(stopped) {
        if(!gobj_is_running(gobj)) {
            gobj_send_event(gobj, EV_STOPPED, 0, gobj);
        }
    } else {
        if(priv->periodic) {
            gobj_send_event(gobj, EV_TIMEOUT_PERIODIC, 0, gobj);
        } else {
            gobj_send_event(gobj, EV_TIMEOUT, 0, gobj);
        }
    }
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Resend to the parent
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_pure_child(gobj)) {
        gobj_send_event(gobj_parent(gobj), event, json_incref(kw), gobj);
    } else {
        gobj_publish_event(gobj, event, json_incref(kw));
    }

    JSON_DECREF(kw)
    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TIMER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
// HACK: Systems events: defined in gobj.h
//GOBJ_DEFINE_EVENT(EV_TIMEOUT);
//GOBJ_DEFINE_EVENT(EV_TIMEOUT_PERIODIC);

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    if(gclass) {
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
    ev_action_t st_idle[] = {
        {EV_TIMEOUT,              ac_timeout,         0},
        {EV_TIMEOUT_PERIODIC,     ac_timeout,         0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    gclass = gclass_create(
        gclass_name,
        0,  // event_types
        states,
        &gmt,
        0,  // lmt,
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        0,  // s_user_trace_level
        gcflag_manual_start // gclass_flag
    );
    if(!gclass) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_timer(void)
{
    return create_gclass(C_TIMER);
}




                    /***************************
                     *      Helpers
                     ***************************/




/***************************************************************************
 *  Set timeout
 ***************************************************************************/
PUBLIC void set_timeout(hgobj gobj, json_int_t msec)
{
    if(gobj_get_deep_tracing()>1) {
        trace_machine("⏲ ✅ set_timeout %ld: %s",
            (long)msec,
            gobj_full_name(gobj)
        );
    }

    if(gobj_is_playing(gobj)) { // clear_timeout(gobj);
        gobj_pause(gobj);
    }

    gobj_write_integer_attr(gobj, "msec", msec);
    gobj_write_bool_attr(gobj, "periodic", FALSE);
    gobj_play(gobj);
}

/***************************************************************************
 *  Set periodic timeout
 ***************************************************************************/
PUBLIC void set_timeout_periodic(hgobj gobj, json_int_t msec)
{
    if(gobj_get_deep_tracing()>1) {
        trace_machine("⏲ ⏲ ✅ set_timeout_periodic %ld: %s",
            (long)msec,
            gobj_full_name(gobj)
        );
    }

    if(gobj_is_playing(gobj)) { // clear_timeout(gobj);
        gobj_pause(gobj);
    }

    gobj_write_integer_attr(gobj, "msec", msec);
    gobj_write_bool_attr(gobj, "periodic", TRUE);
    gobj_play(gobj);
}

/***************************************************************************
 *  Clear timeout
 ***************************************************************************/
PUBLIC void clear_timeout(hgobj gobj)
{
    if(gobj_get_deep_tracing()>1) {
        trace_machine("⏲ ❎ clear_timeout: %s",
            gobj_full_name(gobj)
        );
    }

    if(gobj_is_playing(gobj)) {
        gobj_pause(gobj);
    }
}
