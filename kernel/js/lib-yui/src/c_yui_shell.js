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
    SDATA, SDATA_END, data_type_t, event_flag_t,
    gclass_create, log_error, log_warning,
    gobj_create, gobj_destroy,
    gobj_start, gobj_stop,
    gobj_publish_event,
    gobj_subscribe_event,
    gobj_read_attr, gobj_write_attr,
    gobj_parent, gobj_name,
    createElement2, empty_string, is_object, is_array, is_string,
} from "@yuneta/gobj-js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_SHELL";

/*  Bulma breakpoint order (low → high).  Used by show_on parser. */
const BULMA_BP_ORDER = ["mobile", "tablet", "desktop", "widescreen", "fullhd"];

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


function mt_create(gobj)
{
    let subscriber = gobj_read_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    if(subscriber) {
        gobj_subscribe_event(gobj, null, {}, subscriber);
    }

    /*  Per-instance private state (avoid the gclass-level PRIVATE_DATA). */
    gobj_write_attr(gobj, "priv", {
        zones:        {},
        layers:       {},
        stages:       {},
        navs:         [],
        item_index:   {},
        hash_handler: null
    });

    build_ui(gobj);
}

function mt_start(gobj)
{
    let config = gobj_read_attr(gobj, "config") || {};

    build_item_index(gobj, config);
    instantiate_menus(gobj, config);

    if(gobj_read_attr(gobj, "use_hash")) {
        let priv = gobj_read_attr(gobj, "priv");
        priv.hash_handler = () => {
            let route = hash_to_route(window.location.hash);
            if(!empty_string(route)) {
                navigate_to(gobj, route);
            }
        };
        window.addEventListener("hashchange", priv.hash_handler);
    }

    let initial = hash_to_route(window.location.hash);
    if(empty_string(initial)) {
        initial = gobj_read_attr(gobj, "default_route") ||
                  (config.shell && config.shell.stages && config.shell.stages.main &&
                      config.shell.stages.main.default_route) || "";
    }
    if(!empty_string(initial)) {
        navigate_to(gobj, initial);
    }
}

function mt_stop(gobj)
{
    let priv = gobj_read_attr(gobj, "priv");
    if(!priv) { return; }

    if(priv.hash_handler) {
        window.removeEventListener("hashchange", priv.hash_handler);
        priv.hash_handler = null;
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

function mt_destroy(gobj)
{
    let $container = gobj_read_attr(gobj, "$container");
    if($container && $container.parentNode) {
        $container.parentNode.removeChild($container);
    }
    gobj_write_attr(gobj, "$container", null);
    gobj_write_attr(gobj, "priv", null);
}


                    /******************************
                     *      Local Methods
                     ******************************/


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

function breakpoints_from_expr(expr)
{
    let out = { mobile:false, tablet:false, desktop:false, widescreen:false, fullhd:false };
    expr = String(expr).trim();
    if(expr === "*" || expr === "") {
        for(let bp of BULMA_BP_ORDER) out[bp] = true;
        return out;
    }
    let parts = expr.split("|").map(s => s.trim()).filter(s => s.length);
    for(let p of parts) {
        let m;
        if((m = /^>=(\w+)$/.exec(p))) {
            let idx = BULMA_BP_ORDER.indexOf(m[1]);
            if(idx >= 0) for(let i=idx;i<BULMA_BP_ORDER.length;i++) out[BULMA_BP_ORDER[i]] = true;
        } else if((m = /^<(\w+)$/.exec(p))) {
            let idx = BULMA_BP_ORDER.indexOf(m[1]);
            if(idx > 0) for(let i=0;i<idx;i++) out[BULMA_BP_ORDER[i]] = true;
        } else if((m = /^<=(\w+)$/.exec(p))) {
            let idx = BULMA_BP_ORDER.indexOf(m[1]);
            if(idx >= 0) for(let i=0;i<=idx;i++) out[BULMA_BP_ORDER[i]] = true;
        } else if((m = /^>(\w+)$/.exec(p))) {
            let idx = BULMA_BP_ORDER.indexOf(m[1]);
            if(idx >= 0) for(let i=idx+1;i<BULMA_BP_ORDER.length;i++) out[BULMA_BP_ORDER[i]] = true;
        } else if(out.hasOwnProperty(p)) {
            out[p] = true;
        }
    }
    return out;
}

/*  Map each Bulma breakpoint to the helper class that hides it. */
function bulma_hidden_class(bp)
{
    switch(bp) {
        case "mobile":     return "yui-hidden-mobile";       /*  <769  */
        case "tablet":     return "yui-hidden-tablet-only";  /*  769-1023 */
        case "desktop":    return "yui-hidden-desktop-only"; /*  1024-1215 */
        case "widescreen": return "yui-hidden-widescreen-only"; /*  1216-1407 */
        case "fullhd":     return "yui-hidden-fullhd";       /*  ≥1408 */
    }
    return "";
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
                priv.item_index[item.route] = {
                    item: item,
                    parent_item: null,
                    stage: item.target && item.target.stage || null,
                    target: item.target || null,
                    menu_id: menu_id
                };
            }
            let sub = item.submenu;
            if(sub && is_array(sub.items)) {
                for(let sub_item of sub.items) {
                    if(sub_item.route) {
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

    for(let menu_id in zones_for_menu) {
        let menu = menus[menu_id];
        if(!menu) continue;
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
            if(!sub || !is_array(sub.items)) continue;
            let render_by_zone = sub.render || {};
            for(let zone_id in render_by_zone) {
                if(zone_id === "*") continue;
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
                    gobj, submenu_def, sub_menu_id, zone_id, "secondary"
                );
                /*  Hidden until primary is active. */
                let $c = gobj_read_attr(nav, "$container");
                if($c) { $c.classList.add("is-hidden"); }
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

function instantiate_nav_in_zone(gobj, menu, menu_id, zone_id, level)
{
    let priv = gobj_read_attr(gobj, "priv");
    let render_cfg = (menu.render && (menu.render[zone_id] || menu.render["*"])) ||
                     { layout: "vertical" };
    if(is_string(render_cfg)) {
        render_cfg = { layout: render_cfg };
    }

    let nav_name = `nav_${menu_id.replace(/\./g, "_")}_${zone_id}`;
    let nav = gobj_create(
        nav_name,
        "C_YUI_NAV",
        {
            menu_id:     menu_id,
            menu_items:  menu.items || [],
            zone:        zone_id,
            layout:      render_cfg.layout || "vertical",
            icon_pos:    render_cfg.icon_pos || default_icon_pos(zone_id),
            show_label:  render_cfg.show_label !== false,
            level:       level,
            shell:       gobj
        },
        gobj
    );

    let $c = gobj_read_attr(nav, "$container");
    if($c && priv.zones[zone_id]) {
        priv.zones[zone_id].appendChild($c);
    }

    gobj_start(nav);
    priv.navs.push(nav);
    return nav;
}

function default_icon_pos(zone_id)
{
    if(zone_id === "bottom" || zone_id === "top")       return "top";
    if(zone_id === "left" || zone_id === "right")       return "left";
    if(zone_id === "top-sub" || zone_id === "bottom-sub") return "left";
    return "left";
}


/************************************************************
 *  Hash <-> route
 ************************************************************/
function hash_to_route(hash)
{
    if(!hash) return "";
    let s = String(hash).replace(/^#/, "");
    if(s.charAt(0) !== "/") s = "/" + s;
    return s;
}

function route_to_hash(route)
{
    if(!route) return "";
    let s = route.charAt(0) === "/" ? route : "/" + route;
    return "#" + s;
}


/************************************************************
 *  Navigate: make `route` active
 ************************************************************/
function navigate_to(gobj, route)
{
    let priv = gobj_read_attr(gobj, "priv");
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
        log_warning(`C_YUI_SHELL: no target for route '${route}'`);
        return;
    }

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
            if($c) $c.classList.add("is-hidden");
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
        if(!cur) return;
        stage.items[route] = cur;
    }
    let $c = gobj_read_attr(cur, "$container");
    if($c) $c.classList.remove("is-hidden");

    stage.active_route = route;
    gobj_write_attr(gobj, "current_route", route);

    /*  Show/hide secondary navs according to parent item */
    update_secondary_nav_visibility(gobj, entry);

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

    let $view = gobj_read_attr(view, "$container");
    if(!$view) {
        log_warning(`C_YUI_SHELL: gclass '${target.gclass}' did not expose $container`);
    } else {
        stage.el.appendChild($view);
    }
    gobj_start(view);
    return view;
}

function safe_id(s)
{
    return String(s).replace(/[^a-zA-Z0-9_]/g, "_").replace(/^_+|_+$/g, "");
}

function update_secondary_nav_visibility(gobj, entry)
{
    let priv = gobj_read_attr(gobj, "priv");
    let active_primary_id = entry.parent_item
        ? entry.parent_item.id
        : entry.item.id;

    for(let nav of priv.navs) {
        let level = gobj_read_attr(nav, "level");
        if(level !== "secondary") continue;
        let menu_id = gobj_read_attr(nav, "menu_id") || "";
        let m = /^secondary\.(.+)$/.exec(menu_id);
        if(!m) continue;
        let $c = gobj_read_attr(nav, "$container");
        if(!$c) continue;
        if(m[1] === active_primary_id) {
            $c.classList.remove("is-hidden");
        } else {
            $c.classList.add("is-hidden");
        }
    }
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
            /*  All navigation goes through navigate_to(); the NAV children
             *  call us directly via the `shell` attr, no events required.
             *  The only outgoing event is EV_ROUTE_CHANGED (published).
             */
        ]]
    ];

    const event_types = [
        ["EV_ROUTE_CHANGED", event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_PUBLIC_EVENT]
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

function register_c_yui_shell()
{
    return create_gclass(GCLASS_NAME);
}


                    /******************************
                     *      Public helpers
                     ******************************/


/*  Programmatic navigation (bypass hash).  */
function yui_shell_navigate(shell_gobj, route)
{
    navigate_to(shell_gobj, route);
}


export { register_c_yui_shell, yui_shell_navigate };
