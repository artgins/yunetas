/***********************************************************************
 *          Sw_graph.js
 *
 *          **View** that contains **gobj** **nodes** and **links** (or others gobj)
 *
 *          A **view** can work inside a multiview
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

        //------------ Own Attributes ------------//
        items: [],  // can be nodes or buttons or etc
        links: [],

        //------- Ka_scrollview Attributes --- HACK use ka_scrollview directly ---------//
        x: 100,
        y: 100,
        width: 240,
        height: 240,
        padding: 0,
        background_color: "#cccccc",

        visible: true,
        panning: true,          // Enable (inner dragging) panning
        draggable: false,       // Enable (outer dragging) dragging

        fix_dimension_to_screen: false,
        grid: 10,               // If not 0 then fix the position to near grid position

        enable_vscroll: true,       // Enable content vertical scrolling, default true
        enable_hscroll: true,       // Enable content horizontal scrolling, default true
        scroll_by_step: false,      // false: use native; true: use child dimension; number: use 'number' step;

        quick_display: false,       // For debugging, draw quickly

        kw_border_shape: { /* Border shape */
            strokeWidth: 0,
            opacity: 1,
            shadowBlur: 0
        },
        kw_border_shape_actived: { /* Border shape for active windows */
            // Only used: stroke, opacity, shadowBlur, shadowColor
        },

        //////////////// Private Attributes /////////////////
        _gobj_ka_scrollview: null
    };




            /***************************
             *      Local Methods
             ***************************/




    /************************************************
     *  Return the link gobj, null if error
     ************************************************/
    function create_link(self, kw, common)
    {
        let id = kw_get_str(kw, "id", kw_get_str(kw, "name", ""));

        /*
         *  Check if link exists
         */
        let gobj_link = self.yuno.gobj_find_unique_gobj(id);
        if(gobj_link) {
            log_error(sprintf("%s: link already exists '%s'", self.gobj_short_name(), id));
            return null;
        }

        // 'id' or 'name' can be use as all port names
        kw["source_port"] = kw_get_dict_value(kw, "source_port", id);
        kw["target_port"] = kw_get_dict_value(kw, "target_port", id);

        json_object_update_missing(kw, common);

        return self.yuno.gobj_create_unique(id, Ka_link, kw, self);
    }




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *  EV_ADD_ITEM {
     *      "items": [{id:, gclass:, kw: }, ...]
     *      or
     *      "items": [gobj, ...]
     *  }
     *
     *  You can use "name" instead of "id"
     *
     ********************************************/
    function ac_add_item(self, event, kw, src)
    {
        let items = kw_get_dict_value(kw, "items", null, false, false);

        for(let i=0; i<items.length; i++) {
            let item = items[i];
            let gobj_item = null;
            if(is_gobj(item)) {
                gobj_item = item;

            } else if(is_object(item)) {
                let kw_item = kw_get_dict(item, "kw", {});
                gobj_item = self.yuno.gobj_create(
                    kw_get_str(item, "id", kw_get_str(item, "name", "")),
                    kw_get_dict_value(item, "gclass", null),
                    kw_item,
                    self
                );
                continue; // goes recurrent ac_add_item() by mt_child_added()
            } else {
                log_error("What is it?" + item);
                continue;
            }

            let k = gobj_item.get_konva_container();
            self.private._gobj_ka_scrollview.gobj_send_event(
                "EV_ADD_ITEM",
                {
                    items: [k]
                },
                self
            );
        }

        return 0;
    }

    /********************************************
     *  EV_REMOVE_ITEM {
     *      "items": ["id", ...]
     *      or
     *      "items": [{id: "id", }, ...]
     *      or
     *      "items": [gobj, ...]
     *  }
     ********************************************/
    function ac_remove_item(self, event, kw, src)
    {
        let items = kw_get_dict_value(kw, "items", null, false, false);

        for(let i=0; i<items.length; i++) {
            let item = items[i];
            let childs = null;
            if(is_string(item)) {
                let name = item;
                childs = self.gobj_match_childs({__gobj_name__: name});
            } else if(is_object(item)) {
                let name = kw_get_str(item, "id", kw_get_str(item, "name", ""));
                childs = self.gobj_match_childs({__gobj_name__: name});
            } else if(is_gobj(item)) {
                childs = [item];
            } else {
                log_error("What is?" + item);
                continue;
            }

            for(let child in childs) {
                let k = child.get_konva_container();
                self.private._gobj_ka_scrollview.gobj_send_event(
                    "EV_REMOVE_ITEM",
                    {
                        items: [k]
                    },
                    self
                );
                if(!child.gobj_is_destroying()) {
                    self.yuno.gobj_destroy(child);
                }
            }
        }

        return 0;
    }

    /********************************************
     *  Link supported
     *
     *  EV_LINK {
     *      "links": [{id:, gclass:, kw: }, ...]
     *      or
     *      "links": [gobj, ...]
     *  }
     *
     *  kw_link:
     *      You can use "name" instead of "id"
     *
     *      id/name:
     *          name of link gobj, and name of source_port/target_port if they are empty.
     *
     *      source_node:
     *          - string: name of source gobj (unique or service gobj)
     *          - gobj: source gobj, must be an unique gobj.
     *
     *      target_node:
     *          - string: name of target gobj (unique or service gobj)
     *          - gobj: target gobj, must be an unique gobj.
     *
     *      source_port: Use `id` if source_port is an empty string
     *          - string: name of source port gobj, must be a child of self
     *          - gobj: source port gobj, must be a child of self
     *
     *      target_port: Use `id` if target_port is an empty string
     *          - string: name of target port gobj, must be a child of target_node
     *          - gobj: target port gobj, must be a child of target_node
     *
     ********************************************/
    function ac_link(self, event, kw, src)
    {
        let links = kw_get_dict_value(kw, "links", null, false, false);

        for(let i=0; i<links.length; i++) {
            let link = links[i];
            let gobj_link = null;
            if(is_gobj(link)) {
                gobj_link = link;

            } else if(is_object(link)) {
                create_link(self, link, kw);
                continue; // goes recurrent ac_add_link() by mt_child_added()
            } else {
                log_error("What is it?" + link);
                continue;
            }

            let k = gobj_link.get_konva_container();
            self.private._gobj_ka_scrollview.gobj_send_event(
                "EV_ADD_ITEM",
                {
                    items: [k]
                },
                self
            );
        }

        return 0;
    }

    /********************************************
     *  Link supported
     ********************************************/
    function ac_unlink(self, event, kw, src)
    {
        let source_node = kw_get_dict_value(kw, "source_node", src);
        // TODO
        return 0;
    }

    /********************************************
     *  TODO review
     ********************************************/
    function ac_show_or_hide(self, event, kw, src)
    {
        let __ka_main__ = self.yuno.gobj_find_service("__ka_main__", true);

        /*--------------------------------------*
         *  Check if it's the show for a view
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
        let position = kw;
        let item_dimension = {};
        self.private._gobj_ka_scrollview.gobj_send_event("EV_GET_DIMENSION", item_dimension, self);

        self.config.x = kw_get_int(position, "x", item_dimension.x, true, false);
        self.config.y = kw_get_int(position, "y", item_dimension.y, true, false);
        self.private._gobj_ka_scrollview.gobj_send_event("EV_POSITION", position, self);
        self.private._gobj_ka_scrollview.gobj_send_event(event, kw, self);

        /*
         *  Global event to close popup window when hit outside
         */
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
     *  Top order
     ********************************************/
    function ac_resize(self, event, kw, src)
    {
        self.config.x = kw_get_int(kw, "x", self.config.x);
        self.config.y = kw_get_int(kw, "y", self.config.y);
        self.config.width = kw_get_int(kw, "width", self.config.width);
        self.config.height = kw_get_int(kw, "height", self.config.height);
        self.private._gobj_ka_scrollview.gobj_send_event(
            "EV_RESIZE",
            {
                x: self.config.x,
                y: self.config.y,
                width: self.config.width,
                height: self.config.height
            },
            self
        );

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
            if(strcmp(event, "EV_MOVED")===0) {
                let grid = self.config.grid;
                let change_position = false;
                if(kw.x < grid || kw.y < grid) {
                    // Refuse negative logic
                    if(kw.x < grid) {
                        kw.x = grid;
                    }
                    if(kw.y < grid) {
                        kw.y = grid;
                    }
                    change_position = true;
                }
                if(grid > 0) {
                    kw.x = Math.round(kw.x/grid) * grid;
                    kw.y = Math.round(kw.y/grid) * grid;
                    if(kw.x < grid) {
                        kw.x = grid;
                    }
                    if(kw.y < grid) {
                        kw.y = grid;
                    }
                    change_position = true;
                }
                if(change_position) {
                    src.gobj_send_event("EV_POSITION", kw, self);
                }

                self.private._gobj_ka_scrollview.gobj_send_event(
                    "EV_RESIZE",
                    {
                    },
                    self
                );
            }
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
            "EV_KEYDOWN",
            "EV_ADD_ITEM",
            "EV_REMOVE_ITEM",
            "EV_LINK",
            "EV_UNLINK",
            "EV_ACTIVATE",
            "EV_DEACTIVATE",
            "EV_TOGGLE",
            "EV_POSITION",
            "EV_SIZE",
            "EV_SHOW",
            "EV_HIDE",
            "EV_RESIZE",

            "EV_PANNING",
            "EV_PANNED",
            "EV_MOVING",
            "EV_MOVED",
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

                ["EV_ADD_ITEM",         ac_add_item,            undefined],
                ["EV_REMOVE_ITEM",      ac_remove_item,         undefined],
                ["EV_LINK",             ac_link,                undefined],
                ["EV_UNLINK",           ac_unlink,              undefined],

                ["EV_ACTIVATE",         ac_activate,            undefined],
                ["EV_DEACTIVATE",       ac_deactivate,          undefined],

                ["EV_PANNING",          ac_panning,             undefined],
                ["EV_PANNED",           ac_panning,             undefined],
                ["EV_MOVING",           ac_moving,              undefined],
                ["EV_MOVED",            ac_moving,              undefined],
                ["EV_SHOWED",           ac_showed,              undefined],
                ["EV_HIDDEN",           ac_hidden,              undefined],

                ["EV_TOGGLE",           ac_show_or_hide,        undefined],
                ["EV_SHOW",             ac_show_or_hide,        undefined],
                ["EV_HIDE",             ac_show_or_hide,        undefined],
                ["EV_POSITION",         ac_position,            undefined],
                ["EV_SIZE",             ac_size,                undefined],
                ["EV_RESIZE",           ac_resize,              undefined]
            ]
        }
    };

    let Sw_graph = GObj.__makeSubclass__();
    let proto = Sw_graph.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Sw_graph",
            kw,
            0 //gcflag_no_check_output_events
        );
        return this;
    };
    gobj_register_gclass(Sw_graph, "Sw_graph");




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
                panning: self.config.panning,       // Enable (inner dragging) panning
                draggable: self.config.draggable,   // Enable (outer dragging) dragging

                autosize: false,
                fix_dimension_to_screen: self.config.fix_dimension_to_screen,
                center: false,
                show_overflow: false,

                enable_vscroll: self.config.enable_vscroll,
                enable_hscroll: self.config.enable_hscroll,
                scroll_by_step: self.config.scroll_by_step,

                quick_display: self.config.quick_display,

                kw_border_shape: __duplicate__(
                    kw_get_dict(self.config, "kw_border_shape", {})
                ),
                kw_border_shape_actived: __duplicate__(
                    kw_get_dict(self.config, "kw_border_shape_actived", {})
                )
            },
            self
        );
        self.private._gobj_ka_scrollview.get_konva_container().gobj = self; // cross-link
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
        self.gobj_send_event(
            "EV_ADD_ITEM",
            {
                items: self.config.items
            },
            self
        );
        self.gobj_send_event(
            "EV_LINK",
            {
                links: self.config.links
            },
            self
        );

        if(self.config.visible) {
            self.gobj_send_event("EV_SHOW", {}, self);
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
     *  Framework Method mt_child_added
     ************************************************/
    proto.mt_child_added = function(child)
    {
        let self = this;

        if(self.private._gobj_ka_scrollview) { // It's null when creating the own child _gobj_ka_scrollview
            if (child.gobj_gclass_name() === "Ka_link") {
                self.gobj_send_event(
                    "EV_LINK",
                    {
                        links: [child]
                    },
                    self
                );
            } else {
                self.gobj_send_event(
                    "EV_ADD_ITEM",
                    {
                        items: [child]
                    },
                    self
                );
            }
        }
    };

    /************************************************
     *  Framework Method mt_child_added
     ************************************************/
    proto.mt_child_removed = function(child)
    {
        let self = this;
        if(child.gobj_gclass_name()==="Ne_base") {
            self.gobj_send_event(
                "EV_REMOVE_ITEM",
                {
                    items: [child]
                },
                self
            );
        }

        if(child.gobj_gclass_name()==="Ka_link") {
            self.gobj_send_event(
                "EV_UNLINK",
                {
                    links: [child]
                },
                self
            );
        }
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
    exports.Sw_graph = Sw_graph;

})(this);
