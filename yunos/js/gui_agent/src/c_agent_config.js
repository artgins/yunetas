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
SDATA(data_type_t.DTP_STRING,   "active_node",  sdata_flag_t.SDF_PERSIST, "",      "Active node (hostname/UUID from list-agents)"),
SDATA(data_type_t.DTP_STRING,   "display_mode", sdata_flag_t.SDF_PERSIST, "table", "Command answer display: table | form (raw JSON)"),
SDATA(data_type_t.DTP_JSON,     "selected_nodes", sdata_flag_t.SDF_PERSIST, "[]",  "Nodes with an open Console tab: [{id, host}, ...]"),
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
    gobj_save_persistent_attrs(gobj, "active_node");
    gobj_publish_event(gobj, "EV_ACTIVE_NODE_CHANGED", {active_node: node || ""});
}

/***************************************************************
 *  Command-answer display mode: "table" (default) or "form"
 *  (raw JSON), mirroring ycommand's display_mode attribute.
 ***************************************************************/
function agent_config_get_display_mode(gobj)
{
    return gobj_read_attr(gobj, "display_mode") || "table";
}

/***************************************************************
 *  Set the display mode and persist it.
 ***************************************************************/
function agent_config_set_display_mode(gobj, mode)
{
    gobj_write_attr(gobj, "display_mode", mode || "table");
    gobj_save_persistent_attrs(gobj, "display_mode");
}

/***************************************************************
 *  Selected nodes — the set of nodes with an open Console tab.
 *  Each entry is {id, host}: id is the agent_id used to route
 *  command-agent; host is the display label.
 ***************************************************************/
function agent_config_get_selected_nodes(gobj)
{
    let list = gobj_read_attr(gobj, "selected_nodes");
    return Array.isArray(list) ? list : [];
}

/***************************************************************
 *  Replace the whole selected-nodes list, persist, and notify.
 ***************************************************************/
function agent_config_set_selected_nodes(gobj, list)
{
    gobj_write_attr(gobj, "selected_nodes", Array.isArray(list) ? list : []);
    gobj_save_persistent_attrs(gobj, "selected_nodes");
    gobj_publish_event(gobj, "EV_SELECTED_NODES_CHANGED",
        {selected_nodes: agent_config_get_selected_nodes(gobj)});
}

/***************************************************************
 *  Is a node id currently selected?
 ***************************************************************/
function agent_config_is_node_selected(gobj, id)
{
    if(!id) {
        return false;
    }
    return agent_config_get_selected_nodes(gobj).some((n) => n && n.id === id);
}

/***************************************************************
 *  Add or remove a node {id, host} from the selection (toggle),
 *  preserving order for the remaining tabs.
 ***************************************************************/
function agent_config_toggle_selected_node(gobj, node)
{
    if(!node || !node.id) {
        return;
    }
    let list = agent_config_get_selected_nodes(gobj).slice();
    let idx = list.findIndex((n) => n && n.id === node.id);
    if(idx >= 0) {
        list.splice(idx, 1);
    } else {
        list.push({id: node.id, host: node.host || node.id});
    }
    agent_config_set_selected_nodes(gobj, list);
}

/***************************************************************
 *  Remove one node id from the selection (used by tab close).
 ***************************************************************/
function agent_config_remove_selected_node(gobj, id)
{
    if(!id) {
        return;
    }
    let list = agent_config_get_selected_nodes(gobj).filter((n) => n && n.id !== id);
    agent_config_set_selected_nodes(gobj, list);
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
        ["EV_ACTIVE_NODE_CHANGED",    event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS],
        ["EV_SELECTED_NODES_CHANGED", event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS]
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
    agent_config_get_display_mode,
    agent_config_set_display_mode,
    agent_config_get_selected_nodes,
    agent_config_set_selected_nodes,
    agent_config_is_node_selected,
    agent_config_toggle_selected_node,
    agent_config_remove_selected_node,
};
