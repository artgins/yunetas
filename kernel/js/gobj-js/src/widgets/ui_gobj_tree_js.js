/***********************************************************************
 *          ui_gobj_tree_js.js
 *
 *          JS Gobj's tree UI
 *
 *
 *  Version
 *  -------
 *  1.0     Initial release
 *
 *          Copyright (c) 2020 Niyamaka
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    "use strict";

    /********************************************
     *      Configuration (C attributes)
     ********************************************/
    var CONFIG = {
        gobj_container: null,
        gobj_tree: null,
        gobj_attrs: null,
        gobj_graph: null,
        timeout_request: 1000,
        data: null, // copia local de los datos visualizándose

        $ui: null
    };

    /************************************************************
     *   Schema
     ************************************************************/
    var attrs_cols = [
        {
            "id": "name",
            "header": "Attribute Name",
            "fillspace": 15,
            "type": "string",
            "flag": [
                "persistent",
                "required"
            ]
        },
        {
            "id": "type",
            "header": "Type",
            "fillspace": 8,
            "type": "string",
            "flag": [
                "persistent",
                "required"
            ]
        },
        {
            "id": "value",
            "header": "Current Value",
            "fillspace": 15,
            "type": "string",
            "flag": [
                "persistent",
                "required"
            ]
        },
        {
            "id": "stats",
            "header": "Stats",
            "fillspace": 5,
            "type": "string",
            "flag": [
                "persistent",
                "required"
            ]
        },
        {
            "id": "description",
            "header": "Description",
            "fillspace": 30,
            "type": "string",
            "flag": [
                "persistent",
                "required"
            ]
        }
    ];




            /***************************
             *      Local Methods
             ***************************/




    /********************************************
     *
     ********************************************/
    function build_toolbar(self, mode)
    {
        var elements = [
            {
                view:"button",
                type: "icon",
                icon: "fas fa-border-center-v",
                autowidth: true,
                css: "webix_transparent icon_toolbar_16",
                tooltip: t("view mode: horizontal/vertical"),
                label: t("view mode"),
                click: function() {
                    self.config.gobj_container.gobj_send_event(
                        "EV_CHANGE_MODE",
                        {
                        },
                        self
                    );
                    refresh_gobj_tree(self, false);
                    if(self.config.gobj_container.gobj_read_attr("mode") == "horizontal") {
                        this.define("icon", "fas fa-border-center-v");
                    } else {
                        this.define("icon", "fas fa-border-center-h");
                    }
                    this.refresh();
                }
            },
            {
                view: "button",
                type: "icon",
                icon: "fas fa-sync",
                autowidth: true,
                css: "webix_transparent icon_toolbar_16",
                tooltip: t("refresh"),
                label: t("refresh"),
                click: function() {
                    refresh_gobj_tree(self, true);
                }
            },
            {},
            {
                view: "button",
                autowidth: true,
                css: "webix_transparent",
                label: t("Graph"),
                click: function() {
                    var $widget = self.config.gobj_graph.gobj_read_attr("$ui");
                    if($widget.isVisible()) {
                        $widget.hide();
                    } else {
                        $widget.show();
                    }
                }
            },
            {
                view: "button",
                autowidth: true,
                css: "webix_transparent",
                label: t("Attributes"),
                click: function() {
                    var $widget = self.config.gobj_attrs.gobj_read_attr("$ui");
                    if($widget.isVisible()) {
                        $widget.hide();
                    } else {
                        $widget.show();
                    }
                }
            },
            {
                view: "button",
                autowidth: true,
                css: "webix_transparent",
                label: t("Tree"),
                click: function() {
                    var $widget = self.config.gobj_tree.gobj_read_attr("$ui");
                    if($widget.isVisible()) {
                        $widget.hide();
                    } else {
                        $widget.show();
                    }
                }
            }
        ];

        var toolbar = {
            view: "toolbar"
            //css: "toolbar2color"
        };
        var scrollview = {
            view: "scrollview",
            scroll: "x",
            body: toolbar
        };
        if(mode == "vertical") {
            toolbar["width"] = 40;
            toolbar["rows"] = elements;
        } else {
            toolbar["height"] = 40;
            toolbar["cols"] = elements;
        }
        return scrollview;
    }

    /********************************************
     *
     ********************************************/
    function refresh_gobj_tree(self, clear)
    {
        var data = self.config.data = __yuno__.gobj_list_gobj_tree(__yuno__);

        if(clear) {
            self.config.gobj_tree.gobj_send_event(
                "EV_CLEAR_DATA",
                {
                },
                self
            );
            self.config.gobj_attrs.gobj_send_event(
                "EV_CLEAR_DATA",
                {
                },
                self
            );
            self.config.gobj_graph.gobj_send_event(
                "EV_CLEAR_DATA",
                {
                },
                self
            );
        }
        self.config.gobj_tree.gobj_send_event(
            "EV_LOAD_DATA",
            {
                data: data
            },
            self
        );
        self.config.gobj_graph.gobj_send_event(
            "EV_LOAD_DATA",
            {
                type: "webix-tree",
                data: data
            },
            self
        );

        self.config.gobj_tree.gobj_send_event(
            "EV_SELECT_ITEM",
            {id:data[0].id},
            self
        );
    }




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *
     ********************************************/
    function ac_tree_item_selected(self, event, kw, src)
    {
        var gobj_id = kw.id;
        var gobj_ijs = __yuno__.gobj_find_unique_gobj(gobj_id);
        if(!gobj_ijs) {
            gobj_ijs = __yuno__.gobj_find_gobj(gobj_id);
        }
        if(gobj_ijs) {
            self.config.gobj_attrs.gobj_send_event(
                "EV_CLEAR_DATA",
                {
                },
                self
            );
            var data = __yuno__.gobj_list_gobj_attr(gobj_ijs);
            if(gobj_ijs.config.$ui) {
                data.push({
                    name: '__visible__',
                    type: 'boolean',
                    description: '',
                    stats: 0,
                    value: gobj_ijs.config.$ui.isVisible()
                });
            }
            self.config.gobj_attrs.gobj_send_event(
                "EV_LOAD_DATA",
                data,
                self
            );

            self.config.gobj_graph.gobj_send_event(
                "EV_SELECT_ITEM",
                {id:gobj_id},
                self
            );
        } else {
            log_error("gobj not found: '" + gobj_id + "'");
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_mx_vertex_clicked(self, event, kw, src)
    {
        var id = kw.id;
        var value = kw.value;

        self.config.gobj_tree.gobj_send_event(
            "EV_SELECT_ITEM",
            {id:id},
            self
        );
    }

    /********************************************
     *
     ********************************************/
    function ac_timeout_request(self, event, kw, src)
    {
        refresh_gobj_tree(self, false);
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
            "EV_TREE_ITEM_SELECTED",
            "EV_MX_VERTEX_CLICKED",
            "EV_MX_EDGE_CLICKED",
            "EV_TIMEOUT",
            "EV_SELECT",
            "EV_REFRESH"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["EV_TREE_ITEM_SELECTED",   ac_tree_item_selected,  undefined],
                ["EV_MX_VERTEX_CLICKED",    ac_mx_vertex_clicked,   undefined],
                ["EV_MX_EDGE_CLICKED",      undefined,              undefined],
                ["EV_TIMEOUT",              ac_timeout_request,     undefined],
                ["EV_SELECT",               ac_select,              undefined],
                ["EV_REFRESH",              ac_refresh,             undefined]
            ]
        }
    };

    var Ui_gobj_tree_js = GObj.__makeSubclass__();
    var proto = Ui_gobj_tree_js.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Ui_gobj_tree_js",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Ui_gobj_tree_js, "Ui_gobj_tree_js");




            /***************************
             *      Framework Methods
             ***************************/




    /************************************************
     *      Framework Method create
     ************************************************/
    proto.mt_create = function(kw)
    {
        var self = this;

        /*
         *  Create container
         */
        self.config.gobj_container = self.yuno.gobj_create_unique(
            self.name + ".ct",
            Ui_container,
            {
                mode: "horizontal"
            },
            self
        );

        self.config.gobj_container.gobj_send_event(
            "EV_ADD_TOOLBAR",
            {
                type: "container_top_toolbar",
                toolbar: build_toolbar(self, "horizontal")
            },
            self
        );

        /*
         *  Create mxgraph js gobj tree
         */
        self.config.gobj_graph = self.yuno.gobj_create_unique(
            self.name + ".mx",
            Mx_webix_tree,
            {
                is_pinhold_window: false,
                ui_properties: {
                    gravity: 4,
                    minWidth: 300,
                    minHeight: 300
                },
                panel_properties: {
                    with_panel_top_toolbar: true,
                    with_panel_title: t("Graph"),
                    with_panel_hidden_btn: false,
                    with_panel_fullscreen_btn: true,
                    with_panel_resize_btn: true
                }
            },
            self.config.gobj_container
        );

        /*
         *  Create attributes table
         */
        self.config.gobj_attrs = self.yuno.gobj_create_unique(
            self.name + ".tb",
            Ui_treedb_formtable,
            {
                is_pinhold_window: false,
                ui_properties: {
                    gravity: 2,
                    hidden:true,
                    minWidth: 300,
                    minHeight: 300
                },
                schema: attrs_cols,
                with_checkbox: false,
                with_textfilter: false,
                with_sort: true,
                with_top_title: false,
                with_footer: true,

                panel_properties: {
                    with_panel_top_toolbar: true,
                    with_panel_title: t("Attributes"),
                    with_panel_hidden_btn: true,
                    with_panel_fullscreen_btn: true,
                    with_panel_resize_btn: true
                }
            },
            self.config.gobj_container
        );

        /*
         *  Create webix tree
         */
        self.config.gobj_tree = self.yuno.gobj_create_unique(
            self.name + ".wt",
            Ui_webix_tree,
            {
                is_pinhold_window: false,
                ui_properties: {
                    gravity: 1,
                    hidden:true,
                    minWidth: 300,
                    minHeight: 300
                },
                panel_properties: {
                    with_panel_top_toolbar: true,
                    with_panel_title: t("Tree"),
                    with_panel_hidden_btn: true,
                    with_panel_fullscreen_btn: true,
                    with_panel_resize_btn: true
                }
            },
            self.config.gobj_container
        );

        self.config.$ui = self.config.gobj_container.gobj_read_attr("$ui");
        self.config.gobj_container.gobj_start_tree();
    }

    /************************************************
     *      Framework Method destroy
     *      In this point, all childs
     *      and subscriptions are already deleted.
     ************************************************/
    proto.mt_destroy = function()
    {
        var self = this;
    }

    /************************************************
     *      Framework Method start
     ************************************************/
    proto.mt_start = function(kw)
    {
        var self = this;

        // Pinta los datos después de un timeout
        self.set_timeout(self.config.timeout_request);
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
    exports.Ui_gobj_tree_js = Ui_gobj_tree_js;

})(this);
