/***********************************************************************
 *          c_account_view.js
 *
 *      C_ACCOUNT_VIEW — the three account-menu pages, one gclass
 *      parameterised by the `view` attr:
 *
 *        - "preference" : appearance (theme + language) selectors that
 *                         apply immediately and persist in this browser.
 *        - "developer"  : read-only diagnostics (deployment identity,
 *                         session, control-center link, active node).
 *        - "about"      : product card (mark, version, tenant/plane,
 *                         documentation link, copyright).
 *
 *      Like every shell view it exposes a $container (required by
 *      C_YUI_SHELL). All user-visible strings go through i18next so the
 *      language toggle re-translates them.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA, SDATA_END, data_type_t,
    gclass_create, log_error,
    gobj_parent,
    gobj_read_attr, gobj_read_pointer_attr, gobj_write_attr,
    gobj_subscribe_event, gobj_find_service,
    createElement2,
    refresh_language,
} from "@yuneta/gobj-js";

import i18next from "i18next";

import pkg from "../package.json";

import {deploy_info} from "./conf/deploy.js";
import {current_theme} from "./theme.js";
import {current_locale, switch_locale} from "./locales/locales.js";
import {app_set_theme} from "./c_app.js";
import {agent_login_username, agent_login_is_logged_in} from "./c_agent_login.js";
import {agent_link_is_connected} from "./c_agent_link.js";
import {agent_config_get_active_node} from "./c_agent_config.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_ACCOUNT_VIEW";

const ACCENT = "#2E7CD6";


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",  0,  null,          "Subscriber of output events"),
SDATA(data_type_t.DTP_STRING,   "view",        0,  "about",       "preference | developer | about"),
SDATA(data_type_t.DTP_POINTER,  "$container",  0,  null,          "Root HTMLElement"),
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
     *  CHILD subscription model
     */
    let subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, null, {}, subscriber);

    let $c = createElement2(["div", {class: "view-card account-view"}, []]);
    gobj_write_attr(gobj, "$container", $c);
    render(gobj);
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
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
    if($c && $c.parentNode) {
        $c.parentNode.removeChild($c);
    }
    gobj_write_attr(gobj, "$container", null);
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************
 *  createElement2 bound to the i18next translator, so every node
 *  carrying an `i18n` attr is translated at build time AND can be
 *  re-translated in place by refresh_language() on a language switch.
 ***************************************************************/
function ce(desc)
{
    return createElement2(desc, i18next.t.bind(i18next));
}

/***************************************************************
 *  (Re)render the page into our $container according to `view`.
 ***************************************************************/
function render(gobj)
{
    let $c = gobj_read_attr(gobj, "$container");
    if(!$c) {
        return;
    }
    $c.replaceChildren();

    let view = gobj_read_attr(gobj, "view");
    let body;
    if(view === "preference") {
        body = build_preference(gobj);
    } else {
        body = build_about(gobj);
    }
    $c.appendChild(body);
}

/***************************************************************
 *  Page header (title + optional subtitle). *_text is the English
 *  fallback; *_key the i18n key that overrides it per language.
 ***************************************************************/
function page_header(title_key, title_text, sub_key, sub_text)
{
    let children = [
        ["h1", {class: "title is-3", style: `color:${ACCENT}; margin-bottom:0.25rem;`, i18n: title_key},
            title_text]
    ];
    if(sub_key) {
        children.push(
            ["p", {class: "subtitle is-6", style: "color:#5B6B7E; margin-bottom:1rem;", i18n: sub_key},
                sub_text]
        );
    }
    return ["div", {style: "margin-bottom:0.5rem;"}, children];
}

/***************************************************************
 *  Preference — appearance (theme + language).
 ***************************************************************/
function build_preference(gobj)
{
    let theme = current_theme();
    let lang  = current_locale();

    /*  A segmented button group; the active option carries is-primary.
     *  Click handlers go in the 4th (events) slot of the createElement2
     *  description — an `onclick` attr would be silently dropped. */
    function segment(options, current, on_pick)
    {
        let btns = options.map(function(opt) {
            let active = (opt.value === current);
            let cls = "button" + (active ? " is-primary is-selected" : "");
            let kids = [];
            if(opt.icon) {
                kids.push(["span", {class: "icon"}, [["span", {class: opt.icon}, ""]]]);
            }
            kids.push(opt.i18n
                ? ["span", {i18n: opt.i18n}, opt.text]
                : ["span", {}, opt.text]);
            return ["button", {type: "button", class: cls}, kids,
                {click: function() { on_pick(opt.value); }}];
        });
        return ["div", {class: "buttons has-addons"}, btns];
    }

    let theme_seg = segment(
        [
            {value: "light", i18n: "light theme", text: "Light theme", icon: "yi-sun"},
            {value: "dark",  i18n: "dark theme",  text: "Dark theme",  icon: "yi-moon"}
        ],
        theme,
        function(v) {
            app_set_theme(v);
            render(gobj);
        }
    );

    let lang_seg = segment(
        [
            {value: "en", text: "English"},
            {value: "es", text: "Español"}
        ],
        lang,
        function(v) {
            switch_locale(v);
            refresh_language(document.body, i18next.t.bind(i18next));
            render(gobj);
        }
    );

    function field(label_key, label_text, control)
    {
        return ["div", {class: "field", style: "margin-bottom:1.25rem;"},
            [
                ["label", {class: "label", i18n: label_key}, label_text],
                ["div", {class: "control"}, [control]]
            ]
        ];
    }

    return ce(
        ["div", {},
            [
                page_header("preferences", "Preferences", "appearance", "Appearance"),
                ["div", {class: "box", style: "max-width:540px;"},
                    [
                        field("theme", "Theme", theme_seg),
                        field("language", "Language", lang_seg)
                    ]
                ]
            ]
        ]
    );
}

/***************************************************************
 *  Diagnostics — read-only deployment / link / session table,
 *  shown on the About page (one flat definition table).
 ***************************************************************/
function build_diagnostics(gobj)
{
    let dep = deploy_info();

    /*  These helpers read attrs off their service gobj — fetch the
     *  services first (passing undefined would dereference jn_attrs on
     *  undefined and abort gobj_create of this view). */
    let link   = gobj_find_service("agent_link", false);
    let login  = gobj_find_service("agent_login", false);
    let config = gobj_find_service("agent_config", false);

    let connected = !!(link && agent_link_is_connected(link));
    let logged    = agent_login_is_logged_in(login);
    let username  = agent_login_username(login) || "—";
    let node      = config ? agent_config_get_active_node(config) : "";

    function row(label_key, label_text, value, value_attrs)
    {
        return ["tr", {},
            [
                ["th", {style: "white-space:nowrap; color:#5B6B7E; font-weight:600; width:14rem;",
                        i18n: label_key}, label_text],
                ["td", value_attrs || {}, String(value)]
            ]
        ];
    }

    let node_cell = node
        ? ["td", {style: "font-family:monospace;"}, node]
        : ["td", {i18n: "none"}, "None"];

    let conn_attrs = {
        i18n: connected ? "connected" : "disconnected",
        style: `font-weight:600; color:${connected ? "#1FAE6F" : "#D64545"};`
    };

    let rows = [
        row("application", "Application", "gui_agent"),
        row("version", "Version", pkg.version || "—"),
        row("tenant", "Tenant", dep.tenant),
        row("plane", "Plane", dep.plane),
        row("host", "Host", dep.host),
        row("control center", "Control center", dep.cc_url, {style: "font-family:monospace;"}),
        row("auth bff", "Auth BFF", dep.bff_url, {style: "font-family:monospace;"}),
        row("connected", connected ? "Connected" : "Disconnected", "", conn_attrs),
        ["tr", {}, [
            ["th", {style: "white-space:nowrap; color:#5B6B7E; font-weight:600; width:14rem;",
                    i18n: "active node"}, "Active node"],
            node_cell
        ]],
        row("logged in as", "Logged in as",
            logged ? username : "", logged ? {} : {i18n: "logged out"})
    ];

    return ["div", {style: "max-width:680px; margin-top:1.5rem;"},
        [
            ["h2", {class: "title is-5", style: "margin-bottom:0.5rem;", i18n: "diagnostics"},
                "Diagnostics"],
            ["table", {class: "table is-fullwidth is-narrow"},
                [["tbody", {}, rows]]
            ]
        ]
    ];
}

/***************************************************************
 *  About — product card + diagnostics.
 ***************************************************************/
function build_about(gobj)
{
    let dep = deploy_info();
    let plane_label = (dep.plane === "agent22") ? "agent22" : "agents";

    return ce(
        ["div", {},
            [
                page_header("about", "About", null, ""),
                ["div", {class: "box", style: "max-width:540px; text-align:center;"},
                    [
                        ["img", {
                            src: "/agent-mark.svg",
                            alt: "Agent Console",
                            width: "72",
                            height: "72",
                            style: "margin-bottom:0.75rem;"
                        }, ""],
                        ["h1", {class: "title is-4", style: "margin-bottom:0.25rem;", i18n: "agent console"},
                            "Agent Console"],
                        ["p", {class: "subtitle is-6", style: "color:#5B6B7E; margin-bottom:0.75rem;"},
                            `v${pkg.version || ""} · ${dep.tenant} · ${plane_label}`],
                        ["p", {style: "color:#5B6B7E; margin-bottom:1rem;", i18n: "about description"},
                            "Browser console to operate Yuneta agents through the control center."],
                        ["a", {
                            class: "button is-link is-light",
                            href: "https://doc.yuneta.io",
                            target: "_blank",
                            rel: "noopener noreferrer"
                        },
                            [
                                ["span", {class: "icon"}, [["span", {class: "yi-question"}, ""]]],
                                ["span", {i18n: "documentation"}, "Documentation"]
                            ]
                        ],
                        ["p", {class: "is-size-7", style: "color:#9AA7B4; margin-top:1.25rem;"},
                            "© 2026 ArtGins"]
                    ]
                ],
                build_diagnostics(gobj)
            ]
        ]
    );
}




                    /***************************
                     *      Actions
                     ***************************/




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
        ["ST_IDLE", []]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [];

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        gmt,
        0,  // lmt
        attrs_table,
        PRIVATE_DATA,
        0,  // authz_table
        0,  // command_table
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
function register_c_account_view()
{
    return create_gclass(GCLASS_NAME);
}

export {register_c_account_view};
