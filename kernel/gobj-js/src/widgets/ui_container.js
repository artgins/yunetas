/***********************************************************************
 *          ui_container.js
 *
 *          Container of widgets
 *          NOTE search "Container Panel" phrase to locate related code
 *
 *  panel_properties: all values are false by default

        with_panel_fullscreen_btn: false,   // show fullscreen button in top toolbar
        with_panel_hidden_btn: false,       // show minimize button in top toolbar
        with_panel_resize_btn: false,       // show resizes buttons in top toolbar
        with_panel_title: "",               // title to show if not empty in top toolbar
        with_panel_top_toolbar: false       // show top toolbar


 *  Version
 *  -------
 *  1.0     Initial release
 *
 *          Copyright (c) 2020 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    "use strict";

    /********************************************
     *      Configuration (C attributes)
     ********************************************/
    var CONFIG = {
        toolbar_size: 40,
        mode: "horizontal",     // "horizontal" or "vertical"

        gobj_name_in_fullscreen: "",

        // HACK set this variable in panels to hide fullscreen exit button
        hide_exit_fullscreen_button: false,

        ui_properties: null,
        $ui: null,

        views_opened: {
        },
        views_gravity: {
        },

        __writable_attrs__: [
            "gobj_name_in_fullscreen",
            "views_opened",
            "views_gravity",
        ]
    };




            /***************************
             *      Local Methods
             ***************************/




    /************************************************************
     *   Webix UI
     ************************************************************/
    function build_webix(self)
    {
        if(self.config.mode == "horizontal") {
            self.config.$ui = webix.ui({
                view: "layout",
                id: self.gobj_name(),
                type: "space",
                padding: 5,
                margin: 5,
                rows: [
                    {
                        template: "container_top_toolbar",
                        id: build_name(self, "container_top_toolbar"),
                        height: self.config.toolbar_size,
                        hidden: true
                    },
                    {
                        view: "layout",
                        id: build_name(self, "cols"),
                        type: "wide",
                        margin: 5,
                        cols: [
                            {
                                template: "container_left_toolbar",
                                id: build_name(self, "container_left_toolbar"),
                                width: self.config.toolbar_size,
                                hidden: true
                            },
                            {
                                view: "scrollview",
                                id: build_name(self, "scrollview"),
                                scroll: "auto", // Si lo quitas la cagas
                                body: {
                                    view: "layout",
                                    id: build_name(self, "work_place"),
                                    type: "wide",
                                    margin: 5,
                                    cols: []
                                }
                            },
                            {
                                template: "container_right_toolbar",
                                id: build_name(self, "container_right_toolbar"),
                                width: self.config.toolbar_size,
                                hidden: true
                            }
                        ]
                    },
                    {
                        template: "container_bottom_toolbar",
                        id: build_name(self, "container_bottom_toolbar"),
                        height: self.config.toolbar_size,
                        hidden: true
                    }
                ]
            });
        } else {
            self.config.$ui = webix.ui({
                view: "layout",
                id: self.gobj_name(),
                type: "space",
                padding: 5,
                margin: 5,
                rows: [
                    {
                        template: "container_top_toolbar",
                        id: build_name(self, "container_top_toolbar"),
                        height: self.config.toolbar_size,
                        hidden: true
                    },
                    {
                        view: "layout",
                        id: build_name(self, "cols"),
                        type: "wide",
                        margin: 5,
                        cols: [
                            {
                                template: "container_left_toolbar",
                                id: build_name(self, "container_left_toolbar"),
                                width: self.config.toolbar_size,
                                hidden: true
                            },
                            {
                                view: "scrollview",
                                id: build_name(self, "scrollview"),
                                scroll: "auto",
                                body: {
                                    view: "layout",
                                    id: build_name(self, "work_place"),
                                    type: "wide",
                                    margin: 5,
                                    rows: []
                                }
                            },
                            {
                                template: "container_right_toolbar",
                                id: build_name(self, "container_right_toolbar"),
                                width: self.config.toolbar_size,
                                hidden: true
                            }
                        ]
                    },
                    {
                        template: "container_bottom_toolbar",
                        id: build_name(self, "container_bottom_toolbar"),
                        height: self.config.toolbar_size,
                        hidden: true
                    }
                ]
            });
        }

        self.config.$ui.gobj = self;

        if(self.config.ui_properties) {
            self.config.$ui.define(self.config.ui_properties);
            if(self.config.$ui.refresh) {
                self.config.$ui.refresh();
            }
        }
    }




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *  Panel View is showing, save their state
     ********************************************/
    function ac_on_view_show(self, event, kw, src)
    {
        var visible = src.config.$ui.isVisible();
        self.config.views_opened[src.gobj_name()] = visible;

        /*
         *  Save persistent attrs
         */
        if(self.gobj_is_unique()) {
            self.gobj_save_persistent_attrs();
        }
        return 0;
    }

    /********************************************
     *  Panel View enter in fullscreen
     ********************************************/
    function ac_set_fullscreen(self, event, kw, src)
    {
        var title = kw_get_str(kw, "title", "", 0);

        // Save the gobj name in fullscreen
        self.config.gobj_name_in_fullscreen = src.gobj_name();

        $$(build_name(src, "ct_panel_top_toolbar")).hide();

        webix.fullscreen.set(
            src.config.$ui,
            {
                head: {
                    view:"toolbar",
                    height: 40,
                    elements: [
                        {
                            view: "icon",
                            icon: src.config.hide_exit_fullscreen_button?
                                "":"fas fa-chevron-left",
                            tooltip: t("exit fullscreen"),
                            click: function() {
                                self.gobj_send_event("EV_EXIT_FULLSCREEN", {}, self);
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

        /*
         *  Save persistent attrs
         */
        if(self.gobj_is_unique()) {
            self.gobj_save_persistent_attrs();
        }

        return 0;
    }

    /********************************************
     *  Panel View exit of fullscreen
     ********************************************/
    function ac_exit_fullscreen(self, event, kw, src)
    {
        if(!self.config.gobj_name_in_fullscreen) {
            log_warning("Nobody in fullscreen");
            return -1;
        }
        webix.fullscreen.exit();

        var gobj = self.yuno.gobj_find_unique_gobj(self.config.gobj_name_in_fullscreen, false);
        if(gobj) {
            $$(build_name(gobj, "ct_panel_top_toolbar")).show();
        }

        /*
         *  Save persistent attrs
         */
        self.config.gobj_name_in_fullscreen = "";
        if(self.gobj_is_unique()) {
            self.gobj_save_persistent_attrs();
        }

        return 0;
    }

    /********************************************
     *  type: ["container_top_toolbar",
     *          "container_bottom_toolbar",
     *          "container_left_toolbar",
     *          "container_right_toolbar"
     *      ]
     *  toolbar: webix ui
     ********************************************/
    function ac_add_toolbar(self, event, kw, src)
    {
        var type = kw.type;
        var toolbar = kw.toolbar;
        if(!toolbar) {
            log_error("no toolbar def");
            return -1;
        }
        if(!elm_in_list(type, [
                "container_top_toolbar",
                "container_bottom_toolbar",
                "container_left_toolbar",
                "container_right_toolbar"])) {
            log_error("bad toolbar type: '" + type + "'");
            return -1;
        }
        toolbar = __duplicate__(toolbar);

        if(elm_in_list(type, [
                "container_top_toolbar",
                "container_bottom_toolbar"])) {
            if(!kw_has_key(toolbar, "height")) {
                toolbar["height"] = self.config.toolbar_size;
            }
            // HACK Dynamic UI Modifications: replace toolbar
            webix.ui(toolbar, $$(self.gobj_name()), $$(build_name(self, type)));
        } else {
            if(!kw_has_key(toolbar, "width")) {
                toolbar["width"] = self.config.toolbar_size;
            }
            // HACK Dynamic UI Modifications: replace toolbar
            webix.ui(toolbar, $$(build_name(self, "cols")), $$(build_name(self, type)));
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_change_mode(self, event, kw, src)
    {
        var work_place = null;

        var childs = self.gobj_match_childs({});
        for(var i=0; i < childs.length; i++) {
            var child = childs[i];

            if(child.config.$container_parent) {
                child.config.$container_parent.removeView(child.config.$ui);
                child.config.$container_parent = null;
            }
        }

        if(self.config.mode == "horizontal") {
            work_place = {
                view: "layout",
                id: build_name(self, "work_place"),
                type: "wide",
                margin: 5,
                rows: []
            };
            self.config.mode = "vertical";
        } else {
            work_place = {
                view: "layout",
                id: build_name(self, "work_place"),
                type: "wide",
                margin: 5,
                cols: []
            };
            self.config.mode = "horizontal";
        }

        // HACK Dynamic UI Modifications: replace "work_place"
        webix.ui(
            work_place,
            $$(build_name(self, "scrollview")),
            $$(build_name(self, "work_place"))
        );

        var $container_parent = $$(build_name(self, "work_place"));
        for(var i=0; i < childs.length; i++) {
            var child = childs[i];
            child.gobj_send_event("EV_REBUILD_PANEL", {}, self); // Before this get new child $ui

            if(self.config.views_opened[child.gobj_name()]) {
                child.config.$ui.show();
            } else {
                child.config.$ui.hide();
            }

            $container_parent.addView(child.config.$ui);
            child.config.$container_parent = $container_parent;
        }

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_select(self, event, kw, src)
    {
        return 0;
    }

    /********************************************
     *  Some gravity or other has changed,
     *  refresh all container's pannel
     ********************************************/
    function ac_refresh(self, event, kw, src)
    {
        var childs = self.gobj_match_childs({});

        for(var i=0; i<childs.length; i++) {
            var child = childs[i];
            if(child.config.$ui.isVisible()) {
                child.gobj_send_event("EV_REFRESH", {}, self);
            }
        }

        /*
         *  Get gravity of childs to save persistent attrs
         */
        self.config.views_gravity = {};

        for(var i=0; i<childs.length; i++) {
            var child = childs[i];
            var gravity = child.config.$ui.config.gravity;
            self.config.views_gravity[child.gobj_name()] = gravity;
        }

        /*
         *  Save persistent attrs
         */
        if(self.gobj_is_unique()) {
            self.gobj_save_persistent_attrs();
        }
        return 0;
    }




            /***************************
             *      GClass/Machine
             ***************************/




    var FSM = {
        "event_list": [
            "EV_ON_VIEW_SHOW",
            "EV_SET_FULLSCREEN",
            "EV_EXIT_FULLSCREEN",
            "EV_CHANGE_MODE",
            "EV_ADD_TOOLBAR",
            "EV_SELECT",
            "EV_REFRESH"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["EV_ON_VIEW_SHOW",         ac_on_view_show,        undefined],
                ["EV_SET_FULLSCREEN",       ac_set_fullscreen,      undefined],
                ["EV_EXIT_FULLSCREEN",      ac_exit_fullscreen,     undefined],
                ["EV_CHANGE_MODE",          ac_change_mode,         undefined],
                ["EV_ADD_TOOLBAR",          ac_add_toolbar,         undefined],
                ["EV_SELECT",               ac_select,              undefined],
                ["EV_REFRESH",              ac_refresh,             undefined]
            ]
        }
    };

    var Ui_container = GObj.__makeSubclass__();
    var proto = Ui_container.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Ui_container",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Ui_container, "Ui_container");




            /***************************
             *      Framework Methods
             ***************************/




    /************************************************
     *      Framework Method create
     ************************************************/
    proto.mt_create = function(kw)
    {
        var self = this;

        build_webix(self);
    }

    /************************************************
     *      Framework Method destroy
     *      In this point, all childs
     *      and subscriptions are already deleted.
     ************************************************/
    proto.mt_destroy = function()
    {
    }

    /************************************************
     *      Framework Method start
     ************************************************/
    proto.mt_start = function(kw)
    {
        var self = this;
    }

    /************************************************
     *      Framework Method stop
     ************************************************/
    proto.mt_stop = function(kw)
    {
        var self = this;
    }

    /************************************************
     *      Framework Method inject_event
     ************************************************/
    proto.mt_inject_event = function(event, kw, src)
    {
        var self = this;
        kw = kw || {};
        return self.parent.gobj_send_event(event, kw, src);
    }

    /************************************************
     *  Framework Method mt_child_added
     ************************************************/
    proto.mt_child_added = function(child)
    {
        var self = this;

        var $container_parent = $$(build_name(self, "work_place"));
        $container_parent.addView(child.config.$ui);
        child.config.$container_parent = $container_parent;

        if(kw_has_key(self.config.views_gravity, child.gobj_name())) {
            var gravity = self.config.views_gravity[child.gobj_name()];
            child.config.ui_properties["gravity"] = gravity;
            child.config.$ui.define({gravity:gravity});
        }

        if(!child.config.panel_properties.with_panel_hidden_btn) {
            self.config.views_opened[child.gobj_name()] = true;
            child.config.$ui.hide();
            child.config.$ui.show();
        } else if(!kw_has_key(self.config.views_opened, child.gobj_name())) {
            //self.config.views_opened[child.gobj_name()] = true;
            child.config.$ui.show();
            child.config.$ui.hide();

        } else {
            if(self.config.views_opened[child.gobj_name()]) {
                child.config.$ui.show();
            } else {
                child.config.$ui.hide();
            }
        }
    }

    /************************************************
     *  Framework Method mt_child_added
     ************************************************/
    proto.mt_child_removed = function(child)
    {
        var self = this;
        if(child.config.$container_parent) {
            child.config.$container_parent.removeView(child.config.$ui);
            child.config.$container_parent = null;
        }
    }

    //=======================================================================
    //      Common code for container panels
    //=======================================================================
    function toggle_container_panel(gobj_panel)
    {
        var $ui = gobj_panel.config.$ui;
        if(!$ui) {
            log_error(gobj_panel.gobj_short_name() + ": toggle_container_panel() $ui NULL");
            return;
        }
        if($ui.isVisible()) {
            $ui.hide();
        } else {
            $ui.show();
        }

        /*----------------------------------------------*
         *  Inform of view state to "Container Panel"
         *----------------------------------------------*/
        gobj_panel.parent.gobj_send_event("EV_ON_VIEW_SHOW", {}, gobj_panel);
    }

    function get_container_panel_top_toolbar(gobj_panel)
    {
        /*------------------------------------------*
         *      Top Toolbar of "Container Panel"
         *------------------------------------------*/
        var top_toolbar = {
            view:"toolbar",
            id: build_name(gobj_panel, "ct_panel_top_toolbar"), // HACK build_name() es el de aquí
            hidden: gobj_panel.config.panel_properties.with_panel_top_toolbar?false:true,
            css: "toolbar2color",
            height: 30,
            cols: [
                {
                    view:"icon",
                    hidden: gobj_panel.config.panel_properties.with_panel_resize_btn?false:true,
                    icon: "far fa-expand-alt",
                    tooltip: t("enlarge"),
                    click: function() {
                        if(gobj_panel.parent.config.mode == "horizontal") {
                            var gravity = gobj_panel.config.$ui.config.gravity;
                            gravity++;
                            gobj_panel.config.ui_properties["gravity"] = gravity;

                            gobj_panel.config.$ui.define({gravity:gravity});
                            if(gobj_panel.config.$ui.refresh) {
                                gobj_panel.config.$ui.refresh();
                            } else if(gobj_panel.config.$ui.resize) {
                                gobj_panel.config.$ui.resize();
                            }
                        } else {
                            var height = gobj_panel.config.$ui.$height;
                            height += 40;
                            gobj_panel.config.$ui.define({height:height});
                            if(gobj_panel.config.$ui.resize) {
                                gobj_panel.config.$ui.resize();
                            } else if(gobj_panel.config.$ui.refresh) {
                                gobj_panel.config.$ui.refresh();
                            }
                        }
                        gobj_panel.parent.gobj_send_event("EV_REFRESH", {}, gobj_panel);
                    }
                },
                {
                    view:"icon",
                    hidden: gobj_panel.config.panel_properties.with_panel_resize_btn?false:true,
                    icon: "far fa-compress-alt",
                    tooltip: t("narrow"),
                    click: function() {
                        if(gobj_panel.parent.config.mode == "horizontal") {
                            var gravity = gobj_panel.config.$ui.config.gravity;
                            gravity--;

                            if(gravity>0) {
                                gobj_panel.config.ui_properties["gravity"] = gravity;
                                gobj_panel.config.$ui.define({gravity:gravity});
                                if(gobj_panel.config.$ui.refresh) {
                                    gobj_panel.config.$ui.refresh();
                                } else if(gobj_panel.config.$ui.resize) {
                                    gobj_panel.config.$ui.resize();
                                }
                            }
                        } else {
                            var height = gobj_panel.config.$ui.$height;
                            height -= 40;
                            if(height > 100) {
                                gobj_panel.config.$ui.define({height:height});
                                if(gobj_panel.config.$ui.resize) {
                                    gobj_panel.config.$ui.resize();
                                } else if(gobj_panel.config.$ui.refresh) {
                                    gobj_panel.config.$ui.refresh();
                                }
                            }
                        }
                        gobj_panel.parent.gobj_send_event("EV_REFRESH", {}, gobj_panel);
                    }
                },
                {gravity:1},
                {
                    view: "label",
                    gravity: 10,
                    hidden: gobj_panel.config.panel_properties.with_panel_title?false:true,
                    label: gobj_panel.config.panel_properties.with_panel_title?
                        gobj_panel.config.panel_properties.with_panel_title:"",
                    click: function() {
                    }
                },
                {gravity:1},
                {
                    view:"icon",
                    hidden: gobj_panel.config.panel_properties.with_panel_fullscreen_btn?false:true,
                    icon: "fas fa-expand-wide",
                    tooltip: t("fullscreen"),
                    click: function() {
                        gobj_panel.parent.gobj_send_event(
                            "EV_SET_FULLSCREEN",
                            {
                                title: gobj_panel.config.panel_properties.with_panel_title
                            },
                            gobj_panel
                        );
                    }
                },
                {
                    view:"icon",
                    hidden: gobj_panel.config.panel_properties.with_panel_hidden_btn?false:true,
                    icon:"far fa-window-minimize",
                    tooltip: t("minimize"),
                    click: function() {
                        /*----------------------------*
                         *  Minimize, hide the panel
                         *----------------------------*/
                        this.getParentView().getParentView().hide();

                        /*----------------------------------------------*
                         *  Inform of view state to "Container Panel"
                         *----------------------------------------------*/
                        gobj_panel.parent.gobj_send_event("EV_ON_VIEW_SHOW", {}, gobj_panel);
                    }
                }
            ]
        };
        return top_toolbar;
    }

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.Ui_container = Ui_container;
    exports.get_container_panel_top_toolbar = get_container_panel_top_toolbar;
    exports.toggle_container_panel = toggle_container_panel;

})(this);
