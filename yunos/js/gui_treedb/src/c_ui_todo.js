/***********************************************************************
 *          c_yui_todo.js
 *
 *          Todo
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

import {
    SDATA,
    SDATA_END,
    data_type_t,
    gclass_create,
    log_error
} from "yunetas";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_UI_TODO";

/***************************************************************
 *              Data
 ***************************************************************/
/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",       0,  null,   "Subscriber of output events"),
SDATA(data_type_t.DTP_POINTER,  "$container",       0,  null,   "Container element"),
SDATA(data_type_t.DTP_STRING,   "label",            0,  "todo", "Label"),
SDATA(data_type_t.DTP_STRING,   "icon",             0,  "fab fa-cloudversify", "Icon class"),
SDATA(data_type_t.DTP_INTEGER,  "timeout_retry",    0,  "5",    "Timeout retry in seconds"),
SDATA(data_type_t.DTP_INTEGER,  "timeout_idle",     0,  "5",    "Idle timeout in seconds"),
SDATA_END()
];

let PRIVATE_DATA = {};

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
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
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




/************************************************************
 *
 ************************************************************/
function ac_select(gobj, event, kw, src)
{
    return 0;
}

/************************************************************
 *  {
 *      href: href
 *  }
 ************************************************************/
function ac_show(gobj, event, kw, src)
{
    let href = kw.href;

    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_hide(gobj, event, kw, src)
{
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_timeout(gobj, event, kw, src)
{
    //gobj.set_timeout(1*1000);
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
    mt_start:   mt_start,
    mt_stop:    mt_stop,
    mt_destroy: mt_destroy
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
    const states = [
        ["ST_IDLE", [
            ["EV_SELECT",               ac_select,              null],
            ["EV_SHOW",                 ac_show,                null],
            ["EV_HIDE",                 ac_hide,                null],
            ["EV_TIMEOUT",              ac_timeout,             null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_SELECT",               0],
        ["EV_SHOW",                 0],
        ["EV_HIDE",                 0],
        ["EV_TIMEOUT",              0]
    ];

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
        return -1;
    }

    return 0;
}

/***************************************************************
 *          Register GClass
 ***************************************************************/
function register_c_ui_todo()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_ui_todo };
