/****************************************************************************
 *          c_timer.c
 *
 *          GClass Timer
 *          High level, feed timers from periodic time of yuno
 *          IN SECONDS! although the parameter is in miliseconds (msec)
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <time.h>
#include <gobj.h>
#include "c_timer.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE const sdata_desc_t attrs_table[] = {
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
    json_int_t msec;
    uint64_t t_flush;
} PRIVATE_DATA;

PRIVATE hgclass __gclass__ = 0;





                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    SET_PRIV(periodic,          gobj_read_bool_attr)
    SET_PRIV(msec,              gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(periodic,    gobj_read_bool_attr)
    ELIF_EQ_SET_PRIV(msec,      gobj_read_integer_attr)
        if(priv->msec > 0) {
            priv->t_flush = start_msectimer(priv->msec);
        } else {
            priv->t_flush = 0;
        }
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_subscribe_event(gobj_yuno(), EV_TIMEOUT_PERIODIC, 0, gobj);

    if(priv->msec > 0) {
        priv->t_flush = start_msectimer(priv->msec);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->t_flush = 0;
    gobj_unsubscribe_event(gobj_yuno(), EV_TIMEOUT_PERIODIC, 0, gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}




                    /***************************
                     *      Local methods
                     ***************************/




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Resend to the parent
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_event_t ev = (priv->periodic)? EV_TIMEOUT_PERIODIC : EV_TIMEOUT;

    if(priv->msec > 0) {
        if(test_msectimer(priv->t_flush)) {
            if(priv->periodic) { // Quickly restart to avoid adding the execution time of action
                priv->t_flush = start_msectimer(priv->msec);
            } else {
                priv->t_flush = 0;
            }

            /*
             *  SERVICE subscription model
             */
            if(gobj_is_pure_child(gobj)) {
                gobj_send_event(gobj_parent(gobj), ev, json_incref(kw), gobj);
            } else {
                gobj_publish_event(gobj, ev, json_incref(kw));
            }
        }
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
    ev_action_t st_idle[] = {
        {EV_TIMEOUT_PERIODIC,       ac_timeout,         0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,            EVF_OUTPUT_EVENT},
        {EV_TIMEOUT_PERIODIC,   EVF_OUTPUT_EVENT},
        {EV_STOPPED,            EVF_OUTPUT_EVENT},
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
        0,  // s_user_trace_level
        gcflag_manual_start // gclass_flag
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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_typeof_gclass(gobj, C_TIMER)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "set_timeout() must be used only in C_TIMER",
            NULL
        );
        return;
    }

    uint32_t level = TRACE_TIMER;
    BOOL tracea = is_level_tracing(gobj, level) && !is_level_not_tracing(gobj, level);
    if(tracea) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "set_timeout",
            "msg2",         "%s", "⏰⏰ 🟦 set_timeout",
            "periodic",     "%d", priv->periodic?1:0,
            "msec",         "%ld", (long)priv->msec,
            NULL
        );
    }

    gobj_write_bool_attr(gobj, "periodic", false);
    gobj_write_integer_attr(gobj, "msec", msec);    // This write launch timer
}

/***************************************************************************
 *  Set periodic timeout
 ***************************************************************************/
PUBLIC void set_timeout_periodic(hgobj gobj, json_int_t msec)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_typeof_gclass(gobj, C_TIMER)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "set_timeout_periodic() must be used only in C_TIMER",
            NULL
        );
        return;
    }

    uint32_t level = TRACE_TIMER_PERIODIC;
    BOOL tracea = is_level_tracing(gobj, level) && !is_level_not_tracing(gobj, level);
    if(tracea) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "set_timeout_periodic",
            "msg2",         "%s", "⏰⏰ 🟦🟦 set_timeout_periodic",
            "periodic",     "%d", priv->periodic?1:0,
            "msec",         "%ld", (long)priv->msec,
            NULL
        );
    }

    gobj_write_bool_attr(gobj, "periodic", true);
    gobj_write_integer_attr(gobj, "msec", msec);    // This write launch timer
}

/***************************************************************************
 *  Clear timeout
 ***************************************************************************/
PUBLIC void clear_timeout(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_typeof_gclass(gobj, C_TIMER)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "clear_timeout() must be used only in C_TIMER",
            NULL
        );
        return;
    }

    uint32_t level = priv->periodic? TRACE_TIMER_PERIODIC:TRACE_TIMER;
    BOOL tracea = is_level_tracing(gobj, level) && !is_level_not_tracing(gobj, level);
    if(tracea) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "clear_timeout",
            "msg2",         "%s", "⏰⏰ ❎ clear_timeout",
            "periodic",     "%d", priv->periodic?1:0,
            "msec",         "%ld", (long)priv->msec,
            NULL
        );
    }

    gobj_write_integer_attr(gobj, "msec", 0);    // This write stop timer
}
