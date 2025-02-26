/****************************************************************************
 *          c_yuno.js
 *
 *          The default yuno in js
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved. ****************************************************************************/
import {
    gclass_create,
    log_error,
    gclass_flag_t,
    trace_msg,
} from "yunetas";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUNO";

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Configuration (C attributes)
 *---------------------------------------------*/
let CONFIG = {
    changesLost: false, // Use with window.onbeforeunload in __yuno__.
                        // Set true to warning about leaving page.
    tracing: 0,
    no_poll: 0,
    trace_timer: 0,
    trace_creation: 0,
    trace_i18n: 0,
    trace_inter_event: 0,       // trace traffic
    trace_ievent_callback: null     // trace traffic
};

let PRIVATE_DATA = {
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
    trace_msg("mt_create");
}

/***************************************************************
 *          Framework Method: Writing
 ***************************************************************/
function mt_writing(gobj, path)
{
    trace_msg("mt_writing");
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    trace_msg("mt_start");
    return 0;
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    trace_msg("mt_stop");
    return 0;
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
    trace_msg("mt_destroy");
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
        ["EV_TIMEOUT",      null,       null]
    ];

    const states = [
        ["ST_IDLE",     st_idle]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_TIMEOUT",          null],
        ["EV_TIMEOUT_PERIODIC", null]
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
        CONFIG,
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
