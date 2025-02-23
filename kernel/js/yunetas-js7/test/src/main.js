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
} from "yunetas";

// Import uPlot
import uPlot from "uplot";
import "uplot/dist/uPlot.min.css"; // ✅ Import CSS

window.addEventListener('load', function() {
    /*
     *  Delete message "Loading application. Wait please..."
     */
    document.getElementById("loading-message").remove();

    /*
     *  Startup gobj system
     */
    gobj_start_up(
        null,                           // jn_global_settings
        db_load_persistent_attrs,       // load_persistent_attrs_fn
        db_save_persistent_attrs,       // save_persistent_attrs_fn
        db_remove_persistent_attrs,     // remove_persistent_attrs_fn
        db_list_persistent_attrs,       // list_persistent_attrs_fn
        null,                           // global_command_parser_fn
        null                            // global_stats_parser_fn
    );



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

});
