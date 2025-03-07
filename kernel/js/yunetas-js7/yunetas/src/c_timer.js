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
    GObj,
    gclass_create,
    gobj_parent,
    gobj_send_event,
    gobj_publish_event,
    gobj_read_bool_attr,
    gobj_read_integer_attr,
    gobj_read_pointer_attr,
    gobj_subscribe_event,
    gobj_is_pure_child,
    gobj_gclass_name,
    gobj_write_bool_attr,
    gobj_write_integer_attr,
} from "./gobj.js";

import {
    log_error,
} from "./helpers.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_TIMER";

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
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
    timer_id:   -1,
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

    /*
     *  SERVICE subscription model
     */
    const subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, null, {}, subscriber);
    }

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
           if(priv.timer_id !== -1) {
                clearTimeout(priv.timer_id);
                priv.timer_id = -1;
            }
            if(priv.msec > 0) {
                priv.timer_id = setTimeout(
                    function() {
                        gobj_send_event(gobj, "EV_TIMEOUT", {}, gobj);
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
    return 0;
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    gobj_write_integer_attr(gobj, "msec", -1);    // This write stops any timer
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
    let priv = gobj.priv;

    let ev = priv.periodic ? "EV_TIMEOUT_PERIODIC" : "EV_TIMEOUT";

    /*
     *  SERVICE subscription model
     */
    if (gobj_is_pure_child(gobj)) {
        gobj_send_event(gobj_parent(gobj), ev, {}, gobj);
    } else {
        gobj_publish_event(gobj, ev, {});
    }

    if(priv.periodic) {
        priv.timer_id = setTimeout(
            function() {
                gobj_send_event(gobj, "EV_TIMEOUT", {}, gobj);
            },
            priv.msec
        );
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
        ["EV_TIMEOUT",      ac_timeout,     null]
    ];

    const states = [
        ["ST_IDLE",   st_idle]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_TIMEOUT",          event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_TIMEOUT_PERIODIC", event_flag_t.EVF_OUTPUT_EVENT],
        [0, 0]
    ];

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        gmt,
        0,  // lmt,
        attrs_table,
        PRIVATE_DATA,
        0,  // authz_table,
        0,  // command_table,
        0,  // s_user_trace_level
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
    if(!(gobj instanceof GObj)) {
        log_error(`not GObj TYPE`);
        return -1;
    }
    if(gobj_gclass_name(gobj) !== "C_TIMER") {
        log_error(`not C_TIMER GObj`);
        return -1;
    }

    gobj_write_bool_attr(gobj, "periodic", false);
    gobj_write_integer_attr(gobj, "msec", msec);    // This write launches the timer
}

/***************************************************************************
 *  Set periodic timeout
 ***************************************************************************/
function set_timeout_periodic(gobj, msec)
{
    if(!(gobj instanceof GObj)) {
        log_error(`not GObj TYPE`);
        return -1;
    }
    if(gobj_gclass_name(gobj) !== "C_TIMER") {
        log_error(`not C_TIMER GObj`);
        return -1;
    }

    gobj_write_bool_attr(gobj, "periodic", true);
    gobj_write_integer_attr(gobj, "msec", msec);    // This write launches the timer
}

/***************************************************************************
 *  Clear timeout
 ***************************************************************************/
function clear_timeout(gobj)
{
    if(!(gobj instanceof GObj)) {
        log_error(`not GObj TYPE`);
        return -1;
    }
    if(gobj_gclass_name(gobj) !== "C_TIMER") {
        log_error(`not C_TIMER GObj`);
        return -1;
    }

    gobj_write_integer_attr(gobj, "msec", -1);    // This write stops the timer
}

export { register_c_timer, set_timeout, set_timeout_periodic, clear_timeout };
