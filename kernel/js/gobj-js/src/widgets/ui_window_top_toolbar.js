/***********************************************************************
 *          ui_window_top_toolbar.js
 *
 *          Top toolbar for windows.
 *  Manage Fullscreen and Close/Destroy buttons.
 *
    config.window_title: "",
    config.window_properties: {
        without_window_fullscreen_btn: false,   // true: Hide fullscreen button
        without_window_close_btn: false,        // true: Hide minimize/destroy button
        without_destroy_window_on_close: false  // true: No destroy window on close (hide)
        hide_exit_fullscreen_button: false
    }

 *
 *
 *  Version
 *  -------
 *  1.0     Initial release
 *
 *          Copyright (c) 2021 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    "use strict";

    /********************************************
     *  Enter in fullscreen
     ********************************************/
    function set_fullscreen(self)
    {
        var title = self.config.window_title;
        var $ui_fullscreen = self.gobj_read_attr("$ui_fullscreen");
        if(!$ui_fullscreen) {
            log_error(self.gobj_short_name() + " No $ui_fullscreen found");
            return -1;
        }

        // HACK here don't need hide the window_top_toolbar
        // because in popups the fullscreen is a inside zone of window

        webix.fullscreen.set(
            $ui_fullscreen,
            {
                head: {
                    view:"toolbar",
                    height: 40,
                    elements: [
                        {
                            view: "icon",
                            icon: self.config.window_properties.hide_exit_fullscreen_button?
                                "":"fas fa-chevron-left",
                            tooltip: t("exit fullscreen"),
                            click: function() {
                                webix.fullscreen.exit();
                            }
                        },
                        {},
                        {
                            view: "label",
                            label: title,
                        },
                        {}
                    ]
                }
            }
        );

        return 0;
    }

    /************************************************
     *  Top Toolbar of "Window"
     ************************************************/
    function get_window_top_toolbar(self, cols)
    {
        var toolbar_cols = cols || [];
        __extend_list__(toolbar_cols, [
            {},
            {
                view: "label",
                gravity: 20,
                label: self.config.window_title
            },
            {},
            {
                view:"icon",
                hidden: self.config.window_properties.without_window_fullscreen_btn?true:false,
                icon: "fas fa-expand-wide",
                tooltip: t("fullscreen"),
                click: function() {
                    set_fullscreen(self);
                }
            },
            {
                view: "icon",
                id: build_name(self, "window_top_toolbar-close"),
                hidden: self.config.window_properties.without_window_close_btn?true:false,

                icon: (self.config.window_properties.without_destroy_window_on_close)?
                    "far fa-window-minimize":"fas fa-times",
                tooltip: (self.config.window_properties.without_destroy_window_on_close)?
                    t("hide"):t("close"),
                click: function() {
                    if(self.config.window_properties.without_destroy_window_on_close) {
                        // Only minimize
                        if(self.gobj_event_in_input_event_list("EV_CLOSE_WINDOW")) {
                            self.gobj_send_event("EV_CLOSE_WINDOW", {destroying:false}, self);
                        }
                        self.config.$ui.hide();
                    } else {
                        // Destroy
                        if(self.gobj_event_in_input_event_list("EV_CLOSE_WINDOW")) {
                            self.gobj_send_event("EV_CLOSE_WINDOW", {destroying:true}, self);
                        }
                        if(!self._destroyed) {
                            // Could be already destroyed in EV_CLOSE_WINDOW action event
                            __yuno__.gobj_destroy(self);
                        }
                    }
                }
            }
        ]);

        var top_toolbar = {
            view:"toolbar",
            id: build_name(self, "window_top_toolbar"),
            css: "toolbar2color",
            height: 30,
            cols: toolbar_cols
        };
        return top_toolbar;
    }

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.get_window_top_toolbar = get_window_top_toolbar;

})(this);
