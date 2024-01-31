/***********************************************************************
 *          Ka_button.js
 *
 *          Button
 *
 *          Based in KonvA
 *
 *          Copyright (c) 2022 Niyamaka.
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

        //------------ Own Attributes ------------//
        id: "",         // unique id (not really). If id is empty then id=action if action is a string
        action: null,   // function(e) | string (event to publish when hit item)
        /* HACK: if no `action` or `id` -> the button is a simple label */

        label: "",      // text of item
        icon: "",       // icon of item (from an icon font)
        background_color: "#FFF7E0",
        color: "black",
        text_color: null,
        icon_color: null,

        value: null,    // TODO Value for some buttons type

        icon_position: "left", /* position of icon combined with text: "top", "bottom", "left", "right" */
        disabled: false,    // When True the button is disabled, managed by EV_DISABLE/EV_ENABLE too
        selected: false,    // When True the button is selected, managed by EV_SELECT/EV_UNSELECT too
        unlocked: false,    // When True designing is enabled, managed by EV_UNLOCK/EV_LOCK too

        x: 0,               // HACK: **ALL** attrs are used by shape_label_with_icon()
        y: 0,
        width: 150,
        height: 40,

        visible: true,
        draggable: false,       // Enable (outer dragging) dragging

        icon_size: 18,  // Wanted size, but change by checking pixels in browser (_icon_size will be used)
        text_size: 18,  // it's different in mobile with text size larger (_text_size will be used)
        autosize: false,    // Change dimension to size of font text

        kw_text_font_properties: { // HACK: Used by shape_label_with_icon
            // fontSize:    // Override if you don't want it was calculated internally (_text_size)
            fontFamily: "sans-serif", // "OpenSans"
            fontStyle: "normal",
            padding: 10,
            align: "center",
            verticalAlign: "middle",
            wrap: "char"
        },
        kw_icon_font_properties: { // HACK: Used by shape_label_with_icon
            // fontSize:    // Override if you don't want it was calculated internally (_icon_size)
            fontFamily: "yuneta-icon-font",
            fontStyle: "normal",
            padding: 10,
            align: "center",
            verticalAlign: "middle",
            wrap: "char"
        },

        kw_border_shape: { // HACK: Used by shape_label_with_icon
            cornerRadius: 10,
            strokeWidth: 2,
            stroke: "#f5c211ff",
            opacity: 1,
            shadowBlur: 0,
            shadowColor: "black",
            shadowForStrokeEnabled: false // HTML5 Canvas Optimizing Strokes Performance Tip
        },

        quick_display: false,       // For debugging, draw quickly

        //////////////// Private Attributes /////////////////
        _icon_size: 0,      // Calculated by checking browser
        _text_size: 0,      // Calculated by checking browser

        _ka_container: null // HACK it's the return of shape_label_with_icon()
    };




                /***************************
                 *      Local Methods
                 ***************************/




    /********************************************
     *
     ********************************************/
    function create_shape(self)
    {
        let config = __duplicate__(self.config);

        if(!config.background_color) {
            config.background_color = self.parent.config.background_color;
        }
        if(!config.color) {
            config.color = self.parent.config.color;
        }

        let ka_container = create_shape_label_with_icon(config);
        ka_container.gobj = self; // cross-link

        if (self.config.draggable) {
            ka_container.draggable(true);

            ka_container.on('dragstart', function (ev) {
                ev.cancelBubble = true;
                let min_offset = self.config.kw_border_shape.strokeWidth;
                min_offset +=self.config.kw_border_shape.shadowBlur;
                let dim = ka_container.getClientRect({relativeTo:ka_container.getParent()});
                if(dim.x < 0) {
                    ka_container.x(min_offset);
                }
                if(dim.y < 0) {
                    ka_container.y(min_offset);
                }
                self.config.layer.getStage().container().style.cursor = "move";
                self.gobj_publish_event("EV_MOVING", ka_container.position());
            });
            ka_container.on('dragmove', function (ev) {
                ev.cancelBubble = true;
                let min_offset = self.config.kw_border_shape.strokeWidth;
                min_offset +=self.config.kw_border_shape.shadowBlur;
                let dim = ka_container.getClientRect({relativeTo:ka_container.getParent()});
                if(dim.x < 0) {
                    ka_container.x(min_offset);
                }
                if(dim.y < 0) {
                    ka_container.y(min_offset);
                }
                self.config.layer.getStage().container().style.cursor = "move";
                self.gobj_publish_event("EV_MOVING", ka_container.position());
            });
            ka_container.on('dragend', function (ev) {
                ka_container.opacity(1);
                ev.cancelBubble = true;
                let min_offset = self.config.kw_border_shape.strokeWidth;
                min_offset +=self.config.kw_border_shape.shadowBlur;
                let dim = ka_container.getClientRect({relativeTo:ka_container.getParent()});
                if(dim.x < 0) {
                    ka_container.x(min_offset);
                }
                if(dim.y < 0) {
                    ka_container.y(min_offset);
                }
                self.config.layer.getStage().container().style.cursor = "default";
                self.gobj_publish_event("EV_MOVED", ka_container.position());
            });
        }

        let id = kw_get_str(self.config, "id", null);
        let action = kw_get_dict_value(self.config, "action", null);

        if(empty_string(id)) {
            if(!empty_string(action)) {
                id = action;
            }
        }
        if(is_null(action)) {
            if(!empty_string(id)) {
                action = id;
            }
        }

        if(is_string(action)) {
            let event = action;
            action = function(e) {
                e["__share_kw__"] = true;
                self.gobj_publish_event(event, e);
            };
        }

        if(is_function(action)) {
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
                action(e);
            });
        }

        return ka_container;
    }




            /***************************
             *      Actions
             ***************************/




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

        /*
         *  Resize container
         */
        let ka_container = self.private._ka_container;
        ka_container.shape_label_size({
            width: self.config.width,
            height: self.config.height
        });

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

        kw["absolute_dimension"] = self.private._ka_container.getClientRect();

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
        self.config.disabled = false;
        self.private._ka_container.listening(true);
        self.private._ka_container.opacity(1);
        return 0;
    }

    /********************************************
     *  Disable
     ********************************************/
    function ac_disable(self, event, kw, src)
    {
        self.config.layer.getStage().container().style.cursor = "default";
        self.config.disabled = true;
        self.private._ka_container.listening(false);
        self.private._ka_container.opacity(0.6);
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

    let Ka_button = GObj.__makeSubclass__();
    let proto = Ka_button.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Ka_button",
            kw,
            gcflag_no_check_output_events
        );
        return this;
    };
    gobj_register_gclass(Ka_button, "Ka_button");




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
        if(self.config.disabled) {
            self.gobj_send_event("EV_DISABLE", {}, self);
        }
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
        // TODO if(name == "disabled" "selected" "unlocked")
        // Simulate events:
        //     "EV_SELECT",
        //     "EV_UNSELECT",
        //     "EV_ENABLE",
        //     "EV_DISABLE",
        //     "EV_LOCK",
        //     "EV_UNLOCK",

        switch(name) {
            case "disabled":
                if(self.config.disabled) {
                    self.gobj_send_event("EV_DISABLE", {}, self);
                } else {
                    self.gobj_send_event("EV_ENABLE", {}, self);
                }
                break;
            case "background_color":
                self.private._ka_container.shape_label_background_color(self.config.background_color);
                break;
            case "color":
                self.private._ka_container.shape_label_color(self.config.color);
                break;
            case "icon_color":
                self.private._ka_container.shape_label_icon_color(self.config.icon_color);
                break;
            case "text_color":
                self.private._ka_container.shape_label_text_color(self.config.text_color);
                break;
            case "label":
                self.private._ka_container.shape_label_text(self.config.label);
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
    exports.Ka_button = Ka_button;

})(this);
