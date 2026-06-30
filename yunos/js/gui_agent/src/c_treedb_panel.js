/***********************************************************************
 *          c_treedb_panel.js
 *
 *      C_TREEDB_PANEL — browse a treedb that lives on a managed yuno,
 *      through the agent, as a table. A routed stage view.
 *
 *      It uses the shared C_AGENT_LINK (single connection to the agent)
 *      and a per-tab C_TREEDB_GATE adapter that wraps the reused
 *      C_YUI_TREEDB_TOPICS component's raw treedb commands into the
 *      agent's `command-yuno` so they reach the target yuno's C_NODE
 *      service. Mount happens once the shared link is in session.
 *
 *      Target (yuno_id + treedb_service) currently comes from the route
 *      kw; the discovery + picker + tabs land next.
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
    gobj_create_service,
    gobj_start, gobj_stop_tree, gobj_destroy,
    gobj_send_event,
    createElement2,
} from "@yuneta/gobj-js";

import {t} from "i18next";

import {agent_link_is_connected} from "./c_agent_link.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_TREEDB_PANEL";


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",     0,  null,      "Subscriber of output events"),

SDATA(data_type_t.DTP_STRING,   "title",          0,  "treedb",  "View title (i18n key)"),
SDATA(data_type_t.DTP_STRING,   "mode",           0,  "table",   "table | graph"),
SDATA(data_type_t.DTP_STRING,   "yuno_id",        0,  "",        "Target managed yuno id"),
SDATA(data_type_t.DTP_STRING,   "treedb_service", 0,  "",        "Target C_NODE service name"),
SDATA(data_type_t.DTP_POINTER,  "$container",     0,  null,      "Root HTMLElement"),
SDATA(data_type_t.DTP_POINTER,  "link_svc",       0,  null,      "C_AGENT_LINK service"),
SDATA(data_type_t.DTP_POINTER,  "gate",           0,  null,      "C_TREEDB_GATE adapter"),
SDATA(data_type_t.DTP_POINTER,  "component",      0,  null,      "C_YUI_TREEDB_TOPICS"),
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
        gobj_subscribe_event(link, "EV_ON_OPEN_ERROR", {}, gobj);
        gobj_subscribe_event(link, "EV_ON_ID_NAK", {}, gobj);
    }

    let $c = createElement2(["div", {class: "view-card", style: "display:flex; flex-direction:column; height:100%; padding:0;"}, []]);
    gobj_write_attr(gobj, "$container", $c);
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    let link = gobj_read_attr(gobj, "link_svc");
    if(link && agent_link_is_connected(link)) {
        mount(gobj);
    } else {
        show_message(gobj, t("connecting") + "…");
    }
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
    unmount(gobj);
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
 *  Centered message (no component mounted).
 ***************************************************************/
function show_message(gobj, text)
{
    unmount(gobj);
    let $c = gobj_read_attr(gobj, "$container");
    if(!$c) {
        return;
    }
    clear_node($c);
    $c.appendChild(createElement2(
        ["div", {class: "notification is-light m-4", style: "text-align:center;"}, text]
    ));
}

/***************************************************************
 *  Create the gate + treedb component and mount it.
 ***************************************************************/
function mount(gobj)
{
    unmount(gobj);

    let link = gobj_read_attr(gobj, "link_svc");
    let yuno_id = gobj_read_attr(gobj, "yuno_id");
    let treedb_service = gobj_read_attr(gobj, "treedb_service");

    if(!yuno_id || !treedb_service) {
        show_message(gobj, t("pick a treedb"));
        return;
    }

    let key = `${yuno_id}_${treedb_service}`;

    /*  Per-tab adapter: wraps the component's raw treedb commands into
     *  command-yuno toward (yuno_id, treedb_service) over the link. */
    let gate = gobj_create_service(`treedb_gate_${key}`, "C_TREEDB_GATE", {
        link_svc:       link,
        yuno_id:        yuno_id,
        treedb_service: treedb_service
    }, gobj);
    gobj_write_attr(gobj, "gate", gate);
    gobj_start(gate);

    /*  The reused table component talks to the gate as its remote. */
    let mode = gobj_read_attr(gobj, "mode");
    let gclass = (mode === "graph") ? "C_YUI_TREEDB_GRAPH" : "C_YUI_TREEDB_TOPICS";
    let component = gobj_create_service(`treedb_view_${key}`, gclass, {
        gobj_remote_yuno: gate,
        treedb_name:      treedb_service,
        subscriber:       gobj
    }, gobj);
    gobj_write_attr(gobj, "component", component);
    gobj_write_attr(gate, "component", component);

    let $c = gobj_read_attr(gobj, "$container");
    clear_node($c);
    let $inner = gobj_read_attr(component, "$container");
    if($inner) {
        $c.appendChild($inner);
    }

    gobj_start(component);
    gobj_send_event(component, "EV_SHOW", {}, gobj);
}

/***************************************************************
 *  Destroy the component + gate.
 ***************************************************************/
function unmount(gobj)
{
    let component = gobj_read_attr(gobj, "component");
    if(component) {
        gobj_write_attr(gobj, "component", null);
        gobj_stop_tree(component);
        gobj_destroy(component);
    }
    let gate = gobj_read_attr(gobj, "gate");
    if(gate) {
        gobj_write_attr(gobj, "gate", null);
        gobj_stop_tree(gate);
        gobj_destroy(gate);
    }
}




                    /***************************
                     *      Actions
                     ***************************/




function ac_on_open(gobj, event, kw, src)
{
    mount(gobj);
    return 0;
}

function ac_on_close(gobj, event, kw, src)
{
    show_message(gobj, t("disconnected"));
    return 0;
}

function ac_on_open_error(gobj, event, kw, src)
{
    show_message(gobj, `${t("cannot connect")}: ${kw.url || ""}`);
    return 0;
}

function ac_on_id_nak(gobj, event, kw, src)
{
    show_message(gobj, kw.comment || t("authentication required"));
    return 0;
}

/***************************************************************
 *  Component output events — no-op (the component owns its UX).
 ***************************************************************/
function ac_noop(gobj, event, kw, src)
{
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
            ["EV_ON_OPEN",              ac_on_open,       null],
            ["EV_ON_CLOSE",             ac_on_close,      null],
            ["EV_ON_OPEN_ERROR",        ac_on_open_error, null],
            ["EV_ON_ID_NAK",            ac_on_id_nak,     null],

            /*  bubbled up from the treedb component */
            ["EV_TOPIC_SELECTED",       ac_noop,          null],
            ["EV_MT_COMMAND_ANSWER",    ac_noop,          null],
            ["EV_TREEDB_NODE_CREATED",  ac_noop,          null],
            ["EV_TREEDB_NODE_UPDATED",  ac_noop,          null],
            ["EV_TREEDB_NODE_DELETED",  ac_noop,          null],
            ["EV_OPERATION_MODE_CHANGED", ac_noop,        null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_ON_OPEN",                0],
        ["EV_ON_CLOSE",               0],
        ["EV_ON_OPEN_ERROR",          0],
        ["EV_ON_ID_NAK",              0],
        ["EV_TOPIC_SELECTED",         0],
        ["EV_MT_COMMAND_ANSWER",      0],
        ["EV_TREEDB_NODE_CREATED",    0],
        ["EV_TREEDB_NODE_UPDATED",    0],
        ["EV_TREEDB_NODE_DELETED",    0],
        ["EV_OPERATION_MODE_CHANGED", 0]
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
function register_c_treedb_panel()
{
    return create_gclass(GCLASS_NAME);
}

export {register_c_treedb_panel};
