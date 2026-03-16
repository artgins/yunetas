/***********************************************************************
 *          c_yui_json_graph.js
 *
 *          JSON Graph Viewer with AntV/G6
 *          Displays JSON data as a hierarchical graph visualization
 *          Migrated from mx_json_viewer.js (mxGraph)
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA,
    SDATA_END,
    data_type_t,
    sdata_flag_t,
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
    gobj_short_name,
    gobj_read_str_attr,
    gobj_write_str_attr,
    gobj_publish_event,
    gobj_save_persistent_attrs,
    gobj_read_bool_attr,
    createElement2,
    is_string,
    is_array,
    is_object,
    is_number,
    is_boolean,
    is_null,
    json_object_size,
    json_deep_copy,
    empty_string,
    escapeHtml,
    refresh_language,
} from "yunetas";

import {yui_toolbar} from "./yui_toolbar.js";

import {t} from "i18next";

import {
    BaseLayout,
    ExtensionCategory,
    Graph,
    NodeEvent,
    CanvasEvent,
    register,
} from '@antv/g6';

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_JSON_GRAPH";

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
/*---------------- Public Attributes ----------------*/
SDATA(data_type_t.DTP_POINTER,  "subscriber",       0,  null,   "Subscriber of output events"),

/*---------------- Data ----------------*/
SDATA(data_type_t.DTP_STRING,   "path",             0,  "",     "Root path label"),
SDATA(data_type_t.DTP_JSON,     "json_data",        0,  null,   "JSON data to visualize"),

/*---------------- Sub-container ----------------*/
SDATA(data_type_t.DTP_POINTER,  "$container",       0,  null,   "Container element"),
SDATA(data_type_t.DTP_STRING,   "canvas_id",        0,  "",     "Canvas ID"),

/*---------------- Graph Settings ----------------*/
SDATA(data_type_t.DTP_STRING,   "wide",             0,  "36px", "Height of header"),

SDATA_END()
];

let PRIVATE_DATA = {
    canvas_id: "",
    graph: null,
    node_counter: 0,
    theme: null,
};

let __gclass__ = null;
let __layout_registered__ = false;


/***************************************************************
 *  Type colors matching the original mx_json_viewer
 ***************************************************************/
const TYPE_COLORS = {
    string:  "#006000",     // green
    number:  "#EE422E",     // red
    boolean: "#FF8C00",     // orange
    null:    "#475ED0",     // blue
    list:    "#4C0099",     // purple
    dict:    "#4C0099",     // purple
};

const GROUP_COLORS = {
    dict: {fill: "#FBFBFB", stroke: "#006658"},
    list: {fill: "#fffbd1", stroke: "#006658"},
};




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
    priv.canvas_id = "json-canvas-" + name;
    gobj_write_str_attr(gobj, "canvas_id", priv.canvas_id);

    /*
     *  CHILD subscription model
     */
    let subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, null, {}, subscriber);

    let __yui_main__ = gobj_find_service("__yui_main__", true);
    if(__yui_main__) {
        priv.theme = gobj_read_str_attr(__yui_main__, "theme");
    }

    build_ui(gobj);

    if(!__layout_registered__) {
        register(ExtensionCategory.LAYOUT, 'json-tree-layout', JsonTreeLayout);
        __layout_registered__ = true;
    }
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    build_graph(gobj);
    load_json(gobj);
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

    if(priv.graph) {
        priv.graph.destroy();
        priv.graph = null;
    }

    destroy_ui(gobj);
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
        ['div', {class: 'json-graph', style: 'height:100%; display:flex; flex-direction:column;'}, [
            ['div', {class: 'is-flex-grow-0 is-flex json_graph_toolbar'}, $toolbar],
            ['div', {class: 'is-flex-grow-1', style: 'height:100%; min-height:0; overflow:hidden;'}, [
                ['div', {id: priv.canvas_id, class: 'json-graph-container', style: 'height:100%; min-height:0; border: 1px solid var(--bulma-border-weak); border-radius:0.2rem;'}, [
                ]]
            ]]
        ]]
    );

    gobj_write_attr(gobj, "$container", $container);
    refresh_language($container, t);
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

    let left_items = [];
    let center_items = [];

    let c_icons = [
        ["yi-magnifying-glass-plus", "EV_ZOOM_IN",       false, 'i'],
        ["yi-magnifying-glass",      "EV_ZOOM_RESET",    false, 'i'],
        ["yi-magnifying-glass-minus","EV_ZOOM_OUT",      false, 'i'],
        ["yi-arrows-to-eye",         "EV_CENTER",        false, 'i'],
        ["yi-arrows-rotate",         "EV_REFRESH",       false, 'i'],
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

    const $toolbar = yui_toolbar({}, [
        ['div', {class: 'yui-horizontal-toolbar-section left'}, left_items],
        ['div', {class: 'yui-horizontal-toolbar-section center'}, center_items],
        ['div', {class: 'yui-horizontal-toolbar-section right'}, []],
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

    const graph = priv.graph = new Graph({
        container: priv.canvas_id,
        animation: false,
        autoResize: true,
        zoomRange: [0.1, 4],

        node: {
            style: {
                labelPlacement: 'center',
            },
        },

        edge: {
            type: 'cubic-vertical',
            style: {
                stroke: '#999',
                lineWidth: 1,
                endArrow: true,
            },
        },

        layout: {
            type: 'json-tree-layout',
        },

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
        // Canvas click
    });
}

/************************************************************
 *  Build cell value HTML (matching old mx_json_viewer colors)
 ************************************************************/
function build_cell_html(key, value, type)
{
    let color = TYPE_COLORS[type] || "black";
    let display_value = "";

    switch(type) {
        case "string":
            display_value = `"${escapeHtml(String(value))}"`;
            break;
        case "number":
            display_value = String(value);
            break;
        case "boolean":
            display_value = value ? "true" : "false";
            break;
        case "null":
            display_value = "null";
            break;
        case "list":
            display_value = `[${value.length}]`;
            break;
        case "dict":
            display_value = `{${json_object_size(value)}}`;
            break;
        default:
            display_value = String(value);
            break;
    }

    return `<span style="color:#1A1A1A">• ${escapeHtml(String(key))}: </span><span style="color:${color}">${display_value}</span>`;
}

/************************************************************
 *  Get JSON type string
 ************************************************************/
function get_json_type(value)
{
    if(is_string(value))  return "string";
    if(is_null(value))    return "null";
    if(is_number(value))  return "number";
    if(is_boolean(value)) return "boolean";
    if(is_object(value))  return "dict";
    if(is_array(value))   return "list";
    return "unknown";
}

/************************************************************
 *  Generate a unique node ID
 ************************************************************/
function gen_node_id(gobj)
{
    let priv = gobj.priv;
    priv.node_counter++;
    return "jn-" + priv.node_counter;
}

/************************************************************
 *  Add path segment
 ************************************************************/
function add_segment(path, segment)
{
    if(path.length) {
        return path + "`" + segment;
    }
    return String(segment);
}

/************************************************************
 *  Get last two segments for group label
 ************************************************************/
function get_group_label(path)
{
    let segments = path.split("`");
    if(segments.length >= 2) {
        return segments[segments.length-2] + "." + segments[segments.length-1];
    }
    if(segments.length === 1) {
        return segments[0];
    }
    return path;
}

/************************************************************
 *  Recursively build nodes and edges from JSON
 ************************************************************/
function build_json_nodes(gobj, path, kw, nodes, edges, parent_id)
{
    let group_id = gen_node_id(gobj);
    let is_dict = is_object(kw);
    let is_list = is_array(kw);

    if(!is_dict && !is_list) {
        return null;
    }

    /*
     *  Build HTML content for this group
     */
    let lines = [];
    let pending_complex = [];

    let entries = is_dict ? Object.entries(kw) : kw.map((v, i) => [i, v]);

    for(let [key, value] of entries) {
        let type = get_json_type(value);
        let html = build_cell_html(key, value, type);
        lines.push(html);

        if((is_object(value) && json_object_size(value) > 0) ||
           (is_array(value) && value.length > 0)) {
            pending_complex.push({key: key, value: value});
        }
    }

    let label = get_group_label(path) || (is_dict ? "{}" : "[]");
    let colors = is_dict ? GROUP_COLORS.dict : GROUP_COLORS.list;

    /*
     *  Build node HTML content
     */
    let content_html = lines.map(line =>
        `<div style="font-family:monospace; font-size:13px; padding:1px 6px; white-space:nowrap;">${line}</div>`
    ).join("");

    let min_width = Math.max(180, label.length * 9);

    let node_html = `
<div style="
    min-width: ${min_width}px;
    background: ${colors.fill};
    border: 1px ${is_dict ? 'dashed' : 'solid'} ${colors.stroke};
    border-radius: ${is_dict ? '6px' : '0'};
    opacity: 0.9;
    overflow: hidden;
">
    <div style="
        background: ${colors.stroke};
        color: white;
        font-weight: bold;
        font-size: 12px;
        padding: 3px 8px;
        text-align: center;
    ">${escapeHtml(label)}</div>
    ${content_html}
</div>`;

    let node = {
        id: group_id,
        type: 'html',
        data: {
            path: path,
            is_dict: is_dict,
        },
        style: {
            innerHTML: node_html,
            size: [min_width, 24 + lines.length * 22],
            dx: -(min_width / 2),
            dy: -((24 + lines.length * 22) / 2),
        },
    };
    nodes.push(node);

    /*
     *  Edge from parent to this group
     */
    if(parent_id) {
        edges.push({
            source: parent_id,
            target: group_id,
            style: {
                stroke: colors.stroke,
                lineWidth: 1,
            }
        });
    }

    /*
     *  Recurse into complex children
     */
    for(let pending of pending_complex) {
        build_json_nodes(
            gobj,
            add_segment(path, pending.key),
            pending.value,
            nodes,
            edges,
            group_id
        );
    }

    return group_id;
}

/************************************************************
 *  Load JSON data into graph
 ************************************************************/
function load_json(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    if(!graph) {
        return;
    }

    let json_data = gobj_read_attr(gobj, "json_data");
    if(!json_data) {
        return;
    }

    let json = json_deep_copy(json_data);
    let path = gobj_read_str_attr(gobj, "path") || "`";
    if(empty_string(path)) {
        path = "`";
    }

    priv.node_counter = 0;

    let nodes = [];
    let edges = [];

    build_json_nodes(gobj, path, json, nodes, edges, null);

    if(nodes.length > 0) {
        graph.setData({nodes: nodes, edges: edges});
        graph.render().then(() => {
            graph.fitView();
        });
    }
}

/************************************************************
 *  Clear and reload
 ************************************************************/
function refresh_json(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    if(graph) {
        graph.clear();
        load_json(gobj);
    }
}

/************************************************************
 *  Custom tree layout for JSON visualization
 ************************************************************/
class JsonTreeLayout extends BaseLayout {
    async execute(data, options) {
        const { nodes = [], edges = [] } = data;

        if(nodes.length === 0) {
            return {nodes: []};
        }

        /*
         *  Build adjacency: parent -> children
         */
        let children_map = {};
        let has_parent = new Set();
        for(let edge of edges) {
            if(!children_map[edge.source]) {
                children_map[edge.source] = [];
            }
            children_map[edge.source].push(edge.target);
            has_parent.add(edge.target);
        }

        /*
         *  Find roots (nodes without parents)
         */
        let roots = nodes.filter(n => !has_parent.has(n.id));
        if(roots.length === 0 && nodes.length > 0) {
            roots = [nodes[0]];
        }

        /*
         *  Node dimensions
         */
        let node_dims = {};
        for(let node of nodes) {
            let w = node.style && node.style.size ? node.style.size[0] : 200;
            let h = node.style && node.style.size ? node.style.size[1] : 60;
            node_dims[node.id] = {w, h};
        }

        const H_GAP = 60;  // horizontal gap between siblings
        const V_GAP = 50;  // vertical gap between levels

        /*
         *  Calculate subtree widths (bottom-up)
         */
        let subtree_widths = {};
        function calc_subtree_width(node_id) {
            let kids = children_map[node_id] || [];
            if(kids.length === 0) {
                subtree_widths[node_id] = node_dims[node_id].w;
                return subtree_widths[node_id];
            }
            let total = 0;
            for(let i = 0; i < kids.length; i++) {
                if(i > 0) total += H_GAP;
                total += calc_subtree_width(kids[i]);
            }
            subtree_widths[node_id] = Math.max(total, node_dims[node_id].w);
            return subtree_widths[node_id];
        }

        for(let root of roots) {
            calc_subtree_width(root.id);
        }

        /*
         *  Position nodes (top-down)
         */
        let positions = {};
        function position_node(node_id, x, y) {
            positions[node_id] = {x, y};

            let kids = children_map[node_id] || [];
            if(kids.length === 0) return;

            let total_width = 0;
            for(let i = 0; i < kids.length; i++) {
                if(i > 0) total_width += H_GAP;
                total_width += subtree_widths[kids[i]];
            }

            let child_y = y + node_dims[node_id].h + V_GAP;
            let start_x = x - total_width / 2;

            for(let kid of kids) {
                let kid_w = subtree_widths[kid];
                let kid_x = start_x + kid_w / 2;
                position_node(kid, kid_x, child_y);
                start_x += kid_w + H_GAP;
            }
        }

        /*
         *  Position roots side by side
         */
        let total_root_width = 0;
        for(let i = 0; i < roots.length; i++) {
            if(i > 0) total_root_width += H_GAP;
            total_root_width += subtree_widths[roots[i].id];
        }

        let root_x = 0;
        for(let root of roots) {
            let rw = subtree_widths[root.id];
            position_node(root.id, root_x + rw / 2, 0);
            root_x += rw + H_GAP;
        }

        return {
            nodes: nodes.map(node => ({
                id: node.id,
                style: {
                    x: positions[node.id] ? positions[node.id].x : 0,
                    y: positions[node.id] ? positions[node.id].y : 0,
                },
            })),
        };
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/************************************************************
 *
 ************************************************************/
function ac_load_data(gobj, event, kw, src)
{
    if(kw.path !== undefined) {
        gobj_write_str_attr(gobj, "path", kw.path);
    }
    if(kw.data !== undefined) {
        gobj_write_attr(gobj, "json_data", kw.data);
    }

    refresh_json(gobj);

    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_refresh(gobj, event, kw, src)
{
    refresh_json(gobj);
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

    let nodedata;
    try {
        nodedata = graph.getNodeData(node_id);
    } catch(e) {}

    if(nodedata && nodedata.data) {
        gobj_publish_event(gobj, "EV_JSON_ITEM_CLICKED", {
            path: nodedata.data.path,
            id: node_id
        });
    }

    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_show(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    if(graph) {
        let $canvas = document.getElementById(priv.canvas_id);
        if($canvas) {
            let rect = $canvas.getBoundingClientRect();
            if(rect.width > 0 && rect.height > 0) {
                graph.setSize(rect.width, rect.height);
                graph.render().then(() => {
                    graph.fitView();
                });
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
            ["EV_LOAD_DATA",            ac_load_data,           null],
            ["EV_REFRESH",              ac_refresh,             null],
            ["EV_ZOOM_IN",              ac_zoom_in,             null],
            ["EV_ZOOM_OUT",             ac_zoom_out,            null],
            ["EV_ZOOM_RESET",           ac_zoom_reset,          null],
            ["EV_CENTER",               ac_center,              null],
            ["EV_NODE_CLICK",           ac_node_click,          null],
            ["EV_SHOW",                 ac_show,                null],
            ["EV_HIDE",                 ac_hide,                null],
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_LOAD_DATA",            0],
        ["EV_REFRESH",              0],
        ["EV_ZOOM_IN",              0],
        ["EV_ZOOM_OUT",             0],
        ["EV_ZOOM_RESET",           0],
        ["EV_CENTER",               0],
        ["EV_NODE_CLICK",           0],
        ["EV_SHOW",                 0],
        ["EV_HIDE",                 0],
        ["EV_JSON_ITEM_CLICKED",    event_flag_t.EVF_OUTPUT_EVENT],
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
function register_c_yui_json_graph()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yui_json_graph };
