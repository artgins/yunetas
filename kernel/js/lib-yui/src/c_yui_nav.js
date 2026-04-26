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
    SDATA, SDATA_END, data_type_t, event_flag_t,
    gclass_create, log_error, log_warning,
    gobj_subscribe_event,
    gobj_parent,
    gobj_read_attr, gobj_read_pointer_attr, gobj_write_attr,
    gobj_publish_event,
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
SDATA(data_type_t.DTP_STRING,   "nav_label",     0,  "",           "Human-readable label for aria/heading; falls back to menu_id"),
SDATA(data_type_t.DTP_JSON,     "menu_items",    0,  null,         "Array of menu items to render"),
SDATA(data_type_t.DTP_STRING,   "zone",          0,  "",           "Zone id where this nav lives"),
SDATA(data_type_t.DTP_STRING,   "layout",        0,  "vertical",   "vertical|icon-bar|tabs|drawer|submenu|accordion"),
SDATA(data_type_t.DTP_STRING,   "icon_pos",      0,  "left",       "left|right|top|bottom"),
SDATA(data_type_t.DTP_BOOLEAN,  "show_label",    0,  true,         "Show item label next to/under the icon"),
SDATA(data_type_t.DTP_STRING,   "level",         0,  "primary",    "primary|secondary"),

SDATA(data_type_t.DTP_POINTER,  "shell",         0,  null,         "C_YUI_SHELL gobj (for navigation calls)"),
SDATA(data_type_t.DTP_POINTER,  "$container",    0,  null,         "Root HTMLElement of this nav"),
SDATA(data_type_t.DTP_STRING,   "active_route",  0,  "",           "Currently active route"),
SDATA(data_type_t.DTP_POINTER,  "priv",          0,  null,         "Private runtime state (click listener, etc.)"),
SDATA_END()
];

/*  Monotonic id generator for ARIA pairs (aria-controls etc.). */
let __nav_aria_seq__ = 0;

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
    /*
     *  CHILD subscription model
     */
    let subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, null, {}, subscriber);

    gobj_write_attr(gobj, "priv", {
        click_handler: null
    });
    build_ui(gobj);
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    let shell = gobj_read_attr(gobj, "shell");
    if(shell) {
        /*  Listen to route changes from the shell so we can highlight
         *  the active item. */
        try {
            gobj_subscribe_event(shell, "EV_ROUTE_CHANGED", {}, gobj);
        } catch(e) {
            log_warning(`C_YUI_NAV: subscribe to EV_ROUTE_CHANGED failed: ${e}`);
        }
    }
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
    let $c = gobj_read_attr(gobj, "$container");
    let priv = gobj_read_attr(gobj, "priv");
    if($c && priv && priv.click_handler) {
        $c.removeEventListener("click", priv.click_handler);
    }
    if($c && $c.parentNode) {
        $c.parentNode.removeChild($c);
    }
    gobj_write_attr(gobj, "$container", null);
    gobj_write_attr(gobj, "priv", null);
}




                    /***************************
                     *      Local Methods
                     ***************************/




/************************************************************
 *  Build the UI: dispatch to one of the six layout renderers
 ************************************************************/
function build_ui(gobj)
{
    let layout = gobj_read_attr(gobj, "layout") || "vertical";
    if(SUPPORTED_LAYOUTS.indexOf(layout) < 0) {
        log_error(`C_YUI_NAV: unsupported layout '${layout}', falling back to vertical`);
        layout = "vertical";
    }

    let items = gobj_read_attr(gobj, "menu_items") || [];
    let zone = gobj_read_attr(gobj, "zone") || "";
    let menu_id = gobj_read_attr(gobj, "menu_id") || "nav";
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

    /*  Accessibility: every nav root is a landmark.  For the drawer we
     *  tag the wrapper as role=dialog and the panel as role=navigation. */
    if(layout !== "drawer") {
        $root.setAttribute("role", "navigation");
        if(!$root.hasAttribute("aria-label")) {
            let label = gobj_read_attr(gobj, "nav_label") || menu_id;
            $root.setAttribute("aria-label", label);
        }
    }

    wire_clicks(gobj, $root);

    gobj_write_attr(gobj, "$container", $root);
}

/************************************************************
 *  Layouts
 *
 *  IMPORTANT 1: createElement2 destructures node descriptors as
 *  [tag, attrs, content, events] positionally, so multiple
 *  children MUST be wrapped in a single array (otherwise the
 *  third sibling would be interpreted as an event-listener map
 *  and addEventListener would crash).  In short:
 *
 *      OK:    ["ul", {}, [li1, li2, li3]]
 *      WRONG: ["ul", {}, li1, li2, li3]
 *
 *  IMPORTANT 2: every translatable text node carries an `i18n`
 *  attribute with its canonical (English) key.  createElement2
 *  maps it to `data-i18n` on the rendered element.  The host app
 *  changes language by calling `refresh_language(element, t)`
 *  from `@yuneta/gobj-js`, which walks all `[data-i18n]` and
 *  re-translates the first text node with `t(key)`.  See the
 *  legacy `change_language()` in `c_yui_main.js` for the
 *  canonical pattern.
 ************************************************************/
function render_vertical(gobj, items)
{
    let show_label = gobj_read_attr(gobj, "show_label");
    let icon_pos = gobj_read_attr(gobj, "icon_pos");

    let lis = [];
    for(let it of items) {
        lis.push(item_li(gobj, it, { icon_pos, show_label, stacked: false }));
    }
    return createElement2(
        ["aside", {class: "menu p-3"},
            ["ul", {class: "menu-list"}, lis]
        ]
    );
}

function render_icon_bar(gobj, items)
{
    let show_label = gobj_read_attr(gobj, "show_label");
    let icon_pos = gobj_read_attr(gobj, "icon_pos");   /* typically "top" */

    let bar_items = [];
    for(let it of items) {
        bar_items.push(item_iconbar(gobj, it, { icon_pos, show_label }));
    }
    return createElement2(
        ["div", {class: "yui-nav-iconbar level is-mobile"}, bar_items]
    );
}

function render_tabs(gobj, items)
{
    let show_label = gobj_read_attr(gobj, "show_label");

    let lis = [];
    for(let it of items) {
        let children = [];
        if(!empty_string(it.icon)) {
            children.push(["span", {class: "icon is-small"},
                ["i", {class: it.icon, "aria-hidden":"true"}]]);
        }
        if(show_label && !empty_string(it.name)) {
            children.push(["span", {i18n: it.name}, it.name]);
        }
        lis.push(
            ["li", {class: "", "data-item-id": it.id, "data-route": it.route || ""},
                ["a", {href: it.route ? "#" + it.route : "#",
                       "data-item-id": it.id,
                       "data-route":   it.route || ""},
                 children]
            ]
        );
    }
    return createElement2(
        ["div", {class: "tabs is-boxed"},
            ["ul", {}, lis]
        ]
    );
}

function render_drawer(gobj, items)
{
    /*  Off-canvas drawer: initially hidden; toggled via .is-active on the
     *  outer wrapper.  Open/close is driven from the toolbar or from the
     *  public yui_shell_{open,close,toggle}_drawer() helpers.
     */
    let menu_id = gobj_read_attr(gobj, "menu_id") || "drawer";
    let wrap = document.createElement("div");
    wrap.className = "yui-drawer";
    wrap.setAttribute("role", "dialog");
    wrap.setAttribute("aria-modal", "true");
    wrap.setAttribute("aria-label", menu_id);

    let back = document.createElement("div");
    back.className = "yui-drawer-backdrop";
    back.setAttribute("data-close-drawer", "1");
    back.setAttribute("aria-hidden", "true");

    let panel = document.createElement("div");
    panel.className = "yui-drawer-panel";
    panel.setAttribute("role", "navigation");
    panel.setAttribute("aria-label", menu_id);
    panel.appendChild(render_vertical(gobj, items));

    wrap.appendChild(back);
    wrap.appendChild(panel);
    return wrap;
}

function render_submenu(gobj, items)
{
    let show_label = gobj_read_attr(gobj, "show_label");
    let icon_pos = gobj_read_attr(gobj, "icon_pos");
    let menu_id = gobj_read_attr(gobj, "menu_id") || "";
    let nav_label = gobj_read_attr(gobj, "nav_label") || menu_id;

    let lis = [];
    for(let it of items) {
        lis.push(item_li(gobj, it, { icon_pos, show_label, stacked: false, compact: true }));
    }
    let heading_text = nav_label || "—";
    let heading_attrs = nav_label
        ? { class: "menu-label", i18n: nav_label }
        : { class: "menu-label" };
    return createElement2(
        ["aside", {class: "menu p-2"},
            [
                ["p", heading_attrs, heading_text],
                ["ul", {class: "menu-list"}, lis]
            ]
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
        let acc_id = ++__nav_aria_seq__;
        let head_id = `yui-acc-head-${acc_id}`;
        let body_id = `yui-acc-body-${acc_id}`;

        let head_text = it.name || "";
        let head_attrs = {
            type: "button",
            id: head_id,
            class: "menu-label yui-accordion-head",
            "data-item-id": it.id,
            "data-route":   it.route || "",
            "aria-expanded": "false",
            "aria-controls": body_id
        };
        if(!empty_string(head_text)) {
            head_attrs.i18n = head_text;
        }
        let $hdr = createElement2(["button", head_attrs, head_text]);
        $root.appendChild($hdr);

        let $ul = createElement2(
            ["ul", {id: body_id,
                    class: "menu-list yui-accordion-body is-hidden",
                    role: "region",
                    "aria-labelledby": head_id}]
        );
        if(is_array(it.submenu && it.submenu.items)) {
            for(let sub of it.submenu.items) {
                $ul.appendChild(createElement2(
                    item_li(gobj, sub, { icon_pos, show_label, stacked: false })
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
function item_li(gobj, it, opts)
{
    let { icon_pos, show_label, stacked } = opts;
    let children = [];
    let label = it.name || "";

    let icon_el = !empty_string(it.icon)
        ? ["span", {class: "icon"}, ["i", {class: it.icon, "aria-hidden":"true"}]]
        : null;
    let label_el = (show_label && !empty_string(label))
        ? ["span", {class: "yui-nav-label", i18n: label}, label]
        : null;

    let a_class = "yui-nav-item";
    if(stacked || icon_pos === "top" || icon_pos === "bottom") {
        a_class += " yui-nav-stacked";
    }

    if(icon_pos === "right" || icon_pos === "bottom") {
        if(label_el) {
            children.push(label_el);
        }
        if(icon_el) {
            children.push(icon_el);
        }
    } else {
        if(icon_el) {
            children.push(icon_el);
        }
        if(label_el) {
            children.push(label_el);
        }
    }

    let a_attrs = {
        class: a_class,
        href: it.route ? "#" + it.route : "#",
        "data-item-id": it.id,
        "data-route":   it.route || "",
        "data-disabled": it.disabled ? "1" : "0",
        "aria-label":   label || it.id
    };
    if(it.disabled) {
        a_attrs["aria-disabled"] = "true";
        a_attrs["tabindex"] = "-1";
    }

    return ["li", {},
        ["a", a_attrs, children]
    ];
}

function item_iconbar(gobj, it, opts)
{
    let { icon_pos, show_label } = opts;
    let label = it.name || "";
    let icon_el = !empty_string(it.icon)
        ? ["span", {class: "icon is-medium"},
           ["i", {class: it.icon, "aria-hidden":"true"}]]
        : null;
    let label_el = (show_label && !empty_string(label))
        ? ["span", {class: "yui-nav-label is-size-7", i18n: label}, label]
        : null;

    let children = [];
    if(icon_pos === "bottom") {
        if(label_el) {
            children.push(label_el);
        }
        if(icon_el) {
            children.push(icon_el);
        }
    } else {
        if(icon_el) {
            children.push(icon_el);
        }
        if(label_el) {
            children.push(label_el);
        }
    }

    return ["div", {class: "level-item"},
        ["a", {class: "yui-nav-item yui-nav-stacked",
               href: it.route ? "#" + it.route : "#",
               "data-item-id": it.id,
               "data-route":   it.route || "",
               "aria-label":   label || it.id},
         children]
    ];
}

function wire_clicks(gobj, $root)
{
    let priv = gobj_read_attr(gobj, "priv") || {};

    let handler = ev => {
        let target = ev.target;
        if(!target || !target.closest) {
            return;
        }

        /*  Drawer backdrop close. */
        let $bk = target.closest("[data-close-drawer]");
        if($bk) {
            let $drawer = $bk.closest(".yui-drawer");
            if($drawer) {
                $drawer.classList.remove("is-active");
            }
            ev.preventDefault();
            return;
        }

        /*  Accordion head toggle takes precedence over navigation: the
         *  head is a container for its sub-items, not a destination. */
        let $head = target.closest(".yui-accordion-head");
        if($head && $root.contains($head)) {
            let $next = $head.nextElementSibling;
            if($next && $next.classList.contains("yui-accordion-body")) {
                let open = $next.classList.contains("is-hidden");
                $next.classList.toggle("is-hidden", !open);
                $head.classList.toggle("is-open", open);
                $head.setAttribute("aria-expanded", open ? "true" : "false");
            }
            ev.preventDefault();
            return;
        }

        /*  Normal navigation intent.
         *  The nav never navigates on its own: it emits EV_NAV_CLICKED
         *  and lets the shell decide how to route (via hash or direct
         *  call).  This breaks the circular import on yui_shell_navigate
         *  and keeps ownership of routing in one place. */
        let $a = target.closest("[data-route]");
        if(!$a) {
            return;
        }
        let route = $a.getAttribute("data-route");
        if(empty_string(route)) {
            return;
        }
        if($a.getAttribute("data-disabled") === "1") {
            ev.preventDefault();
            return;
        }
        ev.preventDefault();
        gobj_publish_event(gobj, "EV_NAV_CLICKED", {
            route:    route,
            item_id:  $a.getAttribute("data-item-id") || "",
            zone:     gobj_read_attr(gobj, "zone")  || "",
            level:    gobj_read_attr(gobj, "level") || "primary"
        });
    };

    $root.addEventListener("click", handler);
    priv.click_handler = handler;
}




                    /***************************
                     *      Actions
                     ***************************/




/************************************************************
 *  EV_ROUTE_CHANGED from the shell — refresh active highlight
 ************************************************************/
function ac_route_changed(gobj, event, kw, src)
{
    let route = kw.route || "";
    let parent_item = kw.parent_item;
    let item = kw.item;
    let level = gobj_read_attr(gobj, "level");

    gobj_write_attr(gobj, "active_route", route);
    let $c = gobj_read_attr(gobj, "$container");
    if(!$c) {
        return 0;
    }

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
    if(!id_to_mark) {
        return 0;
    }

    let $a = $c.querySelector(`[data-item-id="${css_escape(id_to_mark)}"]`);
    if($a) {
        /*  For Bulma .tabs and .menu, 'is-active' goes on the <li> parent. */
        let $li = $a.closest("li");
        if($li) {
            $li.classList.add("is-active");
        }
        $a.classList.add("is-active");
    }

    /*  Accordion: ensure only the section containing the active leaf
     *  is expanded.  Any other open section collapses. */
    if(gobj_read_attr(gobj, "layout") === "accordion") {
        let active_leaf_id = (item && item.id) || id_to_mark;
        let heads = $c.querySelectorAll(".yui-accordion-head");
        heads.forEach($hd => {
            let $body = $hd.nextElementSibling;
            if(!$body || !$body.classList.contains("yui-accordion-body")) {
                return;
            }
            let contains_active = !!$body.querySelector(
                `[data-item-id="${css_escape(active_leaf_id)}"]`
            );
            $body.classList.toggle("is-hidden", !contains_active);
            $hd.classList.toggle("is-open", contains_active);
            $hd.setAttribute("aria-expanded", contains_active ? "true" : "false");
        });
    }
    return 0;
}

function css_escape(s)
{
    /*  Minimal escape for attribute selector */
    return String(s).replace(/"/g, '\\"');
}

/***************************************************************
 *              FSM
 ***************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
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
        ["EV_ROUTE_CHANGED", 0],
        ["EV_NAV_CLICKED",   event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_PUBLIC_EVENT]
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
