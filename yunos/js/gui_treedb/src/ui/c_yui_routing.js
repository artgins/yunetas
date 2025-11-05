/***********************************************************************
 *          ui_routing.js
 *
 *          Manage routing (Menu/Contents), a grid of 2 columns,
 *          left with menu, right with content
 *
 *          container-grid-2col
 *              container-grid-2col-left container-grid-2col-right
 *
 *          The ID "main-content-container" is the REFERENCE to know the work zone.
 *
 *          Each gobj must supply his $container.
 *          The routing is responsible to show or hide the container,
 *          according to desires of user clicking in the main left menu.
 *          Each gobj has an entry in the aside menu.
 *          The object himself is directed by #hash-left part of url:
 *
 *                  #hash-left?right-path
 *
 *          The right-path of url is managed inside of gobj, as he wants.
 *
 *          Main goal of routing is hide/show the containers;
 *          routing send EV_SHOW/EV_HIDE events to
 *          respect gobj, to let to do any extra own internal job.
 *
 *
 *          There is a main menu (aside left menu) that manage the content (right side of page).
 *
 *          The routing or the selection of different parts of app is done
 *          by changing the builtin DOM location.hash,
 *          to allow the user navigate backward/forward in the app.
 *
 *          The callback of 'hashchange' event is:
 *
 *              window.addEventListener('hashchange', gobj.priv.hashchange_handler);
 *
 *          The hashchange_handler() will call to
 *
 *              select_item(gobj, location.hash);
 *
 *          What select_item() do?
 *              - Split the href in two, split by '?'.
 *                  IMPORTANT, the first href part is the ID of gobj service that will be found.
 *
 *              - The first part of href will manage the main menu (left aside menu) :
 *                  1) - Unselect the current menu item ($current_item)
 *                       (removing 'is-active')
 *                     - Calling to own mt_hide():
*                      - Send EV_HIDE to associated gobj
 *
 *                  2) - Select the new menu item defined by href first part
 *                       (adding 'is-active')
 *                     - Calling to own mt_show():
*                      - Send EV_SHOW to associated gobj
 *
 *              - The second part of href (right of '?') must be processed by gobj found,
 *                that it must manage his internal selection defined by that href part.
 *
 *
 *  External menu selection can come from
 *      1) startup sending EV_SELECT event with last_selected_menu, forcing change in hash
 *          ==> priv.hashchange_handler()
 *              ==> select_item()
 *      2) user change url (hash) in browser
 *          ==> priv.hashchange_handler()
 *              ==> select_item()
 *
 *  Internal menu selection comes form click in menu item:
 *      3) The click in <a> with href provokes a new hash
 *          ==> priv.hashchange_handler()
 *              ==> select_item()
 *
 *  Use yui_tabs to manage sub-containers or integrate in a gobj.
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

import {
    __yuno__,
    SDATA,
    SDATA_END,
    data_type_t,
    gclass_flag_t,
    sdata_flag_t,
    gclass_create,
    log_error,
    gobj_subscribe_event,
    gobj_read_pointer_attr,
    gobj_send_event,
    sprintf,
    createElement2,
    is_string,
    is_array,
    createOneHtml,
    kw_set_local_storage_value,
    gobj_find_service,
    gobj_read_attr,
    gobj_write_attr,
    gobj_save_persistent_attrs,
    gobj_is_running,
    refresh_language,
} from "yunetas";

import "./c_yui_routing.css"; // Must be in index.js ?

import {t} from "i18next";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_ROUTING";

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",       0,  null,   "Subscriber of output events"),
SDATA(data_type_t.DTP_POINTER,  "$parent",          0,  null,   "HTML content parent"),
SDATA(data_type_t.DTP_POINTER,  "$container",       0,  null,   "Layout set by this"),
SDATA(data_type_t.DTP_JSON,     "menu",             0,  "[]",   "Menu tree, [{ Menu/Content}], See info at build_menu_r(). If exists at start will be decoded"),
SDATA(data_type_t.DTP_INTEGER,  "menu_state",       sdata_flag_t.SDF_PERSIST,  "1", "0: hide all, 1: show all, 2: show icons only"),
SDATA(data_type_t.DTP_POINTER,  "$current_item",    0,  null,   "Current selected menu item"),
SDATA_END()
];

let PRIVATE_DATA = {
    hashchange_handler: null,   // fixed hashchange_handler to can add/remove listener
    isMobile:           false
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

    build_layout(gobj);

    /*
     *  SERVICE subscription model
     */
    const subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, null, {}, subscriber);
    }

    let __yui_main__ = gobj_find_service("__yui_main__", true);
    if(__yui_main__) {
        gobj_subscribe_event(__yui_main__, "EV_APP_MENU", {}, gobj);
        gobj_subscribe_event(__yui_main__, "EV_RESIZE", {}, gobj);
    }

    /*
     *  hashchange handler
     */
    priv.hashchange_handler = function(evt) {
        /*
         *  Get the segments of hash
         */
        gobj_send_event(gobj, "EV_ROUTE", {hash: location.hash}, gobj);
    };
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    build_menu(gobj, gobj_read_attr(gobj, "menu"));

    window.addEventListener('hashchange', gobj.priv.hashchange_handler);
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    destroy_menu(gobj);

    window.removeEventListener('hashchange', gobj.priv.hashchange_handler);
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
    destroy_layout(gobj);
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_command_parser(gobj, command, kw, src)
{
    switch(command) {
        case "help":
            return cmd_help(gobj, command, kw, src);
        case "append-menu":
            return cmd_manage_menu(gobj, command, kw, src);
        case "insert-menu":
            return cmd_manage_menu(gobj, command, kw, src);
        case "remove-menu":
            return cmd_manage_menu(gobj, command, kw, src);
        case "append-content":
            return cmd_manage_content(gobj, command, kw, src);
        case "insert-content":
            return cmd_manage_content(gobj, command, kw, src);
        case "remove-content":
            return cmd_manage_content(gobj, command, kw, src);
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
    let webix = {
        "result": 0,
        "comment": "",
        "schema": null,
        "data": null
    };
    return webix;
}

/************************************************************
 *
 ************************************************************/
function cmd_manage_menu(gobj, command, kw, src)
{
    switch(command) {
    case "append-menu":
        break;
    case "insert-menu":
        break;
    case "remove-menu":
        break;
    default:
        break;
    }
    return null;
}

/************************************************************
 *
 ************************************************************/
function cmd_manage_content(gobj, command, kw, src)
{
    switch(command) {
    case "append-content":
        break;
    case "insert-content":
        break;
    case "remove-content":
        break;
    default:
        break;
    }
    return null;
}



                    /***************************
                     *      Local Methods
                     ***************************/




/************************************************************
 *
 ************************************************************/
function build_layout(gobj)
{
    /*----------------------------------------------*
     *  Layout Schema
     *----------------------------------------------*/
    const $container = createElement2(
        ['div', {class: 'container-grid-2col', style: ''}, [
            ['div', {class: 'container-grid-2col-left'},
                ['aside', {class: 'menu'}, [
                    ['ul', {class: 'menu-list'}, []]
                ]]
            ],
            ['div', {
                id: 'main-content-container',
                class: 'container-grid-2col-right main-content-container'}, []
            ]
        ]]
    );
    gobj_write_attr(gobj, "$container", $container);
    refresh_language($container, t);

    let $parent = gobj_read_attr(gobj, "$parent");
    if($parent) {
        $parent.appendChild($container);
    }
    return 0;
}

/************************************************************
 *
 ************************************************************/
function destroy_layout(gobj)
{
    return 0;
}

/************************************************************
 *  Recurrent
    menu: [{}]
        You can pass a {} and a internal <li><a> will be built,
        or you can pass an already built HTMLElement.

    $menu_list_parent:  Menu parent where the menu items will be added.

    $content_container_parent:  Container parent where the containers will be added.


    Menu item structure:
    {
        id:         string  (if null then it's a title) ==> will be the href IMPORTANT!
        label:      string (i18n)
        icon:       string (fontawesome)
        attrs:      {} html attributes for <li>
        events:     {} html event functions {event: fn()},

        gobj:       gobj implementing the content of container
                    It must have "$container" attribute,
                        that it has precedence over below 'container' parameter
                    It must support EV_SHOW/EV_HIDE events

        menu:       [] submenu children, this is a tree structure

        html:       it not null this 'HTMLElement' or 'html string' or [createElement2]
                    will be used instead of to build the internal  <li><a>

        container:  it not null this 'HTMLElement' or 'html string' or [createElement2]
                    will be used instead of to build the internal  <li><a>

                    if null and gobj have "$container" attribute it will be used

        mt_show:
                    ($container) ==> {$container.classList.remove('is-hidden');}
        mt_hide:
                    ($container) ==> {$container.classList.add('is-hidden');}

    }

    By default, the built structure is, with id:

        <li "attrs">        <== attrs
            <a "events|inside-click" "href=id" "i18n=label|id" >
                label|id
            </a>
        </li>

    without id (title):

        <p "i18n:label">
            label
        </p>

 ************************************************************/
function build_menu_r(gobj, menu, $menu_list_parent, $content_container_parent)
{
    function mt_show($container)
    {
        $container.classList.remove('is-hidden');
    }

    function mt_hide($container)
    {
        $container.classList.add('is-hidden');
    }

    for (let i = 0; i < menu.length; i++) {
        let item = menu[i];
        let $item;
        let create_container = true;
        let icon = item.icon || '';

        /*--------------------------------------------*
         *              Aside menu
         *--------------------------------------------*/
        let html = item.html;
        if(html && (html instanceof HTMLElement) || is_string(html) || is_array(html)) {
            /*-----------------------*
             *  External built
             *-----------------------*/
            if(html instanceof HTMLElement) {
                $item = html;
            } else if(is_string(html)) {
                $item = createOneHtml(html);
            } else if(is_array(html)) {
                $item = createElement2(html);
            }
        } else {
            /*-----------------------*
             *  Internal built
             *-----------------------*/
            if (item.id) {
                $item = createElement2(
                    ['li', item.attrs ? item.attrs : {}, [
                        [
                            'a',
                            {
                                // gobj_name, the first with '#', children with '/' '?' ???
                                id: item.id,
                                href: item.id,
                                // class: 'icon-text',
                            },
                            [
                                ['span',
                                    {
                                        class: `icon ${icon?'':'is-hidden'}`
                                    },
                                    ['i', {class: `${icon}`}]
                                ],
                                ['span',
                                    {
                                        class: 'menu-text pl-3',
                                        i18n: item.label?item.label:item.id
                                    },
                                    item.label?item.label:item.id
                                ],
                                ['span',
                                    {
                                        class: 'yui-badge tag ml-2 is-rounded is-hidden',
                                    },
                                    ''
                                ]
                            ],
                            item.events ? item.events : {}
                        ]
                    ]]
                );

            } else {
                /*
                 *  If id is empty it's title
                 */
                $item = createElement2(
                    ['p', {class: 'menu-label', i18n: item.label}, item.label]
                );
                create_container = false;
            }
        }

        $menu_list_parent.appendChild($item);

        if(!create_container) {
            continue;
        }

        /*--------------------------------------------*
         *              Content
         *--------------------------------------------*/
        let $container;
        let container;
        if(item.gobj) {
            container = gobj_read_attr(item.gobj, "$container");
        }
        if(!container) {
            container = item.container;
        }
        if(container && (container instanceof HTMLElement) || is_string(container)) {
            /*-----------------------*
             *  External built
             *-----------------------*/
            if(container instanceof HTMLElement) {
                $container = container;
            } else if(is_string(container)) {
                $container = createOneHtml(container);
            } else if(is_array(container)) {
                $container = createElement2(container);
            }
        } else {
            /*-----------------------*
             *  Internal built
             *-----------------------*/
            $container = createElement2(
                ['div', {}, `You must supply content for ${item.id}`]
            );
        }

        $container.classList.add('is-hidden');
        $content_container_parent.appendChild($container);

        /*-------------------------------*
         *      Link to gobj's
         *-------------------------------*/
        let $item_a = $item.querySelector('a');
        $item_a.$container = $container;
        $item_a.gobj = item.gobj || null;
        $container.gobj = item.gobj || null;
        $item_a.mt_show = item.mt_show || mt_show;
        $item_a.mt_hide = item.mt_hide || mt_hide;

        /*-------------------------------*
         *      Submenus
         *-------------------------------*/
        if(item.submenu && item.submenu.length > 0) {
            let $submenu = createElement2(
                ['ul', {}, []]
            );
            $item.appendChild($submenu);
            build_menu_r(gobj, item.submenu, $submenu, $content_container_parent);
        }
    }
}

/************************************************************
 *
 ************************************************************/
function build_menu(gobj, menu)
{
    /*-------------------------------*
     *      First level
     *-------------------------------*/
    let $container = gobj_read_attr(gobj, "$container");
    let $menu_list = $container.querySelector('.menu-list');
    let $content_container = $container.querySelector('.main-content-container');

    build_menu_r(gobj, menu, $menu_list, $content_container);
    refresh_language($container, t);

    set_menu_state(gobj, gobj_read_attr(gobj, "menu_state"));

    return 0;
}

/************************************************************
 *
 ************************************************************/
function destroy_menu(gobj, event, kw, src)
{
    window.location.hash = '';

    let $container = gobj_read_attr(gobj, "$container");
    let $menu_list = $container.querySelector('.menu-list');
    let $content_container = $container.querySelector('.main-content-container');

    while ($menu_list.firstChild) {
        $menu_list.removeChild($menu_list.firstChild);
    }
    while ($content_container.firstChild) {
        $content_container.removeChild($content_container.firstChild);
    }

    return 0;
}

/************************************************************
 *  Select item with href that is the same as gobj's id
 ************************************************************/
function select_item(gobj, href)
{
    /*
     *  Recover the menu item
     */
    let gobj_href = href.split('?')[0];
    let $container = gobj_read_attr(gobj, "$container");
    let $item = $container.querySelector(`a[href="${gobj_href}"]`);
    if(!$item) {
        return;
    }

    /*
     *  Clean all active items (with current is enough)
     */
    let $current_item = gobj_read_attr(gobj, "$current_item");
    if($current_item) {
        /*
         *  Unselect menu item
         */
        $current_item.classList.remove('is-active');
        /*
         *  Hide container
         */
        $current_item.mt_hide($current_item.$container);

        /*
         *  Call gobj_hide
         */
        let gobj_hide = $current_item.gobj ||
            gobj_find_service($current_item.getAttribute('href'));

        if(gobj_hide && gobj_is_running(gobj_hide)) {
            gobj_send_event(gobj_hide, "EV_HIDE", {}, gobj);
        }
    }

    /*
     *  Active this item
     */
    gobj_write_attr(gobj, "$current_item", $item);
    $current_item = gobj_read_attr(gobj, "$current_item");
    $current_item.classList.add('is-active');

    /*
     *  Show container
     */
    // $current_item.mt_show(gobj.config.$current_item.$container); // TODO esto había
    $current_item.mt_show($current_item.$container); // TODO ??

    /*
     *  Call gobj_show
     */
    let gobj_show = $item.gobj || gobj_find_service(gobj_href);
    if(gobj_show && gobj_is_running(gobj_show)) {
        gobj_send_event(gobj_show, "EV_SHOW", {href:href}, gobj);
    }

    // TODO per user?
    kw_set_local_storage_value("last_selected_menu", href);
}

/************************************************************
 *
 ************************************************************/
function set_menu_state(gobj, state)
{
    let $container = gobj_read_attr(gobj, "$container");
    let $col_left = $container.querySelector('.menu');
    switch(state) {
        case 0: // Hide ALL
            $col_left.classList.add('is-hidden');
            $container.querySelectorAll('.menu-text, .menu-label').forEach(link => {
                link.classList.add('is-hidden');
            });

            break;

        case 1: // Show ALL
            $col_left.classList.remove('is-hidden');
            $container.querySelectorAll('.menu-text, .menu-label').forEach(link => {
                link.classList.remove('is-hidden');
            });
            break;

        case 2: // Show Icon, hide text
            $col_left.classList.remove('is-hidden');
            $container.querySelectorAll('.menu-text, .menu-label').forEach(link => {
                link.classList.add('is-hidden');
            });
            break;
    }

    gobj_write_attr(gobj, "menu_state", state);
}




                    /***************************
                     *      Actions
                     ***************************/




/************************************************************
 *  User has clicked in the app menu (top left corner button)
 *  (We are subscribed to event of __main__ )
 *  Action: show or hide or show less the main left menu
 ************************************************************/
function ac_app_menu(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let state = gobj_read_attr(gobj, "menu_state");
    if(priv.isMobile) {
        state = ++state % 2;
    } else {
        state = ++state % 3;
    }
    set_menu_state(gobj, state);
    gobj_save_persistent_attrs(gobj, "menu_state");

    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_resize(gobj, event, kw, src)
{
    let priv = gobj.priv;

    priv.isMobile = window.matchMedia("(max-width: 768px)").matches;
    if(priv.isMobile) {
        set_menu_state(gobj, 0); // hide all
    }
    return 0;
}

/************************************************************
 *  From startup sending EV_SELECT event with last_selected_menu
 ************************************************************/
function ac_select(gobj, event, kw, src)
{
    /*
     *  ==> this case: simulate change in hash
     */
    window.location.hash = kw.id; // This will call priv.hashchange_handler() ==> select_item()

    return 0;
}

/************************************************************
 *  From callback priv.hashchange_handler()
 ************************************************************/
function ac_route(gobj, event, kw, src)
{
    let priv = gobj.priv;

    select_item(gobj, kw.hash);
    if(priv.isMobile) {
        set_menu_state(gobj, 0);  // Hide all
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
    if (__gclass__) {
        log_error(`GClass ALREADY created: ${gclass_name}`);
        return -1;
    }

    /*---------------------------------------------*
     *          States
     *---------------------------------------------*/
    const states = [
        ["ST_IDLE", [
            ["EV_ROUTE",        ac_route,       null],
            ["EV_APP_MENU",     ac_app_menu,    null],
            ["EV_RESIZE",       ac_resize,      null],
            ["EV_SELECT",       ac_select,      null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_ROUTE",       0],
        ["EV_APP_MENU",    0],
        ["EV_RESIZE",      0],
        ["EV_SELECT",      0]
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
        gclass_flag_t.gcflag_manual_start   // gclass_flag
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************
 *          Register GClass
 ***************************************************************/
function register_c_yui_routing()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yui_routing };
