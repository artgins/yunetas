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
    gobj_create2,
    gobj_destroy,
    gobj_start,
    gobj_stop,
} from "yunetas";

// Import uPlot
import uPlot from "uplot";
import "uplot/dist/uPlot.min.css";
import {register_c_sample} from "./c_sample.js";

/***************************************************************
 *
 ***************************************************************/
window.addEventListener('load', function() {
    /*
     *  Delete message "Loading application. Wait please..."
     */
    document.getElementById("loading-message").remove();

    main();
    sample_uplot();
});

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
     *  Create a sample gobj
     *------------------------------------------------*/
    let gobj_sample = gobj_create2(
        "sample",
        "C_SAMPLE",
        {},
        null,
        0
    );
    gobj_start(gobj_sample);
    gobj_stop(gobj_sample);
    gobj_destroy(gobj_sample);

    // trace_msg("CREATING __yuno__");
    // let __yuno__ = new Yuno(
    //     yuno_name,
    //     yuno_role,
    //     yuno_version,
    //     kw
    // );
    //
    //
    // trace_msg("CREATING __default_service__: " + yuno_role);
    // __yuno__.__default_service__ = __yuno__.gobj_create(
    //     yuno_role,
    //     gclass_default_service,
    //     kw_main,
    //     __yuno__
    // );
    // trace_msg("CREATED __default_service__");
    // __yuno__.__default_service__.gobj_start();

    /*------------------------------------------------*
     *      Create __default_service__
     *------------------------------------------------*/
}
