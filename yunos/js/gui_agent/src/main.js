/***********************************************************************
 *          main.js
 *
 *          Yuneta Agent Console — entry point.
 *
 *          The GUI is built on the declarative shell + nav stack
 *          (C_YUI_SHELL + C_YUI_NAV) from the v2 line of @yuneta/gobj-ui,
 *          consumed as a local file: dependency on the kernel/js/gobj-ui
 *          submodule. All menu structure lives in src/app_config.json;
 *          this file only wires the GClasses and starts the yuno.
 *
 *          Persistence: writable+SDF_PERSIST attrs are stored in the
 *          browser localStorage via the db_*_persistent_attrs helpers
 *          wired into gobj_start_up() below. This is where the
 *          user-entered connection / auth config lives (no private data
 *          is committed to the repo).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    gobj_start_up,
    db_load_persistent_attrs,
    db_save_persistent_attrs,
    db_remove_persistent_attrs,
    db_list_persistent_attrs,
    gobj_create_yuno,
    gobj_create_default_service,
    gobj_create_service,
    gobj_start,
    gobj_play,
    register_c_yuno,
    register_c_timer,
    register_c_ievent_cli,
} from "@yuneta/gobj-js";

import {
    register_c_yui_shell,
    register_c_yui_nav,
    register_c_yui_treedb_topics,
    register_c_yui_treedb_topic_with_form,
} from "@yuneta/gobj-ui/index.js";

import {register_c_gui_agent_view} from "./c_gui_agent_view.js";
import {register_c_agent_config} from "./c_agent_config.js";
import {register_c_agent_login} from "./c_agent_login.js";
import {register_c_settings} from "./c_settings.js";
import {register_c_auth_settings} from "./c_auth_settings.js";
import {register_c_agent_console} from "./c_agent_console.js";
import {register_c_treedb_panel} from "./c_treedb_panel.js";

import {setup_locale} from "./locales/locales.js";

import "bulma/css/bulma.css";
import "@yuneta/gobj-ui/src/c_yui_shell.css";
import "@yuneta/gobj-ui/src/yui_icons.css";
import "@yuneta/gobj-ui/src/ytable.css";
import "tabulator-tables/dist/css/tabulator.min.css";
import "tabulator-tables/dist/css/tabulator_bulma.css";

import app_config from "./app_config.json";


/************************************************
 *          Yuno identity
 ************************************************/
const yuno_name    = "Yuneta Agent Console";
const yuno_role    = "gui_agent";
const yuno_version = "0.1.0";


/***************************************************************
 *          Startup checks
 ***************************************************************/
if(!("WebSocket" in window)) {
    window.alert("This app cannot run without WebSockets!");
}


/***************************************************************
 *          main()
 ***************************************************************/
function main()
{
    /*----------------------------*
     *      Register gclasses
     *----------------------------*/
    /*  Yunetas-js kernel  */
    register_c_yuno();
    register_c_timer();
    register_c_ievent_cli();    // control-plane transport to the agent

    /*  Shell + nav stack (v2)  */
    register_c_yui_shell();
    register_c_yui_nav();

    /*  App-level config + login services + views  */
    register_c_agent_config();
    register_c_agent_login();
    register_c_gui_agent_view();
    register_c_settings();
    register_c_auth_settings();
    register_c_agent_console();

    /*  TreeDB table stack (reused gobj-ui components)  */
    register_c_yui_treedb_topic_with_form();
    register_c_yui_treedb_topics();
    register_c_treedb_panel();

    /*------------------------------------------------*
     *          Start yuneta
     *  Wire localStorage-backed persistence so that
     *  SDF_PERSIST attrs (user connection/auth config)
     *  survive reloads.
     *------------------------------------------------*/
    gobj_start_up(
        null,                       // jn_global_settings
        db_load_persistent_attrs,
        db_save_persistent_attrs,
        db_remove_persistent_attrs,
        db_list_persistent_attrs,
        null,                       // global_command_parser_fn
        null                        // global_stats_parser_fn
    );

    /*------------------------------------------------*
     *          Create the __yuno__
     *------------------------------------------------*/
    let yuno = gobj_create_yuno(
        "gui_agent_yuno",
        "C_YUNO",
        {
            yuno_name:    yuno_name,
            yuno_role:    yuno_role,
            yuno_version: yuno_version
        }
    );

    /*------------------------------------------------*
     *      Initialise i18n (en / es).
     *------------------------------------------------*/
    setup_locale();

    /*------------------------------------------------*
     *      App config service (named service of the yuno).
     *      Holds the user-entered agent endpoints (persisted in
     *      localStorage), shared by the Settings and Console views.
     *------------------------------------------------*/
    let agent_config = gobj_create_service(
        "agent_config",
        "C_AGENT_CONFIG",
        {},
        yuno
    );

    /*------------------------------------------------*
     *      Login service (named service of the yuno).
     *      Holds the OIDC token in memory and forwards it to the
     *      console's transport.
     *------------------------------------------------*/
    let agent_login = gobj_create_service(
        "agent_login",
        "C_AGENT_LOGIN",
        {},
        yuno
    );

    /*------------------------------------------------*
     *      The shell IS the default service.
     *      It builds the whole UI from app_config.
     *------------------------------------------------*/
    gobj_create_default_service(
        "shell",
        "C_YUI_SHELL",
        {
            config:   app_config,
            use_hash: true
        },
        yuno
    );

    /*------------------------------------------------*
     *          Play
     *  The default service (shell) autostarts on play; the named
     *  config service is NOT autostart, so start it explicitly.
     *------------------------------------------------*/
    gobj_start(yuno);
    gobj_start(agent_config);
    gobj_start(agent_login);
    gobj_play(yuno);
}


/***************************************************************
 *          Bootstrap on window load
 ***************************************************************/
window.addEventListener("load", function() {
    /*  Remove the static loading banner injected by index.html  */
    let loading = document.getElementById("loading-message");
    if(loading) {
        loading.remove();
    }

    /*  Clean any leftover hash from a previous session  */
    if(!window.location.hash) {
        window.location.hash = "";
    }

    main();
});
