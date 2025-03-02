/****************************************************************************
 *          c_yuno.js
 *
 *          The default yuno in js
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved. ****************************************************************************/
import {
    SDATA,
    SDATA_END,
    data_type_t,
    gclass_create,
    gobj_create_pure_child,
    gobj_start,
    gobj_stop,
    gobj_play,
    gobj_pause,
    gobj_is_playing,
    gobj_is_running,
    gobj_default_service,
    gobj_stop_childs,
    gobj_name,
    gobj_read_attr,
} from "./gobj.js";

import {
    log_error,
    trace_msg,
} from "./utils.js";

import {
    set_timeout_periodic
} from "./c_timer.js";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUNO";

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
const attrs_table = [
SDATA(data_type_t.DTP_BOOLEAN,  "changesLost",          0,  false,  "Set true to warn about leaving page."),
SDATA(data_type_t.DTP_STRING,   "yuno_name",            0,  "",     "Yuno name"),
SDATA(data_type_t.DTP_STRING,   "yuno_role",            0,  "",     "Yuno role"),
SDATA(data_type_t.DTP_STRING,   "yuno_version",         0,  "",     "Yuno version"),
SDATA(data_type_t.DTP_INTEGER,  "tracing",              0,  0,      "Tracing level"),
SDATA(data_type_t.DTP_INTEGER,  "trace_timer",          0,  0,      "Trace timers"),
SDATA(data_type_t.DTP_INTEGER,  "trace_inter_event",    0,  0,      "Trace traffic"),
SDATA(data_type_t.DTP_POINTER,  "trace_ievent_callback",0,  null,   "Trace traffic callback"),
SDATA(data_type_t.DTP_INTEGER,  "trace_creation",       0,  0,      "Trace creation"),
SDATA(data_type_t.DTP_INTEGER,  "trace_i18n",           0,  0,      "Trace i18n"),
SDATA(data_type_t.DTP_INTEGER,  "periodic_timeout",     0,  "1000", "Timeout periodic, in miliseconds."),
SDATA_END()
];

let PRIVATE_DATA = {
    gobj_timer: null,
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
     *  Create childs
     */
    gobj.priv.gobj_timer = gobj_create_pure_child(gobj_name(gobj), "C_TIMER", {}, gobj);

    priv.periodic_timeout   = gobj_read_attr(gobj, "periodic_timeout");
}

/***************************************************************
 *          Framework Method: Writing
 ***************************************************************/
function mt_writing(gobj, path)
{
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    gobj_start(gobj.priv.gobj_timer);
    set_timeout_periodic(gobj.priv.gobj_timer, gobj.priv.periodic_timeout);
    return 0;
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    /*
     *  When yuno stops, it's the death of the app
     */
    gobj_stop(gobj.priv.gobj_timer);
    gobj_stop_childs(gobj);
    return 0;
}

/***************************************************************
 *          Framework Method: Play
 ***************************************************************/
function mt_play(gobj)
{
    if(!gobj_is_running(gobj_default_service())) {
        gobj_start(gobj_default_service());
    }
    if(!gobj_is_playing(gobj_default_service())) {
        gobj_play(gobj_default_service());
    }
    return 0;
}

/***************************************************************
 *          Framework Method: Pause
 ***************************************************************/
function mt_pause(gobj)
{
    gobj_pause(gobj_default_service());
    return 0;
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************
 *
 ***************************************************************/




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************
 *
 ***************************************************************/
function ac_timeout(gobj, event, kw, src)
{
    trace_msg("ac_timeout");
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
    mt_play:    mt_play,
    mt_pause:   mt_pause,
};


/***************************************************************
 *          Create the GClass
 ***************************************************************/
function create_gclass(gclass_name)
{
    if(__gclass__) {
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
        ["ST_IDLE",     st_idle]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_TIMEOUT_PERIODIC",     0]
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
        0   // gclass_flag
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
function register_c_yuno()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yuno };
