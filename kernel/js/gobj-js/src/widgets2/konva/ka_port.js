/***********************************************************************
 *          Ka_port.js
 *
 *          Port
 *
 *          Based in KonvA
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    'use strict';

    /********************************************
     *      Configuration (C attributes)
     ********************************************/
    let CONFIG = {
        //////////////// Public Attributes //////////////////
        subscriber: null,   // subscriber of publishing messages (Child model: if null will be the parent)
        layer: null,        // Konva layer
        view: null,         // View containing the link's port

        //------------ Own Attributes ------------//
        id: "",         // unique id (not really). If id is empty then id=action if action is a string
        action: null,   // function(e) | string (event to publish when hit item),
        port_shape: "circle",   // "circle" of "rect"
        port_color: "#FFEEAA",
        port_position: "",          // position of port in ne_base: "left", "right", "top", "bottom"

        port_label: "",             // label text
        port_label_background_color: "#00000000",  // opacity 0
        port_label_color: "black",

        disabled: false,    // When True the port is disabled, managed by EV_DISABLE/EV_ENABLE too
        selected: false,    // When True the port is selected, managed by EV_SELECT/EV_UNSELECT too
        unlocked: false,    // When True designing is enabled, managed by EV_UNLOCK/EV_LOCK too

        x: 0,
        y: 0,
        width: 20,
        height: 20,

        visible: true,
        draggable: false,   // Enable (outer dragging) dragging

        kw_border_shape: { /* Border shape */
            strokeWidth: 2,
            stroke: "#f5c211ff",
            opacity: 1,
            shadowBlur: 0,
            shadowColor: "black",
            shadowForStrokeEnabled: false // HTML5 Canvas Optimizing Strokes Performance Tip
        },

        kw_label: {
            autosize: true,
            kw_border_shape: {
                strokeWidth: 0,
                cornerRadius: 0
            },
            kw_text_font_properties: {
                padding: 0
            },
            kw_icon_font_properties: {
                padding: 0
            }
        },

        quick_display: false,       // For debugging, draw quickly

        //////////////// Private Attributes /////////////////
        _icon_size: 0,      // Calculated by checking browser
        _text_size: 0,      // Calculated by checking browser

        _ka_container: null,
        _ka_border_shape: null,
        _ka_label: null
    };




                /***************************
                 *      Local Methods
                 ***************************/




    /********************************************
     *
     ********************************************/
    function create_shape(self)
    {
        let width = kw_get_int(self.config, "width", 20);
        let height = kw_get_int(self.config, "height", 20);
        let port_color = kw_get_str(self.config, "port_color", "red");

        /*----------------------------*
         *      Container (Group)
         *----------------------------*/
        let ka_container = new Konva.Group({
            id: kw_get_str(self.config, "id", ""),
            name: kw_get_str(self.config, "name", ""),
            x: kw_get_int(self.config, "x", 0),
            y: kw_get_int(self.config, "y", 0),
            width: width,
            height: height,
            visible: kw_get_bool(self.config, "visible", true),
            draggable: self.config.draggable
        });
        ka_container.gobj = self; // cross-link

        /*----------------------------*
         *      Shape
         *----------------------------*/
        let kw_border_shape = __duplicate__(
            kw_get_dict(CONFIG, "kw_border_shape", {})
        );
        json_object_update(kw_border_shape, kw_get_dict(self.config, "kw_border_shape", {}));
        json_object_update_missing(
            kw_border_shape,
            {
                name: "ka_border_shape",
                x: 0,
                y: 0,
                width: width,
                height: height,
                fill: port_color
            }
        );

        switch(self.config.port_shape) {
            case "rect":
                self.private._ka_border_shape = new Konva.Rect(kw_border_shape);
                break;
            case "circle":
            default:
                self.private._ka_border_shape = new Konva.Circle(kw_border_shape);
        }
        ka_container.add(self.private._ka_border_shape);

        /*----------------------------*
         *      Label
         *----------------------------*/
        let label = self.config.port_label;
        if(!empty_string(label)) {
            let kw_label = __duplicate__(
                kw_get_dict(self.config, "kw_label", {})
            );
            json_object_update_missing(
                kw_label,
                {
                    name: "ka_label",
                    label: label,
                    background_color: self.config.port_label_background_color,
                    color: self.config.port_label_color,
                    y: self.config.port_shape === "circle" ? -height / 2 : 0
                }
            );

            let x = 0;
            let y = 0;
            switch(self.config.port_position) {
                case "left":
                    self.private._ka_label = create_shape_label_with_icon(kw_label);
                    x = width;
                    if(self.config.port_shape === "circle") {
                        x = width/2 + 5;
                    } else {
                        x += 5;
                    }
                    self.private._ka_label.x(x);
                    break;
                case "right":
                    self.private._ka_label = create_shape_label_with_icon(kw_label);
                    x = self.private._ka_label.width() + 5;
                    if(self.config.port_shape === "circle") {
                        x += width/2;
                    }
                    self.private._ka_label.x(-x);
                    break;
                case "top":
                    self.private._ka_label = create_shape_label_with_icon(kw_label);
                    self.private._ka_label.rotation(-45);
                    if(self.config.port_shape === "circle") {
                        y = height + 5;
                        x = 0;
                    } else {
                        x = width/2;
                        y = height;
                    }
                    self.private._ka_label.position({x: x, y: -y});
                    break;
                case "bottom":
                    self.private._ka_label = create_shape_label_with_icon(kw_label);
                    self.private._ka_label.rotation(45);
                    if(self.config.port_shape === "circle") {
                        y = height/2 + 5;
                        x = 5;
                    } else {
                        y = height+5;
                        x = width;
                    }
                    self.private._ka_label.position({x: x, y: y});
                    break;
            }
            if(self.private._ka_label) {
                ka_container.add(self.private._ka_label);
            }
        }

        /*----------------------------*
         *      Events
         *----------------------------*/
        if (self.config.draggable) {
            ka_container.on('dragstart', function (ev) {
                ev.cancelBubble = true;
                self.config.layer.getStage().container().style.cursor = "move";
                self.gobj_publish_event("EV_MOVING", ka_container.position());
            });
            ka_container.on('dragmove', function (ev) {
                ev.cancelBubble = true;
                self.config.layer.getStage().container().style.cursor = "move";
                self.gobj_publish_event("EV_MOVING", ka_container.position());
            });
            ka_container.on('dragend', function (ev) {
                ka_container.opacity(1);
                ev.cancelBubble = true;
                if (self.config.action) {
                    self.config.layer.getStage().container().style.cursor = "pointer";
                } else {
                    self.config.layer.getStage().container().style.cursor = "default";
                }
                self.gobj_publish_event("EV_MOVED", ka_container.position());
            });
        }

        let id = kw_get_str(self.config, "id", null);
        let action = kw_get_dict_value(self.config, "action", null);

        if(is_null(id)) {
            if(is_string(item.action)) {
                self.config.id = action;
            }
        }

        if(is_string(action)) {
            let event = action;
            self.config.action = function(e) {
                e["__share_kw__"] = true;
                self.gobj_publish_event(event, e);
            };
        } else if(action && !is_function(action)) {
            log_error("action must be a string or function");
            return ka_container;
        }

        if (is_function(self.config.action)) {
            ka_container.on("mouseenter", function (e) {
                self.config.layer.getStage().container().style.cursor = "pointer";
            });

            ka_container.on("mouseleave", function (e) {
                self.config.layer.getStage().container().style.cursor = "default";
            });

            ka_container.on("pointerdown", function (e) {
                ka_container.opacity(0.5);
                e.gobj = self;
                if (self.is_tracing()) {
                    log_warning(sprintf("%s.%s ==> (%s), cancelBubble: %s, gobj: %s, ka_id: %s, ka_name: %s",
                        "nd_machine", "element",
                        e.type,
                        (e.cancelBubble) ? "Y" : "N",
                        self.gobj_short_name(),
                        kw_get_str(e.target.attrs, "id", ""),
                        kw_get_str(e.target.attrs, "name", "")
                    ));
                }
            });
            ka_container.on("pointerup", function (e) {
                ka_container.opacity(1);
                e.cancelBubble = true;
                e.gobj = self;
                if (self.is_tracing()) {
                    log_warning(sprintf("%s.%s ==> (%s), cancelBubble: %s, gobj: %s, ka_id: %s, ka_name: %s",
                        "nd_machine", "element",
                        e.type,
                        (e.cancelBubble) ? "Y" : "N",
                        self.gobj_short_name(),
                        kw_get_str(e.target.attrs, "id", ""),
                        kw_get_str(e.target.attrs, "name", "")
                    ));
                }
                /*
                 *  WARNING If action provoke deleting the konva item then the event is not bubbled!
                 *  Don't worry, if the konva item is closed, and the event don't arrive to stage listener,
                 *  the window will send a EV_DEACTIVATE and the window will be deactivated,
                 *  so for the activation service will work well.
                 *  BE CAREFUL with service needing bubbling.
                 */
                self.config.action(e);
            });
        }

        return ka_container;
    }




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *  kw: {
     *      what: "port" | "icon" | "text"
     *      color:
     *  }
     ********************************************/
    function ac_color(self, event, kw, src)
    {
        let color = kw_get_str(kw, "color", "");
        switch(kw.what) {
            case "text":
                if(self.private._ka_label) {
                    if(color) {
                        self.private._ka_label.shape_label_text_color(color);
                    } else {
                        kw["color"] = self.private._ka_label.shape_label_text_color();
                    }
                }
                break;

            case "icon":
                if(self.private._ka_label) {
                    if (color) {
                        self.private._ka_label.shape_label_icon_color(color);
                    } else {
                        kw["color"] = self.private._ka_label.shape_label_icon_color();
                    }
                }
                break;

            case "port":
            default:
                if(color) {
                    self.private._ka_border_shape.fill(color);
                } else {
                    kw["color"] = self.private._ka_border_shape.fill();
                }
                break;
        }

        return 0;
    }

    /********************************************
     *  kw: {
     *      x:
     *      y:
     *  }
     ********************************************/
    function ac_position(self, event, kw, src)
    {
        self.config.x = kw_get_int(kw, "x", self.config.x, false, false);
        self.config.y = kw_get_int(kw, "y", self.config.y, false, false);

        /*
         *  Move container
         */
        let ka_container = self.private._ka_container;
        ka_container.position({
            x: self.config.x,
            y: self.config.y
        });

        return 0;
    }

    /********************************************
     *  kw: {
     *      width:
     *      height:
     *  }
     ********************************************/
    function ac_size(self, event, kw, src)
    {
        self.config.width = kw_get_int(kw, "width", self.config.width, false, false);
        self.config.height = kw_get_int(kw, "height", self.config.height, false, false);
        // TODO

        return 0;
    }

    /********************************************
     *  Return dimensions:
     *      - configured position and size (relative to parent)
     *      - absolute position (relative to screen)
     *
     *  kw: {
     *      x:
     *      y:
     *      width:
     *      height:
     *
     *      absolute_dimension: { // To layer
     *          x:
     *          y:
     *          width:
     *          height:
     *      }
     *      relative_dimension: { // To view
     *          x:
     *          y:
     *          width:
     *          height:
     *      }
     *  }
     *
     ********************************************/
    function ac_get_dimension(self, event, kw, src)
    {
        if(!is_object(kw)) {
            log_error(sprintf("%s: get_dimension(): kw not an object, from %s",
                self.gobj_short_name(), src.gobj_short_name()
            ));
            return -1;
        }
        kw["x"] = self.config.x;
        kw["y"] = self.config.y;
        kw["width"] = self.config.width;
        kw["height"] = self.config.height;

        // HACK consider only the port shape, exclude label
        kw["absolute_dimension"] = self.private._ka_border_shape.getClientRect();

        if(!self.config.view) {
            log_error(sprintf("%s: view of port NULL", self.gobj_short_name()));
        }
        kw["relative_dimension"] = self.private._ka_border_shape.getClientRect(
            {
                relativeTo:self.config.view.get_konva_container()
            }
        );

        return 0;
    }

    /********************************************
     *  Select
     ********************************************/
    function ac_select(self, event, kw, src)
    {
        // TODO change appearance
        self.config.selected = true;
        return 0;
    }

    /********************************************
     *  Unselect
     ********************************************/
    function ac_unselect(self, event, kw, src)
    {
        // TODO change appearance
        self.config.selected = false;
        return 0;
    }

    /********************************************
     *  Enable
     ********************************************/
    function ac_enable(self, event, kw, src)
    {
        // TODO change appearance
        self.config.disabled = false;
        return 0;
    }

    /********************************************
     *  Disable
     ********************************************/
    function ac_disable(self, event, kw, src)
    {
        // TODO change appearance
        self.config.disabled = true;
        return 0;
    }

    /********************************************
     *  Lock design
     ********************************************/
    function ac_lock(self, event, kw, src)
    {
        // TODO change appearance
        return 0;
    }

    /********************************************
     *  Unlock design
     ********************************************/
    function ac_unlock(self, event, kw, src)
    {
        // TODO change appearance
        return 0;
    }

    /************************************************
     *
     ************************************************/
    function ac_timeout(self, event, kw, src)
    {
        return 0;
    }

    /********************************************
     *  Top order
     ********************************************/
    function ac_resize(self, event, kw, src)
    {
        return 0;
    }




            /***************************
             *      GClass/Machine
             ***************************/




    let FSM = {
        "event_list": [
            "EV_MOVING: output no_warn_subs",
            "EV_MOVED: output no_warn_subs",
            "EV_COLOR",
            "EV_POSITION",
            "EV_SIZE",
            "EV_GET_DIMENSION",
            "EV_SELECT",
            "EV_UNSELECT",
            "EV_ENABLE",
            "EV_DISABLE",
            "EV_LOCK",
            "EV_UNLOCK",
            "EV_TIMEOUT",
            "EV_RESIZE"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["EV_COLOR",                ac_color,               undefined],
                ["EV_POSITION",             ac_position,            undefined],
                ["EV_SIZE",                 ac_size,                undefined],
                ["EV_GET_DIMENSION",        ac_get_dimension,       undefined],
                ["EV_SELECT",               ac_select,              undefined],
                ["EV_UNSELECT",             ac_unselect,            undefined],
                ["EV_ENABLE",               ac_enable,              undefined],
                ["EV_DISABLE",              ac_disable,             undefined],
                ["EV_LOCK",                 ac_lock,                undefined],
                ["EV_UNLOCK",               ac_unlock,              undefined],
                ["EV_TIMEOUT",              ac_timeout,             undefined],
                ["EV_RESIZE",               ac_resize,              undefined]
            ]
        }
    };

    let Ka_port = GObj.__makeSubclass__();
    let proto = Ka_port.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Ka_port",
            kw,
            gcflag_no_check_output_events
        );
        return this;
    };
    gobj_register_gclass(Ka_port, "Ka_port");




            /***************************
             *      Framework Methods
             ***************************/




    /************************************************
     *      Framework Method create
     ************************************************/
    proto.mt_create = function(kw)
    {
        let self = this;

        /*
         *  CHILD subscription model
         */
        let subscriber = self.gobj_read_attr("subscriber");
        if(!subscriber) {
            subscriber = self.gobj_parent();
        }
        self.gobj_subscribe_event(null, null, subscriber);

        if(!self.config.layer) {
            self.config.layer = self.gobj_parent().config.layer;
        }

        self.private._ka_container = create_shape(self); // WARNING added to layer in mt_child_added of parent
    };

    /************************************************
     *      Framework Method destroy
     *      In this point, all childs
     *      and subscriptions are already deleted.
     ************************************************/
    proto.mt_destroy = function()
    {
        let self = this;

        if(self.private._ka_container) {
            self.private._ka_container.destroy();
            self.private._ka_container = null;
        }
    };

    /************************************************
     *      Framework Method start
     ************************************************/
    proto.mt_start = function(kw)
    {
        let self = this;
    };

    /************************************************
     *      Framework Method stop
     ************************************************/
    proto.mt_stop = function(kw)
    {
        let self = this;
    };

    /************************************************
     *      Framework Method writing
     ************************************************/
    proto.mt_writing = function(name)
    {
        let self = this;

        switch(name) {
            case "port_color":
                self.private._ka_border_shape.fill(self.config.port_color);
                break;
            case "port_label_background_color":
                self.private._ka_label.shape_label_background_color(self.config.port_label_background_color);
                break;
            case "port_label_color":
                self.private._ka_label.fill(self.config.port_label_color);
                break;
        }
    };

    /************************************************
     *      Local Method
     ************************************************/
    proto.get_konva_container = function()
    {
        let self = this;
        return self.private._ka_container;
    };


    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.Ka_port = Ka_port;

})(this);
