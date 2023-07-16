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
PRIVATE int yev_timer_callback(yev_event_t *yev_event);

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
    yev_event_t *yev_event;
    BOOL periodic;
    json_int_t msec;
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

    priv->yev_event = yev_create_timer_event(yuno_event_loop(), yev_timer_callback, gobj);

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

    if(priv->yev_event) {
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
PRIVATE int yev_timer_callback(yev_event_t *yev_event)
{
    hgobj gobj = yev_event->gobj;
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint32_t level = priv->periodic? TRACE_PERIODIC_TIMER:TRACE_TIMER;
    BOOL tracea = is_level_tracing(gobj, level) && !is_level_not_tracing(gobj, level);

    if(tracea) {
        json_t *jn_flags = bits2str(yev_flag_strings(), yev_event->flag);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "timeout got",
            "msg2",         "%s", "⏰⏰ ✅✅ timeout got",
            "type",         "%s", yev_event_type_name(yev_event),
            "fd",           "%d", yev_event->fd,
            "result",       "%d", yev_event->result,
            "sres",         "%s", (yev_event->result<0)? strerror(-yev_event->result):"",
            "p",            "%p", yev_event,
            "flag",         "%j", jn_flags,
            "periodic",     "%d", priv->periodic?1:0,
            "msec",         "%ld", (long)priv->msec,
            "publish",      "%s", (yev_event->result > 0)? "Yes":"No",
            NULL
        );
        json_decref(jn_flags);
    }

    if(yev_event->result > 0) {
        if(priv->periodic) {
            gobj_send_event(gobj, EV_TIMEOUT_PERIODIC, 0, gobj);
        } else {
            gobj_send_event(gobj, EV_TIMEOUT, 0, gobj);
        }
    } else {
        if(yev_event->result !=0 && yev_event->result != -ECANCELED) {
            json_t *jn_flags = bits2str(yev_flag_strings(), yev_event->flag);
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_LIBUV_ERROR,
                "msg",          "%s", "timer FAILED",
                "type",         "%s", yev_event_type_name(yev_event),
                "fd",           "%d", yev_event->fd,
                "errno",        "%d", -yev_event->result,
                "strerror",     "%s", strerror(-yev_event->result),
                "p",            "%p", yev_event,
                "flag",         "%j", jn_flags,
                "periodic",     "%d", priv->periodic?1:0,
                "msec",         "%ld", (long)priv->msec,
                NULL
            );
            json_decref(jn_flags);
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

    event_type_t event_types[] = {
        {EV_TIMEOUT,            EVF_OUTPUT_EVENT},
        {EV_TIMEOUT_PERIODIC,   EVF_OUTPUT_EVENT},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    gclass = gclass_create(
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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint32_t level = TRACE_TIMER;
    BOOL tracea = is_level_tracing(gobj, level) && !is_level_not_tracing(gobj, level);

    gobj_write_integer_attr(gobj, "msec", msec);
    gobj_write_bool_attr(gobj, "periodic", FALSE);

    if(tracea) {
        json_t *jn_flags = bits2str(yev_flag_strings(), priv->yev_event->flag);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "set_timeout",
            "msg2",         "%s", "⏰⏰ 🟦 set_timeout",
            "type",         "%s", yev_event_type_name(priv->yev_event),
            "fd",           "%d", priv->yev_event->fd,
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
PUBLIC void set_timeout_periodic(hgobj gobj, json_int_t msec)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint32_t level = TRACE_PERIODIC_TIMER;
    BOOL tracea = is_level_tracing(gobj, level) && !is_level_not_tracing(gobj, level);

    gobj_write_integer_attr(gobj, "msec", msec);
    gobj_write_bool_attr(gobj, "periodic", TRUE);

    if(tracea) {
        json_t *jn_flags = bits2str(yev_flag_strings(), priv->yev_event->flag);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "set_timeout_periodic",
            "msg2",         "%s", "⏰⏰ 🟦🟦 set_timeout_periodic",
            "type",         "%s", yev_event_type_name(priv->yev_event),
            "fd",           "%d", priv->yev_event->fd,
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
PUBLIC void clear_timeout(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    uint32_t level = priv->periodic? TRACE_PERIODIC_TIMER:TRACE_TIMER;
    BOOL tracea = is_level_tracing(gobj, level) && !is_level_not_tracing(gobj, level);

    if(tracea) {
        json_t *jn_flags = bits2str(yev_flag_strings(), priv->yev_event->flag);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "clear_timeout",
            "msg2",         "%s", "⏰⏰ ❎ clear_timeout",
            "type",         "%s", yev_event_type_name(priv->yev_event),
            "fd",           "%d", priv->yev_event->fd,
            "p",            "%p", priv->yev_event,
            "flag",         "%j", jn_flags,
            "periodic",     "%d", priv->periodic?1:0,
            "msec",         "%ld", (long)priv->msec,
            "exec stop ev", "%s", yev_event_in_ring(priv->yev_event)? "Yes":"No",
            NULL
        );
        json_decref(jn_flags);
    }

    yev_stop_event(priv->yev_event);
}
