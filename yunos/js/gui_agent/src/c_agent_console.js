/***********************************************************************
 *          c_agent_console.js
 *
 *      C_AGENT_CONSOLE — control-plane CLI to a yuneta agent, a routed
 *      stage view (mounted by C_YUI_SHELL at /console/agent).
 *
 *      The v2 successor of the old webix "Yuneta CLI": a command input
 *      with history + a response area. It does NOT own a transport —
 *      it uses the shared C_AGENT_LINK ("agent_link"), which owns the
 *      single C_IEVENT_CLI to the active agent. The console subscribes
 *      to the link's re-published events (EV_ON_OPEN/CLOSE/OPEN_ERROR/
 *      ID_NAK/MT_COMMAND_ANSWER) and sends commands with
 *      agent_link_command(). Connection state drives the shell's
 *      toolbar dot via yui_shell_set_connection_state().
 *
 *      Auth (jwt for OAuth2 agents) lands in the login service; until a
 *      user signs in, an OAuth2 agent answers the identity card with a
 *      NAK, which this view surfaces.
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
    empty_string,
    msg_iev_get_stack,
    msg_iev_write_key,
    msg_iev_read_key,
    kw_get_str,
} from "@yuneta/gobj-js";

import {t} from "i18next";

import {TabulatorFull as Tabulator} from "tabulator-tables";

import {yui_shell_set_connection_state} from "@yuneta/gobj-ui/src/c_yui_shell.js";
import {attach_clear} from "@yuneta/gobj-ui/src/yui_inputs.js";

import {agent_link_command, agent_link_is_connected} from "./c_agent_link.js";
import {
    agent_config_get_display_mode,
    agent_config_get_history,
    agent_config_set_history,
} from "./c_agent_config.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_AGENT_CONSOLE";

const HISTORY_MAX = 30;

/*  A few common agent commands seeded into the command completions. */
const SEED_COMMANDS = ["help", "list-yunos", "stats", "list-binaries", "view-config"];


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",  0,  null,        "Subscriber of output events"),

SDATA(data_type_t.DTP_STRING,   "title",       0,  "console",   "View title (i18n key)"),
SDATA(data_type_t.DTP_POINTER,  "$container",  0,  null,        "Root HTMLElement"),
SDATA(data_type_t.DTP_POINTER,  "link_svc",    0,  null,        "C_AGENT_LINK service"),
SDATA(data_type_t.DTP_POINTER,  "config_svc",  0,  null,        "C_AGENT_CONFIG service"),
SDATA(data_type_t.DTP_BOOLEAN,  "connected",   0,  false,       "True while in session with the agent"),
SDATA(data_type_t.DTP_STRING,   "node",        0,  "",          "Agent id this panel targets (host/uuid); '' = empty state"),
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
    priv.$hint = null;
    priv.tabulator = null;    /*  Tabulator instance for table-mode answers  */
    priv.commands = {};       /*  name -> {name, params[], desc} from `help`  */
    priv.commands_loaded = false;

    /*  Shell-style history recall (Up/Down): hist_idx walks priv.history
     *  (0 = most recent); -1 = editing the live line. hist_draft keeps the
     *  in-progress text so Down past the newest restores it.  */
    priv.hist_idx = -1;
    priv.hist_draft = "";
    priv.$help_dd = null;      /*  "?" available-commands popover  */
    priv.$help_content = null;
    priv.$hist_dd = null;      /*  history popover  */
    priv.$hist_content = null;
    priv.doc_click = null;     /*  outside-click closer for the popovers  */

    /*
     *  CHILD subscription model
     */
    let subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, null, {}, subscriber);

    /*  Use the shared link; subscribe to its re-published events. */
    let link = gobj_find_service("agent_link", true);
    gobj_write_attr(gobj, "link_svc", link);
    if(link) {
        gobj_subscribe_event(link, "EV_ON_OPEN", {}, gobj);
        gobj_subscribe_event(link, "EV_ON_CLOSE", {}, gobj);
        gobj_subscribe_event(link, "EV_ON_OPEN_ERROR", {}, gobj);
        gobj_subscribe_event(link, "EV_ON_ID_NAK", {}, gobj);
        gobj_subscribe_event(link, "EV_MT_COMMAND_ANSWER", {}, gobj);
    }

    /*  config_svc is used for the persistent display_mode.  This panel
     *  is pinned to a single node (the `node` attr), so it does NOT track
     *  the global active_node. */
    gobj_write_attr(gobj, "config_svc", gobj_find_service("agent_config", true));

    if(empty_string(gobj_read_attr(gobj, "node"))) {
        build_empty_state(gobj);   /*  no node -> "select nodes in Nodos"  */
    } else {
        build_ui(gobj);
    }
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    if(empty_string(gobj_read_attr(gobj, "node"))) {
        return;   /*  empty-state panel: nothing to refresh  */
    }
    refresh_status(gobj);
    ensure_commands_cache(gobj);
}

/***************************************************************
 *  Empty-state panel (no node): shown at the console home when no
 *  nodes are selected. Prompts the operator to pick nodes in Nodos.
 ***************************************************************/
function build_empty_state(gobj)
{
    let $c = createElement2(
        ["div", {class: "C_AGENT_CONSOLE CONSOLE_CARD view-card",
                 style: "display:flex; align-items:center; justify-content:center; height:100%;"},
            [
                ["div", {class: "has-text-grey has-text-centered", style: "max-width:32rem;"},
                    [
                        ["p", {class: "is-size-4 mb-2"},
                            [["span", {class: "icon is-large"}, [["i", {class: "yi-square-js"}]]]]],
                        ["p", {class: "is-size-5", i18n: "no consoles"},
                            "No consoles open"],
                        ["p", {class: "is-size-6 mt-2", i18n: "pick nodes hint"},
                            "Select one or more nodes in Nodes to open a console tab for each."]
                    ]
                ]
            ]
        ]
    );
    gobj_write_attr(gobj, "$container", $c);
    refresh_language($c, t);
}

/***************************************************************
 *  Reflect the real state in the status line + dot. The dot tracks
 *  the control-center link; the message also tells the operator to
 *  pick a node once connected.
 ***************************************************************/
function refresh_status(gobj)
{
    let link = gobj_read_attr(gobj, "link_svc");
    let connected = !!(link && agent_link_is_connected(link));
    let node = gobj_read_attr(gobj, "node") || "";

    if(connected) {
        set_status(gobj, true, node
            ? `${t("connected")} · ${node}`
            : t("select a node"));
    } else {
        /*  Not connected to the control center; link events drive the
         *  detailed message (NAK reason, cannot connect). */
        set_status(gobj, false, "");
    }
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    destroy_table(gobj);
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
    destroy_table(gobj);
    let priv = gobj.priv;
    if(priv.doc_click) {
        document.removeEventListener("click", priv.doc_click);
        priv.doc_click = null;
    }
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

    /*  Restore this node's persisted command history (survives reload and
     *  tab close/reopen); priv.history is the mutable working copy.  */
    let config0 = gobj_read_attr(gobj, "config_svc");
    let node0 = gobj_read_attr(gobj, "node") || "";
    if(config0 && node0) {
        priv.history = agent_config_get_history(config0, node0);
    }

    /*  No native <datalist>: its dropdown hijacks Up/Down to browse command
     *  NAMES, burying the history. Up/Down now recall history (shell-style);
     *  command discovery is served by Tab completion, the live hint line and
     *  the "?" popover. */
    let $input = createElement2(["input", {
        class:        "CONSOLE_INPUT input is-family-monospace",
        type:         "text",
        placeholder:  "help",
        autocomplete: "off",
        "aria-label": "command"
    }, null, {
        keydown: (ev) => {
            if(ev.key === "Enter") {
                ev.preventDefault();
                send_command(gobj);
            } else if(ev.key === "Tab") {
                /*  Complete the command word to the unique match / common
                 *  prefix; only swallow Tab when it actually completes. */
                if(complete_command(gobj)) {
                    ev.preventDefault();
                }
            } else if(ev.key === "ArrowUp") {
                ev.preventDefault();
                recall_history(gobj, +1);   /*  older  */
            } else if(ev.key === "ArrowDown") {
                ev.preventDefault();
                recall_history(gobj, -1);   /*  newer / back to draft  */
            }
        },
        input: () => {
            priv.hist_idx = -1;   /*  typing leaves history-recall mode  */
            update_hint(gobj);
        }
    }]);
    priv.$input = $input;

    let $exec = createElement2(
        ["button", {class: "CONSOLE_EXEC button is-primary", type: "button", i18n: "execute"}, "Execute"],
        t
    );
    $exec.addEventListener("click", () => send_command(gobj));

    /*  No separate Clear button: the input's own ✕ (attach_clear) wipes
     *  the command, and its on_clear also empties the response panel.  */
    let $input_control = createElement2(["div", {class: "CONSOLE_INPUT_CONTROL control is-expanded"}, [$input]]);
    attach_clear($input_control, $input, () => {
        priv.$comment.textContent = "";
        priv.$comment.classList.add("is-hidden");
        priv.$data.textContent = "";
    });

    /*  "?" (available commands) + history popovers. Clicking an item inserts
     *  it into the input to edit — it does NOT auto-run. */
    let help_pop = build_popover(gobj, "HELP", "yi-question", t("help"));
    let hist_pop = build_popover(gobj, "HIST", "yi-arrow-rotate-left", t("command history"));
    priv.$help_dd = help_pop.dd;
    priv.$help_content = help_pop.content;
    priv.$hist_dd = hist_pop.dd;
    priv.$hist_content = hist_pop.content;

    let $status = createElement2(["p", {class: "CONSOLE_STATUS has-text-grey"}, ""]);
    priv.$status = $status;

    /*  Command helper: shows the matched command's signature + description
     *  as you type (from the `help` cache). Hidden when nothing matches. */
    let $hint = createElement2(
        ["p", {class: "CONSOLE_HINT is-size-7 is-family-monospace has-text-grey is-hidden",
               style: "white-space:pre-wrap; margin:0;"}, ""]
    );
    priv.$hint = $hint;

    /*  No hardcoded colours: let Bulma's theme-aware <pre> styling
     *  (--bulma-pre-*) drive background/text so both panels read well in
     *  light AND dark.  */
    let $comment = createElement2(
        ["pre", {class: "CONSOLE_COMMENT is-size-7 is-hidden",
                 style: "white-space:pre-wrap; padding:0.5rem; border-radius:4px; min-height:1.5rem;"},
            ""]
    );
    priv.$comment = $comment;

    /*  Response host: holds either a Tabulator table (schema answers) or
     *  a <pre> with raw JSON / text.  Tabulator manages its own scroll;
     *  the <pre> gets overflow:auto when used.  */
    let $data = createElement2(
        ["div", {class: "CONSOLE_RESPONSE", style: "flex:1; min-height:0;"}]
    );
    priv.$data = $data;

    let $c = createElement2(
        ["div", {class: "C_AGENT_CONSOLE CONSOLE_CARD view-card", style: "display:flex; flex-direction:column; height:100%; gap:0.5rem;"},
            [
                $status,
                $data,
                ["div", {class: "CONSOLE_INPUT_ROW field has-addons mb-0"}, [
                    $input_control,
                    help_pop.control,
                    hist_pop.control,
                    ["div", {class: "control"}, [$exec]]
                ]],
                $hint,
                $comment
            ]
        ]
    );
    gobj_write_attr(gobj, "$container", $c);

    /*  Outside-click closes either popover.  */
    priv.doc_click = (ev) => {
        let in_help = priv.$help_dd && priv.$help_dd.contains(ev.target);
        let in_hist = priv.$hist_dd && priv.$hist_dd.contains(ev.target);
        if(!in_help && !in_hist) {
            close_popovers(gobj);
        }
    };
    document.addEventListener("click", priv.doc_click);

    refresh_language($c, t);
}

/***************************************************************
 *  Push a command onto the in-memory history.
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
    /*  Persist per-node so recall survives reloads / tab reopen.  */
    let config = gobj_read_attr(gobj, "config_svc");
    let node = gobj_read_attr(gobj, "node") || "";
    if(config && node) {
        agent_config_set_history(config, node, priv.history);
    }
}

/***************************************************************
 *  Command helper: cache this node's command list once, by fetching
 *  `help` and parsing it. The answer is tagged console_purpose:"cache"
 *  in __md_iev__ so ac_mt_command_answer routes it here (not to the
 *  response panel).
 ***************************************************************/
function ensure_commands_cache(gobj)
{
    let priv = gobj.priv;
    if(priv.commands_loaded) {
        return;
    }
    let node = gobj_read_attr(gobj, "node") || "";
    let link = gobj_read_attr(gobj, "link_svc");
    if(!node || !link || !agent_link_is_connected(link)) {
        return;
    }
    let kw_send = {agent_id: node, cmd2agent: "help"};
    msg_iev_write_key(kw_send, "console_node", node);
    msg_iev_write_key(kw_send, "console_purpose", "cache");
    agent_link_command(link, "command-agent", kw_send);
}

/***************************************************************
 *  Parse a `help` answer into {name -> {name, params[], desc}}.
 *  Command lines look like:
 *    - list-yunos            (1 ) [id='?'] [realm_id='?'] ... . List all yunos
 ***************************************************************/
function parse_help(text)
{
    let cmds = {};
    let lines = String(text || "").split("\n");
    for(let line of lines) {
        let m = /^-\s+(\S+)\s*(?:\(([^)]*)\))?\s*(.*)$/.exec(line);
        if(!m) {
            continue;
        }
        let name = m[1];
        let rest = m[3] || "";
        let params = [];
        let re = /\[([a-zA-Z0-9_]+)=/g;
        let pm;
        while((pm = re.exec(rest)) !== null) {
            params.push(pm[1]);
        }
        let desc = rest.replace(/\[[^\]]*\]/g, "").replace(/^[\s.]+/, "").trim();
        cmds[name] = {name: name, params: params, desc: desc};
    }
    return cmds;
}

/***************************************************************
 *  Store the parsed command cache + refresh the hint.
 ***************************************************************/
function load_commands_cache(gobj, help_text)
{
    let priv = gobj.priv;
    priv.commands = parse_help(help_text);
    priv.commands_loaded = true;
    update_hint(gobj);
}

/***************************************************************
 *  Shell-style history recall. dir=+1 older, dir=-1 newer. On the first
 *  Up we stash the live line (hist_draft) so Down past the newest brings
 *  it back. priv.history[0] is the most recent command.
 ***************************************************************/
function recall_history(gobj, dir)
{
    let priv = gobj.priv;
    if(!priv.$input || priv.history.length === 0) {
        return;
    }
    if(priv.hist_idx === -1 && dir > 0) {
        priv.hist_draft = priv.$input.value;
    }
    let idx = priv.hist_idx + dir;
    if(idx >= priv.history.length) {
        idx = priv.history.length - 1;   /*  clamp at the oldest  */
    }
    if(idx < 0) {
        priv.hist_idx = -1;
        priv.$input.value = priv.hist_draft;   /*  back to the live line  */
    } else {
        priv.hist_idx = idx;
        priv.$input.value = priv.history[idx];
    }
    let n = priv.$input.value.length;
    priv.$input.setSelectionRange(n, n);
    update_hint(gobj);
}

/***************************************************************
 *  Put text in the input (replacing it), focus + move caret to end.
 *  Used by the "?" and history popovers — it does NOT execute.
 ***************************************************************/
function insert_command(gobj, text)
{
    let priv = gobj.priv;
    if(!priv.$input) {
        return;
    }
    priv.$input.value = text;
    priv.hist_idx = -1;
    close_popovers(gobj);
    priv.$input.focus();
    let n = priv.$input.value.length;
    priv.$input.setSelectionRange(n, n);
    update_hint(gobj);
}

/***************************************************************
 *  Build a Bulma dropdown popover (theme-aware, self-contained) with an
 *  icon trigger. kind is "HELP" | "HIST"; content is filled on open.
 *  Returns {control, dd, content} for wiring into the input row.
 ***************************************************************/
function build_popover(gobj, kind, icon, title)
{
    let $content = createElement2(
        ["div", {class: "dropdown-content", style: "max-height:22rem; overflow:auto;"}, []]
    );
    let $btn = createElement2(
        ["button", {class: "button", type: "button", "aria-haspopup": "true", title: title},
            [["span", {class: "icon"}, [["i", {class: icon}]]]]]
    );
    let $dd = createElement2(
        ["div", {class: `CONSOLE_${kind}_DD dropdown is-up is-right`}, [
            ["div", {class: "dropdown-trigger"}, [$btn]],
            ["div", {class: "dropdown-menu", role: "menu"}, [$content]]
        ]]
    );
    $btn.addEventListener("click", (ev) => {
        ev.stopPropagation();   /*  don't let the doc handler close it first  */
        toggle_popover(gobj, kind);
    });
    return {control: createElement2(["div", {class: "control"}, [$dd]]), dd: $dd, content: $content};
}

/***************************************************************
 *  Open one popover (filling it), closing the other.
 ***************************************************************/
function toggle_popover(gobj, kind)
{
    let priv = gobj.priv;
    let dd = (kind === "HELP") ? priv.$help_dd : priv.$hist_dd;
    let other = (kind === "HELP") ? priv.$hist_dd : priv.$help_dd;
    if(!dd) {
        return;
    }
    if(other) {
        other.classList.remove("is-active");
    }
    let open = !dd.classList.contains("is-active");
    dd.classList.toggle("is-active", open);
    if(open) {
        if(kind === "HELP") {
            fill_help_popover(gobj);
        } else {
            fill_hist_popover(gobj);
        }
    }
}

/***************************************************************
 *  Close both popovers.
 ***************************************************************/
function close_popovers(gobj)
{
    let priv = gobj.priv;
    if(priv.$help_dd) {
        priv.$help_dd.classList.remove("is-active");
    }
    if(priv.$hist_dd) {
        priv.$hist_dd.classList.remove("is-active");
    }
}

/***************************************************************
 *  Fill the "?" popover: available commands (seed + cached), each
 *  showing its name + parameter signature; the description is the
 *  tooltip. Clicking inserts "name " ready for parameters.
 ***************************************************************/
function fill_help_popover(gobj)
{
    let priv = gobj.priv;
    let $c = priv.$help_content;
    if(!$c) {
        return;
    }
    $c.replaceChildren();
    let names = command_names(gobj).sort();
    if(names.length === 0) {
        $c.appendChild(createElement2(
            ["div", {class: "dropdown-item has-text-grey", i18n: "no commands"}, t("no commands")]));
        return;
    }
    for(let name of names) {
        let cmd = priv.commands[name];
        let sig = (cmd && cmd.params && cmd.params.length)
            ? " " + cmd.params.map((p) => `[${p}]`).join(" ") : "";
        let $item = createElement2(
            ["a", {class: "dropdown-item is-family-monospace",
                   style: "white-space:normal;", title: (cmd && cmd.desc) ? cmd.desc : ""},
                [
                    ["span", {class: "has-text-weight-semibold"}, name],
                    ["span", {class: "has-text-grey"}, sig]
                ]
            ]
        );
        $item.addEventListener("click", (ev) => {
            ev.preventDefault();
            insert_command(gobj, name + " ");
        });
        $c.appendChild($item);
    }
}

/***************************************************************
 *  Fill the history popover: recent commands, most-recent first.
 *  Clicking inserts the full command line.
 ***************************************************************/
function fill_hist_popover(gobj)
{
    let priv = gobj.priv;
    let $c = priv.$hist_content;
    if(!$c) {
        return;
    }
    $c.replaceChildren();
    if(priv.history.length === 0) {
        $c.appendChild(createElement2(
            ["div", {class: "dropdown-item has-text-grey", i18n: "no history yet"}, t("no history yet")]));
        return;
    }
    for(let cmd of priv.history) {
        let $item = createElement2(
            ["a", {class: "dropdown-item is-family-monospace", style: "white-space:normal;", title: cmd}, cmd]
        );
        $item.addEventListener("click", (ev) => {
            ev.preventDefault();
            insert_command(gobj, cmd);
        });
        $c.appendChild($item);
    }
}

/***************************************************************
 *  Show the current command's signature under the input.
 ***************************************************************/
function update_hint(gobj)
{
    let priv = gobj.priv;
    if(!priv.$hint || !priv.$input) {
        return;
    }
    let val = String(priv.$input.value || "").replace(/^\*/, "").trimStart();
    let word = val.split(/\s+/)[0] || "";
    let cmd = word ? priv.commands[word] : null;
    if(cmd) {
        let sig = (cmd.params.length ? " " + cmd.params.map((p) => `[${p}]`).join(" ") : "");
        let desc = cmd.desc ? `  —  ${cmd.desc}` : "";
        priv.$hint.textContent = word + sig + desc;
        priv.$hint.classList.remove("is-hidden");
    } else {
        priv.$hint.textContent = "";
        priv.$hint.classList.add("is-hidden");
    }
}

/***************************************************************
 *  All known command names (seed + cached).
 ***************************************************************/
function command_names(gobj)
{
    let pool = new Set(SEED_COMMANDS);
    for(let name in gobj.priv.commands) {
        pool.add(name);
    }
    return Array.from(pool);
}

/***************************************************************
 *  Longest common prefix of a non-empty string array.
 ***************************************************************/
function longest_common_prefix(arr)
{
    return arr.reduce((a, b) => {
        let i = 0;
        while(i < a.length && i < b.length && a[i] === b[i]) {
            i++;
        }
        return a.slice(0, i);
    });
}

/***************************************************************
 *  Tab completion of the LAST token: the command name when still on
 *  the first word, otherwise a parameter of that command (the ones
 *  not yet used on the line), completed to the common prefix / unique
 *  match.  Params complete with a trailing '=' (ready for the value).
 *  Returns true when it changed the input.
 ***************************************************************/
function complete_command(gobj)
{
    let priv = gobj.priv;
    let raw = String(priv.$input.value || "");
    let star = raw.charAt(0) === "*" ? "*" : "";
    let body = star ? raw.slice(1) : raw;

    let last_space = body.lastIndexOf(" ");
    let cur = body.slice(last_space + 1);        /*  token being typed  */
    let before = body.slice(0, last_space + 1);  /*  keeps the trailing space  */

    let candidates;
    if(last_space < 0) {
        /*  Completing the command name.  */
        candidates = command_names(gobj);
    } else {
        /*  Completing a parameter of the first word.  */
        let name = body.trimStart().split(/\s+/)[0] || "";
        let cmd = priv.commands[name];
        if(!cmd || !cmd.params.length) {
            return false;
        }
        let used = {};
        let re = /([a-zA-Z0-9_]+)=/g;
        let m;
        while((m = re.exec(body)) !== null) {
            used[m[1]] = true;
        }
        candidates = cmd.params
            .filter((p) => !used[p])
            .map((p) => `${p}=`);
    }

    let matches = candidates.filter((c) => c.indexOf(cur) === 0);
    if(matches.length === 0) {
        return false;
    }
    let common = longest_common_prefix(matches);
    if(common.length > cur.length) {
        priv.$input.value = star + before + common;
        update_hint(gobj);
        return true;
    }
    if(matches.length === 1) {
        /*  Command names get a trailing space; params already end with '='. */
        let suffix = (last_space < 0) ? " " : "";
        priv.$input.value = star + before + matches[0] + suffix;
        update_hint(gobj);
        return true;
    }
    return false;
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
    let is_error = typeof result === "number" && result < 0;
    let text = comment || "";
    priv.$comment.textContent = text;
    priv.$comment.classList.toggle("has-text-danger", is_error);
    /*  Only surface the comment line on an error answer; on success the
     *  payload goes to CONSOLE_RESPONSE, so keep it hidden to reclaim
     *  vertical space (matters on mobile). */
    priv.$comment.classList.toggle("is-hidden", !(is_error && text !== ""));
}

/***************************************************************
 *  Render a command answer's payload into CONSOLE_RESPONSE.
 *
 *  Mirrors ycommand's display_webix_result mapped to the console:
 *    - array + table mode + schema  -> interactive Tabulator table
 *    - array + form/no-schema       -> raw JSON
 *    - object                       -> raw JSON
 *    - no data but a (non-error)    -> paint the comment; this is how
 *      comment (e.g. `help`)           `help` arrives: text in comment,
 *                                       data + schema null.
 *  Errors keep their short comment in the CONSOLE_COMMENT line.
 ***************************************************************/
function show_data(gobj, data, schema, raw, comment, result)
{
    let priv = gobj.priv;
    if(!priv.$data) {
        return;
    }

    /*  Tear down any previous table + content.  */
    destroy_table(gobj);
    priv.$data.replaceChildren();

    /*  Table mode: array payload + schema -> Tabulator.  */
    if(!raw && Array.isArray(data) && Array.isArray(schema) && schema.length) {
        render_table(gobj, schema, data);
        return;
    }

    /*  Otherwise paint "what arrives": the data as raw JSON/text, or the
     *  comment when the answer carries its text there (help) and is not
     *  an error.  */
    let text = null;
    if(data !== null && data !== undefined) {
        if(typeof data === "string") {
            text = data;
        } else {
            try {
                text = JSON.stringify(data, null, 2);
            } catch(e) {
                text = String(data);
            }
        }
    } else if(comment && !(typeof result === "number" && result < 0)) {
        text = comment;
    }

    if(text === null || text === "") {
        return;
    }
    priv.$data.appendChild(createElement2(
        ["pre", {class: "CONSOLE_RESPONSE_TEXT",
                 style: "margin:0; height:100%; overflow:auto; white-space:pre-wrap; " +
                        "font-size:12px; padding:0.5rem;"},
            text]
    ));
}

/***************************************************************
 *  Build a Tabulator table from a ycommand-style schema. Follows the
 *  treedb table convention: skip internal columns (id starting with
 *  '_') and fillspace==0 columns; boolean -> tickCross; integer/real
 *  -> right-aligned numeric.
 ***************************************************************/
function render_table(gobj, schema, data)
{
    let priv = gobj.priv;
    let host = createElement2(["div", {class: "CONSOLE_RESPONSE_TABLE"}]);
    priv.$data.appendChild(host);

    priv.tabulator = new Tabulator(host, {
        data:           data,
        layout:         "fitDataFill",
        columns:        make_columns_from_schema(schema),
        columnDefaults: {headerHozAlign: "left"},
        maxHeight:      "100%",
        placeholder:    "—"
    });
}

/***************************************************************
 *  Map a ycommand schema (cols with id/header/fillspace/type) to
 *  Tabulator column definitions.
 ***************************************************************/
function make_columns_from_schema(schema)
{
    let columns = [];
    for(let col of schema) {
        if(!col.id || col.id[0] === "_") {
            continue;   /*  internal / hidden column  */
        }
        if(col.fillspace === 0) {
            continue;   /*  explicitly hidden (ycommand: fillspace 0)  */
        }
        let colDef = {
            title: col.header || col.id,
            field: col.id
        };
        switch(col.type) {
            case "boolean":
                colDef.hozAlign = "center";
                colDef.sorter = "boolean";
                colDef.formatter = "tickCross";
                break;
            case "integer":
            case "real":
                colDef.hozAlign = "right";
                colDef.sorter = "number";
                colDef.formatter = cell_scalar_or_json;
                break;
            default:
                colDef.formatter = cell_scalar_or_json;
                break;
        }
        columns.push(colDef);
    }
    return columns;
}

/***************************************************************
 *  Tabulator cell formatter: scalars as text, objects/arrays as
 *  compact JSON (Tabulator can't render objects). HTML-escaped.
 ***************************************************************/
function cell_scalar_or_json(cell)
{
    let v = cell.getValue();
    if(v === null || v === undefined) {
        return "";
    }
    if(typeof v === "object") {
        return esc_html(JSON.stringify(v));
    }
    return esc_html(String(v));
}

/***************************************************************
 *  Minimal HTML escaping for values inserted as Tabulator cell HTML.
 ***************************************************************/
function esc_html(s)
{
    return String(s)
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;");
}

/***************************************************************
 *  Destroy the current Tabulator instance, if any.
 ***************************************************************/
function destroy_table(gobj)
{
    let priv = gobj.priv;
    if(priv.tabulator) {
        try {
            priv.tabulator.destroy();
        } catch(e) {
            /*  already gone  */
        }
        priv.tabulator = null;
    }
}

/***************************************************************
 *  Send the typed command to the ACTIVE NODE's agent, routed by
 *  the control center: command-agent agent_id=<node> cmd2agent=<cmd>.
 ***************************************************************/
function send_command(gobj)
{
    let priv = gobj.priv;
    let cmd = priv.$input ? priv.$input.value.trim() : "";
    if(!cmd) {
        return;
    }

    /*  Like ycommand: the display mode is the persistent `display_mode`
     *  attr (default "table"); a leading '*' overrides it to "form" (raw
     *  JSON) for this one command. The '*' is a client-side flag — strip
     *  it before sending.  */
    let config = gobj_read_attr(gobj, "config_svc");
    let mode = config ? agent_config_get_display_mode(config) : "table";
    if(cmd.charAt(0) === "*") {
        mode = "form";
        cmd = cmd.slice(1).trim();
        if(!cmd) {
            return;
        }
    }

    let link = gobj_read_attr(gobj, "link_svc");
    if(!link || !gobj_read_attr(gobj, "connected")) {
        show_comment(gobj, t("not connected to an agent"), -1);
        return;
    }

    let node = gobj_read_attr(gobj, "node") || "";
    if(!node) {
        show_comment(gobj, t("select a node"), -1);
        return;
    }

    add_history(gobj, cmd);
    priv.hist_idx = -1;   /*  a sent command resets history-recall  */
    show_comment(gobj, "…", 0);
    show_data(gobj, null);

    /*  src defaults to the link service; the answer routes back there
     *  and is re-published to EVERY console panel.  Both the display
     *  mode and this panel's node id travel in __md_iev__ and are
     *  echoed back, so each panel renders only its own node's answer
     *  ([[__md_iev__ round-trip]] / ycommand parity).  */
    let kw_send = {agent_id: node, cmd2agent: cmd};
    msg_iev_write_key(kw_send, "display_mode", mode);
    msg_iev_write_key(kw_send, "console_node", node);
    agent_link_command(link, "command-agent", kw_send);
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
    set_status(gobj, true, `${t("connected")} · ${remote}`);
    ensure_commands_cache(gobj);
    return 0;
}

/***************************************************************
 *  Channel dropped after a real session.
 ***************************************************************/
function ac_on_close(gobj, event, kw, src)
{
    set_status(gobj, false, t("disconnected"));
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
    set_status(gobj, false, t("disconnected"));
    show_comment(gobj,
        `${t("cannot connect")}: ${kw.url || ""} (${kw.reason || kw.code || ""})`, -1);
    return 0;
}

/***************************************************************
 *  Identity card refused (auth required / rejected).
 ***************************************************************/
function ac_on_id_nak(gobj, event, kw, src)
{
    gobj.priv.got_nak = true;
    set_status(gobj, false, t("authentication required"));
    show_comment(gobj, kw.comment || t("identity card refused"), -1);
    return 0;
}

/***************************************************************
 *  Effective display mode for an answer: the value echoed back in
 *  __md_iev__ (set when the command was sent) wins; otherwise fall
 *  back to the persistent `display_mode` attr. "form" => raw JSON.
 ***************************************************************/
function answer_is_raw(gobj, kw)
{
    let mode = msg_iev_read_key(kw, "display_mode");
    if(!mode) {
        let config = gobj_read_attr(gobj, "config_svc");
        mode = config ? agent_config_get_display_mode(config) : "table";
    }
    return mode === "form";
}

/***************************************************************
 *  Command answer. The shared link re-publishes every answer; only
 *  render our own command-agent results (the node picker handles
 *  list-agents). Filter by the command in the command_stack.
 ***************************************************************/
function ac_mt_command_answer(gobj, event, kw, src)
{
    let stk = msg_iev_get_stack(gobj, kw, "command_stack", false);
    let command = kw_get_str(gobj, stk, "command", "", 0);

    /*  The Nodes panel's own answer — not ours.  */
    if(command === "list-agents") {
        return 0;
    }

    /*  Multi-agent: each command echoes its origin node in __md_iev__;
     *  render only answers for THIS panel's node.  */
    let my_node = gobj_read_attr(gobj, "node") || "";
    let ans_node = msg_iev_read_key(kw, "console_node");
    if(my_node && ans_node && ans_node !== my_node) {
        return 0;
    }

    /*  command-agent forwards cmd2agent to the agent and returns TWO
     *  answers: (1) the controlcenter's synchronous dispatch ack
     *  ("Command sent to N nodes"), and (2) the agent's asynchronous
     *  real answer, whose stack carries the INNER command we typed.
     *  Surface the ack only when the dispatch itself failed (no agent
     *  matched / not authorized) — on success just wait for (2).  */
    if(command === "command-agent") {
        if(typeof kw.result === "number" && kw.result < 0) {
            show_comment(gobj, kw.comment, kw.result);
            show_data(gobj, kw.data, kw.schema, answer_is_raw(gobj, kw), kw.comment, kw.result);
        }
        return 0;
    }

    /*  Command-cache fetch (a `help` sent by ensure_commands_cache):
     *  parse it into the completion cache instead of rendering it. */
    if(msg_iev_read_key(kw, "console_purpose") === "cache") {
        load_commands_cache(gobj, kw.comment);
        return 0;
    }

    /*  Anything else is the agent's real answer to our command.  */
    show_comment(gobj, kw.comment, kw.result);
    show_data(gobj, kw.data, kw.schema, answer_is_raw(gobj, kw), kw.comment, kw.result);
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
            ["EV_MT_COMMAND_ANSWER", ac_mt_command_answer, null]
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
function register_c_agent_console()
{
    return create_gclass(GCLASS_NAME);
}

export {register_c_agent_console};
