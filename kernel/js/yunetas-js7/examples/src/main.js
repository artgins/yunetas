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
    gobj_create,
    gobj_destroy,
    gobj_start,
    gobj_stop,
    gobj_play,
    gobj_pause,
    gobj_yuno,
    trace_msg,
    register_c_yuno,
    kw_get_local_storage_value,
} from "yunetas";

// Import uPlot
import uPlot from "uplot";
import "uplot/dist/uPlot.min.css";

import {register_c_sample} from "./c_sample.js";

/************************************************
 *          Data
 ************************************************/
const yuno_name = "Sample GUI";
const yuno_role = "sample_gui";
const yuno_version = "1.0.0";

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

/************************************************
 *          Startup code
 ************************************************/
if (!('WebSocket' in window)) {
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

if (!isFlexSupported()) {
    window.alert("This app cannot run in old browser versions!");
}

/***************************************************************
 *
 ***************************************************************/
function sample_uplot()
{
    // Example: Creating a uPlot chart
    const data = [
        [0, 1, 2, 3, 4, 5], // X-axis values
        [3, 4, 5, 6, 7, 8]  // Y-axis values
    ];

    const opts = {
        title: "uPlot Example",
        width: 800,
        height: 400,
        series: [
            {},
            { stroke: "red", fill: "rgba(255, 0, 0, 0.3)" }
        ]
    };

    const chartContainer = document.createElement("div");
    document.body.appendChild(chartContainer);

    new uPlot(opts, data, chartContainer);
}

/***************************************************************
 *
 ***************************************************************/
function main()
{
    /*--------------------*
     *  Register gclass
     *--------------------*/
    register_c_yuno();
    register_c_sample();

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
        "yuno",
        "C_YUNO",
        {
            yuno_name: yuno_name,
            yuno_role: yuno_role,
            yuno_version: yuno_version,
            tracing: tracing,
            trace_timer: trace_timer,
            trace_inter_event: trace_inter_event,
            trace_ievent_callback: null,
            trace_creation: kw_get_local_storage_value("trace_creation", trace_creation,false),
            trace_i18n: kw_get_local_storage_value("trace_i18n", trace_i18n, false),
        }
    );

    /*-------------------------------------*
     *      Create default_service
     *-------------------------------------*/
    trace_msg("CREATING default_service");
    let gobj_default_service = gobj_create_default_service(
        "sample",
        "C_SAMPLE",
        {
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
    sample_uplot();
});
