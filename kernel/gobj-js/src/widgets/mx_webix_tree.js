/***********************************************************************
 *          mx_webix_tree.js
 *
 *          Webix Tree with mxgrah
 *
 *          "Container Panel"
 *
 *
        Schema
        ------

        Vista con mxgraph de un arbol jerarquico de gobjs (js),
        usando la estructura estilo webix:
            {
                "id":
                "value":
                "data": []                                  NOTE hook of self
            }

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
        panel_properties: {},   // creator can set "Container Panel" properties
        ui_properties: null,    // creator can set webix properties

        $ui: null,
        locked: false,

        layout_options: [
            {
                id: "tree_layout",
                value: "Compact Tree Layout",
                layout: function(layout_option, graph) {
                    // Enables automatic layout on the graph and installs
                    // a tree layout for all groups who's children are
                    // being changed, added or removed.
                    var layout = new mxCompactTreeLayout(graph, false);
                    layout.useBoundingBox = false;
                    layout.edgeRouting = false;
                    layout.levelDistance = 30;
                    layout.nodeDistance = 10;
                    return layout;
                }
            },

            {
                id: "edgeLabel_layout",
                value: "EdgeLabel Layout",
                layout: function(layout_option, graph) {
                    var layout = new mxEdgeLabelLayout(graph);
                    return layout;
                }
            },
            {
                id: "parallelEdge_layout",
                value: "ParallelEdge Layout",
                layout: function(layout_option, graph) {
                    var layout = new mxParallelEdgeLayout(graph);
                    return layout;
                }
            },
            {
                id: "partition_layout",
                value: "Partition Layout",
                layout: function(layout_option, graph) {
                    var layout = new mxPartitionLayout(graph);
                    return layout;
                }
            },
            {
                id: "radial_layout",
                value: "Radial Layout",
                layout: function(layout_option, graph) {
                    var layout = new mxRadialTreeLayout(graph);
                    return layout;
                }
            },
            {
                id: "stack_layout",
                value: "Stack Layout",
                layout: function(layout_option, graph) {
                    var layout = new  mxStackLayout(graph);

                    layout.resizeParent = true;
                    layout.horizontal = false;
                    layout.spacing = 10;

                    return layout;
                }
            },
            {
                id: "swimlane_layout",
                value: "Swimlane Layout",
                layout: function(layout_option, graph) {
                    var layout = new mxSwimlaneLayout(graph);
                    layout.orientation = "east";
                    return layout;
                }
            },


            {
                id: "herarchical_layout",
                value: "Herarchical Layout",
                layout: function(layout_option, graph) {
                    var layout = new mxHierarchicalLayout(graph);
                    return layout;
                }
            },
            {
                id: "fastorganic_layout",
                value: "FastOrganic Layout",
                layout: function(layout_option, graph) {
                    var layout = new mxFastOrganicLayout(graph);
                    return layout;
                }
            },
            {
                id: "circle_layout",
                value: "Circle Layout",
                layout: function(layout_option, graph) {
                    var layout = new mxCircleLayout(graph);
                    return layout;
                }
            }

        ],

        layers: [
            {
                id: "__mx_default_layer__"
            }
        ],

        _mxgraph: null,

        layout_selected: "tree_layout",

        __writable_attrs__: [
            "layout_selected"
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
        // Destroy UI, Build UI
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
        rebuild_layouts(self);
    }

    /************************************************************
     *   Webix UI
     ************************************************************/
    function build_webix(self)
    {
        var toolbar = {
            view:"toolbar",
            height: 30,
            css: "toolbar2color",
            cols:[
                {
                    view: "richselect",
                    id: build_name(self, "layout_options"),
                    tooltip: t("Select layout"),
                    width: 180,
                    options: self.config.layout_options,
                    value: self.config.layout_selected,
                    label: "",
                    on: {
                        onChange: function(newVal, oldVal) {
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

                            if(self.gobj_is_unique()) {
                                self.gobj_save_persistent_attrs();
                            }
                        }
                    }
                },
                {
                    view:"button",
                    type: "icon",
                    icon: "fad fa-compress-arrows-alt",
                    css: "webix_transparent icon_toolbar_16",
                    maxWidth: 120,
                    label: t("reset view"),
                    click: function() {
                        self.config._mxgraph.view.scaleAndTranslate(1, 0, 0);
                    }
                },
                {
                    view:"button",
                    type: "icon",
                    icon: "fad fa-expand-arrows-alt",
                    css: "webix_transparent icon_toolbar_16",
                    maxWidth: 120,
                    label: t("fit"),
                    click: function() {
                        self.config._mxgraph.fit();
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
                        self.config._mxgraph.zoomIn();
                        self.config._mxgraph.view.setTranslate(0, 0);
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
                        self.config._mxgraph.zoomOut();
                        self.config._mxgraph.view.setTranslate(0, 0);
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
                            graph.setCellsLocked(false);

                            graph.rubberband.setEnabled(true);
                            graph.panningHandler.useLeftButtonForPanning = false;

                            self.config.locked = false;
                            this.define("icon", "far fa-lock-open-alt");
                            this.define("label", t("lock vertices"));
                        } else {
                            graph.setCellsLocked(true);

                            graph.rubberband.setEnabled(false);
                            graph.panningHandler.useLeftButtonForPanning = true;

                            self.config.locked = true;
                            this.define("icon", "far fa-lock-alt");
                            this.define("label", t("unlock vertices"));
                        }
                        this.refresh();
                    }
                },
                { view:"label", label: ""}
            ]
        };

        /*---------------------------------------*
         *      UI
         *---------------------------------------*/
        self.config.$ui = webix.ui({
            id: self.gobj_name(),
            rows: [
                get_container_panel_top_toolbar(self),
                {
                    view: "mxgraph",
                    id: build_name(self, "mxgraph"),
                    gobj: self
                },
                toolbar
            ]
        });

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
        self.config.$ui.attachEvent("onViewShow", function() {
            self.parent.gobj_send_event("EV_ON_VIEW_SHOW", self, self);
        });
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
        var group = get_layer(self, "layer?");

        var cur_layout = kwid_collect(
            self.config.layout_options,
            self.config.layout_selected,
            null, null
        )[0];
        if(!cur_layout) {
            cur_layout = self.config.layout_options[0];
        }

        if(cur_layout) {
            graph.getModel().beginUpdate();
            try {
                cur_layout.exe.execute(group);
            } catch (e) {
                log_error(e);
            } finally {
                graph.getModel().endUpdate();
            }
        }
    }

    /********************************************
     *  Create root and layers
     ********************************************/
    function create_root_and_layers(graph, layers)
    {
        var root = null;
        if(layers && layers.length) { // TODO
            root = new mxCell();
            root.setId("__mx_root__");

            for(var i=0; i<layers.length; i++) {
                var layer = layers[i];

                // Create the layer
                var __mx_cell__ = root.insert(new mxCell());

                // Set reference
                layer["__mx_cell__"] = __mx_cell__;

                var id = kw_get_str(layer, "id", null, false);
                if(id) {
                    __mx_cell__.setId(id);
                }
            }
        } else {
            root = graph.getModel().createRoot()
        }

        graph.getModel().beginUpdate();
        try {
            graph.getModel().setRoot(root);
        } catch (e) {
            log_error(e);
        } finally {
            graph.getModel().endUpdate();
        }
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

        create_root_and_layers(graph, self.config.layers);

        graph.border = 40;
        graph.view.setTranslate(graph.border/2, graph.border/2);

        // Enables rubberband selection
        graph.rubberband = new mxRubberband(graph);
        graph.rubberband.setEnabled(false);

        graph.setPanning(true);
        graph.panningHandler.useLeftButtonForPanning = true;

        // Negative coordenates?
        graph.allowNegativeCoordinates = false;

        // Multiple connections between the same pair of vertices.
        graph.setMultigraph(false);

        // Enable/Disable basic selection and cell handling
        graph.setEnabled(true);
        graph.setCellsDisconnectable(false); // Modificar egdes/links

        // Enable/Disable tooltips
        graph.setTooltips(true);

        // Adds a highlight on the cell under the mousepointer
        new mxCellTracker(graph);

        // Celdas seleccionables? (marco de redimensionamiento)
        graph.setCellsSelectable(true);

        // Avoids overlap of edges and collapse icons
        graph.keepEdgesInBackground = true;

        // Enables automatic sizing for vertices after editing
        graph.setAutoSizeCells(true);

        // Creates the default style for vertices
        var style = [];
        style[mxConstants.STYLE_SHAPE] = mxConstants.SHAPE_RECTANGLE;
        style[mxConstants.STYLE_PERIMETER] = mxPerimeter.RectanglePerimeter;
        style[mxConstants.STYLE_ROUNDED] = true;
        style[mxConstants.STYLE_STROKECOLOR] = 'gray';
        style[mxConstants.STYLE_FILLCOLOR] = '#D2E3EF';
        style[mxConstants.STYLE_GRADIENTCOLOR] = 'white';
        style[mxConstants.STYLE_FONTCOLOR] = '#774400';
        style[mxConstants.STYLE_ALIGN] = mxConstants.ALIGN_CENTER;
        style[mxConstants.STYLE_VERTICAL_ALIGN] = mxConstants.ALIGN_MIDDLE;
        style[mxConstants.STYLE_FONTSIZE] = '12';
        style[mxConstants.STYLE_FONTSTYLE] = 1;
        graph.getStylesheet().putDefaultVertexStyle(style);

        graph.setHtmlLabels(true);

        // Enables automatic layout on the graph and installs
        // a tree layout for all groups who's children are
        // being changed, added or removed.
//         var layout = new mxCompactTreeLayout(graph, false);
//         layout.useBoundingBox = false;
//         layout.edgeRouting = false;
//         layout.levelDistance = 30;
//         layout.nodeDistance = 10;
//
//         var layoutMgr = new mxLayoutManager(graph);
//
//         layoutMgr.getLayout = function(cell)
//         {
//             if (cell.getChildCount() > 0)
//             {
//                 return layout;
//             }
//         };

        // Handles clicks on cells
        graph.addListener(mxEvent.CLICK, function(sender, evt) {
            var cell = evt.getProperty('cell');
            if (cell != null) {
                //var record = evt.properties.cell.value; TODO save record in value
                if(cell.isVertex()) {
                    self.parent.gobj_send_event("EV_MX_VERTEX_CLICKED", cell, self);
                } else {
                    self.parent.gobj_send_event("EV_MX_EDGE_CLICKED", cell, self);
                }
            }
        });
    }

    /************************************************************
     *
     ************************************************************/
    function br(short_name)
    {
        var n = short_name.split('^');
        return n[0] + "^<br/>" + n[1];
    }

    /************************************************************
     *
     ************************************************************/
    function get_layer(self, layer)
    {
// TODO        var layers = self.config.layers;
//         for(var i=0; i<layers.length; i++) {
//             if(layers[i].id == layer) {
//                 return layers[i].__mx_cell__;
//             }
//         }
//         var x = self.config._mxgraph.getModel().getCell(layer);
        return self.config._mxgraph.getDefaultParent();
    }

    /************************************************************
     *
     ************************************************************/
    function _load_webix_tree(self, group, x, y, parent, childs)
    {
        var cx=120, cy=50, sep=30;
        var style = "";

        /*
         *  Paint Childs
         */
        for(var i=0; i<childs.length; i++) {
            var record = childs[i];
            var child = self.config._mxgraph.insertVertex(
                group,
                record.id,
                br(record.value),
                x, y, cx, cy,
                style
            );
            if(parent) {
                self.config._mxgraph.insertEdge(
                    group,          // group
                    null,           // id
                    '',             // value
                    parent,         // source
                    child,          // target
                    null            // style
                );
            }
            if(kw_has_key(record, "data")) {
                _load_webix_tree(
                    self,
                    group,
                    x,
                    y + cy + sep,
                    child,
                    record.data
                );
            }
            x += cx + sep;
        }

    }

    /************************************************************
     *
     ************************************************************/
    function load_webix_tree(self, data, layer)
    {
        var group = get_layer(self, layer);
        var x=0;
        var y=0; // si meto separación aparece scrollbar al ajustar
        _load_webix_tree(self, group, x, y, 0, data);

        var cur_layout = kwid_collect(
            self.config.layout_options,
            self.config.layout_selected,
            null, null
        )[0];
        if(!cur_layout) {
            cur_layout = self.config.layout_options[0];
        }

        if(cur_layout) {
            cur_layout.exe.execute(group);
        }
    }




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *
     ********************************************/
    function ac_select_item(self, event, kw, src)
    {
        self.config._mxgraph.setSelectionCell(self.config._mxgraph.model.getCell(kw.id));
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_load_data(self, event, kw, src)
    {
        var layer = kw.layer;
        var data = __duplicate__(kw.data);

        var model = self.config._mxgraph.getModel();
        model.beginUpdate();
        try {
            switch(kw.type) {
                case "webix-tree":
                default:
                    load_webix_tree(self, data, layer);
                    break;
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
    function ac_clear_data(self, event, kw, src)
    {
        // Get current index, remove UI from parent, re-build UI, add UI to parent with same idx.
        var idx = self.config.$container_parent.index(self.config.$ui);
        if(idx < 0) {
            return -1;
        }
        var visible = self.parent.config.views_opened?
            self.parent.config.views_opened[self.name]:true;
        self.config.$container_parent.removeView(self.config.$ui);
        rebuild(self);
        self.config.$container_parent.addView(self.config.$ui, idx);
        if(visible) {
            self.config.$ui.show();
        } else {
            self.config.$ui.hide();
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

    /*************************************************************
     *  Refresh, order from container
     *  provocado por entry/exit de fullscreen
     *  o por redimensionamiento del panel, propio o de hermanos
     *
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
            "EV_SELECT_ITEM",
            "EV_LOAD_DATA",
            "EV_CLEAR_DATA",
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
                ["EV_SELECT_ITEM",          ac_select_item,     undefined],
                ["EV_LOAD_DATA",            ac_load_data,       undefined],
                ["EV_CLEAR_DATA",           ac_clear_data,      undefined],
                ["EV_SELECT",               ac_select,          undefined],
                ["EV_REFRESH",              ac_refresh,         undefined],
                ["EV_REBUILD_PANEL",        ac_rebuild_panel,   undefined]
            ]
        }
    };

    var Mx_webix_tree = GObj.__makeSubclass__();
    var proto = Mx_webix_tree.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Mx_webix_tree",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Mx_webix_tree, "Mx_webix_tree");




            /***************************
             *      Framework Methods
             ***************************/




    /************************************************
     *      Framework Method create
     ************************************************/
    proto.mt_create = function(kw)
    {
        var self = this;

        var cur_layout = kwid_collect(
            self.config.layout_options,
            self.config.layout_selected,
            null, null
        )[0];
        if(!cur_layout) {
            cur_layout = self.config.layout_options[0];
        }
        self.config.layout_selected = cur_layout.id;

        rebuild(self);

        $$(build_name(self, "layout_options")).define("options", self.config.layout_options);
        $$(build_name(self, "layout_options")).setValue(cur_layout.id);
    }

    /************************************************
     *      Framework Method destroy
     *      In this point, all childs
     *      and subscriptions are already deleted.
     ************************************************/
    proto.mt_destroy = function()
    {
        var self = this;
        if(self.config._mxgraph) {
            self.config._mxgraph.destroy();
            self.config._mxgraph = null;
        }
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
        mxEvent.disableContextMenu(
            $$(build_name(self, "mxgraph")).getNode()
        );
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
    exports.Mx_webix_tree = Mx_webix_tree;

})(this);
