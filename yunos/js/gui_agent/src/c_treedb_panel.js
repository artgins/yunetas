/***********************************************************************
 *          c_treedb_panel.js
 *
 *      C_TREEDB_PANEL — browse a yuno's TreeDB as a table or a graph,
 *      a routed stage view (shell mounts it at /treedb/table and
 *      /treedb/graph; the `mode` kw picks the component).
 *
 *      It reuses the proven gobj-ui components C_YUI_TREEDB_TOPICS
 *      (table) and C_YUI_TREEDB_GRAPH (graph). Those need a connected
 *      C_IEVENT_CLI (`gobj_remote_yuno`) + a `treedb_name`; this panel
 *      opens that link to the ACTIVE agent's treedb (the agent serves
 *      `treedb_agentdb`), forwarding the login JWT, and mounts the
 *      component once the session is up.
 *
 *      Lifecycle mirrors C_AGENT_CONSOLE: the C_IEVENT_CLI is parented
 *      to the yuno (always alive) and released in mt_destroy, never in
 *      mt_stop, so a shell stop/start during layout cannot drop it.
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
    gobj_yuno,
    gobj_name,
    gobj_create, gobj_create_service,
    gobj_start, gobj_start_tree, gobj_stop_tree, gobj_destroy,
    gobj_send_event,
    createElement2,
} from "@yuneta/gobj-js";

import i18next from "i18next";

import {agent_config_get_active} from "./c_agent_config.js";
import {agent_login_get_token} from "./c_agent_login.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_TREEDB_PANEL";

const DEFAULT_TREEDB = "treedb_agentdb";


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",  0,  null,       "Subscriber of output events"),

SDATA(data_type_t.DTP_STRING,   "title",       0,  "treedb",   "View title (i18n key)"),
SDATA(data_type_t.DTP_STRING,   "mode",        0,  "table",    "table | graph"),
SDATA(data_type_t.DTP_POINTER,  "$container",  0,  null,       "Root HTMLElement"),
SDATA(data_type_t.DTP_POINTER,  "config_svc",  0,  null,       "C_AGENT_CONFIG service"),
SDATA(data_type_t.DTP_POINTER,  "login_svc",   0,  null,       "C_AGENT_LOGIN service"),
SDATA(data_type_t.DTP_POINTER,  "iev",         0,  null,       "C_IEVENT_CLI to the treedb"),
SDATA(data_type_t.DTP_POINTER,  "component",   0,  null,       "C_YUI_TREEDB_TOPICS / _GRAPH"),
SDATA(data_type_t.DTP_STRING,   "active_label",0,  "",         "Label of the connected agent"),
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

    let config = gobj_find_service("agent_config", true);
    gobj_write_attr(gobj, "config_svc", config);
    if(config) {
        gobj_subscribe_event(config, "EV_AGENTS_CHANGED", {}, gobj);
    }

    let login = gobj_find_service("agent_login", true);
    gobj_write_attr(gobj, "login_svc", login);
    if(login) {
        gobj_subscribe_event(login, "EV_LOGIN_ACCEPTED", {}, gobj);
        gobj_subscribe_event(login, "EV_LOGOUT_DONE", {}, gobj);
    }

    let $c = createElement2(["div", {class: "view-card", style: "display:flex; flex-direction:column; height:100%; padding:0;"}, []]);
    gobj_write_attr(gobj, "$container", $c);
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    connect(gobj);
}

/***************************************************************
 *          Framework Method: Stop
 *
 *  Keep the link alive across shell layout stop/starts (see
 *  C_AGENT_CONSOLE). Released in mt_destroy.
 ***************************************************************/
function mt_stop(gobj)
{
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
    disconnect(gobj);
    let $c = gobj_read_attr(gobj, "$container");
    if($c && $c.parentNode) {
        $c.parentNode.removeChild($c);
    }
    gobj_write_attr(gobj, "$container", null);
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************
 *  Show a centered message in the panel (no component mounted).
 ***************************************************************/
function show_message(gobj, text)
{
    destroy_component(gobj);
    let $c = gobj_read_attr(gobj, "$container");
    if(!$c) {
        return;
    }
    clear_node($c);
    $c.appendChild(createElement2(
        ["div", {class: "notification is-light m-4", style: "text-align:center;"}, text]
    ));
}

function clear_node($n)
{
    while($n && $n.firstChild) {
        $n.removeChild($n.firstChild);
    }
}

/***************************************************************
 *  (Re)open the link to the active agent's treedb.
 ***************************************************************/
function connect(gobj)
{
    disconnect(gobj);

    let config = gobj_read_attr(gobj, "config_svc");
    let active = config ? agent_config_get_active(config) : null;
    if(!active) {
        gobj_write_attr(gobj, "active_label", "");
        show_message(gobj, i18next.t("no active agent"));
        return;
    }

    let treedb_name = active.treedb || DEFAULT_TREEDB;
    gobj_write_attr(gobj, "active_label", active.label);

    let login = gobj_read_attr(gobj, "login_svc");
    let jwt = login ? agent_login_get_token(login) : "";

    /*  Link parented to the yuno (always alive). The identity-card
     *  session is established against the agent's default service (same
     *  as the console — the agent closes the channel if the identity
     *  card targets an arbitrary service); the treedb components then
     *  target `treedb_name` per command via kw.service. The JWT comes
     *  from the login service. */
    let iev = gobj_create("treedb_iev", "C_IEVENT_CLI", {
        url:                 active.url,
        remote_yuno_role:    active.yuno_role || "yuneta_agent",
        remote_yuno_service: active.service || "__default_service__",
        remote_yuno_name:    active.yuno_name || "",
        jwt:                 jwt,
        subscriber:          gobj
    }, gobj_yuno());
    gobj_write_attr(gobj, "iev", iev);

    show_message(gobj, `${i18next.t("connecting")}… ${active.label}`);
    gobj_start_tree(iev);
}

/***************************************************************
 *  Tear down the link (and any mounted component).
 ***************************************************************/
function disconnect(gobj)
{
    destroy_component(gobj);
    let iev = gobj_read_attr(gobj, "iev");
    if(iev) {
        gobj_write_attr(gobj, "iev", null);
        gobj_stop_tree(iev);
        gobj_destroy(iev);
    }
}

/***************************************************************
 *  Create + mount the treedb component once the session is up.
 ***************************************************************/
function mount_component(gobj)
{
    destroy_component(gobj);

    let iev = gobj_read_attr(gobj, "iev");
    if(!iev) {
        return;
    }
    let config = gobj_read_attr(gobj, "config_svc");
    let active = config ? agent_config_get_active(config) : null;
    let treedb_name = (active && active.treedb) || DEFAULT_TREEDB;

    let mode = gobj_read_attr(gobj, "mode");
    let gclass = (mode === "graph") ? "C_YUI_TREEDB_GRAPH" : "C_YUI_TREEDB_TOPICS";

    /*  The treedb components are SERVICES so the command answers route
     *  back to them by name (gobj_find_service). */
    let component = gobj_create_service(
        `treedb_${mode}_${treedb_name}`,
        gclass,
        {
            gobj_remote_yuno: iev,
            treedb_name:      treedb_name,
            subscriber:       gobj
        },
        gobj
    );
    gobj_write_attr(gobj, "component", component);

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
 *  Destroy the mounted component, if any.
 ***************************************************************/
function destroy_component(gobj)
{
    let component = gobj_read_attr(gobj, "component");
    if(component) {
        gobj_write_attr(gobj, "component", null);
        gobj_stop_tree(component);
        gobj_destroy(component);
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************
 *  Link in session — mount the treedb component.
 ***************************************************************/
function ac_on_open(gobj, event, kw, src)
{
    mount_component(gobj);
    return 0;
}

/***************************************************************
 *  Link dropped / failed / refused.
 ***************************************************************/
function ac_on_close(gobj, event, kw, src)
{
    show_message(gobj, i18next.t("disconnected"));
    return 0;
}

function ac_on_open_error(gobj, event, kw, src)
{
    show_message(gobj, `${i18next.t("cannot connect")}: ${kw.url || ""}`);
    return 0;
}

function ac_on_id_nak(gobj, event, kw, src)
{
    show_message(gobj, kw.comment || i18next.t("authentication required"));
    return 0;
}

/***************************************************************
 *  Active agent or login state changed: reconnect.
 ***************************************************************/
function ac_reconnect(gobj, event, kw, src)
{
    connect(gobj);
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
            ["EV_AGENTS_CHANGED",       ac_reconnect,     null],
            ["EV_LOGIN_ACCEPTED",       ac_reconnect,     null],
            ["EV_LOGOUT_DONE",          ac_reconnect,     null],

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
        ["EV_AGENTS_CHANGED",         0],
        ["EV_LOGIN_ACCEPTED",         0],
        ["EV_LOGOUT_DONE",            0],
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
