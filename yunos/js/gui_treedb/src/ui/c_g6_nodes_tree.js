/***********************************************************************
 *          g6_nodes_tree.js
 *
 *          Treedb's Nodes Tree Manager using AntV/G6
 *
 *          Migrated from mx_nodes_tree.js (mxGraph) to G6
 *
 *          Each G6 Node contains in his data:
 *          {
 *              topic_name: "...",
 *              schema: schema,     // Schema of topic
 *              record: null,       // Data of node
 *          }
 *
 *          Copyright (c) 2020-2021 Niyamaka.
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA,
    SDATA_END,
    data_type_t,
    kw_flag_t,
    event_flag_t,
    sdata_flag_t,
    gclass_flag_t,
    gclass_create,
    log_error,
    log_warning,
    trace_msg,
    gobj_read_pointer_attr,
    gobj_read_attr,
    gobj_read_str_attr,
    gobj_read_bool_attr,
    gobj_write_attr,
    gobj_write_str_attr,
    gobj_parent,
    gobj_name,
    gobj_short_name,
    gobj_subscribe_event,
    gobj_publish_event,
    gobj_send_event,
    gobj_find_service,
    gobj_save_persistent_attrs,
    clean_name,
    createElement2,
    sprintf,
    is_string,
    is_array,
    is_object,
    kw_get_int,
    kw_get_str,
    kw_get_dict_value,
    kw_set_dict_value,
    kw_clone_by_keys,
    json_object_update,
    json_object_update_missing,
    json_object_size,
    json_size,
    treedb_get_field_desc,
    treedb_decoder_fkey,
    kwid_find_one_record,
    str_in_list,
    delete_from_list,
    escapeHtml,
    safeSrc,
    refresh_language,
} from "yunetas";

import {
    addClasses,
    removeClasses,
    removeChildElements,
    disableElements,
    enableElements,
    set_submit_state,
    set_active_state,
    getStrokeColor,
} from "./lib_graph.js";

import {yui_toolbar} from "./yui_toolbar.js";

import {t} from "i18next";

import {
    BaseLayout,
    ExtensionCategory,
    Graph,
    NodeEvent,
    CanvasEvent,
    HistoryEvent,
    EdgeEvent,
    Circle,
    register,
} from '@antv/g6';

import {Circle as CircleGeometry} from '@antv/g';

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_G6_NODES_TREE";

const node_colors = [
    'rgb(237, 201, 73)',
    'rgb(118, 183, 178)',
    'rgb(255, 157, 167)',
    'rgb(175, 122, 161)',
    'rgb(89, 161, 79)',
    'rgb(186, 176, 171)',
    'rgb(66, 146, 198)',
];

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
/*---------------- Public Attributes ----------------*/
SDATA(data_type_t.DTP_POINTER,  "subscriber",           0,  null,   "Subscriber of output events"),

/*---------------- User last selections  ----------------*/
SDATA(data_type_t.DTP_LIST,     "modes",                0,
    '["reading", "operation", "writing", "edition"]',
    "Available permission or behaviour modes"),
SDATA(data_type_t.DTP_STRING,   "current_mode",         sdata_flag_t.SDF_PERSIST,
    "", "Current mode"),
SDATA(data_type_t.DTP_STRING,   "current_layout",       sdata_flag_t.SDF_PERSIST,
    "", "Current graph layout"),

/*---------------- Remote Connection ----------------*/
SDATA(data_type_t.DTP_POINTER,  "gobj_remote_yuno",     0,  null,   "Remote Yuno"),
SDATA(data_type_t.DTP_STRING,   "treedb_name",          0,  null,   "Treedb name"),
SDATA(data_type_t.DTP_DICT,     "descs",                0,  null,   "Descriptions of topics"),
SDATA(data_type_t.DTP_DICT,     "records",              0,  "{}",   "Data of topics"),
SDATA(data_type_t.DTP_LIST,     "topics",               0,  "[]",   "List of topic objects"),

/*---------------- Sub-container ----------------*/
SDATA(data_type_t.DTP_POINTER,  "$container",           0,  null,   "Container element, set externally"),
SDATA(data_type_t.DTP_STRING,   "canvas_id",            0,  "",     "Canvas ID"),

/*---------------- Graph Settings ----------------*/
SDATA(data_type_t.DTP_BOOLEAN,  "with_treedb_tables",   0,  false,  "Include treedb tables"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_gridline",        0,  false,  "Use gridline plugin"),
SDATA(data_type_t.DTP_DICT,     "graph_settings",       0,  {
    layouts: {
        "dagre": {
            type: 'dagre',
        },
        "antv-dagre": {
            type: 'antv-dagre',
        },
        "d3-force": {
            type: 'd3-force',
            link: {
                distance: 200,
                strength: 2
            },
            collide: {
                radius: 80,
            },
        },
        "force-atlas2": {
            type: 'force-atlas2',
            preventOverlap: true,
            kr: 20,
            graph_center: [250, 250],
        },

        // set manual the last
        "manual": {
            type: 'manual',
        },
    },
}, "Default graph settings"),

SDATA(data_type_t.DTP_STRING,   "hook_port_position",   0,  "bottom",   "Hook port position"),
SDATA(data_type_t.DTP_STRING,   "fkey_port_position",   0,  "top",      "Fkey port position"),

SDATA(data_type_t.DTP_STRING,   "wide",                 0,  "40px", "Height of header"),
SDATA(data_type_t.DTP_STRING,   "padding",              0,  "m-2",  "Padding or margin value"),

SDATA_END()
];

let PRIVATE_DATA = {
    _xy:                100,
    treedb_name:        "",
    gobj_remote_yuno:   null,
    descs:              null,
    records:            {},
    canvas_id:          "",
    graph:              null,   // Instance of G6
    __graphs__:         [],     // Rows of __graphs__
    yet_showed:         false,
    graph_settings:     null,
    edit_mode:          false,
    mode:               null,
    theme:              null,
};

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
        gobj_subscribe_event(__yui_main__, "EV_RESIZE", {}, gobj);
        gobj_subscribe_event(__yui_main__, "EV_THEME", {}, gobj);
        priv.theme = gobj_read_str_attr(__yui_main__, "theme");
    }

    build_ui(gobj);
    register_layouts(gobj);
    build_graph(gobj);
}

/***************************************************************
 *          Framework Method: Writing
 ***************************************************************/
function mt_writing(gobj, path)
{
    let priv = gobj.priv;
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    return 0;
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    return 0;
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
 *  Build node name: unique ID for a G6 node
 ************************************************************/
function build_node_name(gobj, topic_name, id)
{
    let priv = gobj.priv;

    return sprintf("node-%s-%s-%s", priv.treedb_name, topic_name, id);
}

/************************************************************
 *  Get default x,y for new nodes
 ************************************************************/
function get_default_ne_xy(gobj)
{
    let priv = gobj.priv;
    let xy = priv._xy;
    priv._xy += 5;
    return xy;
}

/************************************************************
 *   Build UI
 ************************************************************/
function build_ui(gobj)
{
    // Nothing to do, $container set externally
}

/************************************************************
 *   Destroy UI
 ************************************************************/
function destroy_ui(gobj)
{
    // Nothing to do, $container set externally
}

/************************************************************
 *   Make toolbar
 ************************************************************/
function make_toolbar(gobj)
{
    let priv = gobj.priv;

    let graph_settings = priv.graph_settings;
    let layouts = graph_settings.layouts;
    let layout_options = Object.keys(layouts).map(key => ['option', {}, key]);

    let modes = gobj_read_attr(gobj, "modes");
    let mode_options = modes.map(item => ['option', {}, item]);

    /*
     *  Left: layout and mode selectors
     */
    let left_items = [
        ['div', {class: 'select'}, [
            ['select', {class: 'graph_layout'}, layout_options]
        ], {
            change: (evt) => {
                evt.stopPropagation();
                gobj_send_event(gobj, "EV_LAYOUT", {layout: evt.target.value}, gobj);
            }
        }],

        ['div', {class: 'select'}, [
            ['select', {class: 'graph_mode'}, mode_options]
        ], {
            change: (evt) => {
                evt.stopPropagation();
                gobj_send_event(gobj, "EV_SET_MODE", {mode: evt.target.value}, gobj);
            }
        }],
    ];
    let l_icons = [
        ["fas fa-arrows-rotate",        "EV_REFRESH_TREEDB",false,  'i'],
    ];
    add_buttons(gobj, left_items, l_icons);

    /*
     *  Center: zoom and navigation controls
     */
    let center_items = [];
    let c_icons = [
        ["fas fa-magnifying-glass-plus", "EV_ZOOM_IN",       false,  'i'],
        ["fas fa-magnifying-glass",      "EV_ZOOM_RESET",    false,  'i'],
        ["fas fa-magnifying-glass-minus","EV_ZOOM_OUT",      false,  'i'],
        ["fas fa-arrows-to-eye",         "EV_CENTER",        false,  'i'],
    ];
    add_buttons(gobj, center_items, c_icons);

    /*
     *  Right: mode-specific buttons (filled by set_mode)
     */
    let right_items = [];

    const $toolbar_header = yui_toolbar({}, [
        ['div', {class: 'yui-horizontal-toolbar-section left'}, left_items],
        ['div', {class: 'yui-horizontal-toolbar-section center'}, center_items],
        ['div', {class: 'yui-horizontal-toolbar-section right mode_buttons'}, right_items]
    ]);

    refresh_language($toolbar_header, t);

    return $toolbar_header;
}

/************************************************************
 *  add_buttons() - helper to create toolbar buttons
 ************************************************************/
function create_button_handlers(gobj, event_name)
{
    return {
        click: (evt) => {
            evt.stopPropagation();
            gobj_send_event(gobj, event_name, {evt}, gobj);
        },
        contextmenu: (evt) => {
            evt.stopPropagation();
            evt.preventDefault();
            gobj_send_event(gobj, event_name, {evt}, gobj);
        }
    };
}

function push_button(zone, button, content, handlers)
{
    zone.push([
        'button',
        button,
        content,
        handlers
    ]);
}

function add_buttons(gobj, zone, c_icons)
{
    const toolbar_wide = gobj_read_attr(gobj, "wide");

    for(let i = 0; i < c_icons.length; i++) {
        const item = c_icons[i];
        const icon_name  = item[0];
        const event_name = item[1];
        const disabled   = item[2];
        const type       = item[3];

        const button = {
            class: `button ${event_name}`,
            style: {
                height: `${toolbar_wide}`
            }
        };

        if(disabled) {
            button.disabled = true;
        }

        const handlers = create_button_handlers(gobj, event_name);

        switch(type) {
            case 'i':
                {
                    button.style.width = "2.5em";
                    const icon = ['i', {
                        style: "font-size:1.5em; color:inherit;",
                        class: icon_name
                    }];
                    push_button(zone, button, icon, handlers);
                }
                break;

            default:
            case 't':
                {
                    push_button(zone, button, icon_name, handlers);
                }
                break;
        }
    }
}

/************************************************************
 *  Register custom G6 layouts
 ************************************************************/
function register_layouts(gobj)
{
    register(ExtensionCategory.LAYOUT, 'manual', ManualLayout);
    register(ExtensionCategory.NODE, 'light', LightNode);
}

/************************************************************
 *  Build a G6 graph instance
 ************************************************************/
function build_graph(gobj)
{
    let priv = gobj.priv;

    let layout = select_layout(gobj, gobj_read_str_attr(gobj, "current_layout"));

    const graph = priv.graph = new Graph({
        x: 0,
        y: 0,
        container: priv.$container,
        animation: false,
        autoResize: true,
        zoomRange: [0.2, 4],

        node: {
            palette: {
                type: 'group',
                color: 'tableau',
                field: 'topic_name',
            },
        },

        edge: {
            style: {
                startArrow: true,   // HACK target/source interchanged
                endArrow: false,
            },
        },

        plugins: [],
    });

    /*
     *  Set theme
     */
    graph.setTheme(priv.theme);

    let mode = select_mode(gobj);
    set_mode(gobj, mode);

    graph_set_layout(gobj, layout).then(() => {
        configure_events(gobj);
    });
}

/************************************************************
 *  Configure G6 event handlers
 ************************************************************/
function configure_events(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    graph.on(CanvasEvent.CLICK, (evt) => {
        gobj_send_event(gobj, "EV_CANVAS_CLICK", {evt: evt}, gobj);
    });

    graph.on(NodeEvent.DRAG_END, (evt) => {
        gobj_send_event(gobj, "EV_NODE_DRAG_END", {evt: evt}, gobj);
    });

    graph.on(NodeEvent.CLICK, (evt) => {
        gobj_send_event(gobj, "EV_NODE_CLICK", {evt: evt}, gobj);
    });

    graph.on(NodeEvent.CONTEXT_MENU, (evt) => {
        gobj_send_event(gobj, "EV_NODE_CONTEXT_MENU", {evt: evt}, gobj);
    });

    graph.on(EdgeEvent.CLICK, (evt) => {
        gobj_send_event(gobj, "EV_EDGE_CLICK", {evt: evt}, gobj);
    });

    do_extra_configuration(gobj);
}

/************************************************************
 *  Extra plugin configuration
 ************************************************************/
function do_extra_configuration(gobj)
{
    let priv = gobj.priv;

    if(gobj_read_bool_attr(gobj, "with_gridline")) {
        graph_add_plugin(
            gobj,
            'grid-line',
            {
                follow: false,
                stroke: priv.theme === 'dark'?'#343434':'#EEEEEE',
                borderStroke: priv.theme === 'dark'?'#656565':'#EEEEEE',
            }
        );
    }
}

/************************************************************
 *  Layout selection
 ************************************************************/
function select_layout(gobj, layout_name)
{
    let priv = gobj.priv;

    let graph_settings = priv.graph_settings;
    let $container = gobj_read_attr(gobj, "$container");

    if(!layout_name) {
        layout_name = gobj_read_str_attr(gobj, "current_layout");
    }
    if(!layout_name) {
        layout_name = Object.keys(graph_settings.layouts)[0];
    }

    gobj_write_str_attr(gobj, "current_layout", layout_name);

    let $input = $container.querySelector('.graph_layout');
    if($input) {
        $input.value = layout_name;
    }

    return graph_settings.layouts[layout_name];
}

/************************************************************
 *  Mode selection
 ************************************************************/
function select_mode(gobj, mode_name)
{
    let $container = gobj_read_attr(gobj, "$container");

    if(!mode_name) {
        mode_name = gobj_read_str_attr(gobj, "current_mode");
    }
    if(!mode_name) {
        mode_name = gobj_read_attr(gobj, "modes")[0];
    }

    gobj_write_str_attr(gobj, "current_mode", mode_name);

    let $input = $container.querySelector('.graph_mode');
    if($input) {
        $input.value = mode_name;
    }

    return mode_name;
}

/************************************************************
 *  Set mode: reading, operation, writing, edition
 ************************************************************/
function set_mode(gobj, mode)
{
    let $container = gobj_read_attr(gobj, "$container");

    let $zone = $container.querySelector('.mode_buttons');
    if($zone) {
        removeChildElements($zone);
    }

    if(!mode || !str_in_list(["reading","operation","writing","edition"], mode)) {
        mode = 'reading';
    }

    let behaviors = [];

    switch(mode) {
        case 'edition':
        case 'writing':
        case 'reading':
            behaviors = [
                'drag-canvas',
                'zoom-canvas',
            ];
            break;
        case 'operation':
            break;
    }
    graph_write_behaviors(gobj, behaviors);

    let c_icons = [];
    switch(mode) {
        case 'reading':
        case 'operation':
        case 'writing':
            break;
        case 'edition':
            c_icons = [
                ["fas fa-pen ",                 "EV_EDIT_MODE",     false,  'i'],
                ["fas fa-plus",                 "EV_NEW",           true,   'i'],
                ["fas fa-arrow-rotate-left",    "EV_HISTORY_UNDO",  true,   'i'],
                ["fas fa-arrow-rotate-right",   "EV_HISTORY_REDO",  true,   'i'],
                ["fas fa-save ",                "EV_SAVE_GRAPH",    true,   'i'],
            ];
            break;
    }

    if(c_icons.length && $zone) {
        let right_items = [];
        add_buttons(gobj, right_items, c_icons);
        for(let item of right_items) {
            $zone.appendChild(createElement2(item));
        }
    }
}

/**
 * Returns the proportional position (between 0 and 1) of a specific point
 */
function getPointPosition(count, index, margin = 0.2)
{
    if(count <= 0 || index < 0 || index >= count) {
        log_error("Invalid count or index");
        return 0.5;
    }

    const start = margin / 2;
    const end = 1 - margin / 2;
    const step = (end - start) / count;

    return start + index * step + step / 2;
}

/************************************************************
 *  Count hooks and fkeys in schema, classify node type
 ************************************************************/
function calculate_hooks_fkeys_counter(schema)
{
    let cols = schema.cols;
    schema.hooks_counter = 0;
    schema.fkeys_counter = 0;

    for(let i=0; i<cols.length; i++) {
        let col = cols[i];
        const field_desc = treedb_get_field_desc(col);
        switch(field_desc.type) {
            case "hook":
                schema.hooks_counter++;
                break;
            case "fkey":
                schema.fkeys_counter++;
                break;
        }
    }

    if(schema.hooks_counter === 0) {
        schema.node_treedb_type = 'child';
    } else if(schema.fkeys_counter === 0) {
        schema.node_treedb_type = 'extended';
    } else {
        schema.node_treedb_type = 'hierarchical';
    }
}

/************************************************************
 *  Build ports for a topic schema
 ************************************************************/
function build_ports(gobj, schema)
{
    let priv = gobj.priv;

    let top_ports = [];
    let bottom_ports = [];

    let cols = schema.cols;
    for(let i=0; i<cols.length; i++) {
        let col = cols[i];
        const field_desc = treedb_get_field_desc(col);
        let port = null;
        switch(field_desc.type) {
            case "hook":
                {
                    let child_schema = null;
                    let child_schema_name = Object.keys(col.hook)[0];
                    if(child_schema_name) {
                        child_schema = priv.descs[child_schema_name];
                    }
                    port = {
                        key: col.id,
                        fill: child_schema?child_schema.color:schema.color,
                        stroke: getStrokeColor(schema.color),
                    };
                    bottom_ports.push(port);
                }
                break;
            case "fkey":
                port = {
                    key: col.id,
                    fill: schema.color,
                    stroke: getStrokeColor(schema.color),
                };
                top_ports.push(port);
                break;
        }
    }

    /*
     *  Place the ports
     */
    for(let i=0; i<top_ports.length; i++) {
        top_ports[i].placement = [0.5, 0]; // all from same top point
    }
    for(let i=0; i<bottom_ports.length; i++) {
        let point = getPointPosition(bottom_ports.length, i);
        if(top_ports.length === 0) {
            bottom_ports[i].placement = [0, 0.5];
        } else {
            bottom_ports[i].placement = [point, 1];
        }
    }

    return [...top_ports, ...bottom_ports];
}

/************************************************************
 *  Create a topic node in the G6 graph
 ************************************************************/
function create_topic_node(gobj, schema, record)
{
    let priv = gobj.priv;
    const graph = priv.graph;

    let node_name = build_node_name(gobj, schema.topic_name, record.id);
    let xy = get_default_ne_xy(gobj);
    let geometry = record._geometry || {};
    let x = kw_get_int(gobj, geometry, "x", xy, kw_flag_t.KW_CREATE);
    let y = kw_get_int(gobj, geometry, "y", xy, kw_flag_t.KW_CREATE);

    let ports = build_ports(gobj, schema);

    let node_graph_type = null;
    let node_treedb_type = schema.node_treedb_type;

    let style = {
        x: x,
        y: y,
        fill: schema.color,
        stroke: getStrokeColor(schema.color),
        lineWidth: 1,
    };

    if(node_treedb_type === 'child') {
        // Pure child nodes: circle
        node_graph_type = 'circle';
        style.size = [60];
        json_object_update_missing(style, {
            labelText: record.id,
            labelPlacement: 'center',
            labelWordWrap: true,
            labelMaxWidth: "100%",
        });

    } else if(node_treedb_type === 'extended') {
        // Extended nodes (hooks > 0 && fkeys == 0): don't draw
        // node_graph_type remains null
        style.size = [80, 60];

    } else {
        // Hierarchical node: HTML
        node_graph_type = 'html';
        style.size = [150, 100];
        style.dx = -75;
        style.dy = -50;
        style.innerHTML = `
<div style="
    width: 100%;
    height: 100%;
    background: ${schema.color};
    border: 1px solid ${getStrokeColor(schema.color)};
    border-radius: 0.5rem;
    color: #000;
    display: flex;
    flex-direction: column;
    justify-content: center;
    align-items: center;
    padding: 10px;
">
    <div>
        <span class="icon is-large">
        <img src="${safeSrc(record.icon)}" alt=""/>
        </span>
    </div>
    <div style="font-weight: bold;">
      ${escapeHtml(record.id)}
    </div>
</div>
`;
    }

    let node_def = {
        id: node_name,
        type: node_graph_type,
        style: style,
        data: {
            topic_name: schema.topic_name,
            schema: schema,
            record: record
        }
    };

    if(json_size(ports)) {
        json_object_update_missing(style, {
            port: true,
            ports: ports,
            portR: node_treedb_type === 'child' ? 2 : 6,
            portLineWidth: 1,
        });
    }

    if(node_graph_type) {
        graph.addNodeData([node_def]);
    }
}

/************************************************************
 *  Update topic node data
 ************************************************************/
function update_topic_node(gobj, schema, node_name, record)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    try {
        let nodedata = graph.getNodeData(node_name);
        if(!nodedata) {
            log_error(`update_topic_node: node not found: ${node_name}`);
            return;
        }

        // Update record data
        nodedata.data.record = record;

        // Update label if it's a child node
        if(schema.node_treedb_type === 'child') {
            graph.updateNodeData([{
                id: node_name,
                style: {
                    labelText: record.id,
                }
            }]);
        } else if(schema.node_treedb_type === 'hierarchical') {
            graph.updateNodeData([{
                id: node_name,
                style: {
                    innerHTML: `
<div style="
    width: 100%;
    height: 100%;
    background: ${schema.color};
    border: 1px solid ${getStrokeColor(schema.color)};
    border-radius: 0.5rem;
    color: #000;
    display: flex;
    flex-direction: column;
    justify-content: center;
    align-items: center;
    padding: 10px;
">
    <div>
        <span class="icon is-large">
        <img src="${safeSrc(record.icon)}" alt=""/>
        </span>
    </div>
    <div style="font-weight: bold;">
      ${escapeHtml(record.id)}
    </div>
</div>
`,
                }
            }]);
        }
    } catch(e) {
        log_error(e.message);
    }
}

/************************************************************
 *  Remove a topic node from G6 graph
 ************************************************************/
function remove_topic_node(gobj, node_name)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    try {
        let nodedata = graph.getNodeData(node_name);
        if(nodedata) {
            graph.removeNodeData([node_name]);
        }
    } catch(e) {
        log_error(e.message);
    }
}

/************************************************************
 *  Update local record in records dict
 ************************************************************/
function update_local_node(gobj, topic_name, node)
{
    let priv = gobj.priv;

    let records = priv.records[topic_name];
    if(!records) {
        return;
    }

    for(let i=0; i<records.length; i++) {
        if(records[i].id === node.id) {
            records[i] = node;
            return;
        }
    }
}

/************************************************************
 *  Remove local record from records dict
 ************************************************************/
function remove_local_node(gobj, topic_name, node)
{
    let priv = gobj.priv;

    let records = priv.records[topic_name];
    if(!records) {
        return;
    }

    for(let i=0; i<records.length; i++) {
        if(records[i].id === node.id) {
            records.splice(i, 1);
            return;
        }
    }
}

/************************************************************
 *  Create all links from records and render
 ************************************************************/
function create_links(gobj)
{
    let priv = gobj.priv;

    for(const [topic_name, records] of Object.entries(priv.records)) {
        let schema = priv.descs[topic_name];
        for(let i = 0; i < records.length; i++) {
            let record = records[i];
            draw_links(gobj, schema, record, true);
        }
    }
    graph_render(gobj);
}

/************************************************************
 *  Draw links for a record based on its fkey fields
 ************************************************************/
function draw_links(gobj, schema, record, initial_load)
{
    let cols = schema.cols;
    let topic_name = schema.topic_name;
    let record_id = record.id;

    for(let i=0; i<cols.length; i++) {
        let col = cols[i];
        if(!col.fkey) {
            continue;
        }

        let fkeys = record[col.id];

        if(fkeys) {
            if(is_string(fkeys)) {
                draw_link(gobj, topic_name, record_id, col, fkeys, initial_load);
            } else if(is_array(fkeys)) {
                for(let j=0; j<fkeys.length; j++) {
                    draw_link(gobj, topic_name, record_id, col, fkeys[j], initial_load);
                }
            } else {
                log_error(`${gobj_short_name(gobj)}: fkey type unsupported: ` + JSON.stringify(fkeys));
            }
        }
    }
}

/************************************************************
 *  Draw a single link (edge) between two nodes
 ************************************************************/
function draw_link(
    gobj,
    source_topic_name,
    source_topic_id,
    source_col,
    fkey,
    verbose
)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    /*
     *  Decode fkey: the link to the target
     */
    let target_fkey = treedb_decoder_fkey(source_col, fkey);
    if(!target_fkey) {
        log_error("draw_link: cannot decode fkey");
        return;
    }
    let target_topic_name = target_fkey.topic_name;
    let source_schema = priv.descs[target_topic_name];
    if(source_schema && source_schema.node_treedb_type === 'extended') {
        return;
    }
    let target_topic_id = target_fkey.id;
    let target_hook = target_fkey.hook_name;

    let target_node_name = build_node_name(gobj, target_topic_name, target_topic_id);

    /*
     *  Target node must exist
     */
    let target_cell;
    try {
        target_cell = graph.getNodeData(target_node_name);
    } catch(e) {
        if(verbose) {
            log_error(e.message);
        }
    }
    if(!target_cell) {
        if(verbose) {
            log_error(`${gobj_short_name(gobj)}: target_cell NOT FOUND: ${target_node_name}`);
        }
        return;
    }

    /*
     *  Source node (me, the child)
     */
    let source_node_name = build_node_name(gobj, source_topic_name, source_topic_id);
    let style = graph.getElementRenderStyle(source_node_name);

    /*
     *  Create the edge
     *  HACK: target/source are interchanged so arrows point parent -> child
     */
    let edge = {
        type: 'cubic',
        source: target_node_name,
        target: source_node_name,
        style: {
            sourcePort: target_hook,
            targetPort: source_col.id,
            lineWidth: 2,
            stroke: style.fill,
        }
    };

    try {
        graph.addEdgeData([edge]);
    } catch(e) {
        if(verbose) {
            log_error(e.message);
        }
    }
}

/************************************************************
 *  Clear links for a record
 ************************************************************/
function clear_links(gobj, schema, record, verbose)
{
    let cols = schema.cols;
    let topic_name = schema.topic_name;
    let record_id = record.id;

    for(let i=0; i<cols.length; i++) {
        let col = cols[i];
        if(!col.fkey) {
            continue;
        }

        let fkeys = record[col.id];

        if(fkeys) {
            if(is_string(fkeys)) {
                clear_link(gobj, topic_name, record_id, col, fkeys, verbose);
            } else if(is_array(fkeys)) {
                for(let j=0; j<fkeys.length; j++) {
                    clear_link(gobj, topic_name, record_id, col, fkeys[j], verbose);
                }
            } else {
                log_error(`${gobj_short_name(gobj)}: fkey type unsupported: ` + JSON.stringify(fkeys));
            }
        }
    }
}

/************************************************************
 *  Clear a single link (edge)
 ************************************************************/
function clear_link(
    gobj,
    source_topic_name,
    source_topic_id,
    source_col,
    fkey,
    verbose
)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    let target_fkey = treedb_decoder_fkey(source_col, fkey);
    if(!target_fkey) {
        log_error("clear_link: cannot decode fkey");
        return;
    }
    let target_topic_name = target_fkey.topic_name;
    let target_topic_id = target_fkey.id;

    let target_node_name = build_node_name(gobj, target_topic_name, target_topic_id);

    let target_cell;
    try {
        target_cell = graph.getNodeData(target_node_name);
    } catch(e) {
        if(verbose) {
            log_error(e.message);
        }
    }
    if(!target_cell) {
        if(verbose) {
            log_error(`${gobj_short_name(gobj)}: target_cell NOT FOUND: ${target_node_name}`);
        }
        return;
    }

    let source_node_name = build_node_name(gobj, source_topic_name, source_topic_id);

    try {
        let edge_id = `${source_node_name}-${target_node_name}`;
        let edge = graph.getEdgeData(edge_id);
        if(edge) {
            graph.removeEdge(edge);
        }
    } catch(e) {
        if(verbose) {
            log_error(e.message);
        }
    }
}

/************************************************************
 *  Process nodes command response
 *  Called when data arrives from backend
 ************************************************************/
function process_command_nodes(gobj, topic_name, schema, data)
{
    let priv = gobj.priv;

    if(topic_name.substring(0, 2) === "__") {
        if(topic_name === '__graphs__') {
            priv.__graphs__ = data;
        }
        return;
    }

    /*
     *  Save topic's records
     */
    priv.records[topic_name] = data;
    priv.descs[topic_name].loaded = true;

    /*
     *  Create nodes
     */
    for(let i=0; i<data.length; i++) {
        let record = data[i];
        if(priv.graph) {
            create_topic_node(gobj, schema, record);
        }
    }

    /*
     *  Check if all topics loaded to make links
     */
    let do_links = true;
    for(const [tn, desc] of Object.entries(priv.descs)) {
        if(tn.substring(0, 2) === "__") {
            continue;
        }
        if(!desc.loaded) {
            do_links = false;
            break;
        }
    }

    if(do_links && priv.graph) {
        graph_render(gobj).then(() => {
            create_links(gobj);
        });
    }
}

/************************************************************
 *  Process descs: assign colors and calculate counters
 ************************************************************/
function process_descs(gobj, descs)
{
    let priv = gobj.priv;

    let idx = 0;
    for(const [topic_name, desc] of Object.entries(descs)) {
        calculate_hooks_fkeys_counter(desc);
        if(topic_name.substring(0, 2) === "__") {
            continue;
        }
        desc.color = node_colors[idx % node_colors.length];
        idx++;
    }
}

/************************************************************
 *  Update geometry from render style
 ************************************************************/
function update_geometry(gobj, node_id)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    let nodedata = graph.getNodeData(node_id);
    let style = graph.getElementRenderStyle(node_id);

    let record = nodedata.data.record;
    let _geometry = kw_get_dict_value(gobj,
        record,
        "_geometry",
        {},
        kw_flag_t.KW_CREATE
    );
    if(!is_object(_geometry)) {
        _geometry = {};
        kw_set_dict_value(
            gobj,
            record,
            "_geometry",
            _geometry
        );
    }
    json_object_update(
        _geometry,
        kw_clone_by_keys(gobj,
            style,
            ["x", "y", "size"]
        )
    );
}

/************************************************************
 *  Save all node geometries to backend
 ************************************************************/
function save_geometry(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    const nodes = graph.getData().nodes;
    for(let i=0; i<nodes.length; i++) {
        let node = nodes[i];
        update_geometry(gobj, node.id);

        let nodedata = graph.getNodeData(node.id);
        let record = nodedata.data.record;
        let _geometry = kw_get_dict_value(gobj,
            record,
            "_geometry",
            null,
            kw_flag_t.KW_REQUIRED
        );
        if(!_geometry) {
            continue;
        }

        let options = {
            list_dict: true,
            autolink: false
        };
        let kw_update = {
            treedb_name: priv.treedb_name,
            topic_name: nodedata.data.topic_name,
            record: {
                id: record.id,
                _geometry: _geometry
            },
            options: options
        };
        gobj_publish_event(gobj, "EV_UPDATE_NODE", kw_update);
    }
}

/************************************************************
 *  Graph utility functions
 ************************************************************/
async function graph_center(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    await graph.fitCenter();
    await graph.fitView();
}

async function graph_zoom_in(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    const z = graph.getZoom();
    await graph.zoomTo(z * 1.1);
}

async function graph_zoom_out(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    const z = graph.getZoom();
    await graph.zoomTo(z * 0.9);
}

async function graph_reset(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    if(graph.rendered) {
        await graph.zoomTo(1);
        await graph.translateTo([0, 0]);
    }
}

async function graph_render(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    await graph.render();
}

function graph_add_plugin(gobj, plugin_key, options)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    let plugin = graph.getPluginInstance(plugin_key);
    if(!plugin) {
        let plugin_def = {
            key: plugin_key,
            type: plugin_key,
        };
        if(json_object_size(options)) {
            json_object_update_missing(plugin_def, options);
        }
        graph.setPlugins((plugins) => [...plugins, plugin_def]);
    } else {
        log_error(`${gobj_short_name(gobj)}: Plugin already defined ${plugin_key}`);
    }

    plugin = graph.getPluginInstance(plugin_key);

    switch(plugin_key) {
        case 'history':
            if(plugin) {
                plugin.emitter.on(HistoryEvent.ADD, () => {
                    update_history_buttons(gobj);
                });
            }
            break;
    }
}

function graph_remove_plugin(gobj, plugin_key, verbose)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    const plugin = graph.getPluginInstance(plugin_key);
    if(plugin) {
        let plugins = graph.getPlugins();
        plugins = plugins.filter((p) => p.key !== plugin_key);
        graph.setPlugins(plugins);
        if(!plugin.destroyed) {
            if(plugin.destroy) {
                plugin.destroy();
            }
        }
    } else {
        if(verbose) {
            log_error(`${gobj_short_name(gobj)}: Plugin not defined ${plugin_key}`);
        }
    }
}

async function clear_graph(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    let mode = select_mode(gobj);
    set_mode(gobj, mode);

    priv._xy = 100;
    priv.yet_showed = false;
    priv.edit_mode = false;

    graph_remove_plugin(gobj, 'history');
    update_history_buttons(gobj);

    await graph.clear();

    let layout = select_layout(gobj);
    await graph_set_layout(gobj, layout);
}

async function graph_set_layout(gobj, layout)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    graph.setLayout(layout);
    await graph_reset(gobj);
    await graph_render(gobj);
}

function graph_resize(gobj, width, height)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    graph.setSize(width, height);
}

function graph_write_behaviors(gobj, behaviors)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    graph.setOptions({
        behaviors: behaviors
    });
}

function graph_set_behavior(gobj, behavior, set)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    let options = graph.getOptions();
    let behaviors = options.behaviors;

    if(set) {
        if(!str_in_list(behaviors, behavior)) {
            behaviors.push(behavior);
        }
    } else {
        if(str_in_list(behaviors, behavior)) {
            delete_from_list(behaviors, behavior);
        }
    }

    graph.setOptions({
        behaviors: behaviors
    });
}

function update_history_buttons(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    let $container = gobj_read_attr(gobj, "$container");

    const history = graph.getPluginInstance('history');
    if(history) {
        if(history.canRedo()) {
            enableElements($container, ".EV_HISTORY_REDO");
            set_active_state($container, ".EV_HISTORY_REDO", true);
        } else {
            disableElements($container, ".EV_HISTORY_REDO");
            set_active_state($container, ".EV_HISTORY_REDO", false);
        }

        if(history.canUndo()) {
            enableElements($container, ".EV_HISTORY_UNDO");
            set_active_state($container, ".EV_HISTORY_UNDO", true);
        } else {
            disableElements($container, ".EV_HISTORY_UNDO");
            set_active_state($container, ".EV_HISTORY_UNDO", false);
        }
    } else {
        disableElements($container, ".EV_HISTORY_REDO");
        set_active_state($container, ".EV_HISTORY_REDO", false);
        disableElements($container, ".EV_HISTORY_UNDO");
        set_active_state($container, ".EV_HISTORY_UNDO", false);
    }
}

/************************************************************
 *  Custom G6 node: LightNode (circle with status indicator)
 ************************************************************/
class LightNode extends Circle
{
    render(attributes, container) {
        super.render(attributes, container);
        this.upsert('light', CircleGeometry, { r: 8, fill: '#0f0', cx: 0, cy: -25 }, container);
    }
}

/************************************************************
 *  Custom G6 layout: ManualLayout (uses _geometry positions)
 ************************************************************/
class ManualLayout extends BaseLayout
{
    async execute(data, options) {
        const { nodes = [] } = data;
        return {
            nodes: nodes.map((node) => ({
                id: node.id,
                style: {
                    x: node.data.record._geometry ? node.data.record._geometry.x : 0,
                    y: node.data.record._geometry ? node.data.record._geometry.y : 0,
                },
            })),
        };
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/************************************************************
 *  Receive descs from parent
 ************************************************************/
function ac_descs(gobj, event, kw, src)
{
    let priv = gobj.priv;

    priv.descs = kw;
    gobj_write_attr(gobj, "descs", kw);
    process_descs(gobj, priv.descs);

    return 0;
}

/************************************************************
 *  Clear all graph data
 ************************************************************/
function ac_clear_data(gobj, event, kw, src)
{
    let priv = gobj.priv;

    priv.records = {};
    gobj_write_attr(gobj, "records", priv.records);

    if(priv.graph) {
        clear_graph(gobj);
    }

    return 0;
}

/************************************************************
 *  Load batch data: {schema, data}
 ************************************************************/
function ac_load_data(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let schema = kw.schema;
    let data = kw.data;

    if(!priv.descs) {
        log_error(`${gobj_short_name(gobj)}: descs not available yet`);
        return 0;
    }

    let topic_name = schema ? schema.topic_name : "";
    if(!topic_name) {
        log_error(`${gobj_short_name(gobj)}: No topic_name in schema`);
        return 0;
    }

    /*
     *  Use descs schema (may have colors and counters calculated)
     */
    let desc = priv.descs[topic_name];
    if(desc) {
        schema = desc;
    }

    process_command_nodes(gobj, topic_name, schema, data);

    return 0;
}

/************************************************************
 *  Create graph styles (from parent)
 ************************************************************/
function ac_create_graph_styles(gobj, event, kw, src)
{
    // Styles are handled internally via topic colors
    return 0;
}

/************************************************************
 *  Node created (from subscription)
 ************************************************************/
function ac_node_created(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let schema_kw = kw.schema;
    let topic_name = kw.topic_name;
    let node = kw.node;

    if(!priv.descs || !priv.graph) {
        return 0;
    }

    let schema = priv.descs[topic_name];
    if(!schema) {
        log_error(`ac_node_created: unknown topic: ${topic_name}`);
        return 0;
    }

    /*
     *  Add to local records
     */
    if(!priv.records[topic_name]) {
        priv.records[topic_name] = [];
    }
    priv.records[topic_name].push(node);

    /*
     *  Create graph node and links
     */
    create_topic_node(gobj, schema, node);
    draw_links(gobj, schema, node, false);

    graph_render(gobj);

    return 0;
}

/************************************************************
 *  Node updated (from subscription)
 ************************************************************/
function ac_node_updated(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let topic_name = kw.topic_name;
    let node = kw.node;

    if(!priv.descs || !priv.graph) {
        return 0;
    }

    let schema = priv.descs[topic_name];
    if(!schema) {
        log_error(`ac_node_updated: unknown topic: ${topic_name}`);
        return 0;
    }

    let node_name = build_node_name(gobj, topic_name, node.id);

    /*
     *  Clear old links, update record, draw new links
     */
    let old_record = null;
    let records = priv.records[topic_name];
    if(records) {
        for(let i=0; i<records.length; i++) {
            if(records[i].id === node.id) {
                old_record = records[i];
                break;
            }
        }
    }
    if(old_record) {
        clear_links(gobj, schema, old_record, false);
    }

    update_topic_node(gobj, schema, node_name, node);
    update_local_node(gobj, topic_name, node);

    draw_links(gobj, schema, node, false);

    graph_render(gobj);

    return 0;
}

/************************************************************
 *  Node deleted (from subscription)
 ************************************************************/
function ac_node_deleted(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let topic_name = kw.topic_name;
    let node = kw.node;

    if(!priv.descs || !priv.graph) {
        return 0;
    }

    let schema = priv.descs[topic_name];
    if(!schema) {
        log_error(`ac_node_deleted: unknown topic: ${topic_name}`);
        return 0;
    }

    let node_name = build_node_name(gobj, topic_name, node.id);

    clear_links(gobj, schema, node, false);
    remove_topic_node(gobj, node_name);
    remove_local_node(gobj, topic_name, node);

    graph_render(gobj);

    return 0;
}

/************************************************************
 *  Refresh treedb
 ************************************************************/
function ac_refresh_treedb(gobj, event, kw, src)
{
    clear_graph(gobj);
    gobj_publish_event(gobj, "EV_REFRESH_TREEDB", {});
    return 0;
}

/************************************************************
 *  Show
 ************************************************************/
function ac_show(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let $canvas_container = priv.$container;
    if(!$canvas_container) {
        return 0;
    }
    let rect = $canvas_container.getBoundingClientRect();

    if(!priv.yet_showed && priv.graph) {
        priv.yet_showed = true;

        graph_resize(gobj, rect.width, rect.height);
        graph_reset(gobj).then(() => { graph_render(gobj); });
    }

    return 0;
}

/************************************************************
 *  Hide
 ************************************************************/
function ac_hide(gobj, event, kw, src)
{
    return 0;
}

/************************************************************
 *  Resize (subscribed from __yui_main__)
 ************************************************************/
function ac_resize(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let $canvas_container = priv.$container;
    if(!$canvas_container) {
        return 0;
    }
    let rect = $canvas_container.getBoundingClientRect();
    if(rect.width === 0 || rect.height === 0) {
        priv.yet_showed = false;
    } else {
        if(priv.graph) {
            let h = rect.height;
            if(h < 0) {
                h = 80;
            }
            graph_resize(gobj, rect.width, h);
        }
    }

    return 0;
}

/************************************************************
 *  Theme change (subscribed from __yui_main__)
 ************************************************************/
function ac_theme(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    let theme = kw.theme || 'light';
    priv.theme = theme;
    graph.setTheme(theme);

    graph.updatePlugin({
        key: 'grid-line',
        stroke: theme === 'dark' ? '#343434' : '#EEEEEE',
        borderStroke: theme === 'dark' ? '#656565' : '#EEEEEE',
    });

    graph_render(gobj);

    return 0;
}

/************************************************************
 *  Zoom controls
 ************************************************************/
function ac_zoom_in(gobj, event, kw, src)
{
    graph_zoom_in(gobj);
    return 0;
}

function ac_zoom_out(gobj, event, kw, src)
{
    graph_zoom_out(gobj);
    return 0;
}

function ac_zoom_reset(gobj, event, kw, src)
{
    graph_reset(gobj);
    return 0;
}

function ac_center(gobj, event, kw, src)
{
    graph_center(gobj);
    return 0;
}

function ac_fullscreen(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    const plugin = graph.getPluginInstance('fullscreen');
    if(plugin) {
        plugin.request();
    }
    return 0;
}

/************************************************************
 *  Layout change
 ************************************************************/
function ac_layout(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let layout = select_layout(gobj, kw.layout);
    gobj_save_persistent_attrs(gobj, "current_layout");
    graph_set_layout(gobj, layout).then(() => {
        if(priv.edit_mode) {
            let $container = gobj_read_attr(gobj, "$container");
            enableElements($container, ".EV_SAVE_GRAPH");
            set_submit_state($container, ".EV_SAVE_GRAPH", true);
        }
    });

    return 0;
}

/************************************************************
 *  Mode change
 ************************************************************/
function ac_set_mode(gobj, event, kw, src)
{
    let mode = select_mode(gobj, kw.mode);
    gobj_save_persistent_attrs(gobj, "current_mode");
    set_mode(gobj, mode);
    return 0;
}

/************************************************************
 *  Edit mode toggle
 ************************************************************/
function ac_edit_mode(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let $container = gobj_read_attr(gobj, "$container");

    priv.edit_mode = !priv.edit_mode;
    if(priv.edit_mode) {
        set_active_state($container, ".EV_EDIT_MODE", true);
        graph_set_behavior(gobj, 'drag-element', true);
        graph_add_plugin(gobj, 'history');
    } else {
        set_active_state($container, ".EV_EDIT_MODE", false);
        graph_set_behavior(gobj, 'drag-element', false);
        disableElements($container, ".EV_SAVE_GRAPH");
        set_submit_state($container, ".EV_SAVE_GRAPH", false);
        graph_remove_plugin(gobj, 'history');
        update_history_buttons(gobj);
    }

    return 0;
}

/************************************************************
 *  Save graph geometry
 ************************************************************/
function ac_save_graph(gobj, event, kw, src)
{
    let priv = gobj.priv;

    if(priv.edit_mode) {
        let $container = gobj_read_attr(gobj, "$container");
        disableElements($container, ".EV_SAVE_GRAPH");
        set_submit_state($container, ".EV_SAVE_GRAPH", false);
        save_geometry(gobj);
    }

    return 0;
}

/************************************************************
 *  Node drag end
 ************************************************************/
function ac_node_drag_end(gobj, event, kw, src)
{
    let priv = gobj.priv;

    if(priv.edit_mode) {
        let $container = gobj_read_attr(gobj, "$container");
        enableElements($container, ".EV_SAVE_GRAPH");
        set_submit_state($container, ".EV_SAVE_GRAPH", true);
    }

    return 0;
}

/************************************************************
 *  Node click - publish vertex clicked event
 ************************************************************/
function ac_node_click(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    let node_id = kw.evt.target.id;

    try {
        let nodedata = graph.getNodeData(node_id);
        if(nodedata && nodedata.data && nodedata.data.schema) {
            gobj_publish_event(gobj, "EV_VERTEX_CLICKED", {
                treedb_name: priv.treedb_name,
                topic_name: nodedata.data.schema.topic_name,
                record: nodedata.data.record
            });
        }
    } catch(e) {
        // Clicked on non-node element
    }

    return 0;
}

/************************************************************
 *  Edge click - publish edge clicked event
 ************************************************************/
function ac_edge_click(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let edge_id = kw.evt.target.id;
    trace_msg(`edge_id ${edge_id}`);

    return 0;
}

/************************************************************
 *  Node context menu
 ************************************************************/
function ac_node_context_menu(gobj, event, kw, src)
{
    return 0;
}

/************************************************************
 *  Canvas click
 ************************************************************/
function ac_canvas_click(gobj, event, kw, src)
{
    return 0;
}

/************************************************************
 *  History undo/redo
 ************************************************************/
function ac_history_redo(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    if(priv.edit_mode) {
        const history = graph.getPluginInstance('history');
        if(history && history.canRedo()) {
            history.redo();
        }
        update_history_buttons(gobj);
    }

    return 0;
}

function ac_history_undo(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    if(priv.edit_mode) {
        const history = graph.getPluginInstance('history');
        if(history && history.canUndo()) {
            history.undo();
        }
        update_history_buttons(gobj);
    }

    return 0;
}

/************************************************************
 *  CRUD events - publish to parent for backend handling
 ************************************************************/
function ac_create_node(gobj, event, kw, src)
{
    gobj_publish_event(gobj, "EV_CREATE_NODE", kw);
    return 0;
}

function ac_update_node(gobj, event, kw, src)
{
    gobj_publish_event(gobj, "EV_UPDATE_NODE", kw);
    return 0;
}

function ac_delete_node(gobj, event, kw, src)
{
    gobj_publish_event(gobj, "EV_DELETE_NODE", kw);
    return 0;
}

function ac_link_nodes(gobj, event, kw, src)
{
    gobj_publish_event(gobj, "EV_LINK_NODES", kw);
    return 0;
}

function ac_unlink_nodes(gobj, event, kw, src)
{
    gobj_publish_event(gobj, "EV_UNLINK_NODES", kw);
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
    mt_writing:     mt_writing,
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
            /*--- Data events from parent ---*/
            ["EV_DESCS",                    ac_descs,               null],
            ["EV_CLEAR_DATA",               ac_clear_data,          null],
            ["EV_LOAD_DATA",                ac_load_data,           null],
            ["EV_CREATE_GRAPH_STYLES",      ac_create_graph_styles, null],
            ["EV_NODE_CREATED",             ac_node_created,        null],
            ["EV_NODE_UPDATED",             ac_node_updated,        null],
            ["EV_NODE_DELETED",             ac_node_deleted,        null],

            /*--- Graph interaction events ---*/
            ["EV_NODE_CLICK",               ac_node_click,          null],
            ["EV_EDGE_CLICK",               ac_edge_click,          null],
            ["EV_NODE_CONTEXT_MENU",        ac_node_context_menu,   null],
            ["EV_CANVAS_CLICK",             ac_canvas_click,        null],
            ["EV_NODE_DRAG_END",            ac_node_drag_end,       null],

            /*--- Toolbar events ---*/
            ["EV_REFRESH_TREEDB",           ac_refresh_treedb,      null],
            ["EV_ZOOM_IN",                  ac_zoom_in,             null],
            ["EV_ZOOM_OUT",                 ac_zoom_out,            null],
            ["EV_ZOOM_RESET",               ac_zoom_reset,          null],
            ["EV_CENTER",                   ac_center,              null],
            ["EV_FULLSCREEN",               ac_fullscreen,          null],
            ["EV_LAYOUT",                   ac_layout,              null],
            ["EV_SET_MODE",                 ac_set_mode,            null],
            ["EV_EDIT_MODE",                ac_edit_mode,           null],
            ["EV_SAVE_GRAPH",               ac_save_graph,          null],
            ["EV_HISTORY_UNDO",             ac_history_undo,        null],
            ["EV_HISTORY_REDO",             ac_history_redo,        null],

            /*--- CRUD pass-through events ---*/
            ["EV_CREATE_NODE",              ac_create_node,         null],
            ["EV_UPDATE_NODE",              ac_update_node,         null],
            ["EV_DELETE_NODE",              ac_delete_node,         null],
            ["EV_LINK_NODES",               ac_link_nodes,          null],
            ["EV_UNLINK_NODES",             ac_unlink_nodes,        null],

            /*--- UI events ---*/
            ["EV_SHOW",                     ac_show,                null],
            ["EV_HIDE",                     ac_hide,                null],
            ["EV_RESIZE",                   ac_resize,              null],
            ["EV_THEME",                    ac_theme,               null],
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        /*--- Data events (received) ---*/
        ["EV_DESCS",                    0],
        ["EV_CLEAR_DATA",               0],
        ["EV_LOAD_DATA",                0],
        ["EV_CREATE_GRAPH_STYLES",      0],
        ["EV_NODE_CREATED",             0],
        ["EV_NODE_UPDATED",             0],
        ["EV_NODE_DELETED",             0],

        /*--- Graph interaction (internal) ---*/
        ["EV_NODE_CLICK",               0],
        ["EV_EDGE_CLICK",               0],
        ["EV_NODE_CONTEXT_MENU",        0],
        ["EV_CANVAS_CLICK",             0],
        ["EV_NODE_DRAG_END",            0],

        /*--- Toolbar (internal) ---*/
        ["EV_ZOOM_IN",                  0],
        ["EV_ZOOM_OUT",                 0],
        ["EV_ZOOM_RESET",               0],
        ["EV_CENTER",                   0],
        ["EV_FULLSCREEN",               0],
        ["EV_LAYOUT",                   0],
        ["EV_SET_MODE",                 0],
        ["EV_EDIT_MODE",                0],
        ["EV_SAVE_GRAPH",               0],
        ["EV_HISTORY_UNDO",             0],
        ["EV_HISTORY_REDO",             0],

        /*--- Published to parent ---*/
        ["EV_VERTEX_CLICKED",           event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_EDGE_CLICKED",             event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_REFRESH_TREEDB",           event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_CREATE_NODE",              event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_UPDATE_NODE",              event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_DELETE_NODE",              event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_LINK_NODES",               event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_UNLINK_NODES",             event_flag_t.EVF_OUTPUT_EVENT],

        // TODO some events to review from mx_nodes_tree.js
        // ["EV_SHOW_HOOK_DATA",           event_flag_t.EVF_OUTPUT_EVENT],
        // ["EV_SHOW_TREEDB_TOPIC",        event_flag_t.EVF_OUTPUT_EVENT],

        // "EV_CREATE_VERTEX",
        // "EV_DELETE_VERTEX",
        // "EV_CLONE_VERTEX",
        // "EV_DELETE_EDGE",
        // "EV_SHOW_CELL_DATA_FORM",
        // "EV_SHOW_CELL_DATA_JSON",
        // "EV_POPUP_MENU",
        // "EV_EXTEND_SIZE",    -> show levels
        // "EV_MX_CLICK",
        // "EV_MX_DOUBLECLICK",
        // "EV_MX_SELECTION_CHANGE",
        // "EV_MX_ADDCELLS",
        // "EV_MX_MOVECELLS",
        // "EV_MX_RESIZECELLS",
        // "EV_MX_CONNECTCELL",

        /*--- UI events ---*/
        ["EV_SHOW",                     0],
        ["EV_HIDE",                     0],
        ["EV_RESIZE",                   0],
        ["EV_THEME",                    0],
    ];

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
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
        gclass_flag_t.gcflag_manual_start   // gclass_flag
    );

    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *          Register GClass
 ***************************************************************************/
function register_c_g6_nodes_tree()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_g6_nodes_tree };
