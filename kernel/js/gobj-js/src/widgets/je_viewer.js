/***********************************************************************
 *          je_viewer.js
 *
 *          Mix "Container Panel" & "Pinhold Window"
 *
 *          Viewer of json with jsoneditor
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
        //////////////// Common Attributes //////////////////
        is_pinhold_window: true,// CONF: Select default: window or container panel
        panel_properties: {},   // CONF: creator can set "Container Panel" properties
        window_properties: {},  // CONF: creator can set "Pinhold Window" properties
        ui_properties: null,    // CONF: creator can set webix properties
        window_image: "",       // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"
        window_title: "",       // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"
        left: 0,                // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"
        top: 0,                 // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"
        width: 600,             // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"
        height: 500,            // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"

        $ui: null,
        subscriber: null,       // Subscriber of published events, by default the parent.
        $ui_fullscreen: null,   // Which part of window will be fullscreened "Pinhold Window"
        resizing_event_id: null,// Used for automatic_resizing by window
        pinpushed: false,       // Used by pinhold_window_top_toolbar "Pinhold Window"

        //////////////// Particular Attributes //////////////////
        _jsoneditor: null,
        data: null,

        //////////////////////////////////
        __writable_attrs__: [
            ////// Common /////
            "window_title",
            "window_image",
            "left",
            "top",
            "width",
            "height",
            "pinpushed"

            ////// Particular /////
        ]
    };




            /***************************
             *      Local Methods
             ***************************/




    /************************************************************
     *   Rebuild
     ************************************************************/
    function rebuild(self)
    {
        if(self.config.$ui) {
            self.config.$ui.destructor();
            self.config.$ui = 0;
        }
        if(self.config._jsoneditor) {
            self.config._jsoneditor.destroy();
            self.config._jsoneditor = null;
        }
        build_webix(self);
        self.config._jsoneditor = $$(build_name(self, "jsoneditor")).getJsoneditor();

        initialize_jsoneditor(self);
    }

    /************************************************************
     *   Webix UI
     ************************************************************/
    function build_webix(self)
    {
        /*---------------------------------------*
         *      Particular UI code
         *---------------------------------------*/
        /*---------------------------------------*
         *      Bottom Toolbar
         *---------------------------------------*/
        var bottom_toolbar = {
            view:"toolbar",
            height: 30,
            css: "toolbar2color",
            cols:[
                {
                    view: "button",
                    type: "image",
                    image: "/static/app/images/yuneta/json_graph.svg",
                    autowidth: true,
                    css: "webix_transparent icon_toolbar_16",
                    tooltip: t("graph view"),
                    label: t("graph view"),
                    click: function() {
                        var name = "Graph of Json: " + self.name;
                        var gobj_mx_json_viewer = self.yuno.gobj_find_unique_gobj(name);
                        if(gobj_mx_json_viewer) {
                            gobj_mx_json_viewer.gobj_send_event("EV_TOGGLE", {}, self);
                            return 0;
                        }
                        gobj_mx_json_viewer = self.yuno.gobj_create_unique(
                            name,
                            Mx_json_viewer,
                            {
                                window_title: name
                            },
                            self
                        );
                        gobj_mx_json_viewer.gobj_send_event(
                            "EV_LOAD_DATA",
                            {
                                path: "`", // HACK path hardcoded
                                data: self.config.data
                            },
                            self
                        );
                    }
                }
            ]
        };



        /*----------------------------------------------------*
         *                      UI
         *  Common UI of Pinhold Window and Container Panel
         *----------------------------------------------------*/
        if(self.config.is_pinhold_window) {
            /*-------------------------*
             *      Pinhold Window
             *-------------------------*/
            self.config.$ui = webix.ui({
                view: "window",
                id: self.gobj_escaped_short_name(), // HACK can be a global gobj, use gclass_name+name
                top: self.config.top,
                left: self.config.left,
                width: self.config.width,
                height: self.config.height,
                hidden: self.config.pinpushed?true:false,
                move: true,
                resize: true,
                position: (self.config.left==0 && self.config.top==0)?"center":null,
                head: get_window_top_toolbar(self),
                body: {
                    id: build_name(self, "fullscreen"),
                    ////////////////// REPEATED webix code /////////////////
                    // WARNING Please, put your code outside, here only simple variable names
                    rows: [
                        get_container_panel_top_toolbar(self),
                        {
                            view: "jsoneditor",
                            id: build_name(self, "jsoneditor"),
                            gobj: self
                        },
                        bottom_toolbar
                    ]
                    ////////////////// webix code /////////////////
                },
                on: {
                    "onViewResize": function() {
                        self.config.left = this.gobj.config.$ui.config.left;
                        self.config.top = this.gobj.config.$ui.config.top;
                        self.config.width = this.gobj.config.$ui.config.width;
                        self.config.height = this.gobj.config.$ui.config.height;
                        self.gobj_save_persistent_attrs();
                    },
                    "onViewMoveEnd": function() {
                        self.config.left = this.gobj.config.$ui.config.left;
                        self.config.top = this.gobj.config.$ui.config.top;
                        self.config.width = this.gobj.config.$ui.config.width;
                        self.config.height = this.gobj.config.$ui.config.height;
                        self.gobj_save_persistent_attrs();
                    }
                }
            });
            self.config.$ui_fullscreen = $$(build_name(self, "fullscreen"));

        } else {
            /*-------------------------*
             *      Container Panel
             *-------------------------*/
            self.config.$ui = webix.ui({
                id: self.gobj_name(),
                ////////////////// REPEATED webix code /////////////////
                // WARNING Please, put your code outside, here only simple variable names
                rows: [
                    get_container_panel_top_toolbar(self),
                    {
                        view: "jsoneditor",
                        id: build_name(self, "jsoneditor"),
                        gobj: self
                    },
                    bottom_toolbar
                ]
                ////////////////// webix code /////////////////
            });
        }
        self.config.$ui.gobj = self;

        if(self.config.ui_properties) {
            self.config.$ui.define(self.config.ui_properties);
            if(self.config.$ui.refresh) {
                self.config.$ui.refresh();
            }
        }

        /*----------------------------------------------*
         *  Inform of panel viewed to "Container Panel"
         *----------------------------------------------*/
        if(!self.config.is_pinhold_window) {
            self.config.$ui.attachEvent("onViewShow", function() {
                self.parent.gobj_send_event("EV_ON_VIEW_SHOW", self, self);
            });
        }

        /*----------------------------------------------*
         *  Set fullscreen ui in "Pinhold Window"
         *----------------------------------------------*/
        if(self.config.is_pinhold_window) {
            self.config.$ui_fullscreen = $$(build_name(self, "fullscreen"));
            automatic_resizing_cb(); // Adapt window size to device
        }

        /*---------------------------------------*
         *   Automatic Resizing in "Pinhold Window"
         *---------------------------------------*/
        function automatic_resizing(gadget, window_width, window_height)
        {
            var $gadget = $$(gadget);
            var new_width = -1;
            var new_height = -1;
            var new_x = $gadget.config.left;
            var new_y = $gadget.config.top;

            if($gadget.$width + new_x > window_width) {
                new_width = window_width;
                new_x = 0;
            }
            if($gadget.$height + new_y > window_height) {
                new_height = window_height;
                new_y = 0;
            }

            if(new_width < 0 && new_height < 0) {
                return;
            }

            $gadget.config.width = new_width<0? $gadget.$width:new_width,
            $gadget.config.height = new_height<0? $gadget.$height:new_height;
            $gadget.resize();
            $gadget.setPosition(new_x, new_y);
        }

        function automatic_resizing_cb()
        {
            var window_width = window.innerWidth-8;
            var window_height = window.innerHeight-8;
            automatic_resizing(self.gobj_escaped_short_name(), window_width, window_height);
        }

        if(self.config.is_pinhold_window) {
            if(self.config.resizing_event_id) {
                webix.eventRemove(self.config.resizing_event_id);
                self.config.resizing_event_id = 0;
            }
            self.config.resizing_event_id = webix.event(window, "resize", automatic_resizing_cb);
        }
    }

    /********************************************
     *
     ********************************************/
    function initialize_jsoneditor(self)
    {
        var jsoneditor = self.config._jsoneditor;
    }




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *  WARNING return boolean!
     *  Return true if field is editable
     *  kw: {path, field, value}
     ********************************************/
    function ac_je_is_field_editable(self, event, kw, src)
    {
//         var path = kw.path?kw.path.join("`"):kw.field;
//         trace_msg("is_editable: " + path);
//         //trace_msg(kw);
        return false;
    }

    /********************************************
     *  kw: {path, field, value, event}
     *  value undefined when is over key
     ********************************************/
    function ac_je_click(self, event, kw, src)
    {
//         var path = kw.path?kw.path.join("`"):kw.field;
//         trace_msg("click: " + path);
//         //trace_msg(kw);

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_load_data(self, event, kw, src)
    {
        self.config.data = kw.data;
        self.config._jsoneditor.set(kw.data);

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_update_data(self, event, kw, src)
    {
        self.config._jsoneditor.update(kw.data);

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_clear_data(self, event, kw, src)
    {
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_select_item(self, event, kw, src)
    {
        self.config._jsoneditor.setSelection({path: kw.data});

        return 0;
    }

    /********************************************
     *  Top toolbar informing of window close
     *  kw
     *      {destroying: true}   Window destroying
     *      {destroying: false}  Window minifying
     ********************************************/
    function ac_close_window(self, event, kw, src)
    {
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_toggle(self, event, kw, src)
    {
        if(self.config.$ui.isVisible()) {
            self.config.$ui.hide();
        } else {
            self.config.$ui.show();
        }
        return self.config.$ui.isVisible();
    }

    /********************************************
     *
     ********************************************/
    function ac_show(self, event, kw, src)
    {
        self.config.$ui.show();
        return self.config.$ui.isVisible();
    }

    /********************************************
     *
     ********************************************/
    function ac_hide(self, event, kw, src)
    {
        self.config.$ui.hide();
        return self.config.$ui.isVisible();
    }

    /********************************************
     *
     ********************************************/
    function ac_select(self, event, kw, src)
    {
        return 0;
    }

    /*************************************************************
     *  Refresh, order from container
     *  provocado por entry/exit de fullscreen
     *  o por redimensionamiento del panel, propio o de hermanos
     *************************************************************/
    function ac_refresh(self, event, kw, src)
    {
        return 0;
    }

    /********************************************
     *  "Container Panel"
     *  Order from container (parent): re-create
     ********************************************/
    function ac_rebuild_panel(self, event, kw, src)
    {
        rebuild(self);
        return 0;
    }




            /***************************
             *      GClass/Machine
             ***************************/




    var FSM = {
        "event_list": [
            "JE_IS_FIELD_EDITABLE",
            "JE_CLICK",
            "EV_LOAD_DATA",
            "EV_UPDATE_DATA",
            "EV_CLEAR_DATA",
            "EV_CLOSE_WINDOW",
            "EV_TOGGLE",
            "EV_SHOW",
            "EV_HIDE",
            "EV_SELECT",
            "EV_REFRESH",
            "EV_REBUILD_PANEL"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["JE_IS_FIELD_EDITABLE",    ac_je_is_field_editable,    undefined],
                ["JE_CLICK",                ac_je_click,                undefined],
                ["EV_LOAD_DATA",            ac_load_data,               undefined],
                ["EV_UPDATE_DATA",          ac_update_data,             undefined],
                ["EV_CLEAR_DATA",           ac_clear_data,              undefined],
                ["EV_CLOSE_WINDOW",         ac_close_window,            undefined],
                ["EV_TOGGLE",               ac_toggle,                  undefined],
                ["EV_SHOW",                 ac_show,                    undefined],
                ["EV_HIDE",                 ac_hide,                    undefined],
                ["EV_SELECT",               ac_select,                  undefined],
                ["EV_REFRESH",              ac_refresh,                 undefined],
                ["EV_REBUILD_PANEL",        ac_rebuild_panel,           undefined]
            ]
        }
    };

    var Je_viewer = GObj.__makeSubclass__();
    var proto = Je_viewer.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Je_viewer",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Je_viewer, "Je_viewer");




            /***************************
             *      Framework Methods
             ***************************/




    /************************************************
     *      Framework Method create
     ************************************************/
    proto.mt_create = function(kw)
    {
        var self = this;

        var subscriber = self.gobj_read_attr("subscriber");
        if(!subscriber)
            subscriber = self.gobj_parent();
        self.gobj_subscribe_event(null, null, subscriber);

        rebuild(self);
    }

    /************************************************
     *      Framework Method destroy
     *      In this point, all childs
     *      and subscriptions are already deleted.
     ************************************************/
    proto.mt_destroy = function()
    {
        var self = this;
        if(self.config._jsoneditor) {
            self.config._jsoneditor.destroy();
            self.config._jsoneditor = null;
        }
        if(self.config.$ui) {
            self.config.$ui.destructor();
            self.config.$ui = 0;
        }
        if(self.config.resizing_event_id) {
            webix.eventRemove(self.config.resizing_event_id);
            self.config.resizing_event_id = 0;
        }
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

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.Je_viewer = Je_viewer;

})(this);
