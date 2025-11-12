/***********************************************************************
 *          main.js
 *
 *          Entry point
 *
 *          Copyright (c) 2025, ArtGins.
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
    gobj_start,
    gobj_play,
    gobj_yuno,
    trace_msg,
    register_c_yuno,
    register_c_timer,
    register_c_ievent_cli,
    kw_get_local_storage_value,
    gobj_read_attr,
    gobj_write_bool_attr,
} from "yunetas";

import {register_c_yuneta_gui} from "./c_yuneta_gui.js";
import {register_c_login} from "./c_login.js";
import {register_c_ui_todo} from "./c_ui_todo.js";

import {register_c_yui_map} from "./ui/c_yui_map.js";
import {register_c_yui_treedb_graph} from "./ui/c_yui_treedb_graph.js";
import {register_c_yui_treedb_topic_form} from "./ui/c_yui_treedb_topic_form.js";
import {register_c_yui_treedb_topics} from "./ui/c_yui_treedb_topics.js";
import {register_c_yui_uplot} from "./ui/c_yui_uplot.js";

import {register_c_yui_main} from "./ui/c_yui_main.js";
import {register_c_yui_routing} from "./ui/c_yui_routing.js";
import {register_c_yui_window} from "./ui/c_yui_window.js";
import {register_c_yui_form} from "./ui/c_yui_form.js";
import {register_c_yui_tabs} from "./ui/c_yui_tabs.js";

import {register_c_g6_node} from "./ui/c_g6_node.js";

import "bulma/css/bulma.css";
import "/assets/fontawesome-free-7.1.0-web/css/fontawesome.css";
import "/assets/fontawesome-free-7.1.0-web/css/regular.css";
import "/assets/fontawesome-free-7.1.0-web/css/solid.css";

import "tabulator-tables/dist/css/tabulator.min.css"; // Import Tabulator CSS
import "tabulator-tables/dist/css/tabulator_bulma.css";
import "uplot/dist/uPlot.min.css";

import "vanilla-jsoneditor/themes/jse-theme-dark.css";
import "tom-select/dist/css/tom-select.css"; // Import Tom-Select CSS
import "bootstrap-table/dist/bootstrap-table.css";
import "bootstrap-table/dist/extensions/fixed-columns/bootstrap-table-fixed-columns.css";
import "bootstrap-table/dist/themes/bulma/bootstrap-table-bulma.min.css";

import "bulma-switch-control/css/main.css";

// import "yuneta-icon-font/dist/yuneta-icon-font.js"; // TODO parece que no se usa
// import "yuneta-icon-font/dist/yuneta-icon-font.css";

import "./ui/c_yui_main.css";
import "./ui/c_yui_map.css";
import "./ui/c_yui_routing.css";
import "./ui/ytable.css";
import "./ui/yui_toolbar.css";
import "./ui/lib_graph.css";

/************************************************
 *          Data
 ************************************************/
const yuno_name = "TreeDB GUI";
const yuno_role = "treedb_gui";
const yuno_version = "7.0.0";

const remote_yuno_role = "mqtt_broker";
const remote_yuno_service = "mqtt-broker-db";
const required_services = ["treedb_authzs"];

/*
 *  TEST Trace Simple Machine
 *  Set 1 or 2 to see activity machine.
 *  1: without kw details
 *  2: with kw details.
 */
let tracing = 0;
let trace_timer = 0;

/*
 *  Trace inter-events or gobjs creation
 */
let trace_inter_event = 0;
let trace_creation = 0;
let trace_i18n = 0;
let trace_start_stop = 0;
let trace_subscriptions = 0;

/************************************************
 *          Startup code
 ************************************************/
if(!('WebSocket' in window)) {
    window.alert("This app cannot run without websockets!");
}

function isFlexSupported()
{
    // Create a temporary element
    let testElement = document.createElement('div');

    // Attempt to set the display property to flex
    testElement.style.display = 'flex';

    // Check if the display property is set to flex
    return testElement.style.display === 'flex';
}

if(!isFlexSupported()) {
    window.alert("This app cannot run in old browser versions!");
}

/***************************************************************
 *
 ***************************************************************/
function main()
{
    /*----------------------------*
     *      Register gclass
     *----------------------------*/
    /*
     *  Yunetas-js kernel
     */
    register_c_yuno();
    register_c_timer();
    register_c_ievent_cli();

    /*
     *  App
     */
    register_c_yuneta_gui();
    register_c_login();
    register_c_ui_todo();

    /*
     *  Yui library
     */
    register_c_yui_main();
    register_c_yui_routing();
    register_c_yui_map();
    register_c_yui_treedb_graph();
    register_c_yui_treedb_topic_form();
    register_c_yui_treedb_topics();
    register_c_yui_uplot();
    register_c_yui_window();
    register_c_yui_form();
    register_c_yui_tabs();
    register_c_g6_node();

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    gobj_start_up(
        null,                           // jn_global_settings
        db_load_persistent_attrs,       // load_persistent_attrs_fn
        db_save_persistent_attrs,       // save_persistent_attrs_fn
        db_remove_persistent_attrs,     // remove_persistent_attrs_fn
        db_list_persistent_attrs,       // list_persistent_attrs_fn
        null,                           // global_command_parser_fn
        null                            // global_stats_parser_fn
    );

    /*------------------------------------------------*
     *  Create the __yuno__ gobj, the grandfather.
     *------------------------------------------------*/
    trace_msg("CREATING __yuno__");
    let yuno = gobj_create_yuno(
        "gui_yuno",
        "C_YUNO",
        {
            yuno_name: yuno_name,
            yuno_role: yuno_role,
            yuno_version: yuno_version,
            required_services: required_services,
            tracing: tracing,
            trace_timer: trace_timer,
            trace_inter_event: trace_inter_event,
            trace_ievent_callback: null,
            trace_creation: kw_get_local_storage_value("trace_creation", trace_creation, false),
            trace_i18n: kw_get_local_storage_value("trace_i18n", trace_i18n, false),
            trace_start_stop: kw_get_local_storage_value("trace_start_stop", trace_start_stop, false),
            trace_subscriptions: kw_get_local_storage_value("trace_subscriptions", trace_subscriptions, false),
        }
    );

    /*-------------------------------------*
     *      Create default_service
     *-------------------------------------*/
    trace_msg("CREATING default_service");
    let gobj_service = gobj_create_default_service(
        "gui_service",
        "C_YUNETA_GUI",
        {
            remote_yuno_role: remote_yuno_role,
            remote_yuno_service: remote_yuno_service,
            required_services: required_services,
        },
        gobj_yuno()
    );

    /*-------------------------------------*
     *      Play yuno
     *-------------------------------------*/
    gobj_start(yuno);
    gobj_play(yuno);    // this will start default service
    // gobj_pause(yuno);
    // gobj_stop(yuno);
    //
    // gobj_destroy(gobj_default_service);
    // gobj_destroy(yuno);
}

/***************************************************************
 *
 ***************************************************************/
window.addEventListener('load', function() {
    /*
     *  Delete message "Loading application. Wait please..."
     */
    document.getElementById("loading-message").remove();

    /*
     *  Clean url hash
     */
    window.location.hash = '';

    main();

    manage_bulma_modals();

    window.addEventListener("beforeunload", function() {
        gobj_write_bool_attr(gobj_yuno(), "browser_beforeunload", true);
        let r = gobj_read_attr(gobj_yuno(), "changesLost");
        return r ? r : null;
    });

    document.addEventListener('DOMContentLoaded', () => {
        // Initialize bulma theme on load
        const savedTheme = localStorage.getItem('theme') || 'light';
        document.documentElement.setAttribute("data-theme", savedTheme);
    });

    /*
     *  Manage modals of Bulma
     */
    function manage_bulma_modals()
    {
        // Functions to open and close a modal
        function openModal($el)
        {
            $el.classList.add('is-active');
        }

        function closeModal($el)
        {
            $el.classList.remove('is-active');
            if($el.parentElement.classList.contains('popup-layer')) {
                $el.remove();
            }
        }

        function closeAllModals()
        {
            (document.querySelectorAll('.modal') || []).forEach(($modal) => {
                closeModal($modal);
            });
            (document.querySelectorAll('.popup') || []).forEach(($modal) => {
                closeModal($modal);
            });
        }

        // Add a click event on buttons to open a specific modal
        (document.querySelectorAll('.js-modal-trigger') || []).forEach(($trigger) => {
            const modal = $trigger.dataset.target;
            const $target = document.getElementById(modal);

            $trigger.addEventListener('click', () => {
                openModal($target);
            });
        });

        // Add a click event on various child elements to close the parent modal
        // (document.querySelectorAll('.modal-background, .modal-close, .modal-card-head .delete, .modal-card-foot .button') || []).forEach(($close) => {
        //     const $target = $close.closest('.modal');
        //
        //     $close.addEventListener('click', () => {
        //         closeModal($target);
        //     });
        // });

        // Add a keyboard event to close all modals
        document.addEventListener('keydown', (event) => {
            if(event.key === "Escape") {
                closeAllModals();
            }
        });

        /*
         *
         *  Add a click event to close all modals when click outside from popup
         *  WARNING: Remember add:
         *          event.stopPropagation();
         *  if you don't want to get the event here.
         */
        document.addEventListener('click', (event) => {
            (document.querySelectorAll('.modal') || []).forEach(($element) => {
                if(!$element.contains(event.target)) {
                    if($element.classList.contains('is-active')) {
                        $element.classList.remove('is-active');
                    }
                }
                $element.parentNode.removeChild($element);
            });

            (document.querySelectorAll('.popup') || []).forEach(($element) => {
                if(!$element.contains(event.target)) {
                    if($element.classList.contains('is-active')) {
                        $element.classList.remove('is-active');
                    }
                }
                $element.parentNode.removeChild($element);
            });
        });
    }

});
