/***********************************************************************
 *          c_yui_shell.js
 *
 *      C_YUI_SHELL — Declarative app shell.
 *
 *      Parses a JSON config to build:
 *          - Layers (z-stacked): base, overlay, popup, modal, notification, loading
 *          - Zones (inside base layer): top, top-sub, left, center, right,
 *            bottom-sub, bottom
 *          - Menus (primary + submenus) rendered via C_YUI_NAV — one nav
 *            instance per zone hosting the menu
 *          - Stages: zones declared to host routed view gobjs (typ. center)
 *
 *      Each menu item's `target` declares which gclass to instantiate (or
 *      gobj to reuse) in which stage. Navigating = show the target gobj's
 *      $container in its stage, hide the previous one. Lifecycle per item
 *      decides when the gobj is created/destroyed.
 *
 *      Hash-based 2-level routing (#/primary/secondary). No dependency on
 *      C_YUI_ROUTING.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
/* global window, document */

import {
    SDATA, SDATA_END, data_type_t, event_flag_t, gclass_flag_t,
    gclass_create, log_error, log_warning,
    gobj_create, gobj_destroy,
    gobj_start, gobj_stop,
    gobj_publish_event,
    gobj_subscribe_event,
    gobj_read_attr, gobj_read_pointer_attr, gobj_write_attr,
    createElement2, empty_string, is_object, is_array, is_string,
} from "@yuneta/gobj-js";

import {
    BULMA_BP_ORDER,
    breakpoints_from_expr,
    bulma_hidden_class,
} from "./shell_show_on.js";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_SHELL";

/*  Zones rendered inside the base layer. */
const ZONE_IDS = ["top", "top-sub", "left", "center", "right", "bottom-sub", "bottom"];

/*  Global stacking layers. */
const LAYER_DEFS = [
    ["base",         1  ],
    ["overlay",      15 ],
    ["popup",        20 ],
    ["modal",        99 ],
    ["notification", 120],
    ["loading",      150]
];

/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",     0,  null,  "Subscriber of output events"),

SDATA(data_type_t.DTP_JSON,     "config",         0,  null,  "Shell declarative config (zones, menu, stages, toolbar)"),
SDATA(data_type_t.DTP_STRING,   "default_route",  0,  "",    "Fallback route if hash is empty"),
SDATA(data_type_t.DTP_STRING,   "current_route",  0,  "",    "Current active route"),
SDATA(data_type_t.DTP_BOOLEAN,  "use_hash",       0,  true,  "Bind navigation to window.location.hash"),
SDATA(data_type_t.DTP_POINTER,  "mount_element",  0,  null,  "HTMLElement to mount shell into (default: document.body)"),

SDATA(data_type_t.DTP_POINTER,  "$container",     0,  null,  "Root HTMLElement of the shell"),
SDATA(data_type_t.DTP_POINTER,  "priv",           0,  null,  "Private runtime state (zones/layers/stages/navs)"),
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
    /*
     *  SERVICE subscription model
     */
    const subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, null, {}, subscriber);
    }

    /*  Per-instance private state (avoid the gclass-level PRIVATE_DATA). */
    gobj_write_attr(gobj, "priv", {
        zones:           {},
        layers:          {},
        stages:          {},
        navs:            [],
        item_index:      {},
        hash_handler:    null,
        keydown_handler: null
    });

    build_ui(gobj);
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    let config = gobj_read_attr(gobj, "config") || {};
    let priv = gobj_read_attr(gobj, "priv");

    build_item_index(gobj, config);
    instantiate_menus(gobj, config);
    build_toolbar(gobj, config);

    /*  lifecycle: "eager" — preinstantiate views that must exist from boot. */
    preinstantiate_eager_views(gobj);

    if(gobj_read_attr(gobj, "use_hash")) {
        priv.hash_handler = () => {
            let route = hash_to_route(window.location.hash);
            if(!empty_string(route)) {
                navigate_to(gobj, route);
            }
        };
        window.addEventListener("hashchange", priv.hash_handler);
    }

    /*  Global Escape to close any open drawer. */
    priv.keydown_handler = ev => {
        if(ev.key === "Escape" || ev.keyCode === 27) {
            close_all_drawers(gobj);
        }
    };
    window.addEventListener("keydown", priv.keydown_handler);

    let initial = hash_to_route(window.location.hash);
    if(empty_string(initial)) {
        initial = gobj_read_attr(gobj, "default_route") ||
                  (config.shell && config.shell.stages && config.shell.stages.main &&
                      config.shell.stages.main.default_route) || "";
    }
    if(!empty_string(initial)) {
        navigate_to(gobj, initial);
    } else {
        /*  No hash, no default_route, no stage default: tell the user loudly. */
        show_stage_placeholder(
            gobj, "main",
            "C_YUI_SHELL: no route to display (empty hash, no default_route, " +
            "no stages.main.default_route)"
        );
        log_error(
            "C_YUI_SHELL: no initial route — set default_route, " +
            "shell.stages.<name>.default_route, or navigate via hash"
        );
    }
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    let priv = gobj_read_attr(gobj, "priv");
    if(!priv) {
        return;
    }

    if(priv.hash_handler) {
        window.removeEventListener("hashchange", priv.hash_handler);
        priv.hash_handler = null;
    }
    if(priv.keydown_handler) {
        window.removeEventListener("keydown", priv.keydown_handler);
        priv.keydown_handler = null;
    }

    for(let nav of priv.navs) {
        try { gobj_stop(nav); gobj_destroy(nav); } catch(e) { /* ignore */ }
    }
    priv.navs = [];

    for(let name in priv.stages) {
        let st = priv.stages[name];
        for(let route in st.items) {
            let g = st.items[route];
            try { gobj_stop(g); gobj_destroy(g); } catch(e) { /* ignore */ }
        }
        st.items = {};
        st.active_route = null;
    }
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
    let $container = gobj_read_attr(gobj, "$container");
    if($container && $container.parentNode) {
        $container.parentNode.removeChild($container);
    }
    gobj_write_attr(gobj, "$container", null);
    gobj_write_attr(gobj, "priv", null);
}




                    /***************************
                     *      Local Methods
                     ***************************/




/************************************************************
 *  Build the DOM: layers → base → zones
 ************************************************************/
function build_ui(gobj)
{
    let priv = gobj_read_attr(gobj, "priv");
    let config = gobj_read_attr(gobj, "config") || {};
    let shell_cfg = config.shell || {};

    /*  Root */
    let $root = createElement2(
        ["div", {class: "yui-shell"}]
    );

    /*  Build layers */
    for(let [id, z] of LAYER_DEFS) {
        let $layer = createElement2(
            ["div", {class: `yui-layer yui-layer-${id}`, style: `z-index:${z};`}]
        );
        $root.appendChild($layer);
        priv.layers[id] = $layer;
    }

    /*  Base layer holds the grid of zones */
    let $base = priv.layers.base;
    $base.classList.add("yui-base-grid");

    let zones_cfg = shell_cfg.zones || {};

    for(let id of ZONE_IDS) {
        let $z = createElement2(
            ["div", {class: `yui-zone yui-zone-${id}`,
                     "data-zone": id,
                     style: `grid-area: ${zone_grid_area(id)};`}]
        );
        apply_show_on($z, (zones_cfg[id] && zones_cfg[id].show_on) || "");
        priv.zones[id] = $z;
        $base.appendChild($z);
    }

    /*  Register stages: zones hosting routed gobjs.
     *  Declared via zones[zone].host === "stage.<name>" or shell.stages.<name>.zone === <zone>.
     */
    let stages_cfg = shell_cfg.stages || {};
    for(let stage_name in stages_cfg) {
        let zone = stages_cfg[stage_name].zone || "center";
        priv.stages[stage_name] = {
            el: priv.zones[zone],
            items: {},
            active_route: null
        };
        if(priv.zones[zone]) {
            priv.zones[zone].classList.add("yui-stage", `yui-stage-${stage_name}`);
        }
    }
    /*  Infer main stage from center zone host if not explicitly declared. */
    for(let id in zones_cfg) {
        let host = zones_cfg[id].host || "";
        let m = /^stage\.(.+)$/.exec(host);
        if(m) {
            let name = m[1];
            if(!priv.stages[name]) {
                priv.stages[name] = { el: priv.zones[id], items: {}, active_route: null };
                if(priv.zones[id]) {
                    priv.zones[id].classList.add("yui-stage", `yui-stage-${name}`);
                }
            }
        }
    }
    if(!priv.stages.main && priv.zones.center) {
        priv.stages.main = { el: priv.zones.center, items: {}, active_route: null };
        priv.zones.center.classList.add("yui-stage", "yui-stage-main");
    }

    /*  Mount */
    let $mount = gobj_read_attr(gobj, "mount_element") || document.body;
    $mount.appendChild($root);

    gobj_write_attr(gobj, "$container", $root);
}

/************************************************************
 *  Translate zone id to grid-area name
 ************************************************************/
function zone_grid_area(zone_id)
{
    switch(zone_id) {
        case "top":        return "top";
        case "top-sub":    return "topsub";
        case "left":       return "left";
        case "center":     return "center";
        case "right":      return "right";
        case "bottom-sub": return "botsub";
        case "bottom":     return "bottom";
    }
    return "";
}

/************************************************************
 *  Translate "show_on" expression to Bulma is-hidden-* classes.
 *      ">=desktop"   → hide on touch
 *      "<desktop"    → hide on desktop+
 *      ">=tablet"    → hide on mobile
 *      "<tablet"     → hide on tablet+
 *      "mobile|tablet" → list form (OR of breakpoints)
 *      ""            → always visible
 ************************************************************/
function apply_show_on($el, expr)
{
    if(empty_string(expr)) {
        return;
    }
    let visible = breakpoints_from_expr(expr);
    for(let bp of BULMA_BP_ORDER) {
        if(!visible[bp]) {
            $el.classList.add(bulma_hidden_class(bp));
        }
    }
    $el.setAttribute("data-show-on", expr);
}

/************************************************************
 *  Precompute: route → { item, parent_item, stage, target }
 ************************************************************/
function build_item_index(gobj, config)
{
    let priv = gobj_read_attr(gobj, "priv");
    priv.item_index = {};

    let menus = (config.menu) || {};
    for(let menu_id in menus) {
        let menu = menus[menu_id];
        let items = (menu && menu.items) || [];
        for(let item of items) {
            if(item.route) {
                /*  A later menu must NOT clobber an earlier entry that
                 *  has a valid target with one that has none.  This is
                 *  the common case where a `quick` drawer just reuses
                 *  routes declared (with target) in `primary.submenu`.
                 *  Rule: prefer the first entry with a target. */
                let prev = priv.item_index[item.route];
                if(!prev || (!prev.target && item.target)) {
                    priv.item_index[item.route] = {
                        item: item,
                        parent_item: null,
                        stage: item.target && item.target.stage || null,
                        target: item.target || null,
                        menu_id: menu_id
                    };
                }
            }
            let sub = item.submenu;
            if(sub && is_array(sub.items)) {
                for(let sub_item of sub.items) {
                    if(sub_item.route) {
                        let prev = priv.item_index[sub_item.route];
                        if(!prev || (!prev.target && sub_item.target)) {
                            priv.item_index[sub_item.route] = {
                                item: sub_item,
                                parent_item: item,
                                stage: sub_item.target && sub_item.target.stage || null,
                                target: sub_item.target || null,
                                menu_id: menu_id
                            };
                        }
                    }
                }
            }
        }
    }
}

/************************************************************
 *  For each menu declared: for each zone hosting it, create
 *  a C_YUI_NAV that renders it with that zone's style.
 ************************************************************/
function instantiate_menus(gobj, config)
{
    let priv = gobj_read_attr(gobj, "priv");
    let zones_cfg = (config.shell && config.shell.zones) || {};
    let menus = config.menu || {};

    /*  Invert zones_cfg: which zones host "menu.<id>" */
    let zones_for_menu = {};
    for(let zone_id in zones_cfg) {
        let host = zones_cfg[zone_id].host || "";
        let m = /^menu\.(.+)$/.exec(host);
        if(m) {
            let menu_id = m[1];
            (zones_for_menu[menu_id] = zones_for_menu[menu_id] || []).push(zone_id);
        }
    }

    /*  Drawer menus are overlays: they don't need a host in the zone grid.
     *  Any menu with render[zone].layout === "drawer" is instantiated too;
     *  instantiate_nav_in_zone() will mount it on the overlay layer.       */
    for(let menu_id in menus) {
        let r = (menus[menu_id] && menus[menu_id].render) || {};
        for(let zone_id in r) {
            let cfg = r[zone_id];
            let layout = is_string(cfg) ? cfg : (cfg && cfg.layout);
            if(layout === "drawer") {
                let list = (zones_for_menu[menu_id] = zones_for_menu[menu_id] || []);
                if(list.indexOf(zone_id) < 0) {
                    list.push(zone_id);
                }
            }
        }
    }

    for(let menu_id in zones_for_menu) {
        let menu = menus[menu_id];
        if(!menu) {
            continue;
        }
        for(let zone_id of zones_for_menu[menu_id]) {
            instantiate_nav_in_zone(gobj, menu, menu_id, zone_id, "primary");
        }
    }

    /*  Secondary navs: create one per primary item that has a submenu,
     *  for each zone listed in submenu.render. Initially hidden, shown
     *  when the primary item becomes active.
     */
    let primary = menus.primary;
    if(primary && is_array(primary.items)) {
        for(let item of primary.items) {
            let sub = item.submenu;
            if(!sub || !is_array(sub.items)) {
                continue;
            }
            let render_by_zone = sub.render || {};
            for(let zone_id in render_by_zone) {
                if(zone_id === "*") {
                    continue;
                }
                let layout = render_by_zone[zone_id];
                if(!priv.zones[zone_id]) {
                    log_warning(`C_YUI_SHELL: submenu renders in unknown zone '${zone_id}'`);
                    continue;
                }
                let submenu_def = {
                    items: sub.items,
                    render: { [zone_id]: render_to_obj(layout) }
                };
                let sub_menu_id = `secondary.${item.id}`;
                let nav = instantiate_nav_in_zone(
                    gobj, submenu_def, sub_menu_id, zone_id, "secondary",
                    item.name || ""
                );
                /*  Hidden until primary is active. */
                let $c = gobj_read_attr(nav, "$container");
                if($c) {
                    $c.classList.add("is-hidden");
                }
            }
        }
    }
}

/*  Accept shorthand "tabs"|"vertical"|... as well as object form */
function render_to_obj(layout)
{
    if(is_string(layout)) {
        return { layout: layout };
    }
    if(is_object(layout)) {
        return layout;
    }
    return { layout: "vertical" };
}

function instantiate_nav_in_zone(gobj, menu, menu_id, zone_id, level, nav_label)
{
    let priv = gobj_read_attr(gobj, "priv");
    let render_cfg = (menu.render && (menu.render[zone_id] || menu.render["*"])) ||
                     { layout: "vertical" };
    if(is_string(render_cfg)) {
        render_cfg = { layout: render_cfg };
    }

    let nav_name = `nav_${menu_id.replace(/\./g, "_")}_${zone_id}`;
    let layout = render_cfg.layout || "vertical";
    let nav = gobj_create(
        nav_name,
        "C_YUI_NAV",
        {
            menu_id:     menu_id,
            nav_label:   nav_label || "",
            menu_items:  menu.items || [],
            zone:        zone_id,
            layout:      layout,
            icon_pos:    render_cfg.icon_pos || default_icon_pos(zone_id),
            show_label:  render_cfg.show_label !== false,
            level:       level,
            shell:       gobj
        },
        gobj
    );

    /*  Drawers are position:fixed full-screen overlays: mount them on the
     *  overlay layer, not inside the zone grid cell (which may be display:
     *  none at some breakpoints and would hide the drawer).  The zone still
     *  serves as a declarative anchor in the config. */
    let $c = gobj_read_attr(nav, "$container");
    if($c) {
        if(layout === "drawer" && priv.layers.overlay) {
            priv.layers.overlay.appendChild($c);
        } else if(priv.zones[zone_id]) {
            priv.zones[zone_id].appendChild($c);
        }
    }

    /*  The CHILD subscription model in C_YUI_NAV.mt_create already
     *  subscribes the parent (us) to EV_NAV_CLICKED, so no explicit
     *  call is needed here. */

    gobj_start(nav);
    priv.navs.push(nav);
    return nav;
}

function default_icon_pos(zone_id)
{
    if(zone_id === "bottom" || zone_id === "top") {
        return "top";
    }
    if(zone_id === "left" || zone_id === "right") {
        return "left";
    }
    if(zone_id === "top-sub" || zone_id === "bottom-sub") {
        return "left";
    }
    return "left";
}

/************************************************************
 *  Hash <-> route
 ************************************************************/
function hash_to_route(hash)
{
    if(!hash) {
        return "";
    }
    let s = String(hash).replace(/^#/, "");
    if(s.charAt(0) !== "/") {
        s = "/" + s;
    }
    return s;
}

function route_to_hash(route)
{
    if(!route) {
        return "";
    }
    let s = route.charAt(0) === "/" ? route : "/" + route;
    return "#" + s;
}

/************************************************************
 *  Navigate: make `route` active
 ************************************************************/
function navigate_to(gobj, route)
{
    let priv = gobj_read_attr(gobj, "priv");
    if(empty_string(route)) {
        log_error("C_YUI_SHELL: navigate_to called with empty route");
        return;
    }
    let entry = priv.item_index[route];

    /*  Route level 1 only: if it has a submenu, navigate to its default subitem */
    if(entry && !entry.target && entry.item && entry.item.submenu) {
        let sub = entry.item.submenu;
        let default_sub = sub.default || (sub.items && sub.items[0] && sub.items[0].route);
        if(default_sub) {
            return navigate_to(gobj, default_sub);
        }
    }

    if(!entry || !entry.target) {
        log_error(`C_YUI_SHELL: no target for route '${route}'`);
        show_stage_placeholder(
            gobj, "main",
            `C_YUI_SHELL: route '${route}' is not declared in any menu item`
        );
        return;
    }

    /*  A fresh navigation clears any placeholder shown earlier. */
    clear_stage_placeholder(gobj, entry.stage || "main");

    let stage_name = entry.stage || "main";
    let stage = priv.stages[stage_name];
    if(!stage) {
        log_error(`C_YUI_SHELL: stage '${stage_name}' not declared`);
        return;
    }

    /*  Hide previous */
    let prev_route = stage.active_route;
    if(prev_route && prev_route !== route) {
        let prev_gobj = stage.items[prev_route];
        if(prev_gobj) {
            let $c = gobj_read_attr(prev_gobj, "$container");
            if($c) {
                $c.classList.add("is-hidden");
            }
        }
        /*  lazy_destroy: drop previous on exit */
        let prev_entry = priv.item_index[prev_route];
        if(prev_entry && prev_entry.target && prev_entry.target.lifecycle === "lazy_destroy") {
            try { gobj_stop(prev_gobj); gobj_destroy(prev_gobj); } catch(e) {}
            delete stage.items[prev_route];
        }
    }

    /*  Show or create current */
    let cur = stage.items[route];
    if(!cur) {
        cur = build_view_gobj(gobj, entry, route, stage);
        if(!cur) {
            return;
        }
        stage.items[route] = cur;
    }
    let $c = gobj_read_attr(cur, "$container");
    if($c) {
        $c.classList.remove("is-hidden");
    }

    stage.active_route = route;
    gobj_write_attr(gobj, "current_route", route);

    /*  Show/hide secondary navs according to parent item */
    update_secondary_nav_visibility(gobj, entry);

    /*  Any drawer that triggered (or merely sits open during) the navigation
     *  is a transient overlay — closing it after the route change avoids it
     *  sitting on top of the new view. */
    close_all_drawers(gobj);

    /*  Update hash silently */
    if(gobj_read_attr(gobj, "use_hash")) {
        let target_hash = route_to_hash(route);
        if(window.location.hash !== target_hash) {
            /*  Using history.replaceState avoids extra hashchange fire. */
            try {
                window.history.replaceState(null, "", target_hash);
            } catch(e) {
                window.location.hash = target_hash;
            }
        }
    }

    /*  Broadcast */
    gobj_publish_event(gobj, "EV_ROUTE_CHANGED", {
        route: route,
        item: entry.item,
        parent_item: entry.parent_item,
        stage: stage_name
    });
}

function build_view_gobj(gobj, entry, route, stage)
{
    let target = entry.target;
    let kw = target.kw || {};
    let name = target.name || `view_${safe_id(route)}`;

    let view;
    try {
        view = gobj_create(name, target.gclass, kw, gobj);
    } catch(e) {
        log_error(`C_YUI_SHELL: gobj_create failed for ${target.gclass}: ${e}`);
        return null;
    }

    /*  Hard contract: every view gclass MUST expose a $container
     *  HTMLElement by the time mt_create returns.  If not, the shell
     *  cannot mount or hide it — abort cleanly.  */
    let $view = gobj_read_attr(view, "$container");
    if(!$view) {
        log_error(
            `C_YUI_SHELL: gclass '${target.gclass}' does not expose $container; ` +
            `the view is unusable — check its mt_create`
        );
        try { gobj_destroy(view); } catch(e) {}
        return null;
    }
    stage.el.appendChild($view);
    gobj_start(view);
    return view;
}

function safe_id(s)
{
    return String(s).replace(/[^a-zA-Z0-9_]/g, "_").replace(/^_+|_+$/g, "");
}

/************************************************************
 *  Toolbar — a small declarative bar mounted in whichever zone
 *  declares `host: "toolbar"`.  Items support three action types:
 *      navigate { type:"navigate", route }
 *      drawer   { type:"drawer",   op:"toggle"|"open"|"close", menu_id? }
 *      event    { type:"event",    event, kw? }
 ************************************************************/
function build_toolbar(gobj, config)
{
    let tb = config && config.toolbar;
    if(!tb || !is_array(tb.items)) {
        return;
    }

    let priv = gobj_read_attr(gobj, "priv");
    let zone_id = tb.zone || find_toolbar_zone(config);
    let $zone = priv.zones[zone_id];
    if(!$zone) {
        log_warning(`C_YUI_SHELL: toolbar target zone '${zone_id}' not found`);
        return;
    }

    /*  Toolbar labels follow the same i18n contract as nav labels:
     *  every translatable text node carries `i18n: <canonical key>`,
     *  which createElement2 maps to `data-i18n` on the rendered
     *  element.  Apps swap languages by calling
     *  refresh_language(shell.$container, t) — no DOM rebuild here. */
    let $bar = createElement2(
        ["nav", {class: "yui-toolbar navbar",
                 role: "navigation",
                 "aria-label": tb.aria_label || "Toolbar"},
            [
                ["div", {class: "navbar-brand yui-toolbar-start"}],
                ["div", {class: "navbar-end   yui-toolbar-end"}]
            ]
        ]
    );
    let $start = $bar.querySelector(".yui-toolbar-start");
    let $end   = $bar.querySelector(".yui-toolbar-end");

    for(let it of tb.items) {
        let parent = (it.align === "end") ? $end : $start;
        let children = [];
        if(!empty_string(it.icon)) {
            children.push(["span", {class: "icon"},
                ["i", {class: it.icon, "aria-hidden": "true"}]]);
        }
        if(!empty_string(it.name)) {
            children.push(["span", {i18n: it.name}, it.name]);
        }

        let aria_key = it.aria_label || it.name || it.id || "";
        let $item = createElement2(
            ["button", {class: "navbar-item yui-toolbar-item is-unselectable",
                        type: "button",
                        "data-toolbar-item-id": it.id || "",
                        "aria-label": aria_key},
             children]
        );
        $item.addEventListener("click", ev => {
            ev.preventDefault();
            handle_toolbar_action(gobj, it);
        });
        parent.appendChild($item);
    }

    $zone.appendChild($bar);
}

function find_toolbar_zone(config)
{
    let zones = (config && config.shell && config.shell.zones) || {};
    for(let z in zones) {
        if(zones[z].host === "toolbar") {
            return z;
        }
    }
    return "top";
}

function handle_toolbar_action(gobj, item)
{
    let action = (item && item.action) || {};
    switch(action.type) {
        case "navigate":
            if(!empty_string(action.route)) {
                if(gobj_read_attr(gobj, "use_hash")) {
                    window.location.hash = route_to_hash(action.route);
                } else {
                    navigate_to(gobj, action.route);
                }
            }
            break;
        case "drawer": {
            let op = action.op || "toggle";
            let menu_id = action.menu_id || null;
            if(op === "open") {
                open_drawer(gobj, menu_id);
            }
            else if(op === "close") {
                close_drawer(gobj, menu_id);
            }
            else {
                toggle_drawer(gobj, menu_id);
            }
            break;
        }
        case "event":
            /*  Publish whatever event name the JSON requested.  The shell
             *  is created with gcflag_no_check_output_events so it acts as
             *  an intermediate that forwards arbitrary user-defined events
             *  without each app having to extend our event_types table. */
            if(!empty_string(action.event)) {
                gobj_publish_event(gobj, action.event, action.kw || {});
            }
            break;
        default:
            log_warning(
                `C_YUI_SHELL: toolbar item '${item.id||"?"}' has no/unknown action.type`
            );
    }
}

/************************************************************
 *  Create all views whose item declares lifecycle:"eager".
 *  They are mounted hidden; navigate_to() will reveal them.
 ************************************************************/
function preinstantiate_eager_views(gobj)
{
    let priv = gobj_read_attr(gobj, "priv");
    for(let route in priv.item_index) {
        let entry = priv.item_index[route];
        let t = entry.target;
        if(!t || t.lifecycle !== "eager") {
            continue;
        }
        let stage_name = entry.stage || "main";
        let stage = priv.stages[stage_name];
        if(!stage) {
            log_warning(`C_YUI_SHELL: eager view ${route} has no stage '${stage_name}'`);
            continue;
        }
        if(stage.items[route]) {
            continue;          /*  already built */
        }
        let view = build_view_gobj(gobj, entry, route, stage);
        if(!view) {
            continue;
        }
        stage.items[route] = view;
        let $c = gobj_read_attr(view, "$container");
        if($c) {
            $c.classList.add("is-hidden");
        }
    }
}

/************************************************************
 *  Drawer API — toggle/open/close a nav rendered with
 *  layout:"drawer".  menu_id is optional; if omitted, acts on
 *  the first drawer found.
 ************************************************************/
function drawers(gobj, menu_id)
{
    let priv = gobj_read_attr(gobj, "priv");
    let out = [];
    for(let nav of priv.navs) {
        if(gobj_read_attr(nav, "layout") !== "drawer") {
            continue;
        }
        if(menu_id && gobj_read_attr(nav, "menu_id") !== menu_id) {
            continue;
        }
        let $c = gobj_read_attr(nav, "$container");
        if($c) {
            out.push($c);
        }
    }
    return out;
}

/************************************************************
 *  Focus-trap for an open drawer:
 *      - on activate: save document.activeElement, install a
 *        keydown trap that cycles Tab/Shift+Tab inside the
 *        panel, move focus to the first focusable child.
 *      - on release: remove the trap and restore focus.
 *  State is kept per-drawer in priv.drawer_focus_states so
 *  multiple drawers (rare but legal) work independently.
 ************************************************************/
const FOCUSABLE_SELECTOR =
    'a[href], button:not([disabled]), input:not([disabled]),' +
    ' select:not([disabled]), textarea:not([disabled]),' +
    ' [tabindex]:not([tabindex="-1"])';

function activate_focus_trap(gobj, $drawer)
{
    let priv = gobj_read_attr(gobj, "priv");
    if(!priv) {
        return;
    }
    if(!priv.drawer_focus_states) {
        priv.drawer_focus_states = new Map();
    }
    if(priv.drawer_focus_states.has($drawer)) {
        return;
    }
    let saved = document.activeElement;
    let panel = $drawer.querySelector(".yui-drawer-panel") || $drawer;

    let trap = ev => {
        if(ev.key !== "Tab") {
            return;
        }
        let nodes = panel.querySelectorAll(FOCUSABLE_SELECTOR);
        if(nodes.length === 0) {
            return;
        }
        let first = nodes[0];
        let last = nodes[nodes.length - 1];
        let inside = panel.contains(document.activeElement);
        if(ev.shiftKey) {
            if(!inside || document.activeElement === first) {
                last.focus();
                ev.preventDefault();
            }
        } else {
            if(!inside || document.activeElement === last) {
                first.focus();
                ev.preventDefault();
            }
        }
    };
    document.addEventListener("keydown", trap, true);
    priv.drawer_focus_states.set($drawer, { saved: saved, trap: trap, panel: panel });

    let first = panel.querySelector(FOCUSABLE_SELECTOR);
    if(first && typeof first.focus === "function") {
        first.focus();
    }
}

function release_focus_trap(gobj, $drawer)
{
    let priv = gobj_read_attr(gobj, "priv");
    if(!priv || !priv.drawer_focus_states) {
        return;
    }
    let st = priv.drawer_focus_states.get($drawer);
    if(!st) {
        return;
    }
    document.removeEventListener("keydown", st.trap, true);
    priv.drawer_focus_states.delete($drawer);
    if(st.saved && typeof st.saved.focus === "function" && document.body.contains(st.saved)) {
        st.saved.focus();
    }
}

function open_drawer(gobj, menu_id)
{
    for(let $c of drawers(gobj, menu_id)) {
        if($c.classList.contains("is-active")) {
            continue;
        }
        $c.classList.add("is-active");
        activate_focus_trap(gobj, $c);
    }
}

function close_drawer(gobj, menu_id)
{
    for(let $c of drawers(gobj, menu_id)) {
        if(!$c.classList.contains("is-active")) {
            continue;
        }
        $c.classList.remove("is-active");
        release_focus_trap(gobj, $c);
    }
}

function toggle_drawer(gobj, menu_id)
{
    for(let $c of drawers(gobj, menu_id)) {
        let was = $c.classList.contains("is-active");
        $c.classList.toggle("is-active");
        let now = $c.classList.contains("is-active");
        if(now && !was) {
            activate_focus_trap(gobj, $c);
        } else if(!now && was) {
            release_focus_trap(gobj, $c);
        }
    }
}

function close_all_drawers(gobj)
{
    let priv = gobj_read_attr(gobj, "priv");
    if(!priv) {
        return;
    }
    for(let nav of priv.navs) {
        if(gobj_read_attr(nav, "layout") !== "drawer") {
            continue;
        }
        let $c = gobj_read_attr(nav, "$container");
        if(!$c) {
            continue;
        }
        if(!$c.classList.contains("is-active")) {
            continue;
        }
        $c.classList.remove("is-active");
        release_focus_trap(gobj, $c);
    }
}

/************************************************************
 *  Render a visible placeholder in a stage (used when we have
 *  nothing to show, e.g. no default route configured).
 ************************************************************/
function show_stage_placeholder(gobj, stage_name, message)
{
    let priv = gobj_read_attr(gobj, "priv");
    let stage = priv.stages[stage_name];
    if(!stage || !stage.el) {
        return;
    }
    clear_stage_placeholder(gobj, stage_name);
    let $msg = createElement2(
        ["div", {class: "yui-shell-placeholder notification is-warning is-light m-4"},
            ["p", {class: "is-size-6"}, message]
        ]
    );
    stage.el.appendChild($msg);
}

function clear_stage_placeholder(gobj, stage_name)
{
    let priv = gobj_read_attr(gobj, "priv");
    let stage = priv.stages[stage_name];
    if(!stage || !stage.el) {
        return;
    }
    let $old = stage.el.querySelector(":scope > .yui-shell-placeholder");
    if($old) {
        $old.parentNode.removeChild($old);
    }
}

function update_secondary_nav_visibility(gobj, entry)
{
    let priv = gobj_read_attr(gobj, "priv");
    let active_primary_id = entry.parent_item
        ? entry.parent_item.id
        : entry.item.id;

    for(let nav of priv.navs) {
        let level = gobj_read_attr(nav, "level");
        if(level !== "secondary") {
            continue;
        }
        let menu_id = gobj_read_attr(nav, "menu_id") || "";
        let m = /^secondary\.(.+)$/.exec(menu_id);
        if(!m) {
            continue;
        }
        let $c = gobj_read_attr(nav, "$container");
        if(!$c) {
            continue;
        }
        if(m[1] === active_primary_id) {
            $c.classList.remove("is-hidden");
        } else {
            $c.classList.add("is-hidden");
        }
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/************************************************************
 *  Click-through from a C_YUI_NAV child: we own routing here.
 ************************************************************/
function ac_nav_clicked(gobj, event, kw, src)
{
    let route = (kw && kw.route) || "";
    if(empty_string(route)) {
        return 0;
    }

    /*  When hash routing is on, let the hash drive navigate_to() — that
     *  way back/forward buttons and programmatic hash changes all flow
     *  through the same code path.  Otherwise call navigate_to directly. */
    if(gobj_read_attr(gobj, "use_hash")) {
        let target_hash = route_to_hash(route);
        if(window.location.hash !== target_hash) {
            window.location.hash = target_hash;  /*  fires hashchange */
        } else {
            /*  Same hash: hashchange won't fire — navigate explicitly. */
            navigate_to(gobj, route);
        }
    } else {
        navigate_to(gobj, route);
    }
    return 0;
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
            /*  Navigation requests flow through EV_NAV_CLICKED (emitted
             *  by child C_YUI_NAVs) and through the window.hashchange
             *  listener (see mt_start).  The shell is the sole router.  */
            ["EV_NAV_CLICKED", ac_nav_clicked, null]
        ]]
    ];

    const event_types = [
        ["EV_NAV_CLICKED",   0],
        ["EV_ROUTE_CHANGED", event_flag_t.EVF_OUTPUT_EVENT
                            |event_flag_t.EVF_PUBLIC_EVENT
                            |event_flag_t.EVF_NO_WARN_SUBS]
    ];

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        gmt,
        0,                          /* lmt */
        attrs_table,
        PRIVATE_DATA,
        0,                          /* authz_table */
        0,                          /* command_table */
        0,                          /* s_user_trace_level */
        gclass_flag_t.gcflag_no_check_output_events
    );
    if(!__gclass__) {
        return -1;
    }
    return 0;
}

function register_c_yui_shell()
{
    return create_gclass(GCLASS_NAME);
}

/***************************************************************
 *  Public helpers — exported alongside register_c_yui_shell().
 *  Not gclass methods, no banner needed; left grouped at the
 *  bottom of the file to keep the skeleton layout intact.
 ***************************************************************/

/************************************************************
 *  Programmatic navigation (bypass hash).
 ************************************************************/
function yui_shell_navigate(shell_gobj, route)
{
    navigate_to(shell_gobj, route);
}

/*  Drawer helpers — toggle the off-canvas nav from the outside
 *  (e.g. a hamburger button in the toolbar).  menu_id is optional. */
function yui_shell_open_drawer(shell_gobj, menu_id)    { open_drawer(shell_gobj, menu_id);   }
function yui_shell_close_drawer(shell_gobj, menu_id)   { close_drawer(shell_gobj, menu_id);  }
function yui_shell_toggle_drawer(shell_gobj, menu_id)  { toggle_drawer(shell_gobj, menu_id); }

/*  Note: there is no shell-level language switch helper.  Every
 *  translatable text node rendered by the shell and its navs is
 *  tagged with `data-i18n` (the canonical English key).  Apps swap
 *  language by calling
 *      refresh_language(shell_$container, t)
 *  from `@yuneta/gobj-js`, exactly like `c_yui_main.js` does in
 *  `change_language()`.  The shell does not own that flow. */

export {
    register_c_yui_shell,
    yui_shell_navigate,
    yui_shell_open_drawer,
    yui_shell_close_drawer,
    yui_shell_toggle_drawer
};
