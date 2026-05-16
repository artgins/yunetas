/***********************************************************************
 *          c_yui_gobj_tree_js.js
 *
 *          Hierarchical tree graph (G6) of the gobj tree of the own yuno.
 *
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA,
    SDATA_END,
    data_type_t,
    event_flag_t,
    gclass_create,
    log_error,
    gobj_read_pointer_attr,
    gobj_parent,
    gobj_subscribe_event,
    gobj_name,
    clean_name,
    gobj_read_attr,
    gobj_write_attr,
    gobj_send_event,
    gobj_find_service,
    gobj_write_str_attr,
    gobj_read_str_attr,
    gobj_publish_event,
    gobj_yuno,
    gobj_gclass_name,
    gobj_short_name,
    gobj_full_name,
    gobj_current_state,
    gobj_is_running,
    gobj_is_playing,
    gobj_is_service,
    gobj_is_pure_child,
    gobj_is_volatil,
    createElement2,
    escapeHtml,
    refresh_language,
} from "@yuneta/gobj-js";

import {yui_toolbar} from "@yuneta/lib-yui";

import {t} from "i18next";

import {
    BaseLayout,
    ExtensionCategory,
    Graph,
    NodeEvent,
    CanvasEvent,
    register,
} from '@antv/g6';

import { ensure_drag_canvas_patch } from "./g6_drag_canvas_touch.js";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_GOBJ_TREE_JS";

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
/*---------------- Public Attributes ----------------*/
SDATA(data_type_t.DTP_POINTER,  "subscriber",       0,  null,   "Subscriber of output events"),

/*---------------- Sub-container ----------------*/
SDATA(data_type_t.DTP_POINTER,  "$container",       0,  null,   "Container element"),
SDATA(data_type_t.DTP_STRING,   "canvas_id",        0,  "",     "Canvas ID"),

/*---------------- Graph Settings ----------------*/
SDATA(data_type_t.DTP_STRING,   "wide",             0,  "36px", "Height of header"),
SDATA(data_type_t.DTP_STRING,   "layout",           0,  "vertical-compact", "Current layout key"),
SDATA(data_type_t.DTP_INTEGER,  "collapse_threshold", 0, 10, "Auto-collapse a node whose direct children exceed this number. 0 disables auto-collapse."),

SDATA_END()
];

let PRIVATE_DATA = {
    canvas_id: "",
    graph: null,
    node_counter: 0,
    theme: null,
    $layout_select: null,
    $popover: null,
    $popover_title: null,
    $popover_body: null,
    node_by_id: null,       // map of node_id -> node_data (for popover)
    collapse_state: null,   // map of full_name -> "collapsed" | "expanded"
    resize_observer: null,  // ResizeObserver on the canvas mount element
    _resize_raf: 0,         // rAF id debouncing resize -> EV_RESIZE
};

let __gclass__ = null;
let __layout_registered__ = false;

/***************************************************************
 *  Node colors by role
 ***************************************************************/
const ROLE_COLORS = {
    yuno:    {fill: "#E0F2FE", stroke: "#075985"}, // blue
    service: {fill: "#DCFCE7", stroke: "#166534"}, // green
    pure:    {fill: "#FEF9C3", stroke: "#854D0E"}, // yellow
    volatil: {fill: "#FCE7F3", stroke: "#9D174D"}, // pink
    default: {fill: "#F3F4F6", stroke: "#374151"}, // gray
};

const STATE_COLORS = {
    running_playing: "#15803D",    // green
    running:         "#B45309",    // amber
    stopped:         "#B91C1C",    // red
};

/***************************************************************
 *  Fixed node sizes per role (px).
 *  Services are drawn bigger than pure children to visually
 *  emphasise the service boundary in the hierarchy.
 ***************************************************************/
const NODE_SIZES = {
    full: {
        yuno:    {w: 240, h: 72},
        service: {w: 210, h: 68},
        child:   {w: 160, h: 60},
    },
    compact: {
        yuno:    {w: 210, h: 30},
        service: {w: 185, h: 26},
        child:   {w: 150, h: 22},
    },
};

/***************************************************************
 *  Shared card typography.
 ***************************************************************/
const GT_FONT =
    "-apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, " +
    "Helvetica, Arial, sans-serif";

/***************************************************************
 *  True when the app is in dark theme (<html data-theme>).
 ***************************************************************/
function gt_is_dark()
{
    return (typeof document !== "undefined") &&
        document.documentElement.getAttribute("data-theme") === "dark";
}

/***************************************************************
 *  Soft, theme-aware card palette derived from the role colour
 *  (same visual language as the treedb graph cards): light tint
 *  fill + role-colour border, brightened on dark.
 ***************************************************************/
function role_card_style(stroke, dark)
{
    // On dark, lift the card off the near-black canvas with a
    // clearly-lighter slate base + vivid border (a faint tint on
    // the canvas colour makes the cards invisible).
    return {
        bg: dark
            ? `color-mix(in srgb, ${stroke} 30%, #2c3542)`
            : `color-mix(in srgb, ${stroke} 9%, #ffffff)`,
        border: dark
            ? `color-mix(in srgb, ${stroke} 85%, #ffffff)`
            : stroke,
        title: dark ? "#e8eaed" : "#0f172a",
        sub: dark ? "#9aa4b2" : "#64748b",
        tagbg: dark
            ? `color-mix(in srgb, ${stroke} 42%, #2c3542)`
            : `color-mix(in srgb, ${stroke} 14%, #ffffff)`,
        shadow: dark
            ? "0 1px 3px rgba(0,0,0,0.45)"
            : "0 1px 3px rgba(15,23,42,0.12)",
    };
}

/***************************************************************
 *  Map a gobj to its visual category.
 ***************************************************************/
function get_node_category(target_gobj, is_root)
{
    if(is_root) {
        return "yuno";
    }
    if(gobj_is_service(target_gobj)) {
        return "service";
    }
    return "child";
}

/***************************************************************
 *  Layouts registry.
 *
 *    orientation: "V" top->down | "H" left->right
 *    compact:     one-line nodes (narrow); full:  multi-line nodes
 *    g6_layout:   G6 layout config object applied to the graph
 *    edge_type:   matching G6 edge type
 ***************************************************************/
const LAYOUTS = {
    "vertical": {
        label: "Vertical",
        orientation: "V",
        compact: false,
        g6_layout: {type: 'gobj-tree-v'},
        edge_type: 'cubic-vertical',
    },
    "vertical-compact": {
        label: "Vertical compact",
        orientation: "V",
        compact: true,
        g6_layout: {type: 'gobj-tree-v'},
        edge_type: 'cubic-vertical',
    },
    "horizontal": {
        label: "Horizontal",
        orientation: "H",
        compact: false,
        g6_layout: {type: 'gobj-tree-h'},
        edge_type: 'cubic-horizontal',
    },
    "horizontal-compact": {
        label: "Horizontal compact",
        orientation: "H",
        compact: true,
        g6_layout: {type: 'gobj-tree-h'},
        edge_type: 'cubic-horizontal',
    },
    "lanes-v": {
        label: "Lanes vertical",
        orientation: "V",
        compact: true,
        g6_layout: {type: 'gobj-lanes-v'},
        edge_type: 'polyline',
    },
    "lanes-h": {
        label: "Lanes horizontal",
        orientation: "H",
        compact: true,
        g6_layout: {type: 'gobj-lanes-h'},
        edge_type: 'polyline',
    },
    "dagre-tb": {
        label: "Dagre (top → bottom)",
        orientation: "V",
        compact: true,
        g6_layout: {type: 'antv-dagre', rankdir: 'TB', nodesep: 20, ranksep: 40},
        edge_type: 'polyline',
    },
    "dagre-lr": {
        label: "Dagre (left → right)",
        orientation: "H",
        compact: true,
        g6_layout: {type: 'antv-dagre', rankdir: 'LR', nodesep: 20, ranksep: 40},
        edge_type: 'polyline',
    },
};

function get_layout_cfg(layout_key)
{
    return LAYOUTS[layout_key] || LAYOUTS["vertical-compact"];
}




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************
 *          Framework Method: Create
 ***************************************************************/
function mt_create(gobj)
{
    let priv = gobj.priv;

    let name = clean_name(gobj_name(gobj));
    priv.canvas_id = "gobj-tree-canvas-" + name;
    gobj_write_str_attr(gobj, "canvas_id", priv.canvas_id);

    priv.collapse_state = {};

    /*
     *  CHILD subscription model
     */
    let subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, null, {}, subscriber);

    /*  Optional legacy integration with the old C_YUI_MAIN shell.
     *  Under the new C_YUI_SHELL there is no __yui_main__ — look it
     *  up SILENTLY (no verbose) so its absence is not logged as an
     *  error, and fall back to the <html data-theme> convention. */
    let __yui_main__ = gobj_find_service("__yui_main__");
    if(__yui_main__) {
        priv.theme = gobj_read_str_attr(__yui_main__, "theme");
    } else {
        priv.theme = gt_is_dark() ? "dark" : "light";
    }

    build_ui(gobj);

    ensure_drag_canvas_patch();

    if(!__layout_registered__) {
        register(ExtensionCategory.LAYOUT, 'gobj-tree-v', GobjTreeVLayout);
        register(ExtensionCategory.LAYOUT, 'gobj-tree-h', GobjTreeHLayout);
        register(ExtensionCategory.LAYOUT, 'gobj-lanes-v', GobjLanesVLayout);
        register(ExtensionCategory.LAYOUT, 'gobj-lanes-h', GobjLanesHLayout);
        __layout_registered__ = true;
    }
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    build_graph(gobj);
    load_tree(gobj);
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
    let priv = gobj.priv;

    if(priv.resize_observer) {
        priv.resize_observer.disconnect();
        priv.resize_observer = null;
    }
    if(priv._resize_raf) {
        cancelAnimationFrame(priv._resize_raf);
        priv._resize_raf = 0;
    }

    if(priv.graph) {
        priv.graph.destroy();
        priv.graph = null;
    }

    destroy_ui(gobj);

    priv.$popover = null;
    priv.$popover_title = null;
    priv.$popover_body = null;
    priv.$layout_select = null;
    priv.node_by_id = null;
    priv.collapse_state = null;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/************************************************************
 *   Build UI
 ************************************************************/
function build_ui(gobj)
{
    let priv = gobj.priv;

    let $toolbar = make_toolbar(gobj);

    let $container = createElement2(
        ['div', {class: 'gobj-tree', style: 'height:100%; display:flex; flex-direction:column; position:relative;'}, [
            ['div', {class: 'is-flex-grow-0 is-flex toolbar_yui_gobj_tree'}, $toolbar],
            ['div', {class: 'is-flex-grow-1', style: 'height:100%; min-height:0; overflow:hidden;'}, [
                ['div', {id: priv.canvas_id, class: 'gobj-tree-container', style: 'height:100%; min-height:0; border: 1px solid var(--bulma-border-weak); border-radius:0.2rem;'}, [
                ]]
            ]]
        ]]
    );

    gobj_write_attr(gobj, "$container", $container);
    build_popover(gobj, $container);

    /*
     *  Delegated click handler for the per-node +/- toggle.
     *  Runs in capture phase so it can stop the click from
     *  reaching G6 (which would otherwise also fire a node
     *  click and open the popover).
     */
    $container.addEventListener("click", (evt) => {
        let target = evt.target;
        if(!target || typeof target.closest !== "function") {
            return;
        }
        let $toggle = target.closest('[data-gobj-toggle="true"]');
        if(!$toggle) {
            return;
        }
        evt.stopPropagation();
        evt.stopImmediatePropagation();
        evt.preventDefault();

        let node_id = $toggle.getAttribute("data-node-id");
        if(node_id) {
            gobj_send_event(gobj, "EV_TOGGLE_COLLAPSE", {node_id: node_id}, gobj);
        }
    }, true);

    refresh_language($container, t);
}

/************************************************************
 *   Build node popover (hidden by default).
 *   Kept as a persistent DOM subtree so listeners on the
 *   close button survive popover refreshes.
 ************************************************************/
function build_popover(gobj, $container)
{
    let priv = gobj.priv;

    let $popover = document.createElement("div");
    $popover.className = "gobj-tree-popover";
    $popover.style.cssText = `
        position: absolute;
        display: none;
        top: 0;
        left: 0;
        background: var(--bulma-scheme-main, #fff);
        color: var(--bulma-text-strong, #1A1A1A);
        border: 1px solid var(--bulma-border-weak, #9CA3AF);
        border-radius: 6px;
        box-shadow: 0 4px 14px rgba(0, 0, 0, 0.18);
        min-width: 260px;
        max-width: 380px;
        font-family: sans-serif;
        font-size: 12px;
        z-index: 1000;
    `;
    $popover.addEventListener("click", (evt) => {
        evt.stopPropagation();
    });

    let $header = document.createElement("div");
    $header.style.cssText = `
        display: flex;
        align-items: center;
        padding: 4px 8px;
        background: var(--bulma-scheme-main-bis, #F3F4F6);
        border-bottom: 1px solid var(--bulma-border-weak, #E5E7EB);
        border-top-left-radius: 6px;
        border-top-right-radius: 6px;
    `;

    let $title = document.createElement("span");
    $title.style.cssText = `
        flex: 1;
        font-weight: bold;
        font-size: 12px;
        white-space: nowrap;
        overflow: hidden;
        text-overflow: ellipsis;
    `;
    $header.appendChild($title);

    let $close = document.createElement("button");
    $close.type = "button";
    $close.textContent = "×";
    $close.title = t("Close");
    $close.style.cssText = `
        background: transparent;
        border: none;
        cursor: pointer;
        font-size: 18px;
        line-height: 1;
        padding: 0 4px;
        color: inherit;
    `;
    $close.addEventListener("click", (evt) => {
        evt.stopPropagation();
        hide_popover(gobj);
    });
    $header.appendChild($close);

    let $body = document.createElement("div");
    $body.style.cssText = `
        padding: 6px 8px;
        max-height: 40vh;
        overflow: auto;
    `;

    $popover.appendChild($header);
    $popover.appendChild($body);
    $container.appendChild($popover);

    priv.$popover = $popover;
    priv.$popover_title = $title;
    priv.$popover_body = $body;
}

/************************************************************
 *   Destroy UI
 ************************************************************/
function destroy_ui(gobj)
{
    let $container = gobj_read_attr(gobj, "$container");
    if($container) {
        if($container.parentNode) {
            $container.parentNode.removeChild($container);
        }
        gobj_write_attr(gobj, "$container", null);
    }
}

/************************************************************
 *   Toolbar
 ************************************************************/
function make_toolbar(gobj)
{
    let priv = gobj.priv;
    let toolbar_wide = gobj_read_attr(gobj, "wide");
    let current_layout = gobj_read_str_attr(gobj, "layout");

    let left_items = [];
    let center_items = [];
    let right_items = [];

    let c_icons = [
        ["yi-magnifying-glass-plus",  "EV_ZOOM_IN",      false, 'i'],
        ["yi-magnifying-glass",       "EV_ZOOM_RESET",   false, 'i'],
        ["yi-magnifying-glass-minus", "EV_ZOOM_OUT",     false, 'i'],
        ["yi-arrows-to-eye",          "EV_CENTER",       false, 'i'],
        ["yi-eye",                    "EV_EXPAND_ALL",   false, 'i'],
        ["yi-eye-slash",              "EV_COLLAPSE_ALL", false, 'i'],
        ["yi-arrows-rotate",          "EV_REFRESH",      false, 'i'],
    ];

    for(let item of c_icons) {
        let icon_name = item[0];
        let event_name = item[1];
        let disabled = item[2];

        let button = {
            class: `button ${event_name}`,
            style: {height: toolbar_wide, width: '2.5em'}
        };
        if(disabled) {
            button.disabled = true;
        }
        center_items.push(
            ['button', button,
                ['i', {style: 'font-size:1.5em; color:inherit;', class: icon_name}],
                {
                    click: (evt) => {
                        evt.stopPropagation();
                        gobj_send_event(gobj, event_name, {evt: evt}, gobj);
                    }
                }
            ]
        );
    }

    /*
     *  Layout selector
     */
    let options = [];
    for(let key of Object.keys(LAYOUTS)) {
        let opt_attrs = {value: key};
        if(key === current_layout) {
            opt_attrs.selected = "selected";
        }
        options.push(['option', opt_attrs, LAYOUTS[key].label]);
    }

    let $select = createElement2(
        ['select', {
                class: 'select',
                style: {height: toolbar_wide, "margin-left": "0.5em"},
                title: t("Layout"),
            },
            options,
            {
                change: (evt) => {
                    let value = evt.target.value;
                    gobj_send_event(gobj, "EV_CHANGE_LAYOUT", {layout: value}, gobj);
                }
            }
        ]
    );
    priv.$layout_select = $select;
    right_items.push($select);

    const $toolbar = yui_toolbar({}, [
        ['div', {class: 'yui-horizontal-toolbar-section left'}, left_items],
        ['div', {class: 'yui-horizontal-toolbar-section center'}, center_items],
        ['div', {class: 'yui-horizontal-toolbar-section right'}, right_items],
    ]);

    refresh_language($toolbar, t);
    return $toolbar;
}

/************************************************************
 *  Build a G6 graph instance
 ************************************************************/
function build_graph(gobj)
{
    let priv = gobj.priv;
    let layout_cfg = get_layout_cfg(gobj_read_str_attr(gobj, "layout"));

    const graph = priv.graph = new Graph({
        container: priv.canvas_id,
        animation: false,
        autoResize: false,
        zoomRange: [0.1, 4],

        node: {
            style: {
                labelPlacement: 'center',
            },
        },

        edge: {
            type: layout_cfg.edge_type,
            style: {
                stroke: gt_is_dark() ? '#8b94a3' : '#6b7280',
                lineWidth: 1,
                endArrow: true,
            },
        },

        layout: layout_cfg.g6_layout,

        behaviors: [
            'drag-canvas',
            'zoom-canvas',
        ],
    });

    if(priv.theme) {
        graph.setTheme(priv.theme);
    }

    graph.on(NodeEvent.CLICK, (evt) => {
        gobj_send_event(gobj, "EV_NODE_CLICK", {evt: evt}, gobj);
    });

    graph.on(CanvasEvent.CLICK, (evt) => {
        hide_popover(gobj);
    });

    /*
     *  Self-contained resize.  G6 v5 autoResize only listens to the
     *  global window 'resize'; it ignores container-only changes
     *  (panel/tab/splitter/mobile chrome).  If the configured canvas
     *  size drifts from its on-screen size, @antv/g getScale() scales
     *  the pointer delta by bbox.width/offsetWidth and drag-canvas
     *  panning desyncs (badly on mobile).  Observe the mount box and
     *  re-sync on every change; rAF-debounced, setSize does not change
     *  the observed box so there is no feedback loop.
     */
    if(typeof ResizeObserver !== "undefined") {
        let $canvas = document.getElementById(priv.canvas_id);
        if($canvas) {
            let ro = new ResizeObserver(() => {
                if(priv._resize_raf) {
                    cancelAnimationFrame(priv._resize_raf);
                }
                priv._resize_raf = requestAnimationFrame(() => {
                    priv._resize_raf = 0;
                    if(priv.graph) {
                        gobj_send_event(gobj, "EV_RESIZE", {}, gobj);
                    }
                });
            });
            ro.observe($canvas);
            priv.resize_observer = ro;
        }
    }
}

/************************************************************
 *  Apply layout (edge type + layout config) to existing graph
 ************************************************************/
function apply_layout(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    if(!graph) {
        return;
    }

    let layout_cfg = get_layout_cfg(gobj_read_str_attr(gobj, "layout"));

    graph.setOptions({
        edge: {
            type: layout_cfg.edge_type,
            style: {
                stroke: gt_is_dark() ? '#8b94a3' : '#6b7280',
                lineWidth: 1,
                endArrow: true,
            },
        },
        layout: layout_cfg.g6_layout,
    });
}

/************************************************************
 *  Resolve role colors for a gobj
 ************************************************************/
function get_role_colors(target_gobj, is_root)
{
    if(is_root) {
        return ROLE_COLORS.yuno;
    }
    if(gobj_is_volatil(target_gobj)) {
        return ROLE_COLORS.volatil;
    }
    if(gobj_is_service(target_gobj)) {
        return ROLE_COLORS.service;
    }
    if(gobj_is_pure_child(target_gobj)) {
        return ROLE_COLORS.pure;
    }
    return ROLE_COLORS.default;
}

/************************************************************
 *  Resolve state color
 ************************************************************/
function get_state_color(target_gobj)
{
    if(!gobj_is_running(target_gobj)) {
        return STATE_COLORS.stopped;
    }
    if(gobj_is_playing(target_gobj)) {
        return STATE_COLORS.running_playing;
    }
    return STATE_COLORS.running;
}

/************************************************************
 *  Generate a unique node id
 ************************************************************/
function gen_node_id(gobj)
{
    let priv = gobj.priv;
    priv.node_counter++;
    return "gt-" + priv.node_counter;
}

/************************************************************
 *  Decide whether a node should be drawn collapsed given the
 *  user's explicit overrides and the auto-collapse threshold.
 ************************************************************/
function is_node_collapsed(gobj, full_name, num_children)
{
    if(num_children === 0) {
        return false;
    }

    let priv = gobj.priv;
    let state = priv.collapse_state ? priv.collapse_state[full_name] : undefined;
    if(state === "collapsed") {
        return true;
    }
    if(state === "expanded") {
        return false;
    }

    let threshold = gobj_read_attr(gobj, "collapse_threshold");
    if(threshold > 0 && num_children > threshold) {
        return true;
    }
    return false;
}

/************************************************************
 *  HTML for the +/- toggle badge.
 *
 *  Uses data-gobj-toggle / data-node-id attributes picked up
 *  by the delegated click listener in build_ui().
 ************************************************************/
function render_toggle_html(node_id, collapsed, num_children, cs)
{
    let label = collapsed ? ("+" + num_children) : "−";
    let title = collapsed ? "Expand children" : "Collapse children";
    return `<span
        data-gobj-toggle="true"
        data-node-id="${node_id}"
        title="${title}"
        style="
            flex: 0 0 auto;
            display: inline-flex;
            align-items: center;
            justify-content: center;
            min-width: 22px;
            height: 16px;
            padding: 0 5px;
            margin-left: 4px;
            background: ${cs.tagbg};
            color: ${cs.border};
            border: 1px solid ${cs.border};
            border-radius: 6px;
            font-size: 10px;
            font-weight: 600;
            line-height: 1;
            cursor: pointer;
            user-select: none;
        ">${label}</span>`;
}

/************************************************************
 *  Recursively build nodes and edges from the gobj tree
 ************************************************************/
function build_gobj_nodes(gobj, target_gobj, nodes, edges, parent_id, is_root, compact)
{
    let priv = gobj.priv;
    let node_id = gen_node_id(gobj);

    let gclass_name = gobj_gclass_name(target_gobj) || "";
    let instance_name = gobj_name(target_gobj) || "";
    let state = gobj_current_state(target_gobj) || "";
    let full_name = gobj_full_name(target_gobj);
    let colors = get_role_colors(target_gobj, is_root);
    let dark = gt_is_dark();
    let cs = role_card_style(colors.stroke, dark);
    let state_color = get_state_color(target_gobj);

    let running = gobj_is_running(target_gobj);
    let playing = gobj_is_playing(target_gobj);
    let status_text = !running ? "stopped" : (playing ? "playing" : "running");

    let category = get_node_category(target_gobj, is_root);
    let size = NODE_SIZES[compact ? "compact" : "full"][category];
    let width = size.w;
    let height = size.h;

    /*
     *  Emphasise services with a thicker border so the hierarchy
     *  boundary is visible even when siblings are the same colour.
     */
    let border_width = (category === "yuno" || category === "service") ? 2 : 1;

    let children = target_gobj.dl_children || [];
    let num_children = children.length;
    let has_children = num_children > 0;
    let collapsed = has_children && is_node_collapsed(gobj, full_name, num_children);

    /*
     *  Direct-children index (collected even when this node is
     *  drawn collapsed, so expanding it can set them to the
     *  "collapsed" state to avoid expanding the whole subtree).
     */
    let direct_children = [];
    for(let child of children) {
        direct_children.push({
            full_name: gobj_full_name(child),
            has_grandchildren: (child.dl_children || []).length > 0,
        });
    }

    let toggle_html = has_children
        ? render_toggle_html(node_id, collapsed, num_children, cs)
        : "";

    let node_html;

    if(compact) {
        /*
         *  Compact one-line node: "gclass^name" with a left color bar,
         *  state dot and optional +/- toggle. Name truncates with "...".
         */
        let compact_label = escapeHtml(
            instance_name ? (gclass_name + "^" + instance_name) : gclass_name
        );

        node_html = `
<div style="
    width: ${width}px;
    height: ${height}px;
    background: ${cs.bg};
    border: ${border_width}px solid ${cs.border};
    border-left: 5px solid ${cs.border};
    border-radius: 7px;
    box-shadow: ${cs.shadow};
    font-family: ${GT_FONT};
    display: flex;
    align-items: center;
    padding: 0 8px;
    box-sizing: border-box;
    overflow: hidden;
    cursor: pointer;
" title="${compact_label}">
    <span style="flex:1 1 auto; min-width:0; font-size:11px; font-weight:600; color:${cs.title}; white-space:nowrap; overflow:hidden; text-overflow:ellipsis;">${compact_label}</span>
    <span title="${escapeHtml(state || status_text)}" style="
        flex: 0 0 auto;
        width: 8px; height: 8px; border-radius: 50%;
        background: ${state_color}; margin-left: 6px;
    "></span>
    ${toggle_html}
</div>`;
    } else {
        /*
         *  Full node: header (gclass + optional toggle), name, badges + state.
         *  All rows truncate with "..." — width is fixed.
         */
        let header_label = escapeHtml(gclass_name);
        let name_label = escapeHtml(instance_name);
        let state_label = escapeHtml(state);

        let badges = [];
        if(is_root) {
            badges.push("yuno");
        } else {
            if(gobj_is_service(target_gobj)) {
                badges.push("service");
            }
            if(gobj_is_pure_child(target_gobj)) {
                badges.push("pure");
            }
            if(gobj_is_volatil(target_gobj)) {
                badges.push("volatil");
            }
        }
        let badges_html = badges.map(b =>
            `<span style="font-size:10px; padding:0 5px; margin-right:4px; background:${cs.tagbg}; color:${cs.border}; border:1px solid ${cs.border}; border-radius:6px; white-space:nowrap;">${b}</span>`
        ).join("");

        node_html = `
<div style="
    width: ${width}px;
    height: ${height}px;
    background: ${cs.bg};
    border: ${border_width}px solid ${cs.border};
    border-radius: 9px;
    box-shadow: ${cs.shadow};
    overflow: hidden;
    font-family: ${GT_FONT};
    box-sizing: border-box;
    display: flex;
    flex-direction: column;
    justify-content: center;
    padding: 5px 9px;
    gap: 1px;
    cursor: pointer;
" title="${escapeHtml(gclass_name + (instance_name ? "^" + instance_name : ""))}">
    <div style="flex:0 0 auto; display:flex; align-items:center; gap:6px; overflow:hidden;">
        <span style="flex:1 1 auto; min-width:0; font-size:12px; line-height:1.25; font-weight:600; color:${cs.title}; white-space:nowrap; overflow:hidden; text-overflow:ellipsis;">${header_label}</span>
        ${toggle_html}
    </div>
    <div style="flex:0 0 auto; font-size:12px; line-height:1.25; color:${cs.sub}; white-space:nowrap; overflow:hidden; text-overflow:ellipsis;">${name_label || "&nbsp;"}</div>
    <div style="flex:0 0 auto; display:flex; justify-content:space-between; align-items:center; gap:6px; overflow:hidden;">
        <span style="flex:1 1 auto; min-width:0; overflow:hidden; text-overflow:ellipsis; white-space:nowrap;">${badges_html}</span>
        <span style="flex:0 0 auto; display:flex; align-items:center; gap:4px; color:${state_color}; font-weight:600; font-size:11px; white-space:nowrap;">
            <span style="width:7px; height:7px; border-radius:50%; background:${state_color};"></span>${state_label || status_text}
        </span>
    </div>
</div>`;
    }

    let parent_gobj = gobj_parent(target_gobj);

    /*
     *  Node data kept for the popover. Extend this object when
     *  you want to surface more attributes — then add a row in
     *  get_node_info_rows() with the same key.
     */
    let node_data = {
        gclass: gclass_name,
        name: instance_name,
        short_name: gobj_short_name(target_gobj),
        full_name: full_name,
        parent_short_name: parent_gobj ? gobj_short_name(parent_gobj) : "",
        state: state,
        category: category,
        is_yuno: is_root,
        is_service: gobj_is_service(target_gobj),
        is_pure_child: gobj_is_pure_child(target_gobj),
        is_volatil: gobj_is_volatil(target_gobj),
        is_running: running,
        is_playing: playing,
        num_children: num_children,
        is_collapsed: collapsed,
        direct_children: direct_children,
    };

    if(!priv.node_by_id) {
        priv.node_by_id = {};
    }
    priv.node_by_id[node_id] = node_data;

    let node = {
        id: node_id,
        type: 'html',
        data: node_data,
        style: {
            innerHTML: node_html,
            size: [width, height],
            dx: -(width / 2),
            dy: -(height / 2),
        },
    };
    nodes.push(node);

    if(parent_id) {
        edges.push({
            source: parent_id,
            target: node_id,
            style: {
                stroke: colors.stroke,
                lineWidth: 1,
            }
        });
    }

    /*
     *  Recurse only if not collapsed
     */
    if(!collapsed) {
        for(let child of children) {
            build_gobj_nodes(gobj, child, nodes, edges, node_id, false, compact);
        }
    }

    return node_id;
}

/************************************************************
 *  Load the yuno tree into the graph.
 *
 *  The caller may have set one of:
 *    priv.pending_anchor       — {full_name, viewport_x, viewport_y, zoom}
 *                                anchors the node with `full_name` so it stays
 *                                at the same screen position (and zoom).
 *    priv.pending_preserve_view — keep the current zoom/translate as-is.
 *  Otherwise we fit the whole tree to the viewport.
 ************************************************************/
function load_tree(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    if(!graph) {
        return;
    }

    let yuno = gobj_yuno();
    if(!yuno) {
        return;
    }

    let layout_cfg = get_layout_cfg(gobj_read_str_attr(gobj, "layout"));

    let anchor = priv.pending_anchor || null;
    let preserve_view = !!priv.pending_preserve_view;
    priv.pending_anchor = null;
    priv.pending_preserve_view = false;

    priv.node_counter = 0;
    priv.node_by_id = {};

    let nodes = [];
    let edges = [];

    build_gobj_nodes(gobj, yuno, nodes, edges, null, true, layout_cfg.compact);

    if(nodes.length > 0) {
        graph.setData({nodes: nodes, edges: edges});
        graph.render().then(() => {
            if(anchor) {
                restore_view_anchored(gobj, anchor);
            } else if(!preserve_view) {
                graph.fitView();
            }
        });
    }
}

/************************************************************
 *  Keep the given node visually anchored (same screen
 *  position and zoom) across a re-render.
 ************************************************************/
function restore_view_anchored(gobj, anchor)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    if(!graph || !anchor) {
        return;
    }

    /*
     *  Find the new node id that maps to the same gobj as before
     */
    let new_node_id = null;
    if(priv.node_by_id) {
        for(let id of Object.keys(priv.node_by_id)) {
            if(priv.node_by_id[id].full_name === anchor.full_name) {
                new_node_id = id;
                break;
            }
        }
    }
    if(!new_node_id) {
        return;
    }

    /*
     *  Restore zoom first — translate math is done in viewport pixels,
     *  which depend on the current zoom.
     */
    if(typeof anchor.zoom === "number" && anchor.zoom > 0) {
        try {
            graph.zoomTo(anchor.zoom);
        } catch(e) {}
    }

    /*
     *  Shift the graph so the anchor node lands at its old screen spot
     */
    try {
        let canvas_pos = graph.getElementPosition(new_node_id);
        let vp = graph.getViewportByCanvas([canvas_pos[0], canvas_pos[1]]);
        let dx = anchor.viewport_x - vp[0];
        let dy = anchor.viewport_y - vp[1];
        graph.translateBy([dx, dy]);
    } catch(e) {}
}

/************************************************************
 *  Capture the current screen position + zoom of a node,
 *  so a later re-render can keep it anchored.
 *  Returns null on failure.
 ************************************************************/
function capture_anchor(gobj, node_id)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    if(!graph || !priv.node_by_id || !priv.node_by_id[node_id]) {
        return null;
    }
    let full_name = priv.node_by_id[node_id].full_name;
    if(!full_name) {
        return null;
    }
    try {
        let canvas_pos = graph.getElementPosition(node_id);
        let vp = graph.getViewportByCanvas([canvas_pos[0], canvas_pos[1]]);
        return {
            full_name: full_name,
            viewport_x: vp[0],
            viewport_y: vp[1],
            zoom: graph.getZoom(),
        };
    } catch(e) {
        return null;
    }
}

/************************************************************
 *  Clear and reload.
 *
 *  opts (optional):
 *      {anchor: {full_name, viewport_x, viewport_y, zoom}}
 *          keep the given node anchored at its current screen spot.
 *      {preserve_view: true}
 *          keep the current zoom and translate (no fitView).
 *  Default: fit the whole tree to the viewport.
 ************************************************************/
function refresh_tree(gobj, opts)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    opts = opts || {};

    if(graph) {
        hide_popover(gobj);
        priv.pending_anchor = opts.anchor || null;
        priv.pending_preserve_view = !!opts.preserve_view;
        graph.clear();
        load_tree(gobj);
    }
}

/************************************************************
 *  Rows to show in the popover for a given node data.
 *
 *  This is the extension point: to surface a new attribute,
 *  collect it into `node_data` in build_gobj_nodes(), then
 *  append a `[label, value]` pair here.
 ************************************************************/
function get_node_info_rows(node_data)
{
    let rows = [];

    rows.push([t("GClass"),   node_data.gclass || "—"]);
    rows.push([t("Name"),     node_data.name   || "—"]);

    if(node_data.full_name && node_data.full_name !== node_data.short_name) {
        rows.push([t("Full name"), node_data.full_name]);
    }

    let role;
    if(node_data.is_yuno) {
        role = "yuno";
    } else if(node_data.is_service) {
        role = "service";
    } else if(node_data.is_volatil) {
        role = "volatil child";
    } else if(node_data.is_pure_child) {
        role = "pure child";
    } else {
        role = "child";
    }
    rows.push([t("Role"), role]);

    let status;
    if(!node_data.is_running) {
        status = "stopped";
    } else if(node_data.is_playing) {
        status = "running + playing";
    } else {
        status = "running";
    }
    rows.push([t("Status"), status]);
    rows.push([t("State"),  node_data.state || "—"]);

    if(node_data.parent_short_name) {
        rows.push([t("Parent"), node_data.parent_short_name]);
    }

    if(typeof node_data.num_children === "number") {
        let kids = node_data.num_children;
        if(kids > 0 && node_data.is_collapsed) {
            rows.push([t("Children"), kids + " " + t("(collapsed)")]);
        } else if(kids > 0) {
            rows.push([t("Children"), String(kids)]);
        }
    }

    return rows;
}

/************************************************************
 *  Render popover body from the info rows of a node.
 ************************************************************/
function render_popover_body(gobj, node_data)
{
    let priv = gobj.priv;
    let $body = priv.$popover_body;
    if(!$body) {
        return;
    }

    let rows = get_node_info_rows(node_data);

    let rows_html = rows.map(([label, value]) => `
        <div style="display:grid; grid-template-columns: 90px 1fr; column-gap:10px; padding: 2px 0;">
            <span style="color:#6B7280; font-weight:500;">${escapeHtml(String(label))}</span>
            <span style="color:#1A1A1A; word-break:break-all;">${escapeHtml(String(value))}</span>
        </div>
    `).join("");

    $body.innerHTML = rows_html;
}

/************************************************************
 *  Show popover next to the click position.
 *
 *  client_x/client_y are viewport coordinates (evt.client.x/y).
 ************************************************************/
function show_popover(gobj, node_data, client_x, client_y)
{
    let priv = gobj.priv;
    let $popover = priv.$popover;
    let $title = priv.$popover_title;
    let $container = gobj_read_attr(gobj, "$container");
    if(!$popover || !$title || !$container) {
        return;
    }

    $title.textContent = node_data.short_name ||
        (node_data.gclass + (node_data.name ? "^" + node_data.name : ""));
    $title.title = $title.textContent;

    render_popover_body(gobj, node_data);

    /*
     *  Position relative to container (absolute positioning inside it)
     */
    let rect = $container.getBoundingClientRect();
    let offset = 10;
    let local_x = client_x - rect.left + offset;
    let local_y = client_y - rect.top + offset;

    $popover.style.display = "block";
    $popover.style.left = local_x + "px";
    $popover.style.top = local_y + "px";

    /*
     *  Nudge back inside if we'd overflow the container on the right or bottom
     */
    let pop_rect = $popover.getBoundingClientRect();
    if(pop_rect.right > rect.right - 4) {
        local_x = (client_x - rect.left) - pop_rect.width - offset;
        if(local_x < 4) {
            local_x = 4;
        }
        $popover.style.left = local_x + "px";
    }
    if(pop_rect.bottom > rect.bottom - 4) {
        local_y = (client_y - rect.top) - pop_rect.height - offset;
        if(local_y < 4) {
            local_y = 4;
        }
        $popover.style.top = local_y + "px";
    }
}

/************************************************************
 *  Hide popover.
 ************************************************************/
function hide_popover(gobj)
{
    let priv = gobj.priv;
    if(priv.$popover) {
        priv.$popover.style.display = "none";
    }
    if(priv.$popover_body) {
        priv.$popover_body.innerHTML = "";
    }
    if(priv.$popover_title) {
        priv.$popover_title.textContent = "";
    }
}

/************************************************************
 *  Generic tree layout engine.
 *
 *  orientation = "V": parents above, children below (y grows down)
 *  orientation = "H": parents on the left, children on the right
 *                     (x grows right)
 *
 *  In both cases nodes are positioned so that the subtree of each
 *  node is centered on its "perpendicular" axis. The "along" axis
 *  advances one level at a time.
 ************************************************************/
function compute_tree_positions(nodes, edges, orientation, gap_along, gap_cross)
{
    let children_map = {};
    let has_parent = new Set();
    for(let edge of edges) {
        if(!children_map[edge.source]) {
            children_map[edge.source] = [];
        }
        children_map[edge.source].push(edge.target);
        has_parent.add(edge.target);
    }

    let roots = nodes.filter(n => !has_parent.has(n.id));
    if(roots.length === 0 && nodes.length > 0) {
        roots = [nodes[0]];
    }

    let node_dims = {};
    for(let node of nodes) {
        let w = (node.style && node.style.size) ? node.style.size[0] : 200;
        let h = (node.style && node.style.size) ? node.style.size[1] : 80;
        node_dims[node.id] = {w, h};
    }

    /*
     *  "cross" is the dimension perpendicular to the tree growth:
     *     V: cross = width, along = height
     *     H: cross = height, along = width
     */
    function cross_of(id)
    {
        if(orientation === "V") {
            return node_dims[id].w;
        }
        return node_dims[id].h;
    }
    function along_of(id)
    {
        if(orientation === "V") {
            return node_dims[id].h;
        }
        return node_dims[id].w;
    }

    let subtree_cross = {};
    function calc_subtree_cross(node_id) {
        let kids = children_map[node_id] || [];
        if(kids.length === 0) {
            subtree_cross[node_id] = cross_of(node_id);
            return subtree_cross[node_id];
        }
        let total = 0;
        for(let i = 0; i < kids.length; i++) {
            if(i > 0) {
                total += gap_cross;
            }
            total += calc_subtree_cross(kids[i]);
        }
        subtree_cross[node_id] = Math.max(total, cross_of(node_id));
        return subtree_cross[node_id];
    }

    for(let root of roots) {
        calc_subtree_cross(root.id);
    }

    let positions = {};
    function position_node(node_id, cross_pos, along_pos) {
        if(orientation === "V") {
            positions[node_id] = {x: cross_pos, y: along_pos};
        } else {
            positions[node_id] = {x: along_pos, y: cross_pos};
        }

        let kids = children_map[node_id] || [];
        if(kids.length === 0) {
            return;
        }

        let total_cross = 0;
        for(let i = 0; i < kids.length; i++) {
            if(i > 0) {
                total_cross += gap_cross;
            }
            total_cross += subtree_cross[kids[i]];
        }

        let child_along = along_pos + along_of(node_id) + gap_along;
        let start_cross = cross_pos - total_cross / 2;

        for(let kid of kids) {
            let kid_c = subtree_cross[kid];
            let kid_cross_pos = start_cross + kid_c / 2;
            position_node(kid, kid_cross_pos, child_along);
            start_cross += kid_c + gap_cross;
        }
    }

    let root_cross = 0;
    for(let root of roots) {
        let rc = subtree_cross[root.id];
        position_node(root.id, root_cross + rc / 2, 0);
        root_cross += rc + gap_cross;
    }

    return nodes.map(node => ({
        id: node.id,
        style: {
            x: positions[node.id] ? positions[node.id].x : 0,
            y: positions[node.id] ? positions[node.id].y : 0,
        },
    }));
}

/************************************************************
 *  Custom tree layout — vertical (top → bottom)
 ************************************************************/
class GobjTreeVLayout extends BaseLayout {
    async execute(data, options) {
        const { nodes = [], edges = [] } = data;
        if(nodes.length === 0) {
            return {nodes: []};
        }
        return {
            nodes: compute_tree_positions(nodes, edges, "V", 50, 20),
        };
    }
}

/************************************************************
 *  Custom tree layout — horizontal (left → right)
 ************************************************************/
class GobjTreeHLayout extends BaseLayout {
    async execute(data, options) {
        const { nodes = [], edges = [] } = data;
        if(nodes.length === 0) {
            return {nodes: []};
        }
        return {
            nodes: compute_tree_positions(nodes, edges, "H", 70, 12),
        };
    }
}

/************************************************************
 *  Lanes layout engine.
 *
 *  Each expanded node reserves its own dedicated lane (row in
 *  the V variant, column in the H variant) for its children;
 *  lanes never get re-used or re-ordered when other nodes
 *  expand. Allocation order is pre-order DFS, so the lane
 *  assignment matches the expansion sequence visible to the
 *  user.
 *
 *  Children of one node form a horizontal cluster centered on
 *  the parent's cross-axis position. Different parents' clusters
 *  may overlap on the cross axis but never on the along axis
 *  (because each lives on its own lane).
 *
 *      orientation = "V":   along = y (rows go top→bottom)
 *                            cross = x
 *      orientation = "H":   along = x (lanes go left→right)
 *                            cross = y
 ************************************************************/
function compute_lane_positions(nodes, edges, orientation, gap_along, gap_cross)
{
    const children_map = {};
    const has_parent = new Set();
    for(const edge of edges) {
        if(!children_map[edge.source]) {
            children_map[edge.source] = [];
        }
        children_map[edge.source].push(edge.target);
        has_parent.add(edge.target);
    }

    let roots = nodes.filter(n => !has_parent.has(n.id));
    if(roots.length === 0 && nodes.length > 0) {
        roots = [nodes[0]];
    }

    const node_dims = {};
    for(const node of nodes) {
        const w = (node.style && node.style.size) ? node.style.size[0] : 200;
        const h = (node.style && node.style.size) ? node.style.size[1] : 80;
        node_dims[node.id] = {w, h};
    }

    function cross_size(id) {
        if(orientation === "V") {
            return node_dims[id].w;
        }
        return node_dims[id].h;
    }
    function along_size(id) {
        if(orientation === "V") {
            return node_dims[id].h;
        }
        return node_dims[id].w;
    }

    /*
     *  Pass 1: pre-order DFS allocation of lane indices.
     *      lane_of[id] = the lane this node sits on.
     *  Each expanded node reserves a fresh lane for its children
     *  using next_lane (which only ever increases).
     */
    const lane_of = {};
    let next_lane = 0;

    function visit(id, lane) {
        lane_of[id] = lane;
        next_lane = Math.max(next_lane, lane + 1);
        const kids = children_map[id] || [];
        if(kids.length > 0) {
            const child_lane = next_lane;
            next_lane = child_lane + 1;
            for(const kid of kids) {
                visit(kid, child_lane);
            }
        }
    }

    for(const root of roots) {
        const root_lane = next_lane;
        next_lane = root_lane + 1;
        visit(root.id, root_lane);
    }

    /*
     *  Pass 2: cross-axis position. Children of a parent are
     *  laid out side-by-side, centered on the parent's cross
     *  position. Roots are seeded at 0.
     */
    const cross_of = {};
    let root_seed = 0;
    for(const root of roots) {
        cross_of[root.id] = root_seed;
        root_seed += cross_size(root.id) + gap_cross;
    }

    function place_children(id) {
        const kids = children_map[id] || [];
        if(kids.length === 0) {
            return;
        }
        let total = 0;
        for(let i = 0; i < kids.length; i++) {
            if(i > 0) {
                total += gap_cross;
            }
            total += cross_size(kids[i]);
        }
        let start = cross_of[id] - total / 2;
        for(const kid of kids) {
            const c = cross_size(kid);
            cross_of[kid] = start + c / 2;
            start += c + gap_cross;
            place_children(kid);
        }
    }
    for(const root of roots) {
        place_children(root.id);
    }

    /*
     *  Pass 3: along-axis position per lane. Each lane gets a
     *  thickness equal to the largest along_size of any node
     *  that lives on it, plus gap_along.
     */
    const lane_thickness = {};
    for(const node of nodes) {
        const lane = lane_of[node.id];
        if(lane === undefined) {
            continue;
        }
        const a = along_size(node.id);
        if(!lane_thickness[lane] || lane_thickness[lane] < a) {
            lane_thickness[lane] = a;
        }
    }

    const lane_along = {};
    let cumul = 0;
    const lane_indices = Object.keys(lane_thickness)
        .map(Number)
        .sort((a, b) => a - b);
    for(const lane of lane_indices) {
        lane_along[lane] = cumul + lane_thickness[lane] / 2;
        cumul += lane_thickness[lane] + gap_along;
    }

    return nodes.map(node => {
        const lane = lane_of[node.id];
        const along = (lane !== undefined) ? lane_along[lane] : 0;
        const cross = (cross_of[node.id] !== undefined) ? cross_of[node.id] : 0;
        if(orientation === "V") {
            return {id: node.id, style: {x: cross, y: along}};
        }
        return {id: node.id, style: {x: along, y: cross}};
    });
}

/************************************************************
 *  Lanes vertical layout — each expansion gets a new row.
 ************************************************************/
class GobjLanesVLayout extends BaseLayout {
    async execute(data, options) {
        const { nodes = [], edges = [] } = data;
        if(nodes.length === 0) {
            return {nodes: []};
        }
        return {
            nodes: compute_lane_positions(nodes, edges, "V", 50, 20),
        };
    }
}

/************************************************************
 *  Lanes horizontal layout — each expansion gets a new column.
 ************************************************************/
class GobjLanesHLayout extends BaseLayout {
    async execute(data, options) {
        const { nodes = [], edges = [] } = data;
        if(nodes.length === 0) {
            return {nodes: []};
        }
        return {
            nodes: compute_lane_positions(nodes, edges, "H", 70, 12),
        };
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/************************************************************
 *
 ************************************************************/
function ac_refresh(gobj, event, kw, src)
{
    refresh_tree(gobj);
    return 0;
}

/************************************************************
 *  {layout: "<key>"}
 ************************************************************/
function ac_change_layout(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let new_key = kw.layout;
    if(!LAYOUTS[new_key]) {
        return 0;
    }

    gobj_write_str_attr(gobj, "layout", new_key);

    /*
     *  Keep the dropdown in sync if the event came from elsewhere
     */
    if(priv.$layout_select && priv.$layout_select.value !== new_key) {
        priv.$layout_select.value = new_key;
    }

    apply_layout(gobj);

    /*
     *  Rebuild nodes because compact/full HTML differs per layout
     */
    refresh_tree(gobj);

    return 0;
}

/************************************************************
 *  {node_id: "..."}   toggle collapse/expand for one node
 ************************************************************/
function ac_toggle_collapse(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let node_id = kw.node_id;
    if(!node_id || !priv.node_by_id) {
        return 0;
    }
    let node_data = priv.node_by_id[node_id];
    if(!node_data || !node_data.full_name) {
        return 0;
    }
    if(!node_data.num_children) {
        return 0;
    }

    if(!priv.collapse_state) {
        priv.collapse_state = {};
    }

    /*
     *  Capture the anchor BEFORE changing state — we want the clicked
     *  node to stay at the same screen position after the rebuild.
     */
    let anchor = capture_anchor(gobj, node_id);

    /*
     *  Flip the effective state (handles auto-collapse by threshold too)
     */
    let currently_collapsed = is_node_collapsed(
        gobj, node_data.full_name, node_data.num_children
    );

    if(currently_collapsed) {
        /*
         *  Expanding: reveal ONLY the next level. Force every direct
         *  child that has grandchildren to render as collapsed, so
         *  thresholded auto-expansion down the subtree does not kick in.
         *  The user can drill further by clicking their own `+`.
         */
        priv.collapse_state[node_data.full_name] = "expanded";
        for(let child_info of (node_data.direct_children || [])) {
            if(child_info.has_grandchildren) {
                priv.collapse_state[child_info.full_name] = "collapsed";
            }
        }
    } else {
        priv.collapse_state[node_data.full_name] = "collapsed";
    }

    refresh_tree(gobj, {anchor: anchor});
    return 0;
}

/************************************************************
 *  Force every node visible (overrides threshold).
 ************************************************************/
function ac_expand_all(gobj, event, kw, src)
{
    let priv = gobj.priv;
    if(!priv.collapse_state) {
        priv.collapse_state = {};
    }
    /*
     *  Walk the current nodes and mark every one with children
     *  as explicitly expanded. Nodes encountered later (after the
     *  tree grows) default to expanded if threshold is 0, or will
     *  auto-collapse otherwise — acceptable compromise.
     */
    if(priv.node_by_id) {
        for(let id of Object.keys(priv.node_by_id)) {
            let nd = priv.node_by_id[id];
            if(nd.full_name && nd.num_children > 0) {
                priv.collapse_state[nd.full_name] = "expanded";
            }
        }
    }
    refresh_tree(gobj, {preserve_view: true});
    return 0;
}

/************************************************************
 *  Collapse every node that has children.
 ************************************************************/
function ac_collapse_all(gobj, event, kw, src)
{
    let priv = gobj.priv;
    if(!priv.collapse_state) {
        priv.collapse_state = {};
    }
    if(priv.node_by_id) {
        for(let id of Object.keys(priv.node_by_id)) {
            let nd = priv.node_by_id[id];
            if(nd.full_name && nd.num_children > 0) {
                priv.collapse_state[nd.full_name] = "collapsed";
            }
        }
    }
    refresh_tree(gobj, {preserve_view: true});
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_zoom_in(gobj, event, kw, src)
{
    let graph = gobj.priv.graph;
    if(graph) {
        let z = graph.getZoom();
        graph.zoomTo(z * 1.2);
    }
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_zoom_out(gobj, event, kw, src)
{
    let graph = gobj.priv.graph;
    if(graph) {
        let z = graph.getZoom();
        graph.zoomTo(z * 0.8);
    }
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_zoom_reset(gobj, event, kw, src)
{
    let graph = gobj.priv.graph;
    if(graph && graph.rendered) {
        graph.zoomTo(1);
        graph.translateTo([0, 0]);
    }
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_center(gobj, event, kw, src)
{
    let graph = gobj.priv.graph;
    if(graph) {
        graph.fitCenter();
        graph.fitView();
    }
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_node_click(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    let node_id = kw.evt.target.id;

    /*
     *  Prefer our own cached node_data (richer) and fall back to
     *  what the graph exposes.
     */
    let node_data = (priv.node_by_id && priv.node_by_id[node_id]) || null;
    if(!node_data) {
        try {
            let nd = graph.getNodeData(node_id);
            if(nd && nd.data) {
                node_data = nd.data;
            }
        } catch(e) {}
    }

    if(!node_data) {
        return 0;
    }

    /*
     *  Show popover at click position
     */
    let client_x = (kw.evt && kw.evt.client) ? kw.evt.client.x : 0;
    let client_y = (kw.evt && kw.evt.client) ? kw.evt.client.y : 0;
    show_popover(gobj, node_data, client_x, client_y);

    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_show(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    /*
     *  Always refresh on show: the tree may have changed while hidden
     */
    refresh_tree(gobj);

    if(graph) {
        let $canvas = document.getElementById(priv.canvas_id);
        if($canvas) {
            /*
             *  Content box (clientWidth/Height), not getBoundingClientRect:
             *  @antv/g lays the canvas into the element's content area and
             *  getScale() compares bbox.width / offsetWidth.  Sizing to the
             *  content box keeps that ratio at exactly 1 (no border / sub-px
             *  drift) so drag-canvas panning stays 1:1.
             */
            let cw = $canvas.clientWidth;
            let ch = $canvas.clientHeight;
            if(cw > 0 && ch > 0) {
                graph.setSize(cw, ch);
                graph.render().then(() => {
                    graph.fitView();
                });
            }
        }
    }

    return 0;
}

/************************************************************
 *  Keep the canvas size in sync with its on-screen box.
 *  No fitView: preserve the user's current pan/zoom.
 ************************************************************/
function ac_resize(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    if(graph) {
        let $canvas = document.getElementById(priv.canvas_id);
        if($canvas) {
            let cw = $canvas.clientWidth;
            let ch = $canvas.clientHeight;
            if(cw > 0 && ch > 0) {
                graph.setSize(cw, ch);
            }
        }
    }

    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_hide(gobj, event, kw, src)
{
    hide_popover(gobj);
    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
const gmt = {
    mt_create:      mt_create,
    mt_start:       mt_start,
    mt_stop:        mt_stop,
    mt_destroy:     mt_destroy,
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
            ["EV_REFRESH",              ac_refresh,             null],
            ["EV_CHANGE_LAYOUT",        ac_change_layout,       null],
            ["EV_TOGGLE_COLLAPSE",      ac_toggle_collapse,     null],
            ["EV_EXPAND_ALL",           ac_expand_all,          null],
            ["EV_COLLAPSE_ALL",         ac_collapse_all,        null],
            ["EV_ZOOM_IN",              ac_zoom_in,             null],
            ["EV_ZOOM_OUT",             ac_zoom_out,            null],
            ["EV_ZOOM_RESET",           ac_zoom_reset,          null],
            ["EV_CENTER",               ac_center,              null],
            ["EV_NODE_CLICK",           ac_node_click,          null],
            ["EV_RESIZE",               ac_resize,              null],
            ["EV_SHOW",                 ac_show,                null],
            ["EV_HIDE",                 ac_hide,                null],
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_REFRESH",              0],
        ["EV_CHANGE_LAYOUT",        0],
        ["EV_TOGGLE_COLLAPSE",      0],
        ["EV_EXPAND_ALL",           0],
        ["EV_COLLAPSE_ALL",         0],
        ["EV_ZOOM_IN",              0],
        ["EV_ZOOM_OUT",             0],
        ["EV_ZOOM_RESET",           0],
        ["EV_CENTER",               0],
        ["EV_NODE_CLICK",           0],
        ["EV_RESIZE",               0],
        ["EV_SHOW",                 0],
        ["EV_HIDE",                 0],
    ];

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        gmt,
        0,  // lmt,
        attrs_table,
        PRIVATE_DATA,
        0,  // authz_table,
        0,  // command_table,
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
function register_c_yui_gobj_tree_js()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yui_gobj_tree_js };
