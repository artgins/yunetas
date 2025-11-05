/***********************************************************************
 *          c_ui_monitoring.js
 *
 *          Monitoring real-time data
 *
 *          Es el container de los tabs
 *          Cada tab (c_ui_monitoring_group)
 *              es la monitorización de los devices del grupo
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    __yuno__,
    SDATA,
    SDATA_END,
    data_type_t,
    gclass_create,
    log_error,
    gobj_subscribe_event,
    gobj_read_pointer_attr,
    gobj_parent,
    createElement2,
    clean_name,
    gobj_write_attr,
    gobj_read_attr,
    gobj_send_event,
    gobj_find_service,
    gobj_name,
    gobj_create_service,
    gobj_start,
    gobj_destroy,
    event_flag_t,
    gobj_stop_tree,
    gobj_stop,
    gobj_stop_children,
    refresh_language,
} from "yunetas";

import {display_error_message} from "./ui/c_yui_main.js";

import {t} from "i18next";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_UI_MONITORING";

/***************************************************************
 *              Data
 ***************************************************************/
/*
 *  Helpers of Bulma tabs
 *  Style:                   is-boxed
 *  Sizes:                   is-small is-medium is-large
 *  Alignment:               is-centered is-right
 *  Mutually exclusive:      is-toggle is-toggle-rounded
 *  Whole width available:   is-fullwidth
 */
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",       0,  null,   "Subscriber of output events"),
SDATA(data_type_t.DTP_POINTER,  "gobj_remote_yuno", 0,  null,   "Remote yuno to ask data"),
SDATA(data_type_t.DTP_BOOLEAN,  "autorefresh",      0,  false,  "Enable or disable auto-refresh, from children and to test, seems"),
SDATA(data_type_t.DTP_STRING,   "tabs_style",       0,  "is-toggle is-large is-centered is-toggle-rounded", "Bulma tabs style"),
SDATA(data_type_t.DTP_POINTER,  "$container",       0,  null,   "Container element"),
SDATA(data_type_t.DTP_POINTER,  "$current_item",    0,  null,   "Current item selected"),
SDATA(data_type_t.DTP_STRING,   "last_selection",   0,  "",     "Last href received"),
SDATA(data_type_t.DTP_STRING,   "label",            0,  "monitoring", "Label"),
SDATA(data_type_t.DTP_STRING,   "icon",             0,  "fab fa-cloudversify", "Icon class"),
SDATA_END()
];

let PRIVATE_DATA = {
    groups: null
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
    start_flow(gobj);
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

/***************************************************************
 *          Framework Method: Child removed
 ***************************************************************/
function mt_child_removed(gobj, child)
{
    // TODO review
    remove_tab(gobj, child);
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
    let tabs_style = gobj_read_attr(gobj, "tabs_style");
    let $container = createElement2(
        // Don't use is-flex, don't work well with is-hidden
        ['div', {class: 'monitoring', style: 'height:100%; display:flex; flex-direction:column;'}, [
            ['div', {class: 'is-flex-grow-0'}, [
                ['div', {class: `tabs ${tabs_style}`, style: 'margin-bottom: 10px;'}, [
                    ['ul', {}]
                ]],
            ]],
            ['div', {class: 'is-flex-grow-1 sub-container', style: 'height:100%; overflow: auto;'}, []
            ]
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
function start_flow(gobj)
{
    /*
     *  Subscribe events
     */
    // De momento sin subscripción
//         gobj_subscribe_event(
//             gobj_remote_yuno,
//             "EV_LIST_GROUPS",
//             {
//                 __filter__: {
//                 }
//             },
//             gobj
//         );

    /*
     *  Refresca datos
     */
    refresh_groups(gobj);
}

/************************************************************
 *
 ************************************************************/
function refresh_groups(gobj)
{
    /*
     *  Ask for groups
     */
    let gobj_remote_yuno = gobj_read_attr(gobj, "gobj_remote_yuno");
    let ret = gobj_send_event(
        gobj_remote_yuno,
        "EV_LIST_GROUPS",
        {},
        gobj
    );
    if(ret) {
        log_error(ret);
    }
}

/************************************************************
 *  Response of remote command
 *  [
 *      {
 *          id: {"id of group"}
 *          devices: [
 *              {
 *                  id: {"id of device"}
 *                  topic_name: "devices"
 *              },
 *          ]
 *      },
 *  ]
 ************************************************************/
function procesa_groups(gobj, groups)
{
    let priv = gobj.priv;

    let gobj_remote_yuno = gobj_read_attr(gobj, "gobj_remote_yuno");

    /*
     *  Order can come from creating app or refresh button
     */
    priv.groups = groups;

    /*
     *  Create groups
     */
    for (let i=0; i<groups.length; i++) {
        let group = groups[i];

        let kw_group = {
            subscriber: gobj,  // HACK get all output events
            gobj_remote_yuno: gobj_remote_yuno,
            group_id: group.id,
            devices: group.devices
        };

        let group_id = clean_name(group.id);
        let id = `${gobj_name(gobj)}?${group_id}`;
        let gobj_group = gobj_find_service(id);
        if(!gobj_group) {
            // Could come repeated groups
            gobj_group = gobj_create_service(
                id,
                "C_UI_MONITORING_GROUP",
                kw_group,
                gobj
            );
            if(gobj_group) {
                add_tab(gobj, gobj_group, id, group.id, group.icon);
                gobj_start(gobj_group);
            }
        }
    }

    /*
     *  Active the last_selection
     */
    gobj_send_event(gobj, "EV_SHOW", {href: gobj_read_attr(gobj, "last_selection")}, gobj);
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
                ['span', {class: 'icon is-large'},
                    ['img', {src: `${icon}`, alt: `${icon}`}]
                ]
            )
        );
    }

    /*
     *  TAB: Add text
     */
    $a.appendChild(
        createElement2(['span', {i18n: text, class: 'is-hidden-mobile'}, text])
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
function remove_tab(gobj, gobj2)
{
    let id = gobj_name(gobj2);
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

    /*
     *  SUB-CONTAINER, remove child content
     */
    // Se borra al destroy the child
}




                    /***************************
                     *      Actions
                     ***************************/




/************************************************************
 *  Remote response
 ************************************************************/
function ac_list_groups(gobj, event, kw, src)
{
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
        return -1;
    }
    if(result < 0) {
        display_error_message(
            "Error",
            t(comment)
        );
        // HACK don't return, pass errors when need it.
    }

    procesa_groups(gobj, data);

    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_refresh(gobj, event, kw, src)
{
    let priv = gobj.priv;

    gobj_write_attr(gobj, "$current_item", null);

    /*
     *  Remove groups
     */
    for (let i=0; i<priv.groups.length; i++) {
        let group = priv.groups[i];

        let group_id = clean_name(group.id);
        let id = `${gobj_name(gobj)}?${group_id}`;
        let gobj_group = gobj_find_service(id);
        if(gobj_group) {
            gobj_stop(gobj_group);
            gobj_destroy(gobj_group);
        }
    }
    priv.groups = null;

    start_flow(gobj);

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
    let $a = $container.querySelector(`a[href="${href}"]`);
    let $current_item = gobj_read_attr(gobj, "$current_item");
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




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
const gmt = {
    mt_create:  mt_create,
    mt_start:   mt_start,
    mt_stop:    mt_stop,
    mt_destroy: mt_destroy,
    mt_child_removed: mt_child_removed,
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
            ["EV_LIST_GROUPS",          ac_list_groups,         null],
            ["EV_REFRESH",              ac_refresh,             null],
            ["EV_SHOW",                 ac_show,                null],
            ["EV_HIDE",                 ac_hide,                null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_LIST_GROUPS",          event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_REFRESH",              0],
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
function register_c_ui_monitoring()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_ui_monitoring };
