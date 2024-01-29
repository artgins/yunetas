/***********************************************************************
 *          Sw_menu.js
 *
 *          Menu build with Konva canvas
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
        layer: null,        // Konva layer

        modal: false,       // Activation SERVICE: (webix) Outside disabled but Esc or clicking out will close
        super_modal: false, // Activation SERVICE: Outside disabled and only inside action will close
        autoclose: true,    // Activation SERVICE: Close window on pointerup bubbling to stage or Esc key

        //------------ Own Attributes ------------//
        /*  Item:
         *      {
         *          "id": unique id (not really). If id is empty id=action if action is a string
         *          "label":  text of menu item
         *          "icon":
         *          "action": function(item) | string (event to publish when hit item),
         *          "disabled": bool
         *          "selected": bool
         *      }
        */
        items: [],

        kw_text: {
            fontFamily: "sans-serif",
            fontSize: 20,
            fill: "black",
            padding: 10
        },

        kw_menu_item_rect: { /* Menu item rect */
            fill: "white",
            stroke: "gray",
            strokeWidth: 2,
            opacity: 1
        },

        //------- Ka_scrollview Attributes --- HACK use ka_scrollview directly ---------//
        x: 200,
        y: 200,
        width: 250,
        height: 250,
        padding: 0,
        background_color: "white",

        visible: true,
        draggable: false, // Enable (outer dragging) dragging

        autosize: true,

        kw_border_shape: { /* Border shape */
            strokeWidth: 1,
            stroke: "gray",
            opacity: 1,
            shadowBlur: 0,
            shadowColor: "gray"
        },
        kw_border_shape_actived: { /* Border shape for active windows */
            // Only used: stroke, opacity, shadowBlur, shadowColor
            stroke: "blue",
            opacity: 1,
            shadowBlur: 0,
            shadowColor: "blue"
        },

        //////////////// Private Attributes /////////////////
        _jdb: {
            "type": [], // Can be [] or {}
            "hook": "data",
            "schema": { // topics
                "menu": []
            }
        },
        _topic_name: "menu",
        _gobj_ka_scrollview: null,
        _ka_list: []
    };




            /***************************
             *      Local Methods
             ***************************/




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
     *  konva item: menu item
     *      {
     *          "id": unique id (not really). If id is empty id=action if action is a string
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
        let label = kw_get_str(record, "label", "", false, true);
        if(empty_string(label)) {
            return null;
        }
        let icon = kw_get_str(record, "icon", "", false, false); // TODO draw icon
        let action = kw_get_dict_value(record, "action", null, false, false);
        if(is_string(action)) {
            let event = action;
            record.action = function(_id) {
                self.gobj_publish_event(event, {"id": _id});
            };
        } else if(action && !is_function(action)) {
            log_error("action must be a string or function");
        }

        /*---------------------------------*
         *      Create konva Group
         *---------------------------------*/
        let x = 0;
        // Increase negative offset
        x += kw_get_int(self.config.kw_menu_item_rect, "strokeWidth", 0, false, false)/2;
        x += kw_get_int(self.config.kw_menu_item_rect, "shadowBlur", 0, false, false);

        let ka_group = new Konva.Group({
            //draggable: true, // TODO TEST remove
            id: id,
            name: "sw_menu_container",
            x: x,
            y: y
        });

        /*---------------------------------*
         *      Create konva Text
         *---------------------------------*/
        let kw_text = __duplicate__(kw_get_dict(self.config, "kw_text", {}, false));
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
        ka_group.add(ka_text);

        /*---------------------------------*
         *  Create konva Rect
         *---------------------------------*/
        let text_dimension = ka_text.getClientRect({relativeTo:ka_text.getParent()});
        let kw_menu_item_rect = __duplicate__(kw_get_dict(self.config, "kw_menu_item_rect", {}, false));
        json_object_update(
            kw_menu_item_rect,
            text_dimension
        );
        let ka_rect = new Konva.Rect(kw_menu_item_rect);
        ka_group.add(ka_rect);

        /*---------------------------------*
         *  Update group size
         *---------------------------------*/
        ka_group.size(ka_group.getClientRect());

        /*---------------------------------*
         *  Text to top
         *---------------------------------*/
        ka_text.moveToTop();

        /*---------------------------------*
         *  Cross-link
         *---------------------------------*/
        record.ka_group = ka_group;     // cross-link
        ka_group.record = record;       // cross-link

        /*---------------------------------*
         *  Enable/Disable node
         *---------------------------------*/
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

                if(is_function(record.action)) {
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
                                "Sw_menu", "ka_group",
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
                        log_warning(sprintf("%s.%s ==> (%s), cancelBubble: %s, gobj: %s, ka_id: %s, ka_name: %s",
                            "Sw_menu", "element",
                            e.type,
                            (e.cancelBubble)?"Y":"N",
                            self.gobj_short_name(),
                            kw_get_str(e.target.attrs, "id", ""),
                            kw_get_str(e.target.attrs, "name", "")
                        ));
                        ka_group.opacity(1);
                        record.action(record.id);
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
                                "Sw_menu", "ka_group",
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
     *              "id": unique id (not really). If id is empty id=action if action is a string
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
        let items = kw_get_list(kw, "items", [], false, true);

        /*
         *  Update database of items
         */
        for(let i=0; i<items.length; i++) {
            let item = items[i];
            if(item.id === undefined || item.id === null) {
                if(is_string(item.action)) {
                    item.id = item.action;
                }
            }
            jdb_update(self.private._jdb, self.private._topic_name, "", item);
        }

        /*
         *  Delete current konva list
         */
        remove_ka_list(self);

        /*
         *  Build new konva items and set element node's position
         */
        let menu_topic = jdb_get_topic(self.private._jdb, self.private._topic_name);

        let y = 0;
        // Increase negative offset
        y += kw_get_int(self.config.kw_menu_item_rect, "strokeWidth", 0, false, false)/2;
        y += kw_get_int(self.config.kw_menu_item_rect, "shadowBlur", 0, false, false);

        for(let i=0; i<menu_topic.length; i++) {
            let record = menu_topic[i];
            let ka_group = create_konva_item(self, record, y, self.config.width);
            if(ka_group) {
                // HACK skip shadow/stroke to overlap bottom/up line
                let delta = ka_group.getClientRect({skipShadow:true, skipStroke:true}).height;
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
    function ac_remove_item(self, event, kw, src)
    {
        let items = kw_get_list(kw, "items", [], false, true);

        /*
         *  Update database of items
         */
        for(let i=0; i<items.length; i++) {
            let item = items[i];
            jdb_delete(self.private._jdb, self.private._topic_name, "", item);
        }

        /*
         *  Delete current konva list
         */
        remove_ka_list(self);

        /*
         *  Build new konva items and set position
         */
        let menu_topic = jdb_get_topic(self.private._jdb, self.private._topic_name);
        for(let i=0, y=0; i<menu_topic; i++) {
            let record = menu_topic[i];
            let ka_group = create_konva_item(self, record, y, self.config.width);
            if(ka_group) {
                // HACK skip shadow/stroke to overlap bottom/up line
                let delta = ka_group.getClientRect({skipShadow:true, skipStroke:true}).height;
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
     *
     ********************************************/
    function ac_enable_items(self, event, kw, src)
    {
        let ids = kwid_get_ids(kw);
        for(let i=0; i<ids.length; i++) {
            let id = ids[i];
            let record = jdb_get(self.private._jdb, self.private._topic_name, id);
            if(!record) {
                log_error(sprintf("%s: enable_items, record not found, id:%s", self.gobj_short_name(), id));
                continue;
            }
            enable_konva_item(self, record, true);
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_disable_items(self, event, kw, src)
    {
        let ids = kwid_get_ids(kw);
        for(let i=0; i<ids.length; i++) {
            let id = ids[i];
            let record = jdb_get(self.private._jdb, self.private._topic_name, id);
            if(!record) {
                log_error(sprintf("%s: enable_items, record not found, id:%s", self.gobj_short_name(), id));
                continue;
            }
            enable_konva_item(self, record, false);
        }
        return 0;
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

// TODO     "EV_SELECT_ITEMS",
//             "EV_UNSELECT_ITEMS",
//             "EV_GET_SELECTED_ITEMS",

            "EV_ENABLE_ITEMS",
            "EV_DISABLE_ITEMS",
            "EV_ADD_ITEM",
            "EV_REMOVE_ITEM",
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
                ["EV_REMOVE_ITEM",      ac_remove_item,         undefined],
                ["EV_ENABLE_ITEMS",     ac_enable_items,        undefined],
                ["EV_DISABLE_ITEMS",    ac_disable_items,       undefined],
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

    let Sw_menu = GObj.__makeSubclass__();
    let proto = Sw_menu.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Sw_menu",
            kw,
            gcflag_no_check_output_events
        );
        return this;
    };
    gobj_register_gclass(Sw_menu, "Sw_menu");




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
         *  Child model
         */
        if(!self.config.subscriber) {
            self.config.subscriber = self.gobj_parent();  // Remove if not child model
        }
        if(self.config.subscriber) {
            self.gobj_subscribe_event(null, {}, self.config.subscriber);
        }

        if(!self.config.layer) {
            self.config.layer = self.gobj_parent().config.layer;
        }

        if(!self.config.layer) {
            log_error("What layer?");
        }

        jdb_init(self.private._jdb);

        self.private._gobj_ka_scrollview = self.yuno.gobj_create(
            self.gobj_name(),
            Ka_scrollview,
            {
                layer: self.config.layer,
                x: self.config.x,
                y: self.config.y,
                width: self.config.width,
                height: self.config.height,
                padding: self.config.padding,
                background_color: self.config.background_color,

                visible: self.config.visible,
                panning: true,                      // Enable (inner dragging) panning
                draggable: self.config.draggable,   // Enable (outer dragging) dragging

                autosize: self.config.autosize,
                fix_dimension_to_screen: true,
                center: false,
                show_overflow: false,

                enable_vscroll: true,       // Enable content vertical scrolling
                enable_hscroll: false,      // Enable content horizontal scrolling
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

        if(self.config.visible) {
            self.gobj_send_event("EV_SHOW", {}, self);
        }
    };

    /************************************************
     *      Framework Method destroy
     *      In this point, all childs
     *      and subscriptions are already deleted.
     ************************************************/
    proto.mt_destroy = function()
    {
        let self = this;

        if(self.private._gobj_ka_scrollview) {
            self.yuno.gobj_destroy(self.private._gobj_ka_scrollview);
            self.private._gobj_ka_scrollview = null;
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
     *      Local Method
     ************************************************/
    proto.get_konva_container = function()
    {
        let self = this;
        return self.private._gobj_ka_scrollview.get_konva_container();
    };

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.Sw_menu = Sw_menu;

})(this);
