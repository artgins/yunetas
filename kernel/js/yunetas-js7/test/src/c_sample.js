/****************************************************************************
 *          c_sample.js
 *
 *          A gclass to test
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved. ****************************************************************************/
import {
    gclass_create,
    log_error,
    gclass_flag_t,
} from "yunetas";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_SAMPLE";

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
    return 0;
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
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
        ["EV_TIMEOUT_PERIODIC",     ac_timeout,         0]
    ];

    const states = [
        ["ST_IDLE",     st_idle]
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
        0,  // lmt,
        CONFIG,
        PRIVATE_DATA,
        0,  // authz_table,
        0,  // command_table,
        0,  // s_user_trace_level
        gclass_flag_t.gcflag_manual_start // gclass_flag
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
function register_c_sample()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_sample };
