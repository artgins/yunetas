/****************************************************************************
 *          c_sample.js
 *
 *          A gclass to test
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved. ****************************************************************************/

import { gclass_create, gobj_read_bool_attr, gobj_read_integer_attr, gobj_subscribe_event, gobj_unsubscribe_event, gobj_send_event, gobj_publish_event, gobj_is_pure_child, gobj_parent, gobj_yuno, start_msectimer, test_msectimer, json_incref, JSON_DECREF, gobj_log_error } from "yunetas";

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
function PRIVATE_DATA() {
    return {
        periodic: false,
        msec: 0,
        t_flush: 0
    };
}

/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
const gmt = {
    mt_create,
    mt_writing,
    mt_start,
    mt_stop,
    mt_destroy
};

/*---------------------------------------------*
 *          Define States
 *---------------------------------------------*/
const st_idle = [
    ["EV_TIMEOUT_PERIODIC", ac_timeout, 0]
];

const states = [
    ["ST_IDLE", st_idle]
];

/*---------------------------------------------*
 *          Define Events
 *---------------------------------------------*/
const event_types = [
    ["EV_TIMEOUT", "EVF_OUTPUT_EVENT"],
    ["EV_TIMEOUT_PERIODIC", "EVF_OUTPUT_EVENT"],
    ["EV_STOPPED", "EVF_OUTPUT_EVENT"]
];

/*---------------------------------------------*
 *          Framework Methods
 *---------------------------------------------*/

function mt_create(gobj) {
    gobj.priv.periodic = gobj_read_bool_attr(gobj, "periodic");
    gobj.priv.msec = gobj_read_integer_attr(gobj, "msec");
}

function mt_writing(gobj, path) {
    let periodic = gobj_read_bool_attr(gobj, "periodic");
    let msec = gobj_read_integer_attr(gobj, "msec");

    if (msec > 0) {
        gobj.priv.t_flush = start_msectimer(msec);
    } else {
        gobj.priv.t_flush = 0;
    }
}

function mt_start(gobj) {
    gobj_subscribe_event(gobj_yuno(), "EV_TIMEOUT_PERIODIC", 0, gobj);

    if (gobj.priv.msec > 0) {
        gobj.priv.t_flush = start_msectimer(gobj.priv.msec);
    }
    return 0;
}

function mt_stop(gobj) {
    gobj.priv.t_flush = 0;
    gobj_unsubscribe_event(gobj_yuno(), "EV_TIMEOUT_PERIODIC", 0, gobj);
    return 0;
}

function mt_destroy(gobj) {
    // Cleanup logic if necessary
}

/*---------------------------------------------*
 *          Actions
 *---------------------------------------------*/

function ac_timeout(gobj, event, kw, src) {
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

/*---------------------------------------------*
 *          Register GClass
 *---------------------------------------------*/

function register_c_timer() {
    return gclass_create("C_TIMER", event_types, states, gmt, 0, [], PRIVATE_DATA);
}

export { register_c_timer };
