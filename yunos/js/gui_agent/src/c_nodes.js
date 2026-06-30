/***********************************************************************
 *          c_nodes.js
 *
 *      C_NODES — node picker. Lists the nodes connected to the control
 *      center (`list-agents`) and lets the operator choose the ACTIVE
 *      node; the Console then routes commands to it via command-agent.
 *
 *      The list is a Tabulator table (the repo's official table
 *      manager): header-click sorting, a live search box, and a
 *      strongly highlighted active row. Tabulator's dark palette is
 *      supplied by src/app.css (the v2 shell does not ship gobj-ui v1's
 *      c_yui_main.css override).
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
    gobj_parent, gobj_name,
    gobj_read_attr, gobj_read_pointer_attr, gobj_write_attr,
    gobj_subscribe_event,
    gobj_find_service,
    createElement2,
    refresh_language,
    msg_iev_get_stack,
    kw_get_str,
} from "@yuneta/gobj-js";

import i18next from "i18next";
import {TabulatorFull as Tabulator} from "tabulator-tables";

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
SDATA(data_type_t.DTP_POINTER,  "tabulator",   0,  null,     "Tabulator instance"),
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
    priv.active   = "";     /*  currently selected node (host or uuid)  */
    priv.table_id = `nodes_table_${gobj_name(gobj)}`;

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

    let $c = createElement2(
        ["div", {class: "view-card", style: "display:flex; flex-direction:column; height:100%;"}, []]
    );
    gobj_write_attr(gobj, "$container", $c);
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    build_dom(gobj);
    create_table(gobj);
    update_table(gobj);
    request_agents(gobj);
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    let table = gobj_read_attr(gobj, "tabulator");
    if(table) {
        table.destroy();
        gobj_write_attr(gobj, "tabulator", null);
    }
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
 *  Minimal HTML escaping for values rendered as Tabulator
 *  formatter HTML (host/role/version/uuid come from the agent).
 ***************************************************************/
function esc(s)
{
    return String(s == null ? "" : s).replace(/[&<>"]/g, (c) => {
        return {"&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;"}[c];
    });
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
 *  Is this node the active one? (matched by host or uuid)
 ***************************************************************/
function is_active_node(gobj, n)
{
    let active = gobj.priv.active;
    return !!(active && (active === n.host || active === n.uuid));
}

/***************************************************************
 *  Build the static shell once: header, search toolbar, the
 *  Tabulator host div, and the not-connected notice slot.
 ***************************************************************/
function build_dom(gobj)
{
    let priv = gobj.priv;
    let t = i18next.t.bind(i18next);
    let $c = gobj_read_attr(gobj, "$container");
    if(!$c) {
        return;
    }
    clear_node($c);

    /*  Single-line toolbar: search (grows) · count · refresh.
     *  No title/subtitle block — the nav already labels the view, and
     *  on mobile every extra header line steals the table's space.  */
    let $input = createElement2(["input", {
        class:        "input is-small",
        type:         "search",
        placeholder:  t("search nodes"),
        "aria-label": t("search nodes")
    }, null, {
        input: () => apply_filter(gobj)
    }]);
    priv.$input = $input;

    let $count = createElement2(["span", {class: "is-size-7 has-text-grey"}, ""]);
    priv.$count = $count;

    priv.$toolbar = createElement2(
        ["div", {class: "is-flex is-align-items-center mb-2", style: "gap:0.5rem;"}, [
            ["div", {class: "control has-icons-left", style: "flex:1; min-width:0;"}, [
                $input,
                ["span", {class: "icon is-small is-left"}, [
                    ["span", {class: "yi-magnifying-glass", style: "font-size:0.8rem;"}, ""]
                ]]
            ]],
            $count,
            ["button", {class: "button is-small", type: "button", i18n: "refresh"}, "Refresh",
                {click: () => request_agents(gobj)}]
        ]]
    );
    $c.appendChild(priv.$toolbar);

    /*  Tabulator host  */
    priv.$tablewrap = createElement2(
        ["div", {style: "flex:1; min-height:0;"}, [
            ["div", {id: priv.table_id}, []]
        ]]
    );
    $c.appendChild(priv.$tablewrap);

    /*  Not-connected notice (Tabulator's own placeholder covers no-nodes)  */
    priv.$notif = createElement2(
        ["div", {class: "notification is-light", style: "display:none;", i18n: "not connected to an agent"},
            "Not connected"]
    );
    $c.appendChild(priv.$notif);

    refresh_language($c, t);
}

/***************************************************************
 *  Create the Tabulator instance with sortable columns, the
 *  active-row formatter, and the Select action column.
 ***************************************************************/
function create_table(gobj)
{
    let priv = gobj.priv;
    let t = i18next.t.bind(i18next);

    /*  host: bold + "active" tag when selected  */
    function host_formatter(cell)
    {
        let n = cell.getData();
        let host = n.host || n.uuid || "";
        if(is_active_node(gobj, n)) {
            return `<span class="has-text-weight-bold">${esc(host)}</span>` +
                   `<span class="tag is-success is-light ml-2">${esc(t("active"))}</span>`;
        }
        return esc(host);
    }

    /*  uuid: muted monospace  */
    function uuid_formatter(cell)
    {
        return `<span class="is-family-monospace is-size-7" style="color:#9AA7B4;">` +
               `${esc(cell.getValue() || "")}</span>`;
    }

    /*  action: Select button (none on the active row)  */
    function action_formatter(cell)
    {
        let n = cell.getData();
        if(is_active_node(gobj, n)) {
            return "";
        }
        return `<button class="button is-small is-success is-light" type="button">` +
               `${esc(t("select"))}</button>`;
    }

    function action_click(e, cell)
    {
        let n = cell.getData();
        if(is_active_node(gobj, n)) {
            return;
        }
        on_select(gobj, n);
    }

    /*  green wash + accent on the active row (styles in app.css)  */
    function row_formatter(row)
    {
        let n = row.getData();
        row.getElement().classList.toggle("node-row-active", is_active_node(gobj, n));
    }

    let columns = [
        {title: t("host"),    field: "host",    widthGrow: 2, formatter: host_formatter},
        {title: t("role"),    field: "role",    widthGrow: 1},
        {title: t("version"), field: "version", width: 110},
        {title: t("uuid"),    field: "uuid",    widthGrow: 2, formatter: uuid_formatter},
        {title: "", field: "_action", width: 90, headerSort: false, hozAlign: "right",
            formatter: action_formatter, cellClick: action_click}
    ];

    let settings = {
        index:       "uuid",
        layout:      "fitColumns",
        maxHeight:   "100%",
        placeholder: t("no nodes"),
        columnDefaults: {headerHozAlign: "left", resizable: false},
        columns:     columns,
        rowFormatter: row_formatter
    };

    let table = new Tabulator(`#${priv.table_id}`, settings);
    table._ready = false;
    table.on("tableBuilt", function() {
        table._ready = true;
        if(table._pendingData !== undefined) {
            table.setData(table._pendingData);
            delete table._pendingData;
        }
        update_count(gobj);
    });
    table.on("dataProcessed", () => update_count(gobj));
    gobj_write_attr(gobj, "tabulator", table);
}

/***************************************************************
 *  Push the current node list into the table and refresh the
 *  connected / disconnected state.
 ***************************************************************/
function update_table(gobj)
{
    let priv = gobj.priv;
    let config = gobj_read_attr(gobj, "config_svc");
    priv.active = config ? agent_config_get_active_node(config) : "";

    let nodes = gobj_read_attr(gobj, "nodes");
    if(!Array.isArray(nodes)) {
        nodes = [];
    }

    let table = gobj_read_attr(gobj, "tabulator");
    if(table) {
        if(table._ready) {
            table.replaceData(nodes);
        } else {
            table._pendingData = nodes;
        }
    }
    render_state(gobj);
    update_count(gobj);
}

/***************************************************************
 *  Re-run the row/cell formatters so the active highlight moves
 *  without reloading data (selection changed, not the list).
 ***************************************************************/
function refresh_active(gobj)
{
    let table = gobj_read_attr(gobj, "tabulator");
    if(table && table._ready) {
        table.getRows().forEach((row) => row.reformat());
    }
}

/***************************************************************
 *  Toggle table/toolbar vs the not-connected notice.
 ***************************************************************/
function render_state(gobj)
{
    let priv = gobj.priv;
    let link = gobj_read_attr(gobj, "link_svc");
    let connected = !!(link && agent_link_is_connected(link));

    priv.$toolbar.style.display = connected ? "" : "none";
    priv.$tablewrap.style.display = connected ? "" : "none";
    priv.$notif.style.display = connected ? "none" : "";
}

/***************************************************************
 *  Update the "<shown> / <total>" match count.
 ***************************************************************/
function update_count(gobj)
{
    let priv = gobj.priv;
    let table = gobj_read_attr(gobj, "tabulator");
    if(!priv.$count || !table) {
        return;
    }
    let shown = 0;
    let total = 0;
    try {
        shown = table.getDataCount("active");
        total = table.getDataCount();
    } catch(e) {
        shown = total = 0;
    }
    priv.$count.textContent = (shown === total) ? `${total}` : `${shown} / ${total}`;
}

/***************************************************************
 *  Apply the live search across host / role / version / uuid.
 ***************************************************************/
function apply_filter(gobj)
{
    let priv = gobj.priv;
    let table = gobj_read_attr(gobj, "tabulator");
    if(!table) {
        return;
    }
    let term = String(priv.$input.value || "").trim().toLowerCase();
    if(term) {
        table.setFilter((data) => {
            return ["host", "role", "version", "uuid"].some((k) => {
                return String(data[k] || "").toLowerCase().includes(term);
            });
        });
    } else {
        table.clearFilter();
    }
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
    gobj.priv.active = n.host || n.uuid;
    refresh_active(gobj);
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
    render_state(gobj);
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
    update_table(gobj);
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
