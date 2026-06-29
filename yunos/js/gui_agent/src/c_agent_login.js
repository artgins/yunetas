/***********************************************************************
 *          c_agent_login.js
 *
 *      C_AGENT_LOGIN — OIDC login via the auth BFF (named service
 *      "agent_login"), the same model wattyzer/gui_treedb use.
 *
 *      Security (SEC-06): tokens are NEVER in JavaScript. The browser
 *      POSTs username/password to the BFF /auth/login; the BFF exchanges
 *      them with Keycloak server-side and writes access/refresh tokens as
 *      httpOnly, Secure, SameSite=Strict cookies scoped to this host.
 *      The agent WebSocket upgrade then carries those cookies
 *      automatically (same-host), so the JS side only ever sees
 *      {username, expires_in}. Refresh / logout / session-restore also go
 *      through the BFF.
 *
 *      The BFF base URL comes from the (user-configurable) auth config in
 *      C_AGENT_CONFIG; empty defaults to https://<host>:1806. The BFF
 *      MUST be same-host as the SPA (its cookie is scoped to this host).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA, SDATA_END, data_type_t, event_flag_t,
    gclass_create, log_error, log_info,
    gobj_name, gobj_short_name,
    gobj_read_attr, gobj_read_pointer_attr, gobj_write_attr,
    gobj_change_state, gobj_current_state,
    gobj_subscribe_event, gobj_publish_event, gobj_send_event,
    gobj_find_service,
    gobj_create_pure_child, gobj_start, gobj_stop,
    set_timeout, clear_timeout,
} from "@yuneta/gobj-js";

import {agent_config_get_bff_url} from "./c_agent_config.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_AGENT_LOGIN";


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",  0,  null,  "Subscriber of output events"),
SDATA(data_type_t.DTP_POINTER,  "config_svc",  0,  null,  "C_AGENT_CONFIG service"),
SDATA(data_type_t.DTP_STRING,   "username",    0,  "",    "Authenticated username"),
SDATA_END()
];

let PRIVATE_DATA = {
    gobj_timer:      null,
    timeout_refresh: 0,
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
    priv.gobj_timer = gobj_create_pure_child(gobj_name(gobj), "C_TIMER", {}, gobj);

    /*
     *  SERVICE subscription model
     */
    const subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, null, {}, subscriber);
    }

    gobj_write_attr(gobj, "config_svc", gobj_find_service("agent_config", true));
}

/***************************************************************
 *          Framework Method: Start
 *  Try to restore a session from the httpOnly cookies (F5).
 ***************************************************************/
function mt_start(gobj)
{
    gobj_start(gobj.priv.gobj_timer);
    gobj_change_state(gobj, "ST_WAIT_TOKEN");
    try_restore_session(gobj);
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    clear_timeout(gobj.priv.gobj_timer);
    gobj_stop(gobj.priv.gobj_timer);
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
}




                    /***************************
                     *      Public functions
                     ***************************/




/***************************************************************
 *  Username of the current session, or "".
 ***************************************************************/
function agent_login_username(gobj)
{
    return (gobj && gobj.priv) ? (gobj_read_attr(gobj, "username") || "") : "";
}

/***************************************************************
 *  True while a session is established (cookie held by browser).
 ***************************************************************/
function agent_login_is_logged_in(gobj)
{
    return !!(gobj && gobj_current_state(gobj) === "ST_LOGIN");
}




                    /***************************
                     *      Local Methods
                     ***************************/




function bff_base(gobj)
{
    return agent_config_get_bff_url(gobj_read_attr(gobj, "config_svc"));
}

/***************************************************************
 *  POST username/password to the BFF /auth/login. The BFF sets
 *  httpOnly cookies and returns {success, username, expires_in,
 *  refresh_expires_in}.
 ***************************************************************/
async function do_bff_login(gobj, username, password)
{
    let url = `${bff_base(gobj)}/auth/login`;
    try {
        let resp = await fetch(url, {
            method:      "POST",
            credentials: "include",
            headers:     {"Content-Type": "application/json"},
            body:        JSON.stringify({username, password})
        });
        let data = await resp.json().catch(() => ({}));
        if(resp.ok && data.success) {
            gobj_write_attr(gobj, "username", data.username || data.email || username);
            gobj_send_event(gobj, "EV_LOGIN_ACCEPTED", data, gobj);
        } else {
            gobj_send_event(gobj, "EV_LOGIN_DENIED", {
                error_code: data.error_code || `http_${resp.status}`,
                error:      data.error || `HTTP ${resp.status}`
            }, gobj);
        }
    } catch(err) {
        log_error(`${gobj_short_name(gobj)}: BFF login failed: ${err.message}`);
        gobj_send_event(gobj, "EV_LOGIN_DENIED",
            {error_code: "network_error", error: bff_hint(err, url)}, gobj);
    }
}

/***************************************************************
 *  POST /auth/refresh (reads refresh_token cookie, rotates cookies).
 ***************************************************************/
function do_bff_refresh(gobj)
{
    fetch(`${bff_base(gobj)}/auth/refresh`, {
        method: "POST", credentials: "include",
        headers: {"Content-Type": "application/json"}
    })
    .then(resp => resp.json().catch(() => ({})))
    .then(data => {
        if(data.success) {
            gobj_send_event(gobj, "EV_LOGIN_REFRESHED", data, gobj);
        } else {
            gobj_send_event(gobj, "EV_LOGIN_DENIED", {
                error_code: data.error_code || "refresh_denied",
                error:      data.error || "Refresh denied"
            }, gobj);
        }
    })
    .catch(err => {
        gobj_send_event(gobj, "EV_LOGIN_DENIED",
            {error_code: "network_error", error: err.message}, gobj);
    });
}

/***************************************************************
 *  POST /auth/logout (revokes + clears cookies).
 ***************************************************************/
function do_bff_logout(gobj)
{
    fetch(`${bff_base(gobj)}/auth/logout`, {
        method: "POST", credentials: "include",
        headers: {"Content-Type": "application/json"}
    })
    .then(resp => resp.json().catch(() => ({})))
    .then(() => gobj_send_event(gobj, "EV_LOGOUT_DONE", {}, gobj))
    .catch(() => gobj_send_event(gobj, "EV_LOGOUT_DONE", {}, gobj));
}

/***************************************************************
 *  On load, try to restore a session via /auth/refresh (the
 *  cookies, if still valid). No error shown on failure — it's the
 *  normal "no session yet" case.
 ***************************************************************/
function try_restore_session(gobj)
{
    fetch(`${bff_base(gobj)}/auth/refresh`, {
        method: "POST", credentials: "include",
        headers: {"Content-Type": "application/json"}
    })
    .then(resp => resp.json().catch(() => ({})))
    .then(data => {
        if(data.success) {
            gobj_write_attr(gobj, "username", data.username || data.email || "");
            gobj_send_event(gobj, "EV_LOGIN_ACCEPTED", data, gobj);
        } else {
            gobj_change_state(gobj, "ST_LOGOUT");
        }
    })
    .catch(() => {
        gobj_change_state(gobj, "ST_LOGOUT");
    });
}

/***************************************************************
 *  Arm the refresh timer to run before the access token expires,
 *  so the cookie the WebSocket carries stays valid.
 ***************************************************************/
function save_session_info(gobj, data)
{
    let priv = gobj.priv;
    let access_expires = parseInt(data.expires_in, 10) || 300;
    priv.timeout_refresh = Math.max(Math.floor(access_expires * 0.75), access_expires - 30);
    if(priv.timeout_refresh <= 0) {
        priv.timeout_refresh = 2;
    }
    set_timeout(priv.gobj_timer, priv.timeout_refresh * 1000);
}

/***************************************************************
 *  A "Failed to fetch" against the BFF is usually CORS/host/cert.
 ***************************************************************/
function bff_hint(err, url)
{
    if(err instanceof TypeError) {
        return `${err.message} — check the BFF is reachable at ${url} ` +
               `(same host as this app, cert trusted, allowed_origin set)`;
    }
    return err.message;
}




                    /***************************
                     *      Actions
                     ***************************/




function ac_do_login(gobj, event, kw, src)
{
    do_bff_login(gobj, kw.username || "", kw.password || "");
    return 0;
}

function ac_do_logout(gobj, event, kw, src)
{
    do_bff_logout(gobj);
    return 0;
}

function ac_login_accepted(gobj, event, kw, src)
{
    save_session_info(gobj, kw);
    log_info(`${gobj_name(gobj)}: login accepted for '${gobj_read_attr(gobj, "username")}'`);
    gobj_publish_event(gobj, "EV_LOGIN_ACCEPTED", {username: gobj_read_attr(gobj, "username")});
    return 0;
}

function ac_login_refreshed(gobj, event, kw, src)
{
    save_session_info(gobj, kw);
    gobj_publish_event(gobj, "EV_LOGIN_REFRESHED", {username: gobj_read_attr(gobj, "username")});
    return 0;
}

function ac_login_denied(gobj, event, kw, src)
{
    clear_timeout(gobj.priv.gobj_timer);
    gobj_write_attr(gobj, "username", "");
    gobj_publish_event(gobj, "EV_LOGIN_DENIED", {
        error_code: kw.error_code || "",
        error:      kw.error || ""
    });
    return 0;
}

function ac_logout_done(gobj, event, kw, src)
{
    clear_timeout(gobj.priv.gobj_timer);
    gobj_write_attr(gobj, "username", "");
    gobj_publish_event(gobj, "EV_LOGOUT_DONE", {});
    return 0;
}

function ac_clear_session(gobj, event, kw, src)
{
    gobj_write_attr(gobj, "username", "");
    return 0;
}

function ac_timeout(gobj, event, kw, src)
{
    do_bff_refresh(gobj);
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
        ["ST_LOGOUT", [
            ["EV_DO_LOGIN",        ac_do_login,        "ST_WAIT_TOKEN"],
            ["EV_LOGIN_DENIED",    ac_login_denied,    null],
            ["EV_DO_LOGOUT",       ac_clear_session,   null],
            ["EV_LOGOUT_DONE",     ac_clear_session,   null]
        ]],
        ["ST_WAIT_TOKEN", [
            ["EV_DO_LOGIN",        ac_do_login,        null],
            ["EV_LOGIN_ACCEPTED",  ac_login_accepted,  "ST_LOGIN"],
            ["EV_LOGIN_DENIED",    ac_login_denied,    "ST_LOGOUT"]
        ]],
        ["ST_LOGIN", [
            ["EV_DO_LOGOUT",       ac_do_logout,       null],
            ["EV_LOGIN_REFRESHED", ac_login_refreshed, null],
            ["EV_LOGIN_DENIED",    ac_login_denied,    "ST_LOGOUT"],
            ["EV_LOGOUT_DONE",     ac_logout_done,     "ST_LOGOUT"],
            ["EV_TIMEOUT",         ac_timeout,         null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const out = event_flag_t.EVF_OUTPUT_EVENT | event_flag_t.EVF_NO_WARN_SUBS;
    const event_types = [
        ["EV_DO_LOGIN",        0],
        ["EV_DO_LOGOUT",       0],
        ["EV_TIMEOUT",         0],
        ["EV_LOGIN_ACCEPTED",  out],
        ["EV_LOGIN_REFRESHED", out],
        ["EV_LOGIN_DENIED",    out],
        ["EV_LOGOUT_DONE",     out]
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
function register_c_agent_login()
{
    return create_gclass(GCLASS_NAME);
}

export {
    register_c_agent_login,
    agent_login_username,
    agent_login_is_logged_in,
};
