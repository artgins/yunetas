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
    gobj_subscribe_event,
    createElement2,
    refresh_language,
} from "@yuneta/gobj-js";

import i18next from "i18next";

import pkg from "../package.json";

import {deploy_info} from "./conf/deploy.js";
import {current_theme, apply_theme} from "./theme.js";
import {current_locale, switch_locale} from "./locales/locales.js";
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




function t(key)
{
    return i18next.t(key);
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
    } else if(view === "developer") {
        body = build_developer(gobj);
    } else {
        body = build_about(gobj);
    }
    $c.appendChild(body);
}

/***************************************************************
 *  Page header (title + optional subtitle).
 ***************************************************************/
function page_header(title, subtitle)
{
    let children = [
        ["h1", {class: "title is-3", style: `color:${ACCENT}; margin-bottom:0.25rem;`}, title]
    ];
    if(subtitle) {
        children.push(
            ["p", {class: "subtitle is-6", style: "color:#5B6B7E; margin-bottom:1rem;"}, subtitle]
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

    /*  A segmented button group; the active option carries is-primary.  */
    function segment(options, current, on_pick)
    {
        let btns = options.map(function(opt) {
            let active = (opt.value === current);
            let cls = "button" + (active ? " is-primary is-selected" : "");
            let kids = [];
            if(opt.icon) {
                kids.push(["span", {class: "icon"}, [["span", {class: opt.icon}, ""]]]);
            }
            kids.push(["span", {}, opt.label]);
            return ["button", {
                type: "button",
                class: cls,
                onclick: function() { on_pick(opt.value); }
            }, kids];
        });
        return ["div", {class: "buttons has-addons"}, btns];
    }

    let theme_seg = segment(
        [
            {value: "light", label: t("light theme"), icon: "yi-sun"},
            {value: "dark",  label: t("dark theme"),  icon: "yi-moon"}
        ],
        theme,
        function(v) {
            apply_theme(v);
            render(gobj);
        }
    );

    let lang_seg = segment(
        [
            {value: "en", label: "English"},
            {value: "es", label: "Español"}
        ],
        lang,
        function(v) {
            switch_locale(v);
            refresh_language(document.body, i18next.t.bind(i18next));
            render(gobj);
        }
    );

    function field(label_key, control)
    {
        return ["div", {class: "field", style: "margin-bottom:1.25rem;"},
            [
                ["label", {class: "label"}, t(label_key)],
                ["div", {class: "control"}, [control]]
            ]
        ];
    }

    return createElement2(
        ["div", {},
            [
                page_header(t("preferences"), t("appearance")),
                ["div", {class: "box", style: "max-width:540px;"},
                    [
                        field("theme", theme_seg),
                        field("language", lang_seg)
                    ]
                ]
            ]
        ]
    );
}

/***************************************************************
 *  Developer — read-only diagnostics.
 ***************************************************************/
function build_developer(gobj)
{
    let dep = deploy_info();
    let connected = agent_link_is_connected();
    let logged    = agent_login_is_logged_in();
    let username  = agent_login_username() || "—";
    let node      = agent_config_get_active_node() || t("none");

    function row(label_key, value, value_style)
    {
        return ["tr", {},
            [
                ["th", {style: "white-space:nowrap; color:#5B6B7E; font-weight:600; width:14rem;"},
                    t(label_key)],
                ["td", {style: value_style || ""}, String(value)]
            ]
        ];
    }

    function yesno(b)
    {
        return b ? t("connected") : t("disconnected");
    }

    let app_table = ["table", {class: "table is-fullwidth is-narrow"},
        [["tbody", {},
            [
                row("application", "gui_agent"),
                row("version", pkg.version || "—"),
                row("tenant", dep.tenant),
                row("plane", dep.plane),
                row("host", dep.host)
            ]
        ]]
    ];

    let link_table = ["table", {class: "table is-fullwidth is-narrow"},
        [["tbody", {},
            [
                row("control center", dep.cc_url, "font-family:monospace;"),
                row("auth bff", dep.bff_url, "font-family:monospace;"),
                row("connected", yesno(connected),
                    `font-weight:600; color:${connected ? "#1FAE6F" : "#D64545"};`),
                row("active node", node, "font-family:monospace;")
            ]
        ]]
    ];

    let session_table = ["table", {class: "table is-fullwidth is-narrow"},
        [["tbody", {},
            [
                row("logged in as", logged ? username : t("logged out")),
                row("username", username, "font-family:monospace;")
            ]
        ]]
    ];

    function section(title_key, table)
    {
        return ["div", {style: "margin-bottom:1.5rem;"},
            [
                ["h2", {class: "title is-5", style: "margin-bottom:0.5rem;"}, t(title_key)],
                table
            ]
        ];
    }

    return createElement2(
        ["div", {},
            [
                page_header(t("developer"), t("diagnostics")),
                ["div", {style: "max-width:680px;"},
                    [
                        section("application", app_table),
                        section("control center", link_table),
                        section("session", session_table)
                    ]
                ]
            ]
        ]
    );
}

/***************************************************************
 *  About — product card.
 ***************************************************************/
function build_about(gobj)
{
    let dep = deploy_info();
    let plane_label = (dep.plane === "agent22") ? "agent22" : "agents";

    return createElement2(
        ["div", {},
            [
                page_header(t("about"), ""),
                ["div", {class: "box", style: "max-width:540px; text-align:center;"},
                    [
                        ["img", {
                            src: "/agent-mark.svg",
                            alt: "Agent Console",
                            width: "72",
                            height: "72",
                            style: "margin-bottom:0.75rem;"
                        }, ""],
                        ["h1", {class: "title is-4", style: "margin-bottom:0.25rem;"},
                            t("agent console")],
                        ["p", {class: "subtitle is-6", style: "color:#5B6B7E; margin-bottom:0.75rem;"},
                            `v${pkg.version || ""} · ${dep.tenant} · ${plane_label}`],
                        ["p", {style: "color:#5B6B7E; margin-bottom:1rem;"},
                            t("about description")],
                        ["a", {
                            class: "button is-link is-light",
                            href: "https://doc.yuneta.io",
                            target: "_blank",
                            rel: "noopener noreferrer"
                        },
                            [
                                ["span", {class: "icon"}, [["span", {class: "yi-question"}, ""]]],
                                ["span", {}, t("documentation")]
                            ]
                        ],
                        ["p", {class: "is-size-7", style: "color:#9AA7B4; margin-top:1.25rem;"},
                            "© 2026 ArtGins"]
                    ]
                ]
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
