/***********************************************************************
 *          c_yui_nav.js
 *
 *      C_YUI_NAV — Renders a menu (list of items) in one zone, using one
 *      of the supported layouts.  Driven by C_YUI_SHELL; one instance per
 *      (menu, zone) pair.
 *
 *      Layouts:
 *          "vertical"   — Bulma .menu (left/right side nav)
 *          "icon-bar"   — horizontal bar of icon+label (bottom/top on mobile)
 *          "tabs"       — Bulma .tabs (secondary nav)
 *          "drawer"     — off-canvas vertical (toggled by hamburger)
 *          "submenu"    — Bulma .menu nested under a heading
 *          "accordion"  — Bulma .menu list with collapsible group
 *
 *      Each item supports:
 *          id, name, icon (CSS class or svg id), route, badge, disabled
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
/* global document */

import {
    SDATA, SDATA_END, data_type_t,
    gclass_create, log_error,
    gobj_subscribe_event, gobj_parent,
    gobj_read_attr, gobj_write_attr,
    gobj_name,
    createElement2, empty_string, is_array, is_object, is_string,
} from "@yuneta/gobj-js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_NAV";

const SUPPORTED_LAYOUTS = [
    "vertical", "icon-bar", "tabs", "drawer", "submenu", "accordion"
];


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",    0,  null,         "Subscriber of output events"),

SDATA(data_type_t.DTP_STRING,   "menu_id",       0,  "",           "Id of the menu this nav renders"),
SDATA(data_type_t.DTP_JSON,     "menu_items",    0,  null,         "Array of menu items to render"),
SDATA(data_type_t.DTP_STRING,   "zone",          0,  "",           "Zone id where this nav lives"),
SDATA(data_type_t.DTP_STRING,   "layout",        0,  "vertical",   "vertical|icon-bar|tabs|drawer|submenu|accordion"),
SDATA(data_type_t.DTP_STRING,   "icon_pos",      0,  "left",       "left|right|top|bottom"),
SDATA(data_type_t.DTP_BOOLEAN,  "show_label",    0,  true,         "Show item label next to/under the icon"),
SDATA(data_type_t.DTP_STRING,   "level",         0,  "primary",    "primary|secondary"),

SDATA(data_type_t.DTP_POINTER,  "shell",         0,  null,         "C_YUI_SHELL gobj (for navigation calls)"),
SDATA(data_type_t.DTP_POINTER,  "$container",    0,  null,         "Root HTMLElement of this nav"),
SDATA(data_type_t.DTP_STRING,   "active_route",  0,  "",           "Currently active route"),
SDATA_END()
];

let PRIVATE_DATA = {};
let __gclass__ = null;


                    /******************************
                     *      Framework Methods
                     ******************************/


function mt_create(gobj)
{
    let subscriber = gobj_read_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    if(subscriber) {
        gobj_subscribe_event(gobj, null, {}, subscriber);
    }
    build_ui(gobj);
}

function mt_start(gobj)
{
    let shell = gobj_read_attr(gobj, "shell");
    if(shell) {
        /*  Listen to route changes from the shell so we can highlight the
         *  active item.  */
        try {
            gobj_subscribe_event(shell, "EV_ROUTE_CHANGED", {}, gobj);
        } catch(e) { /* already subscribed, ignore */ }
    }
}

function mt_stop(gobj)
{
}

function mt_destroy(gobj)
{
    let $c = gobj_read_attr(gobj, "$container");
    if($c && $c.parentNode) {
        $c.parentNode.removeChild($c);
    }
    gobj_write_attr(gobj, "$container", null);
}


                    /******************************
                     *      Local Methods
                     ******************************/


function build_ui(gobj)
{
    let layout = gobj_read_attr(gobj, "layout") || "vertical";
    if(SUPPORTED_LAYOUTS.indexOf(layout) < 0) {
        log_error(`C_YUI_NAV: unsupported layout '${layout}', falling back to vertical`);
        layout = "vertical";
    }

    let items = gobj_read_attr(gobj, "menu_items") || [];
    let zone = gobj_read_attr(gobj, "zone") || "";
    let $root;

    switch(layout) {
        case "vertical":   $root = render_vertical(gobj, items); break;
        case "icon-bar":   $root = render_icon_bar(gobj, items); break;
        case "tabs":       $root = render_tabs(gobj, items);     break;
        case "drawer":     $root = render_drawer(gobj, items);   break;
        case "submenu":    $root = render_submenu(gobj, items);  break;
        case "accordion":  $root = render_accordion(gobj, items); break;
        default:           $root = render_vertical(gobj, items);
    }
    $root.setAttribute("data-nav-zone", zone);
    $root.setAttribute("data-nav-layout", layout);
    $root.classList.add("yui-nav", `yui-nav-${layout}`);

    wire_clicks(gobj, $root);

    gobj_write_attr(gobj, "$container", $root);
}

/************************************************************
 *  Layouts
 ************************************************************/
function render_vertical(gobj, items)
{
    let show_label = gobj_read_attr(gobj, "show_label");
    let icon_pos = gobj_read_attr(gobj, "icon_pos");

    let $ul = ["ul", {class: "menu-list"}];
    for(let it of items) {
        $ul.push(item_li(it, { icon_pos, show_label, stacked: false }));
    }
    return createElement2(
        ["aside", {class: "menu p-3"},
            $ul
        ]
    );
}

function render_icon_bar(gobj, items)
{
    let show_label = gobj_read_attr(gobj, "show_label");
    let icon_pos = gobj_read_attr(gobj, "icon_pos");   /* typically "top" */

    let $bar = ["div", {class: "yui-nav-iconbar level is-mobile"}];
    for(let it of items) {
        $bar.push(item_iconbar(it, { icon_pos, show_label }));
    }
    return createElement2($bar);
}

function render_tabs(gobj, items)
{
    let show_label = gobj_read_attr(gobj, "show_label");
    let icon_pos = gobj_read_attr(gobj, "icon_pos");

    let $ul = ["ul", {}];
    for(let it of items) {
        let children = [];
        if(!empty_string(it.icon)) {
            children.push(["span", {class: "icon is-small"},
                ["i", {class: it.icon, "aria-hidden":"true"}]]);
        }
        if(show_label && !empty_string(it.name)) {
            children.push(["span", {}, it.name]);
        }
        $ul.push(
            ["li", {class: "", "data-item-id": it.id, "data-route": it.route || ""},
                ["a", {href: it.route ? "#" + it.route : "#"}, ...children]
            ]
        );
    }
    return createElement2(
        ["div", {class: "tabs is-boxed"}, $ul]
    );
}

function render_drawer(gobj, items)
{
    /*  Off-canvas drawer: initially hidden; toggled via .is-active on the
     *  outer wrapper.  The hamburger toggle is the caller's responsibility
     *  (or declared in the toolbar config).
     */
    let wrap = document.createElement("div");
    wrap.className = "yui-drawer";
    let back = document.createElement("div");
    back.className = "yui-drawer-backdrop";
    back.setAttribute("data-close-drawer", "1");
    let panel = document.createElement("div");
    panel.className = "yui-drawer-panel";
    panel.appendChild(render_vertical(gobj, items));
    wrap.appendChild(back);
    wrap.appendChild(panel);
    return wrap;
}

function render_submenu(gobj, items)
{
    let show_label = gobj_read_attr(gobj, "show_label");
    let icon_pos = gobj_read_attr(gobj, "icon_pos");

    let $ul = ["ul", {class: "menu-list"}];
    for(let it of items) {
        $ul.push(item_li(it, { icon_pos, show_label, stacked: false, compact: true }));
    }
    return createElement2(
        ["aside", {class: "menu p-2"},
            ["p", {class: "menu-label"}, "—"],
            $ul
        ]
    );
}

function render_accordion(gobj, items)
{
    /*  A flat accordion: each top item becomes a heading with a
     *  collapsible child list.  Useful when reusing this renderer for
     *  deeply nested menus — first-level items here are accordion sections.
     */
    let show_label = gobj_read_attr(gobj, "show_label");
    let icon_pos = gobj_read_attr(gobj, "icon_pos");

    let $root = createElement2(["aside", {class: "menu yui-nav-accordion p-2"}]);
    for(let it of items) {
        let $hdr = createElement2(
            ["p", {class: "menu-label yui-accordion-head",
                   "data-item-id": it.id, "data-route": it.route || ""},
                it.name || ""]
        );
        $root.appendChild($hdr);
        let $ul = createElement2(["ul", {class: "menu-list yui-accordion-body is-hidden"}]);
        if(is_array(it.submenu && it.submenu.items)) {
            for(let sub of it.submenu.items) {
                $ul.appendChild(createElement2(
                    item_li(sub, { icon_pos, show_label, stacked: false })
                ));
            }
        }
        $root.appendChild($ul);
    }
    return $root;
}

/************************************************************
 *  Shared helpers
 ************************************************************/
function item_li(it, opts)
{
    let { icon_pos, show_label, stacked } = opts;
    let children = [];

    let icon_el = !empty_string(it.icon)
        ? ["span", {class: "icon"}, ["i", {class: it.icon, "aria-hidden":"true"}]]
        : null;
    let label_el = (show_label && !empty_string(it.name))
        ? ["span", {class: "yui-nav-label"}, it.name]
        : null;

    let a_class = "yui-nav-item";
    if(stacked || icon_pos === "top" || icon_pos === "bottom") {
        a_class += " yui-nav-stacked";
    }

    if(icon_pos === "right" || icon_pos === "bottom") {
        if(label_el) children.push(label_el);
        if(icon_el)  children.push(icon_el);
    } else {
        if(icon_el)  children.push(icon_el);
        if(label_el) children.push(label_el);
    }

    return ["li", {},
        ["a", {class: a_class, href: it.route ? "#" + it.route : "#",
               "data-item-id": it.id, "data-route": it.route || "",
               "data-disabled": it.disabled ? "1" : "0"},
         ...children]
    ];
}

function item_iconbar(it, opts)
{
    let { icon_pos, show_label } = opts;
    let icon_el = !empty_string(it.icon)
        ? ["span", {class: "icon is-medium"},
           ["i", {class: it.icon, "aria-hidden":"true"}]]
        : null;
    let label_el = (show_label && !empty_string(it.name))
        ? ["span", {class: "yui-nav-label is-size-7"}, it.name]
        : null;

    let children = [];
    if(icon_pos === "bottom") {
        if(label_el) children.push(label_el);
        if(icon_el)  children.push(icon_el);
    } else {
        if(icon_el)  children.push(icon_el);
        if(label_el) children.push(label_el);
    }

    return ["div", {class: "level-item"},
        ["a", {class: "yui-nav-item yui-nav-stacked",
               href: it.route ? "#" + it.route : "#",
               "data-item-id": it.id, "data-route": it.route || ""},
         ...children]
    ];
}

function wire_clicks(gobj, $root)
{
    $root.addEventListener("click", ev => {
        let $a = ev.target.closest && ev.target.closest("[data-route]");
        if(!$a) return;
        let route = $a.getAttribute("data-route");
        if(empty_string(route)) return;
        if($a.getAttribute("data-disabled") === "1") {
            ev.preventDefault();
            return;
        }
        /*  Let hashchange do the work — but fall back to explicit call
         *  if hash routing is disabled.  */
        let shell = gobj_read_attr(gobj, "shell");
        let use_hash = shell && gobj_read_attr(shell, "use_hash");
        if(!use_hash && shell) {
            ev.preventDefault();
            /*  Dynamic import of helper to avoid circular require.  */
            import("./c_yui_shell.js").then(m => {
                m.yui_shell_navigate(shell, route);
            });
        }
        /*  Accordion heading toggle: if the anchor is an accordion head,
         *  expand/collapse its body.  */
        if($a.classList.contains("yui-accordion-head")) {
            ev.preventDefault();
            let $next = $a.nextElementSibling;
            if($next && $next.classList.contains("yui-accordion-body")) {
                $next.classList.toggle("is-hidden");
            }
        }
    });
}


                    /******************************
                     *      Actions
                     ******************************/


function ac_route_changed(gobj, event, kw, src)
{
    let route = kw.route || "";
    let parent_item = kw.parent_item;
    let item = kw.item;
    let level = gobj_read_attr(gobj, "level");

    gobj_write_attr(gobj, "active_route", route);
    let $c = gobj_read_attr(gobj, "$container");
    if(!$c) return 0;

    /*  Remove prior active class on any children */
    let previouslyActive = $c.querySelectorAll(".is-active");
    previouslyActive.forEach(n => n.classList.remove("is-active"));

    /*  Decide which id we highlight:
     *      primary nav highlights parent_item (or item if top-level)
     *      secondary nav highlights the leaf item
     */
    let id_to_mark = null;
    if(level === "primary") {
        id_to_mark = parent_item ? parent_item.id : (item && item.id);
    } else {
        id_to_mark = item && item.id;
    }
    if(!id_to_mark) return 0;

    let $a = $c.querySelector(`[data-item-id="${css_escape(id_to_mark)}"]`);
    if($a) {
        /*  For Bulma .tabs and .menu, 'is-active' goes on the <li> parent. */
        let $li = $a.closest("li");
        if($li) $li.classList.add("is-active");
        $a.classList.add("is-active");
    }
    return 0;
}

function css_escape(s)
{
    /*  Minimal escape for attribute selector */
    return String(s).replace(/"/g, '\\"');
}


                    /******************************
                     *           FSM
                     ******************************/


const gmt = {
    mt_create:  mt_create,
    mt_start:   mt_start,
    mt_stop:    mt_stop,
    mt_destroy: mt_destroy
};

function create_gclass(gclass_name)
{
    if(__gclass__) {
        log_error(`GClass ALREADY created: ${gclass_name}`);
        return -1;
    }

    const states = [
        ["ST_IDLE", [
            ["EV_ROUTE_CHANGED", ac_route_changed, null]
        ]]
    ];

    const event_types = [
        ["EV_ROUTE_CHANGED", 0]
    ];

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        gmt,
        0,
        attrs_table,
        PRIVATE_DATA,
        0, 0, 0, 0
    );
    if(!__gclass__) {
        return -1;
    }
    return 0;
}

function register_c_yui_nav()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yui_nav };
