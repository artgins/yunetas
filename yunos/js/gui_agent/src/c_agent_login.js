/***********************************************************************
 *          c_agent_login.js
 *
 *      C_AGENT_LOGIN — OIDC login service (named service "agent_login").
 *
 *      Obtains a Keycloak access token via the Resource Owner Password
 *      Credentials grant (ROPC, grant_type=password) straight against the
 *      token endpoint of the user-configured Keycloak (auth config lives
 *      in C_AGENT_CONFIG), refreshes it before expiry, and exposes the
 *      current access token so C_AGENT_CONSOLE can forward it as the
 *      C_IEVENT_CLI `jwt` (the agent validates the JWT in the identity
 *      card — the path ycommand uses).
 *
 *      Tokens live ONLY in memory (priv): never persisted, so a reload
 *      requires a fresh login — no JWT sits in localStorage.
 *
 *      Publishes EV_LOGIN_ACCEPTED / EV_LOGIN_DENIED / EV_LOGOUT_DONE /
 *      EV_LOGIN_REFRESHED to its subscribers (the auth view + the
 *      console).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA, SDATA_END, data_type_t, event_flag_t,
    gclass_create, log_error, log_info,
    gobj_name,
    gobj_read_pointer_attr,
    gobj_subscribe_event,
    gobj_publish_event,
    gobj_send_event,
    gobj_find_service,
    gobj_create_pure_child,
    gobj_start,
} from "@yuneta/gobj-js";

import {set_timeout, clear_timeout} from "@yuneta/gobj-js";

import {agent_config_get_auth} from "./c_agent_config.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_AGENT_LOGIN";

/*  Refresh this many seconds before the access token expires. */
const REFRESH_SKEW = 30;


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",  0,  null,  "Subscriber of output events"),
SDATA(data_type_t.DTP_POINTER,  "config_svc",  0,  null,  "C_AGENT_CONFIG service"),
SDATA_END()
];

let PRIVATE_DATA = {
    gobj_timer:     null,
    access_token:   "",
    refresh_token:  "",
    username:       "",
    expires_at:     0,
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
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    gobj_start(gobj.priv.gobj_timer);
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    clear_timeout(gobj.priv.gobj_timer);
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
 *  Current access token, or "" when logged out.
 ***************************************************************/
function agent_login_get_token(gobj)
{
    return (gobj && gobj.priv && gobj.priv.access_token) || "";
}

/***************************************************************
 *  Username of the current session, or "".
 ***************************************************************/
function agent_login_username(gobj)
{
    return (gobj && gobj.priv && gobj.priv.username) || "";
}

/***************************************************************
 *  True while a token is held.
 ***************************************************************/
function agent_login_is_logged_in(gobj)
{
    return !!(gobj && gobj.priv && gobj.priv.access_token);
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************
 *  Build the Keycloak token endpoint URL from the auth config.
 ***************************************************************/
function token_url(auth)
{
    let base = String(auth.auth_url || "").replace(/\/+$/, "");
    return `${base}/realms/${encodeURIComponent(auth.realm)}/protocol/openid-connect/token`;
}

/***************************************************************
 *  C_AGENT_CONFIG service (cached).
 ***************************************************************/
function config_service(gobj)
{
    let svc = gobj_read_pointer_attr(gobj, "config_svc");
    if(!svc) {
        svc = gobj_find_service("agent_config", true);
    }
    return svc;
}

/***************************************************************
 *  Store tokens from a Keycloak token response into priv and
 *  schedule the refresh.
 ***************************************************************/
function store_tokens(gobj, data, username)
{
    let priv = gobj.priv;
    priv.access_token  = data.access_token || "";
    priv.refresh_token = data.refresh_token || "";
    if(username) {
        priv.username = username;
    }
    let expires_in = parseInt(data.expires_in, 10) || 0;
    priv.expires_at = Date.now() + expires_in * 1000;

    let delay = expires_in - REFRESH_SKEW;
    if(delay < REFRESH_SKEW) {
        delay = Math.max(5, Math.floor(expires_in / 2));
    }
    set_timeout(priv.gobj_timer, delay * 1000);
}

/***************************************************************
 *  Clear all token state.
 ***************************************************************/
function clear_tokens(gobj)
{
    let priv = gobj.priv;
    clear_timeout(priv.gobj_timer);
    priv.access_token  = "";
    priv.refresh_token = "";
    priv.username      = "";
    priv.expires_at    = 0;
}

/***************************************************************
 *  ROPC login: POST grant_type=password to the token endpoint.
 ***************************************************************/
async function do_login(gobj, username, password)
{
    let auth = agent_config_get_auth(config_service(gobj));
    if(!auth.auth_url || !auth.realm || !auth.client_id) {
        gobj_send_event(gobj, "EV_LOGIN_DENIED",
            {error: "config", error_description: "Authentication is not configured"}, gobj);
        return;
    }

    let body = new URLSearchParams({
        grant_type: "password",
        client_id:  auth.client_id,
        username:   username,
        password:   password,
        scope:      "openid"
    });

    try {
        let resp = await fetch(token_url(auth), {
            method:  "POST",
            headers: {"Content-Type": "application/x-www-form-urlencoded"},
            body:    body
        });
        let data = await resp.json().catch(() => ({}));

        if(resp.ok && data.access_token) {
            store_tokens(gobj, data, username);
            gobj_send_event(gobj, "EV_LOGIN_ACCEPTED",
                {username: username, expires_in: data.expires_in}, gobj);
        } else {
            gobj_send_event(gobj, "EV_LOGIN_DENIED", {
                error:             data.error || ("http_" + resp.status),
                error_description: data.error_description || `HTTP ${resp.status}`
            }, gobj);
        }
    } catch(err) {
        gobj_send_event(gobj, "EV_LOGIN_DENIED",
            {error: "network", error_description: network_error_hint(err)}, gobj);
    }
}

/***************************************************************
 *  A fetch network failure (TypeError "Failed to fetch") from a
 *  cross-origin token endpoint is almost always CORS: the Keycloak
 *  client needs this app's origin in Web Origins (and Direct Access
 *  Grants enabled). Surface that hint instead of a bare message.
 ***************************************************************/
function network_error_hint(err)
{
    if(err instanceof TypeError) {
        return `${err.message} — likely CORS: enable Direct Access Grants and add this ` +
               `app origin to the Keycloak client's Web Origins`;
    }
    return err.message;
}

/***************************************************************
 *  Refresh the access token with the refresh_token grant.
 ***************************************************************/
async function do_refresh(gobj)
{
    let priv = gobj.priv;
    if(!priv.refresh_token) {
        return;
    }
    let auth = agent_config_get_auth(config_service(gobj));

    let body = new URLSearchParams({
        grant_type:    "refresh_token",
        client_id:     auth.client_id,
        refresh_token: priv.refresh_token
    });

    try {
        let resp = await fetch(token_url(auth), {
            method:  "POST",
            headers: {"Content-Type": "application/x-www-form-urlencoded"},
            body:    body
        });
        let data = await resp.json().catch(() => ({}));

        if(resp.ok && data.access_token) {
            store_tokens(gobj, data, priv.username);
            gobj_send_event(gobj, "EV_LOGIN_REFRESHED", {username: priv.username}, gobj);
        } else {
            gobj_send_event(gobj, "EV_LOGIN_DENIED", {
                error:             data.error || ("http_" + resp.status),
                error_description: data.error_description || `HTTP ${resp.status}`
            }, gobj);
        }
    } catch(err) {
        gobj_send_event(gobj, "EV_LOGIN_DENIED",
            {error: "network", error_description: network_error_hint(err)}, gobj);
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************
 *  EV_DO_LOGIN {username, password}
 ***************************************************************/
function ac_do_login(gobj, event, kw, src)
{
    do_login(gobj, kw.username || "", kw.password || "");
    return 0;
}

/***************************************************************
 *  EV_DO_LOGOUT — drop the session locally.
 ***************************************************************/
function ac_do_logout(gobj, event, kw, src)
{
    clear_tokens(gobj);
    gobj_publish_event(gobj, "EV_LOGOUT_DONE", {});
    return 0;
}

/***************************************************************
 *  EV_LOGIN_ACCEPTED — token stored; tell subscribers.
 ***************************************************************/
function ac_login_accepted(gobj, event, kw, src)
{
    log_info(`${gobj_name(gobj)}: login accepted for '${kw.username || ""}'`);
    gobj_publish_event(gobj, "EV_LOGIN_ACCEPTED",
        {username: kw.username || gobj.priv.username});
    return 0;
}

/***************************************************************
 *  EV_LOGIN_REFRESHED — silently refreshed.
 ***************************************************************/
function ac_login_refreshed(gobj, event, kw, src)
{
    gobj_publish_event(gobj, "EV_LOGIN_REFRESHED", {username: gobj.priv.username});
    return 0;
}

/***************************************************************
 *  EV_LOGIN_DENIED — login or refresh failed. Drop any tokens.
 ***************************************************************/
function ac_login_denied(gobj, event, kw, src)
{
    clear_tokens(gobj);
    gobj_publish_event(gobj, "EV_LOGIN_DENIED", {
        error:             kw.error || "",
        error_description: kw.error_description || ""
    });
    return 0;
}

/***************************************************************
 *  EV_TIMEOUT — time to refresh the access token.
 ***************************************************************/
function ac_timeout(gobj, event, kw, src)
{
    do_refresh(gobj);
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
            ["EV_DO_LOGIN",        ac_do_login,        null],
            ["EV_DO_LOGOUT",       ac_do_logout,       null],
            ["EV_LOGIN_ACCEPTED",  ac_login_accepted,  null],
            ["EV_LOGIN_REFRESHED", ac_login_refreshed, null],
            ["EV_LOGIN_DENIED",    ac_login_denied,    null],
            ["EV_TIMEOUT",         ac_timeout,         null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *  Output events tagged EVF_NO_WARN_SUBS: subscribers (auth view,
     *  console) are optional at any given moment.
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
    agent_login_get_token,
    agent_login_username,
    agent_login_is_logged_in,
};
