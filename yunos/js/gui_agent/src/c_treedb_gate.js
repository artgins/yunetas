/***********************************************************************
 *          c_treedb_gate.js
 *
 *      C_TREEDB_GATE — adapter that lets a reused gobj-ui treedb
 *      component (C_YUI_TREEDB_TOPICS / _GRAPH) browse a treedb that
 *      lives on a MANAGED yuno, reached THROUGH the agent gate.
 *
 *      The components drive their backend with raw
 *      gobj_command(gobj_remote_yuno, "descs"|"nodes"|…). Those raw
 *      commands only reach the agent's OWN services. To reach a managed
 *      yuno's C_NODE treedb service we must wrap each one in the agent's
 *      `command-yuno id=<yuno> service=<treedb> command=<cmd> …`. This
 *      gate is the component's gobj_remote_yuno: it implements
 *      mt_command_parser, wraps the call, sends it over the shared
 *      C_AGENT_LINK, and — because it is a named SERVICE — the agent's
 *      answer routes straight back here (no cross-talk with the
 *      console). It then rewrites the command_stack to the INNER
 *      command so the component correlates the answer as if it had
 *      spoken to the treedb directly.
 *
 *      One gate per open treedb tab. Read path (descs / nodes) only;
 *      live node deltas (subscriptions) are no-ops for now — command-
 *      yuno is request/response, so a treedb tab does not auto-refresh
 *      yet.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA, SDATA_END, data_type_t, event_flag_t, gclass_flag_t,
    gclass_create, log_error,
    gobj_read_attr, gobj_read_pointer_attr, gobj_write_attr,
    gobj_send_event,
    msg_iev_get_stack, msg_iev_push_stack,
    kw_get_dict, kw_get_str,
} from "@yuneta/gobj-js";

import {agent_link_command} from "./c_agent_link.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_TREEDB_GATE";

const COMMAND_STACK_ID = "command_stack";


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",    0,  null,  "Subscriber of output events"),
SDATA(data_type_t.DTP_POINTER,  "link_svc",      0,  null,  "C_AGENT_LINK service"),
SDATA(data_type_t.DTP_STRING,   "yuno_id",       0,  "",    "Target managed yuno id"),
SDATA(data_type_t.DTP_STRING,   "treedb_service",0,  "",    "Target C_NODE service name on that yuno"),
SDATA(data_type_t.DTP_POINTER,  "component",     0,  null,  "The treedb component using this as gobj_remote_yuno"),
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

/***************************************************************
 *          Framework Method: Command
 *
 *  The component calls gobj_command(this, "<cmd>", kw, component).
 *  Wrap it as `command-yuno id=<yuno> service=<treedb> command=<cmd>`
 *  (command-yuno is SDF_WILD_CMD, so the inner params ride along) and
 *  send it over the shared link with src = THIS gate, so the agent's
 *  answer routes back here by service name.
 ***************************************************************/
function mt_command(gobj, command, kw, src)
{
    let link = gobj_read_attr(gobj, "link_svc");
    if(!link) {
        log_error(`${GCLASS_NAME}: no link`);
        return null;
    }

    let cmdyuno = Object.assign({}, kw || {});
    cmdyuno["id"] = gobj_read_attr(gobj, "yuno_id");
    cmdyuno["service"] = gobj_read_attr(gobj, "treedb_service");
    cmdyuno["command"] = command;

    agent_link_command(link, "command-yuno", cmdyuno, gobj);

    return null;   // asynchronous
}

/***************************************************************
 *      Framework Method subscription_added / deleted
 *
 *  Live treedb node deltas are not forwarded yet (command-yuno is
 *  request/response). Accept the component's subscription as a no-op
 *  so it does not error; the tab simply does not auto-refresh.
 ***************************************************************/
function mt_subscription_added(gobj, subs)
{
    return 0;
}

function mt_subscription_deleted(gobj, subs)
{
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************
 *  The agent's command-yuno answer. Rewrite the command_stack to
 *  the INNER command and forward to the component as if it had
 *  spoken to the treedb directly.
 ***************************************************************/
function ac_mt_command_answer(gobj, event, kw, src)
{
    let component = gobj_read_attr(gobj, "component");
    if(!component) {
        return 0;
    }

    /*  Our own stack entry: {command:"command-yuno", kw: cmdyuno}. */
    let outer = msg_iev_get_stack(gobj, kw, COMMAND_STACK_ID, true);
    let cmdyuno = kw_get_dict(gobj, outer, "kw", {}, 0);

    let inner_command = kw_get_str(gobj, cmdyuno, "command", "", 0);
    let inner_kw = Object.assign({}, cmdyuno);
    delete inner_kw["id"];
    delete inner_kw["service"];
    delete inner_kw["command"];

    /*  Hand the component a normal command answer carrying the inner
     *  command in the stack (that is what its ac_mt_command_answer
     *  switches on). */
    let answer = {
        result:  kw.result,
        comment: kw.comment,
        schema:  kw.schema,
        data:    kw.data
    };
    msg_iev_push_stack(gobj, answer, COMMAND_STACK_ID, {
        command: inner_command,
        kw:      inner_kw
    });

    gobj_send_event(component, "EV_MT_COMMAND_ANSWER", answer, gobj);
    return 0;
}

/***************************************************************
 *  Unsolicited stats push from the agent — ignore for now.
 ***************************************************************/
function ac_stats_noop(gobj, event, kw, src)
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
    mt_create:               mt_create,
    mt_start:                mt_start,
    mt_stop:                 mt_stop,
    mt_destroy:              mt_destroy,
    mt_command_parser:       mt_command,
    mt_subscription_added:   mt_subscription_added,
    mt_subscription_deleted: mt_subscription_deleted,
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
            ["EV_MT_COMMAND_ANSWER", ac_mt_command_answer, null],
            ["EV_MT_STATS_ANSWER",   ac_stats_noop,        null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *  EV_MT_COMMAND_ANSWER must be PUBLIC so the iev's on_message
     *  delivers it to us (found by service name).
     *---------------------------------------------*/
    const event_types = [
        ["EV_MT_COMMAND_ANSWER", event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_MT_STATS_ANSWER",   event_flag_t.EVF_PUBLIC_EVENT]
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
        gclass_flag_t.gcflag_no_check_output_events
    );

    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************
 *          Register GClass
 ***************************************************************/
function register_c_treedb_gate()
{
    return create_gclass(GCLASS_NAME);
}

export {register_c_treedb_gate};
