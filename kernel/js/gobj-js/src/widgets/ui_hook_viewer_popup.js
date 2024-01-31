/***********************************************************************
 *          ui_hook_viewer_popup.js
 *
 *          Hook Viewer as popup
 *
 *          Copyright (c) 2021 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    "use strict";

    /********************************************
     *      Configuration (C attributes)
     ********************************************/
    var CONFIG = {
        treedb_name: "",
        parent_topic_name: "",
        child_topic_name: "",
        child_field_name: "",
        child_field_value: "",
        click_x: 0,
        click_y: 0,

        $ui: null
    };




            /***************************
             *      Local Methods
             ***************************/




    /************************************************************
     *   Build name
     ************************************************************/
    function build_name(self, name)
    {
        return self.gobj_escaped_short_name() + "-" + name;
    }

    /************************************************************
     *   Webix UI
     ************************************************************/
    function build_webix(self)
    {
        self.config.$ui = webix.ui({
            view: "popup",
            left: self.config.click_x,
            top: self.config.click_y,
            body: {
                id: build_name(self, "menu"),
                view: "list",
                template: "#id#",
                autoheight: true,
                select: false,
                click: function(id, e, node) {
                    //self.gobj_send_event("EV_CREATE_VERTEX", {topic_name: id}, self);
                    //this.hide();
                }
            },
            on: {
                onHide: function() {
                    __yuno__.gobj_destroy(self);
                }
            }
        });
        self.config.$ui.show();
        self.config.$ui.gobj = self;
    }




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *
     ********************************************/
    function ac_select(self, event, kw, src)
    {

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_refresh(self, event, kw, src)
    {

        return 0;
    }




            /***************************
             *      GClass/Machine
             ***************************/




    var FSM = {
        "event_list": [
            "EV_TOGGLE",
            "EV_SELECT",
            "EV_REFRESH"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["EV_TOGGLE",               0,                  undefined],
                ["EV_SELECT",               ac_select,          undefined],
                ["EV_REFRESH",              ac_refresh,         undefined]
            ]
        }
    };

    var Ui_hook_viewer_popup = GObj.__makeSubclass__();
    var proto = Ui_hook_viewer_popup.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Ui_hook_viewer_popup",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Ui_hook_viewer_popup, "Ui_hook_viewer_popup");




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

        var child_data = treedb_get_topic_data(
            self.config.treedb_name,
            self.config.child_topic_name
        );
        var child_field_name = self.config.child_field_name;
        var child_field_value = self.config.child_field_value;

        var load_data = [];
        for(var id in child_data) {
            var record = child_data[id];
            var ids = kwid_get_ids(record[child_field_name]);
            if(elm_in_list(child_field_value, ids)) {
                load_data.push(record);
            }
        }

        var $menu = $$(build_name(self, "menu"));
        $menu.parse(load_data);
    }

    /************************************************
     *      Framework Method destroy
     *      In this point, all childs
     *      and subscriptions are already deleted.
     ************************************************/
    proto.mt_destroy = function()
    {
        var self = this;

        if(self.config.$ui) {
            self.config.$ui.destructor();
            self.config.$ui = 0;
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
    exports.Ui_hook_viewer_popup = Ui_hook_viewer_popup;

})(this);
