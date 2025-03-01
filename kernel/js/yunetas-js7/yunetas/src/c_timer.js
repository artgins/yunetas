/****************************************************************************
 *          c_timer.js
 *
 *          GClass Timer
 *          High-level, feed timers from periodic time of yuno
 *          IN SECONDS! although the parameter is in milliseconds (msec)
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/

import {
    SDATA,
    SDATA_END,
    data_type_t,
    gclass_flag_t,
    event_flag_t,
    gclass_create,
    gobj_parent,
    gobj_yuno,
    gobj_send_event,
    gobj_publish_event,
    gobj_read_bool_attr,
    gobj_read_integer_attr,
    gobj_read_pointer_attr,
} from "./gobj.js";

import {
    log_error,
} from "./utils.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_TIMER";

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Configuration (C attributes)
 *---------------------------------------------*/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",   0,  null,   "Subscriber of output events"),
SDATA(data_type_t.DTP_BOOLEAN,  "periodic",     0,  0,      "True for periodic timeouts"),
SDATA(data_type_t.DTP_INTEGER,  "msec",         0,  0,      "Timeout in milliseconds"),
SDATA_END()
];

let PRIVATE_DATA = {
    periodic:   false,
    msec:       0,
    timer_id:   null,
};

let __gclass__ = null;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************
 *          Framework Method: Create
 ***************************************************************/
function mt_create(gobj)
{
    let priv = gobj.priv;

    priv.periodic = gobj_read_bool_attr(gobj, "periodic");
    priv.msec     = gobj_read_integer_attr(gobj, "msec");
}

/***************************************************************
 *          Framework Method: Writing
 ***************************************************************/
function mt_writing(gobj, path)
{
    let priv = gobj.priv;

    switch(path) {
        case "periodic":
            priv.periodic = gobj_read_bool_attr(gobj, "periodic");
            break;
        case "msec":
            priv.msec = gobj_read_integer_attr(gobj, "msec");
            if (priv.timer_id !== -1) {
                clearTimeout(priv.timer_id);
                priv.timer_id = -1;
            }

            if (priv.msec > 0) {
                priv.timer_id = setTimeout(
                    function() {
                        priv.timer_id = -1;
                        const subscriber = gobj_read_pointer_attr(gobj, "subscriber");
                        let ev = priv.periodic ? "EV_TIMEOUT_PERIODIC" : "EV_TIMEOUT";

                        if (gobj_is_pure_child(gobj)) {
                            gobj_send_event(gobj_parent(gobj), ev, {}, gobj);
                        } else {
                            gobj_publish_event(gobj, ev, {});
                        }


                        gobj_send_event(subscriber, "EV_TIMEOUT", null, gobj);
                        gobj.inject_event(gobj.timer_event_name);
                        if(priv.periodic) {

                        }
                    },
                    priv.msec
                );
            }
            break;
    }
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    gobj_subscribe_event(gobj_yuno(), "EV_TIMEOUT_PERIODIC", 0, gobj);

    if (gobj.priv.msec > 0) {
        gobj.priv.t_flush = start_msectimer(gobj.priv.msec);
    }
    return 0;
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    gobj.priv.t_flush = 0;
    gobj_unsubscribe_event(gobj_yuno(), "EV_TIMEOUT_PERIODIC", 0, gobj);

    return 0;
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************
 *          Action: Timeout
 ***************************************************************/
function ac_timeout(gobj, event, kw, src)
{
    let ev = gobj.priv.periodic ? "EV_TIMEOUT_PERIODIC" : "EV_TIMEOUT";

    if (gobj.priv.msec > 0) {
        if (test_msectimer(gobj.priv.t_flush)) {
            if (gobj.priv.periodic) {
                gobj.priv.t_flush = start_msectimer(gobj.priv.msec);
            } else {
                gobj.priv.t_flush = 0;
            }

            if (gobj_is_pure_child(gobj)) {
                gobj_send_event(gobj_parent(gobj), ev, json_incref(kw), gobj);
            } else {
                gobj_publish_event(gobj, ev, json_incref(kw));
            }
        }
    }

    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
const gmt = {
    mt_create:  mt_create,
    mt_writing: mt_writing,
    mt_start:   mt_start,
    mt_stop:    mt_stop,
    mt_destroy: mt_destroy,
};

/***************************************************************
 *          Create the GClass
 ***************************************************************/
function create_gclass(gclass_name)
{
    if (__gclass__) {
        log_error(`GClass ALREADY created: ${gclass_name}`);
        return -1;
    }

    /*---------------------------------------------*
     *          States
     *---------------------------------------------*/
    const st_idle = [
        ["EV_TIMEOUT_PERIODIC",     ac_timeout,     null]
    ];

    const states = [
        ["ST_IDLE",   st_idle]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_TIMEOUT",          event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_TIMEOUT_PERIODIC", event_flag_t.EVF_OUTPUT_EVENT]
    ];

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        gmt,
        0,               // lmt,
        attrs_table,
        PRIVATE_DATA,
        0,               // authz_table,
        0,               // command_table,
        0,               // s_user_trace_level
        gclass_flag_t.gcflag_manual_start // gclass_flag
    );
    if (!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *          Register GClass
 ***************************************************************************/
function register_c_timer()
{
    return create_gclass(GCLASS_NAME);
}




                    /***************************
                     *      Helpers
                     ***************************/




/***************************************************************************
 *  Set timeout
 ***************************************************************************/
function set_timeout(gobj, msec)
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

    gobj_write_bool_attr(gobj, "periodic", FALSE);
    gobj_write_integer_attr(gobj, "msec", msec);    // This write launch timer
}

/***************************************************************************
 *  Set periodic timeout
 ***************************************************************************/
function set_timeout_periodic(gobj, msec)
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

    gobj_write_bool_attr(gobj, "periodic", TRUE);
    gobj_write_integer_attr(gobj, "msec", msec);    // This write launch timer
}

/***************************************************************************
 *  Clear timeout
 ***************************************************************************/
function clear_timeout(gobj)
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

export { register_c_timer, set_timeout, set_timeout_periodic, clear_timeout };
