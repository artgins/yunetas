/***********************************************************************
 *          c_agent_console.js
 *
 *      C_AGENT_CONSOLE — control-plane CLI to a yuneta agent, a routed
 *      stage view (mounted by C_YUI_SHELL at /console/agent).
 *
 *      The v2 successor of the old webix "Yuneta CLI": a command input
 *      with history + a response area. It opens a C_IEVENT_CLI child to
 *      the ACTIVE agent (from C_AGENT_CONFIG), sends commands with
 *      gobj_command() and renders the agent's EV_MT_COMMAND_ANSWER
 *      ({result, comment, schema, data}) — exactly the ycommand path.
 *
 *      Connection state drives the shell's toolbar connection dot via
 *      yui_shell_set_connection_state(): EV_ON_OPEN → connected,
 *      close / errors / NAK → disconnected. Re-connects when the active
 *      agent changes (EV_AGENTS_CHANGED from C_AGENT_CONFIG).
 *
 *      Auth (jwt for wss:1993 OAuth2 agents) lands in Phase 2; until
 *      then a wss+OAuth2 agent answers the identity card with a NAK,
 *      which this view surfaces. A plain local agent (ws://…:1991)
 *      connects and executes end-to-end already.
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
    gobj_create, gobj_start_tree, gobj_stop_tree, gobj_destroy,
    gobj_command,
    createElement2,
    refresh_language,
} from "@yuneta/gobj-js";

import i18next from "i18next";

import {yui_shell_set_connection_state} from "@yuneta/gobj-ui/index.js";

import {agent_config_get_active} from "./c_agent_config.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_AGENT_CONSOLE";

const HISTORY_MAX = 30;

/*  A few common agent commands seeded into the input's datalist. */
const SEED_COMMANDS = ["help", "list-yunos", "stats", "list-binaries", "view-config"];


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",  0,  null,        "Subscriber of output events"),

SDATA(data_type_t.DTP_STRING,   "title",       0,  "console",   "View title (i18n key)"),
SDATA(data_type_t.DTP_POINTER,  "$container",  0,  null,        "Root HTMLElement"),
SDATA(data_type_t.DTP_POINTER,  "config_svc",  0,  null,        "C_AGENT_CONFIG service"),
SDATA(data_type_t.DTP_POINTER,  "iev",         0,  null,        "C_IEVENT_CLI child to the active agent"),
SDATA(data_type_t.DTP_BOOLEAN,  "connected",   0,  false,       "True while in session with the agent"),
SDATA(data_type_t.DTP_STRING,   "active_label",0,  "",          "Label of the agent we are connected to"),
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
    let priv = gobj.priv;
    priv.history = [];
    priv.got_nak = false;
    priv.$input = null;
    priv.$status = null;
    priv.$comment = null;
    priv.$data = null;
    priv.$datalist = null;

    /*
     *  CHILD subscription model
     */
    let subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, null, {}, subscriber);

    /*  Watch the config service so a change of active agent reconnects. */
    let svc = gobj_find_service("agent_config", true);
    gobj_write_attr(gobj, "config_svc", svc);
    if(svc) {
        gobj_subscribe_event(svc, "EV_AGENTS_CHANGED", {}, gobj);
    }

    build_ui(gobj);
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
 *  Do NOT drop the agent link here: this view is keep_alive and the
 *  shell may stop/start it during mount/layout. Tearing down the
 *  C_IEVENT_CLI on every stop would kill the session mid-handshake.
 *  The link lives on the yuno and is released in mt_destroy.
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
 *  (Re)connect a C_IEVENT_CLI child to the active agent.
 ***************************************************************/
function connect(gobj)
{
    disconnect(gobj);

    let svc = gobj_read_attr(gobj, "config_svc");
    let active = svc ? agent_config_get_active(svc) : null;
    if(!active) {
        gobj_write_attr(gobj, "active_label", "");
        set_status(gobj, false, i18next.t("no active agent"));
        return;
    }

    gobj_write_attr(gobj, "active_label", active.label);

    gobj.priv.got_nak = false;

    /*  Parent the transport to the yuno (always alive), NOT to this
     *  keep_alive view, so a transient view stop/start during shell
     *  layout cannot cascade-stop the session. The view owns the ref
     *  and releases it in mt_destroy / on reconnect. Subscriber is the
     *  view, so all link events reach us. */
    let iev = gobj_create("agent_iev", "C_IEVENT_CLI", {
        url:                 active.url,
        remote_yuno_role:    active.yuno_role || "yuneta_agent",
        remote_yuno_service: active.service || "__default_service__",
        remote_yuno_name:    active.yuno_name || "",
        jwt:                 "",            // Phase 2: real token
        subscriber:          gobj
    }, gobj_yuno());
    gobj_write_attr(gobj, "iev", iev);

    set_status(gobj, false, `${i18next.t("connecting")}… ${active.label}`);
    gobj_start_tree(iev);
}

/***************************************************************
 *  Tear down the current C_IEVENT_CLI child, if any.
 ***************************************************************/
function disconnect(gobj)
{
    let iev = gobj_read_attr(gobj, "iev");
    if(iev) {
        gobj_write_attr(gobj, "iev", null);
        gobj_stop_tree(iev);
        gobj_destroy(iev);
    }
    gobj_write_attr(gobj, "connected", false);
}

/***************************************************************
 *  Update the connection dot (shell) + the local status line.
 ***************************************************************/
function set_status(gobj, connected, text)
{
    gobj_write_attr(gobj, "connected", !!connected);

    let shell = gobj_parent(gobj);
    if(shell) {
        yui_shell_set_connection_state(shell, !!connected);
    }

    let priv = gobj.priv;
    if(priv.$status) {
        priv.$status.textContent = text || "";
        priv.$status.classList.toggle("has-text-success", !!connected);
        priv.$status.classList.toggle("has-text-grey", !connected);
    }
}

/***************************************************************
 *  Build the command input + response DOM.
 ***************************************************************/
function build_ui(gobj)
{
    let priv = gobj.priv;

    let $datalist = createElement2(["datalist", {id: "agent-cmd-history"}, []]);
    for(let c of SEED_COMMANDS) {
        $datalist.appendChild(createElement2(["option", {value: c}]));
    }
    priv.$datalist = $datalist;

    let $input = createElement2(["input", {
        class:        "input is-family-monospace",
        type:         "text",
        list:         "agent-cmd-history",
        placeholder:  "help",
        "aria-label": "command"
    }, null, {
        keydown: (ev) => {
            if(ev.key === "Enter") {
                ev.preventDefault();
                send_command(gobj);
            }
        }
    }]);
    priv.$input = $input;

    let $exec = createElement2(
        ["button", {class: "button is-primary", type: "button", i18n: "execute"}, "Execute"],
        i18next.t.bind(i18next)
    );
    $exec.addEventListener("click", () => send_command(gobj));

    let $clear = createElement2(
        ["button", {class: "button", type: "button", i18n: "clear"}, "Clear"],
        i18next.t.bind(i18next)
    );
    $clear.addEventListener("click", () => {
        priv.$input.value = "";
        priv.$comment.textContent = "";
        priv.$data.textContent = "";
        priv.$input.focus();
    });

    let $status = createElement2(["p", {class: "is-size-7 has-text-grey mb-2"}, ""]);
    priv.$status = $status;

    let $comment = createElement2(
        ["pre", {class: "is-size-7 mb-2",
                 style: "white-space:pre-wrap; background:#F4F6F8; padding:0.5rem; border-radius:4px; min-height:1.5rem;"},
            ""]
    );
    priv.$comment = $comment;

    let $data = createElement2(
        ["pre", {style: "flex:1; overflow:auto; background:#1F2430; color:#D7DEE9; padding:0.75rem; border-radius:4px; font-size:12px;"},
            ""]
    );
    priv.$data = $data;

    let $c = createElement2(
        ["div", {class: "view-card", style: "display:flex; flex-direction:column; height:100%;"},
            [
                $status,
                ["div", {class: "field has-addons mb-2"}, [
                    ["div", {class: "control is-expanded"}, [$input]],
                    ["div", {class: "control"}, [$exec]],
                    ["div", {class: "control"}, [$clear]]
                ]],
                $datalist,
                $comment,
                $data
            ]
        ]
    );
    gobj_write_attr(gobj, "$container", $c);

    refresh_language($c, i18next.t.bind(i18next));
}

/***************************************************************
 *  Push a command onto the in-memory history + datalist.
 ***************************************************************/
function add_history(gobj, cmd)
{
    let priv = gobj.priv;
    if(priv.history[0] === cmd) {
        return;
    }
    priv.history.unshift(cmd);
    if(priv.history.length > HISTORY_MAX) {
        priv.history.pop();
    }
    /*  Surface it for autocomplete if not already an option. */
    if(priv.$datalist &&
        !priv.$datalist.querySelector(`option[value="${CSS.escape(cmd)}"]`)) {
        priv.$datalist.appendChild(createElement2(["option", {value: cmd}]));
    }
}

/***************************************************************
 *  Render a command answer / status comment.
 ***************************************************************/
function show_comment(gobj, comment, result)
{
    let priv = gobj.priv;
    if(!priv.$comment) {
        return;
    }
    priv.$comment.textContent = comment || "";
    priv.$comment.classList.toggle("has-text-danger", typeof result === "number" && result < 0);
}

/***************************************************************
 *  Render the data payload of a command answer.
 ***************************************************************/
function show_data(gobj, data)
{
    let priv = gobj.priv;
    if(!priv.$data) {
        return;
    }
    if(data === null || data === undefined) {
        priv.$data.textContent = "";
        return;
    }
    if(typeof data === "string") {
        priv.$data.textContent = data;
        return;
    }
    try {
        priv.$data.textContent = JSON.stringify(data, null, 2);
    } catch(e) {
        priv.$data.textContent = String(data);
    }
}

/***************************************************************
 *  Send the current input to the active agent.
 ***************************************************************/
function send_command(gobj)
{
    let priv = gobj.priv;
    let cmd = priv.$input ? priv.$input.value.trim() : "";
    if(!cmd) {
        return;
    }

    let iev = gobj_read_attr(gobj, "iev");
    if(!iev || !gobj_read_attr(gobj, "connected")) {
        show_comment(gobj, i18next.t("not connected to an agent"), -1);
        return;
    }

    add_history(gobj, cmd);
    show_comment(gobj, "…", 0);
    show_data(gobj, null);

    gobj_command(iev, cmd, {}, gobj);
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************
 *  Identity card accepted — we are in session.
 ***************************************************************/
function ac_on_open(gobj, event, kw, src)
{
    gobj.priv.got_nak = false;
    let remote = `${kw.remote_yuno_role || ""}^${kw.remote_yuno_name || ""}`;
    set_status(gobj, true, `${i18next.t("connected")} · ${remote}`);
    return 0;
}

/***************************************************************
 *  Channel dropped after a real session.
 ***************************************************************/
function ac_on_close(gobj, event, kw, src)
{
    set_status(gobj, false, i18next.t("disconnected"));
    return 0;
}

/***************************************************************
 *  WebSocket never opened (bad cert / port closed / no backend).
 ***************************************************************/
function ac_on_open_error(gobj, event, kw, src)
{
    /*  A NAK (auth rejection) already set an informative status + the
     *  agent's reason, and is followed by this generic close. Don't
     *  clobber it — just make sure the connection dot is off. */
    if(gobj.priv.got_nak) {
        gobj.priv.got_nak = false;
        let shell = gobj_parent(gobj);
        if(shell) {
            yui_shell_set_connection_state(shell, false);
        }
        return 0;
    }
    set_status(gobj, false, i18next.t("disconnected"));
    show_comment(gobj,
        `${i18next.t("cannot connect")}: ${kw.url || ""} (${kw.reason || kw.code || ""})`, -1);
    return 0;
}

/***************************************************************
 *  Identity card refused (auth required / rejected).
 ***************************************************************/
function ac_on_id_nak(gobj, event, kw, src)
{
    gobj.priv.got_nak = true;
    set_status(gobj, false, i18next.t("authentication required"));
    show_comment(gobj, kw.comment || i18next.t("identity card refused"), -1);
    return 0;
}

/***************************************************************
 *  Command answer from the agent.
 ***************************************************************/
function ac_mt_command_answer(gobj, event, kw, src)
{
    show_comment(gobj, kw.comment, kw.result);
    show_data(gobj, kw.data);
    return 0;
}

/***************************************************************
 *  The active agent (or the agent list) changed: reconnect.
 ***************************************************************/
function ac_agents_changed(gobj, event, kw, src)
{
    let new_active = (kw && kw.active_agent) || "";
    if(new_active !== gobj_read_attr(gobj, "active_label")) {
        connect(gobj);
    }
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
            ["EV_AGENTS_CHANGED",    ac_agents_changed,    null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_ON_OPEN",           0],
        ["EV_ON_CLOSE",          0],
        ["EV_ON_OPEN_ERROR",     0],
        ["EV_ON_ID_NAK",         0],
        ["EV_MT_COMMAND_ANSWER", 0],
        ["EV_AGENTS_CHANGED",    0]
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
function register_c_agent_console()
{
    return create_gclass(GCLASS_NAME);
}

export {register_c_agent_console};
