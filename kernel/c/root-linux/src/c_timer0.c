/****************************************************************************
 *          c_timer0.c
 *
 *          GClass Timer
 *          Low level using liburing
 *
 *          WARNING: don't abuse of this gclass, each class instance opens a file!
 *                  Use c_timer better !!!
 *
 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <time.h>
#include <gobj.h>
#include <yev_loop.h>
#include "c_yuno.h"
#include "c_timer0.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int yev_callback(yev_event_h yev_event);

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE const sdata_desc_t tattr_desc[] = {
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
    yev_event_h yev_event;
    BOOL periodic;
    json_int_t msec;
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

    priv->yev_event = yev_create_timer_event(yuno_event_loop(), yev_callback, gobj);

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
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(yev_event_is_running(priv->yev_event)) {
        yev_stop_event(priv->yev_event);
    }

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
PRIVATE int yev_callback(yev_event_h yev_event)
{
    hgobj gobj = yev_get_gobj(yev_event);
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint32_t level = priv->periodic? TRACE_TIMER_PERIODIC:TRACE_TIMER;
    BOOL tracea = is_level_tracing(gobj, level) && !is_level_not_tracing(gobj, level);

    if(tracea) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_get_flag(yev_event));
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "hard timeout got",
            "msg2",         "%s", "⏰⏰ ✅✅ hard timeout got",
            "type",         "%s", yev_event_type_name(yev_event),
            "state",        "%s", yev_get_state_name(yev_event),
            "fd",           "%d", yev_get_fd(yev_event),
            "result",       "%d", yev_get_result(yev_event),
            "sres",         "%s", (yev_get_result(yev_event)<0)? strerror(-yev_get_result(yev_event)):"",
            "p",            "%p", yev_event,
            "flag",         "%j", jn_flags,
            "periodic",     "%d", priv->periodic?1:0,
            "msec",         "%ld", (long)priv->msec,
            "publish",      "%s", yev_event_is_stopped(yev_event)? "No":"Yes",
            NULL
        );
        json_decref(jn_flags);
    }

    if(!yev_event_is_stopped(yev_event)) {
        if(priv->periodic) {
            gobj_send_event(gobj, EV_TIMEOUT_PERIODIC, 0, gobj);
        } else {
            gobj_send_event(gobj, EV_TIMEOUT, 0, gobj);
        }
    } else {
        gobj_send_event(gobj, EV_STOPPED, 0, gobj);
    }

    return gobj_is_running(gobj)?0:-1;
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
        return gobj_send_event(gobj_parent(gobj), event, kw, gobj); // reuse kw
    } else {
        return gobj_publish_event(gobj, event, kw); // reuse kw
    }
}

/***************************************************************************
 *  Resend to the parent
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_pure_child(gobj)) {
        return gobj_send_event(gobj_parent(gobj), event, kw, gobj); // reuse kw
    } else {
        return gobj_publish_event(gobj, event, kw); // reuse kw
    }
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
GOBJ_DEFINE_GCLASS(C_TIMER0);

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
        {EV_TIMEOUT,                ac_timeout,         0},
        {EV_TIMEOUT_PERIODIC,       ac_timeout,         0},
        {EV_STOPPED,                ac_stopped,         0},
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
        tattr_desc,
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
PUBLIC int register_c_timer0(void)
{
    return create_gclass(C_TIMER0);
}




                    /***************************
                     *      Helpers
                     ***************************/




/***************************************************************************
 *  Set timeout
 ***************************************************************************/
PUBLIC void set_timeout0(hgobj gobj, json_int_t msec)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_typeof_gclass(gobj, C_TIMER0)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "set_timeout0() must be used only in C_TIMER0",
            NULL
        );
        return;
    }

    uint32_t level = TRACE_TIMER;
    BOOL tracea = is_level_tracing(gobj, level) && !is_level_not_tracing(gobj, level);

    gobj_write_integer_attr(gobj, "msec", msec);
    gobj_write_bool_attr(gobj, "periodic", FALSE);

    if(tracea) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_get_flag(priv->yev_event));
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "set_timeout",
            "msg2",         "%s", "⏰⏰ 🟦 set_timeout",
            "type",         "%s", yev_event_type_name(priv->yev_event),
            "fd",           "%d", yev_get_fd(priv->yev_event),
            "p",            "%p", priv->yev_event,
            "flag",         "%j", jn_flags,
            "periodic",     "%d", priv->periodic?1:0,
            "msec",         "%ld", (long)priv->msec,
            NULL
        );
        json_decref(jn_flags);
    }

    yev_start_timer_event(priv->yev_event, msec, FALSE);
}

/***************************************************************************
 *  Set periodic timeout
 ***************************************************************************/
PUBLIC void set_timeout_periodic0(hgobj gobj, json_int_t msec)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_typeof_gclass(gobj, C_TIMER0)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "set_timeout_periodic0() must be used only in C_TIMER0",
            NULL
        );
        return;
    }

    uint32_t level = TRACE_TIMER_PERIODIC;
    BOOL tracea = is_level_tracing(gobj, level) && !is_level_not_tracing(gobj, level);

    gobj_write_integer_attr(gobj, "msec", msec);
    gobj_write_bool_attr(gobj, "periodic", TRUE);

    if(tracea) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_get_flag(priv->yev_event));
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "set_timeout_periodic",
            "msg2",         "%s", "⏰⏰ 🟦🟦 set_timeout_periodic",
            "type",         "%s", yev_event_type_name(priv->yev_event),
            "fd",           "%d", yev_get_fd(priv->yev_event),
            "p",            "%p", priv->yev_event,
            "flag",         "%j", jn_flags,
            "periodic",     "%d", priv->periodic?1:0,
            "msec",         "%ld", (long)priv->msec,
            NULL
        );
        json_decref(jn_flags);
    }

    yev_start_timer_event(priv->yev_event, msec, TRUE);
}

/***************************************************************************
 *  Clear timeout
 ***************************************************************************/
PUBLIC void clear_timeout0(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_typeof_gclass(gobj, C_TIMER0)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "clear_timeout0() must be used only in C_TIMER0",
            NULL
        );
        return;
    }

    uint32_t level = priv->periodic? TRACE_TIMER_PERIODIC:TRACE_TIMER;
    BOOL tracea = is_level_tracing(gobj, level) && !is_level_not_tracing(gobj, level);

    if(tracea) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_get_flag(priv->yev_event));
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "clear_timeout",
            "msg2",         "%s", "⏰⏰ ❎ clear_timeout",
            "type",         "%s", yev_event_type_name(priv->yev_event),
            "fd",           "%d", yev_get_fd(priv->yev_event),
            "p",            "%p", priv->yev_event,
            "flag",         "%j", jn_flags,
            "periodic",     "%d", priv->periodic?1:0,
            "msec",         "%ld", (long)priv->msec,
            "state",        "%d", yev_get_state(priv->yev_event),
            NULL
        );
        json_decref(jn_flags);
    }

    yev_stop_event(priv->yev_event);
}
