/***********************************************************************
 *          c_yui_tabs.js
 *
 *  Container of sub-containers, selected by tabs
 *
 *  The tabs is created when a child is append, and remove when child is destroyed.
 *  This child must have another subscriber than this yui_tabs.
 *
 *  The child must manage the sub-container and must supply next attributes
 *  in order to create and select the tab:
 *
 *      "href": (optional) Href to select the tab.
 *          If href attr not set or not exists then is set internally:
 *              `{yui_tabs_name}?{service_name_of_child}`
 *                    |-> yui_tabs_name must have '#' as prefix !!
 *
 *          yui_ui_routing manage the href.
 *
 *      "label":    String or html? insert as text in the tab.
 *      "icon":     Icon to show in the tab.
 *      "image":    Image to show in the tab.
 *
 *      Use icon or image, not both
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
    gobj_write_attr,
    gobj_read_attr,
    gobj_send_event,
    gobj_name,
    gobj_read_str_attr,
    empty_string,
    clean_name,
    gobj_find_service,
    gobj_short_name,
    gobj_write_str_attr,
    gobj_has_attr, gobj_is_service, gobj_stop_children,
} from "yunetas";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_TABS";

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

SDATA(data_type_t.DTP_STRING,   "tabs_style",       0,  "is-toggle is-medium is-centered is-toggle-rounded", "Bulma tabs style, see https://bulma.io/documentation/components/tabs/"),
SDATA(data_type_t.DTP_STRING,   "image_style",      0,  "image is-medium", "Image bulma style, see https://bulma.io/documentation/elements/image/"),
SDATA(data_type_t.DTP_STRING,   "icon_style",       0,  "icon is-medium", "Icon bulma style, see https://bulma.io/documentation/elements/icon/"),

/*---------------- UI/Routing ----------------*/
SDATA(data_type_t.DTP_POINTER,  "$container",       0,  null,   "HTMLElement root, show/hide managed by external routing"),
SDATA(data_type_t.DTP_POINTER,  "$current_item",    0,  null,   "Current item selected"),
SDATA(data_type_t.DTP_STRING,   "last_selection",   0,  "",     "Last href selected"),
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
    /*
     *  Active the last_selection
     */
    // gobj_send_event(gobj, "EV_SHOW", {href: gobj_read_attr(gobj, "last_selection")}, gobj);
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
 *          Framework Method: Child added
 ***************************************************************/
function mt_child_added(gobj, child)
{
    let href = "";
    if(gobj_has_attr(child, "href")) {
        href = gobj_read_str_attr(child, "href");
    }
    if(empty_string(href)) {
        href = `${gobj_name(gobj)}?${clean_name(gobj_name(child))}`;
    }

    add_tab(gobj, child, href);
}

/***************************************************************
 *          Framework Method: Child removed
 ***************************************************************/
function mt_child_removed(gobj, child)
{
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
        ['div', {class: `yui_tabs`, style: 'height:100%; display:flex; flex-direction:column;'}, [
            ['div', {class: 'is-flex-grow-0'}, [
                ['div', {class: `tabs ${tabs_style}`, style: 'margin-bottom: 10px;'}, [
                    ['ul', {}]
                ]],
            ]],
            ['div', {class: 'is-flex-grow-1 sub-container', style: 'height:100%;min-height:0;'}, []
            ]
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
function add_tab(gobj, child, href)
{
    let text = gobj_read_attr(child, "label");
    let icon = gobj_read_attr(child, "icon");
    let image = gobj_read_attr(child, "image");

    let $container = gobj_read_attr(gobj, "$container");
    let $tabs = $container.querySelector('ul');

    /*
     *  Create li a
     */
    let $item = createElement2(
        ['li', {class: ''}, [ // is-active
            ['a', {href: href}]
        ]]
    );
    $tabs.appendChild($item);
    $item.gobj = child; // Cross-reference

    /*
     *  TAB: Add icon/text to a
     */
    let $a = $item.querySelector('a');

    /*
     *  Add image
     */
    if(!empty_string(image)) {
        let image_style = gobj_read_attr(gobj, "image_style");
        $a.appendChild(
            createElement2(
                ['span', {class: `${image_style}`},
                    ['img', {src: `${image}`, alt: `${image}`}]
                ]
            )
        );
    } else if(!empty_string(icon)) {
        let icon_style = gobj_read_attr(gobj, "icon_style");
        $a.appendChild(
            createElement2(
                ['span', {class: `${icon_style}`},
                    ['i', {class: `${icon}`}]
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

    let $child_content = gobj_read_attr(child, "$container");
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
    mt_child_added: mt_child_added,
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
            ["EV_SHOW",                 ac_show,                null],
            ["EV_HIDE",                 ac_hide,                null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
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
function register_c_yui_tabs()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yui_tabs };
