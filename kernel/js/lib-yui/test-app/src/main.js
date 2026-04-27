/***********************************************************************
 *          main.js - Test harness for C_YUI_SHELL + C_YUI_NAV
 *
 *  Resize the browser window to watch zones swap between breakpoints:
 *      mobile  (<769)   primary menu goes to bottom (icon-bar),
 *                       submenu tabs appear at top-sub.
 *      tablet  (769-1023) primary menu still at bottom (icon-bar),
 *                       submenu tabs still at top-sub.
 *      desktop (>=1024) primary menu moves to left (vertical),
 *                       submenu becomes vertical list at right.
 *
 *  URL switches:
 *      ?preset=accordion    loads app_config_accordion.json instead
 *                           (exercises the accordion + submenu layouts).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    gobj_start_up,
    gobj_create_yuno,
    gobj_create_default_service,
    gobj_create_pure_child,
    gobj_start, gobj_play,
    register_c_yuno,
    register_c_timer,
} from "@yuneta/gobj-js";

import {
    register_c_yui_shell,
    register_c_yui_nav,
} from "@yuneta/lib-yui";

import {register_c_test_view} from "./c_test_view.js";
import {register_c_test_lang} from "./c_test_lang.js";

import "bulma/css/bulma.css";
import "@yuneta/lib-yui/src/c_yui_shell.css";

import app_config           from "./app_config.json";
import app_config_accordion from "./app_config_accordion.json";
import app_config_multimenu from "./app_config_multimenu.json";


function pick_config()
{
    let q = new URLSearchParams(window.location.search);
    switch(q.get("preset")) {
        case "accordion": return app_config_accordion;
        case "multimenu": return app_config_multimenu;
        default:          return app_config;
    }
}


function main()
{
    /*  Framework  */
    register_c_yuno();
    register_c_timer();

    /*  Shell + nav  */
    register_c_yui_shell();
    register_c_yui_nav();

    /*  App views + language controller  */
    register_c_test_view();
    register_c_test_lang();

    gobj_start_up(null, null, null, null, null, null, null);

    let yuno = gobj_create_yuno(
        "test_yuno",
        "C_YUNO",
        { yuno_name: "shell test", yuno_role: "shell_test_gui", yuno_version: "0.1.0" }
    );

    /*  Capture the shell directly — gobj_create_default_service
     *  registers __default_service__ but does NOT populate the
     *  __jn_services__ map, so gobj_find_service("shell") would
     *  return null.  Use the return value instead. */
    let shell = gobj_create_default_service(
        "shell",
        "C_YUI_SHELL",
        {
            config:   pick_config(),
            use_hash: true
        },
        yuno
    );

    /*  Side controller: subscribes to the shell's EV_TOGGLE_LANGUAGE
     *  (published by the toolbar's lang button) and walks the shell's
     *  $container with refresh_language() on each click. */
    let test_lang = gobj_create_pure_child(
        "test_lang",
        "C_TEST_LANG",
        { shell: shell },
        yuno
    );

    gobj_start(yuno);
    gobj_play(yuno);

    /*  c_yuno.mt_play only starts the default_service (the shell);
     *  pure children of the yuno (like test_lang) are NOT started
     *  automatically.  Start it explicitly so its mt_start runs and
     *  the EV_TOGGLE_LANGUAGE subscription is registered before the
     *  user can click the toolbar button. */
    gobj_start(test_lang);
}

window.addEventListener("load", () => {
    main();
});
