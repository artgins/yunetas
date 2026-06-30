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
    let priv = gobj.priv;
    priv.filter   = "";         /*  live search text (lower-case)  */
    priv.sort_key = "host";     /*  host | role | version          */
    priv.sort_dir = "asc";      /*  asc | desc                     */

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
    build_dom(gobj);
    render_rows(gobj);
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
 *  Sortable columns (the search box matches all four fields).
 ***************************************************************/
const SORT_COLUMNS = ["host", "role", "version"];

/***************************************************************
 *  Apply the live filter + current sort to the raw node list.
 ***************************************************************/
function visible_nodes(gobj)
{
    let priv = gobj.priv;
    let nodes = gobj_read_attr(gobj, "nodes");
    if(!Array.isArray(nodes)) {
        nodes = [];
    }

    let f = (priv.filter || "").trim();
    let list = nodes;
    if(f) {
        list = nodes.filter((n) => {
            return (n.host || "").toLowerCase().includes(f) ||
                   (n.role || "").toLowerCase().includes(f) ||
                   (n.version || "").toLowerCase().includes(f) ||
                   (n.uuid || "").toLowerCase().includes(f);
        });
    }

    let key = priv.sort_key || "host";
    let dir = (priv.sort_dir === "desc") ? -1 : 1;
    list = list.slice().sort((a, b) => {
        let va = String(a[key] || "").toLowerCase();
        let vb = String(b[key] || "").toLowerCase();
        if(va < vb) {
            return -1 * dir;
        }
        if(va > vb) {
            return 1 * dir;
        }
        return 0;
    });
    return list;
}

/***************************************************************
 *  Build the static shell once: header, search toolbar, table
 *  skeleton with sortable headers, and the empty/disconnected
 *  notification slot. Rows are filled by render_rows().
 ***************************************************************/
function build_dom(gobj)
{
    let priv = gobj.priv;
    let $c = gobj_read_attr(gobj, "$container");
    if(!$c) {
        return;
    }
    clear_node($c);

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

    /*  Search toolbar: live filter on the left, match count on the right  */
    let $input = createElement2(["input", {
        class:        "input is-small",
        type:         "search",
        placeholder:  i18next.t("search nodes"),
        "aria-label": i18next.t("search nodes")
    }, null, {
        input: () => {
            priv.filter = String(priv.$input.value || "").toLowerCase();
            render_rows(gobj);
        }
    }]);
    priv.$input = $input;

    let $count = createElement2(["span", {class: "is-size-7 has-text-grey"}, ""]);
    priv.$count = $count;

    priv.$toolbar = createElement2(
        ["div", {class: "is-flex is-align-items-center mb-2", style: "gap:0.75rem;"}, [
            ["div", {class: "control has-icons-left", style: "flex:1; max-width:24rem;"}, [
                $input,
                ["span", {class: "icon is-small is-left"}, [
                    ["span", {class: "yi-magnifying-glass", style: "font-size:0.8rem;"}, ""]
                ]]
            ]],
            $count
        ]]
    );
    $c.appendChild(priv.$toolbar);

    /*  Sortable table  */
    priv.$arrows = {};
    let $thead_cells = [];
    for(let key of SORT_COLUMNS) {
        let $arrow = createElement2(["span", {class: "is-size-7 has-text-grey ml-1"}, ""]);
        priv.$arrows[key] = $arrow;
        $thead_cells.push(createElement2(
            ["th", {style: "cursor:pointer; user-select:none; white-space:nowrap;", i18n: key},
                key, {click: () => set_sort(gobj, key)}]
        ));
        /*  attach the arrow span after the (translated) label  */
        $thead_cells[$thead_cells.length - 1].appendChild($arrow);
    }
    $thead_cells.push(createElement2(["th", {i18n: "uuid"}, "uuid"]));
    $thead_cells.push(createElement2(["th", {style: "width:1%;"}, ""]));

    let $tbody = createElement2(["tbody", {}, []]);
    priv.$tbody = $tbody;

    priv.$table = createElement2(
        ["table", {class: "table is-fullwidth is-narrow is-hoverable"}, [
            ["thead", {}, [
                ["tr", {}, $thead_cells]
            ]],
            $tbody
        ]]
    );
    $c.appendChild(priv.$table);

    /*  Notification slot (not-connected / no-nodes / no-matches)  */
    priv.$notif = createElement2(["div", {class: "notification is-light", style: "display:none;"}, ""]);
    $c.appendChild(priv.$notif);

    refresh_language($c, i18next.t.bind(i18next));
}

/***************************************************************
 *  Fill the table body from the filtered + sorted node list,
 *  and toggle the notification slot for the empty states.
 ***************************************************************/
function render_rows(gobj)
{
    let priv = gobj.priv;
    if(!priv.$tbody) {
        return;
    }

    let link = gobj_read_attr(gobj, "link_svc");
    let connected = !!(link && agent_link_is_connected(link));
    let config = gobj_read_attr(gobj, "config_svc");
    let active = config ? agent_config_get_active_node(config) : "";
    let total = (gobj_read_attr(gobj, "nodes") || []).length;

    /*  Sort arrows  */
    for(let key of SORT_COLUMNS) {
        let arrow = "";
        if(priv.sort_key === key) {
            arrow = (priv.sort_dir === "desc") ? "▼" : "▲";
        }
        priv.$arrows[key].textContent = arrow;
    }

    let show_table = false;
    let notif_key = "";
    if(!connected) {
        notif_key = "not connected to an agent";
    } else if(!total) {
        notif_key = "no nodes";
    } else {
        show_table = true;
    }

    /*  Disconnected / no-nodes: hide table + toolbar, show notice  */
    priv.$toolbar.style.display = connected ? "" : "none";
    priv.$table.style.display = show_table ? "" : "none";

    if(notif_key) {
        priv.$notif.setAttribute("data-i18n", notif_key);
        priv.$notif.textContent = i18next.t(notif_key);
        priv.$notif.style.display = "";
        clear_node(priv.$tbody);
        priv.$count.textContent = "";
        return;
    }

    let list = visible_nodes(gobj);
    clear_node(priv.$tbody);
    for(let n of list) {
        priv.$tbody.appendChild(build_node_row(gobj, n, active));
    }

    /*  No rows after filtering: keep the table header, note the gap  */
    if(!list.length) {
        priv.$notif.setAttribute("data-i18n", "no matching nodes");
        priv.$notif.textContent = i18next.t("no matching nodes");
        priv.$notif.style.display = "";
    } else {
        priv.$notif.style.display = "none";
    }

    priv.$count.textContent = (list.length === total)
        ? `${total}`
        : `${list.length} / ${total}`;
}

/***************************************************************
 *  One compact table row. The active node gets a green left
 *  accent + tinted background + bold host so it stands out.
 ***************************************************************/
function build_node_row(gobj, n, active)
{
    let is_active = !!(active && (active === n.host || active === n.uuid));

    let host_cell = is_active
        ? ["td", {style: "white-space:nowrap;"}, [
            ["span", {class: "has-text-weight-bold"}, n.host || n.uuid],
            ["span", {class: "tag is-success is-light ml-2", i18n: "active"}, "active"]
        ]]
        : ["td", {style: "white-space:nowrap;"}, n.host || n.uuid];

    let action = is_active
        ? ["td", {}, ""]
        : ["td", {style: "width:1%;"}, [
            ["button", {class: "button is-small is-success is-light", type: "button", i18n: "select"},
                "Select", {click: () => on_select(gobj, n)}]
        ]];

    /*  Theme-aware highlight lives in app.css (.node-row-active) so the
     *  dark palette can override it — an inline background can't.  */
    let row_attrs = is_active ? {class: "node-row-active"} : {};

    return createElement2(
        ["tr", row_attrs, [
            host_cell,
            ["td", {style: "white-space:nowrap;"}, n.role || ""],
            ["td", {style: "white-space:nowrap;"}, n.version || ""],
            ["td", {class: "is-family-monospace is-size-7", style: "color:#9AA7B4;", title: n.uuid},
                n.uuid || ""],
            action
        ]]
    );
}

/***************************************************************
 *  Header click — toggle direction if same column, else switch
 *  column and reset to ascending.
 ***************************************************************/
function set_sort(gobj, key)
{
    let priv = gobj.priv;
    if(priv.sort_key === key) {
        priv.sort_dir = (priv.sort_dir === "asc") ? "desc" : "asc";
    } else {
        priv.sort_key = key;
        priv.sort_dir = "asc";
    }
    render_rows(gobj);
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
    render_rows(gobj);
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
    render_rows(gobj);
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
    render_rows(gobj);
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
