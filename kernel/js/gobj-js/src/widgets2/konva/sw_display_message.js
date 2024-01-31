/***********************************************************************
 *          Sw_display_message.js
 *
 *          Display messages in modal windows, build with Konva canvas
 *
 *          Based in ScrollvieW
 *
 *          Copyright (c) 2022 Niyamaka.
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
        subscriber: null,   // subscriber of publishing messages (Child model: if null will be the parent)
        layer: null,        // Konva layer, set by parent.

        modal: true,        // Activation SERVICE: (webix) Outside disabled but Esc or clicking out will close
        super_modal: true,  // Activation SERVICE: Outside disabled and only inside action will close
        autoclose: false,   // Activation SERVICE: Close window on pointerup bubbling to stage or Esc key

        //------------ Own Attributes ------------//
        items: [],

        //------- Ka_scrollview Attributes --- HACK use ka_scrollview directly ---------//
        x: 100,
        y: 100,
        width: 240,
        height: 240,
        padding: 10,
        background_color: "white",

        draggable: true, // Enable (outer dragging) dragging

        kw_border_shape: { /* Border shape */
            strokeWidth: 8,
            opacity: 1,
            shadowBlur: 5
        },
        kw_border_shape_actived: { /* Border shape for active windows */
            // Only used: stroke, opacity, shadowBlur, shadowColor
        },

        //////////////// Private Attributes /////////////////
        _gobj_ka_scrollview: null,
        _ka_list: []
    };




            /***************************
             *      Local Methods
             ***************************/




    /********************************************
     *  WARNING: changed fontSize
     ********************************************/
    function calculate_max_width(self, items)
    {
        let max_width = 0;
        for(let i=0; i<items.length; i++) {
            let item = items[i];
            let label = kw_get_str(item, "label", "", false, true);
            let kw_text = kw_get_dict(item, "kw_text", {}, false, false);
            if(empty_string(label)) {
                continue;
            }
            kw_text.fontSize = adjust_font_size(kw_text.fontSize, kw_text.fontFamily, kw_text.padding);

            let text_size = get_text_size(
                label, kw_text.fontFamily, kw_text.fontSize, kw_text.padding, true
            );
            if(text_size.width > max_width) {
                max_width = text_size.width;
            }
        }

        let _w = kw_get_int(self.config.kw_border_shape, "strokeWidth", 0, false, false);
        _w += Math.max(
            kw_get_int(self.config.kw_border_shape, "shadowBlur", 0, false, false),
            kw_get_int(self.config.kw_border_shape_actived, "strokeWidth", 0, false, false)
        );

        let _max_width = window.innerWidth - self.config.padding*2 - _w*2;

        if(_max_width > 0 && max_width > _max_width) {
            max_width = _max_width;
        }

        return max_width;
    }

    /********************************************
     *
     ********************************************/
    function remove_ka_list(self)
    {
        for(let i=0; i<self.private._ka_list.length; i++) {
            let ka_group = self.private._ka_list[i];
            if(ka_group) {
                ka_group.record.ka_group = null; // cross-link
                ka_group.record = null;  // cross-link
                ka_group.destroy();
                self.private._ka_list[i] = null;
            }
        }
        self.private._ka_list = [];
    }

    /********************************************
     *  konva item: text item
     *      {
     *          "id": unique id (not really). If id is empty id=action if it's actions is a string
     *          "name": non-unique name
     *          "label":  text of menu item
     *          "icon":
     *          "action": function(item) | string (event to publish when hit item),
     *          "disabled": bool
     *          "selected": bool
     *      }
     ********************************************/
    function create_konva_item(self, record, y, width)
    {
        let id = kw_get_str(record, "id", "", false, false);
        let name = kw_get_str(record, "name", "", false, false);
        let label = kw_get_str(record, "label", "", false, true);
        if(empty_string(label)) {
            return null;
        }
        let icon = kw_get_str(record, "icon", "", false, false); // TODO draw icon
        let action = kw_get_dict_value(record, "action", null, false, false);
        if(is_string(action)) {
            let event = action;
            record.action = function(_self, _id) {
                _self.gobj_publish_event(event, {"id": _id});
            };
        } else if(action && !is_function(action)) {
            log_error("action must be a string or function");
        }

        /*---------------------------------*
         *      Create konva Group
         *---------------------------------*/
        let ka_group = new Konva.Group({
            id: id,
            name: name,
            x: 0,
            y: y
        });

        /*---------------------------------*
         *      Create konva Text
         *---------------------------------*/
        let kw_text = __duplicate__(kw_get_dict(record, "kw_text", {}, false));
        json_object_update(
            kw_text,
            {
                x: 0,
                y: 0,
                text: label,
                width: width
            }
        );
        let ka_text = new Konva.Text(kw_text);

        /*---------------------------------*
         *  Create konva Rect
         *---------------------------------*/
        let kw_rect = __duplicate__(kw_get_dict(record, "kw_rect", {}, false));
        json_object_update(
            kw_rect,
            {
                x: 0,
                y: 0,
                width: ka_text.getClientRect().width,
                height: ka_text.getClientRect().height
            }
        );
        let ka_rect = new Konva.Rect(kw_rect);

        ka_group.add(ka_rect);
        ka_group.add(ka_text);

        /*---------------------------------*
         *  Cross-link
         *---------------------------------*/
        record.ka_group = ka_group;     // cross-link
        ka_group.record = record;       // cross-link

        enable_konva_item(self, record, !record.disabled, true);

        return ka_group;
    }

    /********************************************
     *
     ********************************************/
    function enable_konva_item(self, record, enable, force)
    {
        let stage = self.config.layer.getStage();
        let ka_group = record.ka_group;
        let ka_text = ka_group.findOne("Text");

        if(enable) {
            /*--------------------------*
             *      Enable
             *--------------------------*/
            if(record.disabled || force) {
                /*
                 *  Enable if not already enabled or forcing
                 */
                record.disabled = false;

                ka_text.opacity(1);

                if(record.action) {
                    ka_group.on("mouseenter", function (e) {
                        stage.container().style.cursor = "pointer";
                    });

                    ka_group.on("mouseleave", function (e) {
                        ka_group.opacity(1);
                        stage.container().style.cursor = "default";
                    });

                    ka_group.on("pointerdown", function (e) {
                        e.cancelBubble = false;
                        e.gobj = self;
                        if(self.is_tracing()) {
                            log_warning(sprintf("%s.%s ==> (%s), cancelBubble: %s, gobj: %s, ka_id: %s, ka_name: %s",
                                "ka_display_message", "ka_group",
                                e.type,
                                (e.cancelBubble)?"Y":"N",
                                self.gobj_short_name(),
                                kw_get_str(e.target.attrs, "id", ""),
                                kw_get_str(e.target.attrs, "name", "")
                            ));
                        }
                        ka_group.opacity(0.5);
                    });
                    ka_group.on("pointerup", function (e) {
                        e.cancelBubble = false;
                        e.gobj = self;
                        if(self.is_tracing()) {
                            log_warning(sprintf("%s.%s ==> (%s), cancelBubble: %s, gobj: %s, ka_id: %s, ka_name: %s",
                                "ka_display_message", "ka_group",
                                e.type,
                                (e.cancelBubble)?"Y":"N",
                                self.gobj_short_name(),
                                kw_get_str(e.target.attrs, "id", ""),
                                kw_get_str(e.target.attrs, "name", "")
                            ));
                        }
                        ka_group.opacity(1);
                        record.action(self, record.id?record.id:record.name);
                    });
                }
            }
        } else {
            /*--------------------------*
             *      Disable
             *--------------------------*/
            if(!record.disabled || force) {
                /*
                 *  Enable if not already disabled or forcing
                 */
                record.disabled = true;

                ka_text.opacity(0.4);

                if(record.action) {
                    ka_group.off("mouseenter");
                    ka_group.off("mouseleave");
                    ka_group.off("pointerdown");
                    ka_group.off("pointerup");
                    ka_group.on("pointerup", function (e) {
                        e.cancelBubble = true;
                        if(self.is_tracing()) {
                            log_warning(sprintf("%s.%s ==> (%s), cancelBubble: %s, gobj: %s, ka_id: %s, ka_name: %s",
                                "ka_display_message", "ka_group",
                                e.type,
                                (e.cancelBubble)?"Y":"N",
                                self.gobj_short_name(),
                                kw_get_str(e.target.attrs, "id", ""),
                                kw_get_str(e.target.attrs, "name", "")
                            ));
                        }
                    });
                }
            }
        }
    }




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *  {
     *      "items": [
     *          {
     *              "id": unique id (not really). If id is empty id=action if it's actions is a string
     *              "name": non-unique name
     *              "label":  text of menu item
     *              "icon":
     *              "action": function(item) | string (event to publish when hit item),
     *              "disabled": bool
     *              "selected": bool
     *          }
     *      ]
     *  }
     ********************************************/
    function ac_add_item(self, event, kw, src)
    {
        let items_ = kw_get_list(kw, "items", [], false, true);
        let items = __duplicate__(items_);

        /*
         *  Delete current konva list
         */
        remove_ka_list(self);

        /*
         *  Build new konva items and set element node's position
         */
        let width = calculate_max_width(self, items);
        let y = 0;
        for(let i=0; i<items.length; i++) {
            let record = items[i];
            let ka_group = create_konva_item(self, record, y, width);
            if(ka_group) {
                let rc = ka_group.getClientRect();
                let delta = rc.height;
                self.private._ka_list.push(ka_group);
                y += delta;
            }
        }

        /*
         *  Repaint scrollview
         */
        self.private._gobj_ka_scrollview.gobj_send_event(
            "EV_ADD_ITEM",
            {
                items: self.private._ka_list
            },
            self
        );

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_keydown(self, event, kw, src)
    {
        let ret = 0;
        if(self.private._gobj_ka_scrollview.isVisible()) {
            switch(kw.keyCode) {
                case 27: // Escape
                    if(self.config.modal) {
                        // Escape close the window
                        self.gobj_send_event("EV_HIDE", {}, self);
                        ret = -1; // OWN the EV_KEYDOWN event
                    }
                    break;
                case 13: // Enter
                    let record = kwid_find_one_record(self.config.items, "submit");
                    if(record) {
                        record.action(self, record.id?record.id:record.name);
                        ret = -1; // OWN the EV_KEYDOWN event
                    }
                    break;
                default:
                    break;
            }
        }
        /*
         * Retorna -1 si quieres poseer el evento (No será subido hacia arriba).
         */
        return ret;
    }

    /********************************************
     *  Order from __ka_main__
     *  Please be idempotent
     ********************************************/
    function ac_activate(self, event, kw, src)
    {
        self.get_konva_container().moveToTop();
        self.private._gobj_ka_scrollview.gobj_send_event("EV_ACTIVATE", {}, self);

        return 0;
    }

    /********************************************
     *  Order from __ka_main__
     *  Please be idempotent
     ********************************************/
    function ac_deactivate(self, event, kw, src)
    {
        self.private._gobj_ka_scrollview.gobj_send_event("EV_DEACTIVATE", {}, self);

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

        let position = {
            x: self.config.x,
            y: self.config.y
        };

        self.private._gobj_ka_scrollview.gobj_send_event(event, position, self);
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

        let size = {
            width: self.config.width,
            height: self.config.height
        };

        self.private._gobj_ka_scrollview.gobj_send_event(event, size, self);
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_show_or_hide(self, event, kw, src)
    {
        let position = kw;
        let node_dimension = {};
        self.private._gobj_ka_scrollview.gobj_send_event("EV_GET_DIMENSION", node_dimension, self);

        self.config.x = kw_get_int(position, "x", node_dimension.x, true, false);
        self.config.y = kw_get_int(position, "y", node_dimension.y, true, false);
        self.private._gobj_ka_scrollview.gobj_send_event("EV_POSITION", position, self);
        self.private._gobj_ka_scrollview.gobj_send_event(event, kw, self);

        /*
         *  Global event to close popup window when hit outside
         */
        let __ka_main__ = self.yuno.gobj_find_service("__ka_main__", true);
        if(self.private._gobj_ka_scrollview.isVisible()) {
            /*
             *  Window visible
             */
            __ka_main__.gobj_send_event("EV_ACTIVATE", {}, self);

        } else {
            __ka_main__.gobj_send_event("EV_DEACTIVATE", {}, self);
        }

        return 0;
    }

    /********************************************
     *  Scrollview child showed
     ********************************************/
    function ac_showed(self, event, kw, src)
    {
        return 0;
    }

    /********************************************
     *  Scrollview child hidden
     ********************************************/
    function ac_hidden(self, event, kw, src)
    {
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_resize(self, event, kw, src)
    {
        let node_dimension = {};
        self.private._gobj_ka_scrollview.gobj_send_event("EV_GET_DIMENSION", node_dimension, self);

        let kw_restore = {};

        let _original_x = kw_get_int(node_dimension, "_original_x", null, false, false);
        let _original_y = kw_get_int(node_dimension, "_original_y", null, false, false);
        let _original_width = kw_get_int(node_dimension, "_original_width", null, false, false);
        let _original_height = kw_get_int(node_dimension, "_original_height", null, false, false);
        if(_original_x !== null) {
            kw_restore.x = _original_x;
        }
        if(_original_y !== null) {
            kw_restore.y = _original_y;
        }
        if(_original_width !== null) {
            kw_restore.width = _original_width;
        }
        if(_original_height !== null) {
            kw_restore.height = _original_height;
        }

        self.private._gobj_ka_scrollview.gobj_send_event("EV_RESIZE", kw_restore, self);

        return 0;
    }




            /***************************
             *      GClass/Machine
             ***************************/




    let FSM = {
        "event_list": [
            "EV_KEYDOWN",
            "EV_ADD_ITEM",
            "EV_ACTIVATE",
            "EV_DEACTIVATE",
            "EV_TOGGLE",
            "EV_POSITION",
            "EV_SIZE",
            "EV_SHOW",
            "EV_HIDE",
            "EV_SHOWED",
            "EV_HIDDEN",
            "EV_RESIZE"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["EV_KEYDOWN",          ac_keydown,             undefined],
                ["EV_ADD_ITEM",         ac_add_item,            undefined],
                ["EV_ACTIVATE",         ac_activate,            undefined],
                ["EV_DEACTIVATE",       ac_deactivate,          undefined],
                ["EV_TOGGLE",           ac_show_or_hide,        undefined],
                ["EV_SHOW",             ac_show_or_hide,        undefined],
                ["EV_HIDE",             ac_show_or_hide,        undefined],
                ["EV_POSITION",         ac_position,            undefined],
                ["EV_SIZE",             ac_size,                undefined],
                ["EV_SHOWED",           ac_showed,              undefined],
                ["EV_HIDDEN",           ac_hidden,              undefined],
                ["EV_RESIZE",           ac_resize,              undefined]
            ]
        }
    };

    let Sw_display_message = GObj.__makeSubclass__();
    let proto = Sw_display_message.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Sw_display_message",
            kw,
            0 //gcflag_no_check_output_events
        );
        return this;
    };
    gobj_register_gclass(Sw_display_message, "Sw_display_message");




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
         *  Check Konva
         */
        if(!self.config.layer) {
            log_error("What konva layer?");
            return;
        }

        /*
         *  CHILD subscription model
         */
        let subscriber = self.gobj_read_attr("subscriber");
        if(!subscriber) {
            subscriber = self.gobj_parent();
        }
        self.gobj_subscribe_event(null, null, subscriber);

        self.private._gobj_ka_scrollview = self.yuno.gobj_create(
            self.gobj_name(),
            Ka_scrollview,
            {
                // quick_display: true, // TODO TEST

                layer: self.config.layer,

                x: self.config.x,
                y: self.config.y,
                width: self.config.width,
                height: self.config.height,
                padding: self.config.padding,
                background_color: self.config.background_color,

                visible: true,
                panning: true,                      // Enable (inner dragging) panning
                draggable: self.config.draggable,   // Enable (outer dragging) dragging

                autosize: true,
                fix_dimension_to_screen: true,
                center: true,
                show_overflow: false,

                enable_vscroll: true,       // Enable content vertical scrolling
                enable_hscroll: true,       // Enable content horizontal scrolling
                scroll_by_step: true,

                kw_border_shape: __duplicate__(
                    kw_get_dict(self.config, "kw_border_shape", {}, false, false)
                ),
                kw_border_shape_actived: __duplicate__(
                    kw_get_dict(self.config, "kw_border_shape_actived", {}, false, false)
                )
            },
            self
        );
        self.private._gobj_ka_scrollview.get_konva_container().gobj = self; // cross-link

        self.gobj_send_event("EV_ADD_ITEM", {items: self.config.items}, self);
        self.gobj_send_event("EV_SHOW", {}, self);
    };

    /************************************************
     *      Framework Method destroy
     *      In this point, all childs
     *      and subscriptions are already deleted.
     ************************************************/
    proto.mt_destroy = function()
    {
        let self = this;
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
     *      Local Method
     ************************************************/
    proto.get_konva_container = function()
    {
        let self = this;
        return self.private._gobj_ka_scrollview.get_konva_container();
    };




                /************************************
                 *          Global functions
                 ************************************/




    /************************************************
     *
     ************************************************/
    function display_message(kw)
    {
        let type = kw_get_str(kw, "type", "", false, false); // "error", "warning" or nothing
        let text = kw_get_str(kw, "text", "", false, false);
        let title = kw_get_str(kw, "title", "", false, false);
        let ok = kw_get_str(kw, "ok", "Ok", false, false);
        let callback = kw_get_dict_value(kw, "callback", null, false, false);

        if(empty_string(title)) {
            if(type == "error") {
                title = t("error");
            } else if(type == "warning") {
                title = t("warning");
            }
        }

        let color;
        switch(type) {
            case "error":
                color = "red";
                break;
            case "warning":
                color = "yellow";
                break;
            default:
                color = "gray";
                break;
        }

        let kw_border_shape = {
            "stroke": color,
            "shadowColor": color
        };
        let kw_border_shape_actived = {
            "stroke": color,
            "shadowColor": color
        };

        let items = [
            {
                "label":  title,
                kw_text: {
                    fontFamily: "sans-serif",
                    fontSize: 30,
                    fill: "black",
                    align: "center",
                    padding: 5
                },
                "kw_rect": {
                    strokeWidth: 0
                }
            },
            {
                "label":  text,
                "kw_text": {
                    fontFamily: "sans-serif",
                    fontSize: 20,
                    fill: "black",
                    align: "center",
                    padding: 20
                },
                "kw_rect": {
                    strokeWidth: 0
                }
            },
            {
                "id": "submit",
                "label":  t(ok),
                "icon": "",
                "kw_text": {
                    fontFamily: "sans-serif",
                    fontSize: 20,
                    fill: "black",
                    align: "center",
                    padding: 10
                },
                "kw_rect": {
                    fill: color,
                    opacity: 0.4,
                    strokeWidth: 0
                },
                "action": function(_self, _item) {
                    _self.gobj_send_event("EV_HIDE", {}, __ka_main__);
                    __yuno__.gobj_destroy(_self);
                    if(callback) {
                        callback();
                    }
                }
            }
        ];

        let __ka_main__ = __yuno__.gobj_find_service("__ka_main__", true);

        let gobj_display_message = __yuno__.gobj_create(
            'display_message',
            Sw_display_message,
            {
                x: 700,
                y: 130,
                layer: __ka_main__.get_main_layer(),
                "kw_border_shape": kw_border_shape,
                "kw_border_shape_actived": kw_border_shape_actived,
                items: items
            },
            __ka_main__
        );

        if(gobj_display_message) {
            __ka_main__.gobj_send_event("EV_ACTIVATE", {}, gobj_display_message);
        }
    }

    /************************************************
     *
     ************************************************/
    function display_error_message(msg, title, callback, ok)
    {
        let kw = {
            type: "error",
            title: title,
            text: msg,
            callback: callback,
            ok: ok
        };
        display_message(kw);
    }

    /************************************************
     *
     ************************************************/
    function display_warning_message(msg, title, callback, ok)
    {
        let kw = {
            type: "warning",
            title: title,
            text: msg,
            callback: callback,
            ok: ok
        };
        display_message(kw);
    }

    /************************************************
     *
     ************************************************/
    function display_ok_message(msg, title)
    {
        let kw = {
            type: "",
            title: title,
            text: msg,
            callback: callback,
            ok: ok
        };
        display_message(kw);
    }

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.Sw_display_message = Sw_display_message;
    exports.display_message = display_message;
    exports.display_error_message = display_error_message;
    exports.display_warning_message = display_warning_message;
    exports.display_ok_message = display_ok_message;

})(this);
