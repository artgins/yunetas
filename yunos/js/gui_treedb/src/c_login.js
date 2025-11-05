/***********************************************************************
 *          login.js
 *
 *          Manage user login/logout with keycloak
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

import {
    SDATA,
    SDATA_END,
    kw_flag_t,
    data_type_t,
    gclass_create,
    event_flag_t,
    log_error,
    jwt2json,
    gobj_subscribe_event,
    gobj_send_event,
    gobj_read_attr,
    gobj_write_attr,
    kw_get_str,
    kw_get_bool,
    json_object_size,
    log_info,
    get_now,
    is_string,
    kw_get_local_storage_value,
    kw_set_local_storage_value,
    kw_remove_local_storage_value,
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
} from "yunetas";

import {keycloak_configs} from "./conf/backend_config.js";

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
SDATA(data_type_t.DTP_POINTER,  "subscriber",           0,  null,   "Subscriber of output events"),
SDATA(data_type_t.DTP_STRING,   "username",             0,  "",     "Username to login"),
SDATA(data_type_t.DTP_JSON,     "oauth_conf",           0,  "{}",   "OAuth configuration"),
SDATA(data_type_t.DTP_STRING,   "access_token",         0,  "",     "JWT access token"),
SDATA(data_type_t.DTP_STRING,   "refresh_token",        0,  "",     "JWT refresh token"),
SDATA(data_type_t.DTP_JSON,     "full_oauth_response",  0,  "{}",   "Full OAuth response"),
SDATA_END()
];

let PRIVATE_DATA = {
    timeout_refresh:    0,
    tokenParsed:        null,
    refreshParsed:      null,
    gobj_timer:         null,
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

    /*
     *  Create children
     */
    priv.gobj_timer = gobj_create_pure_child(gobj_name(gobj), "C_TIMER", {}, gobj);

    /*
     *  SERVICE subscription model
     */
    const subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, null, {}, subscriber);
    }

    // HACK punto gatillo: get the keycloak file associated to the url of location.hostname
    let hostname = window.location.hostname || "localhost";

    let keycloak_config = keycloak_configs[hostname];
    if(keycloak_config) {
        gobj_write_attr(gobj, "oauth_conf", keycloak_config);
    } else {
        log_error(`${gobj_short_name(gobj)}: keycloack config not found: '${hostname}'`);
    }
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    let priv = gobj.priv;

    gobj_start(priv.gobj_timer);

    let session = kw_get_local_storage_value("session", null, false);
    if (session && session.username) {
        gobj_write_attr(gobj, "username", session.username);
        gobj_change_state(gobj, "ST_WAIT_TOKEN");
        gobj_send_event(gobj, "EV_LOGIN_ACCEPTED", session.full_oauth_response, gobj);
    } else {
        kw_remove_local_storage_value("session");
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

    //do_logout(this);
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




/********************************************
 *  {
 *      username: str
 *      password: str
 *      offline_access: bool
 *  }
 ********************************************/
function do_login(gobj, kw)
{
    let username = kw_get_str(gobj, kw, "username", "", kw_flag_t.KW_REQUIRED);
    let password = kw_get_str(gobj, kw, "password", "", kw_flag_t.KW_REQUIRED);
    let offline_access = kw_get_bool(gobj, kw, "offline_access", false, 0);

    if(empty_string(username)) {
        log_error(sprintf("%s: No username", gobj_short_name(gobj)));
        gobj_send_event(gobj,
            "EV_LOGIN_DENIED",
            {
                error: "No username"
            },
            gobj
        );
        return;
    }

    if(empty_string(password)) {
        log_error(sprintf("%s: No password", gobj_short_name(gobj)));
        gobj_send_event(gobj,
            "EV_LOGIN_DENIED",
            {
                error: "No password"
            },
            gobj
        );
        return;
    }

    const oauth_conf = gobj_read_attr(gobj, "oauth_conf");
    if(!json_object_size(oauth_conf)) {
        log_error("NO OAuth Configuration");
        return;
    }

    let oauth_endpoint = oauth_conf["auth-server-url"]; // Ex: "https://localhost:9999/auth/"
    if(empty_string(oauth_endpoint)) {
        log_error(sprintf("%s: NO auth-server-url", gobj_short_name(gobj)));
        gobj_send_event(gobj,
            "EV_LOGIN_DENIED",
            {
                error: "No auth server url"
            },
            gobj
        );
        return;
    }

    /*
     *  Save username
     */
    gobj_write_attr(gobj, "username", kw.username);

    /*
     *  Call OAuth endpoint
     */
    let owner = oauth_conf["realm"];
    let service = oauth_conf["resource"];
    let url = sprintf("%srealms/%s/protocol/openid-connect/token", oauth_endpoint, owner);

    let xhr = new XMLHttpRequest();
    xhr.open("POST", url);
    xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");

    let data = "";
    let form_data = {
        "username": username,
        "password": password,
        "grant_type": "password",
        "client_id": service
    };
    if(offline_access || true) {
        // No uso offline de momento, sigue rechazando el jwt despues del refresh time
        //form_data["scope"] = "openid offline_access";
    }

    /*
     *  Convert json to www-form-urlencoded
     */
    for(let k of Object.keys(form_data)) {
        let v = form_data[k];
        if(empty_string(data)) {
            data += sprintf("%s=%s", k, v);
        } else {
            data += sprintf("&%s=%s", k, v);
        }
    }

    xhr.onreadystatechange = function () {
        if(xhr.readyState === 4) {
            let status = xhr.status;
            /*
             *  WARNING cuando hay un error CORS con keyckoak, el navegador entrega status 0,
             *  aunque el status sea 401 por ejemplo, y además no entrega tampoco
             *  response.description
             */
            //1
            let response = xhr.responseText;
            if(status === 200) {
                let r = JSON.parse(response);
                gobj_send_event(gobj, "EV_LOGIN_ACCEPTED", r, gobj);
            } else {
                let error = "login denied";
                log_info(sprintf("login error, status != 200, status %d, response '%s'", status, response));
                try {
                    if(is_string(response) && !empty_string(response)) {
                        error = JSON.parse(response).error_description;
                    }
                } catch(e) {
                }
                gobj_send_event(gobj,
                    "EV_LOGIN_DENIED",
                    {
                        error: error
                    },
                    gobj
                );
            }
        }
    };
    xhr.send(data);
}

/********************************************
 *
 ********************************************/
function do_logout(gobj)
{
    const oauth_conf = gobj_read_attr(gobj, "oauth_conf");
    if(!json_object_size(oauth_conf)) {
        log_error("NO OAuth Configuration");
        return;
    }

    /*
     *  Call OAuth endpoint
     */
    let oauth_endpoint = oauth_conf["auth-server-url"]; // Ex: "https://localhost:9999/auth/"
    let owner = oauth_conf["realm"];
    let service = oauth_conf["resource"];
    let url = sprintf("%srealms/%s/protocol/openid-connect/logout", oauth_endpoint, owner);

    let xhr = new XMLHttpRequest();
    xhr.open("POST", url);
    xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
    xhr.setRequestHeader("Authorization", "Bearer " + gobj_read_attr(gobj, "access_token"));

    let data = "";
    let form_data = {
        "refresh_token": gobj_read_attr(gobj, "refresh_token"),
        "client_id": service
    };

    /*
     *  Convert json to www-form-urlencoded
     */
    for (let k of Object.keys(form_data)) {
        let v = form_data[k];
        if(empty_string(data)) {
            data += sprintf("%s=%s", k, v);
        } else {
            data += sprintf("&%s=%s", k, v);
        }
    }

    xhr.onreadystatechange = function () {
        if (xhr.readyState === 4) {
            let status = xhr.status;
            let response = xhr.responseText;
            let error = null;
            if(status !== 204) {
                log_info(sprintf("logout: response != 204, status %d, data %s", status, response));
                try {
                    if(is_string(response)) {
                        error = JSON.parse(response);
                    }
                } catch(e) {
                    error = response;
                }
            }
            // do logout in any case
            gobj_send_event(gobj,
                "EV_LOGOUT_DONE",
                {
                    error: error
                },
                gobj
            );
        }
    };
    xhr.send(data);

    /*
     *  Clear username
     */
    gobj_write_attr(gobj, "username", "");
    kw_remove_local_storage_value("session");
}

/********************************************
 *
 ********************************************/
function do_refresh(gobj)
{
    const oauth_conf = gobj_read_attr(gobj, "oauth_conf");
    let oauth_endpoint = oauth_conf["auth-server-url"]; // Ex: "https://localhost:9999/auth/"
    let owner = oauth_conf["realm"];
    let service = oauth_conf["resource"];
    let url = sprintf("%srealms/%s/protocol/openid-connect/token", oauth_endpoint, owner);

    let xhr = new XMLHttpRequest();
    xhr.open("POST", url);
    xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
    xhr.setRequestHeader("Authorization", "Bearer " + gobj_read_attr(gobj, "access_token"));

    let data = "";
    let form_data = {
        "grant_type": "refresh_token",
        "refresh_token": gobj_read_attr(gobj, "refresh_token"),
        "client_id": service
    };
    // if(offline_access) { // Funciona?, es necesario?
    //     form_data["scope"] = "openid offline_access"
    // }

    /*
     *  Convert json to www-form-urlencoded
     */
    for (let k of Object.keys(form_data)) {
        let v = form_data[k];
        if(empty_string(data)) {
            data += sprintf("%s=%s", k, v);
        } else {
            data += sprintf("&%s=%s", k, v);
        }
    }

    xhr.onreadystatechange = function () {
        if (xhr.readyState === 4) {
            let status = xhr.status;
            let response = xhr.responseText;
            if(status === 200) {
                let r = JSON.parse(response);
                gobj_send_event(gobj, "EV_LOGIN_REFRESHED", r, gobj);
            } else {
                /*
                 *  WARNING: if there is a CORS error, the browser return status 0
                 *  although the status were 200, and don't pass the {} response
                 */
                let error = "login denied";
                // log_error(sprintf("status != 200, status %d, data %s", status, response)); // TODO TEST

                try {
                    if(is_string(response)) {
                        error = JSON.parse(response).error_description;
                    }
                } catch(e) {
                }

                gobj_send_event(gobj,
                    "EV_LOGIN_DENIED",
                    {
                        error: error
                    },
                    gobj
                );
            }
        }
    };
    xhr.send(data);
}




                    /***************************
                     *      Actions
                     ***************************/




/********************************************
 *  {
 *      username:
 *      password:
 *  }
 ********************************************/
function ac_do_login(gobj, event, kw, src)
{
    do_login(gobj, kw);
    return 0;
}

/********************************************
 *
 ********************************************/
function ac_do_logout(gobj, event, kw, src)
{
    do_logout(gobj);
    return 0;
}

/********************************************
 *  kw: {
        access_token: ""
        expires_in: 36000                   // 10 hours
        "not-before-policy": 1662894464
        refresh_expires_in: 1800            // 30 minutes
        refresh_token: ""
        scope: "profile email"
        session_state: "d50cebba-c29b-48fc-a87f-0bfb1a09d7f4"
        token_type: "Bearer"
 *  }

Ejemplo keycloak:  {
        "acr": "1",
        "allowed-origins": [],
        "aud": ["realm-management", "account"],
        "azp": "yunetacontrol",
        "email": "ginsmar@artgins.com",
        "email_verified": true,
        "exp": 1666336696,
        "family_name": "Martínez",
        "given_name": "Ginés",
        "iat": 1666336576,
        "iss": "https://localhost:8641/auth/realms/xxxx",
        "jti": "96b60323-05c1-4cb1-87e8-8bd68e25a56c",
        "locale": "en",
        "name": "Ginés Martínez",
        "preferred_username": "ginsmar@artgins.com",
        "realm_access": {},
        "resource_access": {},
        "scope": "profile email",
        "session_state": "aa4fb7ce-d0c7-48a0-ae92-253ef5a600d2",
        "sid": "aa4fb7ce-d0c7-48a0-ae92-253ef5a600d2",
        "sub": "0a1e5c27-80f1-4225-943a-edfbc204972d",
        "typ": "Bearer"
    }
Ejemplo de jwt dado por google  {
        "aud": "990339570472-k6nqn1tpmitg8pui82bfaun3jrpmiuhs.apps.googleusercontent.com",
        "azp": "990339570472-k6nqn1tpmitg8pui82bfaun3jrpmiuhs.apps.googleusercontent.com",
        "email": "ginsmar@gmail.com",
        "email_verified": true,
        "exp": 1666341427,
        "given_name": "Gins",
        "iat": 1666337827,
        "iss": "https://accounts.google.com",
        "jti": "b2a78ed2911514e30e51fb7b0da3c2032ba3a0aa",
        "name": "Gins",
        "nbf": 1666337527,
        "picture": "https://lh3.googleusercontent.com/a/ALm5wu0soemzAFPT0aSqz_-PyPBX_y9RXuSpRcwStQLRBg=s96-c",
        "sub": "109408784262322618770"
    }

 ********************************************/
function ac_login_accepted(gobj, event, kw, src)
{
    let priv = gobj.priv;

    gobj_write_attr(gobj, "full_oauth_response", kw);
    gobj_write_attr(gobj, "access_token", kw["access_token"]);
    gobj_write_attr(gobj, "refresh_token", kw["refresh_token"]);

    /*
     *  Save the session
     */
    kw_set_local_storage_value(
        "session",
        {
            username: gobj_read_attr(gobj, "username"),
            full_oauth_response: kw
        }
    );

    priv.tokenParsed = jwt2json(gobj_read_attr(gobj, "access_token"));
    priv.refreshParsed = jwt2json(gobj_read_attr(gobj, "refresh_token"));

    priv.timeout_refresh = priv.refreshParsed.exp - get_now() - 5;
    if(priv.timeout_refresh <= 0) {
        priv.timeout_refresh = 2; // Validity will be checked in refresh time
    }
    set_timeout(priv.gobj_timer, priv.timeout_refresh*1000);

    // log_warning("====> timeout refresh (seconds):" + priv.timeout_refresh); // TODO TEST

    gobj_publish_event(gobj, "EV_LOGIN_ACCEPTED", {
        username: gobj_read_attr(gobj, "username"),
        jwt: gobj_read_attr(gobj, "access_token")
    });
    return 0;
}

/********************************************
 *  kw: {
        access_token: "",
        expires_in: 65,
        refresh_expires_in: 60,
        refresh_token: "",
        token_type: "Bearer",
        "not-before-policy": 1662909765,
        session_state: "144c039d-6ffb-48f0-97ec-49535d596e5b",
        scope: "profile email"
    }
 *
 ********************************************/
function ac_login_refreshed(gobj, event, kw, src)
{
    let priv = gobj.priv;

    gobj_write_attr(gobj, "full_oauth_response", kw);
    gobj_write_attr(gobj, "access_token", kw["access_token"]);
    gobj_write_attr(gobj, "refresh_token", kw["refresh_token"]);

    /*
     *  Save the session
     */
    kw_set_local_storage_value(
        "session",
        {
            username: gobj_read_attr(gobj, "username"),
            full_oauth_response: kw
        }
    );

    priv.tokenParsed = jwt2json(gobj_read_attr(gobj, "access_token"));
    priv.refreshParsed = jwt2json(gobj_read_attr(gobj, "refresh_token"));

// console.dir("kw", kw); // TODO TEST
// console.dir("_tokenParsed", priv.tokenParsed); // TODO TEST
// console.dir("_refreshParsed", priv.refreshParsed); // TODO TEST
// log_warning(JSON.stringify(priv.tokenParsed));
// log_warning(JSON.stringify(priv.refreshParsed));

    priv.timeout_refresh = priv.refreshParsed.exp - get_now() - 5;
    if(priv.timeout_refresh <= 0) {
        priv.timeout_refresh = 2; // Validity will be checked in refresh time
    }
    set_timeout(priv.gobj_timer, priv.timeout_refresh*1000);

    // log_warning("====> timeout refresh (seconds):" + priv.timeout_refresh); // TODO TEST

    gobj_publish_event(gobj, "EV_LOGIN_REFRESHED", {
        username: gobj_read_attr(gobj, "username"),
        jwt: gobj_read_attr(gobj, "access_token")
    });
}

/********************************************
 *
 ********************************************/
function ac_login_denied(gobj, event, kw, src)
{
    gobj_write_attr(gobj, "full_oauth_response", null);
    gobj_write_attr(gobj, "access_token", null);
    gobj_write_attr(gobj, "refresh_token", null);

    kw_remove_local_storage_value("session");
    gobj_publish_event(gobj, "EV_LOGIN_DENIED", kw);
    return 0;
}

/********************************************
 *
 ********************************************/
function ac_logout_done(gobj, event, kw, src)
{
    gobj_write_attr(gobj, "full_oauth_response", null);
    gobj_write_attr(gobj, "access_token", null);
    gobj_write_attr(gobj, "refresh_token", null);

    kw_remove_local_storage_value("session");
    gobj_publish_event(gobj, "EV_LOGOUT_DONE", kw);
    return 0;
}

/********************************************
 *
 ********************************************/
function ac_clear_session(gobj, event, kw, src)
{
    gobj_write_attr(gobj, "full_oauth_response", null);
    gobj_write_attr(gobj, "access_token", null);
    gobj_write_attr(gobj, "refresh_token", null);
    kw_remove_local_storage_value("session");
    return 0;
}

/********************************************
 *
 ********************************************/
function ac_timeout(gobj, event, kw, src)
{
    let priv = gobj.priv;

    // log_warning("<==== timeout refresh"); // TODO TEST

    if(priv.timeout_refresh > 0) {
        let now = get_now();
        if(now >= priv.timeout_refresh) {
            // Refresh time has elapsed
            do_refresh(gobj);
        }
    }

    return 0;
}




                    /***************************
                     *          FSM
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
    if (__gclass__) {
        log_error(`GClass ALREADY created: ${gclass_name}`);
        return -1;
    }

    const states = [
        ["ST_LOGOUT", [
            ["EV_DO_LOGIN",       ac_do_login,        "ST_WAIT_TOKEN"],
            ["EV_LOGIN_DENIED",   ac_login_denied,    null],
            ["EV_DO_LOGOUT",      ac_clear_session,   null]
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
        0,                // lmt
        attrs_table,
        PRIVATE_DATA,
        0,  // authz_table
        0,  // command_table
        0,  // s_user_trace_level
        0   // gclass_flag
    );

    return __gclass__ ? 0 : -1;
}

/***************************************************************
 *          Register Login GClass
 ***************************************************************/
function register_c_login()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_login };
