/***********************************************************************
 *          c_yui_treedb_graph.js
 *
 *          Manage treedb topics with mxgraph
 *
 *          Copyright (c) 2025, ArtGins.
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
    createElement2,
    sprintf,
    kw_get_int,
    is_string,
    is_array,
    kw_get_str,
    kw_get_dict_value,
    gobj_short_name,
    gobj_read_str_attr,
    json_object_update,
    is_object,
    gobj_create_service,
    kwid_find_one_record,
    json_object_size,
    kw_set_dict_value,
    kw_clone_by_keys,
    treedb_get_field_desc,
    treedb_decoder_fkey,
    gobj_write_str_attr,
    gobj_command,
    msg_iev_get_stack,
    kw_get_dict,
    gobj_hsdata,
    msg_iev_write_key,
    log_warning,
    trace_json,
    json_object_update_missing,
    gobj_start,
    log_info,
    trace_msg,
    refresh_language,
    gobj_unsubscribe_event,
    str_in_list,
    delete_from_list,
    gobj_save_persistent_attrs,
    gobj_read_bool_attr,
    json_size,
} from "yunetas";

import {yui_toolbar} from "./yui_toolbar.js";
import {display_error_message} from "./c_yui_main.js";

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

import {t} from "i18next";

import {
    BaseLayout,
    ExtensionCategory,
    Graph,
    NodeEvent,
    CanvasEvent,
    HistoryEvent,
    GraphEvent,
    EdgeEvent,
    Circle,
    register,
} from '@antv/g6';

import {Circle as CircleGeometry} from '@antv/g';

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_TREEDB_GRAPH";

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
/*---------------- Public Attributes ----------------*/
SDATA(data_type_t.DTP_POINTER,  "subscriber",       0,  null,   "Subscriber of output events"),
SDATA(data_type_t.DTP_LIST,     "modes",            0,  '["reading", "operation", "writing", "edition"]',   "Available permission or behaviour modes"),

/*---------------- User last selections  ----------------*/
SDATA(data_type_t.DTP_STRING,   "current_mode",     sdata_flag_t.SDF_PERSIST, "", "Current mode (internal behaviour or role). Changed by the user trough the gui."),
SDATA(data_type_t.DTP_STRING,   "current_layout",   sdata_flag_t.SDF_PERSIST, "", "Current graph layout or **view**, See graph_settings.layouts for available list. User preference. Changed by the user trough the gui."),

/*---------------- Remote Connection ----------------*/
SDATA(data_type_t.DTP_POINTER,  "gobj_remote_yuno", 0,  null,   "Remote Yuno to request data"),
SDATA(data_type_t.DTP_STRING,   "treedb_name",      0,  null,   "Remote service treedb name"),
SDATA(data_type_t.DTP_DICT,     "descs",            0,  null,   "Descriptions of topics obtained"),
SDATA(data_type_t.DTP_DICT,     "records",          0,  "{}",   "Data of topics"),

/*---------------- Sub-container ----------------*/
SDATA(data_type_t.DTP_POINTER,  "$container",       0,  null,   "Container element"),
SDATA(data_type_t.DTP_STRING,   "href",             0,  "",     "Tab href"),
SDATA(data_type_t.DTP_STRING,   "label",            0,  "",     "Tab label"),
SDATA(data_type_t.DTP_STRING,   "image",            0,  "",     "Tab image"),
SDATA(data_type_t.DTP_STRING,   "icon",             0,  "fa-solid fa-question", "Tab icon"),

/*---------------- Graph General Settings ----------------*/
SDATA(data_type_t.DTP_STRING,   "canvas_id",        0,  "",     "Canvas ID"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_gridline",    0,  false,  "Use gridline plugin"),
SDATA(data_type_t.DTP_DICT,     "graph_settings",   0,  {
    layouts: {
        "manual": {
            type: 'manual',
        },
        "dagre": {
            type: 'dagre',
        },
        "antv-dagre": {
            type: 'antv-dagre',
        },
        // "dagre-with-combos": {
        //     type: 'antv-dagre',
        //     with_combo: true,
        //     ranksep: 50,
        //     nodesep: 5,
        //     sortByCombo: true,
        // },
        // "combo-combined": {
        //     type: 'combo-combined',
        //     with_combo: true,
        //     comboPadding: 2,
        // },
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
    },
}, "Default graph settings"),

SDATA(data_type_t.DTP_STRING,   "wide",                     0,  "40px", "Height of header"),
SDATA(data_type_t.DTP_STRING,   "padding",                  0,  "m-2",  "Padding or margin value"),

SDATA_END()
];

let PRIVATE_DATA = {
    _xy: 100,
    treedb_name: "",
    gobj_remote_yuno: null,
    descs: null,
    records: {},
    canvas_id: "",
    graph: null,        // Instance of G6
    __graphs__: [],     // Rows of __graphs__
    yet_showed: false,
    graph_settings: null,
    edit_mode: false,
    mode: null,
    theme: null,
};

let __gclass__ = null;

const node_colors = [
    'rgb(237, 201, 73)',
    'rgb(118, 183, 178)',
    'rgb(255, 157, 167)',
    'rgb(175, 122, 161)',
    'rgb(89, 161, 79)',
    'rgb(186, 176, 171)',
    'rgb(66, 146, 198)',
];


const node_colors2 = [ // an interesting palette: https://iamkate.com/data/12-bit-rainbow/
    "#817",
    "#a35",
    "#c66",
    "#e94",
    "#ed0",
    "#9d5",
    "#4d8",
    "#2cb",
    "#0bc",
    "#09c",
    "#36b",
    "#639",
];





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
    priv.canvas_id = "canvas" + name;
    gobj_write_str_attr(gobj, "canvas_id", priv.canvas_id);

    /*
     *  CHILD subscription model
     */
    let subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, null, {}, subscriber);

    priv.records = gobj_read_attr(gobj, "records");
    priv.treedb_name = gobj_read_str_attr(gobj, "treedb_name");
    priv.gobj_remote_yuno = gobj_read_attr(gobj, "gobj_remote_yuno");
    priv.graph_settings = gobj_read_attr(gobj, "graph_settings");

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

    switch(path) {
        case "treedb_name":
            priv.treedb_name = gobj_read_str_attr(gobj, "treedb_name");
            break;
        case "gobj_remote_yuno":
            priv.gobj_remote_yuno = gobj_read_attr(gobj, "gobj_remote_yuno");
            break;
        case "descs":
            priv.descs = gobj_read_attr(gobj, "descs");
            break;
    }
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    let priv = gobj.priv;

    request_treedb_descs(gobj);
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
 *
 ************************************************************/
function build_node_name(gobj, topic_name, id)
{
    let priv = gobj.priv;

    return sprintf("node-%s-%s-%s", priv.treedb_name, topic_name, id);
}

/************************************************************
 *
 ************************************************************/
function get_default_ne_xy(gobj, schema, record)
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
    let priv = gobj.priv;

    /*----------------------------------------------*
     *  Layout Schema
     *----------------------------------------------*/
    let padding = gobj_read_attr(gobj, "padding");
    let $toolbar = make_toolbar(gobj);

    let $container = createElement2(
        // Don't use is-flex, don't work well with is-hidden
        ['div', {class: 'graphs', style: `height:100%; display:flex; flex-direction:column;`}, [
            ['div', {class: 'is-flex-grow-0 is-flex graph_toolbar'}, $toolbar],
            ['div', {class: `is-flex-grow-1 ${padding}`, style: 'height:100%; min-height:0; overflow:hidden;'}, [
                ['div', {id: priv.canvas_id, class: `graph-container`, style: 'height:100%; min-height:0;border: 1px solid var(--bulma-border-weak);border-radius:0.2rem;'}, [
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
 *
 ************************************************************/
function make_toolbar(gobj)
{
    let priv = gobj.priv;

    /*---------------------------------------*
     *      Top Header toolbar
     *---------------------------------------*/
    let graph_settings = priv.graph_settings;
    let layouts = graph_settings.layouts;
    let layout_options = Object.keys(layouts).map(key => ['option', {}, key]);

    let modes = gobj_read_attr(gobj, "modes");
    let mode_options = modes.map(item => ['option', {}, item]);

    /*
     *  Left, layout and mode
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
        ["fa-regular fa-arrows-rotate",         "EV_REFRESH_TREEDB",false,  'i'],
    ];
    add_buttons(gobj, left_items, l_icons);

    /*
     *  Center, common controls
     */
    let center_items = [];
    let c_icons = [
        //["fa-regular fa-arrows-maximize",       "EV_FULLSCREEN",    false,  'i'],
        ["fa-regular fa-magnifying-glass-plus", "EV_ZOOM_IN",       false,  'i'],
        ["fa-regular fa-magnifying-glass",      "EV_ZOOM_RESET",    false,  'i'],
        ["fa-regular fa-magnifying-glass-minus","EV_ZOOM_OUT",      false,  'i'],
        ["fa-regular fa-arrows-to-eye",         "EV_CENTER",        false,  'i'],
    ];

    add_buttons(gobj, center_items, c_icons);

    /*
     *  Right, fill in set_mode
     */
    let right_items = [
    ];

    const $toolbar_header = yui_toolbar({}, [
        ['div', {class: 'yui-horizontal-toolbar-section left'}, left_items],
        ['div', {class: 'yui-horizontal-toolbar-section center'}, center_items],
        ['div', {class: 'yui-horizontal-toolbar-section right mode_buttons'}, right_items]
    ]);

    refresh_language($toolbar_header, t);

    return $toolbar_header;
}

/************************************************************
 *
 ************************************************************/
function register_layouts(gobj)
{
    register(ExtensionCategory.LAYOUT, 'manual', ManualLayout);
}

/************************************************************
 *
 ************************************************************/
function register_nodes(gobj)
{
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
        container: priv.canvas_id,
        animation: false,
        autoResize: true,
        // autoFit: 'view', // TODO da error, check it
        // padding: 50,
        zoomRange: [0.2, 4],

        /*
         *  Common and prevalence configuration over nodes, edges, combos
         */

        node: {  // WARNING this affect to all nodes with prevalence over individual defines!
            palette: {
                type: 'group',
                color: 'tableau',
                field: 'topic_name',
            },
        },

        edge: { // WARNING this affect to all edges with prevalence over individual defines!
            style: {
                startArrow: true,   // HACK target/source interchanged
                endArrow: false,
            },
        },

        plugins: [
            // { TODO review, the menu remains when is in fullscreen
            //     type: 'fullscreen',
            //     key: 'fullscreen',
            // },
        ],
    });

    /*--------------------------------*
     *          Set theme
     *--------------------------------*/
    graph.setTheme(priv.theme);

    let mode = select_mode(gobj);
    set_mode(gobj, mode);

    graph_set_layout(gobj, layout).then(() => {
        configure_events(gobj);
        //show_positions(gobj);
    });
}

/************************************************************
 *
 ************************************************************/
function configure_events(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    /*--------------------------------*
     *      Configure events
     *--------------------------------*/
    graph.on(CanvasEvent.CLICK, (evt) => {
        gobj_send_event(gobj, "EV_CANVAS_CLICK", {evt: evt}, gobj);
    });

    // graph.on(NodeEvent.DRAG_START, (evt) => {
    // });

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

    // graph.on(GraphEvent.BATCH_START, (evt) => {
    //     gobj_send_event(gobj, "EV_GRAPH_BATCH_START", {evt: evt}, gobj);
    // });
    // graph.on(GraphEvent.BATCH_END, (evt) => {
    //     gobj_send_event(gobj, "EV_GRAPH_BATCH_END", {evt: evt}, gobj);
    // });

    // graph.once(GraphEvent.AFTER_RENDER, (evt) => {
    //     /*
    //      *  Some event or plugins has to be configured after render
    //      */
        do_extra_configuration(gobj);
    // });
}

/************************************************************
 *
 ************************************************************/
function do_extra_configuration(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    /*--------------------------------*
     *      Configure plugins
     *--------------------------------*/
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

    // graph_add_plugin(
    //     gobj,
    //     'contextmenu',
    //     {
    //         trigger: 'contextmenu', // 'click' or 'contextmenu'
    //         onClick: (v) => {
    //             trace_msg('You have clicked the「' + v + '」item');
    //         },
    //         getItems: () => {
    //             return [
    //                 { name: 'Spread', value: 'spread' },
    //                 { name: 'Detail', value: 'detail' },
    //             ];
    //         },
    //         enable: (e) => e.targetType === 'node',
    //     }
    // );
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
 *
 ************************************************************/
function add_buttons(gobj, zone, c_icons)
{
    let toolbar_wide = gobj_read_attr(gobj, "wide");

    for(let i = 0; i < c_icons.length; i++) {
        let item = c_icons[i];
        let icon_name = item[0];
        let event_name = item[1];
        let disabled = item[2];
        let type = item[3];

        switch(type) {
            default:
            case 't': {
                let button = {
                    class: `button ${event_name}`,
                    style: {
                        height: `${toolbar_wide}`,
                        // width: `2.5em`
                    }
                };
                if(disabled) {
                    button.disabled = true;
                }
                zone.push(
                    ['button',
                        button,
                        `${icon_name}`,
                        {
                            /*
                             *  the anonymous event handler will be removed and
                             *  cleaned up when the element is removed via innerHTML = '',
                             *  as long as nothing else holds a reference to it.
                             */
                            click: (evt) => {
                                //
                                evt.stopPropagation();
                                gobj_send_event(gobj, `${event_name}`, {evt: evt}, gobj);
                            },
                            contextmenu: (evt) => {
                                evt.stopPropagation();
                                evt.preventDefault();
                                gobj_send_event(gobj, `${event_name}`, {evt: evt}, gobj);
                            }
                        }
                    ]
                );
            }
            break;
            case 'i': {
                let button = {
                    class: `button ${event_name}`,
                    style: {
                        height: `${toolbar_wide}`,
                        width: `2.5em`
                    }
                };
                if(disabled) {
                    button.disabled = true;
                }
                zone.push(
                    ['button',
                        button,
                        ['i', {
                            // id: 'icon_yuneta_state',
                            style: `font-size:1.5em; color:inherit;`,
                            class: `${icon_name}`
                        }], {
                            /*
                             *  the anonymous event handler will be removed and
                             *  cleaned up when the element is removed via innerHTML = '',
                             *  as long as nothing else holds a reference to it.
                             */
                            click: (evt) => {
                                evt.stopPropagation();
                                gobj_send_event(gobj, `${event_name}`, {evt: evt}, gobj);
                            },
                            contextmenu: (evt) => {
                                evt.stopPropagation();
                                evt.preventDefault();
                                gobj_send_event(gobj, `${event_name}`, {evt: evt}, gobj);
                            }
                        }
                    ]
                );
            }
            break;
        }
    }
}

/************************************************************
 *
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
    $input.value = layout_name;

    return graph_settings.layouts[layout_name];
}

/************************************************************
 *
 ************************************************************/
function select_mode(gobj, mode_name)
{
    let priv = gobj.priv;

    let $container = gobj_read_attr(gobj, "$container");

    if(!mode_name) {
        mode_name = gobj_read_str_attr(gobj, "current_mode");
    }
    if(!mode_name) {
        mode_name = gobj_read_attr(gobj, "modes")[0];
    }

    gobj_write_str_attr(gobj, "current_mode", mode_name);

    let $input = $container.querySelector('.graph_mode');
    $input.value = mode_name;

    return mode_name;
}

/************************************************************
 *  set mode:
 *      'reading':      A authorized user can read
 *      'operation':    A authorized user can operate
 *      'writing':      A authorized user can modify (settings by example)
 *      'edition':      A authorized user can edit (change design by example)
 ************************************************************/
function set_mode(gobj, mode)
{
    let $container = gobj_read_attr(gobj, "$container");

    let $zone = $container.querySelector('.mode_buttons');
    removeChildElements($zone); // Remove all buttons of mode options

    if(!mode || !str_in_list(["reading","operation","writing","edition"], mode)) {
        mode = 'reading';
    }

    let behaviors = [];

    switch(mode) { // HACK remember, the first has more prevalence
        case 'edition':
            behaviors = [
                'drag-canvas', // TODO repon
                'zoom-canvas',
            ];
            break;
        case 'writing':
            behaviors = [
                'drag-canvas', // TODO repon
                'zoom-canvas',
            ];
            break;
        case 'operation':
            break;
        case 'reading':
            behaviors = [
                'drag-canvas', // TODO repon
                'zoom-canvas',
            ];
            break;
    }
    graph_write_behaviors(gobj, behaviors);

    let c_icons = [];
    switch(mode) {
        case 'reading':
            c_icons = [
            ];
            break;
        case 'operation':
            c_icons = [
            ];
            break;
        case 'writing':
            c_icons = [
            ];
            break;
        case 'edition':
            c_icons = [
                ["fa-solid fa-pen ",                    "EV_EDIT_MODE",     false,  'i'],
                ["fa-solid fa-plus-large",              "EV_NEW",           true,   'i'],
                ["fa-regular fa-arrow-rotate-left",     "EV_HISTORY_UNDO",  true,   'i'],
                ["fa-regular fa-arrow-rotate-right",    "EV_HISTORY_REDO",  true,   'i'],
                ["fa-solid fa-save ",                   "EV_SAVE_GRAPH",    true,   'i'],
            ];
            break;
    }

    if(c_icons.length) {
        let right_items = [];
        add_buttons(gobj, right_items, c_icons);
        for (let item of right_items) {
            $zone.appendChild(createElement2(item));
        }
    }
}

/**
 * Returns the proportional position (between 0 and 1) of a specific point,
 * centered and spaced with margins.
 *
 * @param {number} index - Index of the point (0 to count-1)
 * @param {number} count - Total number of points
 * @param {number} margin - Total margin space (default 0.2 means 10% on each end)
 * @returns {number} Position between 0 and 1
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
 *
 ************************************************************/
function calculate_hooks_fkeys_counter(schema)
{
    /*
     *  Count the hooks/fkeys
     */
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
        // hierarchical node
        schema.node_treedb_type = 'hierarchical';
    }
}

/************************************************************
 *
 ************************************************************/
function create_combo(gobj, schema)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    if(schema.hooks_counter === 0 ||    // pure child
            schema.fkeys_counter === 0) { // extension type
        graph.addComboData([{
            id: 'combo-' + schema.topic_name,
            type: 'rect',
            style: {
                // radius: 8,
                labelText: schema.topic_name,
            }
        }]);
    }
}

/************************************************************
 *
 ************************************************************/
function build_ports(gobj, schema)
{
    let priv = gobj.priv;

    let top_ports = [];
    let bottom_ports = [];

    /*
     *  Get the hooks/fkeys
     */
    let cols = schema.cols;
    for(let i=0; i<cols.length; i++) {
        let col = cols[i];
        const field_desc = treedb_get_field_desc(col);
        let port = null;
        switch(field_desc.type) {
            case "hook":
                let child_schema = null;
                let child_schema_name =  Object.keys(col.hook)[0];
                if(child_schema_name) {
                    child_schema = priv.descs[child_schema_name];
                }
                port = {
                    key: col.id,
                    fill: child_schema?child_schema.color:schema.color,     // Fill color
                    stroke: getStrokeColor(schema.color),   // Stroke color
                };
                bottom_ports.push(port);
                break;
            case "fkey":
                port = {
                    key: col.id,
                    fill: schema.color,     // Fill color
                    stroke: getStrokeColor(schema.color),   // Stroke color
                };
                top_ports.push(port);
                break;
        }
    }

    /*
     *  Place the ports
     */
    for(let i=0; i<top_ports.length; i++) {
        let port = top_ports[i];
        let point = getPointPosition(top_ports.length, i);
        port.placement = [point, 0];
        port.placement = [0.5, 0]; // all from same top point
    }
    for(let i=0; i<bottom_ports.length; i++) {
        let port = bottom_ports[i];
        let point = getPointPosition(bottom_ports.length, i);
        if(top_ports.length === 0) {
            port.placement = [0, 0.5];
        } else {
            port.placement = [point, 1];
        }
    }

    return [...top_ports, ...bottom_ports];
}

/************************************************************
 *  Create topic node, from backend
 ************************************************************/
function create_topic_node(gobj, schema, record)
{
    let priv = gobj.priv;
    const graph = priv.graph;

    /*------------------------------------------*
     *  Creating filled cell from backend data
     *------------------------------------------*/
    let node_name = build_node_name(gobj, schema.topic_name, record.id);
    let xy = get_default_ne_xy(gobj);
    let geometry = record._geometry || {};
    let x = kw_get_int(gobj, geometry, "x", xy, kw_flag_t.KW_CREATE);
    let y = kw_get_int(gobj, geometry, "y", xy, kw_flag_t.KW_CREATE);

    let topic_graphs = kwid_find_one_record( // TODO review
        gobj,
        priv.__graphs__,
        null,
        {
            topic: schema.topic_name,
            active: true
        }
    );

    //log_warning(`create node ==> ${node_name}`);

    let ports = build_ports(gobj, schema);

    let node_graph_type = null;
    let node_treedb_type = schema.node_treedb_type;
    let combo = null;

    let style = {
        x: x,
        y: y,
        fill: schema.color,     // Fill color
        stroke: getStrokeColor(schema.color),   // Stroke color
        lineWidth: 1,           // Stroke width
        //labelText: schema.topic_name + "^" + record.id,
    };


    if(node_treedb_type === 'child') {
        // Pure childs
        node_graph_type = 'circle';
        style.size = [60];
        combo = 'combo-' + schema.topic_name;
        json_object_update_missing(style, {
            labelText: record.id,
            labelPlacement: 'center',
            labelWordWrap: true,
            labelMaxWidth: "100%",
            // iconText: record.id,
            // iconSrc: record.icon,
            // iconFontSize: 14,
        });

    } else if(node_treedb_type === 'extended') {
        // node_graph_type = 'rect'; // Don't draw extended nodes (hooks > 0 && fkeys == 0)
        style.size = [80,60];
        combo = 'combo-' + schema.topic_name;

    } else {
        // hierarchical node
        node_graph_type = 'html';
        style.size = [150,100];
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
    justify-content: center;      /* centers vertically */
    align-items: center;          /* centers horizontally */
    padding: 10px;
">
    <div>
        <span class="icon is-large">
        <img src="${record.icon?record.icon:''}" alt=""/>
        </span>
    </div>
    <div style="font-weight: bold;">
      ${record.id}
    </div>
</div>
`;
    }

    let node_def = {
        id: node_name,

        type: node_graph_type,
        style: style,

        data: {
            // This 4 keys are available in user `data` of G6 Node.
            topic_name: schema.topic_name,
            schema: schema,
            record: record
        }
    };
    if(combo) {
        // No combo by now
        // node_def.combo = combo;
    }

    if(json_size(ports)) {
        json_object_update_missing(style, {
            port: true,
            ports: ports,
            portR: node_treedb_type==='child'?2:6,
            portLineWidth: 1,
        });
    }
    // TODO add_state_overlays(gobj, graph, cell);

    if(node_graph_type) {
        graph.addNodeData([node_def]);
    }
}

/************************************************************
 *  create links and render graph
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
    graph_render(gobj);  // must be the last instruction or use await or .then()
}


/************************************************************
 *  Draw links
 ************************************************************/
function draw_links(gobj, schema, record, initial_load)
{
    let topic_name = schema.topic_name;
    let record_id = record.id;

    let cols = schema.cols;
    for(let i=0; i<cols.length; i++) {
        let col = cols[i];
        if(!col.fkey) {
            continue;
        }

        let fkeys = record[col.id];

        if(fkeys) {
            if(is_string(fkeys)) {
                // log_warning(`LINK1 ${topic_name}^${record_id}^${col.id} -> ${fkeys}`);
                draw_link(gobj, topic_name, record_id, col, fkeys, initial_load);

            } else if(is_array(fkeys)) {
                for(let j=0; j<fkeys.length; j++) {
                    // log_warning(`LINK2 ${topic_name}^${record_id}^${col.id} -> ${JSON.stringify(fkeys[j])}`);
                    draw_link(gobj, topic_name, record_id, col, fkeys[j], initial_load);
                }
            } else {
                log_error(`${gobj_short_name(gobj)}: fkey type unsupported: ` + JSON.stringify(fkeys));
            }
        }
    }
}

/************************************************************
 *  Draw link
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

    /*-----------------------------------------*
     *  Decode fkey: the link to the target
     *  and get the target node name
     *-----------------------------------------*/
    let target_fkey = treedb_decoder_fkey(source_col, fkey);
    if(!target_fkey) {
        log_error("What f*?");
        return;
    }
    let target_topic_name = target_fkey.topic_name;
    let source_schema = priv.descs[target_topic_name];
    if(source_schema.node_treedb_type === 'extended') {
        return;
    }
    let target_topic_id = target_fkey.id;
    let target_hook = target_fkey.hook_name;  // the target port

    let target_node_name = build_node_name(gobj, target_topic_name, target_topic_id);

    /*-----------------------------------------*
     *  target node must exits
     *-----------------------------------------*/
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

    /*----------------------------------------------------------*
     *  Me: the source node.
     *  As a child but we have the info to link to the parent
     *----------------------------------------------------------*/
    let source_node_name = build_node_name(gobj, source_topic_name, source_topic_id);
    let style = graph.getElementRenderStyle(source_node_name);

    /*--------------------*
     *  Create the link
     *--------------------*/
    let edge = {
        type: 'cubic',
        source: target_node_name,   // HACK target/source interchanged
        target: source_node_name,
        // states: ['active'],
        style: {
            sourcePort: target_hook,    // HACK target/source interchanged
            targetPort: source_col.id,
            lineWidth: 2,
            stroke: style.fill,
        }
    };

    try {
        graph.addEdgeData([edge]); // no return
    } catch(e) {
        if(verbose) {
            log_error(e.message);
        }
    }
}

/************************************************************
 *  Clear links
 ************************************************************/
function clear_links(gobj, schema, record, initial_load)
{
    let topic_name = schema.topic_name;
    let record_id = record.id;

    let cols = schema.cols;
    for(let i=0; i<cols.length; i++) {
        let col = cols[i];
        if(!col.fkey) {
            continue;
        }

        let fkeys = record[col.id];

        if(fkeys) {
            if(is_string(fkeys)) {
                // log_warning(`UNLINK1 ${topic_name}^${record_id}^${col.id} -> ${fkeys}`);
                clear_link(gobj, topic_name, record_id, col, fkeys, initial_load);

            } else if(is_array(fkeys)) {
                for(let j=0; j<fkeys.length; j++) {
                    // log_warning(`UNLINK2 ${topic_name}^${record_id}^${col.id} -> ${JSON.stringify(fkeys[j])}`);
                    clear_link(gobj, topic_name, record_id, col, fkeys[j], initial_load);
                }
            } else {
                log_error(`${gobj_short_name(gobj)}: fkey type unsupported: ` + JSON.stringify(fkeys));
            }
        }
    }

}

/************************************************************
 *  Clear link
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

    /*-----------------------------------------*
     *  Decode fkey: the link to the target
     *  and get the target node name
     *-----------------------------------------*/
    let target_fkey = treedb_decoder_fkey(source_col, fkey);
    if(!target_fkey) {
        log_error("What f*?");
        return;
    }
    let target_topic_name = target_fkey.topic_name;
    let target_topic_id = target_fkey.id;
    let target_hook = target_fkey.hook_name;  // the target port

    let target_node_name = build_node_name(gobj, target_topic_name, target_topic_id);

    /*-----------------------------------------*
     *  target node must exits
     *-----------------------------------------*/
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

    /*----------------------------------------------------------*
     *  Me: the source node.
     *  As a child but we have the info to link to the parent
     *----------------------------------------------------------*/
    let source_node_name = build_node_name(gobj, source_topic_name, source_topic_id);

    /*-------------------------------*
     *  Recover the link and remove
     *-------------------------------*/
    try {
        // If not specified, `id` is automatically generated with the format ${source}-${target}
        let edge_id = `${source_node_name}-${target_node_name}`;

        // id in the array must exist, otherwise an exception will be thrown
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
 *
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
 *
 ************************************************************/
function get_col(cols, col_id)
{
    if(is_array(cols))  {
        for(let i=0; i<cols.length; i++) {
            let col = cols[i];
            if(col.id === col_id) {
                return col;
            }
        }
    } else if(is_object(cols)) {
        return cols[col_id];
    }
    return null;
}

/************************************************************
 *  Command to remote service
 ************************************************************/
function treedb_create_node(gobj, treedb_name, topic_name, record, options)
{
    let priv = gobj.priv;

    let command = "create-node";

    let kw = {
        service: treedb_name,
        treedb_name: treedb_name,
        topic_name: topic_name,
        record: record,
        options: options || {}
    };

    kw.__md_command__ = { // Data to be returned
        topic_name: topic_name,
    };

    let ret = gobj_command(priv.gobj_remote_yuno,
        command,
        kw,
        gobj
    );
    if(ret) {
        log_error(ret);
    }
}

/************************************************************
 *  Command to remote service
 ************************************************************/
function treedb_update_node(gobj, treedb_name, topic_name, record, options)
{
    let priv = gobj.priv;
    let command = "update-node";

    let kw = {
        service: treedb_name,
        treedb_name: treedb_name,
        topic_name: topic_name,
        record: record,
        options: options || {}
    };

    kw.__md_command__ = { // Data to be returned
        topic_name: topic_name,
    };

    let ret = gobj_command(priv.gobj_remote_yuno,
        command,
        kw,
        gobj
    );
    if(ret) {
        log_error(ret);
    }
}

/************************************************************
 *  Command to remote service
 ************************************************************/
function treedb_delete_node(gobj, treedb_name, topic_name, record, options)
{
    let priv = gobj.priv;
    let command = "delete-node";

    let kw = {
        service: treedb_name,
        treedb_name: treedb_name,
        topic_name: topic_name,
        record: record,
        options: options || {}
    };

    kw.__md_command__ = { // Data to be returned
        topic_name: topic_name,
    };

    let ret = gobj_command(priv.gobj_remote_yuno,
        command,
        kw,
        gobj
    );
    if(ret) {
        log_error(ret);
    }
}

/************************************************************
 *  Command to remote service
 ************************************************************/
function treedb_link_nodes(
    gobj,
    treedb_name,
    parent_ref,
    child_ref,
    options
)
{
    let priv = gobj.priv;
    let command = "link-nodes";

    let kw = {
        service: treedb_name,
        treedb_name: treedb_name,
        parent_ref: parent_ref,
        child_ref: child_ref,
        options: options || {}
    };

    let ret = gobj_command(priv.gobj_remote_yuno,
        command,
        kw,
        gobj
    );
    if(ret) {
        log_error(ret);
    }
}

/************************************************************
 *  Command to remote service
 ************************************************************/
function treedb_unlink_nodes(
    gobj,
    treedb_name,
    parent_ref,
    child_ref,
    options
)
{
    let priv = gobj.priv;

    let command = "unlink-nodes";

    let kw = {
        service: treedb_name,
        treedb_name: treedb_name,
        parent_ref: parent_ref,
        child_ref: child_ref,
        options: options || {}
    };

    let ret = gobj_command(priv.gobj_remote_yuno,
        command,
        kw,
        gobj
    );
    if(ret) {
        log_error(ret);
    }
}

/************************************************************
 *  Command to remote service
 ************************************************************/
function request_treedb_descs(gobj)
{
    let priv = gobj.priv;

    if(!priv.gobj_remote_yuno) {
        log_error(`${gobj_short_name(gobj)}: No gobj_remote_yuno defined`);
        return;
    }

    let command = "descs";

    let kw = {
        service: priv.treedb_name,
        treedb_name: priv.treedb_name
    };

    let ret = gobj_command(priv.gobj_remote_yuno,
        command,
        kw,
        gobj
    );
    if(ret) {
        log_error(ret);
    }
}

/************************************************************
 *  Response of remote command
 ************************************************************/
function process_treedb_descs(gobj)
{
    let descs = gobj_read_attr(gobj, "descs");

    register_nodes(gobj);

    /*
     *  descs is a dict: { __snaps__: {…}, roles: {…}, users: {…} }
     */

    /*
     *  Calculate hook fkeys size and color
     */
    let idx = 0;
    for (const [topic_name, desc] of Object.entries(descs)) {
        calculate_hooks_fkeys_counter(desc);
        if(topic_name.substring(0, 2) === "__") {
            continue;   // ignore system topics
        }
        desc.color = node_colors[idx % node_colors.length];
        idx++;
    }

    /*
     *  System topics
     *  Get firstly __graphs__, it contains data to personalize graph nodes
     */
    for(const topic_name of Object.keys(descs)) {
        if(topic_name.substring(0, 2) === "__") {
            /*
             *  Only get __graphs__
             */
            if(topic_name === '__graphs__') {
                get_nodes(gobj, topic_name);
            }
        }
    }

    /*
     *  User topics
     */
    for (const [topic_name, desc] of Object.entries(descs)) {
        if(topic_name.substring(0, 2) === "__") {
            continue;   // ignore system topics
        }
        //create_combo(gobj, desc);
        get_nodes(gobj, topic_name);
    }
}

/************************************************************
 *
 ************************************************************/
function subscribe_treedb(gobj, topic_name)
{
    let priv = gobj.priv;
    const gobj_remote_yuno = priv.gobj_remote_yuno;
    const treedb_name = priv.treedb_name;

    gobj_subscribe_event(gobj_remote_yuno,
        "EV_TREEDB_NODE_CREATED",
        {
            __service__: treedb_name,
            __filter__: {
                "treedb_name": treedb_name,
                "topic_name": topic_name
            }
        },
        gobj
    );
    gobj_subscribe_event(gobj_remote_yuno,
        "EV_TREEDB_NODE_UPDATED",
        {
            __service__: treedb_name,
            __filter__: {
                "treedb_name": treedb_name,
                "topic_name": topic_name
            }
        },
        gobj
    );
    gobj_subscribe_event(gobj_remote_yuno,
        "EV_TREEDB_NODE_DELETED",
        {
            __service__: treedb_name,
            __filter__: {
                "treedb_name": treedb_name,
                "topic_name": topic_name
            }
        },
        gobj
    );
}

/************************************************************
 *
 ************************************************************/
function unsubscribe_treedb(gobj, topic_name)
{
    let priv = gobj.priv;
    const gobj_remote_yuno = priv.gobj_remote_yuno;
    const treedb_name = priv.treedb_name;

    gobj_unsubscribe_event(gobj_remote_yuno,
        "EV_TREEDB_NODE_CREATED",
        {
            __service__: treedb_name,
            __filter__: {
                "treedb_name": treedb_name,
                "topic_name": topic_name
            }
        },
        gobj
    );
    gobj_unsubscribe_event(gobj_remote_yuno,
        "EV_TREEDB_NODE_UPDATED",
        {
            __service__: treedb_name,
            __filter__: {
                "treedb_name": treedb_name,
                "topic_name": topic_name
            }
        },
        gobj
    );
    gobj_unsubscribe_event(gobj_remote_yuno,
        "EV_TREEDB_NODE_DELETED",
        {
            __service__: treedb_name,
            __filter__: {
                "treedb_name": treedb_name,
                "topic_name": topic_name
            }
        },
        gobj
    );

}

/************************************************************
 *
 ************************************************************/
function get_nodes(gobj, topic_name)
{
    let priv = gobj.priv;
    const treedb_name = priv.treedb_name;

    // unsubscribe_treedb(gobj, topic_name);
    subscribe_treedb(gobj, topic_name);

    /*
     *  Get data
     */
    treedb_nodes(
        gobj,
        treedb_name,
        topic_name,
        {
            list_dict: true
        }
    );
}

/************************************************************
 *  Command to remote service
 *  Get nodes of a topic
 ************************************************************/
function treedb_nodes(gobj, treedb_name, topic_name, options)
{
    let priv = gobj.priv;

    const gobj_remote_yuno = priv.gobj_remote_yuno;

    let command = "nodes";

    let kw = {
        service: treedb_name,
        treedb_name: treedb_name,
        topic_name: topic_name,
        options: options || {}
    };

    kw.__md_command__ = { // Data to be returned
        topic_name: topic_name,
    };

    let ret = gobj_command(gobj_remote_yuno,
        command,
        kw,
        gobj
    );
    if(ret) {
        log_error(ret);
    }
}

/************************************************************
 *  Got nodes of a topic, response to command "nodes"
 *  Supposing that the graph is empty.
 ************************************************************/
function process_command_nodes(gobj, kw_command, data)
{
    let priv = gobj.priv;

    let topic_name = kw_get_str(
        gobj, kw_command, "topic_name", "", kw_flag_t.KW_REQUIRED
    );
    let schema = priv.descs[topic_name];

    if (topic_name.substring(0, 2) === "__") {
        if(topic_name === '__graphs__') {
            // save style
            priv.__graphs__ = data;
        }
        return;   // ignore system topics
    }

    //log_warning(`create graph treedb ${treedb_name}, topic ${topic_name}`);

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
        if(priv.graph) {
            create_topic_node(gobj, schema, record);
        }
    }

    /*--------------------------------------------------*
     *  Check if all topics loaded to make links
     *--------------------------------------------------*/
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
        graph_render(gobj).then(() => {
            create_links(gobj); // create links and render graph
        });
    }
}

/************************************************************
 *
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

        json_object_update( // Add me
            _geometry,
            {
                __origin__: gobj_read_str_attr(__yuno__, "node_uuid")
            }
        );

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
        // TODO make a api with multi-records
        gobj_send_event(gobj, "EV_UPDATE_RECORD", kw_update, gobj);
    }
}

/************************************************************
 *
 ************************************************************/
function refresh_data(gobj)
{
    clear_graph(gobj).then(() => {
        request_treedb_descs(gobj);
    });
}

/************************************************************
 *  graph function
 ************************************************************/
async function graph_center(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    await graph.fitCenter();
    await graph.fitView();
}

/************************************************************
 *  graph function
 ************************************************************/
async function graph_zoom_in(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    const z = graph.getZoom();
    await graph.zoomTo(z * 1.1);
}

/************************************************************
 *  graph function
 ************************************************************/
async function graph_zoom_out(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    const z = graph.getZoom();
    await graph.zoomTo(z * 0.9);
}

/************************************************************
 *  graph function
 *  reset zoom and pan
 ************************************************************/
async function graph_reset(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    if(graph.rendered) {
        await graph.zoomTo(1);              // Reset zoom
        await graph.translateTo([0, 0]);     // Reset pan
    }
}

/************************************************************
 *  graph function
 ************************************************************/
async function graph_render(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    await graph.render();
}

/************************************************************
 *  graph function
 ************************************************************/
function graph_add_plugin(gobj, plugin_key, options)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    /*
     *  Add plugin
     */
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
                plugin.emitter.on(HistoryEvent.ADD, (evt) => {
                    update_history_buttons(gobj);
                });
            }
            break;
    }
}

/************************************************************
 *  graph function
 ************************************************************/
function graph_remove_plugin(gobj, plugin_key, verbose)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    const plugin = graph.getPluginInstance(plugin_key);
    if(plugin) {
        let plugins = graph.getPlugins();
        plugins = plugins.filter((plugin) => plugin.key !== plugin_key);
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

/************************************************************
 *  graph function
 *  clear all: nodes, edges and combos
 ************************************************************/
async function clear_graph(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    let mode = select_mode(gobj);
    set_mode(gobj, mode);

    priv._xy= 100;
    priv.yet_showed = false;
    priv.edit_mode = false;

    graph_remove_plugin(gobj, 'history');
    update_history_buttons(gobj);

    await graph.clear();      // Clears nodes, edges, states, combos, etc.

    let layout = select_layout(gobj);
    await graph_set_layout(gobj, layout); // it will call graph.render()
}

/************************************************************
 *
 ************************************************************/
async function graph_set_layout(gobj, layout)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    graph.setLayout(layout);
    await graph_reset(gobj);
    await graph_render(gobj);
}

/************************************************************
 *
 ************************************************************/
function graph_resize(gobj, width, height)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    graph.setSize(width, height);
}

/************************************************************
 *  Write behaviors, set new and forget the previous
 ************************************************************/
function graph_write_behaviors(gobj, behaviors)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    graph.setOptions({
        behaviors: behaviors
    });
}

/************************************************************
 *  Change (set or reset) a behavior
 ************************************************************/
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
 *
 ************************************************************/
function update_history_buttons(gobj)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    let $container = gobj_read_attr(gobj, "$container");

    if(priv.edit_mode || true) {
        const history = graph.getPluginInstance('history');
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
}

/************************************************************
 *  Inner Element
 ************************************************************/
class LightNode extends Circle
{
    render(attributes, container) {
        super.render(attributes, container);
        this.upsert('light', CircleGeometry, { r: 8, fill: '#0f0', cx: 0, cy: -25 }, container);
    }
}

/************************************************************
 *  Treedb layout
 ************************************************************/
class ManualLayout extends BaseLayout {
    async execute(data, options) {
        /*
         *  data: {nodes:[], edges:[], combos:[]}
         */
        const { nodes = [] } = data;
        return {
            nodes: nodes.map((node, index) => ({
                id: node.id,
                style: {
                    x: node.data.record._geometry?node.data.record._geometry.x:0,
                    y: node.data.record._geometry?node.data.record._geometry.y:0,
                },
            })),
        };
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/************************************************************
 *  Remote response
 ************************************************************/
function ac_mt_command_answer(gobj, event, kw, src)
{
    let webix_msg = kw;

    let result;
    let comment;
    let schema;
    let data;

    try {
        result = webix_msg.result;
        comment = webix_msg.comment;
        schema = webix_msg.schema;  // What about controlling if version has changed?
        data = webix_msg.data;
    } catch (e) {
        log_error(e);
        return;
    }
    if(result < 0) {
        display_error_message(
            "Error",
            t(comment)
        );
        // HACK don't return, pass errors when need it.
    }

    let __command__  = msg_iev_get_stack(gobj, kw, "command_stack", true);
    let command = kw_get_str(gobj, __command__, "command", "", kw_flag_t.KW_REQUIRED);
    let kw_command = kw_get_dict(gobj, __command__, "kw", {}, kw_flag_t.KW_REQUIRED);

    switch(command) {
        case "descs":
            if(result >= 0) {
                gobj_write_attr(gobj, "descs", data);
                process_treedb_descs(gobj);
            }
            break;

        case "nodes":
            if(result >= 0) {
                /*
                 *  Here it could update cols of `descs`
                 *  seeing if they have changed in schema argument (controlling the version?)
                 *  Now the schema pass to creation of nodes is get from `descs`.
                 */
                process_command_nodes(gobj, kw_command, data);
            }
            break;

        case "create-node":
        case "update-node":
        case "delete-node":
        case "link-nodes":
        case "unlink-nodes":
            // Don't process here, process on subscribed events.
            break;

        default:
            log_error(`${gobj_short_name(gobj)} Command unknown: ${command}`);
    }

    return 0;
}

/********************************************
 *  Remote subscription response
 ********************************************/
function ac_treedb_node_created(gobj, event, kw, src)
{
    let treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    let topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    let node = kw_get_dict_value(gobj, kw, "node", null, 0);

    // let cell = create_topic_node(gobj, kw.schema, kw.node);
    // add_state_overlays(gobj, graph, cell);
    // draw_links(gobj, cell);

    // if(treedb_name === priv.treedb_name) {
    //     gobj.config.gobj_sw_graph.gobj_send_event(
    //         "EV_NODE_CREATED",
    //         {
    //             schema: schema,
    //             topic_name: topic_name,
    //             node: node
    //         },
    //         gobj
    //     );
    // }

    return 0;
}

/********************************************
 *  Remote subscription response
 ********************************************/
function ac_treedb_node_updated(gobj, event, kw, src)
{
    let treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    let topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    let node = kw_get_dict_value(gobj, kw, "node", null, 0);

    // let cell_name = build_cell_name(gobj, kw.topic_name, kw.node.id);
    // let cell = model.getCell(cell_name);
    // if(!cell) {
    //     log_error("ac_node_updated: cell not found");
    //     return -1;
    // }
    //
    // clear_links(gobj, cell);
    // update_topic_cell(gobj, cell, kw.node);
    // update_geometry(gobj, cell, kw.node._geometry);
    // draw_links(gobj, cell);


    // if(treedb_name === priv.treedb_name) {
    //     gobj.config.gobj_sw_graph.gobj_send_event(
    //         "EV_NODE_UPDATED",
    //         {
    //             topic_name: topic_name,
    //             node: node
    //         },
    //         gobj
    //     );
    // }

    return 0;
}

/********************************************
 *  Remote subscription response
 ********************************************/
function ac_treedb_node_deleted(gobj, event, kw, src)
{
    let treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    let topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    let node = kw_get_dict_value(gobj, kw, "node", null, 0);

    // let cell_name = build_cell_name(gobj, kw.topic_name, kw.node.id);
    // let cell = model.getCell(cell_name);
    // if(!cell) {
    //     log_error("ac_node_deleted: cell not found");
    //     return -1;
    // }
    //
    // graph.removeCells([cell]);

    // if(treedb_name === priv.treedb_name) {
    //     gobj.config.gobj_sw_graph.gobj_send_event(
    //         "EV_NODE_DELETED",
    //         {
    //             topic_name: topic_name,
    //             node: node
    //         },
    //         gobj
    //     );
    // }

    return 0;
}

/********************************************
 *  Event from gobj graph
 *  kw: {
 *      topic_name,
 *      record
 *  }
 *  Send to backend
 ********************************************/
function ac_create_record(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let treedb_name = priv.treedb_name;
    let topic_name = kw.topic_name;
    let record = kw.record;
    let options = kw.options || {};

    return treedb_update_node( // HACK use the powerful update_node
        gobj,
        treedb_name,
        topic_name,
        record,
        options
    );
}

/********************************************
 *  Event from gobj graph
 *  kw: {
 *      topic_name,
 *      record
 *  }
 *  Send to backend
 ********************************************/
function ac_update_record(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let treedb_name = priv.treedb_name;
    let topic_name = kw.topic_name;
    let record = kw.record;
    let options = kw.options || {};

    return treedb_update_node(
        gobj,
        treedb_name,
        topic_name,
        record,
        options
    );
}

/********************************************
 *  Event from gobj graph
 *  kw: {
 *      topic_name,
 *      record
 *  }
 *  Send to backend
 ********************************************/
function ac_delete_record(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let treedb_name = priv.treedb_name;
    let topic_name = kw.topic_name;
    let record = kw.record;
    let options = kw.options || {};

    return treedb_delete_node(
        gobj,
        treedb_name,
        topic_name,
        record,
        options
    );
}

/********************************************
 *  Event from gobj graph
 *  Send to backend
 ********************************************/
function ac_link_records(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let treedb_name = priv.treedb_name;
    let parent_ref = kw.parent_ref;
    let child_ref = kw.child_ref;
    let options = kw.options || {};

    return treedb_link_nodes(
        gobj,
        treedb_name,
        parent_ref,
        child_ref,
        options
    );
}

/********************************************
 *  Event from gobj graph
 *  Send to backend
 ********************************************/
function ac_unlink_records(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let treedb_name = priv.treedb_name;
    let parent_ref = kw.parent_ref;
    let child_ref = kw.child_ref;
    let options = kw.options || {};

    return treedb_unlink_nodes(
        gobj,
        treedb_name,
        parent_ref,
        child_ref,
        options
    );
}

/********************************************
 *  Event from gobj graph
 ********************************************/
function ac_refresh_treedb(gobj, event, kw, src)
{
    /*
     *  Get data
     */
    refresh_data(gobj);

    return 0;
}

/************************************************************
 *  Parent (routing) inform us that we go showing
 *
 *      {
 *          href: href
 *      }
 *
 *  WARNING href is the full path,
 *  the path relative to this gobj is the right part of split href by '?'
 ************************************************************/
function ac_show(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let $canvas_container = document.getElementById(priv.canvas_id);
    let rect = $canvas_container.getBoundingClientRect();

    if(!priv.yet_showed && priv.graph) {
        priv.yet_showed = true;

        graph_resize(gobj, rect.width, rect.height);
        graph_reset(gobj).then(() => {graph_render(gobj);});
    }

    return 0;
}

/************************************************************
 *   Parent (routing) inform us that we go hidden
 ************************************************************/
function ac_hide(gobj, event, kw, src)
{
    return 0;
}

/************************************************************
 *  we are subscribed to EV_RESIZE from __yui_main__
 ************************************************************/
function ac_resize(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let $canvas_container = document.getElementById(priv.canvas_id);
    let rect = $canvas_container.getBoundingClientRect();
    if(rect.width === 0 || rect.height === 0) {
        priv.yet_showed = false;
    } else {
        if(priv.graph) {
            let h = rect.height;
            if(h<0) {
                h = 80;
            }
            graph_resize(gobj, rect.width, h); // setSize
            // graph_reset(gobj).then(() => {graph_render(gobj);});
        }
    }

    return 0;
}

/************************************************************
 *  we are subscribed to EV_THEME from __yui_main__
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
        stroke: theme === 'dark'?'#343434':'#EEEEEE',
        borderStroke: theme === 'dark'?'#656565':'#EEEEEE',
    });

    graph_render(gobj);

    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_zoom_in(gobj, event, kw, src)
{
    graph_zoom_in(gobj);
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_zoom_out(gobj, event, kw, src)
{
    graph_zoom_out(gobj);
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_zoom_reset(gobj, event, kw, src)
{
    graph_reset(gobj);
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_center(gobj, event, kw, src)
{
    graph_center(gobj);
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_fullscreen(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    const plugin = graph.getPluginInstance('fullscreen');
    if(plugin) {
        plugin.request();
    }
    // plugin.exit();
    return 0;
}

/************************************************************
 *
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
 *
 ************************************************************/
function ac_set_mode(gobj, event, kw, src)
{
    let mode = select_mode(gobj, kw.mode);
    gobj_save_persistent_attrs(gobj, "current_mode");
    set_mode(gobj, mode);
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_edit_mode(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let graph = priv.graph;
    let $container = gobj_read_attr(gobj, "$container");

    priv.edit_mode = !priv.edit_mode;
    if(priv.edit_mode) {
        /*
         *  Set edition mode
         */
        set_active_state($container, ".EV_EDIT_MODE", true);

        graph_set_behavior(gobj, 'drag-element', true);

        graph_add_plugin(gobj, 'history');

    } else {
        /*
         *  Set non-edition mode
         */
        set_active_state($container, ".EV_EDIT_MODE", false);

        graph_set_behavior(gobj, 'drag-element', false);

        /*
         *  Disable "save" although it could be active
         */
        disableElements($container, ".EV_SAVE_GRAPH");
        set_submit_state($container, ".EV_SAVE_GRAPH", false);

        /*
         *  Remove history plugin
         */
        graph_remove_plugin(gobj, 'history');
        update_history_buttons(gobj);
    }

    return 0;
}

/************************************************************
 *
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
 *
 ************************************************************/
function ac_node_drag_end(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let node_id = kw.evt.target.id;

    if(priv.edit_mode) {
        let $container = gobj_read_attr(gobj, "$container");
        enableElements($container, ".EV_SAVE_GRAPH");
        set_submit_state($container, ".EV_SAVE_GRAPH", true);
    }
}

/************************************************************
 *
 ************************************************************/
function ac_node_click(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let node_id = kw.evt.target.id;

    // const { target, originalTarget } = kw.evt;
    // if (originalTarget.className === 'light') {
    //     graph.updateNodeData([{ id: target.id, states: ['selected'], style: { labelText: 'Clicked!' } }]);
    //     graph.draw();
    // }
}

/************************************************************
 *
 ************************************************************/
function ac_edge_click(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let edge_id = kw.evt.target.id;
    trace_msg(`edge_id ${edge_id}`);
}

/************************************************************
 *
 ************************************************************/
function ac_node_context_menu(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let node_id = kw.evt.target.id;
}

/************************************************************
 *
 ************************************************************/
function ac_canvas_click(gobj, event, kw, src)
{
    let priv = gobj.priv;
    // let node_id = kw.evt.target.id;

    // const selectedIds = graph.getElementDataByState('node', 'selected').map((node) => node.id);
    // graph.updateNodeData(selectedIds.map((id) => ({ id, states: [], style: { labelText: 'Click the Light' } })));
    // graph.draw();
}

/************************************************************
 *
 ************************************************************/
function ac_history_redo(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    if(priv.edit_mode) {
        const history = graph.getPluginInstance('history');
        if (history.canRedo()) {
            history.redo();
        }
        update_history_buttons(gobj);
    }
}

/************************************************************
 *
 ************************************************************/
function ac_history_undo(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let graph = priv.graph;

    if(priv.edit_mode) {
        const history = graph.getPluginInstance('history');
        if (history.canUndo()) {
            history.undo();
        }
        update_history_buttons(gobj);
    }
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
    mt_writing:     mt_writing,
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

    /***************************************************************
     *          FSM
     ***************************************************************/
    /*---------------------------------------------*
     *          States
     *---------------------------------------------*/
    const states = [
        ["ST_IDLE", [
            ["EV_MT_COMMAND_ANSWER",    ac_mt_command_answer,   null],
            ["EV_TREEDB_NODE_CREATED",  ac_treedb_node_created, null],
            ["EV_TREEDB_NODE_UPDATED",  ac_treedb_node_updated, null],
            ["EV_TREEDB_NODE_DELETED",  ac_treedb_node_deleted, null],
            ["EV_CREATE_RECORD",        ac_create_record,       null],
            ["EV_UPDATE_RECORD",        ac_update_record,       null],
            ["EV_DELETE_RECORD",        ac_delete_record,       null],
            ["EV_LINK_RECORDS",         ac_link_records,        null],
            ["EV_UNLINK_RECORDS",       ac_unlink_records,      null],
            ["EV_REFRESH_TREEDB",       ac_refresh_treedb,      null],
            ["EV_SHOW",                 ac_show,                null],
            ["EV_HIDE",                 ac_hide,                null],
            ["EV_RESIZE",               ac_resize,              null],
            ["EV_THEME",                ac_theme,               null],
            ["EV_ZOOM_IN",              ac_zoom_in,             null],
            ["EV_ZOOM_OUT",             ac_zoom_out,            null],
            ["EV_ZOOM_RESET",           ac_zoom_reset,          null],
            ["EV_CENTER",               ac_center,              null],
            ["EV_FULLSCREEN",           ac_fullscreen,          null],
            ["EV_LAYOUT",               ac_layout,              null],
            ["EV_SET_MODE",             ac_set_mode,            null],
            ["EV_EDIT_MODE",            ac_edit_mode,           null],
            ["EV_SAVE_GRAPH",           ac_save_graph,          null],
            ["EV_NODE_DRAG_END",        ac_node_drag_end,       null],
            ["EV_NODE_CLICK",           ac_node_click,          null],
            ["EV_EDGE_CLICK",           ac_edge_click,          null],
            ["EV_NODE_CONTEXT_MENU",    ac_node_context_menu,   null],
            ["EV_CANVAS_CLICK",         ac_canvas_click,        null],
            ["EV_HISTORY_REDO",         ac_history_redo,        null],
            ["EV_HISTORY_UNDO",         ac_history_undo,        null],
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_MT_COMMAND_ANSWER",    event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_TREEDB_NODE_CREATED",  event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_TREEDB_NODE_UPDATED",  event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_TREEDB_NODE_DELETED",  event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_CREATE_RECORD",        0],
        ["EV_UPDATE_RECORD",        0],
        ["EV_DELETE_RECORD",        0],
        ["EV_LINK_RECORDS",         0],
        ["EV_UNLINK_RECORDS",       0],
        ["EV_REFRESH_TREEDB",       0],
        ["EV_SHOW",                 0],
        ["EV_HIDE",                 0],
        ["EV_RESIZE",               0],
        ["EV_THEME",                0],
        ["EV_ZOOM_IN",              0],
        ["EV_ZOOM_OUT",             0],
        ["EV_ZOOM_RESET",           0],
        ["EV_CENTER",               0],
        ["EV_FULLSCREEN",           0],
        ["EV_LAYOUT",               0],
        ["EV_SET_MODE",             0],
        ["EV_EDIT_MODE",            0],
        ["EV_SAVE_GRAPH",           0],
        ["EV_NODE_DRAG_END",        0],
        ["EV_NODE_CLICK",           0],
        ["EV_EDGE_CLICK",           0],
        ["EV_NODE_CONTEXT_MENU",    0],
        ["EV_CANVAS_CLICK",         0],
        ["EV_HISTORY_UNDO",         0],
        ["EV_HISTORY_REDO",         0],
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
function register_c_yui_treedb_graph()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yui_treedb_graph };
