/***********************************************************************
 *          c_yui_treedb_topics.js
 *
 *          Management of TreeDB's topics with Bulma tabs
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

import {
    __yuno__,
    SDATA,
    SDATA_END,
    data_type_t,
    event_flag_t,
    kw_flag_t,
    gclass_create,
    log_error,
    gobj_read_pointer_attr,
    gobj_subscribe_event,
    gobj_parent,
    sprintf,
    gobj_name,
    gobj_find_child,
    gobj_read_attr,
    createElement2,
    kw_get_dict_value,
    kw_get_str,
    gobj_send_event,
    gobj_write_attr,
    gobj_short_name,
    gobj_read_str_attr,
    gobj_read_bool_attr,
    gobj_start,
    gobj_create_service,
    gobj_command,
    gobj_match_children,
    msg_iev_get_stack,
    kw_get_dict, gobj_stop_children,
} from "yunetas";

import {
    display_error_message
} from "./c_yui_main.js";

import {t} from "i18next";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_TREEDB_TOPICS";

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",       0,  null,   "Subscriber of output events"),
SDATA(data_type_t.DTP_POINTER,  "gobj_remote_yuno", 0,  null,   "Remote yuno for data fetching"),
SDATA(data_type_t.DTP_STRING,   "treedb_name",      0,  null,   "Remote TreeDB service name"),
SDATA(data_type_t.DTP_JSON,     "descs",            0,  null,   "Description of topics"),
SDATA(data_type_t.DTP_BOOLEAN,  "system",           0,  false,  "Manage system topics (true) or user topics (false)"),
SDATA(data_type_t.DTP_STRING,   "tabs_style",       0,  "is-toggle is-fullwidth", "Bulma tab styling"),
SDATA(data_type_t.DTP_POINTER,  "$container",       0,  null,   "Root HTML element, show/hide managed by external routing"),
SDATA(data_type_t.DTP_POINTER,  "$current_item",    0,  null,   "Currently selected item"),
SDATA(data_type_t.DTP_STRING,   "last_selection",   0,  null,   "Last href selection"),
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
    build_ui(gobj);

    /*
     *  CHILD subscription model
     */
    let subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, null, {}, subscriber);
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    request_treedb_descs(gobj);
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    gobj_stop_children(gobj);
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
    destroy_ui(gobj);
}

/************************************************************
 *      Framework Method command
 ************************************************************/
function mt_command_parser(gobj, command, kw, src)
{
    switch(command) {
        case "help":
            return cmd_help(gobj, command, kw, src);
        case "get_topic_data":
            return cmd_get_topic_data(gobj, command, kw, src);
        default:
            log_error("Command not found: %s", command);
            return {
                "result": -1,
                "comment": sprintf("Command not found: %s", command),
                "schema": null,
                "data": null
            };
    }
}



                    /***************************
                     *      Commands
                     ***************************/




/************************************************************
 *
 ************************************************************/
function cmd_help(gobj, cmd, kw, src)
{
    return {
        "result": 0,
        "comment": "",
        "schema": null,
        "data": null
    };
}

/************************************************************
 *
 ************************************************************/
function cmd_get_topic_data(gobj, cmd, kw, src)
{
    let webix = {
        "result": 0,
        "comment": "",
        "schema": null,
        "data": null
    };

    let topic_name = kw.topic_name;

    let gobj_topic_form = gobj_find_child(gobj,
        {
            __gobj_name__: `${gobj_name(gobj)}?${topic_name}`
        }
    );

    let $$table = gobj_read_attr(gobj_topic_form, "$$table");
    webix.data = $$table.bootstrapTable("getData");

    return webix;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/************************************************************
 *   Build UI
 ************************************************************/
function build_ui(gobj)
{
    /*----------------------------------------------*
     *  Layout Schema
     *----------------------------------------------*/
    let $container = createElement2(
        ['div', {class: `${gobj_read_attr(gobj, "treedb_name")}`, style: 'height:100%; display:flex; flex-direction:column;'}, [
            ['div', {class: 'is-flex-grow-0'}, [
                ['div', {class: `tabs ${gobj_read_attr(gobj, "tabs_style")}`, style: 'margin-bottom: 10px;'}, [
                    ['ul', {}]
                ]],
            ]],
            ['div', {class: 'is-flex-grow-1 sub-container', style: 'height:100%; min-height:0; overflow: auto;'}, [
            ]]
        ]]
    );
    gobj_write_attr(gobj, "$container", $container);
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
 *   Add tab
 ************************************************************/
function add_tab(gobj, gobj2, id, text, icon)
{
    let $container = gobj_read_attr(gobj, "$container");
    let $tabs = $container.querySelector('ul');

    /*
     *  Create li a
     */
    let $item = createElement2(
        ['li', {class: ''}, [ // is-active
            ['a', {href: id}]
        ]]
    );
    $tabs.appendChild($item);
    $item.gobj = gobj2; // Cross-reference

    /*
     *  TAB: Add icon/text to a
     */
    let $a = $item.querySelector('a');

    /*
     *  Add icon
     */
    if(icon) {
        $a.appendChild(
            createElement2(
                ['span', {class: 'icon is-small is-hidden-mobile'},
                    ['i', {class: `${icon}`}]
                ]
            )
        );
    }

    /*
     *  TAB: Add text
     */
    $a.appendChild(
        createElement2(['span', {i18n: text, class: ''}, text])
    );

    /*
     *  SUB-CONTAINER, add child content
     */
    let $sub_container = $container.querySelector('.sub-container');

    let $child_content = gobj_read_attr(gobj2, "$container");
    $child_content.classList.add("is-hidden");
    $sub_container.appendChild($child_content);
}

/************************************************************
 *   Destroy UI
 ************************************************************/
function remove_tab(gobj, gobj2, id)
{
    let $container = gobj_read_attr(gobj, "$container");
    let $a = $container.querySelector(`a[href="${id}"]`);
    if($a) {
        let $li = $a.parentNode;
        if($li) {
            $li.classList.remove('is-active');
            if($li.parentNode) {
                $li.parentNode.removeChild($li);
            }
        }
    }
    // TODO remove sub-container
}

/************************************************************
 *  Command to remote service
 ************************************************************/
function request_treedb_descs(gobj)
{
    let gobj_remote_yuno = gobj_read_pointer_attr(gobj, "gobj_remote_yuno");
    if(!gobj_remote_yuno) {
        log_error(`${gobj_short_name(gobj)}: No gobj_remote_yuno defined`);
        return;
    }

    let command = "descs";
    let treedb_name = gobj_read_str_attr(gobj, "treedb_name");
    let kw = {
        service: treedb_name,
        treedb_name: treedb_name
    };

    let ret = gobj_command(
        gobj_remote_yuno,
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
    /*
     *  descs is a dict: { __snaps__: {…}, roles: {…}, users: {…} }
     *  Create a topic_form for each topic
     *  Add a Bulma tab for each topic
     */
    let gobj_remote_yuno = gobj_read_pointer_attr(gobj, "gobj_remote_yuno");
    let descs = gobj_read_attr(gobj, "descs");
    let system = gobj_read_bool_attr(gobj, "system");
    let treedb_name = gobj_read_str_attr(gobj, "treedb_name");
    for (const [key, desc] of Object.entries(descs)) {
        if(system) {
            if(key === "__snaps__") {
                continue;
            }
            if (key.substring(0, 2) !== "__") {
                continue;
            }
        } else {
            if (key.substring(0, 2) === "__") {
                continue;
            }
        }

        let kw_topic_form = {
            subscriber: gobj,  // HACK get all output events

            // TODO set according the authz
            //with_edit_button: true,
            with_new_button: true,
            with_delete_button: true,

            treedb_name: treedb_name,
            topic_name: key,
            desc: desc
        };

        let id = `${gobj_name(gobj)}?${desc.topic_name}`;
        let gobj_topic_form = gobj_create_service(
            id,
            "C_YUI_TREEDB_TOPIC_FORM",
            kw_topic_form,
            gobj
        );

        // TODO get icon from remote config
        add_tab(gobj, gobj_topic_form, id, key, "fa-solid fa-table");

        gobj_start(gobj_topic_form);
    }

    /*
     *  Active the last_selection
     */
    gobj_send_event(gobj, "EV_SHOW", {href: gobj_read_str_attr(gobj, "last_selection")}, gobj);

    /*
     *  Subscribe events to manage Ui_treedb_topic_form kids
     *  and get data
     */
    let kids = gobj_match_children(
        gobj,
        {
            __gclass_name__: "C_YUI_TREEDB_TOPIC_FORM"
        }
    );
    for(let i=0; i<kids.length; i++) {
        let kid = kids[i];
        let topic_name = gobj_read_attr(kid, "topic_name");

        gobj_subscribe_event(
            gobj_remote_yuno,
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
        gobj_subscribe_event(
            gobj_remote_yuno,
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
        gobj_subscribe_event(
            gobj_remote_yuno,
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
}

/********************************************
 *
 ********************************************/
function get_gobj_formtable(gobj, topic_name)
{
    let kids = gobj_match_children(
        gobj,
        {
            __gclass_name__: "C_YUI_TREEDB_TOPIC_FORM"
        }
    );

    for(let i=0; i<kids.length; i++) {
        let kid = kids[i];
        let topic_name_ = gobj_read_attr(kid, "topic_name");
        if(topic_name_ === topic_name) {
            return kid;
        }
    }

    return null;
}

/************************************************************
 *  Command to remote service
 ************************************************************/
function treedb_nodes(gobj, treedb_name, topic_name, options)
{
    let command = "nodes";

    let gobj_remote_yuno = gobj_read_pointer_attr(gobj, "gobj_remote_yuno");
    let kw = {
        service: treedb_name,
        treedb_name: treedb_name,
        topic_name: topic_name,
        options: options || {}
    };

    kw.__md_command__ = { // Data to be returned
        topic_name: topic_name,
    };

    // TODO review msg_iev_write_key(kw, "__topic_name__", topic_name);

    let ret = gobj_command(
        gobj_remote_yuno,
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
    let command = "create-node";

    let gobj_remote_yuno = gobj_read_pointer_attr(gobj, "gobj_remote_yuno");
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
    // TODO review msg_iev_write_key(kw, "__topic_name__", topic_name);

    let ret = gobj_command(
        gobj_remote_yuno,
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
    let command = "update-node";

    let gobj_remote_yuno = gobj_read_pointer_attr(gobj, "gobj_remote_yuno");
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
    // TODO review msg_iev_write_key(kw, "__topic_name__", topic_name);

    let ret = gobj_command(
        gobj_remote_yuno,
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
    let command = "delete-node";

    let gobj_remote_yuno = gobj_read_pointer_attr(gobj, "gobj_remote_yuno");
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
    // TODO review msg_iev_write_key(kw, "__topic_name__", topic_name);

    let ret = gobj_command(
        gobj_remote_yuno,
        command,
        kw,
        gobj
    );
    if(ret) {
        log_error(ret);
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
        schema = webix_msg.schema;
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
            let topic_name = kw_get_str(gobj, kw_command, "topic_name", "", kw_flag_t.KW_REQUIRED);
            if(result >= 0) {
                let gobj_topic_form = gobj_find_child(gobj, {
                    __gobj_name__: `${gobj_name(gobj)}?${topic_name}`
                });
                gobj_send_event(
                    gobj_topic_form,
                    "EV_LOAD_NODES",
                    data,
                    gobj
                );
            }
            break;

        case "create-node":
        case "update-node":
        case "delete-node":
            // Don't process by here, process on subscribed events.
            break;

        default:
            log_error(`${gobj_short_name(gobj)} Command unknown: ${command}`);
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
    let href = kw.href;

    let $container = gobj_read_attr(gobj, "$container");
    let $current_item = gobj_read_attr(gobj, "$current_item");
    let $a = $container.querySelector(`a[href="${href}"]`);
    if($a) {
        /*
         *  href pointing to inside gobj (with ? right part)
         */
        if($current_item) {
            $current_item.classList.remove('is-active');
        }
        $current_item = $a.parentNode;
        gobj_write_attr(gobj, "$current_item", $current_item);
        $current_item.classList.add('is-active');
    } else {
        /*
         *  href pointing without ? right part, select the first item
         */
        if(!$current_item) {
            // Get the first item
            $current_item = $container.querySelector(`li`);
            gobj_write_attr(gobj, "$current_item", $current_item);
        }
        if($current_item) {
            $current_item.classList.add('is-active');
        }
    }

    /*
     *  Save last selection, the topics can be not arrived yet.
     */
    gobj_write_attr(gobj, "last_selection", href);

    /*
     *  Show sub-container
     */
    if($current_item) {
        let gobj2 = $current_item.gobj;
        if(gobj2) {
            let $sub_container = gobj_read_attr(gobj2, "$container");
            if($sub_container) {
                $sub_container.classList.remove("is-hidden");
            }
            gobj_send_event(gobj2, "EV_SHOW", kw, gobj);
        }
    }

    return 0;
}

/************************************************************
 *   Parent (routing) inform us that we go hidden
 ************************************************************/
function ac_hide(gobj, event, kw, src)
{
    /*
     *  Deactivate tab
     */
    let $current_item = gobj_read_attr(gobj, "$current_item");
    if($current_item) {
        $current_item.classList.remove('is-active');
    }

    /*
     *  Hide sub-container
     */
    if($current_item) {
        let gobj2 = $current_item.gobj;
        if(gobj2) {
            let $sub_container = gobj_read_attr(gobj2, "$container");
            if($sub_container) {
                $sub_container.classList.add("is-hidden");
            }
            gobj_send_event(gobj2, "EV_HIDE", {}, gobj);
        }
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

    if(treedb_name === gobj_read_str_attr(gobj, "treedb_name")) {
        let gobj_formtable = get_gobj_formtable(gobj, topic_name);
        gobj_send_event(
            gobj_formtable,
            "EV_LOAD_NODE_CREATED",
            [node],
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
    let treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    let topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    let node = kw_get_dict_value(gobj, kw, "node", null, 0);

    if(treedb_name === gobj_read_str_attr(gobj, "treedb_name")) {
        let gobj_formtable = get_gobj_formtable(gobj, topic_name);
        gobj_send_event(
            gobj_formtable,
            "EV_LOAD_NODE_UPDATED",
            [node],
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
    let treedb_name = kw_get_str(gobj, kw, "treedb_name", "", 0);
    let topic_name = kw_get_str(gobj, kw, "topic_name", "", 0);
    let node = kw_get_dict_value(gobj, kw, "node", null, 0);

    if(treedb_name === gobj_read_str_attr(gobj, "treedb_name")) {
        let gobj_formtable = get_gobj_formtable(gobj, topic_name);
        gobj_send_event(
            gobj_formtable,
            "EV_NODE_DELETED",
            [node],
            gobj
        );
    }

    return 0;
}

/********************************************
 *  Event from formtable
 *  kw: {
 *      topic_name,
 *      record
 *  }
 ********************************************/
function ac_create_record(gobj, event, kw, src)
{
    let treedb_name = gobj_read_str_attr(gobj, "treedb_name");
    let topic_name = gobj_read_attr(src, "topic_name");
    let record = kw.record;

    let options = {
        list_dict: true,
        create: true,
        autolink: true
    };

    return treedb_update_node( // HACK use the powerful update_node
        gobj,
        treedb_name,
        topic_name,
        record,
        options
    );
}

/********************************************
 *  Event from formtable
 *  kw: {
 *      topic_name,
 *      record
 *  }
 ********************************************/
function ac_update_record(gobj, event, kw, src)
{
    let treedb_name = gobj_read_str_attr(gobj, "treedb_name");
    let topic_name = gobj_read_attr(src, "topic_name");
    let record = kw.record;

    let options = {
        list_dict: true,
        autolink: true
    };

    return treedb_update_node(
        gobj,
        treedb_name,
        topic_name,
        record,
        options
    );
}

/********************************************
 *  Event from formtable
 *  kw: {
 *      topic_name,
 *      record
 *  }
 ********************************************/
function ac_delete_record(gobj, event, kw, src)
{
    let treedb_name = gobj_read_str_attr(gobj, "treedb_name");
    let topic_name = gobj_read_attr(src, "topic_name");
    let record = kw.record;
    let options = {
        force: true
    };

    return treedb_delete_node(
        gobj,
        treedb_name,
        topic_name,
        record,
        options
    );
}

/********************************************
 *  Event from formtable
 *  kw: {
 *      topic_name,
 *  }
 ********************************************/
function ac_refresh_topic(gobj, event, kw, src)
{
    let options = {
        list_dict: true
    };

    let treedb_name = gobj_read_str_attr(gobj, "treedb_name");

    treedb_nodes(
        gobj,
        treedb_name,
        kw.topic_name,
        options
    );

    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
const gmt = {
    mt_create:          mt_create,
    mt_start:           mt_start,
    mt_stop:            mt_stop,
    mt_destroy:         mt_destroy,
    mt_command_parser:  mt_command_parser,
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
            ["EV_MT_COMMAND_ANSWER",    ac_mt_command_answer,       null],
            ["EV_TREEDB_NODE_CREATED",  ac_treedb_node_created,     null],
            ["EV_TREEDB_NODE_UPDATED",  ac_treedb_node_updated,     null],
            ["EV_TREEDB_NODE_DELETED",  ac_treedb_node_deleted,     null],
            ["EV_CREATE_RECORD",        ac_create_record,           null],
            ["EV_UPDATE_RECORD",        ac_update_record,           null],
            ["EV_DELETE_RECORD",        ac_delete_record,           null],
            ["EV_REFRESH_TOPIC",        ac_refresh_topic,           null],
            ["EV_SHOW",                 ac_show,                    null],
            ["EV_HIDE",                 ac_hide,                    null],
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
        ["EV_REFRESH_TOPIC",        0],
        ["EV_SHOW",                 0],
        ["EV_HIDE",                 0]
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
function register_c_yui_treedb_topics()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yui_treedb_topics };
