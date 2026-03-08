/***********************************************************************
 *          c_login.js
 *
 *          OAuth2 Authorization Code + PKCE login with BFF (Backend For
 *          Frontend) support.
 *
 *  Security model (SEC-06):
 *  - Tokens are NEVER stored in localStorage or sessionStorage.
 *  - The PKCE Authorization Code flow replaces the deprecated ROPC
 *    (Resource Owner Password Credentials) grant, so this JS code never
 *    handles user passwords.
 *  - The browser POSTs the authorization code + PKCE verifier to the BFF
 *    endpoint (/auth/callback).  The BFF exchanges it with Keycloak
 *    server-side and writes the tokens into httpOnly, Secure,
 *    SameSite=Strict cookies that JavaScript cannot read at all.
 *  - Token refresh and logout go through the BFF too (/auth/refresh,
 *    /auth/logout), so the JS side only ever sees {username, expires_in,
 *    refresh_expires_in} — no raw JWT.
 *  - The WebSocket HTTP-Upgrade request automatically carries the
 *    httpOnly cookies; the Yuneta backend reads them from the Cookie
 *    header and validates the JWT server-side.
 *  - Social logins (Google, GitHub, …) are supported via Keycloak
 *    Identity Providers: pass a `kc_idp_hint` in EV_DO_LOGIN.
 *
 *  Keycloak client configuration required:
 *  - Standard Flow (Authorization Code) ENABLED
 *  - Direct Access Grants (ROPC) DISABLED
 *  - Valid Redirect URIs: the app URL (e.g. https://treedb.yunetas.com/*)
 *  - Web Origins: the app origin (for CORS)
 *
 *  PKCE code_verifier lifecycle:
 *  - Generated fresh for every login attempt.
 *  - Stored in sessionStorage ONLY for the duration of the OAuth redirect
 *    round-trip (keyed by `state` nonce).
 *  - Deleted immediately after the callback is processed.
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

import {keycloak_configs, bff_urls} from "./conf/backend_config.js";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_LOGIN";

// sessionStorage key for PKCE state  (short-lived, only during redirect)
const PKCE_SESSION_KEY = "pkce_pending";

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",   0,      null,   "Subscriber of output events"),
SDATA(data_type_t.DTP_STRING,   "username",     0,      "",     "Authenticated username"),
SDATA(data_type_t.DTP_JSON,     "oauth_conf",   0,      "{}",   "OAuth / Keycloak configuration"),
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
                     *      PKCE helpers
                     ******************************/




/***************************************************************
 *  Generate a cryptographically random code_verifier (RFC 7636).
 *  Returns a base64url-encoded 32-byte random value (43 chars).
 ***************************************************************/
function generate_code_verifier()
{
    const array = new Uint8Array(32);
    window.crypto.getRandomValues(array);
    return base64url_encode(array);
}

/***************************************************************
 *  Compute SHA-256 of the verifier, return base64url string.
 ***************************************************************/
async function compute_code_challenge(verifier)
{
    const encoder = new TextEncoder();
    const data = encoder.encode(verifier);
    const digest = await window.crypto.subtle.digest("SHA-256", data);
    return base64url_encode(new Uint8Array(digest));
}

/***************************************************************
 *  base64url encode (no padding).
 ***************************************************************/
function base64url_encode(array)
{
    return btoa(String.fromCharCode(...array))
        .replace(/\+/g, "-")
        .replace(/\//g, "_")
        .replace(/=/g, "");
}

/***************************************************************
 *  Generate a random state nonce for CSRF protection.
 ***************************************************************/
function generate_state()
{
    const array = new Uint8Array(16);
    window.crypto.getRandomValues(array);
    return base64url_encode(array);
}




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

    // Resolve configuration for the current hostname
    let hostname = window.location.hostname || "localhost";

    let keycloak_config = keycloak_configs[hostname];
    if(keycloak_config) {
        gobj_write_attr(gobj, "oauth_conf", keycloak_config);
    } else {
        log_error(`${gobj_short_name(gobj)}: keycloak config not found: '${hostname}'`);
    }

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
     *  SEC-06: check if we are returning from an OAuth2 redirect.
     *
     *  Keycloak redirects back with ?code=<authz_code>&state=<nonce>.
     *  We handle the callback asynchronously and clean the URL before
     *  any script can read it again.
     */
    const url_params = new URLSearchParams(window.location.search);
    const code = url_params.get("code");
    const state = url_params.get("state");

    if(code && state) {
        // We are on the callback leg of the PKCE flow.
        gobj_change_state(gobj, "ST_WAIT_TOKEN");
        handle_oauth_callback(gobj, code, state);  // async — fires EV_LOGIN_ACCEPTED/DENIED
    } else {
        /*
         *  No OAuth callback — try to restore the session from httpOnly
         *  cookies (e.g. after F5 page refresh).  If the BFF refresh
         *  succeeds, fire EV_LOGIN_ACCEPTED and transition to ST_LOGIN.
         *  If it fails (no cookies, expired, network error), silently
         *  remain in ST_LOGOUT so the user sees the login button.
         */
        gobj_change_state(gobj, "ST_WAIT_TOKEN");
        try_restore_session(gobj);
    }
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
 *  Initiate the PKCE Authorization Code flow.
 *
 *  kc_idp_hint:  null  → Keycloak login form (local users)
 *                "google" / "github" / …  → direct IDP redirect
 *
 *  This function NAVIGATES AWAY from the current page.
 *  The app will be reloaded with ?code=…&state=… by Keycloak.
 ***************************************************************/
async function initiate_pkce_login(gobj, kc_idp_hint)
{
    /*
     *  SEC-06: require a secure context for WebCrypto.
     */
    if(!window.crypto || !window.crypto.subtle) {
        log_error(`${gobj_short_name(gobj)}: PKCE requires HTTPS (secure context)`);
        gobj_send_event(gobj, "EV_LOGIN_DENIED",
            { error: "PKCE requires a secure context (HTTPS)" }, gobj);
        return;
    }

    const oauth_conf = gobj_read_attr(gobj, "oauth_conf");
    if(!json_object_size(oauth_conf)) {
        log_error(`${gobj_short_name(gobj)}: no OAuth configuration`);
        gobj_send_event(gobj, "EV_LOGIN_DENIED", { error: "No OAuth configuration" }, gobj);
        return;
    }

    const code_verifier    = generate_code_verifier();
    const code_challenge   = await compute_code_challenge(code_verifier);
    const state            = generate_state();
    const redirect_uri     = window.location.origin + window.location.pathname;

    /*
     *  Store ONLY what is needed for the round-trip callback verification.
     *  This is in sessionStorage, not localStorage:
     *  - Scoped to this tab / session.
     *  - NOT accessible cross-origin.
     *  - Cleared when the tab is closed.
     *  - Deleted immediately after the callback is consumed (one-time use).
     */
    sessionStorage.setItem(PKCE_SESSION_KEY, JSON.stringify({
        code_verifier,
        state
    }));

    const oauth_endpoint    = oauth_conf["auth-server-url"];
    const realm             = oauth_conf["realm"];
    const client_id         = oauth_conf["resource"];
    const url = build_path(
        oauth_endpoint,
        "realms",
        realm,
        "protocol",
        "openid-connect",
        "auth"
    );

    let auth_url =
        `${url}` +
        `?response_type=code` +
        `&client_id=${encodeURIComponent(client_id)}` +
        `&redirect_uri=${encodeURIComponent(redirect_uri)}` +
        `&scope=openid%20profile%20email` +
        `&code_challenge=${code_challenge}` +
        `&code_challenge_method=S256` +
        `&state=${state}`;

    if(kc_idp_hint && !empty_string(kc_idp_hint)) {
        auth_url += `&kc_idp_hint=${encodeURIComponent(kc_idp_hint)}`;
    }

    // Navigate away — this page will be reloaded by Keycloak with ?code=
    window.location.replace(auth_url);
}

/***************************************************************
 *  Handle the OAuth2 callback after Keycloak redirects back.
 *
 *  - Verifies the state nonce (CSRF protection).
 *  - Cleans the authorization code from the URL immediately.
 *  - POSTs code + code_verifier to the BFF endpoint.
 *  - The BFF exchanges them server-side and sets httpOnly cookies.
 *  - Fires EV_LOGIN_ACCEPTED or EV_LOGIN_DENIED.
 ***************************************************************/
async function handle_oauth_callback(gobj, code, state)
{
    /*
     *  SEC-06: immediately remove the authorization code from the
     *  browser URL bar so it cannot be logged, cached, or read by
     *  browser extensions.
     */
    const clean_url = window.location.origin + window.location.pathname;
    window.history.replaceState({}, document.title, clean_url);

    /*
     *  SEC-06: CSRF check — state must match what we stored.
     */
    let pkce_pending_str = sessionStorage.getItem(PKCE_SESSION_KEY);
    sessionStorage.removeItem(PKCE_SESSION_KEY);  // one-time use

    if(!pkce_pending_str) {
        log_error(`${gobj_short_name(gobj)}: no PKCE pending state in sessionStorage`);
        gobj_send_event(gobj, "EV_LOGIN_DENIED",
            { error: "Login session expired or invalid" }, gobj);
        return;
    }

    let pkce_pending;
    try {
        pkce_pending = JSON.parse(pkce_pending_str);
    } catch(e) {
        log_error(`${gobj_short_name(gobj)}: corrupt PKCE pending entry`);
        gobj_send_event(gobj, "EV_LOGIN_DENIED",
            { error: "Login session corrupted" }, gobj);
        return;
    }

    if(pkce_pending.state !== state) {
        log_error(`${gobj_short_name(gobj)}: state mismatch — possible CSRF attempt`);
        gobj_send_event(gobj, "EV_LOGIN_DENIED",
            { error: "Login state mismatch" }, gobj);
        return;
    }

    /*
     *  POST to the BFF.  The BFF will:
     *  1. Exchange code + code_verifier with Keycloak server-side.
     *  2. Set access_token and refresh_token as httpOnly cookies.
     *  3. Return {success, username, email, expires_in, refresh_expires_in}.
     *
     *  credentials:"include" is required so the browser stores the
     *  Set-Cookie headers the BFF sends back.
     */
    const bff_url       = gobj_read_attr(gobj, "bff_url");
    const redirect_uri  = window.location.origin + window.location.pathname;

    try {
        const resp = await fetch(build_path(bff_url, "auth", "callback"), {
            method:         "POST",
            credentials:    "include",
            headers:        { "Content-Type": "application/json" },
            body:           JSON.stringify({
                code,
                code_verifier:  pkce_pending.code_verifier,
                redirect_uri
            })
        });

        const data = await resp.json();

        if(resp.ok && data.success) {
            gobj_write_attr(gobj, "username", data.username || data.email || "");
            gobj_send_event(gobj, "EV_LOGIN_ACCEPTED", data, gobj);
        } else {
            const error = data.error || `HTTP ${resp.status}`;
            log_info(sprintf("%s: BFF callback error: %s", gobj_short_name(gobj), error));
            gobj_send_event(gobj, "EV_LOGIN_DENIED", { error }, gobj);
        }

    } catch(err) {
        log_error(`${gobj_short_name(gobj)}: BFF callback fetch failed: ${err.message}`);
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
 *    go back to ST_LOGOUT so the user sees the login button.
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
 *  EV_DO_LOGIN — initiates the PKCE redirect.
 *  kw may contain:
 *    provider:   null | "google" | "github" | …  (kc_idp_hint)
 ***************************************************************/
function ac_do_login(gobj, event, kw, src)
{
    const provider = (kw && kw.provider) ? kw.provider : null;
    initiate_pkce_login(gobj, provider);  // async, navigates away
    return 0;
}

function ac_do_logout(gobj, event, kw, src)
{
    do_bff_logout(gobj);
    return 0;
}

/***************************************************************
 *  EV_LOGIN_ACCEPTED — BFF exchanged the code successfully.
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
