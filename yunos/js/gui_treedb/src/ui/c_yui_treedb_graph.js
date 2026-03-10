/***********************************************************************
 *          c_ui_treedb_graph.js
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
SDATA(data_type_t.DTP_BOOLEAN,  "with_treedb_tables",0, false,  "Include treedb tables"),

/*---------------- User last selections  ----------------*/
SDATA(data_type_t.DTP_STRING,   "current_mode",     sdata_flag_t.SDF_PERSIST, "", "Current mode (internal behaviour or role). Changed by the user trough the gui."),
SDATA(data_type_t.DTP_STRING,   "current_layout",   sdata_flag_t.SDF_PERSIST, "", "Current graph layout or **view**, See graph_settings.layouts for available list. User preference. Changed by the user trough the gui."),

/*---------------- Remote Connection ----------------*/
SDATA(data_type_t.DTP_POINTER,  "gobj_remote_yuno", 0,  null,   "Remote Yuno to request data"),
SDATA(data_type_t.DTP_STRING,   "treedb_name",      0,  null,   "Remote service treedb name"),
SDATA(data_type_t.DTP_DICT,     "descs",            0,  null,   "Descriptions of topics obtained"),
SDATA(data_type_t.DTP_DICT,     "records",          0,  "{}",   "Data of topics"),
SDATA(data_type_t.DTP_LIST,     "topics",           0,  "[]",   "List of topic objects"),

/*---------------- Sub-container ----------------*/
SDATA(data_type_t.DTP_POINTER,  "$container",       0,  null,   "Container element"),
SDATA(data_type_t.DTP_STRING,   "href",             0,  "",     "Tab href"),
SDATA(data_type_t.DTP_STRING,   "label",            0,  "",     "Tab label"),
SDATA(data_type_t.DTP_STRING,   "image",            0,  "",     "Tab image"),
SDATA(data_type_t.DTP_STRING,   "icon",             0,  "fa-solid fa-question", "Tab icon"),

/*---------------- Particular Attributes ----------------*/
SDATA(data_type_t.DTP_BOOLEAN,  "auto_topics",          0,  true,   "Auto-discover topics"),
SDATA(data_type_t.DTP_POINTER,  "hook_data_viewer",     0,  null,   "GClass Manager/Viewer of hook data"),
SDATA(data_type_t.DTP_BOOLEAN,  "is_pinhold_window",    0,  false,  "Select default: window or container panel"),

SDATA(data_type_t.DTP_STRING,   "wide",                     0,  "40px", "Height of header"),
SDATA(data_type_t.DTP_STRING,   "padding",                  0,  "m-2",  "Padding or margin value"),

SDATA_END()
];

let PRIVATE_DATA = {
    treedb_name:        "",
    gobj_remote_yuno:   null,
    descs:              null,
    topics:             [],
    records:            {},
    gobj_nodes_tree:    null,
    gobj_treedb_tables: null,
    hook_data_viewer:   null,
    with_treedb_tables: false,
    auto_topics:        false,

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

    priv.treedb_name = gobj_read_str_attr(gobj, "treedb_name");
    priv.gobj_remote_yuno = gobj_read_attr(gobj, "gobj_remote_yuno");
    priv.topics = gobj_read_attr(gobj, "topics") || [];
    priv.hook_data_viewer = gobj_read_attr(gobj, "hook_data_viewer");
    priv.is_pinhold_window = gobj_read_bool_attr(gobj, "is_pinhold_window");
    priv.with_treedb_tables = gobj_read_bool_attr(gobj, "with_treedb_tables");
    priv.auto_topics = gobj_read_bool_attr(gobj, "auto_topics");

    if(!priv.treedb_name) {
        log_error(`${gobj_name(gobj)} -> treedb_name not configured`);
    }

    let __yui_main__ = gobj_find_service("__yui_main__", true);
    if(__yui_main__) {
        gobj_subscribe_event(__yui_main__, "EV_RESIZE", {}, gobj);
        gobj_subscribe_event(__yui_main__, "EV_THEME", {}, gobj);
        priv.theme = gobj_read_str_attr(__yui_main__, "theme");
    }

    build_ui(gobj);

    priv.gobj_nodes_tree = gobj_create_service(
        `${gobj_name(gobj)}-g6`,
        "C_G6_NODES_TREE",
        {
            subscriber: gobj,
            gobj_remote_yuno: priv.gobj_remote_yuno,
            treedb_name: priv.treedb_name,
            topics: priv.topics,
            // TODO review if needed
            // topics_style: priv.topics_style,
            with_treedb_tables: priv.with_treedb_tables,
            hook_port_position: "bottom",
            fkey_port_position: "top",
        },
        gobj
    );

    /*
     *  Treedb tables at start
     */
    if(priv.with_treedb_tables) {
        if(!gobj_read_bool_attr(gobj, "auto_topics")) {
            // priv.gobj_treedb_tables = gobj_create_service( TODO
            //     "", // TODO build_name(self, "Topics"),
            //     "Ui_treedb_tables", // TODO
            //     {
            //         subscriber: gobj,
            //         with_treedb_tables: priv.with_treedb_tables,
            //         auto_topics: priv.auto_topics,
            //         // hook_data_viewer: Ui_hook_viewer_popup, TODO
            //         gobj_remote_yuno: priv.gobj_remote_yuno,
            //         treedb_name: priv.treedb_name,
            //         topics: priv.topics,
            //     },
            //     gobj
            // );
        }
    }
}

/***************************************************************
 *          Framework Method: Writing
 ***************************************************************/
function mt_writing(gobj, path)
{
    let priv = gobj.priv;

    switch(path) {
        // TODO needed?
        case "treedb_name":
            priv.treedb_name = gobj_read_str_attr(gobj, "treedb_name");
            break;
        case "gobj_remote_yuno":
            priv.gobj_remote_yuno = gobj_read_attr(gobj, "gobj_remote_yuno");
            break;
        case "descs": // Yes, needed
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

    /*
     *  Left, layout and mode
     */
    let left_items = [
    ];

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
 *  Command to remote service
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

    msg_iev_write_key(kw, "__topic_name__", topic_name);

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

    msg_iev_write_key(kw, "__topic_name__", topic_name);

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

    msg_iev_write_key(kw, "__topic_name__", topic_name);

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

    msg_iev_write_key(kw, "__topic_name__", topic_name);

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
function process_treedb_descs(gobj)
{
    let priv = gobj.priv;

    /*
     *  Auto topics
     */
    if(gobj_read_bool_attr(gobj, "auto_topics")) {
        if(json_size(priv.topics) === 0) {
            for(const topic_name of Object.keys(priv.descs)) {
                if(topic_name.substring(0, 2) === "__") {
                    continue;
                }
                priv.topics.push({topic_name: topic_name});
            }
        }
    }

    /*
     *  Subscribe events
     */
    for(let i=0; i<priv.topics.length; i++) {
        let topic_name = priv.topics[i].topic_name;
        let desc = priv.descs[topic_name];
        if(!desc) {
            log_error("topic desc unknown: " + topic_name);
            continue;
        }

        gobj_subscribe_event(priv.gobj_remote_yuno,
            "EV_TREEDB_NODE_CREATED",
            {
                __service__: priv.treedb_name,
                __filter__: {
                    "treedb_name": priv.treedb_name,
                    "topic_name": topic_name
                }
            },
            gobj
        );
        gobj_subscribe_event(priv.gobj_remote_yuno,
            "EV_TREEDB_NODE_UPDATED",
            {
                __service__: priv.treedb_name,
                __filter__: {
                    "treedb_name": priv.treedb_name,
                    "topic_name": topic_name
                }
            },
            gobj
        );
        gobj_subscribe_event(priv.gobj_remote_yuno,
            "EV_TREEDB_NODE_DELETED",
            {
                __service__: priv.treedb_name,
                __filter__: {
                    "treedb_name": priv.treedb_name,
                    "topic_name": topic_name
                }
            },
            gobj
        );
    }

    /*
     *  Create default styles if not defined
     */
    if(json_size(priv.topics_style) === 0) {
        // priv.topics_style = build_default_graph_topics_style(priv.topics);
        if(priv.gobj_nodes_tree) {
            gobj_send_event(priv.gobj_nodes_tree,
                "EV_CREATE_GRAPH_STYLES",
                {
                    topics_style: priv.topics_style,
                },
                gobj
            );
        }
    }

    /*
     *  Treedb tables on auto_topics
     */
    // TODO: Create gobj_treedb_tables when auto_topics + with_treedb_tables
    // if(gobj_read_bool_attr(gobj, "auto_topics")) {
    //     if(gobj_read_bool_attr(gobj, "with_treedb_tables")) {
    //         if(priv.gobj_treedb_tables) {
    //             log_error("gobj_treedb_tables ALREADY created");
    //         }
    //         priv.gobj_treedb_tables = gobj_create_service(...);
    //         gobj_start(priv.gobj_treedb_tables);
    //     }
    // }

    /*
     *  Get data
     */
    refresh_data(gobj);
}

/********************************************
 *  Refresh data from remote
 ********************************************/
function refresh_data(gobj)
{
    let priv = gobj.priv;

    if(priv.gobj_nodes_tree) {
        gobj_send_event(priv.gobj_nodes_tree,
            "EV_CLEAR_DATA",
            {},
            gobj
        );
    }

    let options = {
        list_dict: true
    };

    for(let i=0; i<priv.topics.length; i++) {
        let topic = priv.topics[i];
        let topic_name = topic.topic_name;

        treedb_nodes(gobj,
            priv.treedb_name,
            topic_name,
            options
        );
    }
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
                gobj_write_attr(gobj, "descs", data);
                process_treedb_descs(gobj);
                // if(priv.gobj_nodes_tree) { // TODO move to process_treedb_descs?
                //     gobj_send_event(priv.gobj_nodes_tree,
                //         "EV_DESCS",
                //         data,
                //         gobj
                //     );
                // }
            }
            break;

        case "nodes":
            if(result >= 0) {
                /*
                 *  Here it could update cols of `descs`
                 *  seeing if they have changed in schema argument (controlling the version?)
                 *  Now the schema pass to creation of nodes is get from `descs`.
                 */
                if(priv.gobj_nodes_tree) {
                    gobj_send_event(priv.gobj_nodes_tree,
                        "EV_LOAD_DATA",
                        {
                            schema: schema,
                            data: data
                        },
                        gobj
                    );
                }
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
function ac_create_record(gobj, event, kw, src)
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
 *  Message from G6_nodes_tree
 *  kw: {
 *      topic_name,
 *      record,
 *      options
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
 *  Message from G6_nodes_tree
 *  kw: {
 *      parent_ref,
 *      child_ref,
 *      options
 *  }
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
 *  Message from G6_nodes_tree
 *  kw: {
 *      parent_ref,
 *      child_ref,
 *      options
 *  }
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

    // TODO
    // let $canvas_container = document.getElementById(priv.canvas_id);
    // let rect = $canvas_container.getBoundingClientRect();
    //
    // if(!priv.yet_showed && priv.graph) {
    //     priv.yet_showed = true;
    //
    //     graph_resize(gobj, rect.width, rect.height);
    //     graph_reset(gobj).then(() => {graph_render(gobj);});
    // }

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
    // TODO
    // let priv = gobj.priv;
    //
    // let $canvas_container = document.getElementById(priv.canvas_id);
    // let rect = $canvas_container.getBoundingClientRect();
    // if(rect.width === 0 || rect.height === 0) {
    //     priv.yet_showed = false;
    // } else {
    //     if(priv.graph) {
    //         let h = rect.height;
    //         if(h<0) {
    //             h = 80;
    //         }
    //         graph_resize(gobj, rect.width, h); // setSize
    //         // graph_reset(gobj).then(() => {graph_render(gobj);});
    //     }
    // }

    return 0;
}

/************************************************************
 *  we are subscribed to EV_THEME from __yui_main__
 ************************************************************/
function ac_theme(gobj, event, kw, src)
{
    // TODO
    // let priv = gobj.priv;
    // let graph = priv.graph;
    // let theme = kw.theme || 'light';
    // priv.theme = theme;
    // graph.setTheme(theme);
    //
    // graph.updatePlugin({
    //     key: 'grid-line',
    //     stroke: theme === 'dark'?'#343434':'#EEEEEE',
    //     borderStroke: theme === 'dark'?'#656565':'#EEEEEE',
    // });
    //
    // graph_render(gobj);

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
            ["EV_MT_COMMAND_ANSWER",        ac_mt_command_answer,       null],
            ["EV_TREEDB_NODE_CREATED",      ac_treedb_node_created,     null],
            ["EV_TREEDB_NODE_UPDATED",      ac_treedb_node_updated,     null],
            ["EV_TREEDB_NODE_DELETED",      ac_treedb_node_deleted,     null],
            ["EV_REFRESH_TREEDB",           ac_refresh_treedb,          null],
            ["EV_SHOW_HOOK_DATA",           ac_show_hook_data,          null],
            ["EV_SHOW_TREEDB_TOPIC",        ac_show_treedb_topic,       null],
            ["EV_MX_VERTEX_CLICKED",        ac_vertex_clicked,          null],
            ["EV_MX_EDGE_CLICKED",          ac_edge_clicked,            null],
            ["EV_CREATE_RECORD",            ac_create_record,           null],
            ["EV_DELETE_RECORD",            ac_delete_record,           null],
            ["EV_UPDATE_RECORD",            ac_update_record,           null],
            ["EV_LINK_RECORDS",             ac_link_records,            null],
            ["EV_UNLINK_RECORDS",           ac_unlink_records,          null],
            ["EV_RUN_NODE",                 ac_run_node,                null],
            ["EV_CLOSE_WINDOW",             ac_close_window,            null],
            ["EV_SHOW",                     ac_show,                    null],
            ["EV_HIDE",                     ac_hide,                    null],
            ["EV_RESIZE",                   ac_resize,                  null],
            ["EV_THEME",                    ac_theme,                   null],
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
        ["EV_MX_VERTEX_CLICKED",        0],
        ["EV_MX_EDGE_CLICKED",          0],
        ["EV_CREATE_RECORD",            0],
        ["EV_DELETE_RECORD",            0],
        ["EV_UPDATE_RECORD",            0],
        ["EV_LINK_RECORDS",             0],
        ["EV_UNLINK_RECORDS",           0],
        ["EV_RUN_NODE",                 0],
        ["EV_CLOSE_WINDOW",             0],
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
