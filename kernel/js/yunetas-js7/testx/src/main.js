/***********************************************************************
 *          main.js
 *
 *          Entry point
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

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
});
