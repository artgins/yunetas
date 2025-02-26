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
} from "yunetas";

// Import uPlot
import uPlot from "uplot";
import "uplot/dist/uPlot.min.css";
import {register_c_sample} from "./c_sample.js";

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
     *  Create the yuno
     *------------------------------------------------*/
    trace_msg("CREATING __yuno__");
    let yuno = gobj_create_yuno(
        "yuno",
        "C_YUNO",
        {
            yuno_name: "",
            yuno_role: "",
            yuno_version: "",
        }
    );

    /*-------------------------------------*
     *      Create default_service
     *-------------------------------------*/
    trace_msg("CREATING default_service");
    let gobj_default_service = gobj_create_default_service(
        "sample",
        "C_SAMPLE",
        {},
        gobj_yuno()
    );

    /*-------------------------------------*
     *      Play yuno
     *-------------------------------------*/
    gobj_play(yuno);
    gobj_pause(yuno);
    gobj_stop(yuno);

    gobj_destroy(gobj_default_service);
    gobj_destroy(yuno);

}

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
