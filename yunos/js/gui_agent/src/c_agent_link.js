/***********************************************************************
 *          c_agent_link.js
 *
 *      C_AGENT_LINK — the single shared connection to the ACTIVE agent
 *      (named service "agent_link").
 *
 *      Owns ONE C_IEVENT_CLI and is the only thing that connects to the
 *      agent, so multiple panels (console now, more later) never open
 *      duplicate channels from the same yuno identity. It manages the
 *      link lifecycle — (re)creating the C_IEVENT_CLI when the active
 *      agent or the login token changes — and RE-PUBLISHES the link's
 *      events to its own subscribers, so consumers subscribe to this
 *      stable service instead of to the volatile transport gobj.
 *
 *      Consumers:
 *          - subscribe to "agent_link" for EV_ON_OPEN / EV_ON_CLOSE /
 *            EV_ON_OPEN_ERROR / EV_ON_ID_NAK / EV_MT_COMMAND_ANSWER /
 *            EV_MT_STATS_ANSWER;
 *          - send via agent_link_command() / agent_link_stats().
 *
 *      The identity-card session always targets the agent's default
 *      service; per-command targeting (e.g. a treedb) is done with
 *      kw.service by the caller.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA, SDATA_END, data_type_t, event_flag_t,
    gclass_create, log_error,
    gobj_read_pointer_attr, gobj_read_attr, gobj_write_attr,
    gobj_subscribe_event,
    gobj_publish_event,
    gobj_find_service,
    gobj_yuno,
    gobj_create,
    gobj_start_tree, gobj_stop_tree, gobj_destroy,
    gobj_is_running,
    gobj_command, gobj_stats,
    gobj_current_state,
} from "@yuneta/gobj-js";

import {agent_config_get_active} from "./c_agent_config.js";
import {agent_login_get_token} from "./c_agent_login.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_AGENT_LINK";


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",  0,  null,  "Subscriber of output events"),
SDATA(data_type_t.DTP_POINTER,  "config_svc",  0,  null,  "C_AGENT_CONFIG service"),
SDATA(data_type_t.DTP_POINTER,  "login_svc",   0,  null,  "C_AGENT_LOGIN service"),
SDATA(data_type_t.DTP_POINTER,  "iev",         0,  null,  "Current C_IEVENT_CLI"),
SDATA(data_type_t.DTP_STRING,   "active_label",0,  "",    "Label of the connected agent"),
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
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    open_link(gobj);
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    close_link(gobj);
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
    close_link(gobj);
}




                    /***************************
                     *      Public functions
                     ***************************/




/***************************************************************
 *  The current transport gobj, or null when there is no agent.
 ***************************************************************/
function agent_link_get_iev(gobj)
{
    return gobj_read_attr(gobj, "iev");
}

/***************************************************************
 *  True while the link is in session.
 ***************************************************************/
function agent_link_is_connected(gobj)
{
    let iev = gobj_read_attr(gobj, "iev");
    return !!(iev && gobj_current_state(iev) === "ST_SESSION");
}

/***************************************************************
 *  Send a control-plane command to the agent (or a service of it
 *  via kw.service). `src` is the caller, so the answer's routing
 *  carries its name.
 ***************************************************************/
function agent_link_command(gobj, command, kw, src)
{
    let iev = gobj_read_attr(gobj, "iev");
    if(!iev) {
        return -1;
    }
    return gobj_command(iev, command, kw || {}, src || gobj);
}

/***************************************************************
 *  Ask the agent (or kw.service) for stats.
 ***************************************************************/
function agent_link_stats(gobj, stats, kw, src)
{
    let iev = gobj_read_attr(gobj, "iev");
    if(!iev) {
        return -1;
    }
    return gobj_stats(iev, stats, kw || {}, src || gobj);
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************
 *  Tear down the current transport, if any.
 ***************************************************************/
function close_link(gobj)
{
    let iev = gobj_read_attr(gobj, "iev");
    if(iev) {
        gobj_write_attr(gobj, "iev", null);
        if(gobj_is_running(iev)) {
            gobj_stop_tree(iev);
        }
        gobj_destroy(iev);
    }
}

/***************************************************************
 *  (Re)create the transport for the active agent. Recreating
 *  (vs reconfiguring) is required because the C_IEVENT_CLI bakes
 *  the wanted_yuno_* identity-card fields in its mt_create from
 *  remote_yuno_*; those are not repointable on a live gobj.
 ***************************************************************/
function open_link(gobj)
{
    close_link(gobj);

    let config = gobj_read_attr(gobj, "config_svc");
    let active = config ? agent_config_get_active(config) : null;
    if(!active) {
        gobj_write_attr(gobj, "active_label", "");
        return;
    }
    gobj_write_attr(gobj, "active_label", active.label);

    let login = gobj_read_attr(gobj, "login_svc");
    let jwt = login ? agent_login_get_token(login) : "";

    /*  Parented to the yuno (always alive); subscriber is THIS link so
     *  its events flow here and get re-published to consumers. The
     *  identity card targets the agent's default service. */
    let iev = gobj_create("agent_iev", "C_IEVENT_CLI", {
        url:                 active.url,
        remote_yuno_role:    active.yuno_role || "yuneta_agent",
        remote_yuno_service: active.service || "__default_service__",
        remote_yuno_name:    active.yuno_name || "",
        jwt:                 jwt,
        subscriber:          gobj
    }, gobj_yuno());
    gobj_write_attr(gobj, "iev", iev);

    gobj_start_tree(iev);
}

/***************************************************************
 *  Re-publish an event from the transport to the link's
 *  subscribers (the panels).
 ***************************************************************/
function bubble(gobj, event, kw)
{
    gobj_publish_event(gobj, event, kw || {});
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




function ac_on_open(gobj, event, kw, src)
{
    return bubble(gobj, "EV_ON_OPEN", kw);
}

function ac_on_close(gobj, event, kw, src)
{
    return bubble(gobj, "EV_ON_CLOSE", kw);
}

function ac_on_open_error(gobj, event, kw, src)
{
    return bubble(gobj, "EV_ON_OPEN_ERROR", kw);
}

function ac_on_id_nak(gobj, event, kw, src)
{
    return bubble(gobj, "EV_ON_ID_NAK", kw);
}

function ac_mt_command_answer(gobj, event, kw, src)
{
    return bubble(gobj, "EV_MT_COMMAND_ANSWER", kw);
}

function ac_mt_stats_answer(gobj, event, kw, src)
{
    return bubble(gobj, "EV_MT_STATS_ANSWER", kw);
}

/***************************************************************
 *  Active agent or login changed: re-open the link.
 ***************************************************************/
function ac_reopen(gobj, event, kw, src)
{
    open_link(gobj);
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
            ["EV_ON_OPEN_ERROR",     ac_on_open_error,     null],
            ["EV_ON_ID_NAK",         ac_on_id_nak,         null],
            ["EV_MT_COMMAND_ANSWER", ac_mt_command_answer, null],
            ["EV_MT_STATS_ANSWER",   ac_mt_stats_answer,   null],
            ["EV_AGENTS_CHANGED",    ac_reopen,            null],
            ["EV_LOGIN_ACCEPTED",    ac_reopen,            null],
            ["EV_LOGOUT_DONE",       ac_reopen,            null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *  The re-published events are output (optional subscribers).
     *---------------------------------------------*/
    const out = event_flag_t.EVF_OUTPUT_EVENT | event_flag_t.EVF_NO_WARN_SUBS;
    const event_types = [
        ["EV_ON_OPEN",           out],
        ["EV_ON_CLOSE",          out],
        ["EV_ON_OPEN_ERROR",     out],
        ["EV_ON_ID_NAK",         out],
        ["EV_MT_COMMAND_ANSWER", out],
        ["EV_MT_STATS_ANSWER",   out],
        ["EV_AGENTS_CHANGED",    0],
        ["EV_LOGIN_ACCEPTED",    0],
        ["EV_LOGOUT_DONE",       0]
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
function register_c_agent_link()
{
    return create_gclass(GCLASS_NAME);
}

export {
    register_c_agent_link,
    agent_link_get_iev,
    agent_link_is_connected,
    agent_link_command,
    agent_link_stats,
};
