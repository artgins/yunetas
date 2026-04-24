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
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    gobj_start_up,
    gobj_create_yuno,
    gobj_create_default_service,
    gobj_start, gobj_play,
    register_c_yuno,
    register_c_timer,
} from "@yuneta/gobj-js";

import {
    register_c_yui_shell,
    register_c_yui_nav,
} from "@yuneta/lib-yui";

import {register_c_test_view} from "./c_test_view.js";

import "bulma/css/bulma.css";
import "@yuneta/lib-yui/src/c_yui_shell.css";

import app_config from "./app_config.json";


function main()
{
    /*  Framework  */
    register_c_yuno();
    register_c_timer();

    /*  Shell + nav  */
    register_c_yui_shell();
    register_c_yui_nav();

    /*  App views  */
    register_c_test_view();

    gobj_start_up(null, null, null, null, null, null, null);

    let yuno = gobj_create_yuno(
        "test_yuno",
        "C_YUNO",
        { yuno_name: "shell test", yuno_role: "shell_test_gui", yuno_version: "0.1.0" }
    );

    /*  Create the shell as default service (gobj_play starts it). */
    gobj_create_default_service(
        "shell",
        "C_YUI_SHELL",
        {
            config: app_config,
            use_hash: true
        },
        yuno
    );

    gobj_start(yuno);
    gobj_play(yuno);
}

window.addEventListener("load", () => {
    if(window.location.hash === "") {
        window.location.hash = "";  /* keep empty so shell uses default_route */
    }
    main();
});
