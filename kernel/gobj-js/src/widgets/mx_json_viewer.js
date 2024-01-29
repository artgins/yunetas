/***********************************************************************
 *          mx_json_viewer.js
 *
 *          Json with mxgrah
 *
 *          Mix "Container Panel" & "Pinhold Window"
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

        path: null,
        json_data: null,

        locked: true,
        fitted: false,

        group_cx_sep: 80,
        group_cy_sep: 40,

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
            "fitted",
            "path",
            "json_data"
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
    }

    /************************************************************
     *   Webix UI
     ************************************************************/
    function build_webix(self)
    {
        /*---------------------------------------*
         *      Bottom Toolbar
         *---------------------------------------*/
        var bottom_toolbar = {
            view:"toolbar",
            height: 30,
            css: "toolbar2color",
            cols:[
                {
                    view: "richselect",
                    id: build_name(self, "layout_options"),
                    hidden: true,
                    tooltip: t("Select layout"),
                    minWidth: 180,
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
                    autosize: true,
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
                    autosize: true,
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
                    icon: self.config.locked? "far fa-lock-alt":"far fa-lock-open-alt",
                    css: "webix_transparent icon_toolbar_16",
                    autosize: true,
                    label: t("unlock vertices"),
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
                {
                    view: "button",
                    type: "icon",
                    icon: "fas fa-sync",
                    autosize: true,
                    css: "webix_transparent btn_icon_toolbar_18",
                    tooltip: t("refresh"),
                    label: t("refresh"),
                    click: function() {
                        if(!this.getTopParentView().config.fullscreen) {
                            // En fullscreen es un desastre si reconstruimos webix
                            self.gobj_send_event("EV_CLEAR_DATA", {}, self);
                        }
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
                    rows: [
                        {
                            view: "mxgraph",
                            background_color: "#fefffe",
                            id: build_name(self, "mxgraph"),
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
                rows: [
                    // HACK "Container Panel" toolbar suministrada por ui_container
                    get_container_panel_top_toolbar(self),
                    {
                        view: "mxgraph",
                        background_color: "#fefffe",
                        id: build_name(self, "mxgraph"),
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
            graph.getModel().beginUpdate();
            try {
                cur_layout.exe.execute(group);
            } catch (e) {
                log_error(e);
            } finally {
                graph.getModel().endUpdate();
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
     *  Create root and layers
     ********************************************/
    function create_root_and_layers(self)
    {
        var graph = self.config._mxgraph;
        var root = null;

        root = new mxCell();
        root.setId("__mx_root__");

        // Create the layer
        var __mx_cell__ = root.insert(new mxCell());

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
    function get_layer(self, layer)
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

        // Enables rubberband selection
        graph.rubberband = new mxRubberband(graph);
        graph.rubberband.setEnabled(false);

        graph.setPanning(true);
        graph.panningHandler.useLeftButtonForPanning = true;

        // Negative coordenates?
        graph.allowNegativeCoordinates = false;

        // Multiple connections between the same pair of vertices.
        graph.setMultigraph(true);

        // Avoids overlap of edges and collapse icons
        graph.keepEdgesInBackground = false;

        graph.setAutoSizeCells(true);

        /*---------------------------*
         *      PERMISOS
         *---------------------------*/
        // Enable/Disable cell handling
        graph.setEnabled(true);

        graph.setConnectable(false); // Crear edges/links
        graph.setCellsDisconnectable(false); // Modificar egdes/links
        mxGraphHandler.prototype.setCloneEnabled(false); // Ctrl+Drag will clone a cell
        graph.setCellsLocked(self.config.locked);
        graph.setPortsEnabled(true);
        graph.setCellsEditable(false);

        mxGraph.prototype.isCellSelectable = function(cell) {
            return true;
        };

        /*
         *  HACK Por defecto si los hijos salen un overlap del 50%
         *  se quitan del padre y pasan al default
         */
        graph.graphHandler.setRemoveCellsFromParent(false); // HACK impide quitar hijos
        mxGraph.prototype.isAllowOverlapParent = function(cell) { return true;}
        mxGraph.prototype.defaultOverlap = 1; // Permite a hijos irse tan lejos como quieran

        /*
         *  General Vertex Style
         */
        var style = graph.getStylesheet().getDefaultVertexStyle();
        style[mxConstants.STYLE_FONTFAMILY] = 'monospace, "dejavu sans mono", "droid sans mono", consolas, monaco, "lucida console", sans-serif, "courier new", courier';
        style[mxConstants.STYLE_FONTSIZE] = '14';
        style[mxConstants.STYLE_FONTSTYLE] = '0';

        /*
         *  General Edge Style
         */
        style = graph.getStylesheet().getDefaultEdgeStyle();
        style[mxConstants.STYLE_ROUNDED] = true;
        style[mxConstants.STYLE_STROKEWIDTH] = '1';

        // style[mxConstants.STYLE_EDGE] = mxEdgeStyle.ElbowConnector;
        // style[mxConstants.STYLE_EDGE] = mxEdgeStyle.EntityRelation;
        // style[mxConstants.STYLE_EDGE] = mxEdgeStyle.Loop;
        // style[mxConstants.STYLE_EDGE] = mxEdgeStyle.SideToSide;
        // style[mxConstants.STYLE_EDGE] = mxEdgeStyle.TopToBottom;
        // style[mxConstants.STYLE_EDGE] = mxEdgeStyle.OrthConnector;
        // style[mxConstants.STYLE_EDGE] = mxEdgeStyle.SegmentConnector;

        create_graph_style(
            graph,
            "string",
            "text;html=1;strokeColor=none;fillColor=none;align=left;whiteSpace=nowrap;rounded=0;shadow=1;glass=0;sketch=0;fontColor=#1A1A1A;"
        );
        create_graph_style(
            graph,
            "number",
            "text;html=1;strokeColor=none;fillColor=none;align=left;whiteSpace=nowrap;rounded=0;shadow=1;glass=0;sketch=0;fontColor=#1A1A1A;"
        );
        create_graph_style(
            graph,
            "boolean",
            "text;html=1;strokeColor=none;fillColor=none;align=left;whiteSpace=nowrap;rounded=0;shadow=1;glass=0;sketch=0;fontColor=#1A1A1A;"
        );
        create_graph_style(
            graph,
            "null",
            "text;html=1;strokeColor=none;fillColor=none;align=left;whiteSpace=nowrap;rounded=0;shadow=1;glass=0;sketch=0;fontColor=#1A1A1A;"
        );
        create_graph_style(
            graph,
            "list",
            "text;html=1;strokeColor=none;fillColor=none;align=left;whiteSpace=nowrap;rounded=0;shadow=1;glass=0;sketch=0;fontColor=#1A1A1A;"
        );
        create_graph_style(
            graph,
            "dict",
            "text;html=1;strokeColor=none;fillColor=none;align=left;whiteSpace=nowrap;rounded=0;shadow=1;glass=0;sketch=0;fontColor=#1A1A1A;"
        );

        // Handles clicks on cells
        graph.addListener(mxEvent.CLICK, function(sender, evt) {
            var cell = evt.getProperty('cell');
            if (cell != null) {
                var id = evt.properties.cell.id;
                if(cell.isVertex()) {
                    self.gobj_publish_event("EV_MX_JSON_ITEM_CLICKED", {id:id});
                }
            }
        });

        /*
         *  Own getLabel
         */
        graph.setHtmlLabels(true);

        /*
         *  Tooltip
         */
        graph.setTooltips(true);
        graph.getTooltipForCell = function(cell) {
            var tip = null;
            if (cell != null && cell.getTooltip != null) {
                tip = cell.getTooltip();
            } else {
                if(cell.id && cell.isVertex()) {
                    return cell.id;
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
    }

    /************************************************************
     *
     ************************************************************/
    function _load_json(self, path, kw, parent_group, parent_port, levels, level)
    {
        // HACK is already in a beginUpdate/endUpdate
        var graph = self.config._mxgraph;
        var model = graph.getModel();
        var layer = get_layer(self);

        var cx = 100;
        var cy = 16; // 14 fontSize
        var cy_sep = 4;

        var cells = []; // Cells of this new group
        var pending_complex_fields = [];

        /*------------------------------------------------------------*
         *      First Step: all keys (simple an complex) in a group
         *------------------------------------------------------------*/

        /*-----------------------------------------------*
         *      Firstly simple data
         *  HACK initially all cells belongs to layer,
         *  when grouping will belong to the group
         *-----------------------------------------------*/
        var x = 0;
        var y = cy + cy_sep; // creo que no hace nada, al agrupar las celdas se pierde
        if(is_object(kw)) {
            for(var k in kw) {
                var v = kw[k];

                var type = "";
                if(is_string(v)) {
                    type = "string";
                } else if(is_null(v)) {
                    type = "null";
                } else if(is_number(v)) {
                    type = "number";
                } else if(is_boolean(v)) {
                    type = "boolean";
                } else if(is_object(v)) {
                    type = "dict";
                } else if(is_array(v)) {
                    type = "list";
                } else {
                    log_error("What fack is this?");
                    log_error(v);
                }

                var value = build_cell_value(self, type, k, v);

                var cell = graph.insertVertex(
                    layer,                  // group
                    add_segment(path, k),   // id
                    value,                  // value
                    x, y,                   // x,y
                    cx, cy,                 // width,height
                    type,                   // style
                    false                   // relative
                );
                y += cy + cy_sep;
                graph.autoSizeCell(cell);
                cells.push(cell);

                if((is_array(v) && v.length>0) || (is_object(v) && json_object_size(v)>0)) {
                    // Guarda para desplegar luego
                    var pending = {
                        kw: v,
                        cell: cell,
                        segment: k
                    }
                    pending_complex_fields.push(pending);
                }
            }

        } else if(is_array(kw)) {
            for(var i=0; i<kw.length; i++) {
                var v = kw[i];

                var type = "";
                if(is_string(v)) {
                    type = "string";
                } else if(is_null(v)) {
                    type = "null";
                } else if(is_number(v)) {
                    type = "number";
                } else if(is_boolean(v)) {
                    type = "boolean";
                } else if(is_object(v)) {
                    type = "dict";
                } else if(is_array(v)) {
                    type = "list";
                } else {
                    log_error("What fack is this?");
                    log_error(v);
                }

                var value = build_cell_value(self, type, i, v);

                var cell = graph.insertVertex(
                    layer,                  // group
                    add_segment(path, i),   // id
                    value,                  // value
                    x, y,                   // x,y
                    cx, cy,                 // width,height
                    type,                   // style
                    false                   // relative
                );
                y += cy + cy_sep;
                graph.autoSizeCell(cell);
                cells.push(cell);

                if((is_array(v) && v.length>0) || (is_object(v) && json_object_size(v)>0)) {
                    // Guarda para desplegar luego
                    var pending = {
                        kw: v,
                        cell: cell,
                        segment: i
                    }
                    pending_complex_fields.push(pending);
                }
            }
        }

        /*--------------------------------------------------------------*
         *      Create the group for all fields, simples and complexes
         *--------------------------------------------------------------*/
        var group_style = "";
        var group_value = "";
        if(is_object(kw)) {
            // Style dict group
            group_style =
            "whiteSpace=nowrap;html=1;fillColor=#FBFBFB;strokeColor=#006658;fontColor=#4C0099;dashed=1;rounded=1;labelPosition=center;verticalLabelPosition=middle;align=center;verticalAlign=top;spacingTop=0;strokeWidth=1;fontStyle=1;foldable=0;opacity=60;";
            group_value = get_two_last_segment(path);
        } else {
            // Style list group
            group_style =
            "whiteSpace=nowrap;html=1;fillColor=#fffbd1;strokeColor=#006658;fontColor=#4C0099;dashed=1;rounded=0;labelPosition=center;verticalLabelPosition=middle;align=center;verticalAlign=top;spacingTop=0;strokeWidth=1;fontStyle=1;foldable=0;opacity=60;";
            group_value = get_two_last_segment(path);
        }

        var group = new mxCell(
            group_value, // value
            new mxGeometry(),
            group_style // HACK no uses style con create_graph_style(), falla
        );
        group.setVertex(true);
        group.setConnectable(false);

        graph.groupCells1(
            group,
            0, // border between the child area and the group bounds
            cells
        );

        graph.updateGroupBounds(
            [group],    // cells
            15,         // border
            false,      // moveGroup
            10,         // topBorder
            0,          // rightBorder
            0,          // bottomBorder
            0           // leftBorder
        );
        if(group.geometry.width < 200) {
            group.geometry.width = 200;
        }
        group.setId(path); // HACK siempre después de groupCells() porque le pone un id
        group.pending_complex_fields = pending_complex_fields;
        group.parent_group = parent_group;
        group.parent_port = parent_port;

        if(levels[level] === undefined) {
            levels[level] = {
                x: 0,
                y: 0,
                width: 0,
                height: 0,
                groups: []
            }
        }
        levels[level].groups.push(group);

        /*------------------------------------------------------------*
         *      Second Step: create the link of group (complex json)
         *      with his parent_port
         *------------------------------------------------------------*/
        if(parent_port) {
            /*
             *  Create the group port
             */
            var port_size = 10;
            var port = graph.insertVertex(
                group,              // group
                null,               // id
                null,               // value
                group.geometry.width/2 -port_size/2,         // x
                -port_size,             // y
                port_size, port_size,   // width,height
                "",                 // style
                false               // relative
            );

            graph.insertEdge(
                layer,          // parent
                null,           // id
                '',             // value
                parent_port,    // source
                port,           // target
                "edgeStyle=orthogonalEdgeStyle;orthogonalLoop=1;jettySize=auto;html=1;curved=1;"
            );
        }

        /*------------------------------------------------------------*
         *      Third Step: recursive with complex values
         *------------------------------------------------------------*/
        level ++;

        for(var i=pending_complex_fields.length; i>0; i--) {
            var pending = pending_complex_fields[i-1];

            _load_json(
                self,
                add_segment(path, pending.segment),   // path
                pending.kw,     // kw
                group,          // parent_group
                pending.cell,   // parent_port
                levels,         // levels
                level           // current level
            );
        }

        return group;
    }

    /************************************************************
     *  Build value of a cell
     ************************************************************/
    function build_cell_value(self, type, key, v)
    {
        // #006000 verde string
        // #EE422E rojo number
        // #FF8C00 naranja boolean
        // #475ED0 azul null
        var color = "black";
        var prefix = "";

        var value = "";
        switch(type) {
            case "string":
                color = "#006000";
                prefix = "•";
                value = '"' + v + '"';
                break;
            case "number":
                color = "#EE422E";
                prefix = "•";
                value = v;
                break;
            case "boolean":
                color = "#FF8C00";
                prefix = "•";
                value = v?"true":"false";
                break;
            case "null":
                color = "#475ED0";
                prefix = "•";
                value = "null";
                break;
            case "list":
                color = "black";
                prefix = "•";
                value = "[" + v.length+ "]";
                break;
            case "dict":
                color = "black";
                prefix = "•";
                value = "{" + json_object_size(v) + "}";
                break;
            case "container_dict":
                color = "black";
                value = "";
                break;
            case "container_list":
                color = "black";
                value = "";
                break;
            default:
                color = "black";
                break;
        }
        var t = "";
        t += "<span style='display:inline;color:#1A1A1A'>" +
            prefix + " " + key +
            ": </span>";
        t += "<span style='display:inline;color:" +
            color + "'>" + value +
            "</span>";
        return t;
    }

    /************************************************************
     *
     ************************************************************/
    function get_last_segment(path)
    {
        var segment = path.split("`");
        if(segment.length > 0) {
            return segment[segment.length-1];
        }
        return "";
    }

    /************************************************************
     *
     ************************************************************/
    function get_two_last_segment(path)
    {
        var segment = path.split("`");
        if(segment.length >=2) {
            return segment[segment.length-2] + "`" + segment[segment.length-1];
        }
        return "";
    }

    /************************************************************
     *
     ************************************************************/
    function add_segment(path, segment)
    {
        var new_path = path;
        if(path.length) {
            new_path += "`";
        }
        new_path += segment;
        return new_path;
    }

    /************************************************************
     *
     ************************************************************/
    function set_positions(self, graph, model, levels)
    {
        for(var i=0; i<levels.length; i++) {
            var level = levels[i];

            // Set the level cx/cy with the bigger group
            for(var j=0; j<level.groups.length; j++) {
                var group = level.groups[j];
                level.width = Math.max(level.width, group.geometry.width);
                level.height = Math.max(level.height, group.geometry.height);
            }
            if(i==0) {
                level.y = 0;
            } else {
                level.y = levels[i-1].y + levels[i-1].height + self.config.group_cy_sep;
            }
            for(var j=0; j<level.groups.length; j++) {
                var group = level.groups[j];
                if(j==0) {
                    level.x = (group.parent_group)?group.parent_group.geometry.x:0;
                    level.x += (group.parent_port)?group.parent_port.geometry.width:0;
                    if(i>0) {
                        level.x += self.config.group_cy_sep*2;
                    }
                } else {
                    level.x += level.groups[j-1].geometry.width;
                    level.x += self.config.group_cx_sep;
                }
                var geo = graph.getCellGeometry(group).clone();
                geo.x = level.x;
                geo.y = level.y;
                model.setGeometry(group, geo);
            }
        }
    }

    /************************************************************
     *
     ************************************************************/
    function load_json(self)
    {
        var json = __duplicate__(self.config.json_data);
        if(!json) {
            return;
        }
        var path = kw_get_str(self.config, "path", "`", false);
        if(empty_string(path)) {
            path = "`";
        }
        var graph = self.config._mxgraph;
        var model = graph.getModel();
        model.beginUpdate();
        try {
            var levels = [
                { // level 0
                    x: 0,
                    y: 0,
                    width: 0,
                    height: 0,
                    groups: []
                }
            ];

            _load_json(
                self,
                path,       // path
                json,       // kw
                null,       // parent_group,
                null,       // parent_port,
                levels,     // levels
                0           // current level
            );
            set_positions(self, graph, model, levels);

            graph.view.setTranslate(graph.border, graph.border);
            execute_layout(self);

        } catch (e) {
            log_error(e);
        } finally {
            model.endUpdate();
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
        var cell = self.config._mxgraph.model.getCell(kw.id);
        self.config._mxgraph.setSelectionCell(cell);

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_load_data(self, event, kw, src)
    {
        self.config.path = kw.path;
        self.config.json_data = kw.data;
        load_json(self);

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_clear_data(self, event, kw, src)
    {
        rebuild(self);
        load_json(self);
        self.config.$ui.show();
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
            "EV_MX_JSON_ITEM_CLICKED: output no_warn_subs",
            "EV_SELECT_ITEM",
            "EV_LOAD_DATA",
            "EV_CLEAR_DATA",
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
                ["EV_SELECT_ITEM",          ac_select_item,     undefined],
                ["EV_LOAD_DATA",            ac_load_data,       undefined],
                ["EV_CLEAR_DATA",           ac_clear_data,      undefined],
                ["EV_TOGGLE",               ac_toggle,          undefined],
                ["EV_SHOW",                 ac_show,            undefined],
                ["EV_HIDE",                 ac_hide,            undefined],
                ["EV_SELECT",               ac_select,          undefined],
                ["EV_REFRESH",              ac_refresh,         undefined],
                ["EV_REBUILD_PANEL",        ac_rebuild_panel,   undefined]
            ]
        }
    };

    var Mx_json_viewer = GObj.__makeSubclass__();
    var proto = Mx_json_viewer.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Mx_json_viewer",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Mx_json_viewer, "Mx_json_viewer");




            /***************************
             *      Framework Methods
             ***************************/




    /************************************************
     *      Framework Method create
     ************************************************/
    proto.mt_create = function(kw)
    {
        var self = this;

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
        if(self.config._mxgraph) {
            self.config._mxgraph.destroy();
            self.config._mxgraph = null;
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
        mxEvent.disableContextMenu(
            $$(build_name(self, "mxgraph")).getNode()
        );
        load_json(self);
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
    exports.Mx_json_viewer = Mx_json_viewer;

})(this);
