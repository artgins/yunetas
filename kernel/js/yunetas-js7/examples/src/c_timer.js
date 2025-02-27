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
    gclass_create,
    log_error,
    gobj_read_bool_attr,
    gobj_read_integer_attr,
    gobj_subscribe_event,
    gobj_unsubscribe_event,
    gobj_send_event,
    gobj_publish_event,
    gobj_is_pure_child,
    gobj_parent,
    gobj_yuno,
    start_msectimer,
    test_msectimer,
    json_incref,
    JSON_DECREF,
    gcflag_manual_start
} from "yunetas";

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
let CONFIG = {
    subscriber: null,   // Subscriber of output events, default is parent
    periodic:   false,  // True for periodic timeouts
    msec:       0,      // Timeout in milliseconds
};

let PRIVATE_DATA = {
    periodic:   false,
    msec:       0,
    t_flush:    0
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
    gobj.priv.periodic = gobj_read_bool_attr(gobj, "periodic");
    gobj.priv.msec     = gobj_read_integer_attr(gobj, "msec");
}

/***************************************************************
 *          Framework Method: Writing
 ***************************************************************/
function mt_writing(gobj, path)
{
    gobj.priv.periodic = gobj_read_bool_attr(gobj, "periodic");
    gobj.priv.msec     = gobj_read_integer_attr(gobj, "msec");

    if (gobj.priv.msec > 0) {
        gobj.priv.t_flush = start_msectimer(gobj.priv.msec);
    } else {
        gobj.priv.t_flush = 0;
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

    JSON_DECREF(kw);
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
        ["EV_TIMEOUT_PERIODIC",   ac_timeout,   0]
    ];

    const states = [
        ["ST_IDLE",   st_idle]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_TIMEOUT",          "EVF_OUTPUT_EVENT"],
        ["EV_TIMEOUT_PERIODIC", "EVF_OUTPUT_EVENT"]
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
        CONFIG,
        PRIVATE_DATA,
        0,               // authz_table,
        0,               // command_table,
        0,               // s_user_trace_level
        gcflag_manual_start // gclass_flag
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

export { register_c_timer };
