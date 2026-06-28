/***********************************************************************
 *          c_settings.js
 *
 *      C_SETTINGS — agent endpoints settings, a routed stage view
 *      (mounted by C_YUI_SHELL at /settings/agents).
 *
 *      Lists the agents stored in C_AGENT_CONFIG and offers a small
 *      form to add / edit / delete them and pick the active one. All
 *      state lives in the C_AGENT_CONFIG service (persisted in the
 *      browser localStorage) — this view is a pure editor over it, so
 *      it can be created and destroyed freely on navigation.
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
} from "@yuneta/gobj-js";

import i18next from "i18next";

import {agent_config_get, agent_config_set} from "./c_agent_config.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_SETTINGS";

const DEFAULT_ROLE    = "yuneta_agent";
const DEFAULT_SERVICE = "__default_service__";
const DEFAULT_TREEDB  = "treedb_agentdb";


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",  0,  null,        "Subscriber of output events"),

SDATA(data_type_t.DTP_STRING,   "title",       0,  "settings",  "View title (i18n key)"),
SDATA(data_type_t.DTP_POINTER,  "$container",  0,  null,        "Root HTMLElement"),
SDATA(data_type_t.DTP_POINTER,  "config_svc",  0,  null,        "C_AGENT_CONFIG service"),
SDATA(data_type_t.DTP_INTEGER,  "editing",     0,  -1,          "Index of the agent being edited (-1 = adding new)"),
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

    let svc = gobj_find_service("agent_config", true);
    gobj_write_attr(gobj, "config_svc", svc);

    let $c = createElement2(["div", {class: "view-card", style: "overflow:auto;"}, []]);
    gobj_write_attr(gobj, "$container", $c);

    render(gobj);
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
 *  Remove every child of a DOM node.
 ***************************************************************/
function clear_node($n)
{
    while($n && $n.firstChild) {
        $n.removeChild($n.firstChild);
    }
}

/***************************************************************
 *  One bulma form field (label + text input).
 *  Returns a createElement2 description (not an element).
 ***************************************************************/
function field(label_key, name, value, placeholder)
{
    return ["div", {class: "field"}, [
        ["label", {class: "label is-small", i18n: label_key}, label_key],
        ["div", {class: "control"}, [
            ["input", {
                class: "input is-small",
                type: "text",
                name: name,
                value: value || "",
                placeholder: placeholder || ""
            }]
        ]]
    ]];
}

/***************************************************************
 *  Rebuild the whole view from the current C_AGENT_CONFIG state.
 ***************************************************************/
function render(gobj)
{
    let $c = gobj_read_attr(gobj, "$container");
    if(!$c) {
        return;
    }
    clear_node($c);

    let svc = gobj_read_attr(gobj, "config_svc");
    if(!svc) {
        $c.appendChild(createElement2(
            ["div", {class: "notification is-danger is-light"},
                "C_AGENT_CONFIG service not found"]
        ));
        return;
    }

    let {agents, active_agent} = agent_config_get(svc);
    let editing = gobj_read_attr(gobj, "editing");

    /*-------------------------------*
     *      Header
     *-------------------------------*/
    $c.appendChild(createElement2(
        ["div", {}, [
            ["h1", {class: "title is-4", style: "color:#2E7CD6;", i18n: "agents"}, "Agents"],
            ["p", {class: "subtitle is-6", style: "color:#5B6B7E;",
                   i18n: "agents subtitle"}, "Agent endpoints, stored in this browser"]
        ]]
    ));

    /*-------------------------------*
     *      Agent list
     *-------------------------------*/
    if(!agents.length) {
        $c.appendChild(createElement2(
            ["div", {class: "notification is-light", i18n: "no agents configured"},
                "No agents configured yet"]
        ));
    } else {
        for(let i = 0; i < agents.length; i++) {
            $c.appendChild(build_agent_card(gobj, svc, agents, active_agent, i));
        }
    }

    /*-------------------------------*
     *      Add / edit form
     *-------------------------------*/
    $c.appendChild(build_form(gobj, svc, agents, editing));

    refresh_language($c, i18next.t.bind(i18next));
}

/***************************************************************
 *  One agent row (bulma box) with active badge + actions.
 ***************************************************************/
function build_agent_card(gobj, svc, agents, active_agent, i)
{
    let a = agents[i];
    let is_active = (active_agent === a.label);

    let badge = is_active
        ? ["span", {class: "tag is-success ml-2", i18n: "active"}, "active"]
        : null;

    let meta = `${a.yuno_role || DEFAULT_ROLE} · ${a.service || DEFAULT_SERVICE}` +
               (a.yuno_name ? ` · ${a.yuno_name}` : "");

    let actions = [];
    if(!is_active) {
        actions.push(["button",
            {class: "button is-small is-success is-light", type: "button", i18n: "set active"},
            "Set active",
            {click: () => on_set_active(gobj, svc, agents, i)}
        ]);
    }
    actions.push(["button",
        {class: "button is-small", type: "button", i18n: "edit"},
        "Edit",
        {click: () => on_edit(gobj, i)}
    ]);
    actions.push(["button",
        {class: "button is-small is-danger is-light", type: "button", i18n: "delete"},
        "Delete",
        {click: () => on_delete(gobj, svc, agents, active_agent, i)}
    ]);

    return createElement2(
        ["div", {class: "box p-3 mb-2"}, [
            ["div", {class: "is-flex is-justify-content-space-between is-align-items-flex-start"}, [
                ["div", {}, [
                    ["span", {class: "has-text-weight-bold"}, a.label],
                    badge,
                    ["p", {class: "is-size-7", style: "font-family:monospace; color:#5B6B7E;"}, a.url],
                    ["p", {class: "is-size-7", style: "color:#9AA7B4;"}, meta]
                ]],
                ["div", {class: "buttons are-small is-flex-wrap-nowrap"}, actions]
            ]]
        ]]
    );
}

/***************************************************************
 *  The add / edit form box.
 ***************************************************************/
function build_form(gobj, svc, agents, editing)
{
    let cur = (editing >= 0 && agents[editing]) ? agents[editing] : null;

    let $err = createElement2(
        ["p", {class: "help is-danger", style: "min-height:1.2em;"}, ""]
    );

    let buttons = [
        ["button",
            {class: "button is-small is-primary", type: "button",
             i18n: cur ? "save" : "add agent"},
            cur ? "Save" : "Add agent",
            {click: () => on_save(gobj, svc, $form, $err)}
        ]
    ];
    if(cur) {
        buttons.push(["button",
            {class: "button is-small", type: "button", i18n: "cancel"},
            "Cancel",
            {click: () => on_cancel(gobj)}
        ]);
    }

    let $form = createElement2(
        ["div", {class: "box"}, [
            ["h2", {class: "title is-5", i18n: cur ? "edit agent" : "add agent"},
                cur ? "Edit agent" : "Add agent"],
            field("label",          "label",     cur ? cur.label : "",     "My agent"),
            field("endpoint url",   "url",       cur ? cur.url : "",       "wss://host:1993  (or ws://host:1991)"),
            field("remote role",    "yuno_role", cur ? cur.yuno_role : DEFAULT_ROLE,    DEFAULT_ROLE),
            field("remote service", "service",   cur ? cur.service : DEFAULT_SERVICE,   DEFAULT_SERVICE),
            field("remote name",    "yuno_name", cur ? cur.yuno_name : "", "(optional)"),
            field("treedb",         "treedb",    cur ? cur.treedb : DEFAULT_TREEDB,     DEFAULT_TREEDB),
            $err,
            ["div", {class: "buttons"}, buttons]
        ]]
    );

    return $form;
}

/***************************************************************
 *  Form field value by name.
 ***************************************************************/
function field_value($form, name)
{
    let el = $form.querySelector(`[name="${name}"]`);
    return el ? el.value.trim() : "";
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************
 *  Save the form (add a new agent or update the edited one).
 ***************************************************************/
function on_save(gobj, svc, $form, $err)
{
    let show_err = (key) => { $err.textContent = i18next.t(key) || key; };

    let label = field_value($form, "label");
    let url   = field_value($form, "url");
    if(!label || !url) {
        show_err("label and url are required");
        return;
    }
    if(!/^wss?:\/\//.test(url)) {
        show_err("url must start with ws or wss");
        return;
    }

    let agent = {
        label:     label,
        url:       url,
        yuno_role: field_value($form, "yuno_role") || DEFAULT_ROLE,
        service:   field_value($form, "service")   || DEFAULT_SERVICE,
        yuno_name: field_value($form, "yuno_name"),
        treedb:    field_value($form, "treedb")    || DEFAULT_TREEDB
    };

    let {agents, active_agent} = agent_config_get(svc);
    let editing = gobj_read_attr(gobj, "editing");

    if(editing >= 0 && agents[editing]) {
        let old = agents[editing];
        /*  Reject a rename that collides with another agent. */
        if(agents.some((a, i) => i !== editing && a.label === label)) {
            show_err("an agent with this label already exists");
            return;
        }
        agents[editing] = agent;
        if(active_agent === old.label) {
            active_agent = agent.label;
        }
    } else {
        if(agents.some((a) => a.label === label)) {
            show_err("an agent with this label already exists");
            return;
        }
        agents.push(agent);
        /*  First agent added becomes the active one. */
        if(!active_agent) {
            active_agent = label;
        }
    }

    agent_config_set(svc, agents, active_agent);
    gobj_write_attr(gobj, "editing", -1);
    render(gobj);
}

/***************************************************************
 *  Mark agent i as the active one.
 ***************************************************************/
function on_set_active(gobj, svc, agents, i)
{
    agent_config_set(svc, agents, agents[i].label);
    render(gobj);
}

/***************************************************************
 *  Load agent i into the form for editing.
 ***************************************************************/
function on_edit(gobj, i)
{
    gobj_write_attr(gobj, "editing", i);
    render(gobj);
}

/***************************************************************
 *  Cancel an in-progress edit, back to "add new".
 ***************************************************************/
function on_cancel(gobj)
{
    gobj_write_attr(gobj, "editing", -1);
    render(gobj);
}

/***************************************************************
 *  Delete agent i (clearing the active pointer if it pointed
 *  at it).
 ***************************************************************/
function on_delete(gobj, svc, agents, active_agent, i)
{
    let removed = agents[i];
    agents.splice(i, 1);
    if(active_agent === removed.label) {
        active_agent = agents.length ? agents[0].label : "";
    }
    agent_config_set(svc, agents, active_agent);
    gobj_write_attr(gobj, "editing", -1);
    render(gobj);
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
     *---------------------------------------------*/
    const event_types = [];

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
function register_c_settings()
{
    return create_gclass(GCLASS_NAME);
}

export {register_c_settings};
