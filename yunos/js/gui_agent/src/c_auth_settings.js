/***********************************************************************
 *          c_auth_settings.js
 *
 *      C_AUTH_SETTINGS — authentication settings, a routed stage view
 *      (mounted by C_YUI_SHELL at /settings/auth).
 *
 *      Two sections:
 *        1. OIDC/Keycloak config (auth url, realm, client id) → stored
 *           in C_AGENT_CONFIG (persisted in the browser).
 *        2. Login (username / password) → sent to C_AGENT_LOGIN, which
 *           does the ROPC grant and holds the token in memory. The view
 *           reflects the login state from the service's events.
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
    gobj_find_service,
    gobj_send_event,
    createElement2,
    refresh_language,
} from "@yuneta/gobj-js";

import i18next from "i18next";

import {agent_login_is_logged_in, agent_login_username} from "./c_agent_login.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_AUTH_SETTINGS";


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",  0,  null,       "Subscriber of output events"),

SDATA(data_type_t.DTP_STRING,   "title",       0,  "authentication", "View title (i18n key)"),
SDATA(data_type_t.DTP_POINTER,  "$container",  0,  null,       "Root HTMLElement"),
SDATA(data_type_t.DTP_POINTER,  "config_svc",  0,  null,       "C_AGENT_CONFIG service"),
SDATA(data_type_t.DTP_POINTER,  "login_svc",   0,  null,       "C_AGENT_LOGIN service"),
SDATA(data_type_t.DTP_STRING,   "notice",      0,  "",         "Transient status/error to show"),
SDATA(data_type_t.DTP_BOOLEAN,  "notice_err",  0,  false,      "Whether the notice is an error"),
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

    gobj_write_attr(gobj, "config_svc", gobj_find_service("agent_config", true));

    let login = gobj_find_service("agent_login", true);
    gobj_write_attr(gobj, "login_svc", login);
    if(login) {
        gobj_subscribe_event(login, null, {}, gobj);
    }

    let $c = createElement2(["div", {class: "view-card", style: "overflow:auto;"}, []]);
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




function clear_node($n)
{
    while($n && $n.firstChild) {
        $n.removeChild($n.firstChild);
    }
}

/***************************************************************
 *  One bulma form field (label + input).
 ***************************************************************/
function field(label_key, name, value, type, placeholder)
{
    return ["div", {class: "field"}, [
        ["label", {class: "label is-small", i18n: label_key}, label_key],
        ["div", {class: "control"}, [
            ["input", {
                class: "input is-small",
                type: type || "text",
                name: name,
                value: value || "",
                placeholder: placeholder || ""
            }]
        ]]
    ]];
}

function field_value($root, name)
{
    let el = $root.querySelector(`[name="${name}"]`);
    return el ? el.value.trim() : "";
}

/***************************************************************
 *  Password field with a show/hide eye toggle.
 ***************************************************************/
function password_field(name, value)
{
    let $input = createElement2(["input", {
        class: "input is-small", type: "password", name: name, value: value || ""
    }]);
    let $icon = createElement2(["i", {class: "yi-eye"}]);
    let $btn = createElement2(
        ["button", {class: "button is-small", type: "button", "aria-label": "show password"},
            [["span", {class: "icon is-small"}, [$icon]]]]
    );
    $btn.addEventListener("click", () => {
        let show = ($input.type === "password");
        $input.type = show ? "text" : "password";
        $icon.className = show ? "yi-eye-slash" : "yi-eye";
        $input.focus();
    });
    return createElement2(
        ["div", {class: "field"}, [
            ["label", {class: "label is-small", i18n: "password"}, "Password"],
            ["div", {class: "field has-addons mb-0"}, [
                ["div", {class: "control is-expanded"}, [$input]],
                ["div", {class: "control"}, [$btn]]
            ]]
        ]]
    );
}

/***************************************************************
 *  Rebuild the whole view.
 ***************************************************************/
function render(gobj)
{
    let $c = gobj_read_attr(gobj, "$container");
    if(!$c) {
        return;
    }
    clear_node($c);

    let login_svc  = gobj_read_attr(gobj, "login_svc");
    let logged_in = login_svc ? agent_login_is_logged_in(login_svc) : false;
    let username  = login_svc ? agent_login_username(login_svc) : "";

    /*-------------------------------*
     *      Header
     *-------------------------------*/
    $c.appendChild(createElement2(
        ["div", {}, [
            ["h1", {class: "title is-4", style: "color:#2E7CD6;", i18n: "authentication"}, "Authentication"],
            ["p", {class: "subtitle is-6", style: "color:#5B6B7E;",
                   i18n: "auth subtitle"}, "Sign in via the control center's BFF"]
        ]]
    ));

    /*-------------------------------*
     *      Notice
     *-------------------------------*/
    let notice = gobj_read_attr(gobj, "notice");
    if(notice) {
        let err = gobj_read_attr(gobj, "notice_err");
        $c.appendChild(createElement2(
            ["div", {class: `notification is-light ${err ? "is-danger" : "is-success"} p-2 mb-3`}, notice]
        ));
    }

    /*-------------------------------*
     *      Login form / session
     *-------------------------------*/
    let $login;
    if(logged_in) {
        $login = createElement2(
            ["div", {class: "box"}, [
                ["h2", {class: "title is-5", i18n: "session"}, "Session"],
                ["p", {class: "mb-3"}, [
                    ["span", {i18n: "logged in as"}, "Logged in as"],
                    ["span", {class: "has-text-weight-bold ml-2"}, username]
                ]],
                ["div", {class: "buttons"}, [
                    ["button", {class: "button is-small is-danger is-light", type: "button", i18n: "logout"},
                        "Logout", {click: () => on_logout(gobj)}]
                ]]
            ]]
        );
    } else {
        $login = createElement2(
            ["div", {class: "box"}, [
                ["h2", {class: "title is-5", i18n: "login"}, "Sign In"],
                field("username", "username", "", "text", ""),
                password_field("password", ""),
                ["div", {class: "buttons"}, [
                    ["button", {class: "button is-small is-primary", type: "button", i18n: "login"},
                        "Sign In", {click: () => on_login(gobj, $login)}]
                ]]
            ]]
        );
    }
    $c.appendChild($login);

    refresh_language($c, i18next.t.bind(i18next));
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************
 *  Submit the login form.
 ***************************************************************/
function on_login(gobj, $login)
{
    let login_svc = gobj_read_attr(gobj, "login_svc");
    if(!login_svc) {
        return;
    }
    let username = field_value($login, "username");
    let password = field_value($login, "password");
    if(!username || !password) {
        set_notice(gobj, i18next.t("username and password are required"), true);
        render(gobj);
        return;
    }
    set_notice(gobj, i18next.t("signing in") + "…", false);
    render(gobj);
    gobj_send_event(login_svc, "EV_DO_LOGIN", {username: username, password: password}, gobj);
}

/***************************************************************
 *  Log out.
 ***************************************************************/
function on_logout(gobj)
{
    let login_svc = gobj_read_attr(gobj, "login_svc");
    if(login_svc) {
        gobj_send_event(login_svc, "EV_DO_LOGOUT", {}, gobj);
    }
}

/***************************************************************
 *  Set a transient notice.
 ***************************************************************/
function set_notice(gobj, text, is_err)
{
    gobj_write_attr(gobj, "notice", text || "");
    gobj_write_attr(gobj, "notice_err", !!is_err);
}

/***************************************************************
 *  Login service events.
 ***************************************************************/
function ac_login_accepted(gobj, event, kw, src)
{
    set_notice(gobj, `${i18next.t("logged in as")} ${kw.username || ""}`, false);
    render(gobj);
    return 0;
}

function ac_login_denied(gobj, event, kw, src)
{
    let msg = kw.error_description || kw.error || i18next.t("login failed");
    set_notice(gobj, `${i18next.t("login failed")}: ${msg}`, true);
    render(gobj);
    return 0;
}

function ac_logout_done(gobj, event, kw, src)
{
    set_notice(gobj, i18next.t("logged out"), false);
    render(gobj);
    return 0;
}

function ac_login_refreshed(gobj, event, kw, src)
{
    return 0;
}




                    /***************************
                     *              FSM
                     ***************************/




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
        ["ST_IDLE", [
            ["EV_LOGIN_ACCEPTED",  ac_login_accepted,  null],
            ["EV_LOGIN_DENIED",    ac_login_denied,    null],
            ["EV_LOGOUT_DONE",     ac_logout_done,     null],
            ["EV_LOGIN_REFRESHED", ac_login_refreshed, null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_LOGIN_ACCEPTED",  0],
        ["EV_LOGIN_DENIED",    0],
        ["EV_LOGOUT_DONE",     0],
        ["EV_LOGIN_REFRESHED", 0]
    ];

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
function register_c_auth_settings()
{
    return create_gclass(GCLASS_NAME);
}

export {register_c_auth_settings};
