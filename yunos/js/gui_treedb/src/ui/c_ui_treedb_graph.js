/***********************************************************************
 *          c_ui_treedb_graph.js
 *
 *          NOTE Wrapper on (Mix "Container Panel" & "Pinhold Window")
 *
 *          Manage treedb topics with mxgraph
 *
 *          Copyright (c) 2021 Niyamaka.
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA,
    SDATA_END,
    data_type_t,
    event_flag_t,
    gclass_flag_t,
    gclass_create,
    log_error,
    log_info,
    trace_msg,
    gobj_read_pointer_attr,
    gobj_read_attr,
    gobj_read_str_attr,
    gobj_read_bool_attr,
    gobj_write_attr,
    gobj_parent,
    gobj_name,
    gobj_short_name,
    gobj_subscribe_event,
    gobj_send_event,
    gobj_start,
    gobj_command,
    gobj_find_gclass,
    json_size,
    kw_get_str,
    kw_get_dict_value,
    msg_iev_write_key,
    msg_iev_get_stack,
    kw_get_dict,
    kw_flag_t,
    __yuno__,
    gobj_create_unique,
    gobj_find_unique_gobj,
    gobj_destroy,
    info_user_warning,
    build_name,
} from "yunetas";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_UI_TREEDB_GRAPH";

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
/*---------------- Wrapper Common Attributes ----------------*/
SDATA(data_type_t.DTP_BOOLEAN,  "is_pinhold_window",    0,  false,  "Select default: window or container panel"),
SDATA(data_type_t.DTP_DICT,     "panel_properties",     0,  {},     "Creator can set Container Panel properties"),
SDATA(data_type_t.DTP_DICT,     "window_properties",    0,  {},     "Creator can set Pinhold Window properties"),
SDATA(data_type_t.DTP_POINTER,  "ui_properties",        0,  null,   "Creator can set webix properties"),
SDATA(data_type_t.DTP_STRING,   "window_image",         0,  "",     "Used by pinhold_window_top_toolbar"),
SDATA(data_type_t.DTP_STRING,   "window_title",         0,  "",     "Used by pinhold_window_top_toolbar"),
SDATA(data_type_t.DTP_INTEGER,  "left",                 0,  0,      "Used by pinhold_window_top_toolbar"),
SDATA(data_type_t.DTP_INTEGER,  "top",                  0,  0,      "Used by pinhold_window_top_toolbar"),
SDATA(data_type_t.DTP_INTEGER,  "width",                0,  600,    "Used by pinhold_window_top_toolbar"),
SDATA(data_type_t.DTP_INTEGER,  "height",               0,  500,    "Used by pinhold_window_top_toolbar"),

SDATA(data_type_t.DTP_POINTER,  "$ui",                  0,  null,   "$ui from wrapped window or panel"),
SDATA(data_type_t.DTP_POINTER,  "subscriber",           0,  null,   "Subscriber of published events, by default the parent"),

/*---------------- Particular Attributes ----------------*/
SDATA(data_type_t.DTP_BOOLEAN,  "with_treedb_tables",   0,  false,  "Include treedb tables"),
SDATA(data_type_t.DTP_BOOLEAN,  "auto_topics",          0,  false,  "Auto-discover topics"),
SDATA(data_type_t.DTP_LIST,     "topics_style",         0,  null,   "Style definitions per topic"),

SDATA(data_type_t.DTP_POINTER,  "hook_data_viewer",     0,  null,   "GClass Manager/Viewer of hook data"),
SDATA(data_type_t.DTP_POINTER,  "gobj_remote_yuno",     0,  null,   "Remote yuno to ask data"),
SDATA(data_type_t.DTP_STRING,   "treedb_name",          0,  null,   "Treedb name"),
SDATA(data_type_t.DTP_LIST,     "topics",               0,  "[]",   "List of topic objects"),
SDATA(data_type_t.DTP_DICT,     "descs",                0,  null,   "Topic descriptions"),

SDATA_END()
];

let PRIVATE_DATA = {
    is_pinhold_window:  false,
    treedb_name:        "",
    gobj_remote_yuno:   null,
    descs:              null,
    topics:             [],
    topics_style:       [],
    gobj_window:        null,
    gobj_nodes_tree:    null,
    gobj_treedb_tables: null,
    gobj_container:     null,
    info_wait:          null,
    info_no_wait:       null,
    hook_data_viewer:   null,
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
    priv.is_pinhold_window = gobj_read_bool_attr(gobj, "is_pinhold_window");
    priv.topics = gobj_read_attr(gobj, "topics") || [];
    priv.topics_style = gobj_read_attr(gobj, "topics_style") || [];
    priv.hook_data_viewer = gobj_read_attr(gobj, "hook_data_viewer");
    priv.info_wait = gobj_read_attr(gobj, "info_wait") || function() {};
    priv.info_no_wait = gobj_read_attr(gobj, "info_no_wait") || function() {};

    if(!priv.treedb_name) {
        log_error(`${gobj_name(gobj)} -> treedb_name not configured`);
    }

    // TODO create container/window and child gobjs
    // if(!priv.is_pinhold_window) {
    //     priv.gobj_container = gobj_create_unique(...);
    // }
    // priv.gobj_nodes_tree = gobj_create_unique(...);
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
}




                    /***************************
                     *      Local Methods
                     ***************************/




/********************************************
 *  Build default graph topics style
 ********************************************/
function build_default_graph_topics_style(topics)
{
    const default_styles = [
        {
            node:
            "html=1;strokeColor=DarkOrange;fillColor=#fff2cc;whiteSpace=wrap;shadow=0;strokeWidth=2;spacingLeft=10;spacingTop=5;fontSize=12;verticalAlign=top;spacingTop=20;opacity=60;",
            hook:
            "html=1;strokeColor=DarkOrange;fillColor=#fff2cc;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",
            fkey:
            "ellipse;html=1;strokeColor=DarkOrange;fillColor=#fff2cc;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=25;opacity=60;",
            arrow:
            "edgeStyle=topToBottomEdgeStyle;html=1;rounded=1;curved=1;strokeWidth=2;strokeColor=DarkOrange;"
        },
        {
            node:
            "ellipse;html=1;fillColor=#f8cecc;strokeColor=#b85450;whiteSpace=wrap;shadow=0;spacingLeft=10;spacingTop=5;fontSize=12;verticalAlign=top;spacingTop=20;opacity=60;",
            hook:
            "html=1;fillColor=#f8cecc;strokeColor=#b85450;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",
            fkey:
            "ellipse;html=1;fillColor=#f8cecc;strokeColor=#b85450;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",
            arrow:
            "edgeStyle=topToBottomEdgeStyle;html=1;rounded=1;curved=1;strokeWidth=2;strokeColor=#b85450;"
        },
        {
            node:
            "ellipse;html=1;strokeColor=#6c8ebf;fillColor=#dae8fc;whiteSpace=wrap;shadow=0;spacingLeft=10;spacingTop=5;fontSize=12;verticalAlign=top;spacingTop=20;opacity=60;",
            hook:
            "html=1;strokeColor=#6c8ebf;fillColor=#dae8fc;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",
            fkey:
            "ellipse;html=1;strokeColor=#6c8ebf;fillColor=#dae8fc;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",
            arrow:
            "edgeStyle=topToBottomEdgeStyle;html=1;rounded=1;curved=1;strokeWidth=2;strokeColor=#6c8ebf;"
        },
        {
            node:
            "ellipse;html=1;strokeColor=#82b366;fillColor=#d5e8d4;whiteSpace=wrap;shadow=0;spacingLeft=10;spacingTop=5;fontSize=12;verticalAlign=top;spacingTop=20;opacity=60;",
            hook:
            "html=1;strokeColor=#82b366;fillColor=#d5e8d4;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",
            fkey:
            "ellipse;html=1;strokeColor=#82b366;fillColor=#d5e8d4;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",
            arrow:
            "edgeStyle=topToBottomEdgeStyle;html=1;rounded=1;curved=1;strokeWidth=2;strokeColor=#82b366;"
        },
        {
            node:
            "ellipse;html=1;fillColor=#f5f5f5;strokeColor=#666666;whiteSpace=wrap;shadow=0;spacingLeft=10;spacingTop=5;fontSize=12;verticalAlign=top;spacingTop=20;opacity=60;",
            hook:
            "html=1;fillColor=#f5f5f5;strokeColor=#666666;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",
            fkey:
            "ellipse;html=1;fillColor=#f5f5f5;strokeColor=#666666;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",
            arrow:
            "edgeStyle=topToBottomEdgeStyle;html=1;rounded=1;curved=1;strokeWidth=2;strokeColor=#666666;"
        },
        {
            node:
            "ellipse;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;whiteSpace=wrap;shadow=0;spacingLeft=10;spacingTop=5;fontSize=12;verticalAlign=top;spacingTop=20;opacity=60;",
            hook:
            "html=1;fillColor=#e1d5e7;strokeColor=#9673a6;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",
            fkey:
            "ellipse;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",
            arrow:
            "edgeStyle=topToBottomEdgeStyle;html=1;rounded=1;curved=1;strokeWidth=2;strokeColor=#9673a6;"
        },
        {
            node:
            "ellipse;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;whiteSpace=wrap;shadow=0;spacingLeft=10;spacingTop=5;fontSize=12;verticalAlign=top;spacingTop=20;opacity=60;",
            hook:
            "html=1;fillColor=#ffe6cc;strokeColor=#d79b00;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",
            fkey:
            "ellipse;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",
            arrow:
            "edgeStyle=topToBottomEdgeStyle;html=1;rounded=1;curved=1;strokeWidth=2;strokeColor=#d79b00;"
        }
    ];

    let topics_style = [];

    for(let i=0; i<topics.length && i<default_styles.length; i++) {
        let topic_style = {
            topic_name: topics[i].topic_name,
            run_event: false,
            default_cx: 200,
            default_cy: 180,
            default_alt_cx: 110,
            default_alt_cy: 80,
            graph_styles: default_styles[i]
        };

        topics_style.push(topic_style);
    }

    return topics_style;
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

    priv.info_wait();

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

    priv.info_wait();

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

    priv.info_wait();

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

    priv.info_wait();

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

    priv.info_wait();

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

    priv.info_wait();

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

    priv.info_wait();

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
function process_descs(gobj)
{
    let priv = gobj.priv;

    /*
     *  Auto topics
     */
    if(gobj_read_bool_attr(gobj, "auto_topics")) {
        if(json_size(priv.topics) === 0) {
            for(let topic_name in priv.descs) {
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
        priv.topics_style = build_default_graph_topics_style(priv.topics);
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
    //         priv.gobj_treedb_tables = gobj_create_unique(...);
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

/********************************************
 *  Refresh treedb
 ********************************************/
function refresh_treedb(gobj)
{
    refresh_data(gobj);
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

    priv.info_no_wait();

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
        info_user_warning(comment);
        // HACK don't return, pass errors when need it.
    }

    let __command__ = msg_iev_get_stack(gobj, kw, "command_stack", true);
    let command = kw_get_str(gobj, __command__, "command", "", kw_flag_t.KW_REQUIRED);

    switch(command) {
        case "descs":
            if(result >= 0) {
                priv.descs = data;
                gobj_write_attr(gobj, "descs", data);
                process_descs(gobj);
                if(priv.gobj_nodes_tree) {
                    gobj_send_event(priv.gobj_nodes_tree,
                        "EV_DESCS",
                        data,
                        gobj
                    );
                }
            }
            break;

        case "nodes":
            if(result >= 0) {
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
    refresh_treedb(gobj);
    return 0;
}

/********************************************
 *  Event from Mx_nodes_tree
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
    let found_gobj = gobj_find_unique_gobj(name);
    if(!found_gobj) {
        found_gobj = gobj_create_unique(
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
 *  Event from Mx_nodes_tree
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
 *  Event from Mx_nodes_tree
 *  kw: {
 *      treedb_name,
 *      topic_name,
 *      record
 *  }
 ********************************************/
function ac_mx_vertex_clicked(gobj, event, kw, src)
{
    return 0;
}

/********************************************
 *  Event from Mx_nodes_tree
 *  kw: {
 *      child_topic_name,
 *      child_topic_id,
 *      child_fkey,
 *      parent_topic_name,
 *      parent_topic_id,
 *      parent_hook
 * }
 ********************************************/
function ac_mx_edge_clicked(gobj, event, kw, src)
{
    return 0;
}

/********************************************
 *  Message from Mx_nodes_tree
 *  Send to backend, speaking of node
 ********************************************/
function ac_create_record(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let treedb_name = kw.treedb_name;
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
 *  Message from Mx_nodes_tree
 *  Send to backend, speaking of node
 ********************************************/
function ac_update_record(gobj, event, kw, src)
{
    let treedb_name = kw.treedb_name;
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
 *  Message from Mx_nodes_tree
 *  Send to backend, speaking of node
 ********************************************/
function ac_delete_record(gobj, event, kw, src)
{
    let treedb_name = kw.treedb_name;
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
 *  Message from Mx_nodes_tree
 *  Send to backend, speaking of node
 ********************************************/
function ac_link_records(gobj, event, kw, src)
{
    let treedb_name = kw.treedb_name;
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
 *  Message from Mx_nodes_tree
 *  Send to backend, speaking of node
 ********************************************/
function ac_unlink_records(gobj, event, kw, src)
{
    let treedb_name = kw.treedb_name;
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
 *  Message from Mx_nodes_tree
 ********************************************/
function ac_run_node(gobj, event, kw, src)
{
    let record = kw.record;

    let url = record.url;
    let dst_role = record.dst_role;
    let dst_service = record.dst_service;
    let dst_yuno = record.dst_yuno;
    let viewer_engine = record.viewer_engine;

    let gclass = gobj_find_gclass(viewer_engine);
    if(!gclass) {
        log_error("Viewer engine (gclass) not found: " + viewer_engine);
        return -1;
    }

    let name = viewer_engine + ">" + url + ">" + dst_role + ">" + dst_service;
    let found_gobj = gobj_find_unique_gobj(name);
    if(!found_gobj) {
        found_gobj = gobj_create_unique(
            name,
            gclass,
            {
                is_pinhold_window: true,
                window_title: name,
                window_image: "",

                dst_role: dst_role,
                dst_service: dst_service,
                dst_yuno: dst_yuno,
                url: url
            },
            gobj
        );
        gobj_start(found_gobj);
    } else {
        gobj_send_event(found_gobj, "EV_TOGGLE", {}, gobj);
    }

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

/********************************************
 *  Toggle visibility
 ********************************************/
function ac_toggle(gobj, event, kw, src)
{
    let $ui = gobj_read_attr(gobj, "$ui");
    if($ui) {
        if($ui.isVisible()) {
            $ui.hide();
        } else {
            $ui.show();
        }
        return $ui.isVisible();
    }
    return 0;
}

/********************************************
 *  Show
 ********************************************/
function ac_show(gobj, event, kw, src)
{
    let $ui = gobj_read_attr(gobj, "$ui");
    if($ui) {
        $ui.show();
        return $ui.isVisible();
    }
    return 0;
}

/********************************************
 *  Hide
 ********************************************/
function ac_hide(gobj, event, kw, src)
{
    let $ui = gobj_read_attr(gobj, "$ui");
    if($ui) {
        $ui.hide();
        return $ui.isVisible();
    }
    return 0;
}

/********************************************
 *  Select
 ********************************************/
function ac_select(gobj, event, kw, src)
{
    return 0;
}

/*************************************************************
 *  Refresh, order from container
 *  provocado por entry/exit de fullscreen
 *  o por redimensionamiento del panel, propio o de hermanos
 *************************************************************/
function ac_refresh(gobj, event, kw, src)
{
    return 0;
}

/********************************************
 *  "Container Panel"
 *  Order from container (parent): re-create
 ********************************************/
function ac_rebuild_panel(gobj, event, kw, src)
{
    // TODO rebuild(gobj);
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
            ["EV_MX_VERTEX_CLICKED",        ac_mx_vertex_clicked,       null],
            ["EV_MX_EDGE_CLICKED",          ac_mx_edge_clicked,         null],
            ["EV_CREATE_RECORD",            ac_create_record,           null],
            ["EV_DELETE_RECORD",            ac_delete_record,           null],
            ["EV_UPDATE_RECORD",            ac_update_record,           null],
            ["EV_LINK_RECORDS",             ac_link_records,            null],
            ["EV_UNLINK_RECORDS",           ac_unlink_records,          null],
            ["EV_RUN_NODE",                 ac_run_node,                null],
            ["EV_CLOSE_WINDOW",             ac_close_window,            null],
            ["EV_TOGGLE",                   ac_toggle,                  null],
            ["EV_SHOW",                     ac_show,                    null],
            ["EV_HIDE",                     ac_hide,                    null],
            ["EV_SELECT",                   ac_select,                  null],
            ["EV_REFRESH",                  ac_refresh,                 null],
            ["EV_REBUILD_PANEL",            ac_rebuild_panel,           null],
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
        ["EV_TOGGLE",                   0],
        ["EV_SHOW",                     0],
        ["EV_HIDE",                     0],
        ["EV_SELECT",                   0],
        ["EV_REFRESH",                  0],
        ["EV_REBUILD_PANEL",            0],
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
        gclass_flag_t.gcflag_manual_start // gclass_flag
    );

    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *          Register GClass
 ***************************************************************************/
function register_c_ui_treedb_graph()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_ui_treedb_graph };
