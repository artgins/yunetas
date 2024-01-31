/***********************************************************************
 *          Ne_base.js
 *
 *          Node basic
 *          A node is a gobj publishing their movements that has child gobjs with the role of ports.
 *          A node is a gobj that can be connected with another gobj through their ports.
 *
 *          Mixin of ka_scrollview and ka_button
 *


                                ┌── Top Ports (Channels) 'top'
        Button 'top_left'       │                   ┌──────── Button 'top_right'
              │                 ▼                   ▼
              │      ┌─────┬──┬──┬──┬──┬──┬──┬──┬─────┐
              └────► │     │  │  │  │  │  │  │  │     │
                     ├─────┼──┴──┴──┴──┴──┴──┴──┼─────┤
                     │     │     'title'        │     │
                     ├─────┤                    ├─────┤
           ┌───────► │     │                    │     │ ─────► Output Ports (Channels) 'output'
           │         ├─────┤     Scrollview     ├─────┤
 Input Ports 'input' │     │     'center'       │     │
 (Channels)          ├─────┤                    ├─────┤
                     │     │                    │     │
                     ├─────┤                    ├─────┤
                     │     │                    │     │
                     ├─────┼──┬──┬──┬──┬──┬──┬──┼─────┤
                     │     │  │  │  │  │  │  │  │     │
                     └─────┴──┴──┴──┴──┴──┴──┴──┴─────┘
                      ▲         ▲                    ▲
   Button             │         │                    │
      'bottom_left' ──┘         │                    └─────── Button 'bottom_right'
                                └─── Bottom Ports (Channels) 'bottom'


 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    'use strict';

    /********************************************
     *  Configuration (C attributes)
     *  Attributes without underscore prefix
     *      will be set in self.config
     *  Attributes with underscore ("_") prefix:
     *      will be set in self.private
     ********************************************/
    let CONFIG = {
        //////////////// Public Attributes //////////////////
        subscriber: null,       // subscriber of publishing messages (Child model: if null will be the parent)
        layer: null,            // Konva layer
        view: null,             // View containing the node (default the parent), used for ports and links

        //------------ Own Attributes ------------//
        x: 0,
        y: 0,
        width: 400,
        height: 250,
        padding: 20,        /* Padding: left space to ports, title and center will decrease in padding size */

        background_color: "#FFF7E0",
        color: "black",       /* Default color for texts */

        visible: true,
        draggable: false,   // Enable (outer dragging) dragging

        top_on_activate: false,     // Move node to the top of its siblings on activation

        disabled: false,    // When True the button is disabled, managed by EV_DISABLE/EV_ENABLE too
        selected: false,    // When True the button is selected, managed by EV_SELECT/EV_UNSELECT too
        unlocked: false,    // When True designing is enabled, managed by EV_UNLOCK/EV_LOCK too

        kw_border_shape: { /* Border shape */
            cornerRadius: 10,
            strokeWidth: 2,
            stroke: "#f5c211ff",
            opacity: 1,
            shadowBlur: 0,
            shadowColor: "black",
            shadowForStrokeEnabled: false // HTML5 Canvas Optimizing Strokes Performance Tip
        },
        kw_border_shape_actived: { /* Border shape for active windows */
            // Only used: stroke, opacity, shadowBlur, shadowColor
        },

        //------------ Components ------------//
        port_width: 20,
        port_height: 20,
        port_shape: "circle",
        port_color: "#FFEEAA",
        port_offset: 0,

        port_label: "",              // label text
        port_label_background_color: "#00000000",  // opacity 0
        port_label_color: "black",

        title: { // HACK See shape_label_with_icon attributes
            height: 30
        },

        // Ports
        input: {},
        output: {},
        top: {},
        bottom: {},

        // Corners
        kw_top_left: {},
        kw_top_right: {},
        kw_bottom_left: {},
        kw_bottom_right: {},

        // Center
        enable_center: false,
        center_y_offset: true,
        kw_center: {
            padding: 0,
            kw_border_shape: {
                strokeWidth: 0,
            }
        },
        gobj_sw_center: null,   // gobj center is public available

        //////////////// Private Attributes /////////////////
        _ka_container: null,
        _ka_border_shape: null,
        _ka_title: null
    };




            /***************************
             *      Local Methods
             ***************************/




    /********************************************
     *
     ********************************************/
    function create_shape(self)
    {
        let width = self.config.width;
        let height = self.config.height;

        /*----------------------------*
         *  Container (Group)
         *----------------------------*/
        let ka_container = self.private._ka_container = new Konva.Group({
            id: self.gobj_short_name(),
            name: "ka_container",
            x: self.config.x,
            y: self.config.y,
            width: width,
            height: height,
            visible: self.config.visible,
            draggable: self.config.draggable
        });
        ka_container.gobj = self; // cross-link

        /*----------------------------*
         *  Border
         *----------------------------*/
        let kw_border_shape = __duplicate__(
            kw_get_dict(self.config, "kw_border_shape", {}, false, false)
        );
        json_object_update(
            kw_border_shape,
            {
                name: "ka_border_shape",
                x: 0,
                y: 0,
                width: width,
                height: height,
                fill: kw_get_str(self.config, "background_color", null)
            }
        );
        self.private._ka_border_shape = new Konva.Rect(kw_border_shape);
        ka_container.add(self.private._ka_border_shape);

        /*----------------------------*
         *  Control of positions
         *----------------------------*/
        let padding = kw_get_int(self.config, "padding", 0);

        /*----------------------------*
         *  Title
         *----------------------------*/
        let kw_title = kw_get_dict(self.config, "title", {}, false, false);
        let title_width = width - 2*padding;
        let title_height = kw_get_int(kw_title, "height", 30);
        json_object_update(
            kw_title,
            {
                name: "ka_title",
                x: padding,
                y: padding,
                width: title_width,
                height: title_height
            }
        );
        let kw_text_font_properties = kw_get_dict(kw_title, "kw_text_font_properties", {}, true);
        kw_text_font_properties.width = title_width;
        kw_text_font_properties.height = title_height;

        self.private._ka_title = create_shape_label_with_icon(kw_title);
        ka_container.add(self.private._ka_title);

        /*----------------------------*
         *  Corner Button: top_left
         *----------------------------*/
        // TODO

        /*----------------------------*
         *  Corner Button: top_right
         *----------------------------*/
        // TODO

        /*-------------------------------*
         *  Corner Button: bottom_left
         *-------------------------------*/
        // TODO

        /*--------------------------------*
         *  Corner Button: bottom_right
         *--------------------------------*/
        // TODO

        /*--------------------------------------------*
         *  Scrollview: center
         *--------------------------------------------*/
        if(self.config.enable_center) {
            let kw_center = __duplicate__(
                kw_get_dict(self.config, "kw_center", {}, false, false)
            );

            let _y = padding + title_height;
            let _height = height - 2*padding - title_height;
            if(self.config.center_y_offset) {
                _y += self.config.center_y_offset;
                _height -= self.config.center_y_offset;
            }
            json_object_update(
                kw_center,
                {
                    name: "ka_center",
                    x: padding,
                    y: _y,
                    width: width - 2*padding,
                    height: _height,
                    background_color: kw_get_str(kw_center, "background_color", self.config.background_color)
                }
            );
            self.config.gobj_sw_center = self.yuno.gobj_create(
                "center",
                Ka_scrollview,
                kw_center,
                self
            );
        }

        /*----------------------------*
         *  Dragg events
         *----------------------------*/
        if (self.config.draggable) {
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

        return ka_container;
    }

    /********************************************
     *
     ********************************************/
    function create_ports(self, port_position, kw_common)
    {
        let node_width = self.config.width;
        let node_height = self.config.height;
        let title_height = self.private._ka_title.height();

        /*----------------------------*
         *  Control of positions
         *----------------------------*/
        let padding = kw_get_int(self.config, "padding", 0);

        /*----------------------------*
         *  Ports
         *----------------------------*/
        let ports = kw_get_list(kw_common, "ports", []);
        let port_x, port_y, port_size;

        switch(port_position) {
            case "left":
                port_size = (node_height - (3*padding + title_height))/(ports.length);
                break;
            case "right":
                port_size = (node_height - (3*padding + title_height))/(ports.length);
                break;
            case "top":
                port_size = (node_width - (3*padding))/(ports.length);
                break;
            case "bottom":
                port_size = (node_width - (3*padding))/(ports.length);
                break;
        }

        let port_width = kw_get_int(kw_common, "width", self.config.port_width);
        let port_height = kw_get_int(kw_common, "height", self.config.port_height);
        let port_shape = kw_get_str(kw_common, "port_shape", self.config.port_shape);
        let port_color = kw_get_str(kw_common, "port_color", self.config.port_color);
        let port_offset = kw_get_int(kw_common, "port_offset", self.config.port_offset);
        let port_label = kw_get_str(kw_common, "port_label", self.config.port_label);
        let port_label_background_color = kw_get_str(
            kw_common, "port_label_background_color", self.config.port_label_background_color
        );
        let port_label_color = kw_get_str(kw_common, "port_label_color", self.config.port_label_color);

        json_object_update(
            kw_common,
            {
                layer: self.config.layer,
                subscriber: self.config.subscriber,
                width: port_width,
                height: port_height,
                port_shape: port_shape,
                port_position: port_position,
                port_color: port_color,
                port_label: port_label,
                port_label_background_color: port_label_background_color,
                port_label_color: port_label_color
            }
        );

        for (let i = 0; i < ports.length; i++) {
            let kw_port = ports[i];

            let port_shape2 = kw_get_str(kw_port, "port_shape", port_shape);

            switch(port_position) {
                case "left":
                    if(port_shape2 === "circle") {
                        port_x = 0;
                    } else {
                        port_x = -port_width/2;
                    }
                    port_x -= kw_get_int(kw_port, "port_offset", port_offset);
                    port_y = padding*2 + title_height;
                    port_y += port_size*i;
                    break;
                case "right":
                    if(port_shape2 === "circle") {
                        port_x = node_width;
                    } else {
                        port_x = node_width -port_width/2;
                    }
                    port_x += kw_get_int(kw_port, "port_offset", port_offset);
                    port_y = padding*2 + title_height;
                    port_y += port_size*i;
                    break;
                case "top":
                    if(port_shape2 === "circle") {
                        port_y = 0;
                    } else {
                        port_y = -port_height/2;
                    }
                    port_y -= kw_get_int(kw_port, "port_offset", port_offset);
                    port_x = padding*2;
                    port_x += port_size*i;
                    break;
                case "bottom":
                    if(port_shape2 === "circle") {
                        port_y = node_height;
                    } else {
                        port_y = node_height -port_width/2;
                    }
                    port_y += kw_get_int(kw_port, "port_offset", port_offset);
                    port_x = padding*2;
                    port_x += port_size*i;
                    break;
            }
            kw_common.x = port_x;
            kw_common.y = port_y;
            json_object_update_missing(kw_port, kw_common);
            kw_port.view = self.config.view;

            self.yuno.gobj_create(
                kw_get_str(kw_port, "id", kw_get_str(kw_port, "name", "")),
                Ka_port,
                kw_port,
                self
            );
        }
    }




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *
     ********************************************/
    function ac_keydown(self, event, kw, src)
    {
        let ret = 0;
        /*
         * Retorna -1 si quieres poseer el evento (No será subido hacia arriba).
         */
        return ret;
    }

    /********************************************
     *
     *  {
     *      input: {
     *          ports: [
     *              {
     *              }
     *          ]
     *      },
     *      output: {
     *          ports: [
     *              {
     *              }
     *          ]
     *      },
     *      top: {
     *          ports: [
     *              {
     *              }
     *          ]
     *      },
     *      bottom: {
     *          ports: [
     *              {
     *              }
     *          ]
     *      }
     *  }
     ********************************************/
    function ac_add_port(self, event, kw, src)
    {
        let input_ports = kw_get_list(kw, "input`ports", []);
        if(input_ports.length > 0) {
            create_ports(self, "left", kw.input);
        }
        let output_ports = kw_get_list(kw, "output`ports", []);
        if(output_ports.length > 0) {
            create_ports(self, "right", kw.output);
        }
        let top_ports = kw_get_list(kw, "top`ports", []);
        if(top_ports.length > 0) {
            create_ports(self, "top", kw.top);
        }
        let bottom_ports = kw_get_list(kw, "bottom`ports", []);
        if(bottom_ports.length > 0) {
            create_ports(self, "bottom", kw.bottom);
        }

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_remove_port(self, event, kw, src)
    {
        // TODO
        return 0;
    }

    /********************************************
     *  Order from __ka_main__
     *  Please be idempotent
     ********************************************/
    function ac_activate(self, event, kw, src)
    {
        if(self.config.top_on_activate) {
            self.private._ka_container.moveToTop();
        }

        /*
         *  Only used: stroke, opacity, shadowBlur, shadowColor
         */
        let kw_rect = self.config.kw_border_shape_actived;

        let stroke = kw_get_str(kw_rect,"stroke",null);
        if(!is_null(stroke)) {
            self.private._ka_border_rect.stroke(stroke);
        }

        let opacity = kw_get_real(kw_rect, "opacity", null);
        if(!is_null(opacity)) {
            self.private._ka_border_rect.opacity(opacity);
        }

        let shadowBlur = kw_get_int(kw_rect, "shadowBlur", null);
        if(!is_null(shadowBlur)) {
            self.private._ka_border_rect.shadowBlur(shadowBlur);
        }

        let shadowColor = kw_get_str(kw_rect, "shadowColor", null);
        if(!is_null(shadowColor)) {
            self.private._ka_border_rect.shadowColor(shadowColor);
        }

        return 0;
    }

    /********************************************
     *  Order from __ka_main__
     *  Please be idempotent
     ********************************************/
    function ac_deactivate(self, event, kw, src)
    {
        /*
         *  Only used: stroke, opacity, shadowBlur, shadowColor
         */
        let kw_rect = self.config.kw_border_shape;

        let stroke = kw_get_str(kw_rect,"stroke",null);
        if(!is_null(stroke)) {
            self.private._ka_border_rect.stroke(stroke);
        }

        let opacity = kw_get_real(kw_rect, "opacity", null);
        if(!is_null(opacity)) {
            self.private._ka_border_rect.opacity(opacity);
        }

        let shadowBlur = kw_get_int(kw_rect, "shadowBlur", null);
        if(!is_null(shadowBlur)) {
            self.private._ka_border_rect.shadowBlur(shadowBlur);
        }

        let shadowColor = kw_get_str(kw_rect, "shadowColor", null);
        if(!is_null(shadowColor)) {
            self.private._ka_border_rect.shadowColor(shadowColor);
        }

        return 0;
    }

    /********************************************
     *  TODO review (ka_link too)
     ********************************************/
    function ac_show_or_hide(self, event, kw, src)
    {
        let __ka_main__ = self.yuno.gobj_find_service("__ka_main__", true);

        /*--------------------------------------*
         *  Check if it's the show for a item
         *--------------------------------------*/
        let name = kw_get_str(kw, "id", kw_get_str(kw, "name", ""));
        if(!empty_string(name)) {
            if(event === "EV_SHOW") {
                let gobj = self.gobj_child_by_name(name);
                if(gobj) {
                    gobj.get_konva_container().moveToTop();
                    __ka_main__.gobj_send_event("EV_ACTIVATE", {}, gobj);
                }
            }
            return 0;
        }

        /*-----------------------------*
         *  Show/Hide self
         *-----------------------------*/
        switch(event) {
            case "EV_TOGGLE":
                if(self.private._ka_container.isVisible()) {
                    self.private._ka_container.hide();
                } else {
                    self.private._ka_container.show();
                }
                break;
            case "EV_SHOW":
                self.private._ka_container.show();
                break;
            case "EV_HIDE":
                self.private._ka_container.hide();
                break;
        }

        if(self.private._ka_container.isVisible()) {
            self.gobj_publish_event("EV_SHOWED", {});

            /*
             *  Window visible
             */
            __ka_main__.gobj_send_event("EV_ACTIVATE", {}, self); // TODO is necessary?

        } else {
            self.gobj_publish_event("EV_HIDDEN", {});

            __ka_main__.gobj_send_event("EV_DEACTIVATE", {}, self); // TODO is necessary?
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

        /*
         *  Resize container
         */
        // let ka_container = self.private._ka_container;
        // shape_label_size(ka_container, { TODO
        //     width: self.config.width,
        //     height: self.config.height
        // });

        return 0;
    }

    /********************************************
     *  Top order
     ********************************************/
    function ac_resize(self, event, kw, src)
    {
        return 0;
    }

    /********************************************
     *  Child panning/panned
     ********************************************/
    function ac_panning(self, event, kw, src)
    {
        if(src === self.private._gobj_ka_scrollview) {
            // Self panning
        }
        return 0;
    }

    /********************************************
     *  Child moving/moved
     ********************************************/
    function ac_moving(self, event, kw, src)
    {
        if(src === self.private._gobj_ka_scrollview) {
            // Self moving
        } else {
            // Child moving
        }
        return 0;
    }

    /********************************************
     *  Child showed
     ********************************************/
    function ac_showed(self, event, kw, src)
    {
        return 0;
    }

    /********************************************
     *  Child hidden
     ********************************************/
    function ac_hidden(self, event, kw, src)
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
            "EV_KEYDOWN",
            "EV_ADD_PORT",
            "EV_REMOVE_PORT",
            "EV_ACTIVATE",
            "EV_DEACTIVATE",

            "EV_TOGGLE",
            "EV_SHOW",
            "EV_HIDE",
            "EV_POSITION",
            "EV_SIZE",
            "EV_RESIZE",

            "EV_PANNING",
            "EV_PANNED",
            "EV_SHOWED",
            "EV_HIDDEN"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["EV_KEYDOWN",          ac_keydown,             undefined],

                ["EV_ADD_PORT",         ac_add_port,            undefined],
                ["EV_REMOVE_PORT",      ac_remove_port,         undefined],

                ["EV_ACTIVATE",         ac_activate,            undefined],
                ["EV_DEACTIVATE",       ac_deactivate,          undefined],

                ["EV_TOGGLE",           ac_show_or_hide,        undefined],
                ["EV_SHOW",             ac_show_or_hide,        undefined],
                ["EV_HIDE",             ac_show_or_hide,        undefined],
                ["EV_POSITION",         ac_position,            undefined],
                ["EV_SIZE",             ac_size,                undefined],
                ["EV_RESIZE",           ac_resize,              undefined],

                ["EV_PANNING",          ac_panning,             undefined],
                ["EV_PANNED",           ac_panning,             undefined],
                ["EV_MOVING",           ac_moving,              undefined],
                ["EV_MOVED",            ac_moving,              undefined],
                ["EV_SHOWED",           ac_showed,              undefined],
                ["EV_HIDDEN",           ac_hidden,              undefined],
            ]
        }
    };

    let Ne_base = GObj.__makeSubclass__();
    let proto = Ne_base.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Ne_base",
            kw,
            0 //gcflag_no_check_output_events
        );
        return this;
    };
    gobj_register_gclass(Ne_base, "Ne_base");




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
        if(!self.config.view) {
            self.config.view = self.gobj_parent();
        }

        create_shape(self);
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
        // TODO algo más habrá que destroy() no?
    };

    /************************************************
     *      Framework Method start
     ************************************************/
    proto.mt_start = function(kw)
    {
        let self = this;
        self.gobj_send_event("EV_ADD_PORT", self.config, self);
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
    };

    /************************************************
     *  Framework Method mt_child_added
     ************************************************/
    proto.mt_child_added = function(child)
    {
        let self = this;

        self.private._ka_container.add(child.get_konva_container());
    };

    /************************************************
     *  Framework Method mt_child_added
     ************************************************/
    proto.mt_child_removed = function(child)
    {
        let self = this;
        // TODO remove child container
    };

    /************************************************
     *      Local Method
     ************************************************/
    proto.get_konva_container = function()
    {
        let self = this;
        return self.private._ka_container;
    };

    /************************************************
     *      Local Method
     ************************************************/
    proto.get_ka_title_label = function()
    {
        let self = this;
        return self.private._ka_title;
    };

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.Ne_base = Ne_base;

})(this);
