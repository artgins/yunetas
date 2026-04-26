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
    gobj_find_service,
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

import app_config          from "./app_config.json";
import app_config_accordion from "./app_config_accordion.json";


/*  Identity i18n resolver — shows the SHELL translate hook without
 *  doing any actual translation. The C_TEST_LANG controller swaps
 *  this for a Spanish dictionary on every click of the toolbar's
 *  ES/EN button (see c_test_lang.js). */
function identity_translate(key) {
    return key;
}


function pick_config()
{
    let q = new URLSearchParams(window.location.search);
    switch(q.get("preset")) {
        case "accordion": return app_config_accordion;
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

    gobj_create_default_service(
        "shell",
        "C_YUI_SHELL",
        {
            config:    pick_config(),
            use_hash:  true,
            translate: identity_translate
        },
        yuno
    );

    /*  Side controller: subscribes to the shell's EV_TOGGLE_LANGUAGE
     *  (published by the toolbar's lang button) and swaps the shell's
     *  translate hook on each click. */
    let shell = gobj_find_service("shell", false);
    gobj_create_pure_child(
        "test_lang",
        "C_TEST_LANG",
        { shell: shell },
        yuno
    );

    gobj_start(yuno);
    gobj_play(yuno);
}

window.addEventListener("load", () => {
    main();
});
