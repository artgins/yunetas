/***********************************************************************
 *          c_nodes.js
 *
 *      C_NODES — node picker. Lists the nodes connected to the control
 *      center (`list-agents`) and lets the operator choose the ACTIVE
 *      node; the Console then routes commands to it via command-agent.
 *
 *      The control-center link re-publishes EV_MT_COMMAND_ANSWER to all
 *      panels, so this view filters by the command in the command_stack
 *      and only handles `list-agents` answers (the Console handles
 *      `command-agent`).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA, SDATA_END, data_type_t,
    gclass_create, log_error,
    gobj_parent,
    gobj_read_attr, gobj_read_pointer_attr, gobj_write_attr,
    gobj_subscribe_event,
    gobj_find_service,
    createElement2,
    refresh_language,
    msg_iev_get_stack,
    kw_get_str,
} from "@yuneta/gobj-js";

import i18next from "i18next";

import {agent_link_command, agent_link_is_connected} from "./c_agent_link.js";
import {agent_config_get_active_node, agent_config_set_active_node} from "./c_agent_config.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_NODES";


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",  0,  null,     "Subscriber of output events"),

SDATA(data_type_t.DTP_STRING,   "title",       0,  "nodes",  "View title (i18n key)"),
SDATA(data_type_t.DTP_POINTER,  "$container",  0,  null,     "Root HTMLElement"),
SDATA(data_type_t.DTP_POINTER,  "link_svc",    0,  null,     "C_AGENT_LINK service"),
SDATA(data_type_t.DTP_POINTER,  "config_svc",  0,  null,     "C_AGENT_CONFIG service"),
SDATA(data_type_t.DTP_JSON,     "nodes",       0,  "[]",     "Parsed list-agents result"),
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
     *  CHILD subscription model
     */
    let subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, null, {}, subscriber);

    let link = gobj_find_service("agent_link", true);
    gobj_write_attr(gobj, "link_svc", link);
    if(link) {
        gobj_subscribe_event(link, "EV_ON_OPEN", {}, gobj);
        gobj_subscribe_event(link, "EV_ON_CLOSE", {}, gobj);
        gobj_subscribe_event(link, "EV_MT_COMMAND_ANSWER", {}, gobj);
    }
    gobj_write_attr(gobj, "config_svc", gobj_find_service("agent_config", true));

    let $c = createElement2(["div", {class: "view-card", style: "overflow:auto;"}, []]);
    gobj_write_attr(gobj, "$container", $c);
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    render(gobj);
    request_agents(gobj);
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
    let $c = gobj_read_attr(gobj, "$container");
    if($c && $c.parentNode) {
        $c.parentNode.removeChild($c);
    }
    gobj_write_attr(gobj, "$container", null);
}




                    /***************************
                     *      Local Methods
                     ***************************/




function clear_node($n)
{
    while($n && $n.firstChild) {
        $n.removeChild($n.firstChild);
    }
}

/***************************************************************
 *  Ask the control center for the connected nodes.
 ***************************************************************/
function request_agents(gobj)
{
    let link = gobj_read_attr(gobj, "link_svc");
    if(link && agent_link_is_connected(link)) {
        agent_link_command(link, "list-agents", {}, gobj);
    }
}

/***************************************************************
 *  Parse one list-agents line:
 *  "UUID:<uuid> (<role>,<ver>),  HOSTNAME:'<host>'"
 ***************************************************************/
function parse_agent_line(s)
{
    s = String(s || "");
    let uuid = (/UUID:(\S+)/.exec(s) || [])[1] || "";
    let rv   = /\(([^,]+),\s*([^)]+)\)/.exec(s);
    let host = (/HOSTNAME:'([^']*)'/.exec(s) || [])[1] || "";
    return {
        uuid:    uuid,
        role:    rv ? rv[1].trim() : "",
        version: rv ? rv[2].trim() : "",
        host:    host
    };
}

/***************************************************************
 *  Rebuild the view from the current node list.
 ***************************************************************/
function render(gobj)
{
    let $c = gobj_read_attr(gobj, "$container");
    if(!$c) {
        return;
    }
    clear_node($c);

    let link = gobj_read_attr(gobj, "link_svc");
    let connected = !!(link && agent_link_is_connected(link));
    let config = gobj_read_attr(gobj, "config_svc");
    let active = config ? agent_config_get_active_node(config) : "";
    let nodes = gobj_read_attr(gobj, "nodes");
    if(!Array.isArray(nodes)) {
        nodes = [];
    }

    /*  Header + refresh  */
    $c.appendChild(createElement2(
        ["div", {class: "is-flex is-justify-content-space-between is-align-items-center mb-3"}, [
            ["div", {}, [
                ["h1", {class: "title is-4", style: "color:#2E7CD6;", i18n: "nodes"}, "Nodes"],
                ["p", {class: "subtitle is-6", style: "color:#5B6B7E;", i18n: "nodes subtitle"},
                    "Nodes connected to the control center"]
            ]],
            ["button", {class: "button is-small", type: "button", i18n: "refresh"}, "Refresh",
                {click: () => request_agents(gobj)}]
        ]]
    ));

    if(!connected) {
        $c.appendChild(createElement2(
            ["div", {class: "notification is-light", i18n: "not connected to an agent"},
                "Not connected"]));
        return;
    }
    if(!nodes.length) {
        $c.appendChild(createElement2(
            ["div", {class: "notification is-light", i18n: "no nodes"}, "No nodes connected"]));
        return;
    }

    for(let n of nodes) {
        $c.appendChild(build_node_card(gobj, n, active));
    }
}

/***************************************************************
 *  One node row.
 ***************************************************************/
function build_node_card(gobj, n, active)
{
    let is_active = (active && (active === n.host || active === n.uuid));
    let badge = is_active
        ? ["span", {class: "tag is-success ml-2", i18n: "active"}, "active"]
        : null;

    let action = is_active
        ? null
        : ["button", {class: "button is-small is-success is-light", type: "button", i18n: "select"},
            "Select", {click: () => on_select(gobj, n)}];

    return createElement2(
        ["div", {class: "box p-3 mb-2"}, [
            ["div", {class: "is-flex is-justify-content-space-between is-align-items-flex-start"}, [
                ["div", {}, [
                    ["span", {class: "has-text-weight-bold"}, n.host || n.uuid],
                    badge,
                    ["p", {class: "is-size-7", style: "color:#9AA7B4;"},
                        `${n.role || ""} ${n.version || ""}  ·  ${n.uuid}`]
                ]],
                ["div", {class: "buttons are-small"}, action ? [action] : []]
            ]]
        ]]
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************
 *  Pick a node as active (commands route there).
 ***************************************************************/
function on_select(gobj, n)
{
    let config = gobj_read_attr(gobj, "config_svc");
    if(config) {
        agent_config_set_active_node(config, n.host || n.uuid);
    }
    render(gobj);
}

/***************************************************************
 *  Link in session — fetch the node list.
 ***************************************************************/
function ac_on_open(gobj, event, kw, src)
{
    request_agents(gobj);
    return 0;
}

function ac_on_close(gobj, event, kw, src)
{
    render(gobj);
    return 0;
}

/***************************************************************
 *  Command answer — only handle our own list-agents result.
 ***************************************************************/
function ac_mt_command_answer(gobj, event, kw, src)
{
    let stk = msg_iev_get_stack(gobj, kw, "command_stack", false);
    let command = kw_get_str(gobj, stk, "command", "", 0);
    if(command !== "list-agents") {
        return 0;   /*  belongs to another panel  */
    }

    let data = kw.data;
    let nodes = [];
    if(Array.isArray(data)) {
        for(let line of data) {
            nodes.push(parse_agent_line(line));
        }
    }
    gobj_write_attr(gobj, "nodes", nodes);
    render(gobj);
    return 0;
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
        ["ST_IDLE", [
            ["EV_ON_OPEN",           ac_on_open,           null],
            ["EV_ON_CLOSE",          ac_on_close,          null],
            ["EV_MT_COMMAND_ANSWER", ac_mt_command_answer, null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_ON_OPEN",           0],
        ["EV_ON_CLOSE",          0],
        ["EV_MT_COMMAND_ANSWER", 0]
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
function register_c_nodes()
{
    return create_gclass(GCLASS_NAME);
}

export {register_c_nodes};
