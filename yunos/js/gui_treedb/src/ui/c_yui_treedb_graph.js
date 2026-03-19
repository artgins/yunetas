/***********************************************************************
 *          c_yui_treedb_graph.js
 *
 *          Manage treedb topics with graphs
 *
 *          This gclass manages the communication between backend and graph gobj.
 *
 *          Copyright (c) 2021 Niyamaka.
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
    gobj_destroy,
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
    escapeHtml,
    safeSrc,
    gclass_find_by_name,
} from "yunetas";

import {yui_toolbar} from "./yui_toolbar.js";
import {
    removeChildElements,
    disableElements,
    enableElements,
    set_submit_state,
    set_active_state,
} from "./lib_graph.js";
import {display_error_message} from "./c_yui_main.js";

import {t} from "i18next";

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
SDATA(data_type_t.DTP_LIST,     "operation_modes",  0,
'["reading", "operation", "writing", "edition"]',
"Available **permission** or behaviour modes. These operation modes are required to be accomplish by the graph handler (G6 child). TODO permissions must match treedb permissions."),
SDATA(data_type_t.DTP_BOOLEAN,  "with_treedb_tables",0, false,  "Include treedb tables"),

/*---------------- User last selections  ----------------*/
SDATA(data_type_t.DTP_STRING,   "operation_mode",   sdata_flag_t.SDF_PERSIST, "reading", "Current operation mode (internal behaviour or role). Changed by the user trough the gui."),
SDATA(data_type_t.DTP_STRING,   "layout",           sdata_flag_t.SDF_PERSIST, "", "Current graph layout. User preference. Changed by the user through the gui."),

/*---------------- Remote Connection ----------------*/
SDATA(data_type_t.DTP_POINTER,  "gobj_remote_yuno", 0,  null,   "Remote Yuno to request data"),
SDATA(data_type_t.DTP_STRING,   "treedb_name",      0,  null,   "Remote service treedb name"),
SDATA(data_type_t.DTP_DICT,     "descs",            0,  null,   "Descriptions of topics obtained"),
SDATA(data_type_t.DTP_BOOLEAN,  "system",           0,  false,  "Manage system topics (true) or user topics (false)"),
SDATA(data_type_t.DTP_DICT,     "records",          0,  "{}",   "Data of topics"),
SDATA(data_type_t.DTP_LIST,     "topics",           0,  "[]",   "List of topic objects"),

/*---------------- Sub-container ----------------*/
SDATA(data_type_t.DTP_POINTER,  "$container",       0,  null,   "Container element"),
SDATA(data_type_t.DTP_STRING,   "href",             0,  "",     "Tab href"),
SDATA(data_type_t.DTP_STRING,   "label",            0,  "",     "Tab label"),
SDATA(data_type_t.DTP_STRING,   "image",            0,  "",     "Tab image"),
SDATA(data_type_t.DTP_STRING,   "icon",             0,  "yi-question", "Tab icon"),

/*---------------- Particular Attributes ----------------*/
SDATA(data_type_t.DTP_POINTER,  "hook_data_viewer",     0,  null,   "GClass Manager/Viewer of hook data"),
SDATA(data_type_t.DTP_BOOLEAN,  "is_pinhold_window",    0,  false,  "Select default: window or container panel"),

SDATA(data_type_t.DTP_STRING,   "wide",                 0,  "40px", "Height of header"),
SDATA(data_type_t.DTP_STRING,   "padding",              0,  "m-2",  "Padding or margin value"),
SDATA(data_type_t.DTP_STRING,   "canvas_id",            0,  "",     "Canvas ID"),

SDATA_END()
];

let PRIVATE_DATA = {
    $container:         null,
    treedb_name:        "",
    gobj_remote_yuno:   null,
    descs:              null,
    topics:             [],
    records:            {},
    gobj_nodes_tree:    null,
    gobj_treedb_tables: null,
    hook_data_viewer:   null,
    with_treedb_tables: false,
    canvas_id:          null,
    operation_mode:     null,
    layout:             null,

    is_pinhold_window:  false, // inherited of v6, todo review
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

    if(!priv.treedb_name) {
        log_error(`${gobj_name(gobj)} -> treedb_name not configured`);
    }

    let __yui_main__ = gobj_find_service("__yui_main__", true);
    if(__yui_main__) {
        gobj_subscribe_event(__yui_main__, "EV_RESIZE", {}, gobj);
    }

    /*
     *  set canvas_id, before build_ui()
     */
    let canvas_id = clean_name(gobj_name(gobj)) + "-canvas";
    gobj_write_attr(gobj, "canvas_id", canvas_id);

    /*
     *  Build UI
     */
    let $container = build_ui(gobj);

    /*
     *  Get canvas container
     */
    let $container_canvas = $container.querySelector(`#${priv.canvas_id}`);

    priv.gobj_nodes_tree = gobj_create_service(
        `${gobj_name(gobj)}-g6`,
        "C_G6_NODES_TREE",
        {
            $container: $container_canvas,
            subscriber: gobj,
            gobj_remote_yuno: priv.gobj_remote_yuno,
            treedb_name: priv.treedb_name,
            topics: priv.topics,
            operation_mode: priv.operation_mode,
            layout: priv.layout,
            // TODO review if needed
            // topics_style: priv.topics_style,
            with_treedb_tables: priv.with_treedb_tables,
            hook_port_position: "bottom",
            fkey_port_position: "top",
        },
        gobj
    );

    /*
     *  Populate layout dropdown from child's available layouts
     */
    populate_nodes_tree_options(gobj);

    /*
     *  Treedb tables at start
     */
    if(priv.with_treedb_tables) {
        // priv.gobj_treedb_tables = gobj_create_service( TODO
        //     "", // TODO build_name(self, "Topics"),
        //     "Ui_treedb_tables", // TODO
        //     {
        //         subscriber: gobj,
        //         with_treedb_tables: priv.with_treedb_tables,
        //         // hook_data_viewer: Ui_hook_viewer_popup, TODO
        //         gobj_remote_yuno: priv.gobj_remote_yuno,
        //         treedb_name: priv.treedb_name,
        //         topics: priv.topics,
        //     },
        //     gobj
        // );
    }
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
    let priv = gobj.priv;

    if(priv.gobj_nodes_tree) {
        gobj_start(priv.gobj_nodes_tree);
    }
    if(priv.gobj_treedb_tables) {
        gobj_start(priv.gobj_treedb_tables);
    }

    request_treedb_descs(gobj);

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
    return $container;
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
    let modes = gobj_read_attr(gobj, "operation_modes");
    let mode_options = modes.map(item => ['option', {}, item]);

    /*
     *  Left: layout and mode selectors
     *  Layout options are empty — populated after child creation
     *  via populate_nodes_tree_options()
     */
    let left_items = [
        ['span', {class: 'is-hidden-mobile', style: 'padding-right:5px;'}, 'Layout:'],
        ['div', {class: 'select'}, [
            ['select', {class: 'graph_layout'}]
        ], {
            change: (evt) => {
                evt.stopPropagation();
                gobj_send_event(gobj, "EV_SET_LAYOUT", {layout: evt.target.value}, gobj);
            }
        }],

        ['span', {class: 'is-hidden-mobile', style: 'padding-left:10px; padding-right:5px;'}, 'Operation Mode:'],
        ['div', {class: 'select'}, [
            ['select', {class: 'graph_operation_mode'}, mode_options]
        ], {
            change: (evt) => {
                evt.stopPropagation();
                gobj_send_event(gobj, "EV_SET_OPERATION_MODE", {operation_mode: evt.target.value}, gobj);
            }
        }],
    ];
    let l_icons = [
        ["yi-arrows-rotate",        "EV_REFRESH_TREEDB",false,  'il', 'Refresh'],
    ];
    add_buttons(gobj, left_items, l_icons);

    /*
     *  Center, common controls
     */
    let center_items = [];

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
 *  Populate layout dropdown from child's available layouts
 ************************************************************/
function populate_nodes_tree_options(gobj)
{
    let priv = gobj.priv;
    let $container = priv.$container;

    let layout_names = gobj_read_attr(priv.gobj_nodes_tree, "layout_names");
    let $layout_select = $container.querySelector('.graph_layout');
    if($layout_select && layout_names) {
        for(let name of layout_names) {
            let option = document.createElement('option');
            option.value = name;
            option.textContent = name;
            $layout_select.appendChild(option);
        }
        // Restore persisted layout selection
        let current_layout = gobj_read_str_attr(priv.gobj_nodes_tree, "layout");
        if(current_layout) {
            $layout_select.value = current_layout;
        }
    }

    // Restore persisted operation_mode selection
    let $operation_mode_select = $container.querySelector('.graph_operation_mode');
    $operation_mode_select.value = priv.operation_mode;
}

/************************************************************
 *  add_buttons() - helper to create toolbar buttons
 ************************************************************/
function create_button_handlers(gobj, event_name, target_gobj)
{
    let dst = target_gobj || gobj;
    return {
        click: (evt) => {
            evt.stopPropagation();
            gobj_send_event(dst, event_name, {evt}, gobj);
        },
        contextmenu: (evt) => {
            evt.stopPropagation();
            evt.preventDefault();
            gobj_send_event(dst, event_name, {evt}, gobj);
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

function add_buttons(gobj, zone, c_icons, target_gobj)
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

        const handlers = create_button_handlers(gobj, event_name, target_gobj);

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

            case 'il':
            {
                const label = item[4] || '';
                const content = [
                    ['i', {
                        style: "font-size:1.5em; color:inherit;",
                        class: icon_name
                    }],
                    ['span', {class: 'is-hidden-mobile', style: 'padding-left:5px;'}, label]
                ];
                push_button(zone, button, content, handlers);
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
 *  Command to remote service
 *  Get nodes of a topic
 ************************************************************/
function treedb_nodes(gobj, treedb_name, topic_name, options)
{
    let priv = gobj.priv;

    if(!priv.gobj_remote_yuno) {
        log_error(`${gobj_short_name(gobj)}: No gobj_remote_yuno defined`);
        return;
    }

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
function treedb_create_node(gobj, treedb_name, topic_name, record, options)
{
    let priv = gobj.priv;

    if(!priv.gobj_remote_yuno) {
        log_error(`${gobj_short_name(gobj)}: No gobj_remote_yuno defined`);
        return;
    }

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

    if(!priv.gobj_remote_yuno) {
        log_error(`${gobj_short_name(gobj)}: No gobj_remote_yuno defined`);
        return;
    }

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

    if(!priv.gobj_remote_yuno) {
        log_error(`${gobj_short_name(gobj)}: No gobj_remote_yuno defined`);
        return;
    }

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
function treedb_link_nodes(gobj, treedb_name, parent_ref, child_ref, options)
{
    let priv = gobj.priv;

    if(!priv.gobj_remote_yuno) {
        log_error(`${gobj_short_name(gobj)}: No gobj_remote_yuno defined`);
        return;
    }

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
function treedb_unlink_nodes(gobj, treedb_name, parent_ref, child_ref, options)
{
    let priv = gobj.priv;

    if(!priv.gobj_remote_yuno) {
        log_error(`${gobj_short_name(gobj)}: No gobj_remote_yuno defined`);
        return;
    }

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
 *  Process topic descriptions received from remote
 ************************************************************/
function process_treedb_descs(gobj, descs)
{
    let priv = gobj.priv;

    gobj_write_attr(gobj, "descs", descs);  // TRIGGER POINT: Topics cleared

    /*
     *  descs is a dict: { __snaps__: {…}, roles: {…}, users: {…} }
     */

    if(priv.gobj_nodes_tree) {
        gobj_send_event(priv.gobj_nodes_tree,
            "EV_DESCS",
            descs,
            gobj
        );
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

/********************************************
 *  Refresh data from remote
 ********************************************/
function refresh_data(gobj)
{
    let priv = gobj.priv;

    if(priv.gobj_nodes_tree) { // TODO must do clear_graph(gobj)
        gobj_send_event(priv.gobj_nodes_tree,
            "EV_CLEAR_DATA",
            {},
            gobj
        );
    }
    request_treedb_descs(gobj);
}




                    /***************************
                     *      Actions
                     ***************************/




/********************************************
 *  Remote response
 ********************************************/
function ac_mt_command_answer(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let result;
    let comment;
    let schema;
    let data;

    try {
        result = kw.result;
        comment = kw.comment;
        schema = kw.schema;
        data = kw.data;
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

    let __command__ = msg_iev_get_stack(gobj, kw, "command_stack", true);
    let command = kw_get_str(gobj, __command__, "command", "", kw_flag_t.KW_REQUIRED);
    let kw_command = kw_get_dict(gobj, __command__, "kw", {}, kw_flag_t.KW_REQUIRED);

    switch(command) {
        case "descs":
            if(result >= 0) {
                process_treedb_descs(gobj, data);
            }
            break;

        case "nodes":
            if(result >= 0) {
                /*
                 *  Here it could update cols of `descs`
                 *  seeing if they have changed in schema argument (controlling the version?)
                 *  Now the schema pass to creation of nodes is get from `descs`.
                 */
                gobj_send_event(priv.gobj_nodes_tree,
                    "EV_LOAD_DATA",
                    {
                        kw_command: kw_command,
                        schema: schema,
                        data: data
                    },
                    gobj
                );
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
    let priv = gobj.priv;
    let treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    let topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    let node = kw_get_dict_value(gobj, kw, "node", null, 0);

    if(treedb_name !== priv.treedb_name) {
        log_error("It's not my treedb_name: " + treedb_name);
        return 0;
    }

    let schema = priv.descs[topic_name];

    if(priv.gobj_nodes_tree) {
        gobj_send_event(priv.gobj_nodes_tree,
            "EV_NODE_CREATED",
            {
                schema: schema,
                topic_name: topic_name,
                node: node
            },
            gobj
        );
    }

    return 0;
}

/********************************************
 *  Remote subscription response
 ********************************************/
function ac_treedb_node_updated(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    let topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    let node = kw_get_dict_value(gobj, kw, "node", null, 0);

    if(treedb_name !== priv.treedb_name) {
        log_error("It's not my treedb_name: " + treedb_name);
        return 0;
    }

    if(priv.gobj_nodes_tree) {
        gobj_send_event(priv.gobj_nodes_tree,
            "EV_NODE_UPDATED",
            {
                topic_name: topic_name,
                node: node
            },
            gobj
        );
    }

    return 0;
}

/********************************************
 *  Remote subscription response
 ********************************************/
function ac_treedb_node_deleted(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    let topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    let node = kw_get_dict_value(gobj, kw, "node", null, 0);

    if(treedb_name !== priv.treedb_name) {
        log_error("It's not my treedb_name: " + treedb_name);
        return 0;
    }

    if(priv.gobj_nodes_tree) {
        gobj_send_event(priv.gobj_nodes_tree,
            "EV_NODE_DELETED",
            {
                topic_name: topic_name,
                node: node
            },
            gobj
        );
    }

    return 0;
}

/********************************************
 *  Refresh treedb action
 ********************************************/
function ac_refresh_treedb(gobj, event, kw, src)
{
    /*
     *  Get data
     */
    refresh_data(gobj);
    return 0;
}

/********************************************
 *  Event from G6_nodes_tree
 *  kw: {
 *      treedb_name,
 *      parent_topic_name,
 *      child_topic_name,
 *      child_field_name,
 *      child_field_value,
 *      click_x,
 *      click_y
 *  }
 ********************************************/
function ac_show_hook_data(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let treedb_name = kw.treedb_name;
    let parent_topic_name = kw.parent_topic_name;
    let child_topic_name = kw.child_topic_name;
    let child_field_name = kw.child_field_name;
    let child_field_value = kw.child_field_value;

    if(!priv.hook_data_viewer) {
        trace_msg(kw);
        return 0;
    }

    let name = "Graph Hook>" + treedb_name + ">" +
        parent_topic_name + ">" +
        child_topic_name + ">" +
        child_field_name + ">" +
        child_field_value;
    let found_gobj = gobj_find_service(name);
    if(!found_gobj) {
        found_gobj = gobj_create_service(
            name,
            priv.hook_data_viewer,
            kw,
            gobj
        );
        gobj_start(found_gobj);
    } else {
        gobj_send_event(found_gobj, "EV_TOGGLE", {}, gobj);
    }

    return 0;
}

/********************************************
 *  Event from G6_nodes_tree
 ********************************************/
function ac_show_treedb_topic(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let topic_name = kw.topic_name;

    if(!priv.gobj_treedb_tables) {
        log_error("gobj_treedb_tables not available");
        return 0;
    }

    let gobj_topic_formtable = gobj_send_event(priv.gobj_treedb_tables,
        "EV_GET_TOPIC_FORMTABLE",
        {
            topic_name: topic_name
        },
        gobj
    );

    if(gobj_topic_formtable) {
        gobj_send_event(gobj_topic_formtable, "EV_TOGGLE", {}, gobj);
    } else {
        log_error("Topic Formtable not found: " + topic_name);
    }

    return 0;
}

/********************************************
 *  Event from G6_nodes_tree
 *  kw: {
 *      treedb_name,
 *      topic_name,
 *      record
 *  }
 ********************************************/
function ac_vertex_clicked(gobj, event, kw, src)
{
    return 0;
}

/********************************************
 *  Event from G6_nodes_tree
 *  kw: {
 *      child_topic_name,
 *      child_topic_id,
 *      child_fkey,
 *      parent_topic_name,
 *      parent_topic_id,
 *      parent_hook
 * }
 ********************************************/
function ac_edge_clicked(gobj, event, kw, src)
{
    return 0;
}

/********************************************
 *  Message from G6_nodes_tree
 *  kw: {
 *      topic_name,
 *      record,
 *      options
 *  }
 *  Send to backend
 ********************************************/
function ac_create_node(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let treedb_name = priv.treedb_name;
    let topic_name = kw.topic_name;
    let record = kw.record;
    let options = kw.options || {};

    return treedb_create_node(
        gobj,
        treedb_name,
        topic_name,
        record,
        options
    );
}

/********************************************
 *  Message from G6_nodes_tree
 *  kw: {
 *      topic_name,
 *      record,
 *      options
 *  }
 *  Send to backend
 ********************************************/
function ac_update_node(gobj, event, kw, src)
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
 *  Message from G6_nodes_tree
 *  kw: {
 *      topic_name,
 *      record,
 *      options
 *  }
 *  Send to backend
 ********************************************/
function ac_delete_node(gobj, event, kw, src)
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
 *  Message from G6_nodes_tree
 *  kw: {
 *      parent_ref,
 *      child_ref,
 *      options
 *  }
 *  Send to backend
 ********************************************/
function ac_link_nodes(gobj, event, kw, src)
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
 *  Message from G6_nodes_tree
 *  kw: {
 *      parent_ref,
 *      child_ref,
 *      options
 *  }
 *  Send to backend
 ********************************************/
function ac_unlink_nodes(gobj, event, kw, src)
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
 *  Message from G6_nodes_tree
 ********************************************/
function ac_run_node(gobj, event, kw, src)
{
    // TODO what is this?
    // let record = kw.record;
    //
    // let url = record.url;
    // let dst_role = record.dst_role;
    // let dst_service = record.dst_service;
    // let dst_yuno = record.dst_yuno;
    // let viewer_engine = record.viewer_engine;
    //
    // let gclass = gclass_find_by_name(viewer_engine);
    // if(!gclass) {
    //     log_error("Viewer engine (gclass) not found: " + viewer_engine);
    //     return -1;
    // }
    //
    // let name = viewer_engine + ">" + url + ">" + dst_role + ">" + dst_service;
    // let found_gobj = gobj_find_service(name);
    // if(!found_gobj) {
    //     found_gobj = gobj_create_service(
    //         name,
    //         gclass,
    //         {
    //             is_pinhold_window: true,
    //             window_title: name,
    //             window_image: "",
    //
    //             dst_role: dst_role,
    //             dst_service: dst_service,
    //             dst_yuno: dst_yuno,
    //             url: url
    //         },
    //         gobj
    //     );
    //     gobj_start(found_gobj);
    // } else {
    //     gobj_send_event(found_gobj, "EV_TOGGLE", {}, gobj);
    // }

    return 0;
}

/********************************************
 *  From wrapped $ui, destroy self
 *  - Top toolbar informing of window close
 *      {destroying: true}   Window destroying
 *      {destroying: false}  Window minifying
 ********************************************/
function ac_close_window(gobj, event, kw, src)
{
    let priv = gobj.priv;

    if(priv.is_pinhold_window) {
        gobj_destroy(gobj);
    }
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_set_layout(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let layout = kw.layout;
    gobj_write_str_attr(gobj, "layout", layout);
    gobj_save_persistent_attrs(gobj, "layout");

    let $layout_select = priv.$container.querySelector('.graph_layout');
    $layout_select.value = priv.layout;

    gobj_send_event(
        priv.gobj_nodes_tree,
        "EV_SET_LAYOUT",
        {
            layout: layout
        },
        gobj
    );

    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_set_operation_mode(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let operation_mode = kw.operation_mode;
    gobj_write_str_attr(gobj, "operation_mode", operation_mode);
    gobj_save_persistent_attrs(gobj, "operation_mode");

    let $operation_mode_select = priv.$container.querySelector('.graph_operation_mode');
    $operation_mode_select.value = priv.operation_mode;

    gobj_send_event(
        priv.gobj_nodes_tree,
        "EV_SET_OPERATION_MODE",
        {
            operation_mode: operation_mode
        },
        gobj
    );

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
    return gobj_send_event(priv.gobj_nodes_tree, event, kw, gobj);
}

/************************************************************
 *   Parent (routing) inform us that we go hidden
 ************************************************************/
function ac_hide(gobj, event, kw, src)
{
    let priv = gobj.priv;
    return gobj_send_event(priv.gobj_nodes_tree, event, kw, gobj);
}

/************************************************************
 *  we are subscribed to EV_RESIZE from __yui_main__
 ************************************************************/
function ac_resize(gobj, event, kw, src)
{
    let priv = gobj.priv;
    return gobj_send_event(priv.gobj_nodes_tree, event, kw, gobj);
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
            ["EV_MT_COMMAND_ANSWER",        ac_mt_command_answer,       null],
            ["EV_TREEDB_NODE_CREATED",      ac_treedb_node_created,     null],
            ["EV_TREEDB_NODE_UPDATED",      ac_treedb_node_updated,     null],
            ["EV_TREEDB_NODE_DELETED",      ac_treedb_node_deleted,     null],
            ["EV_REFRESH_TREEDB",           ac_refresh_treedb,          null],
            ["EV_SHOW_HOOK_DATA",           ac_show_hook_data,          null],
            ["EV_SHOW_TREEDB_TOPIC",        ac_show_treedb_topic,       null],
            ["EV_VERTEX_CLICKED",           ac_vertex_clicked,          null],
            ["EV_EDGE_CLICKED",             ac_edge_clicked,            null],
            ["EV_CREATE_NODE",              ac_create_node,             null],
            ["EV_DELETE_NODE",              ac_delete_node,             null],
            ["EV_UPDATE_NODE",              ac_update_node,             null],
            ["EV_LINK_NODES",               ac_link_nodes,              null],
            ["EV_UNLINK_NODES",             ac_unlink_nodes,            null],
            ["EV_RUN_NODE",                 ac_run_node,                null],
            ["EV_CLOSE_WINDOW",             ac_close_window,            null],
            ["EV_SET_LAYOUT",               ac_set_layout,              null],
            ["EV_SET_OPERATION_MODE",       ac_set_operation_mode,      null],
            ["EV_SHOW",                     ac_show,                    null],
            ["EV_HIDE",                     ac_hide,                    null],
            ["EV_RESIZE",                   ac_resize,                  null],
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_MT_COMMAND_ANSWER",        event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_TREEDB_NODE_CREATED",      event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_TREEDB_NODE_UPDATED",      event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_TREEDB_NODE_DELETED",      event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_REFRESH_TREEDB",           0],
        ["EV_SHOW_HOOK_DATA",           0],
        ["EV_SHOW_TREEDB_TOPIC",        0],
        ["EV_VERTEX_CLICKED",           0],
        ["EV_EDGE_CLICKED",             0],
        ["EV_CREATE_NODE",              0],
        ["EV_DELETE_NODE",              0],
        ["EV_UPDATE_NODE",              0],
        ["EV_LINK_NODES",               0],
        ["EV_UNLINK_NODES",             0],
        ["EV_RUN_NODE",                 0],
        ["EV_CLOSE_WINDOW",             0],
        ["EV_SET_LAYOUT",               0],
        ["EV_SET_OPERATION_MODE",       0],
        ["EV_SHOW",                     0],
        ["EV_HIDE",                     0],
        ["EV_RESIZE",                   0],
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
        0   // gclass_flag
    );

    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *          Register GClass
 ***************************************************************************/
function register_c_yui_treedb_graph()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yui_treedb_graph };
