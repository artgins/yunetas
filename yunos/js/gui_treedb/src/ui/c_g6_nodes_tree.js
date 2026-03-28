/***********************************************************************
 *          c_g6_nodes_tree.js
 *
 *          Treedb's Nodes Tree Manager using AntV/G6
 *
 *          Each G6 Node contains in his data:
 *          {
 *              topic_name: "...",
 *              desc: desc,         // Desc of topic
 *              record: null,       // Data of node
 *          }
 *
 *          Copyright (c) 2020-2021 Niyamaka.
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    __yuno__,
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
    treedb_encoder_fkey,
    kwid_find_one_record,
    str_in_list,
    delete_from_list,
    escapeHtml,
    safeSrc,
} from "yunetas";

import {
    addClasses,
    removeClasses,
    toggleClasses,
    removeChildElements,
    disableElements,
    enableElements,
    set_submit_state,
    set_cancel_state,
    set_active_state,
    getStrokeColor,
} from "./lib_graph.js";

import {
    BaseLayout,
    ExtensionCategory,
    Graph,
    NodeEvent,
    CanvasEvent,
    HistoryEvent,
    EdgeEvent,
    Circle,
    Toolbar,
    register,
} from '@antv/g6';

import {Circle as CircleGeometry} from '@antv/g';
import {t} from "i18next";

/***************************************************************
 *  YuiToolbar — G6 Toolbar subclass that adds per-item className
 *  and disabled support.
 *
 *  Each item in getItems() may carry:
 *    className  — CSS class added to the div (e.g. 'EV_SAVE_GRAPH')
 *    disabled   — boolean; sets the 'disabled' attribute initially
 *
 *  With className set to the event name, the lib_graph.js state
 *  functions (set_submit_state, disableElements, …) work on toolbar
 *  icons exactly like on regular Bulma/FA buttons — no special
 *  icon-specific helpers needed.
 ***************************************************************/
class YuiToolbar extends Toolbar
{
    async getDOMContent()
    {
        const items = await this.options.getItems();
        return items.map((item) => {
            const extra_class = item.className ? ` ${item.className}` : '';
            const disabled    = item.disabled  ? ' disabled'          : '';
            return (
                `<div class="g6-toolbar-item${extra_class}"` +
                ` value="${item.value}" title="${item.title ?? ''}"${disabled}>` +
                `<svg aria-hidden="true" focusable="false">` +
                `<use xlink:href="#${item.id}"></use>` +
                `</svg>` +
                `</div>`
            );
        }).join('');
    }
}
register(ExtensionCategory.PLUGIN, 'yui-toolbar', YuiToolbar);

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_G6_NODES_TREE";

/***************************************************************
 *  Internal layout and operation mode definitions
 ***************************************************************/
const _layouts = {
    // set manual the first
    "manual": {
        type: 'manual',
    },
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
};

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
SDATA(data_type_t.DTP_STRING,   "operation_mode",       0,  '["reading", "operation", "writing", "edition"]', // WARNING put only the implemented modes
"Current operation mode. Interface to fulfill requirements of the parent."),
SDATA(data_type_t.DTP_STRING,   "layout",               0,  "", "Current graph layout"),

/*---------------- Remote Connection ----------------*/
SDATA(data_type_t.DTP_POINTER,  "gobj_remote_yuno",     0,  null,   "Remote Yuno"),
SDATA(data_type_t.DTP_STRING,   "treedb_name",          0,  null,   "Treedb name"),
SDATA(data_type_t.DTP_DICT,     "descs",                0,  null,   "Descriptions of topics"),
SDATA(data_type_t.DTP_DICT,     "records",              0,  "{}",   "Data of topics"),
SDATA(data_type_t.DTP_LIST,     "topics",               0,  "[]",   "List of topic objects"),

/*---------------- Sub-container ----------------*/
SDATA(data_type_t.DTP_POINTER,  "$container",           0,  null,   "Graph container element, set externally"),

/*---------------- Graph Settings ----------------*/
SDATA(data_type_t.DTP_STRING,   "theme",                0,  "light", "Theme: light or dark"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_treedb_tables",   0,  false,  "Include treedb tables"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_gridline",        0,  true,   "Use gridline plugin"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_fullscreen",      0,  true,   "Use fullscreen plugin"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_toolbar",         0,  true,   "Use toolbar plugin"),
SDATA(data_type_t.DTP_STRING,   "toolbar_position",     0,  "right-top",
    "Toolbar position: top-left, top-right, bottom-left, bottom-right, left-top, right-top"),
SDATA(data_type_t.DTP_LIST,     "layout_names",         sdata_flag_t.SDF_RD,
    JSON.stringify(Object.keys(_layouts)),
    "Available layout names (read-only, for parent to query)"),

SDATA(data_type_t.DTP_STRING,   "hook_port_position",   0,  "bottom",   "Hook port position"),
SDATA(data_type_t.DTP_STRING,   "fkey_port_position",   0,  "top",      "Fkey port position"),

SDATA(data_type_t.DTP_STRING,   "wide",                 0,  "40px", "Height of header"),
SDATA(data_type_t.DTP_STRING,   "padding",              0,  "m-2",  "Padding or margin value"),

SDATA_END()
];

let PRIVATE_DATA = {
    _xy:                100,
    _edge_seq:          0,
    _history_paused:    false,
    treedb_name:        "",
    gobj_remote_yuno:   null,
    descs:              null,
    records:            {},
    $container:         null,
    graph:              null,       // Instance of G6
    __graphs__:         [],         // Rows of __graphs__
    _graph_properties:  {},         // topic_name → {nodes: {node_id: {x,y,size,...}}}
    yet_showed:         false,
    edit_mode:          false,
    operation_mode:     null,
    layout:             null,
    theme:              null,
    _selected_node_id:  null,
    _selected_port_key: null,       // key of selected port (null = node selected)
    _resize_handles_el: null,
    _resize_handles:    [],
    _resize_sel_rect:   null,
    _port_handles_el:   null,
    _port_handles:      [],
    _port_ring:         null,
    _selected_edge_id:  null,       // selected edge id
    _edge_icon_el:      null,       // floating properties icon element
    _edge_delete_el:    null,       // floating delete icon element for edge
    _edge_popover_el:   null,       // edge properties popover element
    _node_icon_el:      null,       // floating node properties icon element
    _node_delete_el:    null,       // floating delete icon element for node
    _node_popover_el:   null,       // node properties popover element
    _context_node_id:   null,       // node id for context menu target
    _context_port_key:  null,       // port key for context menu target (null = node body)
    _context_edge_id:   null,       // edge id for context menu target
    _linking_mode:      false,      // true when in link-drag mode
    _link_source:       null,       // {node_id, port_key, col, topic_name} of fkey being linked
    _link_icon_el:      null,       // floating link icon on fkey port
    _link_drag_svg:     null,       // SVG overlay for drag line
    _link_valid_hooks:  [],         // [{node_id, port_key}] compatible hook targets
    _link_saved_styles: [],         // saved port styles to restore on cancel
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
        gobj_subscribe_event(__yui_main__, "EV_THEME", {}, gobj);
        gobj_write_attr(gobj, "theme", gobj_read_str_attr(__yui_main__, "theme"));
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
 *  Build edge id: independent, stable, auto-incremented.
 *
 *  The id is decoupled from the treedb relationship so that
 *  an edge can exist in intermediate states (dangling,
 *  half-connected) during design-mode interactions.
 *  The treedb semantics live in edge.data (see create_edge_data).
 ************************************************************/
function build_edge_id(gobj)
{
    let priv = gobj.priv;
    priv._edge_seq++;
    return sprintf("edge-%s-%d", priv.treedb_name, priv._edge_seq);
}

/************************************************************
 *  Create edge data structure — the semantic treedb relationship.
 *
 *  Lifecycle states:
 *
 *  DANGLING (just created, not connected to any port):
 *    all fields null
 *
 *  HALF-CONNECTED (one port attached):
 *    parent side OR child side filled in
 *
 *  FULLY-CONNECTED (both ports attached):
 *    all fields filled — ready for treedb_link_nodes
 ************************************************************/
function create_edge_data()
{
    return {
        hook_name:    null,
        fkey_name:    null,
        parent_topic: null,
        parent_id:    null,
        child_topic:  null,
        child_id:     null,
    };
}

/************************************************************
 *  Edge connection state queries
 ************************************************************/
function edge_is_fully_connected(edge_data)
{
    return !!(edge_data.hook_name && edge_data.fkey_name &&
              edge_data.parent_id && edge_data.child_id);
}

function edge_is_half_connected(edge_data)
{
    let has_parent = !!(edge_data.hook_name && edge_data.parent_id);
    let has_child  = !!(edge_data.fkey_name && edge_data.child_id);
    return (has_parent || has_child) && !(has_parent && has_child);
}

function edge_is_dangling(edge_data)
{
    return (!edge_data.hook_name && !edge_data.fkey_name &&
            !edge_data.parent_id && !edge_data.child_id);
}

/************************************************************
 *  Find all edges connected to a hook port on a parent node
 ************************************************************/
function get_edges_by_hook(gobj, parent_topic, parent_id, hook_name)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    let edges = graph.getData().edges || [];
    return edges.filter(e =>
        e.data &&
        e.data.parent_topic === parent_topic &&
        e.data.parent_id    === parent_id &&
        e.data.hook_name    === hook_name
    );
}

/************************************************************
 *  Find all edges connected to a fkey port on a child node
 ************************************************************/
function get_edges_by_fkey(gobj, child_topic, child_id, fkey_name)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    let edges = graph.getData().edges || [];
    return edges.filter(e =>
        e.data &&
        e.data.child_topic === child_topic &&
        e.data.child_id    === child_id &&
        e.data.fkey_name   === fkey_name
    );
}

/************************************************************
 *  Find the exact edge for a specific fully-connected
 *  treedb link (parent hook <-> child fkey).
 ************************************************************/
function get_edge_by_link(gobj, parent_topic, parent_id, hook_name,
                                child_topic, child_id, fkey_name)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    let edges = graph.getData().edges || [];
    return edges.find(e =>
        e.data &&
        e.data.parent_topic === parent_topic &&
        e.data.parent_id    === parent_id &&
        e.data.hook_name    === hook_name &&
        e.data.child_topic  === child_topic &&
        e.data.child_id     === child_id &&
        e.data.fkey_name    === fkey_name
    ) || null;
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
 *  Register custom G6 layouts
 ************************************************************/
let _g6_extensions_registered = false;

function register_layouts(gobj)
{
    if(!_g6_extensions_registered) {
        _g6_extensions_registered = true;
        register(ExtensionCategory.LAYOUT, 'manual', ManualLayout);
        register(ExtensionCategory.NODE, 'light', LightNode);
    }
}

/************************************************************
 *  Build a G6 graph instance
 ************************************************************/
function build_graph(gobj)
{
    let priv = gobj.priv;

    let layout = select_layout(gobj, priv.layout);

    const graph = priv.graph = new Graph({
        x: 0,
        y: 0,
        container: priv.$container,
        animation: false,
        autoResize: false,
        zoomRange: [0.2, 4],
        node: {  // WARNING this affect to all nodes with prevalence over individual defines!
            palette: {
                type: 'group',
                color: 'tableau',
                field: 'topic_name',
            },
            state: {
                selected: {
                    lineWidth: 2,
                    stroke: '#1890ff',
                    labelFill: '#000',          // Force black; remove to let G6 dark theme control it
                    labelFontWeight: 'normal',
                },
            },
        },

        edge: { // WARNING this affect to all edges with prevalence over individual defines!
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

    graph.setLayout(layout);
    //show_positions(gobj);

    graph_render(gobj).then(() => {
        configure_events(gobj);
        configure_behaviour(gobj);
        configure_plugins(gobj);
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

    graph.on(NodeEvent.DRAG, (evt) => {
        update_resize_handles_position(gobj);
        update_port_resize_handles_position(gobj);
        update_edge_icon_position(gobj);
        update_node_icon_position(gobj);
        update_link_icon_position(gobj);
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

    graph.on('aftertransform', () => {
        update_resize_handles_position(gobj);
        update_port_resize_handles_position(gobj);
        update_edge_icon_position(gobj);
        update_node_icon_position(gobj);
        update_link_icon_position(gobj);
    });

    if(gobj_read_bool_attr(gobj, "with_fullscreen")) {
        graph.on("keydown", (evt) => {
            const key = evt.key;
            const fullscreen = graph_get_plugin(gobj, "fullscreen");
            if(!fullscreen) {
                return;
            }
            if(key === "F" || key === "f") {
                fullscreen.request();
            } else if(key === "Escape") {
                fullscreen.exit();
            }
        });
    }
}

/************************************************************
 *  Plugin configuration
 ************************************************************/
function configure_plugins(gobj)
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

    if(gobj_read_bool_attr(gobj, "with_fullscreen")) {
        graph_add_plugin(
            gobj,
            'fullscreen',
            {
                autoFit: true,
            }
        );
    }

    graph_add_plugin(
        gobj,
        'contextmenu',
        {
            trigger: 'contextmenu',
            onClick: (value) => {
                handle_context_menu_click(gobj, value);
            },
            getItems: (e) => {
                return build_context_menu_items(gobj, e);
            },
            enable: (e) => {
                return e.targetType === 'node' || e.targetType === 'edge';
            },
        }
    );

    if(gobj_read_bool_attr(gobj, "with_toolbar")) {
        configure_toolbar(gobj);
        configure_toolbar_edit(gobj);
    }
}

/************************************************************
 *  Configure G6 Toolbar plugin
 *  Uses G6 built-in icons: zoom-in, zoom-out, redo, undo,
 *  edit, delete, auto-fit, export, reset
 ************************************************************/
function update_toolbar(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    if(!gobj_read_bool_attr(gobj, "with_toolbar")) {
        return;
    }

    let toolbar = graph_get_plugin(gobj, 'toolbar');
    if(toolbar) {
        /*
         *  Force toolbar to re-render by updating the plugin.
         */
        graph.updatePlugin({
            key: 'toolbar',
        });
    }

    // Add or remove edit toolbar based on edit mode
    configure_toolbar_edit(gobj);

    let toolbar_edit = graph_get_plugin(gobj, 'toolbar-edit');
    if(toolbar_edit) {
        graph.updatePlugin({
            key: 'toolbar-edit',
        });
    }
}

function configure_toolbar(gobj)
{
    let priv = gobj.priv;
    let toolbar_position = gobj_read_str_attr(gobj, "toolbar_position") || "top-left";

    graph_add_plugin(
        gobj,
        'toolbar',
        {
            type: 'yui-toolbar',
            className: 'g6-toolbar-large',
            position: toolbar_position,
            style: {
                backgroundColor: '#f5f5f5',
                padding: '8px',
                boxShadow: '0 2px 8px rgba(0, 0, 0, 0.15)',
                borderRadius: '8px',
                border: '1px solid #e8e8e8',
                opacity: '0.85',
                marginTop: '12px',
                marginLeft: '12px',
            },
            getItems: () => {
                let items = [
                    { id: 'zoom-in',  value: 'zoom-in',  className: 'EV_ZOOM_IN',    title: 'Zoom In'    },
                    { id: 'zoom-out', value: 'zoom-out', className: 'EV_ZOOM_OUT',   title: 'Zoom Out'   },
                    { id: 'reset',    value: 'reset',    className: 'EV_ZOOM_RESET', title: 'Reset Zoom' },
                    { id: 'auto-fit', value: 'auto-fit', className: 'EV_AUTO_FIT',   title: 'Auto Fit'   },
                ];

                if(gobj_read_bool_attr(gobj, "with_fullscreen")) {
                    items.push(
                        { id: 'request-fullscreen', value: 'request-fullscreen', className: 'EV_REQUEST_FULLSCREEN', title: 'Enter Full Screen' },
                        { id: 'exit-fullscreen',    value: 'exit-fullscreen',    className: 'EV_EXIT_FULLSCREEN',    title: 'Exit Full Screen'  },
                    );
                }

                return items;
            },
            onClick: (value) => {
                switch(value) {
                    case 'zoom-in':
                        gobj_send_event(gobj, "EV_ZOOM_IN", {}, gobj);
                        break;
                    case 'zoom-out':
                        gobj_send_event(gobj, "EV_ZOOM_OUT", {}, gobj);
                        break;
                    case 'reset':
                        gobj_send_event(gobj, "EV_ZOOM_RESET", {}, gobj);
                        break;
                    case 'auto-fit':
                        gobj_send_event(gobj, "EV_AUTO_FIT", {}, gobj);
                        break;
                    case 'center':
                        gobj_send_event(gobj, "EV_CENTER", {}, gobj);
                        break;
                    case 'request-fullscreen':
                        gobj_send_event(gobj, "EV_REQUEST_FULLSCREEN", {}, gobj);
                        break;
                    case 'exit-fullscreen':
                        gobj_send_event(gobj, "EV_EXIT_FULLSCREEN", {}, gobj);
                        break;
                }
            },
        }
    );
}

/************************************************************
 *  Configure G6 Edit Toolbar plugin (horizontal, top-right)
 *  Contains edit-mode buttons: Undo, Redo, Save
 ************************************************************/
function configure_toolbar_edit(gobj)
{
    let priv = gobj.priv;

    if(!priv.edit_mode) {
        // Remove edit toolbar when not in edit mode
        graph_remove_plugin(gobj, 'toolbar-edit');
        return;
    }

    // Already exists, just update it
    if(graph_get_plugin(gobj, 'toolbar-edit')) {
        return;
    }

    graph_add_plugin(
        gobj,
        'toolbar-edit',
        {
            type: 'yui-toolbar',
            className: 'g6-toolbar-large',
            position: 'left-top',
            style: {
                backgroundColor: '#f5f5f5',
                padding: '8px',
                boxShadow: '0 2px 8px rgba(0, 0, 0, 0.15)',
                borderRadius: '8px',
                border: '1px solid #e8e8e8',
                opacity: '0.85',
            },
            getItems: () => {
                return [
                    { id: 'g6-icon-save', value: 'save', className: 'EV_SAVE_GRAPH',   title: 'Save', disabled: true },
                    { id: 'undo',         value: 'undo', className: 'EV_HISTORY_UNDO', title: 'Undo', disabled: true },
                    { id: 'redo',         value: 'redo', className: 'EV_HISTORY_REDO', title: 'Redo', disabled: true },
                ];
            },
            onClick: (value) => {
                switch(value) {
                    case 'undo':
                        gobj_send_event(gobj, "EV_HISTORY_UNDO", {}, gobj);
                        break;
                    case 'redo':
                        gobj_send_event(gobj, "EV_HISTORY_REDO", {}, gobj);
                        break;
                    case 'save':
                        gobj_send_event(gobj, "EV_SAVE_GRAPH", {}, gobj);
                        break;
                }
            },
        }
    );
}

/************************************************************
 *
 ************************************************************/
function show_positions(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    window.console.log(`${gobj_short_name(gobj)}: show, size ${graph.getSize()}`);
    window.console.log(`${gobj_short_name(gobj)}: show, canvas center ${graph.getCanvasCenter()}`);
    window.console.log(`${gobj_short_name(gobj)}: show, position ${graph.getPosition()}`);
    window.console.log(`${gobj_short_name(gobj)}: show, viewport center ${graph.getViewportCenter()}`);
    window.console.log(`${gobj_short_name(gobj)}: show, zoom ${graph.getZoom()}`);
    window.console.log(`${gobj_short_name(gobj)}: show, rotation ${graph.getRotation()}`);
}

/************************************************************
 *  Layout selection
 ************************************************************/
function select_layout(gobj, layout_name)
{
    let priv = gobj.priv;

    if(!layout_name) {
        layout_name = priv.layout;
    }

    let layouts = Object.keys(_layouts);

    if(!layout_name || !str_in_list(layouts, layout_name)) {
        layout_name = layouts[0];
    }
    gobj_write_attr(gobj, "layout", layout_name);

    return _layouts[layout_name];
}

/************************************************************
 *  Set behavior of operation mode:
 *  reading, operation, writing, edition
 ************************************************************/
function configure_behaviour(gobj)
{
    let priv = gobj.priv;
    let operation_mode = priv.operation_mode;

    /*
     *  Behaviors
     */
    let behaviors = [];
    switch(operation_mode) {
        case "writing":
        case "reading":
            priv.edit_mode = false;
            behaviors = [
                "drag-canvas",
                "zoom-canvas",
            ];
            break;
        case "edition":
            priv.edit_mode = true;
            behaviors = [
                "drag-canvas",
                "zoom-canvas",
                "drag-element",
            ];
            break;
        case "operation":
            priv.edit_mode = false;
            break;
        default:
            log_error(`operation mode unknown: ${operation_mode}`);
            break;
    }
    if(!priv.edit_mode) {
        deselect_node(gobj);
    }
    graph_write_behaviors(gobj, behaviors);
}

/************************************************************
 * Returns the proportional position (between 0 and 1) of a specific point,
 * centered and spaced with margins.
 *
 * index - Index of the point (0 to count-1)
 * count - Total number of points
 * margin - Total margin space (default 0.2 means 10% on each end)
 ************************************************************/
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
 *  Count hooks and fkeys in topic desc, classify node type
 ************************************************************/
function calculate_hooks_fkeys_counter(desc)
{
    let cols = desc.cols;
    desc.hooks_counter = 0;
    desc.fkeys_counter = 0;

    for(let i=0; i<cols.length; i++) {
        let col = cols[i];
        const field_desc = treedb_get_field_desc(col);
        switch(field_desc.type) {
            case "hook":
                desc.hooks_counter++;
                break;
            case "fkey":
                desc.fkeys_counter++;
                break;
        }
    }

    if(desc.hooks_counter === 0) {
        desc.node_treedb_type = 'child';
    } else if(desc.fkeys_counter === 0) {
        desc.node_treedb_type = 'extended';
    } else {
        desc.node_treedb_type = 'hierarchical';
    }
}

/************************************************************
 *  Build ports for a topic desc
 ************************************************************/
function build_ports(gobj, desc)
{
    let priv = gobj.priv;

    let top_ports = [];
    let bottom_ports = [];

    let cols = desc.cols;
    for(let i=0; i<cols.length; i++) {
        let col = cols[i];
        const field_desc = treedb_get_field_desc(col);
        let port = null;
        switch(field_desc.type) {
            case "hook":
                {
                    let child_desc = null;
                    let child_topic_name = Object.keys(col.hook)[0];
                    if(child_topic_name) {
                        child_desc = priv.descs[child_topic_name];
                    }
                    port = {
                        key: col.id,
                        fill: child_desc?child_desc.color:desc.color,
                        stroke: getStrokeColor(desc.color),
                    };
                    bottom_ports.push(port);
                }
                break;
            case "fkey":
                port = {
                    key: col.id,
                    fill: desc.color,
                    stroke: getStrokeColor(desc.color),
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
function create_topic_node(gobj, desc, record)
{
    let priv = gobj.priv;
    const graph = priv.graph;

    /*------------------------------------------*
     *  Creating filled cell from backend data
     *------------------------------------------*/
    let node_name = build_node_name(gobj, desc.topic_name, record.id);
    let xy = get_default_ne_xy(gobj);
    let geometry = get_node_graph_props(gobj, desc.topic_name, record.id, record);
    let x = kw_get_int(gobj, geometry, "x", xy, kw_flag_t.KW_CREATE);
    let y = kw_get_int(gobj, geometry, "y", xy, kw_flag_t.KW_CREATE);

    //log_warning(`create node ==> ${node_name}`);

    let ports = build_ports(gobj, desc);

    let node_graph_type = null;
    let node_treedb_type = desc.node_treedb_type;

    let style = {
        x: x,
        y: y,
        fill: desc.color,     // Fill color
        stroke: getStrokeColor(desc.color),   // Stroke color
        lineWidth: 1,           // Stroke width
        //labelText: desc.topic_name + "^" + record.id,
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
            labelFill: '#000',          // Force black; remove to let G6 dark theme control it
            labelFontWeight: 'normal',
            // iconText: record.id,
            // iconSrc: record.icon,
            // iconFontSize: 14,
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
    background: ${desc.color};
    border: 1px solid ${getStrokeColor(desc.color)};
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

    // Apply topic defaults (from "resize all") for nodes without saved geometry
    let topic_props = priv._graph_properties[desc.topic_name];
    let topic_defaults = (is_object(topic_props) && is_object(topic_props.defaults))?
        topic_props.defaults : null;
    if(topic_defaults) {
        let def_size = topic_defaults.size;
        if(!geometry.size && Array.isArray(def_size) && def_size.length > 0) {
            style.size = [...def_size];
            if(node_graph_type === 'html') {
                style.dx = -def_size[0] / 2;
                style.dy = -(def_size.length > 1 ? def_size[1] : def_size[0]) / 2;
            }
        }
        if(!geometry.portR && topic_defaults.portR > 0) {
            style.portR = topic_defaults.portR;
        }
    }

    // Override size and portR with saved per-node geometry
    let saved_size = geometry.size;
    if(Array.isArray(saved_size) && saved_size.length > 0) {
        style.size = saved_size;
        // Recalculate dx/dy for HTML nodes to keep content centered
        if(node_graph_type === 'html') {
            style.dx = -saved_size[0] / 2;
            style.dy = -(saved_size.length > 1 ? saved_size[1] : saved_size[0]) / 2;
        }
    }
    if(geometry.portR > 0) {
        style.portR = geometry.portR;
    }

    let node_def = {
        id: node_name,
        type: node_graph_type,
        style: style,
        data: {
            // This 4 keys are available in user `data` of G6 Node.
            topic_name: desc.topic_name,
            desc: desc,
            record: record,
            graph_props: geometry  // from __graphs__ (or _geometry fallback)
        }
    };

    if(json_size(ports)) {
        json_object_update_missing(style, {
            port: true,
            ports: ports,
            portR: node_treedb_type === 'child' ? 2 : 6,
            portLineWidth: 1,
        });

        // Restore per-port radius: saved geometry first, then topic defaults
        let port_sizes = geometry.port_sizes;
        let default_port_sizes = topic_defaults ? topic_defaults.port_sizes : null;
        if(is_object(port_sizes) || is_object(default_port_sizes)) {
            for(let i = 0; i < style.ports.length; i++) {
                let key = style.ports[i].key;
                let r = is_object(port_sizes) ? port_sizes[key] : null;
                if(r == null && is_object(default_port_sizes)) {
                    r = default_port_sizes[key];
                }
                if(r != null) {
                    style.ports[i].r = r;
                }
            }
        }
    }

    if(node_graph_type) {
        graph.addNodeData([node_def]);
    }
}

/************************************************************
 *  Update topic node data
 ************************************************************/
function update_topic_node(gobj, desc, node_name, record)
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
        if(desc.node_treedb_type === 'child') {
            graph.updateNodeData([{
                id: node_name,
                style: {
                    labelText: record.id,
                }
            }]);
        } else if(desc.node_treedb_type === 'hierarchical') {
            graph.updateNodeData([{
                id: node_name,
                style: {
                    innerHTML: `
<div style="
    width: 100%;
    height: 100%;
    background: ${desc.color};
    border: 1px solid ${getStrokeColor(desc.color)};
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
 *  Create all links from records, initial load from parent
 ************************************************************/
function create_links(gobj)
{
    let priv = gobj.priv;

    for(const [topic_name, records] of Object.entries(priv.records)) {
        let desc = priv.descs[topic_name];
        for(let i = 0; i < records.length; i++) {
            let record = records[i];
            draw_links(gobj, desc, record, true);
        }
    }
}

/************************************************************
 *  Collect all fkey references from a record as a Set.
 *
 *  Each entry is a string: "col_id\tcol_idx\tfkey_value"
 *  where col_idx is the index into desc.cols for recovering
 *  the col object, and fkey_value is the raw fkey string
 *  (e.g. "departments^direction^departments" or just "admin").
 *
 *  This allows diffing old vs new records to find added/removed
 *  links without destroying and recreating all edges.
 ************************************************************/
function collect_fkey_refs(desc, record)
{
    let refs = new Set();
    let cols = desc.cols;
    for(let i=0; i<cols.length; i++) {
        let col = cols[i];
        if(!col.fkey) {
            continue;
        }
        let fkeys = record[col.id];
        if(fkeys) {
            if(is_array(fkeys)) {
                for(let j=0; j<fkeys.length; j++) {
                    let key = treedb_encoder_fkey(col, fkeys[j]);
                    if(key) {
                        refs.add(i + "\t" + key);
                    }
                }
            } else {
                // string or object — normalize to canonical form
                let key = treedb_encoder_fkey(col, fkeys);
                if(key) {
                    refs.add(i + "\t" + key);
                }
            }
        }
    }
    return refs;
}

/************************************************************
 *  Draw links for a record based on its fkey fields
 ************************************************************/
function draw_links(gobj, desc, record, initial_load)
{
    let cols = desc.cols;
    let topic_name = desc.topic_name;
    let record_id = record.id;

    for(let i=0; i<cols.length; i++) {
        let col = cols[i];
        if(!col.fkey) {
            continue;
        }

        let fkeys = record[col.id];

        if(fkeys) {
            if(is_array(fkeys)) {
                for(let j=0; j<fkeys.length; j++) {
                    draw_link(gobj, topic_name, record_id, col, fkeys[j], initial_load);
                }
            } else {
                // string or object
                draw_link(gobj, topic_name, record_id, col, fkeys, initial_load);
            }
        }
    }
}

/************************************************************
 *  Draw a single link (edge) between two nodes.
 *
 *  The edge gets an independent id (build_edge_id) and
 *  carries the full treedb relationship in its data section.
 ************************************************************/
function draw_link(
    gobj,
    child_topic,
    child_id,
    source_col,
    fkey,
    verbose
)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    /*
     *  Decode fkey: the link to the parent
     */
    let target_fkey = treedb_decoder_fkey(source_col, fkey);
    if(!target_fkey) {
        log_error("draw_link: cannot decode fkey");
        return;
    }
    let parent_topic = target_fkey.topic_name;
    let parent_desc = priv.descs[parent_topic];
    if(parent_desc && parent_desc.node_treedb_type === 'extended') {
        return;
    }
    let parent_id  = target_fkey.id;
    let hook_name  = target_fkey.hook_name;
    let fkey_name  = source_col.id;

    let parent_node = build_node_name(gobj, parent_topic, parent_id);

    /*
     *  Parent node must exist
     */
    let parent_cell;
    try {
        parent_cell = graph.getNodeData(parent_node);
    } catch(e) {
        if(verbose) {
            log_error(e.message);
        }
    }
    if(!parent_cell) {
        if(verbose) {
            log_error(`${gobj_short_name(gobj)}: parent node NOT FOUND: ${parent_node}`);
        }
        return;
    }

    /*
     *  Child node (me)
     */
    let child_node = build_node_name(gobj, child_topic, child_id);
    let style = graph.getElementRenderStyle(child_node);

    /*
     *  Create the edge with independent id and semantic data
     *  HACK: source/target are interchanged so arrows point parent -> child
     */
    // Restore saved edge style from __graphs__
    let saved_lineWidth = 2;
    let saved_stroke = style.fill;
    let topic_props = priv._graph_properties[parent_topic];
    if(topic_props) {
        // Per-edge saved style
        if(is_object(topic_props.edges)) {
            let edge_key = hook_name + ":" + parent_id + ":" + child_id;
            let edge_props = topic_props.edges[edge_key];
            if(edge_props) {
                if(edge_props.lineWidth != null) {
                    saved_lineWidth = edge_props.lineWidth;
                }
                if(edge_props.stroke) {
                    saved_stroke = edge_props.stroke;
                }
            }
        }
        // Fall back to topic defaults
        if(saved_lineWidth === 2 && is_object(topic_props.defaults)) {
            let defs = topic_props.defaults;
            if(defs.edge_styles && defs.edge_styles[hook_name]) {
                let hook_style = defs.edge_styles[hook_name];
                if(hook_style.lineWidth != null) {
                    saved_lineWidth = hook_style.lineWidth;
                }
                if(hook_style.stroke) {
                    saved_stroke = hook_style.stroke;
                }
            } else if(defs.edge_style) {
                if(defs.edge_style.lineWidth != null) {
                    saved_lineWidth = defs.edge_style.lineWidth;
                }
                if(defs.edge_style.stroke) {
                    saved_stroke = defs.edge_style.stroke;
                }
            }
        }
    }

    let edge = {
        id: build_edge_id(gobj),
        type: 'cubic',
        source: parent_node,
        target: child_node,
        style: {
            sourcePort: hook_name,
            targetPort: fkey_name,
            lineWidth: saved_lineWidth,
            stroke: saved_stroke,
        },
        data: {
            parent_topic: parent_topic,
            parent_id:    parent_id,
            hook_name:    hook_name,
            child_topic:  child_topic,
            child_id:     child_id,
            fkey_name:    fkey_name,
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
function clear_links(gobj, desc, record, verbose)
{
    let cols = desc.cols;
    let topic_name = desc.topic_name;
    let record_id = record.id;

    for(let i=0; i<cols.length; i++) {
        let col = cols[i];
        if(!col.fkey) {
            continue;
        }

        let fkeys = record[col.id];

        if(fkeys) {
            if(is_array(fkeys)) {
                for(let j=0; j<fkeys.length; j++) {
                    clear_link(gobj, topic_name, record_id, col, fkeys[j], verbose);
                }
            } else {
                // string or object
                clear_link(gobj, topic_name, record_id, col, fkeys, verbose);
            }
        }
    }
}

/************************************************************
 *  Clear a single link (edge).
 *  Finds the edge by its semantic data, removes by its id.
 ************************************************************/
function clear_link(
    gobj,
    child_topic,
    child_id,
    source_col,
    fkey,
    verbose
)
{
    let target_fkey = treedb_decoder_fkey(source_col, fkey);
    if(!target_fkey) {
        log_error("clear_link: cannot decode fkey");
        return;
    }

    let edge = get_edge_by_link(gobj,
        target_fkey.topic_name, target_fkey.id, target_fkey.hook_name,
        child_topic, child_id, source_col.id
    );

    if(edge) {
        let priv = gobj.priv;
        let graph = priv.graph;
        try {
            graph.removeEdgeData([edge.id]);
        } catch(e) {
            if(verbose) {
                log_error(e.message);
            }
        }
    } else if(verbose) {
        log_error(`${gobj_short_name(gobj)}: clear_link: edge not found for ` +
            `${target_fkey.topic_name}^${target_fkey.id}^${target_fkey.hook_name} -> ` +
            `${child_topic}^${child_id}^${source_col.id}`
        );
    }
}

/************************************************************
 *  Build _graph_properties from __graphs__ records.
 *  Indexes active __graphs__ records by topic_name for
 *  fast per-node geometry/style lookups.
 ************************************************************/
function build_graph_properties(gobj)
{
    let priv = gobj.priv;
    priv._graph_properties = {};

    for(let i = 0; i < priv.__graphs__.length; i++) {
        let rec = priv.__graphs__[i];
        if(!rec.active || !rec.topic) {
            continue;
        }
        let props = rec.properties;
        if(is_object(props)) {
            priv._graph_properties[rec.topic] = props;
        }
    }
}

/************************************************************
 *  Get graph properties for a specific node.
 *  Returns the node's visual properties from __graphs__,
 *  falling back to record._geometry for backward compat.
 ************************************************************/
function get_node_graph_props(gobj, topic_name, node_id, record)
{
    let priv = gobj.priv;

    // Primary source: __graphs__ properties
    let topic_props = priv._graph_properties[topic_name];
    if(topic_props && is_object(topic_props.nodes)) {
        let node_props = topic_props.nodes[node_id];
        if(is_object(node_props)) {
            return node_props;
        }
    }

    // Fallback: legacy _geometry on the record
    if(is_object(record._geometry)) {
        return record._geometry;
    }

    return {};
}

/************************************************************
 *  Update geometry of a single node into _graph_properties.
 *  Collects x, y, size from the current render style and
 *  stores them in the in-memory _graph_properties structure.
 *  Also updates the node data's graph_props reference.
 ************************************************************/
function update_geometry(gobj, node_id)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    let nodedata = graph.getNodeData(node_id);
    let style = graph.getElementRenderStyle(node_id);
    let topic_name = nodedata.data.topic_name;
    let record = nodedata.data.record;

    // Ensure topic entry exists in _graph_properties
    if(!is_object(priv._graph_properties[topic_name])) {
        priv._graph_properties[topic_name] = {};
    }
    let topic_props = priv._graph_properties[topic_name];
    if(!is_object(topic_props.nodes)) {
        topic_props.nodes = {};
    }

    // Extract geometry from render style
    let node_props = topic_props.nodes[record.id] || {};
    json_object_update(
        node_props,
        kw_clone_by_keys(gobj, style, ["x", "y", "size", "portR"])
    );

    // Save per-port radius values (only ports with custom r)
    let node_style = nodedata.style || {};
    let ports = node_style.ports || [];
    let port_sizes = {};
    for(let i = 0; i < ports.length; i++) {
        if(ports[i].r != null) {
            port_sizes[ports[i].key] = ports[i].r;
        }
    }
    if(Object.keys(port_sizes).length > 0) {
        node_props.port_sizes = port_sizes;
    } else {
        delete node_props.port_sizes;
    }

    topic_props.nodes[record.id] = node_props;

    // Keep node data's graph_props in sync
    nodedata.data.graph_props = node_props;
}

/************************************************************
 *  Save all node geometries to __graphs__ topic.
 *  One EV_UPDATE_NODE per topic instead of one per node.
 ************************************************************/
function save_geometry(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    // Collect geometry for all nodes into _graph_properties
    const nodes = graph.getData().nodes;
    for(let i = 0; i < nodes.length; i++) {
        update_geometry(gobj, nodes[i].id);
    }

    // Collect edge styles into _graph_properties
    const edges = graph.getData().edges;
    for(let i = 0; i < edges.length; i++) {
        update_edge_geometry(gobj, edges[i].id);
    }

    // Save one __graphs__ record per topic
    let origin = gobj_read_str_attr(__yuno__, "node_uuid");

    for(const [topic_name, properties] of Object.entries(priv._graph_properties)) {
        // Add origin metadata
        properties.__origin__ = origin;

        let kw_update = {
            treedb_name: priv.treedb_name,
            topic_name: "__graphs__",
            record: {
                id: topic_name,
                topic: topic_name,
                active: true,
                properties: properties
            },
            options: {
                list_dict: true,
                autolink: false,
                create: true    // Create if doesn't exist, update if it does
            }
        };
        gobj_publish_event(gobj, "EV_UPDATE_NODE", kw_update);
    }
}

/************************************************************
 *  Save __graphs__ properties for a single topic to backend.
 ************************************************************/
function save_topic_graph_properties(gobj, topic_name)
{
    let priv = gobj.priv;
    let properties = priv._graph_properties[topic_name];
    if(!properties) {
        return;
    }

    let origin = gobj_read_str_attr(__yuno__, "node_uuid");
    properties.__origin__ = origin;

    let kw_update = {
        treedb_name: priv.treedb_name,
        topic_name: "__graphs__",
        record: {
            id: topic_name,
            topic: topic_name,
            active: true,
            properties: properties
        },
        options: {
            list_dict: true,
            autolink: false,
            create: true
        }
    };
    gobj_publish_event(gobj, "EV_UPDATE_NODE", kw_update);
}

/************************************************************
 *  Pause/resume history recording.
 *
 *  Backend-driven updates (node created/updated/deleted) must
 *  not be recorded as undoable user actions.
 *
 *  Uses a flag checked by the history plugin's beforeAddCommand
 *  callback — when paused, beforeAddCommand returns false and
 *  the command is not added to the undo queue.
 ************************************************************/
function history_pause(gobj)
{
    gobj.priv._history_paused = true;
}

function history_resume(gobj)
{
    gobj.priv._history_paused = false;
}

/************************************************************
 *
 ************************************************************/
function update_history_buttons(gobj)
{
    let priv = gobj.priv;
    let $container = gobj_read_attr(gobj, "$container");

    if(priv.edit_mode) {
        const history = graph_get_plugin(gobj, "history");
        if(history) {
            if(history.canRedo()) {
                enableElements($container, ".EV_HISTORY_REDO");
                set_active_state($container, ".EV_HISTORY_REDO", true);
            } else {
                disableElements($container, ".EV_HISTORY_REDO");
                set_active_state($container, ".EV_HISTORY_REDO", false);
            }

            if (history.canUndo()) {
                enableElements($container, ".EV_HISTORY_UNDO");
                set_active_state($container, ".EV_HISTORY_UNDO", true);

                // Pending changes exist, enable save
                enableElements($container, ".EV_SAVE_GRAPH");
                set_submit_state($container, ".EV_SAVE_GRAPH", true);
            } else {
                disableElements($container, ".EV_HISTORY_UNDO");
                set_active_state($container, ".EV_HISTORY_UNDO", false);

                // No more undos: graph is back to original state, disable save
                disableElements($container, ".EV_SAVE_GRAPH");
                set_submit_state($container, ".EV_SAVE_GRAPH", false);
            }
        } else {
            disableElements($container, ".EV_HISTORY_REDO");
            set_active_state($container, ".EV_HISTORY_REDO", false);

            disableElements($container, ".EV_HISTORY_UNDO");
            set_active_state($container, ".EV_HISTORY_UNDO", false);
        }
    }
}

/************************************************************
 *  Graph utility functions
 *
 *  From the G6 documentation:
 *
 *  Reorganization after adding data
 *      // Add new nodes and edges
 *      graph.addData({
 *          nodes: [{id: 'newNode1'}, {id: 'newNode2'}],
 *          edges: [{id: 'newEdge', origin: 'existingNode', destination: 'newNode1'}]
 *      });
 *
 *      // Draw new nodes and edges
 *      await graph.draw();
 *
 *      // Recalculate the layout
 *      await graph.layout();

    What does graph.render() do? it calls internally to graph.layout()?
    graph.render() does call layout internally. Here's what it does

    render()
    ├── prepare()           // init canvas + runtime
    ├── BEFORE_RENDER event
    ├── [one of three branches depending on layout config:]
    │   ├── No layout       → draw({ type: 'render' }) + autoFit()
    │   ├── Pre-layout      → preLayoutDraw() + autoFit()
    │   └── Post-layout     → draw({ type: 'render' }) + postLayout() + autoFit()
    ├── this.rendered = true
    └── AFTER_RENDER event

    And graph.layout() (graph.ts:1209) is just a thin wrapper:
    public async layout(layoutOptions?: LayoutOptions) {
        await this.context.layout!.postLayout(layoutOptions);
    }

    So render() calls postLayout() directly (the same thing layout() calls) in the post-layout branch.

    Key difference from the documentation's draw() + layout() pattern:

    - render(): Full pipeline:
            initializes canvas/runtime + draw + layout + autoFit.
            Use on first render.
    - draw(): Only redraws elements — no layout recalculation.
    - layout(): Only recalculates positions and redraws — no canvas init.

    After addData(), using draw() + layout() is preferred over render()
    because render() re-runs prepare() (canvas init) which is unnecessary overhead.
    However, render() would also work since it does include layout.


 ************************************************************/
async function graph_center(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    await graph.fitCenter();
}

async function graph_fitview(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

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

async function graph_render(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    await graph.render();
}

async function graph_draw(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    await graph.draw();
}

async function graph_layout(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    await graph.layout();
}

function graph_get_plugin(gobj, plugin_key)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    const plugins = graph.getPlugins();
    const exists = plugins.some((p) => typeof p === 'object' && p.key === plugin_key);
    if(exists) {
        return graph.getPluginInstance(plugin_key);
    } else {
        return null;
    }
}

function graph_add_plugin(gobj, plugin_key, options)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    let plugin = graph_get_plugin(gobj, plugin_key);
    if(!plugin) {
        let plugin_def = {
            key: plugin_key,
            type: plugin_key,
        };
        if(json_object_size(options)) {
            json_object_update(plugin_def, options);  // options can override type
        }
        graph.setPlugins((plugins) => [...plugins, plugin_def]);
    }

    plugin = graph_get_plugin(gobj, plugin_key);

    switch(plugin_key) {
        case "history":
            if(plugin) {
                plugin.on(HistoryEvent.CHANGE, () => {
                    update_history_buttons(gobj);
                });
                graph.updatePlugin({
                    key: plugin_key,
                    beforeAddCommand: () => {
                        return !priv._history_paused;
                    }
                });
            }
            break;
    }
}

function graph_remove_plugin(gobj, plugin_key)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    const plugin = graph_get_plugin(gobj, plugin_key);
    if(plugin) {
        let plugins = graph.getPlugins();
        plugins = plugins.filter((p) => p.key !== plugin_key);
        graph.setPlugins(plugins);
        if(!plugin.destroyed) {
            if(plugin.destroy) {
                plugin.destroy();
            }
        }
    }
}

async function graph_clear(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    priv._xy = 100;
    priv._edge_seq = 0;
    priv.yet_showed = false;

    await graph.clear();
}

async function graph_set_layout(gobj, layout)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    graph.setLayout(layout);
    if(graph.rendered) {
        await graph_layout(gobj);
    } else {
        await graph_render(gobj);
    }
}

function graph_resize(gobj, width, height)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    // Deselect edge before resize
    deselect_edge(gobj);

    graph.setSize(width, height);

    // setSize() may not fire aftertransform, update overlays manually
    update_resize_handles_position(gobj);
    update_port_resize_handles_position(gobj);
    update_node_icon_position(gobj);
    update_link_icon_position(gobj);
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
 *  Custom G6 layout: ManualLayout
 *  Reads positions from graph_props (__graphs__),
 *  falls back to legacy _geometry on the record.
 ************************************************************/
class ManualLayout extends BaseLayout
{
    async execute(data, options) {
        const { nodes = [] } = data;
        return {
            nodes: nodes.map((node) => {
                let gp = node.data.graph_props;
                let geo = node.data.record._geometry;
                let x = (gp && gp.x != null) ? gp.x :
                         (geo && geo.x != null) ? geo.x : 0;
                let y = (gp && gp.y != null) ? gp.y :
                         (geo && geo.y != null) ? geo.y : 0;
                return {
                    id: node.id,
                    style: { x: x, y: y },
                };
            }),
        };
    }
}

/************************************************************
 *  Node selection and resize handles
 ************************************************************/
const RESIZE_HANDLE_SIZE = 8;
const RESIZE_MIN_VP = 20;
const RESIZE_MIN_WORLD = 30;

/************************************************************
 *  SVG icons for floating action buttons (28×28 circles).
 *  Each value is a raw SVG string sized 18×18 in a 24×24 viewBox.
 *  Stroke inherits from CSS `color` via `stroke="currentColor"`.
 ************************************************************/
const SVG_ICONS = {
    gear:
        '<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" ' +
        'fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" ' +
        'stroke-linejoin="round">' +
        '<circle cx="12" cy="12" r="3"/>' +
        '<path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06' +
        'a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09' +
        'A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83' +
        'l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09' +
        'A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83' +
        'l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09' +
        'a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83' +
        'l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09' +
        'a1.65 1.65 0 0 0-1.51 1z"/>' +
        '</svg>',
    trash:
        '<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" ' +
        'fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" ' +
        'stroke-linejoin="round">' +
        '<polyline points="3 6 5 6 21 6"/>' +
        '<path d="M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/>' +
        '<path d="M10 11v6"/><path d="M14 11v6"/>' +
        '<path d="M9 6V4a1 1 0 0 1 1-1h4a1 1 0 0 1 1 1v2"/>' +
        '</svg>',
    link:
        '<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" ' +
        'fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" ' +
        'stroke-linejoin="round">' +
        '<path d="M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71"/>' +
        '<path d="M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71"/>' +
        '</svg>',
};

/************************************************************
 *  Create a 28×28 floating circular icon button.
 *  @param {string} svgKey   - key into SVG_ICONS
 *  @param {string} color    - border & text color (e.g. '#1890ff')
 *  @param {number} left     - CSS left in px
 *  @param {number} top      - CSS top in px
 *  @param {string} title    - tooltip text
 *  @param {Function} onClick
 *  @returns {HTMLElement}
 ************************************************************/
function create_floating_icon(svgKey, color, left, top, title, onClick)
{
    const el = document.createElement('div');
    el.title = title;
    el.innerHTML = SVG_ICONS[svgKey] || '';
    el.style.cssText =
        'position:absolute;' +
        'left:' + left + 'px;' +
        'top:' + top + 'px;' +
        'width:28px;height:28px;' +
        'display:flex;align-items:center;justify-content:center;' +
        'background:#fff;border:1px solid ' + color + ';border-radius:50%;' +
        'cursor:pointer;pointer-events:all;z-index:11;' +
        'box-shadow:0 2px 6px rgba(0,0,0,0.15);' +
        'color:' + color + ';';
    el.addEventListener('click', (e) => {
        e.stopPropagation();
        onClick();
    });
    return el;
}

function select_node(gobj, node_id)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    // Deselect previous
    deselect_node(gobj);

    // Set selected state (not recorded in history, resume after draw)
    history_pause(gobj);
    try {
        graph.setElementState(node_id, ['selected']);
    } catch(e) {}
    priv._selected_node_id = node_id;

    // Show resize handles and properties icon
    show_resize_handles(gobj);
    show_node_icon(gobj);

    graph_draw(gobj).then(() => {
        history_resume(gobj);
    });
}

function deselect_node(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    deselect_port(gobj);
    deselect_edge(gobj);
    hide_node_icon(gobj);
    hide_node_popover(gobj);

    if(priv._selected_node_id) {
        history_pause(gobj);
        try {
            graph.setElementState(priv._selected_node_id, []);
        } catch(e) {}
        priv._selected_node_id = null;

        graph_draw(gobj).then(() => {
            history_resume(gobj);
        });
    }

    hide_resize_handles(gobj);
}

function get_node_viewport_rect(gobj, node_id)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    const pos = graph.getElementPosition(node_id);
    const nodeData = graph.getNodeData(node_id);
    const style = nodeData.style || {};
    const size = style.size || [60];
    const w = Array.isArray(size) ? size[0] : size;
    const h = Array.isArray(size) ? (size.length > 1 ? size[1] : size[0]) : size;

    const vpMin = graph.getViewportByCanvas([pos[0] - w/2, pos[1] - h/2]);
    const vpMax = graph.getViewportByCanvas([pos[0] + w/2, pos[1] + h/2]);

    return {
        left: vpMin[0], top: vpMin[1],
        right: vpMax[0], bottom: vpMax[1]
    };
}

function show_resize_handles(gobj)
{
    hide_resize_handles(gobj);

    let priv = gobj.priv;
    if(!priv._selected_node_id) {
        return;
    }

    // Create container for handles overlay
    const container = document.createElement('div');
    container.className = 'g6-resize-handles';
    container.style.cssText =
        'position:absolute;top:0;left:0;width:100%;height:100%;' +
        'pointer-events:none;z-index:10;';

    // Selection rectangle (dashed border)
    const selRect = document.createElement('div');
    container.appendChild(selRect);

    // 8 resize handles: nw, n, ne, w, e, sw, s, se
    const handle_defs = [
        { cursor: 'nw-resize', mx: -1, my: -1 },
        { cursor: 'n-resize',  mx:  0, my: -1 },
        { cursor: 'ne-resize', mx:  1, my: -1 },
        { cursor: 'w-resize',  mx: -1, my:  0 },
        { cursor: 'e-resize',  mx:  1, my:  0 },
        { cursor: 'sw-resize', mx: -1, my:  1 },
        { cursor: 's-resize',  mx:  0, my:  1 },
        { cursor: 'se-resize', mx:  1, my:  1 },
    ];

    const handles = [];
    for(const def of handle_defs) {
        const el = document.createElement('div');
        el.style.cssText =
            'position:absolute;' +
            'width:' + RESIZE_HANDLE_SIZE + 'px;' +
            'height:' + RESIZE_HANDLE_SIZE + 'px;' +
            'background:#fff;' +
            'border:1px solid #1890ff;' +
            'cursor:' + def.cursor + ';' +
            'pointer-events:all;' +
            'box-sizing:border-box;';
        el.addEventListener('pointerdown', (e) => {
            start_node_resize(gobj, e, def.mx, def.my);
        });
        handles.push({ el: el, mx: def.mx, my: def.my });
        container.appendChild(el);
    }

    priv.$container.appendChild(container);
    priv._resize_handles_el = container;
    priv._resize_handles = handles;
    priv._resize_sel_rect = selRect;

    update_resize_handles_position(gobj);
}

function hide_resize_handles(gobj)
{
    let priv = gobj.priv;

    if(priv._resize_handles_el) {
        priv._resize_handles_el.remove();
        priv._resize_handles_el = null;
        priv._resize_handles = [];
        priv._resize_sel_rect = null;
    }
}

function update_resize_handles_position(gobj)
{
    let priv = gobj.priv;

    if(!priv._selected_node_id || priv._selected_port_key) {
        return;
    }

    // Recreate handles if they were removed (e.g. by resize)
    if(!priv._resize_handles_el) {
        show_resize_handles(gobj);
        return;
    }

    try {
        const rect = get_node_viewport_rect(gobj, priv._selected_node_id);
        apply_handles_to_rect(gobj, rect);
    } catch(e) {
        // Node may have been removed
        hide_resize_handles(gobj);
    }
}

function apply_handles_to_rect(gobj, rect)
{
    let priv = gobj.priv;
    const HALF = RESIZE_HANDLE_SIZE / 2;

    const left = rect.left;
    const top = rect.top;
    const w = rect.right - rect.left;
    const h = rect.bottom - rect.top;

    // Selection rectangle
    const selRect = priv._resize_sel_rect;
    if(selRect) {
        selRect.style.cssText =
            'position:absolute;' +
            'left:' + left + 'px;' +
            'top:' + top + 'px;' +
            'width:' + w + 'px;' +
            'height:' + h + 'px;' +
            'border:1px dashed #1890ff;' +
            'pointer-events:none;' +
            'box-sizing:border-box;';
    }

    // Handle positions: nw, n, ne, w, e, sw, s, se
    const positions = [
        { x: left,       y: top },
        { x: left + w/2, y: top },
        { x: left + w,   y: top },
        { x: left,       y: top + h/2 },
        { x: left + w,   y: top + h/2 },
        { x: left,       y: top + h },
        { x: left + w/2, y: top + h },
        { x: left + w,   y: top + h },
    ];

    for(let i = 0; i < priv._resize_handles.length; i++) {
        const handle = priv._resize_handles[i];
        const p = positions[i];
        handle.el.style.left = (p.x - HALF) + 'px';
        handle.el.style.top = (p.y - HALF) + 'px';
    }
}

function start_node_resize(gobj, e, mx, my)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    e.preventDefault();
    e.stopPropagation();

    const node_id = priv._selected_node_id;
    if(!node_id) {
        return;
    }

    // Capture original state
    const pos = graph.getElementPosition(node_id);
    const nodeData = graph.getNodeData(node_id);
    const nodeType = nodeData.type;
    const style = nodeData.style || {};
    const size = style.size || [60];
    const origW = Array.isArray(size) ? size[0] : size;
    const origH = Array.isArray(size) ? (size.length > 1 ? size[1] : size[0]) : size;
    const origPortR = style.portR || 0;
    const origCx = pos[0];
    const origCy = pos[1];

    const origVpRect = get_node_viewport_rect(gobj, node_id);
    const startX = e.clientX;
    const startY = e.clientY;
    const zoom = graph.getZoom();

    let currentRect = { ...origVpRect };

    function onPointerMove(ev) {
        const dx = ev.clientX - startX;
        const dy = ev.clientY - startY;

        currentRect = { ...origVpRect };

        if(mx === -1) {
            currentRect.left = Math.min(origVpRect.left + dx, origVpRect.right - RESIZE_MIN_VP);
        }
        if(mx === 1) {
            currentRect.right = Math.max(origVpRect.right + dx, origVpRect.left + RESIZE_MIN_VP);
        }
        if(my === -1) {
            currentRect.top = Math.min(origVpRect.top + dy, origVpRect.bottom - RESIZE_MIN_VP);
        }
        if(my === 1) {
            currentRect.bottom = Math.max(origVpRect.bottom + dy, origVpRect.top + RESIZE_MIN_VP);
        }

        apply_handles_to_rect(gobj, currentRect);
    }

    function onPointerUp(ev) {
        document.removeEventListener('pointermove', onPointerMove);
        document.removeEventListener('pointerup', onPointerUp);

        // Calculate final size in world coordinates from the viewport rect deltas
        const dLeft = (currentRect.left - origVpRect.left) / zoom;
        const dTop = (currentRect.top - origVpRect.top) / zoom;
        const dRight = (currentRect.right - origVpRect.right) / zoom;
        const dBottom = (currentRect.bottom - origVpRect.bottom) / zoom;

        // New world bounds
        const newLeft = (origCx - origW/2) + dLeft;
        const newTop = (origCy - origH/2) + dTop;
        const newRight = (origCx + origW/2) + dRight;
        const newBottom = (origCy + origH/2) + dBottom;

        let newW = newRight - newLeft;
        let newH = newBottom - newTop;
        const newCx = (newLeft + newRight) / 2;
        const newCy = (newTop + newBottom) / 2;

        let updateStyle = {
            x: newCx,
            y: newCy,
        };

        // Circle nodes: single size value (keep it a circle)
        if(nodeType === 'circle') {
            const d = Math.max(newW, newH);
            updateStyle.size = [d];
        } else {
            updateStyle.size = [newW, newH];
        }

        // HTML nodes: update dx, dy to keep content centered
        if(nodeType === 'html') {
            updateStyle.dx = -newW / 2;
            updateStyle.dy = -newH / 2;
        }

        // Scale port radius proportionally to node size change
        if(origPortR > 0) {
            const scale = (nodeType === 'circle') ?
                Math.max(newW, newH) / Math.max(origW, origH) :
                Math.max(newW / origW, newH / origH);
            updateStyle.portR = Math.max(2, Math.round(origPortR * scale));
        }

        graph.updateNodeData([{ id: node_id, style: updateStyle }]);
        graph.draw().then(() => {
            update_resize_handles_position(gobj);

            // Mark graph as dirty (enable save button)
            let $container = gobj_read_attr(gobj, "$container");
            enableElements($container, ".EV_SAVE_GRAPH");
            set_submit_state($container, ".EV_SAVE_GRAPH", true);
        });
    }

    document.addEventListener('pointermove', onPointerMove);
    document.addEventListener('pointerup', onPointerUp);
}

/************************************************************
 *  Port selection and individual port resizing
 ************************************************************/
const PORT_HANDLE_SIZE = 8;

/************************************************************
 *  Compute the canvas (world) position of a port on a node.
 *  Returns {x, y} in world coordinates.
 ************************************************************/
function get_port_canvas_position(gobj, node_id, port_key)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    const pos = graph.getElementPosition(node_id);
    const nodeData = graph.getNodeData(node_id);
    const style = nodeData.style || {};
    const size = style.size || [60];
    const w = Array.isArray(size) ? size[0] : size;
    const h = Array.isArray(size) ? (size.length > 1 ? size[1] : size[0]) : size;
    const ports = style.ports || [];

    for(let i = 0; i < ports.length; i++) {
        if(ports[i].key === port_key) {
            let pl = ports[i].placement || [0.5, 0.5];
            return {
                x: pos[0] + (pl[0] - 0.5) * w,
                y: pos[1] + (pl[1] - 0.5) * h
            };
        }
    }
    return null;
}

/************************************************************
 *  Get the radius of a specific port on a node.
 ************************************************************/
function get_port_radius(gobj, node_id, port_key)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    const nodeData = graph.getNodeData(node_id);
    const style = nodeData.style || {};
    const ports = style.ports || [];

    for(let i = 0; i < ports.length; i++) {
        if(ports[i].key === port_key) {
            // Per-port r overrides node-level portR
            if(ports[i].r != null) {
                return ports[i].r;
            }
            break;
        }
    }
    return style.portR || 6;
}

/************************************************************
 *  Detect if a click in canvas coordinates hits a port.
 *  Returns the port key string, or null if no port hit.
 ************************************************************/
function detect_port_click(gobj, node_id, canvasX, canvasY)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    const pos = graph.getElementPosition(node_id);
    const nodeData = graph.getNodeData(node_id);
    const style = nodeData.style || {};
    const size = style.size || [60];
    const w = Array.isArray(size) ? size[0] : size;
    const h = Array.isArray(size) ? (size.length > 1 ? size[1] : size[0]) : size;
    const ports = style.ports || [];
    const defaultR = style.portR || 6;

    let best_key = null;
    let best_dist = Infinity;

    for(let i = 0; i < ports.length; i++) {
        let pl = ports[i].placement || [0.5, 0.5];
        let px = pos[0] + (pl[0] - 0.5) * w;
        let py = pos[1] + (pl[1] - 0.5) * h;
        let r = ports[i].r != null ? ports[i].r : defaultR;

        let dx = canvasX - px;
        let dy = canvasY - py;
        let dist = Math.sqrt(dx * dx + dy * dy);

        // Hit area is the port radius + a tolerance of 4 world units
        if(dist <= r + 4 && dist < best_dist) {
            best_dist = dist;
            best_key = ports[i].key;
        }
    }

    return best_key;
}

/************************************************************
 *  Select a port: deselect node handles, show port handles.
 ************************************************************/
function select_port(gobj, node_id, port_key)
{
    let priv = gobj.priv;

    // Hide node resize handles and icon (but keep node selected state)
    hide_resize_handles(gobj);
    hide_node_icon(gobj);
    hide_node_popover(gobj);
    exit_linking_mode(gobj);

    priv._selected_node_id = node_id;
    priv._selected_port_key = port_key;

    show_port_resize_handles(gobj);
    show_link_icon_if_fkey(gobj);
}

/************************************************************
 *  Deselect port: clear port state, return to node selection.
 ************************************************************/
function deselect_port(gobj)
{
    let priv = gobj.priv;

    exit_linking_mode(gobj);
    hide_link_icon(gobj);
    hide_port_resize_handles(gobj);
    priv._selected_port_key = null;
}

/************************************************************
 *  Show a link icon next to a selected fkey port.
 ************************************************************/
function show_link_icon_if_fkey(gobj)
{
    hide_link_icon(gobj);

    let priv = gobj.priv;
    let graph = priv.graph;
    if(!priv._selected_node_id || !priv._selected_port_key) {
        return;
    }

    let nodeData = graph.getNodeData(priv._selected_node_id);
    if(!nodeData || !nodeData.data || !nodeData.data.desc) {
        return;
    }
    let desc = nodeData.data.desc;
    let col = get_col_by_id(desc, priv._selected_port_key);
    if(!col) {
        return;
    }
    let field_desc = treedb_get_field_desc(col);
    if(field_desc.type !== 'fkey') {
        return;
    }

    // Get viewport position of the port
    let canvasPos = get_port_canvas_position(gobj, priv._selected_node_id, priv._selected_port_key);
    if(!canvasPos) {
        return;
    }
    let vp = graph.getViewportByCanvas([canvasPos.x, canvasPos.y]);

    let icon = create_floating_icon(
        'link', '#52c41a', vp[0] + 18, vp[1] - 14,
        t('link to hook'), () => {
            // Click (no drag) toggles linking mode
            if(!priv._linking_mode) {
                enter_linking_mode(gobj);
            }
        }
    );
    // Add drag support: pointerdown starts drag-link
    icon.addEventListener('pointerdown', (e) => {
        e.stopPropagation();
        e.preventDefault();
        start_link_drag(gobj, e);
    });
    priv.$container.appendChild(icon);
    priv._link_icon_el = icon;
}

function hide_link_icon(gobj)
{
    let priv = gobj.priv;
    if(priv._link_icon_el) {
        priv._link_icon_el.remove();
        priv._link_icon_el = null;
    }
}

function update_link_icon_position(gobj)
{
    let priv = gobj.priv;
    if(!priv._link_icon_el || !priv._selected_node_id || !priv._selected_port_key) {
        return;
    }
    try {
        let graph = priv.graph;
        let canvasPos = get_port_canvas_position(
            gobj, priv._selected_node_id, priv._selected_port_key
        );
        if(!canvasPos) {
            hide_link_icon(gobj);
            return;
        }
        let vp = graph.getViewportByCanvas([canvasPos.x, canvasPos.y]);
        priv._link_icon_el.style.left = (vp[0] + 18) + 'px';
        priv._link_icon_el.style.top = (vp[1] - 14) + 'px';
    } catch(e) {
        hide_link_icon(gobj);
    }
}

/************************************************************
 *  Get a column descriptor by field id from a topic desc.
 ************************************************************/
function get_col_by_id(desc, col_id)
{
    let cols = desc.cols;
    for(let i = 0; i < cols.length; i++) {
        if(cols[i].id === col_id) {
            return cols[i];
        }
    }
    return null;
}

/************************************************************
 *  Linking mode: click-click flow.
 *  1. User clicks link icon on a fkey port → enter_linking_mode
 *  2. Compatible hook ports are highlighted
 *  3. User clicks on a valid hook port → complete_link
 *  4. User clicks elsewhere or ESC → exit_linking_mode
 ************************************************************/
function enter_linking_mode(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    if(priv._linking_mode) {
        exit_linking_mode(gobj);
        return;
    }

    let node_id = priv._selected_node_id;
    let port_key = priv._selected_port_key;
    if(!node_id || !port_key) {
        return;
    }

    let nodeData = graph.getNodeData(node_id);
    if(!nodeData || !nodeData.data || !nodeData.data.desc) {
        return;
    }
    let desc = nodeData.data.desc;
    let col = get_col_by_id(desc, port_key);
    if(!col || !col.fkey) {
        return;
    }

    // col.fkey = { "parent_topic": "hook_name" }
    // Find all compatible hook ports on all nodes
    let valid_hooks = [];
    let saved_styles = [];

    for(const [parent_topic, hook_name] of Object.entries(col.fkey)) {
        let parent_desc = priv.descs[parent_topic];
        if(!parent_desc) {
            continue;
        }

        // Find all nodes of this topic
        let nodes = graph.getData().nodes || [];
        for(let n of nodes) {
            let nd = graph.getNodeData(n.id);
            if(!nd || !nd.data || !nd.data.desc) {
                continue;
            }
            if(nd.data.desc.topic_name !== parent_topic) {
                continue;
            }
            // Don't link to self
            if(n.id === node_id) {
                continue;
            }
            valid_hooks.push({
                node_id: n.id,
                port_key: hook_name,
                parent_topic: parent_topic,
                parent_id: nd.data.record ? nd.data.record.id : null,
            });
        }
    }

    if(valid_hooks.length === 0) {
        return;
    }

    // Highlight valid hook ports (enlarge + green stroke)
    // Pause history so highlighting doesn't pollute undo queue
    history_pause(gobj);
    for(let vh of valid_hooks) {
        let nd = graph.getNodeData(vh.node_id);
        if(!nd) continue;
        let style = nd.style || {};
        let ports = style.ports || [];
        for(let p of ports) {
            if(p.key === vh.port_key) {
                saved_styles.push({
                    node_id: vh.node_id,
                    port_key: vh.port_key,
                    orig_stroke: p.stroke,
                    orig_lineWidth: p.lineWidth,
                    orig_r: p.r,
                });
                p.stroke = '#52c41a';
                p.lineWidth = 3;
                if(p.r != null) {
                    p.r = p.r + 4;
                } else {
                    p.r = (style.portR || 6) + 4;
                }
            }
        }
        graph.updateNodeData([{ id: vh.node_id, style: { ports: ports } }]);
    }

    priv._linking_mode = true;
    priv._link_source = {
        node_id: node_id,
        port_key: port_key,
        col: col,
        topic_name: desc.topic_name,
        child_id: nodeData.data.record ? nodeData.data.record.id : null,
    };
    priv._link_valid_hooks = valid_hooks;
    priv._link_saved_styles = saved_styles;

    graph.draw().then(() => {
        history_resume(gobj);
    });
}

function exit_linking_mode(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    if(!priv._linking_mode) {
        return;
    }

    // Clean up drag line if active
    if(priv._link_drag_svg) {
        priv._link_drag_svg.remove();
        priv._link_drag_svg = null;
    }
    priv.$container.style.cursor = '';

    // Restore original port styles
    let updated_nodes = {};
    for(let saved of priv._link_saved_styles) {
        let nd = graph.getNodeData(saved.node_id);
        if(!nd) continue;
        let style = nd.style || {};
        let ports = style.ports || [];
        for(let p of ports) {
            if(p.key === saved.port_key) {
                p.stroke = saved.orig_stroke;
                p.lineWidth = saved.orig_lineWidth;
                if(saved.orig_r != null) {
                    p.r = saved.orig_r;
                } else {
                    delete p.r;
                }
            }
        }
        updated_nodes[saved.node_id] = { id: saved.node_id, style: { ports: ports } };
    }
    let updates = Object.values(updated_nodes);
    if(updates.length > 0) {
        history_pause(gobj);
        graph.updateNodeData(updates);
        graph.draw().then(() => {
            history_resume(gobj);
        });
    }

    priv._linking_mode = false;
    priv._link_source = null;
    priv._link_valid_hooks = [];
    priv._link_saved_styles = [];
}

function try_complete_link(gobj, clicked_node_id, clicked_port_key)
{
    let priv = gobj.priv;

    if(!priv._linking_mode || !priv._link_source) {
        return false;
    }

    // Check if the clicked port is a valid hook target
    let target = priv._link_valid_hooks.find(vh =>
        vh.node_id === clicked_node_id && vh.port_key === clicked_port_key
    );

    if(!target) {
        return false;
    }

    let source = priv._link_source;

    // Build parent_ref and child_ref for the backend
    // parent_ref = "parent_topic^parent_id^hook_name"
    // child_ref = "child_topic^child_id"
    let parent_ref = target.parent_topic + "^" + target.parent_id + "^" + target.port_key;
    let child_ref = source.topic_name + "^" + source.child_id;

    exit_linking_mode(gobj);
    deselect_port(gobj);
    deselect_node(gobj);

    gobj_publish_event(gobj, "EV_LINK_NODES", {
        treedb_name: priv.treedb_name,
        parent_ref: parent_ref,
        child_ref: child_ref,
    });

    return true;
}

/************************************************************
 *  Drag-link: drag from link icon to a hook port.
 *  Draws a temporary SVG line from the fkey port to the cursor.
 ************************************************************/
function start_link_drag(gobj, e)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    // Enter linking mode first (highlights valid hooks)
    if(!priv._linking_mode) {
        enter_linking_mode(gobj);
    }
    if(!priv._linking_mode) {
        return; // no valid hooks, nothing to drag
    }

    // Line starts from the center of the link icon
    let containerRect = priv.$container.getBoundingClientRect();
    let iconRect = priv._link_icon_el.getBoundingClientRect();
    let startX = iconRect.left + iconRect.width / 2 - containerRect.left;
    let startY = iconRect.top + iconRect.height / 2 - containerRect.top;

    // Create SVG overlay for the drag line
    let svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    svg.style.cssText =
        'position:absolute;top:0;left:0;width:100%;height:100%;' +
        'pointer-events:none;z-index:10;';
    let line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
    line.setAttribute('x1', startX);
    line.setAttribute('y1', startY);
    line.setAttribute('x2', e.clientX - containerRect.left);
    line.setAttribute('y2', e.clientY - containerRect.top);
    line.setAttribute('stroke', '#52c41a');
    line.setAttribute('stroke-width', '2');
    line.setAttribute('stroke-dasharray', '6 3');
    svg.appendChild(line);
    priv.$container.appendChild(svg);
    priv._link_drag_svg = svg;

    let dragged = false;

    function onPointerMove(ev) {
        dragged = true;
        let cx = ev.clientX - containerRect.left;
        let cy = ev.clientY - containerRect.top;
        line.setAttribute('x2', cx);
        line.setAttribute('y2', cy);

        // Check if cursor is over a valid hook port → change cursor
        let canvasPt = graph.getCanvasByViewport([cx, cy]);
        let hoverTarget = find_hook_at_point(gobj, canvasPt[0], canvasPt[1]);
        priv.$container.style.cursor = hoverTarget ? 'copy' : 'not-allowed';
    }

    function onPointerUp(ev) {
        document.removeEventListener('pointermove', onPointerMove);
        document.removeEventListener('pointerup', onPointerUp);
        priv.$container.style.cursor = '';

        // Remove drag line
        if(priv._link_drag_svg) {
            priv._link_drag_svg.remove();
            priv._link_drag_svg = null;
        }

        if(!dragged) {
            // It was a click, not a drag — let the click handler deal with it
            return;
        }

        // Check if dropped on a valid hook port
        let cx = ev.clientX - containerRect.left;
        let cy = ev.clientY - containerRect.top;
        let canvasPt = graph.getCanvasByViewport([cx, cy]);
        let target = find_hook_at_point(gobj, canvasPt[0], canvasPt[1]);

        if(target) {
            try_complete_link(gobj, target.node_id, target.port_key);
        } else {
            exit_linking_mode(gobj);
        }
    }

    document.addEventListener('pointermove', onPointerMove);
    document.addEventListener('pointerup', onPointerUp);
}

/************************************************************
 *  Find if a canvas point hits a valid hook port during
 *  linking mode. Returns {node_id, port_key} or null.
 ************************************************************/
function find_hook_at_point(gobj, canvasX, canvasY)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    for(let vh of priv._link_valid_hooks) {
        let nd = graph.getNodeData(vh.node_id);
        if(!nd) continue;
        let style = nd.style || {};
        let size = style.size || [60];
        let w = Array.isArray(size) ? size[0] : size;
        let h = Array.isArray(size) ? (size.length > 1 ? size[1] : size[0]) : size;
        let pos = graph.getElementPosition(vh.node_id);
        let ports = style.ports || [];

        for(let p of ports) {
            if(p.key !== vh.port_key) continue;
            let pl = p.placement || [0.5, 0.5];
            let px = pos[0] + (pl[0] - 0.5) * w;
            let py = pos[1] + (pl[1] - 0.5) * h;
            let r = p.r != null ? p.r : (style.portR || 6);
            let dx = canvasX - px;
            let dy = canvasY - py;
            if(Math.sqrt(dx * dx + dy * dy) <= r + 6) {
                return { node_id: vh.node_id, port_key: vh.port_key };
            }
        }
    }
    return null;
}

/************************************************************
 *  Show 4 resize handles (N, E, S, W) around selected port.
 ************************************************************/
function show_port_resize_handles(gobj)
{
    hide_port_resize_handles(gobj);

    let priv = gobj.priv;
    if(!priv._selected_port_key || !priv._selected_node_id) {
        return;
    }

    const container = document.createElement('div');
    container.className = 'g6-port-resize-handles';
    container.style.cssText =
        'position:absolute;top:0;left:0;width:100%;height:100%;' +
        'pointer-events:none;z-index:11;';

    // Dashed circle indicator
    const ring = document.createElement('div');
    container.appendChild(ring);

    // 4 handles: N, E, S, W
    const handle_defs = [
        { cursor: 'n-resize',  dx:  0, dy: -1 },
        { cursor: 'e-resize',  dx:  1, dy:  0 },
        { cursor: 's-resize',  dx:  0, dy:  1 },
        { cursor: 'w-resize',  dx: -1, dy:  0 },
    ];

    const handles = [];
    for(const def of handle_defs) {
        const el = document.createElement('div');
        el.style.cssText =
            'position:absolute;' +
            'width:' + PORT_HANDLE_SIZE + 'px;' +
            'height:' + PORT_HANDLE_SIZE + 'px;' +
            'background:#fff;' +
            'border:1px solid #fa8c16;' +
            'border-radius:50%;' +
            'cursor:' + def.cursor + ';' +
            'pointer-events:all;' +
            'box-sizing:border-box;';
        el.addEventListener('pointerdown', (e) => {
            start_port_resize(gobj, e);
        });
        handles.push({ el: el, dx: def.dx, dy: def.dy });
        container.appendChild(el);
    }

    priv.$container.appendChild(container);
    priv._port_handles_el = container;
    priv._port_handles = handles;
    priv._port_ring = ring;

    update_port_resize_handles_position(gobj);
}

/************************************************************
 *  Hide port resize handles.
 ************************************************************/
function hide_port_resize_handles(gobj)
{
    let priv = gobj.priv;

    if(priv._port_handles_el) {
        priv._port_handles_el.remove();
        priv._port_handles_el = null;
        priv._port_handles = [];
        priv._port_ring = null;
    }
}

/************************************************************
 *  Update port handle positions after zoom/pan/resize.
 ************************************************************/
function update_port_resize_handles_position(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    if(!priv._selected_port_key || !priv._port_handles_el) {
        return;
    }

    try {
        let canvasPos = get_port_canvas_position(
            gobj, priv._selected_node_id, priv._selected_port_key
        );
        if(!canvasPos) {
            hide_port_resize_handles(gobj);
            return;
        }

        let r = get_port_radius(gobj, priv._selected_node_id, priv._selected_port_key);
        let vpCenter = graph.getViewportByCanvas([canvasPos.x, canvasPos.y]);
        let vpEdge = graph.getViewportByCanvas([canvasPos.x + r, canvasPos.y]);
        let vpR = vpEdge[0] - vpCenter[0]; // radius in viewport pixels

        apply_port_handles(gobj, vpCenter[0], vpCenter[1], vpR);
    } catch(e) {
        hide_port_resize_handles(gobj);
    }
}

/************************************************************
 *  Position port handles and ring around viewport center.
 ************************************************************/
function apply_port_handles(gobj, cx, cy, vpR)
{
    let priv = gobj.priv;
    const HALF = PORT_HANDLE_SIZE / 2;

    // Dashed ring
    const ring = priv._port_ring;
    if(ring) {
        const d = vpR * 2;
        ring.style.cssText =
            'position:absolute;' +
            'left:' + (cx - vpR) + 'px;' +
            'top:' + (cy - vpR) + 'px;' +
            'width:' + d + 'px;' +
            'height:' + d + 'px;' +
            'border:1px dashed #fa8c16;' +
            'border-radius:50%;' +
            'pointer-events:none;' +
            'box-sizing:border-box;';
    }

    // N, E, S, W handles
    const offsets = [
        { x: cx,        y: cy - vpR },  // N
        { x: cx + vpR,  y: cy },         // E
        { x: cx,        y: cy + vpR },  // S
        { x: cx - vpR,  y: cy },         // W
    ];

    for(let i = 0; i < priv._port_handles.length; i++) {
        const h = priv._port_handles[i];
        const o = offsets[i];
        h.el.style.left = (o.x - HALF) + 'px';
        h.el.style.top = (o.y - HALF) + 'px';
    }
}

/************************************************************
 *  Drag handler for port resize.
 *  Uses distance from port center to pointer as new radius.
 ************************************************************/
function start_port_resize(gobj, e)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    e.preventDefault();
    e.stopPropagation();

    const node_id = priv._selected_node_id;
    const port_key = priv._selected_port_key;
    if(!node_id || !port_key) {
        return;
    }

    const canvasPos = get_port_canvas_position(gobj, node_id, port_key);
    if(!canvasPos) {
        return;
    }

    // Get viewport center of port for live feedback
    // vpCenter is container-relative; client coords need container offset
    const vpCenter = graph.getViewportByCanvas([canvasPos.x, canvasPos.y]);
    const containerRect = priv.$container.getBoundingClientRect();
    const clientCx = vpCenter[0] + containerRect.left;
    const clientCy = vpCenter[1] + containerRect.top;
    const zoom = graph.getZoom();

    function onPointerMove(ev) {
        const dx = ev.clientX - clientCx;
        const dy = ev.clientY - clientCy;
        let vpR = Math.max(PORT_HANDLE_SIZE, Math.sqrt(dx * dx + dy * dy));

        apply_port_handles(gobj, vpCenter[0], vpCenter[1], vpR);
    }

    function onPointerUp(ev) {
        document.removeEventListener('pointermove', onPointerMove);
        document.removeEventListener('pointerup', onPointerUp);

        // Calculate new radius in world coordinates
        const dx = ev.clientX - clientCx;
        const dy = ev.clientY - clientCy;
        let vpR = Math.max(PORT_HANDLE_SIZE, Math.sqrt(dx * dx + dy * dy));
        let newR = Math.max(2, Math.round(vpR / zoom));

        // Update the individual port's r in the ports array
        const nodeData = graph.getNodeData(node_id);
        const style = nodeData.style || {};
        let ports = style.ports ? [...style.ports] : [];
        for(let i = 0; i < ports.length; i++) {
            if(ports[i].key === port_key) {
                ports[i] = { ...ports[i], r: newR };
                break;
            }
        }

        graph.updateNodeData([{ id: node_id, style: { ports: ports } }]);
        graph.draw().then(() => {
            update_port_resize_handles_position(gobj);

            let $container = gobj_read_attr(gobj, "$container");
            enableElements($container, ".EV_SAVE_GRAPH");
            set_submit_state($container, ".EV_SAVE_GRAPH", true);
        });
    }

    document.addEventListener('pointermove', onPointerMove);
    document.addEventListener('pointerup', onPointerUp);
}

/************************************************************
 *  Edge selection: show a floating properties icon near
 *  the edge midpoint. Clicking it opens a popover form.
 ************************************************************/
function select_edge(gobj, edge_id)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    deselect_edge(gobj);

    priv._selected_edge_id = edge_id;

    history_pause(gobj);
    try {
        graph.setElementState(edge_id, ['selected']);
    } catch(e) {}

    show_edge_icon(gobj);

    graph_draw(gobj).then(() => {
        history_resume(gobj);
    });
}

function deselect_edge(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    hide_edge_popover(gobj);
    hide_edge_icon(gobj);

    if(priv._selected_edge_id) {
        history_pause(gobj);
        try {
            graph.setElementState(priv._selected_edge_id, []);
        } catch(e) {}
        priv._selected_edge_id = null;

        graph_draw(gobj).then(() => {
            history_resume(gobj);
        });
    }
}

/************************************************************
 *  Get the midpoint of an edge in viewport coordinates.
 ************************************************************/
function get_edge_viewport_midpoint(gobj, edge_id)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    let edgeData = graph.getEdgeData(edge_id);
    if(!edgeData) {
        return null;
    }

    let sourcePos = graph.getElementPosition(edgeData.source);
    let targetPos = graph.getElementPosition(edgeData.target);

    let midX = (sourcePos[0] + targetPos[0]) / 2;
    let midY = (sourcePos[1] + targetPos[1]) / 2;

    let vp = graph.getViewportByCanvas([midX, midY]);
    return { x: vp[0], y: vp[1] };
}

/************************************************************
 *  Show a floating properties icon at the edge midpoint.
 ************************************************************/
function show_edge_icon(gobj)
{
    hide_edge_icon(gobj);

    let priv = gobj.priv;
    if(!priv._selected_edge_id) {
        return;
    }

    let mid = get_edge_viewport_midpoint(gobj, priv._selected_edge_id);
    if(!mid) {
        return;
    }

    const icon = create_floating_icon(
        'gear', '#52c41a', mid.x + 4, mid.y - 14,
        t('edge properties'), () => toggle_edge_popover(gobj)
    );
    priv.$container.appendChild(icon);
    priv._edge_icon_el = icon;

    const del_icon = create_floating_icon(
        'trash', '#ff4d4f', mid.x + 4, mid.y + 18,
        t('unlink'), () => request_unlink_edge(gobj)
    );
    priv.$container.appendChild(del_icon);
    priv._edge_delete_el = del_icon;
}

function hide_edge_icon(gobj)
{
    let priv = gobj.priv;
    if(priv._edge_icon_el) {
        priv._edge_icon_el.remove();
        priv._edge_icon_el = null;
    }
    if(priv._edge_delete_el) {
        priv._edge_delete_el.remove();
        priv._edge_delete_el = null;
    }
}

function update_edge_icon_position(gobj)
{
    let priv = gobj.priv;
    if(!priv._selected_edge_id || !priv._edge_icon_el) {
        return;
    }

    try {
        let mid = get_edge_viewport_midpoint(gobj, priv._selected_edge_id);
        if(!mid) {
            hide_edge_icon(gobj);
            hide_edge_popover(gobj);
            return;
        }
        priv._edge_icon_el.style.left = (mid.x + 4) + 'px';
        priv._edge_icon_el.style.top = (mid.y - 14) + 'px';
        if(priv._edge_delete_el) {
            priv._edge_delete_el.style.left = (mid.x + 4) + 'px';
            priv._edge_delete_el.style.top = (mid.y + 18) + 'px';
        }

        // Reposition popover if open
        if(priv._edge_popover_el) {
            priv._edge_popover_el.style.left = (mid.x + 36) + 'px';
            priv._edge_popover_el.style.top = (mid.y - 14) + 'px';
            clamp_popover_position(gobj, priv._edge_popover_el);
        }
    } catch(e) {
        hide_edge_icon(gobj);
        hide_edge_popover(gobj);
    }
}

/************************************************************
 *  Edge properties popover: form with lineWidth, color,
 *  and apply-to scope.
 ************************************************************/
function toggle_edge_popover(gobj)
{
    let priv = gobj.priv;
    if(priv._edge_popover_el) {
        hide_edge_popover(gobj);
    } else {
        show_edge_popover(gobj);
    }
}

function show_edge_popover(gobj)
{
    hide_edge_popover(gobj);

    let priv = gobj.priv;
    let graph = priv.graph;
    let edge_id = priv._selected_edge_id;
    if(!edge_id) {
        return;
    }

    let edgeData = graph.getEdgeData(edge_id);
    if(!edgeData) {
        return;
    }
    let style = edgeData.style || {};
    let currentLW = style.lineWidth || 2;
    let currentStroke = style.stroke || '#000000';

    // Save original style for cancel/restore
    let origLW = currentLW;
    let origStroke = currentStroke;

    // Clear selected state so the real style is visible during preview
    history_pause(gobj);
    try {
        graph.setElementState(edge_id, []);
    } catch(e) {}
    graph_draw(gobj).then(() => {
        history_resume(gobj);
    });

    let mid = get_edge_viewport_midpoint(gobj, edge_id);
    if(!mid) {
        return;
    }

    const popover = document.createElement('div');
    popover.className = 'g6-edge-popover';
    popover.style.cssText =
        'position:absolute;' +
        'left:' + (mid.x + 36) + 'px;' +
        'top:' + (mid.y - 14) + 'px;' +
        'background:#fff;border:1px solid #d9d9d9;border-radius:6px;' +
        'padding:12px;z-index:100;pointer-events:all;' +
        'box-shadow:0 4px 12px rgba(0,0,0,0.15);' +
        'min-width:180px;font-size:13px;';

    // Prevent clicks inside popover from deselecting
    popover.addEventListener('click', (e) => e.stopPropagation());
    popover.addEventListener('pointerdown', (e) => e.stopPropagation());

    // Live preview: update the selected edge in real time
    function preview_edge() {
        let lw = parseInt(lwInput.value) || 2;
        let color = colorInput.value;
        graph.updateEdgeData([{ id: edge_id, style: { lineWidth: lw, stroke: color } }]);
        graph.draw();
    }

    // Line width
    let lwLabel = document.createElement('label');
    lwLabel.textContent = t('line width');
    lwLabel.style.cssText = 'display:block;margin-bottom:4px;font-weight:500;';
    popover.appendChild(lwLabel);

    let lwInput = document.createElement('input');
    lwInput.type = 'number';
    lwInput.min = '1';
    lwInput.max = '20';
    lwInput.value = currentLW;
    lwInput.style.cssText =
        'width:100%;padding:4px 6px;border:1px solid #d9d9d9;border-radius:4px;' +
        'box-sizing:border-box;margin-bottom:10px;';
    lwInput.addEventListener('input', preview_edge);
    popover.appendChild(lwInput);

    // Color
    let colorLabel = document.createElement('label');
    colorLabel.textContent = t('color');
    colorLabel.style.cssText = 'display:block;margin-bottom:4px;font-weight:500;';
    popover.appendChild(colorLabel);

    let colorInput = document.createElement('input');
    colorInput.type = 'color';
    colorInput.value = currentStroke;
    colorInput.style.cssText =
        'width:100%;height:30px;padding:0;border:1px solid #d9d9d9;border-radius:4px;' +
        'cursor:pointer;margin-bottom:10px;';
    colorInput.addEventListener('input', preview_edge);
    popover.appendChild(colorInput);

    // Apply-to scope
    let scopeLabel = document.createElement('label');
    scopeLabel.textContent = t('apply to');
    scopeLabel.style.cssText = 'display:block;margin-bottom:4px;font-weight:500;';
    popover.appendChild(scopeLabel);

    let scopeSelect = document.createElement('select');
    scopeSelect.style.cssText =
        'width:100%;padding:4px 6px;border:1px solid #d9d9d9;border-radius:4px;' +
        'box-sizing:border-box;margin-bottom:12px;';
    let options = [
        { value: 'this', label: t('this edge') },
        { value: 'same_type', label: t('same type edges') },
        { value: 'all', label: t('all edges') },
    ];
    for(let opt of options) {
        let o = document.createElement('option');
        o.value = opt.value;
        o.textContent = opt.label;
        scopeSelect.appendChild(o);
    }
    popover.appendChild(scopeSelect);

    // Button row
    let btnRow = document.createElement('div');
    btnRow.style.cssText = 'display:flex;gap:8px;';

    // Cancel button
    let cancelBtn = document.createElement('button');
    cancelBtn.textContent = t('cancel');
    cancelBtn.style.cssText =
        'flex:1;padding:6px;background:#fff;color:#333;border:1px solid #d9d9d9;' +
        'border-radius:4px;cursor:pointer;font-size:13px;font-weight:500;';
    cancelBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        // Restore original style and deselect
        graph.updateEdgeData([{ id: edge_id, style: { lineWidth: origLW, stroke: origStroke } }]);
        graph.draw();
        deselect_edge(gobj);
    });
    btnRow.appendChild(cancelBtn);

    // Apply button
    let applyBtn = document.createElement('button');
    applyBtn.textContent = t('apply');
    applyBtn.style.cssText =
        'flex:1;padding:6px;background:#52c41a;color:#fff;border:none;' +
        'border-radius:4px;cursor:pointer;font-size:13px;font-weight:500;';
    applyBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        apply_edge_properties(gobj, edge_id,
            parseInt(lwInput.value) || 2,
            colorInput.value,
            scopeSelect.value
        );
    });
    btnRow.appendChild(applyBtn);

    popover.appendChild(btnRow);

    priv.$container.appendChild(popover);
    priv._edge_popover_el = popover;

    // Clamp popover inside the container
    clamp_popover_position(gobj, popover);
}

/************************************************************
 *  Clamp a popover so it stays inside the container.
 ************************************************************/
function clamp_popover_position(gobj, popover)
{
    let priv = gobj.priv;
    if(!popover) {
        return;
    }

    let containerRect = priv.$container.getBoundingClientRect();
    let popRect = popover.getBoundingClientRect();
    let margin = 4;

    let left = popover.offsetLeft;
    let top = popover.offsetTop;

    // Clamp right edge
    if(left + popRect.width > containerRect.width - margin) {
        left = containerRect.width - popRect.width - margin;
    }
    // Clamp bottom edge
    if(top + popRect.height > containerRect.height - margin) {
        top = containerRect.height - popRect.height - margin;
    }
    // Clamp left/top
    if(left < margin) {
        left = margin;
    }
    if(top < margin) {
        top = margin;
    }

    popover.style.left = left + 'px';
    popover.style.top = top + 'px';
}

function hide_edge_popover(gobj)
{
    let priv = gobj.priv;
    if(priv._edge_popover_el) {
        priv._edge_popover_el.remove();
        priv._edge_popover_el = null;
    }
}

/************************************************************
 *  Node properties icon and popover
 ************************************************************/
function show_node_icon(gobj)
{
    hide_node_icon(gobj);

    let priv = gobj.priv;
    if(!priv._selected_node_id || priv._selected_port_key) {
        return;
    }

    let rect = get_node_viewport_rect(gobj, priv._selected_node_id);
    if(!rect) {
        return;
    }

    const icon = create_floating_icon(
        'gear', '#1890ff', rect.right + 4, rect.top - 14,
        t('node properties'), () => toggle_node_popover(gobj)
    );
    priv.$container.appendChild(icon);
    priv._node_icon_el = icon;

    const del_icon = create_floating_icon(
        'trash', '#ff4d4f', rect.right + 4, rect.top + 18,
        t('delete node'), () => request_delete_node(gobj)
    );
    priv.$container.appendChild(del_icon);
    priv._node_delete_el = del_icon;
}

function hide_node_icon(gobj)
{
    let priv = gobj.priv;
    if(priv._node_icon_el) {
        priv._node_icon_el.remove();
        priv._node_icon_el = null;
    }
    if(priv._node_delete_el) {
        priv._node_delete_el.remove();
        priv._node_delete_el = null;
    }
}

function update_node_icon_position(gobj)
{
    let priv = gobj.priv;
    if(!priv._selected_node_id || priv._selected_port_key) {
        return;
    }

    // Recreate icon if it was removed (e.g. by resize)
    if(!priv._node_icon_el) {
        show_node_icon(gobj);
        return;
    }

    try {
        let rect = get_node_viewport_rect(gobj, priv._selected_node_id);
        if(!rect) {
            hide_node_icon(gobj);
            hide_node_popover(gobj);
            return;
        }
        priv._node_icon_el.style.left = (rect.right + 4) + 'px';
        priv._node_icon_el.style.top = (rect.top - 14) + 'px';
        if(priv._node_delete_el) {
            priv._node_delete_el.style.left = (rect.right + 4) + 'px';
            priv._node_delete_el.style.top = (rect.top + 18) + 'px';
        }

        // Reposition popover if open
        if(priv._node_popover_el) {
            priv._node_popover_el.style.left = (rect.right + 36) + 'px';
            priv._node_popover_el.style.top = (rect.top - 14) + 'px';
            clamp_popover_position(gobj, priv._node_popover_el);
        }
    } catch(e) {
        hide_node_icon(gobj);
        hide_node_popover(gobj);
    }
}

/************************************************************
 *  Node properties popover: form with fill color, stroke
 *  color, lineWidth, and apply-to scope.
 ************************************************************/
function toggle_node_popover(gobj)
{
    let priv = gobj.priv;
    if(priv._node_popover_el) {
        hide_node_popover(gobj);
    } else {
        show_node_popover(gobj);
    }
}

function show_node_popover(gobj)
{
    hide_node_popover(gobj);

    let priv = gobj.priv;
    let graph = priv.graph;
    let node_id = priv._selected_node_id;
    if(!node_id) {
        return;
    }

    let nodeData = graph.getNodeData(node_id);
    if(!nodeData) {
        return;
    }
    let style = nodeData.style || {};
    let currentFill = style.fill || '#ffffff';
    let currentStroke = style.stroke || '#000000';
    let currentLW = style.lineWidth || 1;

    // Save original style for cancel/restore
    let origFill = currentFill;
    let origStroke = currentStroke;
    let origLW = currentLW;

    let rect = get_node_viewport_rect(gobj, node_id);
    if(!rect) {
        return;
    }

    const popover = document.createElement('div');
    popover.className = 'g6-node-popover';
    popover.style.cssText =
        'position:absolute;' +
        'left:' + (rect.right + 36) + 'px;' +
        'top:' + (rect.top - 14) + 'px;' +
        'background:#fff;border:1px solid #d9d9d9;border-radius:6px;' +
        'padding:12px;z-index:100;pointer-events:all;' +
        'box-shadow:0 4px 12px rgba(0,0,0,0.15);' +
        'min-width:180px;font-size:13px;';

    // Prevent clicks inside popover from deselecting
    popover.addEventListener('click', (e) => e.stopPropagation());
    popover.addEventListener('pointerdown', (e) => e.stopPropagation());

    let node_graph_type = nodeData.data && nodeData.data.desc ?
        nodeData.data.desc.node_treedb_type : null;

    // Live preview
    function preview_node() {
        let fill = fillInput.value;
        let stroke = strokeInput.value;
        let lw = parseInt(lwInput.value) || 1;
        let updateStyle = { fill: fill, stroke: stroke, lineWidth: lw };

        if(node_graph_type === 'hierarchical') {
            let record = nodeData.data.record || {};
            updateStyle.innerHTML = build_node_innerHTML(
                fill, stroke, record.icon, record.id
            );
        }
        graph.updateNodeData([{ id: node_id, style: updateStyle }]);
        graph.draw();
    }

    // Fill color
    let fillLabel = document.createElement('label');
    fillLabel.textContent = t('fill color');
    fillLabel.style.cssText = 'display:block;margin-bottom:4px;font-weight:500;';
    popover.appendChild(fillLabel);

    let fillInput = document.createElement('input');
    fillInput.type = 'color';
    fillInput.value = currentFill;
    fillInput.style.cssText =
        'width:100%;height:30px;padding:0;border:1px solid #d9d9d9;border-radius:4px;' +
        'cursor:pointer;margin-bottom:10px;';
    fillInput.addEventListener('input', preview_node);
    popover.appendChild(fillInput);

    // Stroke color
    let strokeLabel = document.createElement('label');
    strokeLabel.textContent = t('stroke color');
    strokeLabel.style.cssText = 'display:block;margin-bottom:4px;font-weight:500;';
    popover.appendChild(strokeLabel);

    let strokeInput = document.createElement('input');
    strokeInput.type = 'color';
    strokeInput.value = currentStroke;
    strokeInput.style.cssText =
        'width:100%;height:30px;padding:0;border:1px solid #d9d9d9;border-radius:4px;' +
        'cursor:pointer;margin-bottom:10px;';
    strokeInput.addEventListener('input', preview_node);
    popover.appendChild(strokeInput);

    // Line width
    let lwLabel = document.createElement('label');
    lwLabel.textContent = t('line width');
    lwLabel.style.cssText = 'display:block;margin-bottom:4px;font-weight:500;';
    popover.appendChild(lwLabel);

    let lwInput = document.createElement('input');
    lwInput.type = 'number';
    lwInput.min = '1';
    lwInput.max = '20';
    lwInput.value = currentLW;
    lwInput.style.cssText =
        'width:100%;padding:4px 6px;border:1px solid #d9d9d9;border-radius:4px;' +
        'box-sizing:border-box;margin-bottom:10px;';
    lwInput.addEventListener('input', preview_node);
    popover.appendChild(lwInput);

    // Apply-to scope
    let scopeLabel = document.createElement('label');
    scopeLabel.textContent = t('apply to');
    scopeLabel.style.cssText = 'display:block;margin-bottom:4px;font-weight:500;';
    popover.appendChild(scopeLabel);

    let scopeSelect = document.createElement('select');
    scopeSelect.style.cssText =
        'width:100%;padding:4px 6px;border:1px solid #d9d9d9;border-radius:4px;' +
        'box-sizing:border-box;margin-bottom:12px;';
    let options = [
        { value: 'this', label: t('this node') },
        { value: 'same_topic', label: t('same topic nodes') },
        { value: 'all', label: t('all nodes') },
    ];
    for(let opt of options) {
        let o = document.createElement('option');
        o.value = opt.value;
        o.textContent = opt.label;
        scopeSelect.appendChild(o);
    }
    popover.appendChild(scopeSelect);

    // Button row
    let btnRow = document.createElement('div');
    btnRow.style.cssText = 'display:flex;gap:8px;';

    // Cancel button
    let cancelBtn = document.createElement('button');
    cancelBtn.textContent = t('cancel');
    cancelBtn.style.cssText =
        'flex:1;padding:6px;background:#fff;color:#333;border:1px solid #d9d9d9;' +
        'border-radius:4px;cursor:pointer;font-size:13px;font-weight:500;';
    cancelBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        // Restore original style
        let restoreStyle = { fill: origFill, stroke: origStroke, lineWidth: origLW };
        if(node_graph_type === 'hierarchical') {
            let record = nodeData.data.record || {};
            restoreStyle.innerHTML = build_node_innerHTML(
                origFill, origStroke, record.icon, record.id
            );
        }
        graph.updateNodeData([{ id: node_id, style: restoreStyle }]);
        graph.draw();
        hide_node_popover(gobj);
    });
    btnRow.appendChild(cancelBtn);

    // Apply button
    let applyBtn = document.createElement('button');
    applyBtn.textContent = t('apply');
    applyBtn.style.cssText =
        'flex:1;padding:6px;background:#1890ff;color:#fff;border:none;' +
        'border-radius:4px;cursor:pointer;font-size:13px;font-weight:500;';
    applyBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        apply_node_properties(gobj, node_id,
            fillInput.value,
            strokeInput.value,
            parseInt(lwInput.value) || 1,
            scopeSelect.value
        );
    });
    btnRow.appendChild(applyBtn);

    popover.appendChild(btnRow);

    priv.$container.appendChild(popover);
    priv._node_popover_el = popover;

    // Clamp popover inside the container
    clamp_popover_position(gobj, popover);
}

function hide_node_popover(gobj)
{
    let priv = gobj.priv;
    if(priv._node_popover_el) {
        priv._node_popover_el.remove();
        priv._node_popover_el = null;
    }
}

/************************************************************
 *  Build innerHTML for hierarchical (HTML) nodes.
 ************************************************************/
function build_node_innerHTML(fill, stroke, icon, id)
{
    return `
<div style="
    width: 100%;
    height: 100%;
    background: ${fill};
    border: 1px solid ${stroke};
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
        <img src="${safeSrc(icon)}" alt=""/>
        </span>
    </div>
    <div style="font-weight: bold;">
      ${escapeHtml(id)}
    </div>
</div>
`;
}

/************************************************************
 *  Request delete node: publish EV_DELETE_NODE to parent.
 ************************************************************/
function request_delete_node(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    let node_id = priv._selected_node_id;
    if(!node_id) {
        return;
    }

    let nodeData = graph.getNodeData(node_id);
    if(!nodeData || !nodeData.data || !nodeData.data.desc) {
        return;
    }

    gobj_publish_event(gobj, "EV_DELETE_NODE", {
        treedb_name: priv.treedb_name,
        topic_name: nodeData.data.desc.topic_name,
        record: nodeData.data.record,
    });

    deselect_node(gobj);
}

/************************************************************
 *  Request unlink edge: publish EV_UNLINK_NODES to parent.
 ************************************************************/
function request_unlink_edge(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    let edge_id = priv._selected_edge_id;
    if(!edge_id) {
        return;
    }

    let edgeData = graph.getEdgeData(edge_id);
    if(!edgeData || !edgeData.data) {
        return;
    }

    const d = edgeData.data;

    /*
     * Backend decode_parent_ref() expects "topic^id^hook_name".
     * Backend decode_child_ref()  expects "topic^id".
     */
    gobj_publish_event(gobj, "EV_UNLINK_NODES", {
        treedb_name: priv.treedb_name,
        parent_ref: `${d.parent_topic}^${d.parent_id}^${d.hook_name}`,
        child_ref:  `${d.child_topic}^${d.child_id}`,
    });

    deselect_edge(gobj);
}

/************************************************************
 *  Apply node properties to one or more nodes.
 *  scope: 'this', 'same_topic', 'all'
 ************************************************************/
function apply_node_properties(gobj, node_id, fill, stroke, lineWidth, scope)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    let nodeData = graph.getNodeData(node_id);
    if(!nodeData || !nodeData.data) {
        return;
    }

    let source_topic = nodeData.data.desc ? nodeData.data.desc.topic_name : null;
    let updates = [];
    const nodes = graph.getData().nodes;

    for(let i = 0; i < nodes.length; i++) {
        let nd = graph.getNodeData(nodes[i].id);
        if(!nd || !nd.data || !nd.data.desc) {
            continue;
        }

        if(scope === 'this' && nodes[i].id !== node_id) {
            continue;
        }
        if(scope === 'same_topic' && nd.data.desc.topic_name !== source_topic) {
            continue;
        }

        let updateStyle = { fill: fill, stroke: stroke, lineWidth: lineWidth };
        if(nd.data.desc.node_treedb_type === 'hierarchical') {
            let record = nd.data.record || {};
            updateStyle.innerHTML = build_node_innerHTML(
                fill, stroke, record.icon, record.id
            );
        }
        updates.push({ id: nodes[i].id, style: updateStyle });
    }

    if(updates.length > 0) {
        graph.updateNodeData(updates);
        graph.draw().then(() => {
            let $container = gobj_read_attr(gobj, "$container");
            enableElements($container, ".EV_SAVE_GRAPH");
            set_submit_state($container, ".EV_SAVE_GRAPH", true);
        });
    }

    hide_node_popover(gobj);
}

/************************************************************
 *  Apply edge properties to one or more edges.
 *  scope: 'this', 'same_type', 'all'
 ************************************************************/
function apply_edge_properties(gobj, edge_id, lineWidth, stroke, scope)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    let edgeData = graph.getEdgeData(edge_id);
    if(!edgeData || !edgeData.data) {
        return;
    }

    let source_hook = edgeData.data.hook_name;
    let newStyle = { lineWidth: lineWidth, stroke: stroke };

    let updates = [];
    const edges = graph.getData().edges;

    for(let i = 0; i < edges.length; i++) {
        let ed = graph.getEdgeData(edges[i].id);
        if(!ed || !ed.data) {
            continue;
        }

        if(scope === 'this' && edges[i].id !== edge_id) {
            continue;
        }
        if(scope === 'same_type' && ed.data.hook_name !== source_hook) {
            continue;
        }

        updates.push({ id: edges[i].id, style: { ...newStyle } });
    }

    if(updates.length > 0) {
        graph.updateEdgeData(updates);
        graph.draw().then(() => {
            let $container = gobj_read_attr(gobj, "$container");
            enableElements($container, ".EV_SAVE_GRAPH");
            set_submit_state($container, ".EV_SAVE_GRAPH", true);
        });
    }

    // Store defaults in _graph_properties
    let parent_topic = edgeData.data.parent_topic;
    if(scope === 'same_type') {
        if(!is_object(priv._graph_properties[parent_topic])) {
            priv._graph_properties[parent_topic] = {};
        }
        let defaults = priv._graph_properties[parent_topic].defaults || {};
        let edge_defaults = defaults.edge_styles || {};
        edge_defaults[source_hook] = { lineWidth: lineWidth, stroke: stroke };
        defaults.edge_styles = edge_defaults;
        priv._graph_properties[parent_topic].defaults = defaults;
    } else if(scope === 'all') {
        for(const topic_name of Object.keys(priv.descs || {})) {
            if(!is_object(priv._graph_properties[topic_name])) {
                priv._graph_properties[topic_name] = {};
            }
            let defaults = priv._graph_properties[topic_name].defaults || {};
            defaults.edge_style = { lineWidth: lineWidth, stroke: stroke };
            priv._graph_properties[topic_name].defaults = defaults;
        }
    }

    deselect_edge(gobj);
}

/************************************************************
 *  Save edge styles into _graph_properties (called from
 *  save_geometry).
 ************************************************************/
function update_edge_geometry(gobj, edge_id)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    let edgeData = graph.getEdgeData(edge_id);
    if(!edgeData || !edgeData.data) {
        return;
    }

    let style = edgeData.style || {};
    let lineWidth = style.lineWidth;
    let stroke = style.stroke;

    let parent_topic = edgeData.data.parent_topic;
    let edge_key = edgeData.data.hook_name + ":" +
        edgeData.data.parent_id + ":" + edgeData.data.child_id;

    // If default values, remove any previously saved entry
    let hasCustom = (lineWidth != null && lineWidth !== 2);
    if(!hasCustom) {
        let topic_props = priv._graph_properties[parent_topic];
        if(topic_props && is_object(topic_props.edges)) {
            delete topic_props.edges[edge_key];
            if(Object.keys(topic_props.edges).length === 0) {
                delete topic_props.edges;
            }
        }
        return;
    }

    if(!is_object(priv._graph_properties[parent_topic])) {
        priv._graph_properties[parent_topic] = {};
    }
    let topic_props = priv._graph_properties[parent_topic];
    if(!is_object(topic_props.edges)) {
        topic_props.edges = {};
    }

    let edge_props = { lineWidth: lineWidth };
    if(stroke) {
        edge_props.stroke = stroke;
    }
    topic_props.edges[edge_key] = edge_props;
}


/************************************************************
 *  Context menu: build items based on target element.
 *  Returns different menu items for nodes, ports, and edges.
 *  Stores the target in priv for use in the click handler.
 ************************************************************/
function build_context_menu_items(gobj, e)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    // Reset context target
    priv._context_node_id = null;
    priv._context_port_key = null;
    priv._context_edge_id = null;

    if(e.targetType === 'edge') {
        priv._context_edge_id = e.target.id;
        return build_edge_context_menu(gobj);
    }

    if(e.targetType === 'node') {
        let node_id = e.target.id;
        priv._context_node_id = node_id;

        // Detect if right-click is on a port
        let containerRect = priv.$container.getBoundingClientRect();
        let canvasPoint = graph.getCanvasByViewport([
            e.client.x - containerRect.left,
            e.client.y - containerRect.top
        ]);
        let port_key = detect_port_click(
            gobj, node_id, canvasPoint[0], canvasPoint[1]
        );

        if(port_key) {
            priv._context_port_key = port_key;
            return build_port_context_menu(gobj, node_id, port_key);
        } else {
            return build_node_context_menu(gobj, node_id);
        }
    }

    return [];
}

/************************************************************
 *  Build node context menu items.
 ************************************************************/
function build_node_context_menu(gobj, node_id)
{
    let items = [];

    if(gobj.priv.edit_mode) {
        items.push({ name: t('resize all'), value: 'resize_all_nodes' });
        items.push({ name: t('resize topic nodes'), value: 'resize_topic_nodes' });
    }

    return items;
}

/************************************************************
 *  Build port context menu items.
 ************************************************************/
function build_port_context_menu(gobj, node_id, port_key)
{
    let items = [];

    if(gobj.priv.edit_mode) {
        items.push({ name: t('resize all ports'), value: 'resize_all_ports' });
        items.push({ name: t('resize topic ports'), value: 'resize_topic_ports' });
    }

    return items;
}

/************************************************************
 *  Build edge context menu items (prepared for expansion).
 ************************************************************/
function build_edge_context_menu(gobj)
{
    let items = [];
    // Future: add edge-specific actions here
    return items;
}

/************************************************************
 *  Handle context menu click: dispatch by value.
 ************************************************************/
function handle_context_menu_click(gobj, value)
{
    let priv = gobj.priv;

    switch(value) {
        case 'resize_all_nodes':
            copy_size_to_nodes(gobj, false);
            break;
        case 'resize_topic_nodes':
            copy_size_to_nodes(gobj, true);
            break;
        case 'resize_all_ports':
            copy_size_to_ports(gobj, false);
            break;
        case 'resize_topic_ports':
            copy_size_to_ports(gobj, true);
            break;
    }
}

/************************************************************
 *  Copy the selected node's size to other nodes.
 *  If same_topic_only=true, only nodes of the same topic.
 *  If same_topic_only=false, all nodes in the graph.
 *  Also copies portR and stores as default.
 ************************************************************/
function copy_size_to_nodes(gobj, same_topic_only)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    let node_id = priv._context_node_id;
    if(!node_id) {
        return;
    }

    let nodedata = graph.getNodeData(node_id);
    if(!nodedata || !nodedata.data) {
        return;
    }

    let source_topic = nodedata.data.topic_name;
    let source_style = nodedata.style || {};
    let source_size = source_style.size;
    let source_portR = source_style.portR;
    let source_type = nodedata.type;

    if(!source_size) {
        return;
    }

    // Store as default for new nodes
    let defaults = { size: [...source_size] };
    if(source_portR != null) {
        defaults.portR = source_portR;
    }

    if(same_topic_only) {
        // Store default only for this topic
        if(!is_object(priv._graph_properties[source_topic])) {
            priv._graph_properties[source_topic] = {};
        }
        priv._graph_properties[source_topic].defaults = defaults;
    } else {
        // Store default for all topics present in the graph
        for(const topic_name of Object.keys(priv.descs || {})) {
            if(!is_object(priv._graph_properties[topic_name])) {
                priv._graph_properties[topic_name] = {};
            }
            priv._graph_properties[topic_name].defaults = { ...defaults };
        }
    }

    // Iterate nodes and update matching ones
    let updates = [];
    const nodes = graph.getData().nodes;
    for(let i = 0; i < nodes.length; i++) {
        let nd = graph.getNodeData(nodes[i].id);
        if(!nd || !nd.data) {
            continue;
        }
        if(same_topic_only && nd.data.topic_name !== source_topic) {
            continue;
        }
        if(nodes[i].id === node_id) {
            continue; // skip source
        }

        let updateStyle = {
            size: [...source_size],
        };

        if(source_portR != null) {
            updateStyle.portR = source_portR;
        }

        // Recalculate dx/dy for HTML nodes
        if(nd.type === 'html') {
            updateStyle.dx = -source_size[0] / 2;
            let h = source_size.length > 1 ? source_size[1] : source_size[0];
            updateStyle.dy = -h / 2;
        }

        updates.push({ id: nodes[i].id, style: updateStyle });
    }

    if(updates.length > 0) {
        graph.updateNodeData(updates);
        graph.draw().then(() => {
            update_resize_handles_position(gobj);

            let $container = gobj_read_attr(gobj, "$container");
            enableElements($container, ".EV_SAVE_GRAPH");
            set_submit_state($container, ".EV_SAVE_GRAPH", true);
        });
    }
}

/************************************************************
 *  Copy the selected port's radius to matching ports.
 *  If same_topic_only=true, only ports in same-topic nodes.
 *  If same_topic_only=false, matching ports in all nodes.
 *  Also stores as default.
 ************************************************************/
function copy_size_to_ports(gobj, same_topic_only)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    let node_id = priv._context_node_id;
    let port_key = priv._context_port_key;
    if(!node_id || !port_key) {
        return;
    }

    let nodedata = graph.getNodeData(node_id);
    if(!nodedata || !nodedata.data) {
        return;
    }

    let source_topic = nodedata.data.topic_name;
    let source_r = get_port_radius(gobj, node_id, port_key);

    // Store as default
    if(same_topic_only) {
        // Store for matching port key in this topic only
        if(!is_object(priv._graph_properties[source_topic])) {
            priv._graph_properties[source_topic] = {};
        }
        let defaults = priv._graph_properties[source_topic].defaults || {};
        let port_sizes = defaults.port_sizes || {};
        port_sizes[port_key] = source_r;
        defaults.port_sizes = port_sizes;
        priv._graph_properties[source_topic].defaults = defaults;
    } else {
        // Store for all topics — set portR as the global default
        for(const topic_name of Object.keys(priv.descs || {})) {
            if(!is_object(priv._graph_properties[topic_name])) {
                priv._graph_properties[topic_name] = {};
            }
            let defaults = priv._graph_properties[topic_name].defaults || {};
            defaults.portR = source_r;
            priv._graph_properties[topic_name].defaults = defaults;
        }
    }

    // Iterate nodes and update ports
    let updates = [];
    const nodes = graph.getData().nodes;
    for(let i = 0; i < nodes.length; i++) {
        let nd = graph.getNodeData(nodes[i].id);
        if(!nd || !nd.data) {
            continue;
        }
        if(same_topic_only && nd.data.topic_name !== source_topic) {
            continue;
        }

        let style = nd.style || {};
        let ports = style.ports;
        if(!ports) {
            continue;
        }

        let port_updated = false;
        let new_ports = ports.map((p) => {
            if(!same_topic_only || p.key === port_key) {
                port_updated = true;
                return { ...p, r: source_r };
            }
            return p;
        });

        if(port_updated) {
            let upd = { ports: new_ports };
            // When resizing all ports, also update node-level portR
            if(!same_topic_only) {
                upd.portR = source_r;
            }
            updates.push({ id: nodes[i].id, style: upd });
        }
    }

    if(updates.length > 0) {
        graph.updateNodeData(updates);
        graph.draw().then(() => {
            update_port_resize_handles_position(gobj);

            let $container = gobj_read_attr(gobj, "$container");
            enableElements($container, ".EV_SAVE_GRAPH");
            set_submit_state($container, ".EV_SAVE_GRAPH", true);
        });
    }
}


                    /***************************
                     *      Actions
                     ***************************/




/************************************************************
 *  Receive descs, from parent
 ************************************************************/
function ac_descs(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let descs = kw;

    gobj_write_attr(gobj, "descs", descs); // TRIGGER POINT: Topics cleared

    // TODO register_nodes(gobj) = register(ExtensionCategory.NODE, 'light', LightNode);

    /*
     *  Assign colors and calculate counters
     *  descs is a dict: { __snaps__: {…}, roles: {…}, users: {…} }
     */
    let idx = 0;
    for(const [topic_name, desc] of Object.entries(descs)) {
        calculate_hooks_fkeys_counter(desc);
        if(topic_name.substring(0, 2) === "__") {
            continue;
        }
        desc.color = node_colors[idx % node_colors.length];
        idx++;
    }

    return 0;
}

/************************************************************
 *  Clear all graph data, from parent
 ************************************************************/
function ac_clear_data(gobj, event, kw, src)
{
    let priv = gobj.priv;

    deselect_node(gobj);

    gobj_write_attr(gobj, "records", {});

    graph_remove_plugin(gobj, "history");
    update_history_buttons(gobj);
    graph_clear(gobj);

    return 0;
}

/************************************************************
 *  Load batch data, from parent
 *  {
 *      kw_command,
 *      desc,
 *      data
 *  },
 ************************************************************/
function ac_load_data(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    let kw_command = kw.kw_command;
    let data = kw.data;

    let topic_name = kw_get_str(
        gobj, kw_command, "topic_name", "", kw_flag_t.KW_REQUIRED
    );

    if(!topic_name) {
        log_error(`${gobj_short_name(gobj)}: No topic_name in desc`);
        return 0;
    }

    if(topic_name.substring(0, 2) === "__") {
        if(topic_name === '__graphs__') {
            priv.__graphs__ = data;
            build_graph_properties(gobj);
        }
        return 0;
    }

    let desc = priv.descs[topic_name];

    /*-------------------------*
     *  Save topic's records
     *-------------------------*/
    priv.records[topic_name] = data;
    priv.descs[topic_name].loaded = true;

    /*--------------------------------------------------*
     *  Creating and loading topic cells from backend
     *--------------------------------------------------*/
    for(let i=0; i<data.length; i++) {
        let record = data[i];
        create_topic_node(gobj, desc, record);
    }

    /*----------------------------------------------------*
     *  Check if all topics are loaded to make the links
     *----------------------------------------------------*/
    let do_links = true;
    for (const [topic_name, desc] of Object.entries(priv.descs)) {
        if (topic_name.substring(0, 2) === "__") {
            continue;   // ignore system topics
        }
        if(!desc.loaded) {
            do_links = false;
            break;
        }
    }

    if(do_links && priv.graph) {
        graph_draw(gobj).then(() => { // draw nodes, else the link fails
            create_links(gobj);
            graph_draw(gobj).then(() => {
                graph_layout(gobj).then(() => {
                    if(priv.edit_mode) {
                        graph_add_plugin(gobj, "history");
                    } else {
                        graph_remove_plugin(gobj, "history");
                    }
                });
            });
        });
    }

    return 0;
}

/************************************************************
 *  Save graph geometry, from click
 ************************************************************/
function ac_save_graph(gobj, event, kw, src)
{
    let priv = gobj.priv;

    if(priv.edit_mode) {
        let $container = gobj_read_attr(gobj, "$container");

        disableElements($container, ".EV_SAVE_GRAPH");
        set_submit_state($container, ".EV_SAVE_GRAPH", false);

        let history = graph_get_plugin(gobj, "history");
        if(history) {
            history.clear();
            update_history_buttons(gobj);
        }

        save_geometry(gobj); // Publish an EV_UPDATE_NODE of each node
    }

    return 0;
}

/************************************************************
 *  Node created, from subscription
 ************************************************************/
function ac_node_created(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let desc_kw = kw.desc; // ignore changes in desc, by now
    let topic_name = kw.topic_name;
    let node = kw.node;

    if(!priv.descs || !priv.graph) {
        return 0;
    }

    // Handle __graphs__ creation: update _graph_properties
    if(topic_name === '__graphs__') {
        priv.__graphs__.push(node);
        build_graph_properties(gobj);
        return 0;
    }

    let desc = priv.descs[topic_name];
    if(!desc) {
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
     *  Create graph node and draw its links
     */
    history_pause(gobj);
    create_topic_node(gobj, desc, node);
    draw_links(gobj, desc, node, false);
    graph_draw(gobj).then(() => {
        history_resume(gobj);
    });

    return 0;
}

/************************************************************
 *  Node updated, from subscription.
 *
 *  Diff old vs new fkey references:
 *  - removed refs → clear those edges
 *  - added refs   → draw those edges
 *  - unchanged    → leave as-is
 ************************************************************/
function ac_node_updated(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let topic_name = kw.topic_name;
    let node = kw.node;

    if(!priv.descs || !priv.graph) {
        return 0;
    }

    // Handle __graphs__ updates: refresh _graph_properties
    if(topic_name === '__graphs__') {
        // Update the __graphs__ record in our local list
        let found = false;
        for(let i = 0; i < priv.__graphs__.length; i++) {
            if(priv.__graphs__[i].id === node.id) {
                priv.__graphs__[i] = node;
                found = true;
                break;
            }
        }
        if(!found) {
            priv.__graphs__.push(node);
        }
        build_graph_properties(gobj);
        return 0;
    }

    let desc = priv.descs[topic_name];
    if(!desc) {
        log_error(`ac_node_updated: unknown topic: ${topic_name}`);
        return 0;
    }

    let node_name = build_node_name(gobj, topic_name, node.id);
    let cols = desc.cols;

    /*
     *  Find old record
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

    /*
     *  Diff fkey references: old vs new
     */
    let old_refs = old_record ? collect_fkey_refs(desc, old_record) : new Set();
    let new_refs = collect_fkey_refs(desc, node);

    history_pause(gobj);

    // Remove edges for refs that disappeared
    for(let ref of old_refs) {
        if(!new_refs.has(ref)) {
            let [col_idx, fkey_value] = ref.split("\t");
            clear_link(gobj, topic_name, node.id, cols[parseInt(col_idx)], fkey_value, false);
        }
    }

    /*
     *  Update node data and local record
     */
    update_topic_node(gobj, desc, node_name, node);
    update_local_node(gobj, topic_name, node);

    // Draw edges for refs that appeared
    for(let ref of new_refs) {
        if(!old_refs.has(ref)) {
            let [col_idx, fkey_value] = ref.split("\t");
            draw_link(gobj, topic_name, node.id, cols[parseInt(col_idx)], fkey_value, false);
        }
    }

    graph_draw(gobj).then(() => {
        history_resume(gobj);
    });

    return 0;
}

/************************************************************
 *  Node deleted, from subscription
 ************************************************************/
function ac_node_deleted(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let topic_name = kw.topic_name;
    let node = kw.node;

    if(!priv.descs || !priv.graph) {
        return 0;
    }

    let desc = priv.descs[topic_name];
    if(!desc) {
        log_error(`ac_node_deleted: unknown topic: ${topic_name}`);
        return 0;
    }

    /*
     *  Remove node entry from __graphs__ properties and persist
     */
    let topic_props = priv._graph_properties[topic_name];
    if(topic_props && is_object(topic_props.nodes)) {
        delete topic_props.nodes[node.id];
        save_topic_graph_properties(gobj, topic_name);
    }

    /*
     *  Delete graph node and links
     */
    history_pause(gobj);
    let node_name = build_node_name(gobj, topic_name, node.id);
    clear_links(gobj, desc, node, false);
    remove_topic_node(gobj, node_name);
    remove_local_node(gobj, topic_name, node);

    graph_draw(gobj).then(() => {
        history_resume(gobj);
    });

    return 0;
}

/************************************************************
 *  Show, from parent
 ************************************************************/
function ac_show(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let $canvas_container = priv.$container;
    if(!$canvas_container) {
        return 0;
    }
    let rect = $canvas_container.getBoundingClientRect();

    if(!priv.yet_showed) {
        priv.yet_showed = true;

        graph_resize(gobj, rect.width, rect.height);
    }

    return 0;
}

/************************************************************
 *  Hide, from parent
 ************************************************************/
function ac_hide(gobj, event, kw, src)
{
    return 0;
}

/************************************************************
 *  Resize, from parent
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
    gobj_write_attr(gobj, "theme", theme);
    graph.setTheme(theme);

    graph.updatePlugin({
        key: 'grid-line',
        stroke: theme === 'dark' ? '#343434' : '#EEEEEE',
        borderStroke: theme === 'dark' ? '#656565' : '#EEEEEE',
    });

    graph_draw(gobj).then(() => {
        // Restore toolbar icon states lost when G6 re-renders the DOM
        update_history_buttons(gobj);
    });

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
    let priv = gobj.priv;
    let graph = priv.graph;

    graph.zoomTo(1);
    return 0;
}

function ac_auto_fit(gobj, event, kw, src)
{
    graph_fitview(gobj);
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

    const plugin = graph_get_plugin(gobj, 'fullscreen');
    if(plugin) {
        plugin.request();
    }
    return 0;
}

/************************************************************
 *  Layout change
 ************************************************************/
function ac_set_layout(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let layout = select_layout(gobj, kw.layout);
    graph_set_layout(gobj, layout).then(() => {
        configure_behaviour(gobj);
        update_toolbar(gobj);
    });

    return 0;
}

/************************************************************
 *  Mode change
 ************************************************************/
function ac_set_operation_mode(gobj, event, kw, src)
{
    gobj_write_attr(gobj, "operation_mode", kw.operation_mode);
    configure_behaviour(gobj);
    update_toolbar(gobj);
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
        if(nodedata && nodedata.data && nodedata.data.desc) {
            gobj_publish_event(gobj, "EV_VERTEX_CLICKED", {
                treedb_name: priv.treedb_name,
                topic_name: nodedata.data.desc.topic_name,
                record: nodedata.data.record
            });

            if(priv.edit_mode) {
                // Check if click hits a port
                // Convert client coords to viewport (container-relative) then to canvas
                let containerRect = priv.$container.getBoundingClientRect();
                let canvasPoint = graph.getCanvasByViewport([
                    kw.evt.client.x - containerRect.left,
                    kw.evt.client.y - containerRect.top
                ]);
                let port_key = detect_port_click(
                    gobj, node_id, canvasPoint[0], canvasPoint[1]
                );

                // In linking mode, try to complete the link
                if(priv._linking_mode) {
                    if(port_key && try_complete_link(gobj, node_id, port_key)) {
                        return 0;
                    }
                    // Clicked on non-valid target, cancel linking
                    exit_linking_mode(gobj);
                    return 0;
                }

                if(port_key) {
                    select_port(gobj, node_id, port_key);
                } else {
                    deselect_port(gobj);
                    select_node(gobj, node_id);
                }
            }
        }
    } catch(e) {
        // Clicked on non-node element
    }

    return 0;
}

/************************************************************
 *  Edge click - publish edge clicked event with semantic data
 ************************************************************/
function ac_edge_click(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    let edge_id = kw.evt.target.id;

    try {
        let edgedata = graph.getEdgeData(edge_id);
        if(edgedata && edgedata.data) {
            gobj_publish_event(gobj, "EV_EDGE_CLICKED", {
                treedb_name: priv.treedb_name,
                edge_id: edge_id,
                parent_topic: edgedata.data.parent_topic,
                parent_id:    edgedata.data.parent_id,
                hook_name:    edgedata.data.hook_name,
                child_topic:  edgedata.data.child_topic,
                child_id:     edgedata.data.child_id,
                fkey_name:    edgedata.data.fkey_name,
            });

            if(priv.edit_mode) {
                deselect_node(gobj);
                select_edge(gobj, edge_id);
            }
        }
    } catch(e) {
        // Clicked on non-edge element
    }

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
    let priv = gobj.priv;

    if(priv._linking_mode) {
        exit_linking_mode(gobj);
        return 0;
    }

    if(priv.edit_mode) {
        deselect_node(gobj);
    }

    return 0;
}

/************************************************************
 *  Synchronize a history command with the treedb backend.
 *
 *  Call after undo (pass cmd.original) or after redo (pass cmd.current).
 *  `cmdData` has the shape produced by G6's parseCommand():
 *      { add: { nodes?, edges? }, update: { ... }, remove: { nodes?, edges? } }
 *
 *  Only structural add/remove operations require backend sync:
 *    - add.nodes    → nodes restored by undo/redo  → EV_CREATE_NODE
 *    - remove.nodes → nodes removed  by undo/redo  → EV_DELETE_NODE
 *    - add.edges    → edges restored by undo/redo  → EV_LINK_NODES
 *    - remove.edges → edges removed  by undo/redo  → EV_UNLINK_NODES
 *
 *  update.nodes/edges are style/position-only changes; they are saved
 *  to the backend in bulk via EV_SAVE_GRAPH, not individually.
 *
 *  Ref formats expected by the backend (c_node.c):
 *    parent_ref: "topic^id^hook_name"  (decoded by decode_parent_ref())
 *    child_ref:  "topic^id"            (decoded by decode_child_ref())
 ************************************************************/
function sync_history_to_backend(gobj, cmdData)
{
    if(!cmdData) {
        return;
    }
    let priv = gobj.priv;

    /*
     * Nodes re-added by undo (undo of a delete) or redo (redo of a create).
     * node.data carries topic_name and the full treedb record,
     * set when the node was first created via create_topic_node().
     */
    (cmdData.add?.nodes || []).forEach((node) => {
        const d = node.data;
        if(!d || !d.topic_name || !d.record) {
            return;
        }
        gobj_publish_event(gobj, "EV_CREATE_NODE", {
            treedb_name: priv.treedb_name,
            topic_name:  d.topic_name,
            record:      d.record,
        });
    });

    /*
     * Nodes removed by undo (undo of a create) or redo (redo of a delete).
     */
    (cmdData.remove?.nodes || []).forEach((node) => {
        const d = node.data;
        if(!d || !d.topic_name || !d.record) {
            return;
        }
        gobj_publish_event(gobj, "EV_DELETE_NODE", {
            treedb_name: priv.treedb_name,
            topic_name:  d.topic_name,
            record:      d.record,
        });
    });

    /*
     * Edges re-added by undo (undo of an unlink) or redo (redo of a link).
     * edge.data carries the full connection info set when the edge was drawn.
     */
    (cmdData.add?.edges || []).forEach((edge) => {
        const d = edge.data;
        if(!d || !d.parent_topic || !d.parent_id || !d.hook_name ||
                 !d.child_topic  || !d.child_id) {
            return;
        }
        gobj_publish_event(gobj, "EV_LINK_NODES", {
            treedb_name: priv.treedb_name,
            parent_ref: `${d.parent_topic}^${d.parent_id}^${d.hook_name}`,
            child_ref:  `${d.child_topic}^${d.child_id}`,
        });
    });

    /*
     * Edges removed by undo (undo of a link) or redo (redo of an unlink).
     */
    (cmdData.remove?.edges || []).forEach((edge) => {
        const d = edge.data;
        if(!d || !d.parent_topic || !d.parent_id || !d.hook_name ||
                 !d.child_topic  || !d.child_id) {
            return;
        }
        gobj_publish_event(gobj, "EV_UNLINK_NODES", {
            treedb_name: priv.treedb_name,
            parent_ref: `${d.parent_topic}^${d.parent_id}^${d.hook_name}`,
            child_ref:  `${d.child_topic}^${d.child_id}`,
        });
    });
}

/************************************************************
 *  History undo/redo
 ************************************************************/
function ac_history_redo(gobj, event, kw, src)
{
    let priv = gobj.priv;

    if(priv.edit_mode) {
        const history = graph_get_plugin(gobj, "history");
        if(history && history.canRedo()) {
            /*
             * Capture the command about to be redone before the stack changes.
             * cmd.current describes what G6 will re-apply.
             */
            const cmd = history.redoStack[history.redoStack.length - 1];

            history.redo();
            update_resize_handles_position(gobj);

            /*
             * Sync backend using cmd.current: it contains the add/remove sets
             * that G6 just re-applied, mirroring what the user originally did.
             */
            sync_history_to_backend(gobj, cmd ? cmd.current : null);
        }
    }

    return 0;
}

function ac_history_undo(gobj, event, kw, src)
{
    let priv = gobj.priv;

    if(priv.edit_mode) {
        const history = graph_get_plugin(gobj, "history");
        if(history && history.canUndo()) {
            /*
             * Capture the command about to be undone before the stack changes.
             * cmd.original describes the state G6 will restore.
             */
            const cmd = history.undoStack[history.undoStack.length - 1];

            history.undo();
            update_resize_handles_position(gobj);

            /*
             * Sync backend using cmd.original: it contains the add/remove sets
             * that G6 just restored, which are the inverse of the user's action.
             */
            sync_history_to_backend(gobj, cmd ? cmd.original : null);
        }
    }

    return 0;
}

/************************************************************
 *  Fullscreen
 ************************************************************/
function ac_request_fullscreen(gobj, event, kw, src)
{
    const fullscreen = graph_get_plugin(gobj, "fullscreen");
    if(fullscreen) {
        fullscreen.request();
    }

    return 0;
}

function ac_exit_fullscreen(gobj, event, kw, src)
{
    const fullscreen = graph_get_plugin(gobj, "fullscreen");
    if(fullscreen) {
        fullscreen.exit();
    }

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
            ["EV_ZOOM_IN",                  ac_zoom_in,             null],
            ["EV_ZOOM_OUT",                 ac_zoom_out,            null],
            ["EV_ZOOM_RESET",               ac_zoom_reset,          null],
            ["EV_AUTO_FIT",                 ac_auto_fit,            null],
            ["EV_CENTER",                   ac_center,              null],
            ["EV_FULLSCREEN",               ac_fullscreen,          null],
            ["EV_SET_LAYOUT",               ac_set_layout,          null],
            ["EV_SET_OPERATION_MODE",       ac_set_operation_mode,  null],
            ["EV_SAVE_GRAPH",               ac_save_graph,          null],
            ["EV_HISTORY_UNDO",             ac_history_undo,        null],
            ["EV_HISTORY_REDO",             ac_history_redo,        null],
            ["EV_REQUEST_FULLSCREEN",       ac_request_fullscreen,  null],
            ["EV_EXIT_FULLSCREEN",          ac_exit_fullscreen,     null],

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
        ["EV_AUTO_FIT",                 0],
        ["EV_CENTER",                   0],
        ["EV_FULLSCREEN",               0],
        ["EV_SET_LAYOUT",               0],
        ["EV_SET_OPERATION_MODE",       0],
        ["EV_HISTORY_UNDO",             0],
        ["EV_HISTORY_REDO",             0],
        ["EV_SAVE_GRAPH",               0],
        ["EV_REQUEST_FULLSCREEN",       0],
        ["EV_EXIT_FULLSCREEN",          0],

        /*--- Published to parent ---*/
        ["EV_VERTEX_CLICKED",           event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_EDGE_CLICKED",             event_flag_t.EVF_OUTPUT_EVENT],
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
