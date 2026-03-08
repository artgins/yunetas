/***********************************************************************
 *          c_login.js
 *
 *          OAuth2 Direct Access Grant login with BFF (Backend For
 *          Frontend) support.
 *
 *  Security model (SEC-06):
 *  - Tokens are NEVER stored in localStorage or sessionStorage.
 *  - The browser sends username/password to the BFF endpoint
 *    (/auth/login).  The BFF exchanges them with Keycloak server-side
 *    using the Direct Access Grant (grant_type=password) and writes
 *    the tokens into httpOnly, Secure, SameSite=Strict cookies that
 *    JavaScript cannot read at all.
 *  - Token refresh and logout go through the BFF too (/auth/refresh,
 *    /auth/logout), so the JS side only ever sees {username, expires_in,
 *    refresh_expires_in} — no raw JWT.
 *  - The WebSocket HTTP-Upgrade request automatically carries the
 *    httpOnly cookies; the Yuneta backend reads them from the Cookie
 *    header and validates the JWT server-side.
 *
 *  Keycloak client configuration required:
 *  - Direct Access Grants (ROPC) ENABLED
 *  - Valid Redirect URIs: the app URL (e.g. https://treedb.yunetas.com/*)
 *  - Web Origins: the app origin (for CORS)
 *
 *  Copyright (c) 2025, ArtGins.
 *  All Rights Reserved.
 ***********************************************************************/

import {
    SDATA,
    SDATA_END,
    kw_flag_t,
    data_type_t,
    gclass_create,
    event_flag_t,
    log_error,
    log_info,
    gobj_subscribe_event,
    gobj_send_event,
    gobj_read_attr,
    gobj_write_attr,
    kw_get_str,
    json_object_size,
    get_now,
    is_object,
    empty_string,
    sprintf,
    gobj_change_state,
    gobj_read_pointer_attr,
    gobj_short_name,
    gobj_publish_event,
    set_timeout,
    clear_timeout,
    gobj_create_pure_child,
    gobj_name,
    gobj_start,
    gobj_stop,
    build_path,
} from "yunetas";

import {bff_urls} from "./conf/backend_config.js";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_LOGIN";

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",   0,      null,   "Subscriber of output events"),
SDATA(data_type_t.DTP_STRING,   "username",     0,      "",     "Authenticated username"),
SDATA(data_type_t.DTP_STRING,   "bff_url",      0,      "",     "Base URL of the BFF auth endpoint"),
SDATA_END()
];

let PRIVATE_DATA = {
    timeout_refresh:        0,  // seconds until next refresh
    refresh_expires_in:     0,  // seconds until refresh_token expires (from BFF)
    gobj_timer:             null,
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

    const subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, null, {}, subscriber);
    }

    // Resolve BFF URL for the current hostname
    let hostname = window.location.hostname || "localhost";

    let bff_url = bff_urls[hostname];
    if(bff_url !== undefined) {
        gobj_write_attr(gobj, "bff_url", bff_url);
    } else {
        log_error(`${gobj_short_name(gobj)}: BFF URL not found: '${hostname}'`);
    }
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    let priv = gobj.priv;

    gobj_start(priv.gobj_timer);

    /*
     *  Try to restore the session from httpOnly cookies on page load
     *  (e.g. after F5 refresh).  If the BFF refresh succeeds, fire
     *  EV_LOGIN_ACCEPTED.  If it fails, remain in ST_LOGOUT so the
     *  user sees the login form.
     */
    gobj_change_state(gobj, "ST_WAIT_TOKEN");
    try_restore_session(gobj);
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    let priv = gobj.priv;
    clear_timeout(priv.gobj_timer);
    gobj_stop(priv.gobj_timer);
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************
 *  POST username/password to the BFF /auth/login endpoint.
 *
 *  The BFF exchanges them with Keycloak using the Direct Access
 *  Grant (grant_type=password) and sets httpOnly cookies.
 *  Returns { success, username, email, expires_in,
 *            refresh_expires_in }.
 ***************************************************************/
async function do_bff_login(gobj, username, password)
{
    const bff_url = gobj_read_attr(gobj, "bff_url");

    try {
        const resp = await fetch(build_path(bff_url, "auth", "login"), {
            method:         "POST",
            credentials:    "include",
            headers:        { "Content-Type": "application/json" },
            body:           JSON.stringify({ username, password })
        });

        const data = await resp.json();

        if(resp.ok && data.success) {
            gobj_write_attr(gobj, "username", data.username || data.email || "");
            gobj_send_event(gobj, "EV_LOGIN_ACCEPTED", data, gobj);
        } else {
            const error = data.error || `HTTP ${resp.status}`;
            log_info(sprintf("%s: BFF login error: %s", gobj_short_name(gobj), error));
            gobj_send_event(gobj, "EV_LOGIN_DENIED", { error }, gobj);
        }

    } catch(err) {
        log_error(`${gobj_short_name(gobj)}: BFF login fetch failed: ${err.message}`);
        gobj_send_event(gobj, "EV_LOGIN_DENIED",
            { error: "Network error during login" }, gobj);
    }
}

/***************************************************************
 *  Call the BFF /auth/logout endpoint.
 *  The BFF revokes the refresh_token with Keycloak and clears cookies.
 ***************************************************************/
function do_bff_logout(gobj)
{
    const bff_url = gobj_read_attr(gobj, "bff_url");

    fetch(build_path(bff_url, "auth", "logout"), {
        method:         "POST",
        credentials:    "include",
        headers:        { "Content-Type": "application/json" }
    })
    .then(resp => resp.json().catch(() => ({})))
    .then(() => {
        // Always fire EV_LOGOUT_DONE regardless of BFF result — the
        // browser-side session is over; the cookie will expire anyway.
        gobj_send_event(gobj, "EV_LOGOUT_DONE", {}, gobj);
    })
    .catch(err => {
        log_error(`${gobj_short_name(gobj)}: BFF logout error: ${err.message}`);
        gobj_send_event(gobj, "EV_LOGOUT_DONE", { error: err.message }, gobj);
    });
}

/***************************************************************
 *  Call the BFF /auth/refresh endpoint.
 *  The BFF reads the refresh_token httpOnly cookie, calls Keycloak,
 *  and sets fresh access_token / refresh_token cookies.
 *  Returns {success, username, expires_in, refresh_expires_in}.
 ***************************************************************/
function do_bff_refresh(gobj)
{
    const bff_url = gobj_read_attr(gobj, "bff_url");

    fetch(build_path(bff_url, "auth", "refresh"), {
        method:         "POST",
        credentials:    "include",
        headers:        { "Content-Type": "application/json" }
    })
    .then(resp => resp.json())
    .then(data => {
        if(data.success) {
            gobj_send_event(gobj, "EV_LOGIN_REFRESHED", data, gobj);
        } else {
            log_info(sprintf("%s: BFF refresh denied: %s",
                gobj_short_name(gobj), data.error || "unknown"));
            gobj_send_event(gobj, "EV_LOGIN_DENIED",
                { error: data.error || "Refresh denied" }, gobj);
        }
    })
    .catch(err => {
        log_error(`${gobj_short_name(gobj)}: BFF refresh failed: ${err.message}`);
        gobj_send_event(gobj, "EV_LOGIN_DENIED",
            { error: "Network error during refresh" }, gobj);
    });
}

/***************************************************************
 *  Try to restore the session from httpOnly cookies on page load
 *  (e.g. after F5 refresh).
 *
 *  Calls BFF /auth/refresh:
 *  - If it succeeds, the cookies are still valid → fire
 *    EV_LOGIN_ACCEPTED to transition to ST_LOGIN.
 *  - If it fails (no cookies, expired, network error), silently
 *    go back to ST_LOGOUT so the user sees the login form.
 *    No error is shown — this is a normal "no session" case.
 ***************************************************************/
function try_restore_session(gobj)
{
    const bff_url = gobj_read_attr(gobj, "bff_url");

    fetch(build_path(bff_url, "auth", "refresh"), {
        method:         "POST",
        credentials:    "include",
        headers:        { "Content-Type": "application/json" }
    })
    .then(resp => resp.json())
    .then(data => {
        if(data.success) {
            gobj_write_attr(gobj, "username", data.username || data.email || "");
            gobj_send_event(gobj, "EV_LOGIN_ACCEPTED", data, gobj);
        } else {
            // No valid session — go back to login screen (no error shown)
            gobj_change_state(gobj, "ST_LOGOUT");
        }
    })
    .catch(() => {
        // Network error or BFF not available — go back to login screen
        gobj_change_state(gobj, "ST_LOGOUT");
    });
}

/***************************************************************
 *  Store session timing info (no tokens — never in JS).
 *  Arms the refresh timer so the BFF is called before the
 *  refresh_token expires.
 ***************************************************************/
function save_session_info(gobj, data)
{
    let priv = gobj.priv;

    /*
     *  data = { success, username, email, expires_in, refresh_expires_in }
     *
     *  expires_in:         access token lifetime in seconds
     *  refresh_expires_in: refresh token lifetime in seconds
     *
     *  We schedule a BFF /auth/refresh call before the access_token
     *  expires, so the WebSocket always has a valid JWT cookie.
     *  Refresh 30 seconds early (or 75% of lifetime if very short).
     *  If the refresh_token itself has expired, the BFF will return
     *  an error and we fall back to ST_LOGOUT.
     */
    priv.refresh_expires_in = data.refresh_expires_in || 0;

    let access_expires = data.expires_in || 300;
    priv.timeout_refresh = Math.max(
        Math.floor(access_expires * 0.75),
        access_expires - 30
    );
    if(priv.timeout_refresh <= 0) {
        priv.timeout_refresh = 2;
    }
    set_timeout(priv.gobj_timer, priv.timeout_refresh * 1000);
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************
 *  EV_DO_LOGIN — initiates the Direct Access Grant via BFF.
 *  kw: { username, password }
 ***************************************************************/
function ac_do_login(gobj, event, kw, src)
{
    const username = (kw && kw.username) ? kw.username : "";
    const password = (kw && kw.password) ? kw.password : "";
    do_bff_login(gobj, username, password);
    return 0;
}

function ac_do_logout(gobj, event, kw, src)
{
    do_bff_logout(gobj);
    return 0;
}

/***************************************************************
 *  EV_LOGIN_ACCEPTED — BFF exchanged the credentials successfully.
 *  kw: { success, username, email, expires_in, refresh_expires_in }
 ***************************************************************/
function ac_login_accepted(gobj, event, kw, src)
{
    save_session_info(gobj, kw);

    gobj_publish_event(gobj, "EV_LOGIN_ACCEPTED", {
        username: gobj_read_attr(gobj, "username")
        /*
         *  SEC-06: jwt is intentionally omitted.  The httpOnly cookie is
         *  sent automatically by the browser with the WebSocket upgrade —
         *  the JS side never needs to see or forward the token.
         */
    });
    return 0;
}

/***************************************************************
 *  EV_LOGIN_REFRESHED — BFF refresh succeeded.
 *  kw: { success, username, expires_in, refresh_expires_in }
 ***************************************************************/
function ac_login_refreshed(gobj, event, kw, src)
{
    save_session_info(gobj, kw);

    gobj_publish_event(gobj, "EV_LOGIN_REFRESHED", {
        username: gobj_read_attr(gobj, "username")
    });
    return 0;
}

function ac_login_denied(gobj, event, kw, src)
{
    gobj_write_attr(gobj, "username", "");
    gobj_publish_event(gobj, "EV_LOGIN_DENIED", kw);
    return 0;
}

function ac_logout_done(gobj, event, kw, src)
{
    gobj_write_attr(gobj, "username", "");
    gobj_publish_event(gobj, "EV_LOGOUT_DONE", kw);
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
                     *          FSM
                     ***************************/




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
        ["ST_LOGOUT", [
            ["EV_DO_LOGIN",       ac_do_login,        "ST_WAIT_TOKEN"],
            ["EV_LOGIN_DENIED",   ac_login_denied,    null],
            ["EV_DO_LOGOUT",      ac_clear_session,   null],
            ["EV_LOGOUT_DONE",    ac_clear_session,   null]
        ]],

        ["ST_WAIT_TOKEN", [
            ["EV_LOGIN_ACCEPTED", ac_login_accepted,  "ST_LOGIN"],
            ["EV_LOGIN_DENIED",   ac_login_denied,    "ST_LOGOUT"]
        ]],

        ["ST_LOGIN", [
            ["EV_DO_LOGOUT",      ac_do_logout,       null],
            ["EV_LOGIN_REFRESHED",ac_login_refreshed, null],
            ["EV_LOGIN_DENIED",   ac_login_denied,    "ST_LOGOUT"],
            ["EV_LOGOUT_DONE",    ac_logout_done,     "ST_LOGOUT"],
            ["EV_TIMEOUT",        ac_timeout,         null]
        ]]
    ];

    const event_types = [
        ["EV_DO_LOGIN",         0],
        ["EV_DO_LOGOUT",        0],
        ["EV_LOGIN_ACCEPTED",   event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_LOGIN_DENIED",     event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_LOGIN_REFRESHED",  event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_LOGOUT_DONE",      event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_TIMEOUT",          0]
    ];

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        gmt,
        0,
        attrs_table,
        PRIVATE_DATA,
        0,
        0,
        0,
        0
    );

    return __gclass__ ? 0 : -1;
}

function register_c_login()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_login };
