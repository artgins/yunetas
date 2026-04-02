/***********************************************************************
 *          yuneta_gui.js
 *
 *          Yuneta GUI (Yunetas V7)
 *
 *          This is the main gobj (__default_service__): create all other services
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

import {
    gobj_yuno,
    SDATA,
    SDATA_END,
    data_type_t,
    gclass_create,
    event_flag_t,
    log_error,
    gobj_subscribe_event,
    empty_string,
    kw_get_local_storage_value,
    gobj_start_tree,
    gobj_stop_tree,
    gobj_stop,
    gobj_destroy,
    gobj_services,
    gobj_name,
    gobj_find_service,
    gobj_write_str_attr,
    gobj_read_str_attr,
    gobj_send_event,
    gobj_create_service,
    gobj_write_attr,
    gobj_write_bool_attr,
    gobj_start,
    gobj_publish_event,
    gobj_read_attr,
    json_size,
    strs_in_list,
    gobj_short_name,
    gobj_is_running,
    gobj_create_pure_child,
    refresh_language,
    set_remote_log_functions,
    escapeHtml,
} from "@yuneta/gobj-js";

import {backend_urls} from "./conf/backend_config.js";

import {setup_dev, display_error_message} from "@yuneta/lib-yui";

import {setup_locale} from "./locales/locales.js";
import {flags_of_world} from "./locales/flags.js";
import {logo_wide_svg} from "./logos_svg.js";
import {t} from "i18next";

// import "yuneta-icon-font/dist/yuneta-icon-font.js"; // TODO parece que no se usa
// import "yuneta-icon-font/dist/yuneta-icon-font.css";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUNETA_GUI";

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
const attrs_table = [
SDATA (data_type_t.DTP_STRING,  "username",             0,  "",     "username logged"),
SDATA (data_type_t.DTP_STRING,  "remote_yuno_role",     0,  "",     "remote yuno role"),
SDATA (data_type_t.DTP_STRING,  "remote_yuno_name",     0,  "",     "remote yuno name"),
SDATA (data_type_t.DTP_STRING,  "remote_yuno_service",  0,  "",     "remote yuno service"),
SDATA (data_type_t.DTP_LIST,    "required_services",    0,  [],     "required services"),
SDATA (data_type_t.DTP_STRING,  "home",                 0,  "",     "Home of the app in the browser"),
SDATA (data_type_t.DTP_STRING,  "url",                  0,  "",     "remote url to connect"),
SDATA_END()
];

let PRIVATE_DATA = {
    user_gobjs: [],
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
    setup_locale();
    let v = Number(kw_get_local_storage_value("open_developer_window", 0, false));
    setup_dev(gobj, v);
    if(build_remote_service(gobj)) {
        build_ui(gobj);
        window.dispatchEvent(new Event("resize"));
    }
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    gobj_start_tree(gobj);
    return 0;
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    gobj_stop_tree(gobj);
    return 0;
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




/************************************************************
 *  Parameter of set_remote_log_functions()
 *  Set in ac_on_open and clear in ac_on_close
 ************************************************************/
function console_log_remote(msg)
{
    gobj_send_event(gobj_find_service("__remote_service__"), "EV_REMOTE_LOG", {msg: msg}, gobj_yuno());
}

/********************************************
 *
 ********************************************/
function build_remote_service(gobj)
{
    /*
     *  HACK "punto gatillo" trigger point: from the backend_urls.js file,
     *  retrieve the ws/wss connection associated with the url location.hostname.
     *
     *  Use an exact key lookup instead of indexOf() substring matching.
     *  indexOf("localhost") would match "localhost.attacker.com", which would
     *  redirect the WebSocket connection to wss://localhost:1800 on the victim's
     *  machine instead of the intended backend.
     *  The empty-hostname case (file:// or synthetic environments) falls back to
     *  "localhost" by explicit key, keeping the same behaviour as before.
     */
    const hostname = empty_string(window.location.hostname) ? "localhost" : window.location.hostname;
    const url = backend_urls[hostname];
    if (empty_string(url)) {
        let msg = t("There is no registered URL for this remote service.: " + hostname);
        log_error(msg);
        display_error_message(
            "Error",
            msg,
            function () {
                close_all(gobj);
            },
            true // leave the message forever
        );
        return false;
    }

    gobj_write_str_attr(gobj, "url", url);

    /*------------------------------------*
     *      Realtime service
     *------------------------------------*/
    let remote_service = gobj_create_service(
        "__remote_service__",
        "C_IEVENT_CLI",
        {
            remote_yuno_role: gobj_read_str_attr(gobj, "remote_yuno_role"),
            remote_yuno_service: gobj_read_str_attr(gobj, "remote_yuno_service"),
            jwt: null,
            url: url
        },
        gobj_yuno() // remote_service is child of yuno: avoid to start it with gobj_start_tree()
    );
    /*
     *  Subscribe to IEvent null, to receive all events of IEvent
     *      EV_ON_OPEN
     *      EV_ON_CLOSE
     *      EV_ON_ID_NAK
     */
    gobj_subscribe_event(
        remote_service,
        null,
        {},
        gobj
    );

    return true;
}

/********************************************
 *
 ********************************************/
function do_connect(gobj, jwt)
{
    let remote_service = gobj_find_service("__remote_service__");
    gobj_write_attr(remote_service, "jwt", jwt);

    /*
     *  Start
     */
    gobj_start_tree(remote_service);
}

/********************************************
 *
 ********************************************/
function close_all(gobj)
{
    let remote_service = gobj_find_service("__remote_service__");
    if (remote_service) {
        gobj_stop_tree(remote_service);
    }
    let login = gobj_find_service("__login__");
    if (login) {
        gobj_send_event(login, "EV_DO_LOGOUT", {}, gobj);
    }
}

/********************************************
 *
 ********************************************/
function close_services(gobj)
{
    let priv = gobj.priv;

    while(priv.user_gobjs.length > 0) {
        let gobj_ = priv.user_gobjs.pop(); // Pop the last item from the array
        if(gobj_is_running(gobj_)) {
            gobj_stop(gobj_);
        }
        gobj_destroy(gobj_);
    }
}

/********************************************
 *
 ********************************************/
function build_ui(gobj)
{
    gobj_create_service(
        "__login__",
        "C_LOGIN",
        {
            subscriber: gobj
        },
        gobj
    );

    gobj_create_service(
        "__yui_main__",
        "C_YUI_MAIN",
        {
            logo_wide_svg: logo_wide_svg,
            flags_of_world: flags_of_world,
        },
        gobj
    );

    gobj_create_service(
        "__yui_routing__",
        "C_YUI_ROUTING",
        {
            // "content-layer" is built by ui_main
            $parent: document.getElementById("content-layer"),
        },
        gobj
    );

    /*
     *  HACK:
     *  Subscribe to ui_main all from login and gobj (default_service)
     */
    let login = gobj_find_service("__login__");
    let yui_main = gobj_find_service("__yui_main__");
    if(login) {
        gobj_subscribe_event(login, null, {}, yui_main);
        gobj_subscribe_event(gobj, null, {}, yui_main);
    }
}

/********************************************
 *
 ********************************************/
function build_app(gobj, services_roles)
{
    let priv = gobj.priv;
    let remote_service = gobj_find_service("__remote_service__");

    let main_remote_service = gobj_read_str_attr(gobj, "remote_yuno_service");

    if(!json_size(services_roles[main_remote_service])) {
        return null; // No permission
    }

    let menu = [];

    /*-----------------------------------*
     *      Settings
     *-----------------------------------*/
    let main_roles = services_roles[main_remote_service];
    if(!main_roles || !strs_in_list(main_roles, ["root","owner"], true)) {
        // Don't show settings, only for admin
        menu.push(
            {
                // If it doesn't have an ID, then it's a menu title.
                label: `version ${gobj_read_str_attr(gobj_yuno(), "yuno_version")}`
            }
        );
        return menu;
    }

    menu.push(
        {
            // If it doesn't have an ID, then it's a menu title.
            label: "settings",
            icon: "yi-gear",
        }
    );

    /*----------------------------------------*
     *      Treedb Mqtt
     *----------------------------------------*/
    let mqtt_broker_roles = services_roles["treedb_mqtt_broker"];
    if(mqtt_broker_roles && strs_in_list(mqtt_broker_roles, ["root","owner"], true)) {
        /*----------------------*
         *      USER Topics
         *----------------------*/
        let gobj_tables_mqtt_broker = gobj_create_service(
            "#topics_mqtt_broker", // HACK href
            "C_YUI_TREEDB_TOPICS",
            {
                gobj_remote_yuno: remote_service,
                treedb_name: "treedb_mqtt_broker",
            },
            gobj
        );
        priv.user_gobjs.push(gobj_tables_mqtt_broker);
        // gobj_start(gobj_tables_mqtt_broker);
        menu.push(
            {
                id: gobj_name(gobj_tables_mqtt_broker),
                label: "MqttDB Topics",
                icon: "yi-table",
                gobj: gobj_tables_mqtt_broker  // use "$container" attribute
            }
        );

        /*----------------------*
         *      USER Graphs
         *----------------------*/
        let gobj_graph_mqtt_broker = gobj_create_service(
            "#graphs_mqtt_broker", // HACK href
            "C_YUI_TREEDB_GRAPH",
            {
                gobj_remote_yuno: remote_service,
                treedb_name: "treedb_mqtt_broker",
            },
            gobj
        );
        priv.user_gobjs.push(gobj_graph_mqtt_broker);
        // gobj_start(gobj_graph_mqtt_broker);
        menu.push(
            {
                id: gobj_name(gobj_graph_mqtt_broker),
                label: "MqttDB Graphs",
                icon: "yi-hexagon-nodes",
                gobj: gobj_graph_mqtt_broker  // use "$container" attribute
            }
        );
    }

    /*----------------------------------------*
     *      Treedb Authzs
     *----------------------------------------*/
    // TODO repon
    // let authzs_roles = services_roles["treedb_authzs"];
    // if(authzs_roles && strs_in_list(authzs_roles, ["root","owner"], true)) {
    //     /*----------------------*
    //      *      USER Topics
    //      *----------------------*/
    //     let gobj_tables_authzsdb = gobj_create_service(
    //         "#topics_authzs", // HACK href
    //         "C_YUI_TREEDB_TOPICS",
    //         {
    //             gobj_remote_yuno: remote_service,
    //             treedb_name: "treedb_authzs",
    //         },
    //         gobj
    //     );
    //     priv.user_gobjs.push(gobj_tables_authzsdb);
    //     // gobj_start(gobj_tables_authzsdb);
    //     menu.push(
    //         {
    //             id: gobj_name(gobj_tables_authzsdb),
    //             label: "AuthzDB Topics",
    //             icon: "yi-table",
    //             gobj: gobj_tables_authzsdb  // use "$container" attribute
    //         }
    //     );
    //
    //     /*----------------------*
    //      *      USER Graphs
    //      *----------------------*/
    //     let gobj_tabs = gobj_create_pure_child(
    //         "#graphs_authzs", // HACK href
    //         "C_YUI_TABS",
    //         {
    //         },
    //         gobj
    //     );
    //     let label = "AuthzDB Graphs";
    //     menu.push(
    //         {
    //             id: gobj_name(gobj_tabs),
    //             label: label,
    //             icon: "yi-hexagon-nodes",
    //             gobj: gobj_tabs   // use "$container" attribute
    //         }
    //     );
    //
    //     gobj_create_service(
    //         "graphs_authzs",
    //         "C_YUI_TREEDB_GRAPH",
    //         {
    //             subscriber: gobj,
    //             treedb_name: "treedb_authzs",
    //             gobj_remote_yuno: remote_service,
    //             label: label,
    //             icon: "yi-hexagon-nodes"
    //         },
    //         gobj_tabs
    //     );
    //     priv.user_gobjs.push(gobj_tabs);
    // }

    /*----------------------------------------*
     *      Design
     *----------------------------------------*/
    menu.push(
        {
            // If it doesn't have an ID, then it's a menu title.
            label: "developer",
            icon: "yi-gear",
        }
    );

    if(mqtt_broker_roles && strs_in_list(mqtt_broker_roles, ["root","owner"], true)) {
        /*-------------------------*
         *      SYSTEM Topics
         *-------------------------*/
        let gobj_tables_mqtt_broker = gobj_create_service(
            "#topics_mqtt_broker_system", // HACK href
            "C_YUI_TREEDB_TOPICS",
            {
                gobj_remote_yuno: remote_service,
                treedb_name: "treedb_mqtt_broker",
                system: true,
            },
            gobj
        );
        priv.user_gobjs.push(gobj_tables_mqtt_broker);
        // gobj_start(gobj_tables_mqtt_broker);
        menu.push(
            {
                id: gobj_name(gobj_tables_mqtt_broker),
                label: "MQTT Design",
                icon: "yi-table",
                gobj: gobj_tables_mqtt_broker  // use "$container" attribute
            }
        );
    }

    // TODO repon
    // if(authzs_roles && strs_in_list(authzs_roles, ["root","owner"], true)) {
    //     /*-------------------------*
    //      *      SYSTEM Topics
    //      *-------------------------*/
    //     let gobj_tables_authzsdb = gobj_create_service(
    //         "#topics_authzs_system", // HACK href
    //         "C_YUI_TREEDB_TOPICS",
    //         {
    //             gobj_remote_yuno: remote_service,
    //             treedb_name: "treedb_authzs",
    //             system: true,
    //         },
    //         gobj
    //     );
    //     priv.user_gobjs.push(gobj_tables_authzsdb);
    //     // gobj_start(gobj_tables_authzsdb);
    //     menu.push(
    //         {
    //             id: gobj_name(gobj_tables_authzsdb),
    //             label: "AuthzDB Design",
    //             icon: "yi-table",
    //             gobj: gobj_tables_authzsdb  // use "$container" attribute
    //         }
    //     );
    // }

    /*----------------------------------------*
     *      Developer
     *----------------------------------------*/
    if(main_roles && strs_in_list(main_roles, ["developer"], true)) {
        let gobj_tree_js = gobj_create_service(
            "#JS", // HACK href
            "C_UI_TODO", //Ui_gobj_tree_js,
            {
            },
            gobj
        );
        priv.user_gobjs.push(gobj_tree_js);
        // gobj_start(gobj_tree_js);
        menu.push(
            {
                id: gobj_name(gobj_tree_js),
                label: "Frontend View",
                icon: "yi-square-js",
                gobj: gobj_tree_js  // use "$container" attribute
            }
        );
    }

    /*----------------------------------------*
     *      Version
     *----------------------------------------*/
    menu.push(
        {
            // If it doesn't have an ID, then it's a menu title.
            label: `version ${gobj_read_str_attr(gobj_yuno(), "yuno_version")}`
        }
    );

    return menu;
}




                    /***************************
                     *      Actions
                     ***************************/




/********************************************
 *      Connected to yuneta
 *
 *  Example of kw (connection data of __remote_service__):
 {
     "url": "wss://localhost:1996",
     "remote_yuno_name": "pepe.com",
     "remote_yuno_role": "controlcenter",
     "remote_yuno_service": "wss-1",
     "services_roles": {
         "controlcenter": [
             "root"
         ],
         "treedb_controlcenter": [
             "root"
         ],
         "treedb_authzs": [
             "root"
         ]
     },
     "data": null
 }
 *
 ********************************************/
function ac_on_open(gobj, event, kw, src)
{
    let username = gobj_read_str_attr(gobj, "username");
    let username_ = kw.username;
    if(empty_string(username)) {
        /*
         *  Session restored from httpOnly cookies (e.g. F5 refresh).
         *  The BFF /auth/refresh may not return a username, so adopt
         *  the one from the backend identity card (the JWT was already
         *  validated server-side).
         */
        gobj_write_attr(gobj, "username", username_);
        let login = gobj_find_service("__login__");
        if(login) {
            gobj_write_attr(login, "username", username_);
        }
        let yui_main = gobj_find_service("__yui_main__");
        if(yui_main) {
            gobj_send_event(yui_main, "EV_LOGIN_ACCEPTED",
                {username: username_}, gobj);
        }
    } else if(username !== username_) {
        log_error(`${gobj_short_name(gobj)}: username NOT match ${username}, ${username_}`);
        close_all(gobj);
        return -1;
    }
    let services_roles = kw.services_roles || {};

    /*----------------------------------------*
     *      Send log to remote
     *----------------------------------------*/
    set_remote_log_functions(console_log_remote);

    /*----------------------------------------*
     *      Developer
     *----------------------------------------*/
    let main_remote_service = gobj_read_str_attr(gobj, "remote_yuno_service");
    let main_roles = services_roles[main_remote_service];
    if(main_roles && strs_in_list(main_roles, ["developer"], true)) {
        gobj_write_bool_attr(gobj_yuno(), "developer", true); // TODO review
    }

    /*----------------------------------------*
     *      Build the menu's, based in roles
     *----------------------------------------*/
    let home = "#monitoring"; // TODO: derive from user config preferences
    gobj_write_attr(gobj, "home", home);

    let menu = build_app(gobj, services_roles);
    let __yui_routing__ = gobj_find_service("__yui_routing__");
    gobj_write_attr(__yui_routing__, "menu", menu);
    gobj_start(__yui_routing__);

    /*------------------------------------------*
     *  Start services with quickly containers
     *------------------------------------------*/
    // let gobj_monitoring = gobj_find_service("#monitoring");
    // gobj_start(gobj_monitoring);

    /*
     *  HACK: trigger point
     *      Before publish all sizes are 0
     *      After publish all sizes are fill
     *
     *  Publish EV_ON_OPEN:
     *      - ui_main will display APP and hide PUBLI
     */
    gobj_publish_event(gobj, event, kw);
    gobj_start_tree(gobj);

    /*
     *  Select last selection
     *  TODO debería ser por usuario? por si hay mas cuentas en el mismo pc
     */
    let last_selected_menu = kw_get_local_storage_value("last_selected_menu", null, false);
    if(!last_selected_menu) {
        last_selected_menu = home;
    }
    if(last_selected_menu) {
        gobj_send_event(
            __yui_routing__,
            "EV_SELECT",
            {
                id: last_selected_menu
            },
            gobj
        );
    }

    window.dispatchEvent(new Event("resize"));

    return 0;
}

/********************************************
 *  Disconnected from yuneta
 *  Example of kw (disconnection data of __remote_service__):
 {
     "url": "wss://localhost:1996",
     "remote_yuno_name": "estadodelaire.com",
     "remote_yuno_role": "controlcenter",
     "remote_yuno_service": "wss-1"
 }
 ********************************************/
function ac_on_close(gobj, event, kw, src)
{
    close_services(gobj);

    /*----------------------------------------*
     *      Clear log to remote
     *----------------------------------------*/
    set_remote_log_functions(null);

    let __yui_routing__ = gobj_find_service("__yui_routing__");
    gobj_stop(__yui_routing__); // Delete app content
    gobj_write_attr(gobj, "home", "");

    /*
     *  Publish EV_ON_CLOSE:
     *      - ui_main will display PUBLI and hide APP
     */
    gobj_publish_event(gobj, event, kw);

    /*
     *  Do NOT call close_all() here — keep the remote service running
     *  so c_ievent_cli auto-reconnects when the backend comes back.
     *  The httpOnly session cookies remain valid; ac_on_open() will
     *  rebuild the app when the WebSocket reconnects.
     */

    return 0;
}

/********************************************
 *  From login.js
 ********************************************/
function ac_login_accepted(gobj, event, kw, src)
{
    gobj_write_attr(gobj, "username", kw.username);

    /*
     *  SEC-06: the JWT is now stored exclusively in an httpOnly cookie set
     *  by the BFF.  We no longer forward it from JavaScript.  The browser
     *  will send the cookie automatically during the WebSocket HTTP Upgrade
     *  and the Yuneta backend reads it from the Cookie header.
     */
    if (empty_string(gobj_read_str_attr(gobj, "url"))) {
        display_error_message(
            "Error",
            t("no yuneta backend url available"),
            function () {
                close_all(gobj);
            }
        );
    } else {
        do_connect(gobj, null);
    }

    return 0;
}

/********************************************
 *  From login.js
 ********************************************/
function ac_login_denied(gobj, event, kw, src)
{
    close_all(gobj);
    return 0;
}

/********************************************
 *  From login.js
 ********************************************/
function ac_login_refreshed(gobj, event, kw, src)
{
    return 0;
}

/********************************************
 *  From login.js
 ********************************************/
function ac_logout_done(gobj, event, kw, src)
{
    close_all(gobj);
    return 0;
}

/********************************************
 *  Refused identity_card
 ********************************************/
function ac_id_refused(gobj, event, kw, src)
{
    close_all(gobj);

    let message = `<div>
        ${escapeHtml(t('cause'))}: ${escapeHtml(t(kw.comment))}
        <br>
        ${escapeHtml(t('user'))}: ${escapeHtml(kw.username)}
        <br>
        ${escapeHtml(t('remote-service'))}: ${escapeHtml(kw.remote_yuno_role)}/${escapeHtml(kw.remote_yuno_name)}
        <br>
        ${escapeHtml(t('url'))}: ${escapeHtml(kw.url)}
        </div>
    `;

    let title = `${t("connection-backend-refused")}`;

    display_error_message(title, message);

    return 0;
}

/************************************************************
 *  kw: {
 *      topic_name: string,
 *      record: object
 *  }
 ************************************************************/
function ac_view_node_json(gobj, event, kw, src)
{
    let topic_name = kw.topic_name || "";
    let record = kw.record || {};
    let node_id = record.id || "";

    let window_name = `json-view-${topic_name}-${node_id}`;

    // Check if window already exists
    let existing = gobj_find_service(window_name, false);
    if(existing) {
        // Update data in existing window
        let json_graph = gobj_find_service(window_name + "-graph", false);
        if(json_graph) {
            gobj_send_event(json_graph, "EV_LOAD_DATA", {
                path: `${topic_name} / ${node_id}`,
                data: record
            });
        }
        return 0;
    }

    let gobj_json_graph = gobj_create_pure_child(
        window_name + "-graph",
        "C_YUI_JSON_GRAPH",
        {
            json_data: record,
            path: `${topic_name} / ${node_id}`,
        },
        gobj
    );

    gobj_create_service(
        window_name,
        "C_YUI_WINDOW",
        {
            $parent: document.getElementById('top-layer'),
            width: 800,
            height: 600,
            auto_save_size_and_position: true,
            center: true,
            body: gobj_json_graph,
            on_close: function() {
                gobj_destroy(gobj_json_graph);
            }
        },
        gobj
    );

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
    mt_destroy: mt_destroy,
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

    /*---------------------------------------------*
     *          States
     *---------------------------------------------*/
    const st_idle = [
        ["EV_ON_OPEN",                  ac_on_open,             null],
        ["EV_ON_CLOSE",                 ac_on_close,            null],
        ["EV_ON_ID_NAK",                ac_id_refused,          null],
        ["EV_LOGIN_ACCEPTED",           ac_login_accepted,      null],
        ["EV_LOGIN_DENIED",             ac_login_denied,        null],
        ["EV_LOGIN_REFRESHED",          ac_login_refreshed,     null],
        ["EV_LOGOUT_DONE",              ac_logout_done,         null],
        ["EV_VIEW_NODE_JSON",           ac_view_node_json,      null]
    ];

    const states = [
        ["ST_IDLE",     st_idle]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_ON_OPEN",                  event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_ON_CLOSE",                 event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_ON_ID_NAK",                0],
        ["EV_LOGIN_ACCEPTED",           0],
        ["EV_LOGIN_REFRESHED",          0],
        ["EV_LOGIN_DENIED",             0],
        ["EV_LOGOUT_DONE",              0],
        ["EV_VIEW_NODE_JSON",           0],
    ];

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        gmt,
        0,
        attrs_table,
        PRIVATE_DATA,
        0,  // authz_table
        0,  // command_table
        0,  // s_user_trace_level
        0   // gclass_flag
    );

    return __gclass__ ? 0 : -1;
}

/***************************************************************************
 *          Register Yuneta GUI
 ***************************************************************************/
function register_c_yuneta_gui()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yuneta_gui };
