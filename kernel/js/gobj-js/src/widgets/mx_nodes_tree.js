/***********************************************************************
 *          mx_nodes_tree.js
 *
 *          Mix "Container Panel" & "Pinhold Window"
 *
 *          Treedb's Nodes Tree Manager
 *
 *
 *  Each Vertex Cell contains in his value a node of a treedb topic:

    {
        cell_name: null,        // cell id: treedb_name`topic_name^record_id, null in new records
        schema: schema,         // Schema of topic
        record: null,           // Data of node
    },

 *
 *
 *
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
        is_pinhold_window:true, // CONF: Select default: window or container panel
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
        with_treedb_tables: false,
        descs: null,        // all treedb topic's desc
        treedb_name: null,  // treedb editing
        topics: [],  // topics editing
        topics_style: [],

        uuid: null, // to publish and avod feedback loops
        lock_publish_geometry: false,

        locked: true,
        fitted: false,
        collapsed: true,
        foldable_icon_size: 16,
        top_overlay_icon_size: 24,
        image_json_graph: null,
        image_formtable: null,
        image_data_in_disk: null,
        image_data_in_memory: null,
        image_data_on_moving: null,
        image_save_red: null,
        image_run: null,
        image_delete: null,
        image_clone: null,
        image_collapsed: null,
        image_expanded: null,

        // port_position: top, bottom, left, right
        hook_port_position: "bottom",
        fkey_port_position: "top",
        hook_port_cx: 20,
        hook_port_cy: 20,
        fkey_port_cx: 20,
        fkey_port_cy: 20,
        cell_cx_sep: 100,
        cell_cy_sep: 100,

        layout_options: [
            {
                id: "no_layout",
                value: "No Layout",
                layout: function(layout_option, graph) {
                    return null;
                }
            }
        ],

        layout_selected: "no_layout",

        _mxgraph: null,

        //////////////////////////////////
        __writable_attrs__: [
            ////// Common /////
            "window_title",
            "window_image",
            "left",
            "top",
            "width",
            "height",

            ////// Particular /////
            "layout_selected",
            "fitted",
            "collapsed"
        ]
    };




            /***************************
             *      Local Methods
             ***************************/




    /************************************************************
     *
     ************************************************************/
    function build_cell_name(self, topic_name, id)
    {
        return self.config.treedb_name + "'" + topic_name + "'" + id;
    }

    /************************************************************
     *
     ************************************************************/
    function load_icons(self)
    {
        /*
         *  Load control button images
         */
        self.config.image_folder_tree = new mxImage(
            '/static/app/images/yuneta/folder-tree.svg',
            self.config.top_overlay_icon_size, self.config.top_overlay_icon_size
        );
        self.config.image_json_graph = new mxImage(
            '/static/app/images/yuneta/json_graph.svg',
            self.config.top_overlay_icon_size, self.config.top_overlay_icon_size
        );
        self.config.image_formtable = new mxImage(
            '/static/app/images/yuneta/formtable.svg',
            self.config.top_overlay_icon_size, self.config.top_overlay_icon_size
        );
        self.config.image_data_in_disk = new mxImage(
            '/static/app/images/yuneta/threads.svg',
            self.config.top_overlay_icon_size, self.config.top_overlay_icon_size
        );
        self.config.image_data_in_memory = new mxImage(
            '/static/app/images/yuneta/pull_down.svg',
            self.config.top_overlay_icon_size, self.config.top_overlay_icon_size
        );
        self.config.image_data_on_moving = new mxImage(
            '/static/app/images/yuneta/sequence.svg',
            self.config.top_overlay_icon_size, self.config.top_overlay_icon_size
        );
        self.config.image_run = new mxImage(
            '/static/app/images/yuneta/instance_running.svg',
            self.config.top_overlay_icon_size, self.config.top_overlay_icon_size
        );
        self.config.image_save_red = new mxImage(
            '/static/app/images/yuneta/save_red.svg',
            self.config.top_overlay_icon_size, self.config.top_overlay_icon_size
        );
        self.config.image_delete = new mxImage(
            '/static/app/images/yuneta/delete.svg',
            self.config.top_overlay_icon_size, self.config.top_overlay_icon_size
        );
        self.config.image_clone = new mxImage(
            '/static/app/images/yuneta/add_point.svg',
            self.config.top_overlay_icon_size, self.config.top_overlay_icon_size
        );
        self.config.image_collapsed = new mxImage(
            '/static/app/images/yuneta/plus-square.svg',
            self.config.foldable_icon_size, self.config.foldable_icon_size
        );
        self.config.image_expanded = new mxImage(
            '/static/app/images/yuneta/minus-square.svg',
            self.config.foldable_icon_size, self.config.foldable_icon_size
        );
    }

    /************************************************************
     *   Rebuild
     ************************************************************/
    function rebuild(self)
    {
        if($$(build_name(self, "topics_menu_popup"))) {
            $$(build_name(self, "topics_menu_popup")).destructor();
        }
        if($$(build_name(self, "cell_menu_popup"))) {
            $$(build_name(self, "cell_menu_popup")).destructor();
        }

        if(self.config.$ui) {
            self.config.$ui.destructor();
            self.config.$ui = 0;
        }
        if(self.config._mxgraph) {
            self.config._mxgraph.destroy();
            self.config._mxgraph = null;
        }
        build_webix(self);
        self.config._mxgraph = $$(build_name(self, "mxgraph")).getMxgraph();

        initialize_mxgraph(self);
   }

    /************************************************************
     *   Webix UI
     ************************************************************/
    function build_webix(self)
    {
        /*---------------------------------------*
         *      Particular UI code
         *---------------------------------------*/
        /*
         *  Once inside a fullscreen, you CANNOT set fullscreen AGAIN
         *  You can check if it's already in fullscreen with:

            if(this.getTopParentView().config.fullscreen) {
                // En fullscreen es un desastre si reconstruimos webix
                // o volvemos a poner en fullscreen.
                // Solo puede existir un $ui en fullscreen a la vez!!
            }
         */

        /*---------------------------------------*
         *      Menu for treedb topics
         *---------------------------------------*/
        webix.ui({
            view: "popup",
            id: build_name(self, "topics_menu_popup"),
            width: 200,
            body: {
                id: build_name(self, "topics_menu"),
                view: "menu",
                layout: "y",
                template: "#value#",
                autoheight: true,
                select: true,
                click: function(id, e, node) {
                    this.hide();
                    self.gobj_publish_event("EV_SHOW_TREEDB_TOPIC", {topic_name: id}, self);
                }
            },
            on: {
                "onShow": function(e) {
                    $$(build_name(self, "topics_menu")).unselectAll();
                }
            }
        }).hide();

        /*---------------------------------------*
         *      Menu for create nodes
         *---------------------------------------*/
        webix.ui({
            view: "popup",
            id: build_name(self, "cell_menu_popup"),
            width: 200,
            body: {
                view: "menu",
                id: build_name(self, "cell_menu"),
                layout: "y",
                autoheight: true,
                select: true,
                data: [
                    {id: "EV_EXTEND_SIZE", value: "Extend size"}
                ],
                click: function(id, e, node) {
                    this.hide();
                    self.gobj_send_event(
                        id,
                        {
                            cell: $$(build_name(self, "cell_menu_popup")).cell
                        },
                        self
                    );
                }
            },
            on: {
                "onShow": function(e) {
                    $$(build_name(self, "cell_menu")).unselectAll();
                }
            }
        }).hide();

        /*---------------------------------------*
         *      Bottom Toolbar
         *---------------------------------------*/
        var bottom_toolbar = {
            view:"toolbar",
            height: 30,
            css: "toolbar2color",
            cols:[
                {
                    view:"button",
                    type: "icon",
                    hidden: self.config.with_treedb_tables?false:true,
                    icon: "fal fa-table",
                    css: "webix_transparent icon_toolbar_16",
                    label: t("topics"),
                    popup: build_name(self, "topics_menu_popup")
                },
                {
                    view:"button",
                    type: "icon",
                    icon: "far fa-send-back",
                    css: "webix_transparent icon_toolbar_16",
                    maxWidth: 120,
                    label: t("reorder"),
                    click: function() {
                        webix.confirm(
                            {
                                title: t("warning"),
                                text: t("are you sure"),
                                type:"confirm-warning"
                            }).then(function(result) {
                                reordena_graph(self);
                                set_btn_submmit_state(self, "save_graph", true);
                            }
                        );

                    }
                },
                {
                    view:"button",
                    id: build_name(self, "save_graph"),
                    type: "icon",
                    icon: "fas fa-save",
                    css: "webix_transparent icon_toolbar_16",
                    maxWidth: 120,
                    label: t("save"),
                    click: function() {
                        save_graph(self);
                        set_btn_submmit_state(self, "save_graph", false);
                    }
                },
                {
                    view: "richselect",
                    id: build_name(self, "layout_options"),
                    hidden: true,
                    tooltip: t("Select layout"),
                    width: 180,
                    options: self.config.layout_options,
                    value: self.config.layout_selected,
                    label: "",
                    on: {
                        "onChange": function(newVal, oldVal) {
                            var cur_layout = kwid_collect(
                                self.config.layout_options,
                                newVal,
                                null, null
                            )[0];
                            if(!cur_layout) {
                                cur_layout = self.config.layout_options[0];
                            }
                            self.config.layout_selected = cur_layout.id;

                            execute_layout(self);

                            self.gobj_save_persistent_attrs();
                        }
                    }
                },
                {
                    view:"button",
                    type: "icon",
                    icon: "",
                    icon: self.config.fitted? "fad fa-compress-arrows-alt":"fad fa-expand-arrows-alt",
                    css: "webix_transparent icon_toolbar_16",
                    maxWidth: 120,
                    label: self.config.fitted? t("reset view"):t("fit"),
                    click: function() {
                        var graph = self.config._mxgraph;
                        if(self.config.fitted) {
                            self.config.fitted = false;
                            graph.view.scaleAndTranslate(1, graph.border, graph.border);
                            this.define("icon", "fad fa-expand-arrows-alt");
                            this.define("label", t("fit"));
                        } else {
                            graph.fit();
                            self.config.fitted = true;
                            this.define("icon", "fad fa-compress-arrows-alt");
                            this.define("label",  t("reset view"));
                        }
                        this.refresh();
                        self.gobj_save_persistent_attrs();
                    }
                },
                {
                    view:"button",
                    type: "icon",
                    icon: "far fa-search-plus",
                    css: "webix_transparent icon_toolbar_16",
                    maxWidth: 120,
                    label: t("zoom in"),
                    click: function() {
                        var graph = self.config._mxgraph;
                        graph.zoomIn();
                        graph.view.setTranslate(0, 0);
                    }
                },
                {
                    view:"button",
                    type: "icon",
                    icon: "far fa-search-minus",
                    css: "webix_transparent icon_toolbar_16",
                    maxWidth: 120,
                    label: t("zoom out"),
                    click: function() {
                        var graph = self.config._mxgraph;
                        graph.zoomOut();
                        graph.view.setTranslate(0, 0);
                    }
                },
                {
                    view:"button",
                    type: "icon",
                    icon: "",
                    icon: self.config.collapsed? "far fa-plus-square":"far fa-minus-square",
                    css: "webix_transparent icon_toolbar_16",
                    maxWidth: 120,
                    label: self.config.collapsed? t("expand"):t("collapse"),
                    click: function() {
                        if(self.config.collapsed) {
                            self.config.collapsed = false;
                            this.define("icon", "far fa-minus-square");
                            this.define("label", t("collapse"));
                        } else {
                            self.config.collapsed = true;
                            this.define("icon", "far fa-plus-square");
                            this.define("label", t("expand"));
                        }
                        this.refresh();
                        self.gobj_save_persistent_attrs();
                        collapse_edition(self, self.config.collapsed);
                    }
                },
                {
                    view:"button",
                    type: "icon",
                    icon: self.config.locked? "far fa-lock-alt":"far fa-lock-open-alt",
                    css: "webix_transparent icon_toolbar_16",
                    autosize: true,
                    label: self.config.locked? t("unlock vertices"):t("lock vertices"),
                    click: function() {
                        var graph = self.config._mxgraph;
                        if(graph.isCellsLocked()) {
                            unlock_graph(self, graph);
                            this.define("icon", "far fa-lock-open-alt");
                            this.define("label", t("lock vertices"));
                        } else {
                            lock_graph(self, graph);
                            this.define("icon", "far fa-lock-alt");
                            this.define("label", t("unlock vertices"));
                        }
                        this.refresh();
                    }
                },
                { view:"label", label: ""},

                {
                    view: "button",
                    type: "icon",
                    icon: "fas fa-sync",
                    autowidth: true,
                    css: "webix_transparent icon_toolbar_16",
                    tooltip: t("refresh"),
                    label: t("refresh"),
                    click: function() {
                        self.gobj_publish_event("EV_REFRESH_TREEDB", {}, self);
                    }
                },

                {
                    view:"button",
                    type: "icon",
                    icon: "fas fa-folder-tree",
                    css: "webix_transparent icon_toolbar_16",
                    maxWidth: 120,
                    label: t("Mxgraph"),
                    click: function() {
                        var n = "Json Mxgraph Inside: " + self.name;
                        var gobj_je = __yuno__.gobj_find_unique_gobj(n);
                        if(!gobj_je) {
                            gobj_je = __yuno__.gobj_create_unique(
                                n,
                                Je_viewer,
                                {
                                    window_title: n,
                                    width: 900,
                                    height: 600
                                },
                                self
                            );
                            gobj_je.gobj_start();
                        }
                        gobj_je.gobj_send_event(
                            "EV_SHOW",
                            {},
                            self
                        );
                        gobj_je.gobj_send_event(
                            "EV_CLEAR_DATA",
                            {},
                            self
                        );
                        gobj_je.gobj_send_event(
                            "EV_LOAD_DATA",
                            {data: mxgraph2json(self.config._mxgraph)},
                            self
                        );
                    }
                },
                {
                    view:"button",
                    type: "icon",
                    icon: "far fa-question",
                    css: "webix_transparent icon_toolbar_16",
                    maxWidth: 120,
                    label: t("help"),
                    click: function() {
                        var $help_window = $$(build_name(self, "help_window"));
                        if($help_window) {
                            if($help_window.isVisible()) {
                                $help_window.hide();
                            } else {
                                $help_window.show();
                            }
                        }
                    }
                }
            ]
        };

        var mx_events = [
            mxEvent.CLICK,
            mxEvent.DOUBLE_CLICK,
            mxEvent.MOVE_CELLS,
            mxEvent.RESIZE_CELLS,
            mxEvent.ADD_CELLS,
            mxEvent.CONNECT_CELL
        ];

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
                // HACK can be a global gobj, use gclass_name+name
                id: self.gobj_escaped_short_name(),
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
                        {
                            view: "mxgraph",
                            id: build_name(self, "mxgraph"),
                            events: mx_events,
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
                        view: "mxgraph",
                        id: build_name(self, "mxgraph"),
                        all_events: false,
                        events: mx_events,
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
     *  Rebuild layouts
     ********************************************/
    function rebuild_layouts(self)
    {
        for(var i=0; i<self.config.layout_options.length; i++) {
            var layout = self.config.layout_options[i];
            layout.exe = layout.layout(layout, self.config._mxgraph);
        }
    }

    /********************************************
     *  Execute layout
     ********************************************/
    function execute_layout(self)
    {
        var graph = self.config._mxgraph;
        var model = graph.getModel();
        var group = get_layer(self, "layer?");

        var cur_layout = kwid_collect(
            self.config.layout_options,
            self.config.layout_selected,
            null, null
        )[0];
        if(!cur_layout) {
            cur_layout = self.config.layout_options[0];
        }

        var locked = graph.isCellsLocked();
        if(locked) {
            graph.setCellsLocked(false);
        }

        if(cur_layout && cur_layout.exe) {
            model.beginUpdate();
            try {
                cur_layout.exe.execute(group);
            } catch (e) {
                log_error(e);
            } finally {
                model.endUpdate();
            }
        }

        if(self.config.fitted) {
            graph.fit();
        } else {
            graph.view.scaleAndTranslate(1, graph.border, graph.border);
        }
        if(locked) {
            graph.setCellsLocked(true);
        }
    }

    /********************************************
     *
     ********************************************/
    function collapse_edition(self, collapse)
    {
        var graph = self.config._mxgraph;
        var model = graph.getModel();
        var layer = get_layer(self, layer);

        var cells = kw_collect(layer.children, {vertex:true});
        model.beginUpdate();
        try {
            graph.clearSelection();
            graph.foldCells(collapse, false, cells, true);

        } catch (e) {
            log_error(e);
        } finally {
            model.endUpdate();
        }
        if(collapse) {
            // Colapsado no puede resize
            graph.setCellsResizable(false);
        } else {
            graph.setCellsResizable(true);
        }
    }

    /********************************************
     *  Create root and layers
     ********************************************/
    function create_root_and_layers(self)
    {
        var graph = self.config._mxgraph;
        var layers = self.config.layers;
        var root = null;

        root = graph.getModel().createRoot()

        graph.getModel().beginUpdate();
        try {
            graph.getModel().setRoot(root);
        } catch (e) {
            log_error(e);
        } finally {
            graph.getModel().endUpdate();
        }
    }

    /************************************************************
     *
     ************************************************************/
    function get_layer(self, layer_id)
    {
        return self.config._mxgraph.getDefaultParent();
    }

    /********************************************
     *
     ********************************************/
    function create_graph_style(graph, name, s)
    {
        var style = {};
        var list = s.split(";");
        for(var i=0; i<list.length; i++) {
            var sty = list[i];
            if(empty_string(sty)) {
                continue;
            }
            var key_value = sty.split("=");
            if(key_value.length==1) {
                // Without = must be the shape
                style["shape"] = key_value[0];
            } else if(key_value.length==2) {
                style[key_value[0]] = key_value[1];
            } else {
                log_error("create_graph_style() bad style: " + sty);
            }
        }
        graph.getStylesheet().putCellStyle(name, style);
    }

    /********************************************
     *
     ********************************************/
    function configureStylesheet(graph) {
        var style = graph.getStylesheet().getDefaultVertexStyle();
        style[mxConstants.STYLE_SHAPE] = mxConstants.SHAPE_RECTANGLE;
        style[mxConstants.STYLE_PERIMETER] = mxPerimeter.RectanglePerimeter;
        style[mxConstants.STYLE_ALIGN] = mxConstants.ALIGN_CENTER;
        style[mxConstants.STYLE_VERTICAL_ALIGN] = mxConstants.ALIGN_MIDDLE;
        style[mxConstants.STYLE_FONTCOLOR] = '#000000';

        style[mxConstants.STYLE_ROUNDED] = true;
        style[mxConstants.STYLE_OPACITY] = '80';
        style[mxConstants.STYLE_FONTSIZE] = '12';
        style[mxConstants.STYLE_FONTSTYLE] = 0;
        style[mxConstants.STYLE_IMAGE_WIDTH] = '48';
        style[mxConstants.STYLE_IMAGE_HEIGHT] = '48';
        style[mxConstants.STYLE_SHADOW] = false;

        style = graph.getStylesheet().getDefaultEdgeStyle();

//         style[mxConstants.STYLE_EDGE] = mxEdgeStyle.ElbowConnector;
//         style[mxConstants.STYLE_EDGE] = mxEdgeStyle.EntityRelation;
//         style[mxConstants.STYLE_EDGE] = mxEdgeStyle.Loop;
//         style[mxConstants.STYLE_EDGE] = mxEdgeStyle.SideToSide;
//         style[mxConstants.STYLE_EDGE] = mxEdgeStyle.TopToBottom;
//         style[mxConstants.STYLE_EDGE] = mxEdgeStyle.OrthConnector;
//         style[mxConstants.STYLE_EDGE] = mxEdgeStyle.SegmentConnector;

        style[mxConstants.STYLE_EDGE] = mxEdgeStyle.TopToBottom;
        style[mxConstants.STYLE_STROKEWIDTH] = '2';
        style[mxConstants.STYLE_STROKECOLOR] = 'black';

        style[mxConstants.STYLE_ROUNDED] = true;
        style[mxConstants.STYLE_CURVED] = "1";
    };

    /********************************************
     *
     ********************************************/
    function unlock_graph(self, graph)
    {
        graph.clearSelection();
        graph.setConnectable(true);         // Crear edges/links
        graph.setCellsDisconnectable(true); // Modificar egdes/links
        graph.setCellsLocked(false);
        graph.rubberband.setEnabled(true);  // selection with mouse

        // Enables panning
        graph.setPanning(false);
        graph.panningHandler.useLeftButtonForPanning = false;

        graph.setCellsLocked(false);

        self.config.locked = false;
    }

    /********************************************
     *
     ********************************************/
    function lock_graph(self, graph)
    {
        graph.clearSelection();
        graph.setConnectable(false);            // Crear edges/links
        graph.setCellsDisconnectable(false);    // Modificar egdes/links
        graph.setCellsLocked(true);
        graph.rubberband.setEnabled(false);     // selection with mouse

        // Enables panning
        graph.setPanning(true);
        graph.panningHandler.useLeftButtonForPanning = true;

        graph.setCellsLocked(true);

        self.config.locked = true;
    }

    /********************************************
     *  HACK una cell está compuesta gráficamente de:
     *      - Shape de la celda
     *      - Label     (Contenido a pintar en la celda)
     *      - Overlays  (Cells extras)
     *      - Control   (folding icon) + deleteControl?
     ********************************************/
    function initialize_mxgraph(self)
    {
        var graph = self.config._mxgraph;

        create_root_and_layers(self);
        rebuild_layouts(self);

        mxEvent.disableContextMenu(graph.container);

        graph.border = 30;
        graph.view.setTranslate(graph.border, graph.border);

        // Assigns some global constants for general behaviour, eg. minimum
        // size (in pixels) of the active region for triggering creation of
        // new connections, the portion (100%) of the cell area to be used
        // for triggering new connections, as well as some fading options for
        // windows and the rubberband selection.
        mxConstants.MIN_HOTSPOT_SIZE = 16;
        mxConstants.DEFAULT_HOTSPOT = 1;

        /*---------------------------*
         *      PERMISOS
         *---------------------------*/
        // Enable/Disable cell handling
        graph.setHtmlLabels(true);
        graph.setTooltips(true);

        graph.setPortsEnabled(true);
        mxGraphHandler.prototype.setCloneEnabled(false); // Ctrl+Drag will clone a cell
        graph.setCellsEditable(false);
        graph.setAllowDanglingEdges(false); // not allow dangling edges

        // Enables guides
        mxGraphHandler.prototype.guidesEnabled = true;

        // Enables snapping waypoints to terminals
        mxEdgeHandler.prototype.snapToTerminals = true;

        mxGraph.prototype.ordered = false;

        graph.border = 40;
        graph.view.setTranslate(graph.border/2, graph.border/2);

        // Enables rubberband selection
        graph.rubberband = new mxRubberband(graph);

        // Multiple connections between the same pair of vertices.
        graph.setMultigraph(false);

        if(0) {
            // Removes cells when [DELETE] is pressed
            var keyHandler = new mxKeyHandler(graph);
            keyHandler.bindKey(46, function(evt) {
                if(!graph.isCellsLocked()) {
                    graph.removeCells();
                }
            });
        }

        // Installs automatic validation (use editor.validation = true
        // if you are using an mxEditor instance)
        if(0) {
            var listener = function(sender, evt)
            {
                graph.validateGraph();
            };

            graph.getModel().addListener(mxEvent.CHANGE, listener);
        }

        // Negative coordenates?
        graph.allowNegativeCoordinates = false;

        // Avoids overlap of edges and collapse icons
        graph.keepEdgesInBackground = true;

        /*
         *  HACK Por defecto si los hijos salen un overlap del 50%
         *  se quitan del padre y pasan al default
         */
        if(1) {
            graph.graphHandler.setRemoveCellsFromParent(false); // HACK impide quitar hijos
            mxGraph.prototype.isAllowOverlapParent = function(cell) { return true;}
            mxGraph.prototype.defaultOverlap = 1; // Permite a hijos irse tan lejos como quieran
        }

        // Uses the port icon while connections are previewed
        //graph.connectionHandler.getConnectImage = function(state) {
        //    // TODO pon la imagen???
        //    return new mxImage(state.style[mxConstants.STYLE_IMAGE], 16, 16);
        //};

        // Centers the port icon on the target port
        graph.connectionHandler.targetConnectImage = true;

        mxGraph.prototype.isCellSelectable = function(cell) {
            return true;
        };

        /*
         *  Foldable
         */
        // Defines new collapsed/expanded images
        // Busca mejores imagenes
        //mxGraph.prototype.collapsedImage = self.config.image_collapsed;
        //mxGraph.prototype.expandedImage = self.config.image_expanded;

        // Defines the condition for showing the folding icon
        graph.isCellFoldable = function(cell) {
            if(cell.value && cell.value.schema) {
                return true;
            }
            return false;
        };

        /*
         *  Unlock/lock
         */
        if(self.config.locked) {
            lock_graph(self, graph);
        } else {
            unlock_graph(self, graph);
        }

        /*
         *  Set stylesheet options
         */
        configureStylesheet(graph);

        /*
         *  Create own styles
         */
        for(var i=0; i<self.config.topics_style.length; i++) {
            var topic = self.config.topics_style[i];
            var topic_name = topic.topic_name;
            var graph_styles = topic.graph_styles;

            for(var style_name in graph_styles) {
                var style = graph_styles[style_name];
                create_graph_style(
                    graph,
                    topic_name + "_" + style_name,
                    style
                );
            }
        }

        /*
         *  Own getLabel
         */
        graph.getLabel = function(cell) {
            if (this.getModel().isVertex(cell)) {
                if(cell.value.hook) {
                    return get_hook_info(cell, true, false);
                } if(cell.value.fkey) {
                    return get_fkey_info(cell, true, false);
                } else if(cell.value.schema) {
                    var topic_name = cell.value.schema.topic_name;
                    var id = "";
                    if(cell.value.record) {
                        if(cell.value.record.value) {
                            id = cell.value.record.value;
                        } else if(cell.value.record.id) {
                            id = cell.value.record.id;
                        }
                    }

                    var t = topic_name + "^<br/><b>" + id + "</b><br/>";

                    // Ejemplo de cómo poner un icono dentro de la cell
                    //if(cell.value.tosave_red) {
                    //    t += "<input " +
                    //    "style='cursor:default' " +
                    //    "type='image' src='" +
                    //    "/static/app/images/yuneta/save_red.svg" +
                    //    "' alt='Fix data to save' " +
                    //    "width='" +
                    //    self.config.top_overlay_icon_size +
                    //    "' " +
                    //    "height='" +
                    //    self.config.top_overlay_icon_size +
                    //    "'>"
                    //}
                    return t;
                }
            }
            return "";
        };

        /*
         *  Tooltip
         */
        graph.getTooltipForCell = function(cell) {
            var tip = null;
            if (cell != null && cell.getTooltip != null) {
                tip = cell.getTooltip();
            } else {
                if(this.getModel().isVertex(cell)) {
                    if(cell.value.hook) {
                        return get_hook_info(cell, false, true);
                    } if(cell.value.fkey) {
                        return get_fkey_info(cell, false, true);
                    } else if(cell.value.schema) {
                        var topic_name = cell.value.schema? cell.value.schema.topic_name: "";
                        var id = cell.value.record? cell.value.record.id: "";
                        return topic_name + "^<br/><b>" + id + "</b><br/>";
                    }
                } else if(this.getModel().isEdge(cell)) {
                    if(cell.id) {
                        return "<b>" + cell.id + "</b>";
                    }
                }
            }

            return tip;
        };

        /*
         *  Cursor pointer
         */
        graph.getCursorForCell = function(cell) {
            if(this.model.isEdge(cell)) {
                return 'default';
            } else {
                return 'default';
            }
        };

        /*
         *  Add callback: Only cells selected have "class overlays"
         */
        graph.getSelectionModel().addListener(mxEvent.CHANGE, function(sender, evt) {
            var properties = evt.getProperties(); // HACK associative array, merde!!!
            var kw = {};
            for(var k in properties) {
                if(properties.hasOwnProperty(k)) {
                    kw[k] = properties[k];
                }
            }
            self.gobj_send_event(
                "EV_MX_SELECTION_CHANGE",
                kw,
                self
            );
        });

        /*
         *  Context menu
         */
        graph.popupMenuHandler.factoryMethod = function(menu, cell, evt) {
            self.gobj_send_event("EV_POPUP_MENU", {cell: cell, evt: evt}, self);
        }
    }

    /************************************************************
     *
     ************************************************************/
    function add_class_overlays(self, graph, cell)
    {
        var model = graph.getModel();
        model.beginUpdate();
        try {
            var offsy = self.config.top_overlay_icon_size/1.5;
            var offsx = self.config.top_overlay_icon_size + 5;

            if(cell.isVertex() && cell.value && cell.value.schema) {
                /*--------------------------------------*
                 *          Topics
                 *--------------------------------------*/

                /*--------------------------*
                 *  Data Formtable button
                 *--------------------------*/
                if(1) {
                    var overlay_role = new mxCellOverlay(
                        self.config.image_formtable,
                        "Edit data", // tooltip
                        mxConstants.ALIGN_LEFT, // horizontal align ALIGN_LEFT,ALIGN_CENTER,ALIGN_RIGH
                        mxConstants.ALIGN_TOP, // vertical align  ALIGN_TOP,ALIGN_MIDDLE,ALIGN_BOTTOM
                        new mxPoint(1*offsx - offsy, -offsy), // offset
                        "pointer" // cursor
                    );
                    graph.addCellOverlay(cell, overlay_role);
                    overlay_role.addListener(mxEvent.CLICK, function(sender, evt2) {
                        self.gobj_send_event(
                            "EV_SHOW_CELL_DATA_FORM",
                            {
                                cell: cell
                            },
                            self
                        );
                    });
                }

                /*--------------------------*
                 *  Json Inside of cell
                 *--------------------------*/
                if(!self.config.locked) {
                    var overlay_instance = new mxCellOverlay(
                        self.config.image_folder_tree,
                        "Inside Json View", // tooltip
                        mxConstants.ALIGN_LEFT, // horizontal align ALIGN_LEFT,ALIGN_CENTER,ALIGN_RIGH
                        mxConstants.ALIGN_TOP, // vertical align  ALIGN_TOP,ALIGN_MIDDLE,ALIGN_BOTTOM
                        new mxPoint(2*offsx - offsy, -offsy), // offset
                        "pointer" // cursor
                    );
                    graph.addCellOverlay(cell, overlay_instance);
                    overlay_instance.addListener(mxEvent.CLICK, function(sender, evt2) {
                        self.gobj_send_event(
                            "EV_SHOW_CELL_DATA_JSON",
                            {
                                cell: cell
                            },
                            self
                        );
                    });
                }

                /*--------------------------*
                 *  Delete button
                 *--------------------------*/
                if(!self.config.locked) {
                    var overlay_instance = new mxCellOverlay(
                        self.config.image_delete,
                        "Delete node", // tooltip
                        mxConstants.ALIGN_RIGH, // horizontal align ALIGN_LEFT,ALIGN_CENTER,ALIGN_RIGH
                        mxConstants.ALIGN_TOP, // vertical align  ALIGN_TOP,ALIGN_MIDDLE,ALIGN_BOTTOM
                        new mxPoint(-1*offsx - offsy, -offsy), // offset
                        "pointer" // cursor
                    );
                    graph.addCellOverlay(cell, overlay_instance);
                    overlay_instance.addListener(mxEvent.CLICK, function(sender, evt2) {
                        webix.confirm(
                            {
                                title: t("warning"),
                                text: t("are you sure"),
                                type:"confirm-warning"
                            }).then(function(result) {
                                self.gobj_send_event(
                                    "EV_DELETE_VERTEX",
                                    {
                                        cell: cell
                                    },
                                    self
                                );
                            }
                        );
                    });
                }

                /*--------------------------*
                 *  Clone button
                 *--------------------------*/
                if(!self.config.locked) {
                    var pinta = false;
                    if(cell.value.schema) {
                        var col = kw_collect(cell.value.schema.cols, {id:"id"});
                        if(col && col.length) {
                            if(elm_in_list("rowid", col[0].flag)) {
                                pinta = true;
                            }
                        }
                    }
                    if(pinta) {
                        var overlay_instance = new mxCellOverlay(
                            self.config.image_clone,
                            "Clone node", // tooltip
                            mxConstants.ALIGN_RIGH, // horizontal align ALIGN_LEFT,ALIGN_CENTER,ALIGN_RIGH
                            mxConstants.ALIGN_TOP, // vertical align  ALIGN_TOP,ALIGN_MIDDLE,ALIGN_BOTTOM
                            new mxPoint(0*offsx - offsy, -offsy), // offset
                            "pointer" // cursor
                        );
                        graph.addCellOverlay(cell, overlay_instance);
                        overlay_instance.addListener(mxEvent.CLICK, function(sender, evt2) {
                            webix.prompt({
                                title: "New Service",
                                text: "Service",
                                ok: "Submit",
                                cancel: "Cancel",
                                input: {
                                    required:true,
                                    placeholder: cell.value.record.value,
                                },
                                width:350,
                            }).then(function(result) {
                                self.gobj_send_event(
                                    "EV_CLONE_VERTEX",
                                    {
                                        cell: cell,
                                        name: result
                                    },
                                    self
                                );
                            }).fail(function(){
                            });
                        });
                    }
                }

            } else if(cell.isEdge()) {
                /*--------------------------------------*
                 *          Links
                 *--------------------------------------*/

                /*--------------------------*
                 *  Delete button
                 *--------------------------*/
                if(!self.config.locked) {
                    var overlay_instance = new mxCellOverlay(
                        self.config.image_delete,
                        "Delete link",  // tooltip
                        mxConstants.ALIGN_RIGH, // horizontal align ALIGN_LEFT,ALIGN_CENTER,ALIGN_RIGH
                        mxConstants.ALIGN_TOP, // vertical align  ALIGN_TOP,ALIGN_MIDDLE,ALIGN_BOTTOM
                        new mxPoint(-1*offsx - offsy, -offsy), // offset
                        "pointer" // cursor
                    );
                    graph.addCellOverlay(cell, overlay_instance);
                    overlay_instance.addListener(mxEvent.CLICK, function(sender, evt2) {
                        webix.confirm(
                            {
                                title: t("warning"),
                                text: t("are you sure"),
                                type:"confirm-warning"
                            }).then(function(result) {
                                self.gobj_send_event(
                                    "EV_DELETE_EDGE",
                                    {
                                        cell: cell
                                    },
                                    self
                                );
                            }
                        );
                    });
                }
            }

        } catch (e) {
            log_error(e);
        } finally {
            model.endUpdate();
        }
    }

    /************************************************************
     *
     ************************************************************/
    function add_state_overlays(self, graph, cell)
    {
        var model = graph.getModel();
        model.beginUpdate();
        try {
            var offsy = self.config.top_overlay_icon_size/1.5;
            var offsx = self.config.top_overlay_icon_size + 5;

            if(cell.isVertex() && cell.value && cell.value.schema) {
                /*--------------------------------------*
                 *          Topics
                 *--------------------------------------*/

                /*--------------------------*
                 *  Play button
                 *--------------------------*/
                if(cell.value.torun_node) {
                    var overlay_instance = new mxCellOverlay(
                        self.config.image_run,
                        "Run Node", // tooltip
                        mxConstants.ALIGN_CENTER, // horizontal align ALIGN_LEFT,ALIGN_CENTER,ALIGN_RIGH
                        mxConstants.ALIGN_MIDDLE,  // vertical align  ALIGN_TOP,ALIGN_MIDDLE,ALIGN_BOTTOM
                        new mxPoint(0*offsx, 2*offsy), // offset
                        "pointer" // cursor
                    );
                    graph.addCellOverlay(cell, overlay_instance);
                    overlay_instance.addListener(mxEvent.CLICK, function(sender, evt2) {
                        self.gobj_send_event(
                            "EV_RUN_NODE",
                            {
                                cell: cell
                            },
                            self
                        );
                    });
                }

            } else if(cell.isEdge()) {
                /*--------------------------------------*
                 *          Links
                 *--------------------------------------*/
            }

        } catch (e) {
            log_error(e);
        } finally {
            model.endUpdate();
        }
    }

    /********************************************
     *
     ********************************************/
    function get_hook_info(cell, inclinado, full)
    {
        var col = cell.value.col;

        var t = "<div class=" + (inclinado?"'inclinado font_fijo'":"'font_fijo'") + ">";

        if(full) {
            t += "<b>" + col.id + "</b>&nbsp;" + "(" + col.header + ")";
        } else {
            t += col.header;
        }

        if(full) {
            t += "<br/>";
            t += "<br/>";
            t += "ref=";
            var dict = col.hook;
            for(var k in dict) {
                var v = dict[k];
                t += k;
                t += ":" + v;
            }
        }
        t+= "</div>";

        return t;
    }

    /********************************************
     *
     ********************************************/
    function get_fkey_info(cell, inclinado, full)
    {
        var col = cell.value.col;

        var t = "<div class=" + (inclinado?"'inclinado font_fijo'":"'font_fijo'") + ">";

        if(full) {
            t += "<b>" + col.id + "</b>&nbsp;" + "(" + col.header + ")";
        } else {
            t += col.header;
        }

        if(full) {
            if(cell.parent.value && cell.parent.value.record) {
                var v = cell.parent.value.record[col.id];
                t += "<br/>";
                t += "<br/>";
                t += "value=" + JSON.stringify(v, 2);
            }

            t += "<br/>";
            t += "<br/>";
            t += "ref=";
            var dict = col.fkey;
            for(var k in dict) {
                var v = dict[k];
                t += k;
                t += ":" + v;
            }
        }

        t+= "</div>";

        return t;
    }

    /********************************************
     *
     ********************************************/
    function build_hook_port_id(self, topic_name, topic_id, col)
    {
        var name = topic_name + "^" + topic_id + "^" + col.id;
        return name;
    }

    /********************************************
     *
     ********************************************/
    function build_fkey_port_id(self, topic_name, topic_id, col)
    {
        var name = topic_name + "^" + topic_id + "^" + col.id;
        return name;
    }

    /********************************************
     *
     ********************************************/
    function get_hook_style(self, col)
    {
        var style = "";

        for(var k in col.hook) {
            return k + "_hook";
        }

        return style;
    }

    /********************************************
     *
     ********************************************/
    function get_torun_node(self, topic_name)
    {
        for(var i=0; i<self.config.topics_style.length; i++) {
            var topic = self.config.topics_style[i];
            if(topic_name == topic.topic_name) {
                return topic.run_event;
            }
        }
    }

    /********************************************
     *
     ********************************************/
    function get_default_cx(self, topic_name)
    {
        for(var i=0; i<self.config.topics_style.length; i++) {
            var topic = self.config.topics_style[i];
            if(topic_name == topic.topic_name) {
                return topic.default_cx;
            }
        }
    }

    /********************************************
     *
     ********************************************/
    function get_default_cy(self, topic_name)
    {
        for(var i=0; i<self.config.topics_style.length; i++) {
            var topic = self.config.topics_style[i];
            if(topic_name == topic.topic_name) {
                return topic.default_cy;
            }
        }
    }

    /********************************************
     *
     ********************************************/
    function get_default_alt_cx(self, topic_name)
    {
        for(var i=0; i<self.config.topics_style.length; i++) {
            var topic = self.config.topics_style[i];
            if(topic_name == topic.topic_name) {
                return topic.default_alt_cx;
            }
        }
    }

    /********************************************
     *
     ********************************************/
    function get_default_alt_cy(self, topic_name)
    {
        for(var i=0; i<self.config.topics_style.length; i++) {
            var topic = self.config.topics_style[i];
            if(topic_name == topic.topic_name) {
                return topic.default_alt_cy;
            }
        }
    }

    /********************************************
     *
     ********************************************/
    function add_hook_fkey_ports(self, cell)
    {
        if(!cell.value.record) {
            return;
        }
        var graph = self.config._mxgraph;
        var model = graph.model;
        var topic_name = cell.value.schema.topic_name;
        var topic_id = cell.value.record.id;
        var cols = cell.value.schema.cols;
        var hook_port_cx = self.config.hook_port_cx;
        var hook_port_cy = self.config.hook_port_cy;
        var fkey_port_cx = self.config.fkey_port_cx;
        var fkey_port_cy = self.config.fkey_port_cy;
        var x, y, slot;

        /*
         *  Get the hooks/fkeys
         */
        var hooks = [];
        var fkeys = [];
        for(var i=0; i<cols.length; i++) {
            var col = cols[i];
            var flag = col.flag;
            var is_hook = elm_in_list("hook", flag);
            var is_fkey = elm_in_list("fkey", flag);
            if(is_hook) {
                hooks.push(col);
            }
            if(is_fkey) {
                fkeys.push(col);
            }
        }

        /*
         *  Draw the hooks ports
         */
        slot = 1/(hooks.length+1);
        switch(self.config.hook_port_position) {
            case "top":
                x = slot - (hook_port_cx/cell.geometry.width)/2;
                y = 0 - (hook_port_cy/cell.geometry.height);
                break;
            case "bottom":
                x = slot - (hook_port_cx/cell.geometry.width)/2;
                y = 1;
                break;
            case "left":
                x = 0 - hook_port_cx/cell.geometry.width;
                y = slot - (hook_port_cy/cell.geometry.height)/2;
                break;
            case "right":
                x = 1;
                y = slot - (hook_port_cy/cell.geometry.height)/2;
                break;
        }
        for(var i=0; i<hooks.length; i++) {
            var col = hooks[i];

            /*
             *  hook types: list, dict, string  // TODO review types
             */
            var hook = kw_get_dict_value(col, "hook", null, false);
            if(hook) {
                /*-----------------------------*
                 *      Draw the hook port
                 *-----------------------------*/
                var cell_id = build_hook_port_id(self, topic_name, topic_id, col);
                if(model.getCell(cell_id)) {
                    log_error("Hook cell duplicated: " + cell_id);
                }
                var hook_port = graph.insertVertex(
                    cell,                       // group
                    cell_id,                    // id
                    {                           // value
                        topic_name: topic_name,
                        topic_id: topic_id,
                        col: col,
                        hook: hook
                    },
                    x, y,                   // x,y
                    hook_port_cx,           // width
                    hook_port_cy,           // height
                    get_hook_style(self, col), // style
                    true                    // relative
                );

                switch(self.config.hook_port_position) {
                    case "top":
                    case "bottom":
                        x += slot;
                        break;
                    case "left":
                    case "right":
                        y += slot;
                        break;
                }
            }
        }

        /*
         *  Draw the fkeys ports
         */
        slot = 1/(fkeys.length+1);
        switch(self.config.fkey_port_position) {
            case "top":
                x = slot - (fkey_port_cx/cell.geometry.width)/2;
                y = 0 - (fkey_port_cy/cell.geometry.height);
                break;
            case "bottom":
                x = slot - (fkey_port_cx/cell.geometry.width)/2;
                y = 1;
                break;
            case "left":
                x = 0 - (fkey_port_cx)/ cell.geometry.width;
                y = slot - (fkey_port_cy/cell.geometry.height)/2;
                break;
            case "right":
                x = 1;
                y = slot - (fkey_port_cy/cell.geometry.height)/2;
                break;
        }

        for(var i=0; i<fkeys.length; i++) {
            var col = fkeys[i];
            /*
             *  fkey types: list, string        // TODO review types
             */
            var fkey = kw_get_dict_value(col, "fkey", null, false);
            if(fkey) {
                /*-----------------------------*
                 *      Draw the fkey port
                 *-----------------------------*/
                var cell_id = build_fkey_port_id(self, topic_name, topic_id, col);
                if(model.getCell(cell_id)) {
                    log_error("Fkey cell duplicated: " + cell_id);
                }
                var fkey_port_cell = graph.insertVertex(
                    cell,                       // group
                    cell_id,                    // id
                    {                           // value
                        topic_name: topic_name,
                        topic_id: topic_id,
                        col: col,
                        fkey: fkey
                    },
                    x, y,                   // x,y
                    fkey_port_cx,           // width
                    fkey_port_cy,           // height
                    topic_name + "_fkey",   // style
                    true                    // relative
                );

                switch(self.config.fkey_port_position) {
                    case "top":
                    case "bottom":
                        x += slot;
                        break;
                    case "left":
                    case "right":
                        y += slot;
                        break;
                }
            }
        }
    }

    /************************************************************
     *  Clear links
     ************************************************************/
    function clear_links(self, cell)
    {
        var graph = self.config._mxgraph;
        var model = graph.model;

        if(!cell.value.record) {
            // Virgin node
            return;
        }

        var topic_name = cell.value.schema.topic_name;
        var topic_id = cell.value.record.id;

        var cols = cell.value.schema.cols;
        for(var i=0; i<cols.length; i++) {
            var col = cols[i];
            if(!col.fkey) {
                continue;
            }

            var fkey_port_name = build_fkey_port_id(self, topic_name, topic_id, col);
            var fkey_port_cell = model.getCell(fkey_port_name);
            var fkeys = cell.value.record[col.id];

            if(fkeys) {
                if(is_string(fkeys)) {
                    clear_link(self, topic_name, col, fkey_port_cell, fkeys);
                } else if(is_array(fkeys)) {
                    for(var j=0; j<fkeys.length; j++) {
                        clear_link(self, topic_name, col, fkey_port_cell, fkeys[j]);
                    }
                } else {
                    log_error("fkey type unsupported: " + JSON.stringify(fkeys));
                }
            }
        }
    }

    /************************************************************
     *  Clear link
     ************************************************************/
    function clear_link(
        self,
        source_topic_name,
        source_col,
        fkey_port_cell,
        fkey
    )
    {
        var graph = self.config._mxgraph;
        var model = graph.model;

        fkey = decoder_fkey(source_col, fkey);
        if(!fkey) {
            return;
        }
        var target_topic_name = fkey.topic_name;
        var target_topic_id = fkey.id;
        var target_hook = fkey.hook_name;

        var target_cell_name = build_cell_name(
            self, target_topic_name, target_topic_id
        );
        var target_cell = model.getCell(target_cell_name);
        if(!target_cell) {
            log_error("target_cell '" + target_cell_name + "' NOT FOUND");
            return;
        }
        var targer_hook_col = get_col(target_cell.value.schema.cols, target_hook);

        var target_port_name = build_hook_port_id(
            self,
            target_topic_name,
            target_topic_id,
            targer_hook_col
        );
        var target_port_cell = model.getCell(target_port_name);
        if(!target_port_cell) {
            log_error("target_port_cell '" + target_port_name + "' NOT FOUND");
            return;
        }

        // HACK "==>" repeated
        var cell_id = fkey_port_cell.id + " ==> " + target_port_name;
        var link_cell = model.getCell(cell_id);
        if(link_cell) {
            graph.removeCells([link_cell]);
        }
    }

    /************************************************************
     *  Draw links
     ************************************************************/
    function draw_links(self, cell)
    {
        var graph = self.config._mxgraph;
        var model = graph.model;

        var topic_name = cell.value.schema.topic_name;
        var topic_id = cell.value.record.id;

        var cols = cell.value.schema.cols;
        for(var i=0; i<cols.length; i++) {
            var col = cols[i];
            if(!col.fkey) {
                continue;
            }

            var fkey_port_name = build_fkey_port_id(self, topic_name, topic_id, col);
            var fkey_port_cell = model.getCell(fkey_port_name);
            var fkeys = cell.value.record[col.id];

            if(fkeys) {
                if(is_string(fkeys)) {
                    draw_link(self, topic_name, col, fkey_port_cell, fkeys);
                } else if(is_array(fkeys)) {
                    for(var j=0; j<fkeys.length; j++) {
                        draw_link(self, topic_name, col, fkey_port_cell, fkeys[j]);
                    }
                } else {
                    log_error("fkey type unsupported: " + JSON.stringify(fkeys));
                }
            }
        }
    }

    /************************************************************
     *
     ************************************************************/
    function get_col(cols, col_id)
    {
        if(is_array(cols))  {
            for(var i=0; i<cols.length; i++) {
                var col = cols[i];
                if(col.id == col_id) {
                    return col;
                }
            }
        } else if(is_object(cols)) {
            return cols[cold_id];
        }
        return null;
    }

    /************************************************************
     *  Draw link
     ************************************************************/
    function draw_link(
        self,
        source_topic_name,
        source_col,
        fkey_port_cell,
        fkey
    )
    {
        var graph = self.config._mxgraph;
        var model = graph.model;

        var source_fkey = decoder_fkey(source_col, fkey_port_cell.id);
        if(!source_fkey) {
            log_error("What f*?");
            return;
        }
        var source_topic_name = source_fkey.topic_name;
        var source_topic_id = source_fkey.id;
        var source_fkey = source_fkey.hook_name;

        var target_fkey = decoder_fkey(source_col, fkey);
        if(!target_fkey) {
            log_error("What f*?");
            return;
        }
        var target_topic_name = target_fkey.topic_name;
        var target_topic_id = target_fkey.id;
        var target_hook = target_fkey.hook_name;

        var target_cell_name = build_cell_name(
            self, target_topic_name, target_topic_id
        );

        var target_cell = model.getCell(target_cell_name);
        if(!target_cell) {
            log_error("target_cell '" + target_cell_name + "' NOT FOUND");
            return;
        }

        var targer_hook_col = get_col(target_cell.value.schema.cols, target_hook);

        var target_port_name = build_hook_port_id(
            self,
            target_topic_name,
            target_topic_id,
            targer_hook_col
        );

        var target_port_cell = model.getCell(target_port_name);
        if(!target_port_cell) {
            log_error("target_port_cell '" + target_port_name + "' NOT FOUND");
            return;
        }

        // HACK "==>" repeated
        var cell_id = fkey_port_cell.id + " ==> " + target_port_name;
        var link_cell = model.getCell(cell_id);
        if(!link_cell) {
            var cell_value = {
                child_topic_name: source_topic_name,
                child_topic_id: source_topic_id,
                child_fkey: source_fkey,
                parent_topic_name: target_topic_name,
                parent_topic_id: target_topic_id,
                parent_hook: target_hook
            }
            graph.insertEdge(
                get_layer(self),                // group
                cell_id,                        // id
                cell_value,                     // value
                fkey_port_cell,                 // source
                target_port_cell,               // target
                source_topic_name + "_arrow"    // style
            );
        }
    }

    /************************************************************
     *  Create topic cell,
     *      - from backend
     ************************************************************/
    function create_topic_cell(self, schema, record)
    {
        var graph = self.config._mxgraph;
        var model = graph.model;
        var cell = null;

        var torun_node = get_torun_node(self, schema.topic_name);

        /*------------------------------------------*
         *  Creating filled cell from backend data
         *------------------------------------------*/
        var geometry = record._geometry;
        var x = kw_get_int(geometry, "x", 40, false);
        var y = kw_get_int(geometry, "y", 40, false);
        var width = kw_get_int(
            geometry,
            "width",
            get_default_cx(self, schema.topic_name),
            false
        );
        var height = kw_get_int(
            geometry,
            "height",
            get_default_cy(self, schema.topic_name),
            false
        );
        var cell_name = build_cell_name(self, schema.topic_name, record.id);

        if(model.getCell(cell_name)) {
            log_error("Cell duplicated: " + cell_name);
        }
        cell = graph.insertVertex(
            get_layer(self),    // group
            cell_name,          // id
            {                   // value
                cell_name: cell_name,
                schema: schema,
                record: record,
                torun_node: torun_node
            },
            x, y,               // x,y
            width, height,      // width,height
            schema.topic_name + "_node",  // style
            false               // relative
        );
        cell.setConnectable(false);
        cell.geometry.alternateBounds = new mxRectangle(
            0,
            0,
            get_default_alt_cx(self, schema.topic_name),
            get_default_alt_cy(self, schema.topic_name),
        );
        add_hook_fkey_ports(self, cell);

        return cell;
    }

    /************************************************************
     *  Update topic cell
     *      - from backend
     ************************************************************/
    function update_topic_cell(self, cell, record)
    {
        var graph = self.config._mxgraph;
        var model = graph.model;

        if(!cell || !cell.value) {
            log_error("What cell?");
        }

        /*---------------------------------*
         *  Updating existing topic cell
         *---------------------------------*/
        cell.value.record = record;

        /*
         *  Update formtable if it's opened
         */
        if(cell.value.gobj_cell_formtable) {
            cell.value.gobj_cell_formtable.gobj_send_event(
                "EV_LOAD_DATA",
                [record],
                self
            );
        }

        return cell;
    }

    /************************************************************
     *  Update topic cell
     *      - from backend
     ************************************************************/
    function update_geometry(self, cell, geometry)
    {
        self.config.lock_publish_geometry = true;
        var __origin__ = kw_get_str(geometry, "__origin__", "", false);
        if(__origin__ && __origin__ != self.config.uuid) {
            /*
             *  pinto updates de otros, pero no el mio
             *  porque volverá a publicar otro update que nos volverá a llegar:
             *      INFINITE LOOP!
             * No retroalimentes
             */
            var graph = self.config._mxgraph;
            graph.resizeCell(cell, geometry, false);
        }
        self.config.lock_publish_geometry = false;
    }


    /************************************************************
     *
     ************************************************************/
    function is_node_cell(cell)
    {
        if(!cell.value.schema) {
            return false;
        }
        var topic_name = cell.value.schema.topic_name;
        if(cell.style != topic_name + "_node") {
            return false;
        }
        return true;
    }

    /************************************************************
     *
     ************************************************************/
    function get_node_cells(self, topic_name)
    {
        var graph = self.config._mxgraph;
        var node_cells = [];

        graph.selectCells(
            true,               // vertices
            false,              // edges
            get_layer(self),    // parent
            true                // selectGroups
        );

        var cells = graph.getSelectionCells();
        for(var i=0; i<cells.length; i++) {
            var cell = cells[i];
            if(!is_node_cell(cell)) {
                continue;
            }
            if(topic_name) {
                if(cell.value.schema.topic_name == topic_name) {
                    node_cells.push(cell);
                }
            } else {
                // All
                node_cells.push(cell);
            }
        }

        graph.clearSelection();

        return node_cells;
    }

    /************************************************************
     *
     ************************************************************/
    function set_positions(self, graph, model)
    {
        var levels = [];

        for(var i=0; i<self.config.topics_style.length; i++) {
            levels.push({
                topic_name: self.config.topics_style[i].topic_name,
                x: 0,
                y: 0,
                width: 0,
                height: 0,
                cells: []
            });
        }

        var cells = get_node_cells(self);
        for(var i=0; i<cells.length; i++) {
            var cell = cells[i];
            var topic_name = cell.value.schema.topic_name;

            var level = null;
            for(var j=0; j<levels.length; j++) {
                if(levels[j].topic_name == topic_name) {
                    level = levels[j];
                    break;
                }
            }
            if(!level) {
                log_error("No level for topic_name: " + topic_name);
                continue;
            }

            // Set the level cx/cy with the bigger cell
            level.width = Math.max(level.width, cell.geometry.width);
            level.height = Math.max(level.height, cell.geometry.height);
            level.cells.push(cell);
        }

        for(var i=0; i<levels.length; i++) {
            var level = levels[i];

            // Set the level cx/cy with the bigger group
            for(var j=0; j<level.cells.length; j++) {
                var cell = level.cells[j];
                level.width = Math.max(level.width, cell.geometry.width);
                level.height = Math.max(level.height, cell.geometry.height);
            }
            if(i==0) {
                level.y = 60;
            } else {
                level.y = levels[i-1].y + levels[i-1].height + self.config.cell_cy_sep;
            }
            for(var j=0; j<level.cells.length; j++) {
                var cell = level.cells[j];
                if(j==0) {
                    level.x = 0;
                    if(i>0) {
                        level.x += self.config.cell_cy_sep*2;
                    }
                } else {
                    level.x += level.cells[j-1].geometry.width;
                    level.x += self.config.cell_cx_sep;
                }
                var geo = graph.getCellGeometry(cell).clone();
                geo.x = level.x;
                geo.y = level.y;
                model.setGeometry(cell, geo);
            }
        }
    }

    /************************************************************
     *  Reordena graph, layout como el de mx_json_viewer
     ************************************************************/
    function reordena_graph(self)
    {
        var graph = self.config._mxgraph;
        var model = graph.getModel();

        model.beginUpdate();
        try {
            set_positions(self, graph, model);

            graph.view.setTranslate(graph.border, graph.border);

        } catch (e) {
            log_error(e);
        } finally {
            model.endUpdate();
        }
    }

    /************************************************************
     *  Save graph
     ************************************************************/
    function save_graph(self)
    {
        var cells = get_node_cells(self);
        for(var i=0; i<cells.length; i++) {
            var cell = cells[i];

            var _geometry = kw_get_dict_value(
                cell.value.record,
                "_geometry",
                {},
                true
            );
            if(!is_object(_geometry)) {
                _geometry = {};
                kw_set_dict_value(cell.value.record, "_geometry", _geometry);
            }
            __extend_dict__(
                _geometry,
                filter_dict(
                    cell.geometry,
                    ["x", "y", "width", "height"]
                )
            );
            __extend_dict__(
                _geometry,
                {
                    __origin__: self.config.uuid
                }
            );

            var options = {
                list_dict: true,
                autolink: false
            };
            var kw_update = {
                treedb_name: self.config.treedb_name,
                topic_name: cell.value.schema.topic_name,
                record: cell.value.record,
                options: options
            };

            self.gobj_publish_event("EV_UPDATE_RECORD", kw_update, self);
        }
    }




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *  From parent, load data
     ********************************************/
    function ac_load_data(self, event, kw, src)
    {
        var graph = self.config._mxgraph;
        var model = graph.getModel();

        model.beginUpdate();
        try {
            var schema = kw.schema;
            var data = kw.data;

            /*--------------------------------------------*
             *  Creating and loading cells from backend
             *--------------------------------------------*/
            var cells = [];
            for(var i=0; i<data.length; i++) {
                var record = data[i];
                var cell = create_topic_cell(self, schema, record);
                add_state_overlays(self, graph, cell);
                cells.push(cell);
            }

            for(var i=0; i<cells.length; i++) {
                var cell = cells[i];
                clear_links(self, cell);
                draw_links(self, cell);
            }
            if(self.config.collapsed) {
                collapse_edition(self, self.config.collapsed);
            }
            execute_layout(self);

        } catch (e) {
            log_error(e);
        } finally {
            model.endUpdate();
        }

        return 0;
    }

    /********************************************
     *  From parent, clear all
     ********************************************/
    function ac_clear_data(self, event, kw, src)
    {
        var graph = self.config._mxgraph;
        var model = graph.getModel();

        set_btn_submmit_state(self, "save_graph", false);

        model.beginUpdate();
        try {
            graph.selectCells(
                true,               // vertices
                true,               // edges
                get_layer(self),    // parent
                true                // selectGroups
            );

            /*
             *  Borra los formtable de celda abiertos
             */
            var cells = graph.getSelectionCells();
            for(var i=0; i<cells.length; i++) {
                var cell = cells[i];
                if(cell.value.gobj_cell_formtable) {
                    __yuno__.gobj_destroy(cell.value.gobj_cell_formtable);
                    cell.value.gobj_cell_formtable = 0;
                }
            }

            graph.removeCells();

        } catch (e) {
            log_error(e);
        } finally {
            model.endUpdate();
        }

        return 0;
    }

    /********************************************
     *  From parent, inform of treedb schemas
     ********************************************/
    function ac_descs(self, event, kw, src)
    {
        self.config.descs = kw;  // save schemas

        /*
         *  Update topics_name menu
         */
        var topics = [];

        for(var i=0; i<self.config.topics.length; i++) {
            var topic = self.config.topics[i];
            var topic_name = topic.topic_name;
            var desc = self.config.descs[topic_name];
            if(!desc) {
                log_error("DESC of " + topic_name + " Not found");
                continue;
            }
            topics.push({
                id: desc.topic_name,
                value: desc.topic_name,
                icon: "" // TODO
            });
        }
        $$(build_name(self, "topics_menu")).parse(topics);

        return 0;
    }

    /********************************************
     *  From parent, ack to create record
     *              (mine or from others)
     ********************************************/
    function ac_node_created(self, event, kw, src)
    {
        var graph = self.config._mxgraph;
        var model = graph.getModel();

        model.beginUpdate();
        try {
            var cell = create_topic_cell(self, kw.schema, kw.node);
            add_state_overlays(self, graph, cell);
            draw_links(self, cell);

        } catch (e) {
            log_error(e);
        } finally {
            model.endUpdate();
        }

        return 0;
    }

    /********************************************
     *  From parent, ack to update record
     *              (mine or from others)
     ********************************************/
    function ac_node_updated(self, event, kw, src)
    {
        var graph = self.config._mxgraph;
        var model = graph.getModel();

        var cell_name = build_cell_name(self, kw.topic_name, kw.node.id);
        var cell = model.getCell(cell_name);
        if(!cell) {
            log_error("ac_node_updated: cell not found");
            return -1;
        }

        model.beginUpdate();
        try {
            clear_links(self, cell);
            update_topic_cell(self, cell, kw.node);
            update_geometry(self, cell, kw.node._geometry);
            draw_links(self, cell);

        } catch (e) {
            log_error(e);
        } finally {
            model.endUpdate();
        }

        return 0;
    }

    /********************************************
     *  From parent, ack to delete record
     *              (mine or from others)
     ********************************************/
    function ac_node_deleted(self, event, kw, src)
    {
        var graph = self.config._mxgraph;
        var model = graph.getModel();

        var cell_name = build_cell_name(self, kw.topic_name, kw.node.id);
        var cell = model.getCell(cell_name);
        if(!cell) {
            log_error("ac_node_deleted: cell not found");
            return -1;
        }

        model.beginUpdate();
        try {
            graph.removeCells([cell]);

        } catch (e) {
            log_error(e);
        } finally {
            model.endUpdate();
        }

        return 0;
    }

    /********************************************
     *  Create a cell vertex of topic node
     *  From popup menu
     *  WARNING not used
     ********************************************/
    function ac_create_vertex(self, event, kw, src)
    {
        var graph = self.config._mxgraph;
        var model = graph.getModel();
        var topic_name = kw.topic_name;
        var schema = self.config.descs[topic_name];

        model.beginUpdate();
        try {
            create_topic_cell(self, schema, null)

        } catch (e) {
            log_error(e);
        } finally {
            model.endUpdate();
        }

        return 0;
    }

    /********************************************
     *  From vertex's overlay icon
     ********************************************/
    function ac_delete_vertex(self, event, kw, src)
    {
        var graph = self.config._mxgraph;
        var model = graph.getModel();
        var cell = kw.cell;

        model.beginUpdate();
        try {
            if(!cell.value.cell_name) {
                // new vertex in progress, delete formtable of cell
                if(cell.value.gobj_cell_formtable) {
                    __yuno__.gobj_destroy(cell.value.gobj_cell_formtable);
                    cell.value.gobj_cell_formtable = 0;
                }
                graph.removeCells([cell]);
            } else {
                // Wait to EV_NODE_DELETED to delete cell
                if(cell.value.gobj_cell_formtable) {
                    __yuno__.gobj_destroy(cell.value.gobj_cell_formtable);
                    cell.value.gobj_cell_formtable = 0;
                }

                var options = {
                    force: false // que se borre por partes, primero los link
                };

                var kw_delete = {
                    treedb_name: self.config.treedb_name,
                    topic_name: cell.value.schema.topic_name,
                    record: cell.value.record,
                    options: options
                };
                self.gobj_publish_event("EV_DELETE_RECORD", kw_delete, self);
            }

        } catch (e) {
            log_error(e);
        } finally {
            model.endUpdate();
        }

        return 0;
    }

    /********************************************
     *  From vertex's overlay icon
     ********************************************/
    function ac_clone_vertex(self, event, kw, src)
    {
        var graph = self.config._mxgraph;
        var model = graph.getModel();
        var cell = kw.cell;
        var name = kw.name;

        model.beginUpdate();
        try {
            if(cell.value.cell_name) {
                // new vertex in progress, delete formtable of cell
                if(cell.value.gobj_cell_formtable) {
                    __yuno__.gobj_destroy(cell.value.gobj_cell_formtable);
                    cell.value.gobj_cell_formtable = 0;
                }

                var options = {
                    list_dict: true
                };
                var kw_create = {
                    treedb_name: self.config.treedb_name,
                    topic_name: cell.value.schema.topic_name,
                    record: __duplicate__(cell.value.record),
                    options: options
                };
                delete kw_create.record.id;
                kw_create.record.value = name;
                kw_create.record._geometry.x += kw_create.record._geometry.width;
                kw_create.record._geometry.y += kw_create.record._geometry.height;
                self.gobj_publish_event("EV_CREATE_RECORD", kw_create, self);
            }

        } catch (e) {
            log_error(e);
        } finally {
            model.endUpdate();
        }

        return 0;
    }

    /********************************************
     *  From edge's overlay icon
     ********************************************/
    function ac_delete_edge(self, event, kw, src)
    {
        var graph = self.config._mxgraph;
        var model = graph.getModel();
        var cell = kw.cell;

        var source_id = cell.source.id;
        var target_id = cell.target.id;

        var source_cell = model.getCell(cell.source.id);
        var target_cell = model.getCell(cell.target.id);
        var parent_cell = null;
        var child_cell = null;

        if(source_cell.value.fkey) {
            child_cell = source_cell;
        } else if(source_cell.value.hook) {
            parent_cell = source_cell;
        }
        if(target_cell.value.fkey) {
            child_cell = target_cell;
        } else if(target_cell.value.hook) {
            parent_cell = target_cell;
        }

        var parent_ref = parent_cell.value.topic_name + "^";
            parent_ref += parent_cell.value.topic_id + "^";
            parent_ref += parent_cell.value.col.id

        var child_ref = child_cell.value.topic_name + "^";
            child_ref += child_cell.value.topic_id;

        var options = {
            list_dict: true
        };
        var kw_unlink = {
            treedb_name: self.config.treedb_name,
            parent_ref: parent_ref,
            child_ref: child_ref,
            options: options
        };

        // Wait to EV_NODE_UPDATED to delete cell
        self.gobj_publish_event("EV_UNLINK_RECORDS", kw_unlink, self);

        return 0;
    }

    /********************************************
     *  Create record (topic node)
     *  Event from formtable
     *  kw: {
     *      topic_name,
     *      record
     *      + treedb_name
     *  }
     ********************************************/
    function ac_create_record(self, event, kw, src)
    {
        var graph = self.config._mxgraph;
        var model = graph.model;
        var cell_id = src.gobj_read_attr("user_data"); // HACK reference cell <-> gobj_formtable
        var cell = model.getCell(cell_id);

        var options = {
            list_dict: true,
            create: true,
            autolink: true
        };

        var kw_create = {
            treedb_name: self.config.treedb_name,
            topic_name: kw.topic_name,
            record: kw.record,
            options: options
        };

        kw_create.record["_geometry"] = filter_dict(
            cell.geometry,
            ["x", "y", "width", "height"]
        );

        // HACK use the powerful update_node
        self.gobj_publish_event("EV_UPDATE_RECORD", kw_create, self);

        return 0;
    }

    /********************************************
     *  Update record (topic node)
     *  Event from formtable
     *  kw: {
     *      + treedb_name
     *      topic_name,
     *      record
     *  }
     ********************************************/
    function ac_update_record(self, event, kw, src)
    {
        var graph = self.config._mxgraph;
        var model = graph.model;
        var cell_id = src.gobj_read_attr("user_data"); // HACK reference cell <-> gobj_formtable
        var cell = model.getCell(cell_id);

        var options = {
            list_dict: true,
            autolink: true
        };

        var kw_update = {
            treedb_name: self.config.treedb_name,
            topic_name: kw.topic_name,
            record: kw.record,
            options: options
        };

        kw_update.record["_geometry"] = filter_dict(
            cell.geometry,
            ["x", "y", "width", "height"]
        );

        self.gobj_publish_event("EV_UPDATE_RECORD", kw_update, self);

        return 0;
    }

    /********************************************
     *  Event from formtable
     ********************************************/
    function ac_show_hook_data(self, event, kw, src)
    {
        var treedb_name = kw.treedb_name;
        var parent_topic_name = kw.parent_topic_name;
        var child_topic_name = kw.child_topic_name;
        var child_field_name = kw.child_field_name;
        var child_field_value = kw.child_field_value;
        var click_x = kw.click_x;
        var click_y = kw.click_x;

        return self.gobj_publish_event("EV_SHOW_HOOK_DATA", kw, self);
    }

    /********************************************
     *  Show formtable to edit record, from here
     ********************************************/
    function ac_show_cell_data_form(self, event, kw, src)
    {
        var cell = kw.cell;
        var cell_name = cell.value.cell_name; // Null on new nodes
        var schema = cell.value.schema;
        var record = cell.value.record;

        var name = self.gobj_escaped_short_name() + "'" + (cell_name?cell_name:"");

        var gobj_cell_formtable = cell.value.gobj_cell_formtable;
        if(gobj_cell_formtable) {
            gobj_cell_formtable.gobj_send_event("EV_TOGGLE", {}, self);
            return 0;
        }

        var kw_formtable = {
            subscriber: self,  // HACK get all output events
            user_data: cell.getId(),  // HACK reference cell <-> gobj_formtable

            ui_properties: {
                minWidth: 360,
                minHeight: 300
            },

            treedb_name: self.config.treedb_name,
            topic_name: schema.topic_name,
            cols: schema.cols,
            with_checkbox: false,
            with_textfilter: true,
            with_sort: true,
            with_top_title: true,
            with_footer: true,
            with_navigation_toolbar: true,
            with_refresh: true,
            with_trash_button: false,
            with_clone_button: false,
            with_rowid_field: true,
            hide_private_fields: true,
            list_mode_enabled: true,
            current_mode: cell_name?"update":"create",
            update_mode_enabled: cell_name?true:false,
            create_mode_enabled: cell_name?false:true,
            delete_mode_enabled: cell_name?true:false,

            window_properties: {
                without_window_fullscreen_btn: false,   // Hide fullscreen button
                without_window_close_btn: false,        // Hide minimize/destroy button
                without_destroy_window_on_close: false, // No destroy window on close (hide)
                with_create_window_on_start: false,     // Don't create window on start
            },
            is_pinhold_window: true,
            window_title: schema.topic_name,
            window_image: kw.image,
            width: 950,
            height: 600
        };

        if(cell_name) {
            gobj_cell_formtable = __yuno__.gobj_create_unique(
                name,
                Ui_treedb_formtable,
                kw_formtable,
                self
            );
        } else {
            gobj_cell_formtable = __yuno__.gobj_create(
                name,
                Ui_treedb_formtable,
                kw_formtable,
                self
            );
        }
        cell.value.gobj_cell_formtable = gobj_cell_formtable;

        if(record) {
            gobj_cell_formtable.gobj_send_event(
                "EV_LOAD_DATA",
                [record],
                self
            );
        }

        return 0;
    }

    /********************************************
     *  Show inside json of cell
     ********************************************/
    function ac_show_cell_data_json(self, event, kw, src)
    {
        var cell = kw.cell;

        var n = "Json Cell Inside: " + cell.id;
        var gobj_je = __yuno__.gobj_find_unique_gobj(n);
        if(!gobj_je) {
            gobj_je = __yuno__.gobj_create_unique(
                n,
                Je_viewer,
                {
                    window_title: n,
                    width: 900,
                    height: 600
                },
                self
            );
            gobj_je.gobj_start();
        }
        gobj_je.gobj_send_event(
            "EV_SHOW",
            {},
            self
        );
        gobj_je.gobj_send_event(
            "EV_CLEAR_DATA",
            {},
            self
        );
        var data = new Object(cell.value);
        delete data.gobj_cell_formtable;
        gobj_je.gobj_send_event(
            "EV_LOAD_DATA",
            {data: cell.value},
            self
        );

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_run_node(self, event, kw, src)
    {
        var graph = self.config._mxgraph;
        var model = graph.model;

        var cell = kw.cell;
        if(!cell) {
            return -1;
        }
        if(cell.isVertex()) {
            if(cell.value && cell.value.schema) {
                // It's a topic node cell
                var record = __duplicate__(cell.value.record);

                for(var key in record) {
                    var value = record[key];
                    if(is_string(value) && value.indexOf("#") != -1) {
                        var nodes = record.nodes;
                        if(nodes.length >= 1) {
                            var node = nodes[0];
                            var parent_cell = model.getCell(
                                build_cell_name(self, node.topic_name, node.id)
                            );
                            if(parent_cell) {
                                // For now only to ip variable
                                var ip = parent_cell.value.record.ip;
                                var regex = replace_variable_engine("ip");
                                value = value.replace(regex, ip);
                                record[key] = value;
                            }
                        }
                    }
                }

                var kw_cell = {
                    treedb_name: self.config.treedb_name,
                    topic_name: cell.value.schema.topic_name,
                    record: record
                }
                self.gobj_publish_event("EV_RUN_NODE", kw_cell, self);
            }
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_popup_menu(self, event, kw, src)
    {
        var cell = kw.cell;
        var evt = kw.evt;

        if(!cell) {
            return -1;
        }
        if(self.config.locked) {
            return 0;
        }
        var $popup = $$(build_name(self, "cell_menu_popup"));
        $popup.cell = cell;
        $popup.show(
        {
            x: evt.x,
            y: evt.y
        });

        if(cell.isVertex()) {
        } else {
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_extend_size(self, event, kw, src)
    {
        var graph = self.config._mxgraph;
        var model = graph.model;

        var orig_cell = kw.cell;
        if(!orig_cell) {
            return -1;
        }
        if(!is_node_cell(orig_cell)) {
            return -1;
        }
        var topic_name = orig_cell.value.schema.topic_name;
        var width = orig_cell.geometry.width;
        var height = orig_cell.geometry.height;

        model.beginUpdate();
        try {
            var cells = get_node_cells(self, topic_name);
            for(var i=0; i<cells.length; i++) {
                var cell = cells[i];
                var geo = graph.getCellGeometry(cell).clone();
                geo.width = width;
                geo.height = height;
                model.setGeometry(cell, geo);
            }
        } catch (e) {
            log_error(e);
        } finally {
            model.endUpdate();
        }

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_mx_click(self, event, kw, src)
    {
        var cell = kw.cell;
        if(!cell) {
            return -1;
        }
        if(cell.isVertex()) {
            if(cell.value && cell.value.schema) {
                // It's a topic node cell
                var kw_cell = {
                    treedb_name: self.config.treedb_name,
                    topic_name: cell.value.schema.topic_name,
                    record: cell.value.record
                }
                self.gobj_publish_event("EV_MX_VERTEX_CLICKED", kw_cell, self);
            }
        } else {
            if(cell.value) {
                self.gobj_publish_event("EV_MX_EDGE_CLICKED", cell.value, self);
            }
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_mx_double_click(self, event, kw, src)
    {
        var cell = kw.cell;
        if(!cell) {
            return -1;
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_selection_change(self, event, kw, src)
    {
        var graph = self.config._mxgraph;

        /*
         *  HACK "added" vs "removed"
         *  The names are inverted due to historic reasons.  This cannot be changed.
         *
         *  HACK don't change the order, first removed, then added
         */
        try {
            var cells_removed = kw.added;
            if(cells_removed) {
                for (var i = 0; i < cells_removed.length; i++) {
                    var cell = cells_removed[i];
                    graph.removeCellOverlays(cell); // Delete all previous overlays
                    add_state_overlays(self, graph, cell);
                }
            }
        } catch (e) {
            info_user_error(e);
        }

        try {
            var cells_added = kw.removed;
            if(cells_added) {
                for (var i = 0; i < cells_added.length; i++) {
                    var cell = cells_added[i];
                    graph.removeCellOverlays(cell); // Delete all previous overlays
                    add_state_overlays(self, graph, cell);
                    add_class_overlays(self, graph, cell);
                }
            }
        } catch (e) {
            info_user_error(e);
        }

        return 0;
    }

    /********************************************
     *  Vertices or edges has been added
     ********************************************/
    function ac_mx_addcells(self, event, kw, src)
    {
        var graph = self.config._mxgraph;
        var model = graph.model;

        var cells = kw.cells;

        for(var i=0; i<cells.length; i++) {
            var cell = cells[i];
            if(cell.isVertex()) {
                // IGNORE Vertices, add/remove are controlled by program buttons
                continue;
            }

            if(cell.isEdge()) {
                if(strstr(cell.id, "==>")) {
                    // Not a new link done by user edition
                    continue;
                }

                // New Edges or edges source/target changes are done by user edition
                var source_cell = null;
                var target_cell = null;
                try {
                    source_cell = model.getCell(cell.source.id);
                    target_cell = model.getCell(cell.target.id);
                } catch (e) {
                }
                if(!source_cell) {
                    log_error("source_cell '" + cell.source.id + "' NOT FOUND");
                    continue;
                }
                if(!target_cell) {
                    log_error("target_cell '" + cell.target.id + "' NOT FOUND");
                    continue;
                }
                var parent_cell = null;
                var child_cell = null;

                if(source_cell.value.fkey) {
                    child_cell = source_cell;
                } else if(source_cell.value.hook) {
                    parent_cell = source_cell;
                }
                if(target_cell.value.fkey) {
                    child_cell = target_cell;
                } else if(target_cell.value.hook) {
                    parent_cell = target_cell;
                }

                if(!parent_cell || !child_cell) {
                    graph.removeCells([cell]);
                    log_warning(t("links must be between fkey and hook port"));
                    return -1;
                }

                // Remove, Wait to EV_NODE_UPDATED to create cell
                graph.removeCells([cell]);

                var parent_ref = parent_cell.value.topic_name + "^";
                    parent_ref += parent_cell.value.topic_id + "^";
                    parent_ref += parent_cell.value.col.id

                var child_ref = child_cell.value.topic_name + "^";
                    child_ref += child_cell.value.topic_id;

                var options = {
                    list_dict: true
                };
                var kw_link = {
                    treedb_name: self.config.treedb_name,
                    parent_ref: parent_ref,
                    child_ref: child_ref,
                    options: options
                };

                // Wait to EV_NODE_UPDATED to fix cell
                self.gobj_publish_event("EV_LINK_RECORDS", kw_link, self);
            }
        }
        return 0;
    }

    /********************************************
     *  An edge has changed his source/target
     ********************************************/
    function ac_mx_connectcell(self, event, kw, src)
    {
        //  I don't know how to disable moving a link.
        // While refresh to clean the action

        self.gobj_publish_event("EV_REFRESH_TREEDB", {}, self);
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_mx_movecells(self, event, kw, src)
    {
        if(self.config.lock_publish_geometry) {
            return 0;
        }
        var cells = kw.cells;
        for(var i=0; i<cells.length; i++) {
            var cell = cells[i];
            if(cell.value.cell_name) {
                var _geometry = kw_get_dict_value(
                    cell.value.record,
                    "_geometry",
                    {},
                    true
                );
                if(!is_object(_geometry)) {
                    _geometry = {};
                    kw_set_dict_value(cell.value.record, "_geometry", _geometry);
                }
                __extend_dict__(
                    _geometry,
                    filter_dict(
                        cell.geometry,
                        ["x", "y", "width", "height"]
                    )
                );
                __extend_dict__(
                    _geometry,
                    {
                        __origin__: self.config.uuid
                    }
                );

                var options = {
                    list_dict: true,
                    autolink: false
                };
                var kw_update = {
                    treedb_name: self.config.treedb_name,
                    topic_name: cell.value.schema.topic_name,
                    record: cell.value.record,
                    options: options
                };

                // Don't save cells individually, Save all with button
                //self.gobj_publish_event("EV_UPDATE_RECORD", kw_update, self);
                set_btn_submmit_state(self, "save_graph", true);
            }
        }

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_mx_resizecells(self, event, kw, src)
    {
        if(self.config.lock_publish_geometry) {
            return 0;
        }

        var cells = kw.cells;
        for(var i=0; i<cells.length; i++) {
            var cell = cells[i];
            if(cell.isCollapsed()) {
                continue;
            }
            if(cell.value.cell_name) {
                var _geometry = filter_dict(
                    cell.geometry,
                    ["x", "y", "width", "height"]
                );
                __extend_dict__(
                    _geometry,
                    {
                        __origin__: self.config.uuid
                    }
                );
                kw_set_dict_value(cell.value.record, "_geometry", _geometry);

                var options = {
                    list_dict: true,
                    autolink: false
                };
                var kw_update = {
                    treedb_name: self.config.treedb_name,
                    topic_name: cell.value.schema.topic_name,
                    record: cell.value.record,
                    options: options
                };

                // Don't save cells individually, Save all with button
                //self.gobj_publish_event("EV_UPDATE_RECORD", kw_update, self);
                set_btn_submmit_state(self, "save_graph", true);
            }
        }

        return 0;
    }

    /********************************************
     *  From formtable,
     *  when window is destroying or minififying
     *
     *  Top toolbar informing of window close
     *  kw
     *      {destroying: true}   Window destroying
     *      {destroying: false}  Window minifying
     ********************************************/
    function ac_close_window(self, event, kw, src)
    {
        var graph = self.config._mxgraph;
        var model = graph.model;
        var cell_id = src.gobj_read_attr("user_data");  // HACK reference cell <-> gobj_formtable
        var cell = model.getCell(cell_id);

        if(kw.destroying) {
            if(cell) {
                cell.value.gobj_cell_formtable = 0;
            }
        }
        if(src == self) {
            self.gobj_publish_event(event, kw, self);
        }
        return 0;
    }

    /********************************************
     *  Create graph styles
     ********************************************/
    function ac_create_graph_styles(self, event, kw, src)
    {
        var graph = self.config._mxgraph;

        self.config.topics_style = kw.topics_style;

        for(var i=0; i<self.config.topics_style.length; i++) {
            var topic = self.config.topics_style[i];
            var topic_name = topic.topic_name;
            var graph_styles = topic.graph_styles;

            for(var style_name in graph_styles) {
                var style = graph_styles[style_name];
                create_graph_style(
                    graph,
                    topic_name + "_" + style_name,
                    style
                );
            }
        }

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
            "EV_LOAD_DATA",
            "EV_CLEAR_DATA",
            "EV_DESCS",
            "EV_NODE_CREATED",
            "EV_NODE_UPDATED",
            "EV_NODE_DELETED",

            "EV_MX_VERTEX_CLICKED: output",
            "EV_MX_EDGE_CLICKED: output",
            "EV_CREATE_RECORD: output",
            "EV_DELETE_RECORD: output",
            "EV_UPDATE_RECORD: output",
            "EV_LINK_RECORDS: output",
            "EV_UNLINK_RECORDS: output",
            "EV_RUN_NODE: output",
            "EV_REFRESH_TREEDB: output",
            "EV_SHOW_HOOK_DATA: output",
            "EV_SHOW_TREEDB_TOPIC: output",

            "EV_CREATE_VERTEX",
            "EV_DELETE_VERTEX",
            "EV_CLONE_VERTEX",
            "EV_DELETE_EDGE",
            "EV_SHOW_CELL_DATA_FORM",
            "EV_SHOW_CELL_DATA_JSON",
            "EV_POPUP_MENU",
            "EV_EXTEND_SIZE",

            "EV_MX_CLICK",
            "EV_MX_DOUBLECLICK",
            "EV_MX_SELECTION_CHANGE",
            "EV_MX_ADDCELLS",
            "EV_MX_MOVECELLS",
            "EV_MX_RESIZECELLS",
            "EV_MX_CONNECTCELL",

            "EV_CREATE_GRAPH_STYLES",
            "EV_CLOSE_WINDOW: output",
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
                ["EV_LOAD_DATA",                ac_load_data,               undefined],
                ["EV_CLEAR_DATA",               ac_clear_data,              undefined],
                ["EV_DESCS",                    ac_descs,                   undefined],
                ["EV_NODE_CREATED",             ac_node_created,            undefined],
                ["EV_NODE_UPDATED",             ac_node_updated,            undefined],
                ["EV_NODE_DELETED",             ac_node_deleted,            undefined],

                ["EV_CREATE_VERTEX",            ac_create_vertex,           undefined],
                ["EV_DELETE_VERTEX",            ac_delete_vertex,           undefined],
                ["EV_CLONE_VERTEX",             ac_clone_vertex,            undefined],
                ["EV_DELETE_EDGE",              ac_delete_edge,             undefined],

                ["EV_SHOW_HOOK_DATA",           ac_show_hook_data,          undefined],
                ["EV_SHOW_CELL_DATA_FORM",      ac_show_cell_data_form,     undefined],
                ["EV_SHOW_CELL_DATA_JSON",      ac_show_cell_data_json,     undefined],
                ["EV_CREATE_RECORD",            ac_create_record,           undefined],
                ["EV_UPDATE_RECORD",            ac_update_record,           undefined],

                ["EV_RUN_NODE",                 ac_run_node,                undefined],

                ["EV_POPUP_MENU",               ac_popup_menu,              undefined],
                ["EV_EXTEND_SIZE",              ac_extend_size,             undefined],
                ["EV_MX_CLICK",                 ac_mx_click,                undefined],
                ["EV_MX_DOUBLECLICK",           ac_mx_double_click,         undefined],
                ["EV_MX_SELECTION_CHANGE",      ac_selection_change,        undefined],
                ["EV_MX_ADDCELLS",              ac_mx_addcells,             undefined],
                ["EV_MX_MOVECELLS",             ac_mx_movecells,            undefined],
                ["EV_MX_RESIZECELLS",           ac_mx_resizecells,          undefined],
                ["EV_MX_CONNECTCELL",           ac_mx_connectcell,          undefined],

                ["EV_CREATE_GRAPH_STYLES",      ac_create_graph_styles,     undefined],
                ["EV_CLOSE_WINDOW",             ac_close_window,            undefined],
                ["EV_TOGGLE",                   ac_toggle,                  undefined],
                ["EV_SHOW",                     ac_show,                    undefined],
                ["EV_HIDE",                     ac_hide,                    undefined],
                ["EV_SELECT",                   ac_select,                  undefined],
                ["EV_REFRESH",                  ac_refresh,                 undefined],
                ["EV_REBUILD_PANEL",            ac_rebuild_panel,           undefined]
            ]
        }
    };

    var Mx_nodes_tree = GObj.__makeSubclass__();
    var proto = Mx_nodes_tree.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Mx_nodes_tree",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Mx_nodes_tree, "Mx_nodes_tree");




            /***************************
             *      Framework Methods
             ***************************/




    /************************************************
     *      Framework Method create
     ************************************************/
    proto.mt_create = function(kw)
    {
        var self = this;

        load_icons(self);

        self.config.uuid = get_unique_id("me");

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
    exports.Mx_nodes_tree = Mx_nodes_tree;

})(this);
