/***********************************************************************
 *          c_agent_config.js
 *
 *      C_AGENT_CONFIG — app-level config service (named "agent_config").
 *
 *      In the controlcenter model the SPA does NOT store agent URLs: the
 *      single backend is the control center co-located on this host
 *      (derived in conf/deploy.js), and the operable nodes come live from
 *      `list-agents`. The only persisted choice is which node is active
 *      (its hostname or UUID, as returned by the control center).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA, SDATA_END, data_type_t, sdata_flag_t, event_flag_t,
    gclass_create, log_error,
    gobj_read_attr, gobj_read_pointer_attr, gobj_write_attr,
    gobj_save_persistent_attrs,
    gobj_subscribe_event,
    gobj_publish_event,
} from "@yuneta/gobj-js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_AGENT_CONFIG";


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",   0,                        null, "Subscriber of output events"),
SDATA(data_type_t.DTP_STRING,   "active_node",  sdata_flag_t.SDF_PERSIST, "",   "Active node (hostname/UUID from list-agents)"),
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
    /*
     *  SERVICE subscription model
     */
    const subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, null, {}, subscriber);
    }
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
                     *      Public functions
                     ***************************/




/***************************************************************
 *  Active node (hostname/UUID), or "".
 ***************************************************************/
function agent_config_get_active_node(gobj)
{
    return gobj_read_attr(gobj, "active_node") || "";
}

/***************************************************************
 *  Set the active node, persist it, notify subscribers.
 ***************************************************************/
function agent_config_set_active_node(gobj, node)
{
    gobj_write_attr(gobj, "active_node", node || "");
    gobj_save_persistent_attrs(gobj);
    gobj_publish_event(gobj, "EV_ACTIVE_NODE_CHANGED", {active_node: node || ""});
}




                    /***************************
                     *              FSM
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
        ["ST_IDLE", []]
    ];

    /*---------------------------------------------*
     *          Events
     ***************************************************************/
    const event_types = [
        ["EV_ACTIVE_NODE_CHANGED", event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS]
    ];

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        gmt,
        0,  // lmt
        attrs_table,
        PRIVATE_DATA,
        0,  // authz_table
        0,  // command_table
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
function register_c_agent_config()
{
    return create_gclass(GCLASS_NAME);
}

export {
    register_c_agent_config,
    agent_config_get_active_node,
    agent_config_set_active_node,
};
