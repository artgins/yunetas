/***********************************************************************
 *          __yuno__.js
 *
 *          Root gobj __yuno__
 *
 *          Copyright (c) 2022 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function(exports) {
    "use strict";

    /************************************************
     *          Data
     ************************************************/
    const yuno_name = "Yuneta GUI";
    const yuno_role = "yuneta_gui";
    const yuno_version = "1";
    const gclass_default_service = Yuneta_gui;

    /*
     *  TEST Trace Simple Machine
     *  Set 1 or 2 to see activity machine.
     *  1: without kw details
     *  2: with kw details.
     */
    let tracing = 1;
    let trace_timer = 0;

    /*
     *  Trace inter-events or gobjs creation
     */
    let trace_inter_event = 1;
    let trace_creation = 0;

    let locales = {
        en: en,
        es: es
    };

    /************************************************
     *          Startup code
     ************************************************/
    if(!Modernizr.websockets) {
        alert("This app cannot run without websockets!");
        return;
    }

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

    /*
     *  Create the __yuno__ gobj, the grandfather.
     */
    let kw = {
        tracing: tracing,
        trace_timer: trace_timer,
        trace_inter_event: trace_inter_event,
        trace_creation: trace_creation,
        trace_ievent_callback: null
    };
    trace_msg("CREATING __yuno__");
    let __yuno__ = new Yuno(
        yuno_name,
        yuno_role,
        yuno_version,
        kw
    );

    window.onload = function() {
        /*
         *  Delete message waiting
         */
        document.getElementById("loading-message").remove();

        let kw_main = {
            locales: locales
        };

        /*
         *  Create __default_service__
         */
        trace_msg("CREATING __default_service__: " + yuno_role);
        __yuno__.__default_service__ = __yuno__.gobj_create(
            yuno_role,
            gclass_default_service,
            kw_main,
            __yuno__
        );
        trace_msg("CREATED __default_service__");
        __yuno__.__default_service__.gobj_start();
    };

    window.onbeforeunload = function() {
        let r =__yuno__.gobj_read_attr("changesLost");
        return r?r:null;
    };

    /************************************************
     *          Expose to the global object
     ************************************************/
    exports.__yuno__ = __yuno__;
    exports.t = function(key, options) {
        return i18next.t(key, options);
    };
    exports.locales = locales;

})(this);
