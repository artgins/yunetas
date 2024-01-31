/***********************************************************************
 *          ui_pinhold.js
 *
 *          Holder of pin's
 *          NOTE search "Pinhold Window" phrase to locate related code
 *
 *  window_properties: all values are false by default

        with_window_pin_btn: false,             // true: Show pin button
        without_window_fullscreen_btn: false,   // true: Hide fullscreen button
        without_window_close_btn: false,        // true: Hide minimize/destroy button
        without_destroy_window_on_close: false, // true: No destroy window on close (hide)
        with_create_window_on_start: false,     // true: Create window on start
 *
 *
 *  Version
 *  -------
 *  1.0     Initial release
 *  1.1     Change attributes with_window_pin_btn,with_create_window_on_start
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
        ui_properties: null,
        $ui: null,

        layout_options: [
            {
                id: "stack_layout",
                value: "Stack Layout",
                layout: function(layout_option, graph) {
                    var layout = new mxStackLayout(graph);
                    layout.spacing = 5;
                    return layout;
                }
            }
        ],
        layout_selected: "stack_layout",

        vertex_cx: 140,
        vertex_cy: 80,

        layers: [
            {
                id: "__mx_default_layer__"
            }
        ],
        _mxgraph: null,

        showing: false,
        left: 0,
        top: 0,
        width: 400,
        height: 140,

        gobj_name_in_fullscreen: "",

        windows_pinpushed: {
        },

        __writable_attrs__: [
            "gobj_name_in_fullscreen",
            "windows_pinpushed",
            "showing",
            "left",
            "top",
            "width",
            "height"
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
        var toolbar = {
            view: "toolbar",
            id: build_name(self, "pinhold_toolbar"),
            css: "pinhold_toolbar"
        };
        // FUTURE que la mini-ventana se pinche algún botón y
        // sea presentación de diapositivas fullscreen
        // con presentación configurable (tiempo de visualización, animación, ...)
        var popup = {
            view: "window",
            id: self.gobj_escaped_short_name(),
            top: self.config.top,
            left: self.config.left,
            width: self.config.width,
            height: self.config.height,
            minHeight: 40,
            move: true,
            resize: true,
            position: (self.config.top<=0)?"top":null,
            head: {
                view:"toolbar",
                height: 30,
                css: "pinhold_toolbar",
                cols:[
                    {},
                    { view:"label", label: t("quick views")},
                    {},
                    { view:"icon", icon:"fas fa-times", click:function() {
                        this.getTopParentView().hide();
                        self.config.showing = false;
                        self.gobj_save_persistent_attrs();
                    }}
                ]
            },
            body: {
                view: "mxgraph",
                id: build_name(self, "mxgraph"),
                gobj: self
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
                },
                "onShow": function() {
                    self.config.showing = true;
                    self.config.left = this.gobj.config.$ui.config.left;
                    self.config.top = this.gobj.config.$ui.config.top;
                    self.config.width = this.gobj.config.$ui.config.width;
                    self.config.height = this.gobj.config.$ui.config.height;
                    self.gobj_save_persistent_attrs();
                }
            }
        };
        self.config.$ui = webix.ui(popup);
        self.config.$ui.gobj = self;

        var element = $$(build_name(self, "mxgraph")).getNode();
        element.firstChild.style.overflow = "hidden"; // HACK cambia style in mxgraph div
        element.firstChild.style["overflow-x"] = "auto"; // HACK cambia style in mxgraph div

        if(self.config.showing) {
            self.config.$ui.show();
        } else {
            self.config.$ui.hide();
        }

        if(self.config.ui_properties) {
            self.config.$ui.define(self.config.ui_properties);
            if(self.config.$ui.refresh) {
                self.config.$ui.refresh();
            }
        }

        automatic_resizing_cb(); // Adapt window size to device

        /*---------------------------------------*
         *      Automatic Resizing
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

        webix.event(window, "resize", automatic_resizing_cb);

        // HACK Write our $ui in the main menu
        __yuno__.__ui_main__.gobj_write_attr("$pinhold_content", self.config.$ui);
    }

    /************************************************************
     *
     ************************************************************/
    function manage_pinhold_button(self)
    {
        // Manage pinhold button in the main menu
        var $btn = __yuno__.__ui_main__.config.$pinhold_button;
        if($btn) {
            if(json_object_size(self.config.windows_pinpushed)==0) {
                $btn.define("image", ""); //"/static/app/images/yuneta/pin.svg");
                $btn.refresh();
            } else {
                $btn.define("image", "/static/app/images/yuneta/pin-push.svg");
                $btn.refresh();
            }
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

        mxEvent.disableContextMenu(graph.container);

        graph.border = 20;
        graph.view.setTranslate(graph.border/2, graph.border/2);

        graph.setPanning(true);
        graph.panningHandler.useLeftButtonForPanning = true;

        // Negative coordenates?
        graph.allowNegativeCoordinates = false;

        // Multiple connections between the same pair of vertices.
        graph.setMultigraph(true);

        // Avoids overlap of edges and collapse icons
        graph.keepEdgesInBackground = true;

        /*---------------------------*
         *      PERMISOS
         *---------------------------*/
        // Enable/Disable cell handling
        graph.setEnabled(true);

        graph.setConnectable(false); // Crear edges/links
        graph.setCellsDisconnectable(false); // Modificar egdes/links
        mxGraphHandler.prototype.setCloneEnabled(false); // Ctrl+Drag will clone a cell
        graph.setCellsLocked(false);
        graph.setCellsResizable(false);
        graph.setPortsEnabled(true);
        graph.setCellsEditable(false);

        mxGraph.prototype.isCellSelectable = function(cell) {
            return true;
        };

        /*
         *  Set stylesheet options
         */
        create_graph_style(
            graph,
            "mystyle",
            "text;html=1;strokeColor=#666666;fillColor=#F5F5F5;align=left;verticalAlign=top;whiteSpace=wrap;overflow=hidden;gradientColor=#ffffff;shadow=1;spacingLeft=10;spacingTop=5;fontSize=12;"
        );

        // Handles clicks on cells
        graph.addListener(mxEvent.CLICK, function(sender, evt) {
            var cell = evt.getProperty('cell');
            if (cell != null) {
                var id = evt.properties.cell.id;
                if(cell.isVertex()) {
                    self.gobj_send_event("EV_MX_VERTEX_CLICKED", id, self);
                }
            }
        });

        /*
         *  Own getLabel
         */
        graph.setHtmlLabels(true);
        graph.getLabel = function(cell) {
            if (this.getModel().isVertex(cell)) {
                var t = "<div>" + cell.id.replace(/-/g, '<br/>') + "</div>";
                t += "<image src='" + cell.value.image + "</image>";
                return t;
            }
            return "";
        };

        /*
         *  Cursor pointer
         */
        graph.getCursorForCell = function(cell) {
            if(this.model.isEdge(cell)) {
                return 'default';
            } else {
                return 'pointer';
            }
        };
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

    /************************************************************
     *
     ************************************************************/
    function get_layer(self, layer)
    {
        return self.config._mxgraph.getDefaultParent();
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
        var group = get_layer(self);

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

    /************************************************************
     *
     ************************************************************/
    function add_icon(self, name, image)
    {
        var graph = self.config._mxgraph;

        var cx = self.config.vertex_cx;
        var cy = self.config.vertex_cy;

        var model = graph.getModel();
        model.beginUpdate();
        try {
            var cell = graph.insertVertex(
                get_layer(self),        // group
                name,                   // id
                name,                   // value
                0, 0,                   // x,y
                cx, cy,                 // width,height
                "mystyle",              // style
                false                   // relative
            );
            if(image) {
                var overlay = new mxCellOverlay(
                    new mxImage(image, 32, 32),
                    "",                         // tooltip
                    mxConstants.ALIGN_RIGH,     // horizontal align ALIGN_LEFT,ALIGN_CENTER,ALIGN_RIGH>
                    mxConstants.ALIGN_BOTTOM,      // vertical align  ALIGN_TOP,ALIGN_MIDDLE,ALIGN_BOTTOM
                    new mxPoint(-32/2, -32/2),    // offset
                    "pointer"                   // cursor
                );
                graph.addCellOverlay(cell, overlay);
            }
            execute_layout(self);
        } catch (e) {
            log_error(e);
        } finally {
            model.endUpdate();
        }
    }

    /************************************************************
     *
     ************************************************************/
    function del_icon(self, name)
    {
        var graph = self.config._mxgraph;

        var cell = graph.getModel().getCell(name);
        graph.removeCells([cell]);
    }




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *  Evento enviado por la get_pinhold_window_top_toolbar()
     *  de los "Pinhold Window", cuando se pincha el "pin"
     *  toggle el estado del pin: on/off
     *
     *  Pinpushed toggle: on/off
     *
     *  Si pasa a on, guarda los datos en windows_pinpushed para reabrir
     *  la ventana al relanzar y que aparezca en la lista de acceso rápido
     *
     *  Si pasa a off, elimina de windows_pinpushed
     *
     *  Return the state of pin
     ********************************************/
    function ac_toggle_window(self, event, kw, src)
    {
        if(!src.gobj_is_unique() || !self.gobj_is_unique()) {
            return false; // Self and src must be unique to be pinpushed
        }

        var src_name = src.gobj_name();
        if(self.config.windows_pinpushed[src_name]) {
            src.config.pinpushed = false;
            // Delete from pinhold handler
            delete self.config.windows_pinpushed[src_name];
            src.gobj_remove_persistent_attrs();
            del_icon(self, src_name);

        } else {
            src.config.pinpushed = true;
            // Add self to pinhold handler
            self.config.windows_pinpushed[src_name] = {
                gobj_name: src.gobj_name(),
                gclass_name: src.gobj_gclass_name(),
                window_image: kw.window_image,
                with_create_window_on_start:
                    src.config.window_properties.with_create_window_on_start?true:false
            }
            src.gobj_save_persistent_attrs();

            add_icon(self, src_name, kw.window_image);
        }
        self.gobj_save_persistent_attrs();

        manage_pinhold_button(self);

        return src.config.pinpushed;
    }

    /********************************************
     *  Evento enviado por la get_pinhold_window_top_toolbar()
     *  de los "Pinhold Window"
     *
     *  Cierra una ventana "Pinhold Window"
     *      si está pinchada la ocultas,
     *      si no está pinchada la destruyes
     ********************************************/
    function ac_close_window(self, event, kw, src)
    {
        var src_name = src.gobj_name();
        if(self.config.windows_pinpushed[src_name] ||
                src.config.window_properties.without_destroy_window_on_close) {

            if(src.gobj_event_in_input_event_list("EV_CLOSE_WINDOW")) {
                src.gobj_send_event("EV_CLOSE_WINDOW", {destroying:false}, self);
            }

            // Only minimize
            src.config.$ui.hide();
        } else {
            // Destroy
            if(src.gobj_event_in_input_event_list("EV_CLOSE_WINDOW")) {
                src.gobj_send_event("EV_CLOSE_WINDOW", {destroying:true}, self);
            }
            __yuno__.gobj_destroy(src); // deleted from windows_pinpushed in mt_destroy()
            manage_pinhold_button(self);
        }

        return 0;
    }

    /********************************************
     *  Panel View enter in fullscreen
     ********************************************/
    function ac_set_fullscreen(self, event, kw, src)
    {
        var title = kw_get_str(kw, "title", "", 0);
        var $ui_fullscreen = src.gobj_read_attr("$ui_fullscreen");
        if(!$ui_fullscreen) {
            log_error(src.gobj_short_name() + " No $ui_fullscreen found");
            return -1;
        }

        if(!src.gobj_name()) {
            log_error(src.gobj_short_name() + " Fullscreen needs a gobj with name");
            return -1;
        }

        // HACK here don't need hide the pinhold_window_top_toolbar
        // because in popups the fullscreen is a inside zone of window

        // Save the gobj name in fullscreen
        self.config.gobj_name_in_fullscreen = src.gobj_name();

        webix.fullscreen.set(
            $ui_fullscreen,
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
     *
     ********************************************/
    function ac_activate_fullscreen(self, event, kw, src)
    {
        if(self.config.gobj_name_in_fullscreen) {
            var gobj = self.yuno.gobj_find_unique_gobj(
                self.config.gobj_name_in_fullscreen, false
            );
            if(!gobj) {
                self.config.gobj_name_in_fullscreen = "";

                /*
                 *  Save persistent attrs
                 */
                if(self.gobj_is_unique()) {
                    self.gobj_save_persistent_attrs();
                }
                return -1;
            }

            self.config.$ui.hide(); // Hide pinholder window

            self.gobj_send_event(
                "EV_SET_FULLSCREEN",
                {
                    title: gobj.config.window_title
                },
                gobj
            );
        }

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_mx_vertex_clicked(self, event, kw, src)
    {
        var gobj = __yuno__.gobj_find_unique_gobj(kw, true);
        if(gobj) {
            gobj.gobj_send_event("EV_TOGGLE", {}, self);
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
            "EV_TOGGLE_WINDOW",
            "EV_CLOSE_WINDOW",
            "EV_MX_VERTEX_CLICKED",
            "EV_SET_FULLSCREEN",
            "EV_EXIT_FULLSCREEN",
            "EV_ACTIVATE_FULLSCREEN",
            "EV_SELECT",
            "EV_REFRESH"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["EV_TOGGLE_WINDOW",        ac_toggle_window,       undefined],
                ["EV_CLOSE_WINDOW",         ac_close_window,        undefined],
                ["EV_MX_VERTEX_CLICKED",    ac_mx_vertex_clicked,   undefined],
                ["EV_SET_FULLSCREEN",       ac_set_fullscreen,      undefined],
                ["EV_EXIT_FULLSCREEN",      ac_exit_fullscreen,     undefined],
                ["EV_ACTIVATE_FULLSCREEN",  ac_activate_fullscreen, undefined],
                ["EV_SELECT",               ac_select,              undefined],
                ["EV_REFRESH",              ac_refresh,             undefined]
            ]
        }
    };

    var Ui_pinhold = GObj.__makeSubclass__();
    var proto = Ui_pinhold.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Ui_pinhold",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Ui_pinhold, "Ui_pinhold");




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
        self.config._mxgraph = $$(build_name(self, "mxgraph")).getMxgraph();
        initialize_mxgraph(self);
        rebuild_layouts(self);
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

        // Create windows pinpushed
        for(var key in self.config.windows_pinpushed) {
            var gclass_name = self.config.windows_pinpushed[key].gclass_name;
            var gobj_name = self.config.windows_pinpushed[key].gobj_name;
            var image = self.config.windows_pinpushed[key].window_image;
            var create = self.config.windows_pinpushed[key].with_create_window_on_start;
            var add = true;
            if(!create) {
                try {
                    self.yuno.gobj_create_unique(
                        gobj_name,
                        gobj_find_gclass(gclass_name, true),
                        {
                            pinpushed: true
                        },
                        self
                    );
                } catch (e) {
                    add = false;
                    log_error(e);
                }
            }
            if(add) {
                add_icon(self, gobj_name, image);
            }
        }
        manage_pinhold_button(self);
    }

    /************************************************
     *      Framework Method stop
     ************************************************/
    proto.mt_stop = function(kw)
    {
        var self = this;
    }

    /************************************************
     *  Framework Method mt_child_added
     ************************************************/
    proto.mt_child_added = function(child)
    {
        var self = this;
    }

    /************************************************
     *  Framework Method mt_child_added
     ************************************************/
    proto.mt_child_removed = function(child)
    {
        var self = this;

        delete self.config.windows_pinpushed[child.gobj_name()];

        manage_pinhold_button(self);
    }

    //=======================================================================
    //      Common code for pinhold panels
    //=======================================================================
    function get_pinhold_window_top_toolbar(gobj_window)
    {
        /*------------------------------------------*
         *      Top Toolbar of "Pinhold Window"
         *------------------------------------------*/
        var top_toolbar = {
            view:"toolbar",
            // HACK build_name() is from this file
            id: build_name(gobj_window, "pinhold_window_top_toolbar"),
            css: "toolbar2color",
            height: 30,
            cols: [
                {
                    view: "button",
                    hidden: true, //gobj_window.config.window_properties.with_window_pin_btn?false:true,
                    type: "image",
                    width: 40,
                    image: gobj_window.config.pinpushed?
                        "/static/app/images/yuneta/pin-push.svg":
                        "/static/app/images/yuneta/pin.svg",
                    css: "webix_transparent icon_toolbar_16",
                    tooltip: t("Pin the window"),
                    click: function() {
                        var pinpushed = gobj_window.parent.gobj_send_event(
                            "EV_TOGGLE_WINDOW",
                            {
                                window_image: gobj_window.config.window_image
                            },
                            gobj_window
                        );
                        if(pinpushed) {
                            // Pin push on
                            this.define("image", "/static/app/images/yuneta/pin-push.svg");
                        } else {
                            // Pin push off
                            this.define("image", "/static/app/images/yuneta/pin.svg");
                        }
                        this.refresh();

                        var $close_btn = $$(build_name(gobj_window, "pinhold_window_top_toolbar-close"));
                        $close_btn.define("icon",
                            (gobj_window.config.pinpushed ||
                            gobj_window.config.window_properties.without_destroy_window_on_close)?
                            "far fa-window-minimize":"fas fa-times"
                        );
                        $close_btn.define("tooltip",
                            (gobj_window.config.pinpushed ||
                            gobj_window.config.window_properties.without_destroy_window_on_close)?
                            t("hide"):t("close")
                        );
                        $close_btn.refresh();
                    }
                },
                {gravity: 1},
                {
                    view: "label",
                    gravity: 20,
                    label: gobj_window.config.window_title
                },
                {gravity: 1},
                {
                    view:"icon",
                    hidden: gobj_window.config.window_properties.without_window_fullscreen_btn?true:false,
                    icon: "fas fa-expand-wide",
                    tooltip: t("fullscreen"),
                    click: function() {
                        gobj_window.parent.gobj_send_event(
                            "EV_SET_FULLSCREEN",
                            {
                                title: gobj_window.config.window_title
                            },
                            gobj_window
                        );
                    }
                },
                {
                    view: "icon",
                    // HACK build_name() is from this file
                    id: build_name(gobj_window, "pinhold_window_top_toolbar-close"),
                    hidden: gobj_window.config.window_properties.without_window_close_btn?true:false,

                    // HACK repeated above
                    icon: (gobj_window.config.pinpushed ||
                            gobj_window.config.window_properties.without_destroy_window_on_close)?
                        "far fa-window-minimize":"fas fa-times",
                    tooltip: (gobj_window.config.pinpushed ||
                            gobj_window.config.window_properties.without_destroy_window_on_close)?
                        t("hide"):t("close"),
                    click: function() {
                        gobj_window.parent.gobj_send_event("EV_CLOSE_WINDOW", {}, gobj_window);
                    }
                }
            ]
        };
        return top_toolbar;
    }

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.Ui_pinhold = Ui_pinhold;
    exports.get_pinhold_window_top_toolbar = get_pinhold_window_top_toolbar;

})(this);
