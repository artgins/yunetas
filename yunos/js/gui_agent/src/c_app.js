/***********************************************************************
 *          c_app.js
 *
 *      C_APP — application root (the default service), wattyzer's
 *      C_WZ_APP model:
 *        1. Owns the BFF login flow (C_AGENT_LOGIN) and the shared link
 *           to the control center (C_AGENT_LINK).
 *        2. Shows the pre-shell login screen when there is no session
 *           (and on logout / restore-failure).
 *        3. Builds the declarative shell (C_YUI_SHELL) LAZILY — only on
 *           the first EV_ON_OPEN, so views that issue backend commands in
 *           mt_start find the session already open. Tears it down on
 *           logout.
 *        4. Owns the app chrome the shell publishes: avatar initials,
 *           theme toggle, language toggle, logout.
 *
 *      The link is started ONLY after login (so it never NAK-loops
 *      against the control center without a cookie) and stopped on logout.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA, SDATA_END, data_type_t,
    gclass_create, log_error, log_info,
    gobj_read_attr, gobj_read_pointer_attr, gobj_write_attr, gobj_write_str_attr,
    gobj_create_service, gobj_create_pure_child,
    gobj_subscribe_event, gobj_send_event,
    gobj_find_service, gobj_yuno,
    gobj_start, gobj_start_tree, gobj_stop, gobj_stop_tree, gobj_destroy, gobj_is_running,
    refresh_language,
} from "@yuneta/gobj-js";

import i18next from "i18next";

import {
    yui_shell_set_avatar_provider,
    yui_shell_refresh_avatars,
    yui_shell_set_translator,
} from "@yuneta/gobj-ui/src/c_yui_shell.js";

import {switch_locale, current_locale} from "./locales/locales.js";
import {toggle_theme} from "./theme.js";
import {mount_login} from "./login.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_APP";


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",  0,  null,  "Subscriber of output events"),
SDATA(data_type_t.DTP_JSON,     "config",      0,  null,  "Shell config (app_config.json)"),
SDATA(data_type_t.DTP_BOOLEAN,  "use_hash",    0,  true,  "Pass-through to C_YUI_SHELL"),
SDATA(data_type_t.DTP_STRING,   "username",    0,  "",    "Authenticated username"),
SDATA_END()
];

let PRIVATE_DATA = {
    shell:    null,
    login_ui: null,
    link:     null,
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
    /*  Config service (child of self).  */
    gobj_create_service("agent_config", "C_AGENT_CONFIG", {}, gobj);

    /*  Login service (child of self) — subscribe to all its output
     *  events (EV_LOGIN_ACCEPTED/DENIED/REFRESHED/LOGOUT_DONE) with the
     *  subscriber attr. mt_start runs its session-restore. */
    gobj_create_service("agent_login", "C_AGENT_LOGIN", {subscriber: gobj}, gobj);

    /*  Shared control-center link — child of the YUNO (NOT of self), so
     *  gobj_start_tree(self) does NOT start it. Started only after login
     *  so it never retries against the CC without a cookie. */
    let link = gobj_create_service("agent_link", "C_AGENT_LINK", {subscriber: gobj}, gobj_yuno());
    gobj.priv.link = link;
    gobj_subscribe_event(link, "EV_ON_OPEN", {}, gobj);
    gobj_subscribe_event(link, "EV_ON_ID_NAK", {}, gobj);
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    /*  Start our subtree → C_AGENT_LOGIN.mt_start runs try_restore_session,
     *  which answers EV_LOGIN_ACCEPTED (valid cookie) or EV_RESTORE_FAILED.
     *  The link (child of yuno) is started later, on login. */
    gobj_start_tree(gobj);
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
}




                    /***************************
                     *      Local Methods
                     ***************************/




function compute_initials(gobj)
{
    /*  Read THIS gobj's username attr — set from a fresh /auth/login
     *  (kw.username) and, crucially after F5, from the control-center
     *  identity-ack in ac_on_open (the BFF /auth/refresh used to restore
     *  the session does not return a username). */
    let name = gobj_read_attr(gobj, "username") || "";
    if(!name) {
        return "";
    }
    let local = String(name).split("@")[0];
    let parts = local.split(/[._\-\s]+/).filter(Boolean);
    let ini = parts.slice(0, 2).map(p => p[0]).join("");
    return (ini || local.slice(0, 2)).toUpperCase();
}

/***************************************************************
 *  Show / hide the pre-shell login screen.
 ***************************************************************/
function show_login_screen(gobj)
{
    let priv = gobj.priv;
    if(priv.login_ui) {
        return;
    }
    priv.login_ui = mount_login({
        on_submit: function(creds) {
            let login = gobj_find_service("agent_login", false);
            if(login) {
                gobj_send_event(login, "EV_DO_LOGIN", creds, gobj);
            }
        }
    });
}

function hide_login_screen(gobj)
{
    let priv = gobj.priv;
    if(priv.login_ui) {
        priv.login_ui.unmount();
        priv.login_ui = null;
    }
}

/***************************************************************
 *  Build the declarative shell (once, on the first session-open).
 ***************************************************************/
function build_shell(gobj)
{
    let priv = gobj.priv;
    if(priv.shell) {
        return priv.shell;
    }

    let shell = gobj_create_pure_child("shell", "C_YUI_SHELL", {
        config:     gobj_read_attr(gobj, "config"),
        use_hash:   gobj_read_attr(gobj, "use_hash"),
        subscriber: gobj
    }, gobj);
    priv.shell = shell;
    gobj_start_tree(shell);

    yui_shell_set_avatar_provider(shell, () => compute_initials(gobj));
    yui_shell_set_translator(shell, i18next.t.bind(i18next));
    return shell;
}

function destroy_shell(gobj)
{
    let priv = gobj.priv;
    if(priv.shell) {
        if(gobj_is_running(priv.shell)) {
            gobj_stop_tree(priv.shell);
        }
        gobj_destroy(priv.shell);
        priv.shell = null;
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************
 *  Login accepted (fresh or restored). Connect the link; the shell
 *  is built on the first EV_ON_OPEN.
 ***************************************************************/
function ac_login_accepted(gobj, event, kw, src)
{
    if(kw && kw.username) {
        gobj_write_str_attr(gobj, "username", kw.username);
    }
    hide_login_screen(gobj);
    let link = gobj.priv.link;
    if(link && !gobj_is_running(link)) {
        gobj_start(link);
    }
    return 0;
}

/***************************************************************
 *  Control-center session open → build the shell.
 *
 *  The CC identity-ack carries the authenticated username — this is
 *  the authoritative source (the BFF /auth/refresh used on F5 does
 *  NOT return a username, unlike /auth/login). Adopt it before
 *  building the shell so the avatar initials are correct, and repaint
 *  if the shell already existed.
 ***************************************************************/
function ac_on_open(gobj, event, kw, src)
{
    if(kw && kw.username) {
        gobj_write_str_attr(gobj, "username", kw.username);
    }
    let shell = build_shell(gobj);
    yui_shell_refresh_avatars(shell);
    return 0;
}

/***************************************************************
 *  Login refused / refresh failed. If a shell is up, the session
 *  expired mid-use → tear down + back to login; else paint the error.
 ***************************************************************/
function ac_login_denied(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let msg = (kw && (kw.error || kw.error_code)) || i18next.t("login failed");

    if(priv.shell) {
        destroy_shell(gobj);
        if(priv.link && gobj_is_running(priv.link)) {
            gobj_stop(priv.link);
        }
    }
    show_login_screen(gobj);
    if(priv.login_ui) {
        priv.login_ui.set_busy(false);
        priv.login_ui.set_error(`${i18next.t("login failed")}: ${msg}`);
    }
    return 0;
}

/***************************************************************
 *  The control center rejected the (cookie) identity → session
 *  invalid; behave like a denied login.
 ***************************************************************/
function ac_on_id_nak(gobj, event, kw, src)
{
    return ac_login_denied(gobj, event,
        {error: (kw && kw.comment) || i18next.t("authentication required")}, src);
}

/***************************************************************
 *  No valid session on load — show the login form (no error).
 ***************************************************************/
function ac_restore_failed(gobj, event, kw, src)
{
    show_login_screen(gobj);
    return 0;
}

/***************************************************************
 *  Silent refresh ok — no UI change.
 ***************************************************************/
function ac_login_refreshed(gobj, event, kw, src)
{
    return 0;
}

/***************************************************************
 *  Logout requested (from the shell user menu) → ask the BFF.
 ***************************************************************/
function ac_logout(gobj, event, kw, src)
{
    let login = gobj_find_service("agent_login", false);
    if(login) {
        gobj_send_event(login, "EV_DO_LOGOUT", {}, gobj);
    }
    return 0;
}

/***************************************************************
 *  Logout done — tear down shell + link, back to login.
 ***************************************************************/
function ac_logout_done(gobj, event, kw, src)
{
    gobj_write_str_attr(gobj, "username", "");
    destroy_shell(gobj);
    if(gobj.priv.link && gobj_is_running(gobj.priv.link)) {
        gobj_stop(gobj.priv.link);
    }
    show_login_screen(gobj);
    return 0;
}

/***************************************************************
 *  Shell chrome — theme / language toggles.
 ***************************************************************/
function ac_toggle_theme(gobj, event, kw, src)
{
    toggle_theme();
    return 0;
}

function ac_toggle_language(gobj, event, kw, src)
{
    switch_locale(current_locale() === "es" ? "en" : "es");
    refresh_language(document.body, i18next.t.bind(i18next));
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
            ["EV_LOGIN_ACCEPTED",   ac_login_accepted,  null],
            ["EV_LOGIN_REFRESHED",  ac_login_refreshed, null],
            ["EV_LOGIN_DENIED",     ac_login_denied,    null],
            ["EV_RESTORE_FAILED",   ac_restore_failed,  null],
            ["EV_LOGOUT_DONE",      ac_logout_done,     null],
            ["EV_ON_OPEN",          ac_on_open,         null],
            ["EV_ON_ID_NAK",        ac_on_id_nak,       null],
            /*  shell chrome  */
            ["EV_LOGOUT",           ac_logout,          null],
            ["EV_TOGGLE_THEME",     ac_toggle_theme,    null],
            ["EV_TOGGLE_LANGUAGE",  ac_toggle_language, null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_LOGIN_ACCEPTED",   0],
        ["EV_LOGIN_REFRESHED",  0],
        ["EV_LOGIN_DENIED",     0],
        ["EV_RESTORE_FAILED",   0],
        ["EV_LOGOUT_DONE",      0],
        ["EV_ON_OPEN",          0],
        ["EV_ON_ID_NAK",        0],
        ["EV_LOGOUT",           0],
        ["EV_TOGGLE_THEME",     0],
        ["EV_TOGGLE_LANGUAGE",  0]
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
function register_c_app()
{
    return create_gclass(GCLASS_NAME);
}

export {register_c_app};
