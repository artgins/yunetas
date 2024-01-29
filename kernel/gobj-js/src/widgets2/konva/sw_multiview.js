/***********************************************************************
 *          Sw_multiview.js
 *
 *          Keeps a set of **gobj** **view** and displays one **view** at a time.
 *
 *          Based in ScrollvieW
 *
 *          When a **view** is added its size will be resize to size of multiview
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
        views: [],

        //------- Ka_scrollview Attributes --- HACK use ka_scrollview directly ---------//
        x: 100,
        y: 100,
        width: 240,
        height: 240,
        padding: 0,
        background_color: "#cccccc",

        visible: true,
        draggable: false,       // Enable (outer dragging) dragging

        fix_dimension_to_screen: false,
        center: false,

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




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *  When a view is added its size will be resize to size of multiview
     *
     *  EV_ADD_VIEW {
     *      "views": [{id:, gclass:, kw: }, ...]
     *      or
     *      "views": [gobj, ...]
     *  }
     *
     *  You can use "name" instead of "id"
     *
     ********************************************/
    function ac_add_view(self, event, kw, src)
    {
        let views = kw_get_dict_value(kw, "views", null, false, false);

        for(let i=0; i<views.length; i++) {
            let view = views[i];
            let child = null;
            if(is_gobj(view)) {
                child = view;
            } else if(is_object(view)) {
                child = self.yuno.gobj_create(
                    kw_get_str(view, "id", kw_get_str(view, "name", "")),
                    kw_get_dict_value(view, "gclass", null),
                    kw_get_dict(view, "kw", {}),
                    self
                );
                continue; // goes recurrent ac_add_view() by mt_child_added()
            } else {
                log_error("What is it?" + view);
                continue;
            }

            child.gobj_send_event(
                "EV_RESIZE",
                self.private._gobj_ka_scrollview.get_util_dimension(),
                self
            );

            let k = child.get_konva_container();
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
     *  EV_REMOVE_VIEW {
     *      "views": ["id", ...]
     *      or
     *      "views": [{id: "id", }, ...]
     *      or
     *      "views": [gobj, ...]
     *  }
     ********************************************/
    function ac_remove_view(self, event, kw, src)
    {
        let views = kw_get_dict_value(kw, "views", null, false, false);

        for(let i=0; i<views.length; i++) {
            let view = views[i];
            let childs = null;
            if(is_string(view)) {
                let name = view;
                childs = self.gobj_match_childs({__gobj_name__: name});
            } else if(is_object(view)) {
                let name = kw_get_str(view, "id", kw_get_str(view, "name", ""));
                childs = self.gobj_match_childs({__gobj_name__: name});
            } else if(is_gobj(view)) {
                childs = [view];
            } else {
                log_error("What is?" + view);
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
     *  To show a view of multiview:
     *      {
     *          "id":
     *          or
     *          "name:
     *      }
     *  else (to show or hide the multiview self):
     *      {
     *          x:
     *          y:
     *      }
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
                let gobj = find_gobj_in_list(self.private._views, name); // TODO elimina, usa gobj api
                if(gobj) {
                    gobj.get_konva_container().moveToTop();
                    __ka_main__.gobj_send_event("EV_ACTIVATE", {}, gobj);
                }
            }
            return 0;
        }

        /*-----------------------------*
         *  Show/Hide self multiview
         *-----------------------------*/
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
     *  Some child moving
     ********************************************/
    function ac_moving(self, event, kw, src)
    {
        return 0;
    }

    /********************************************
     *  Some child moved
     ********************************************/
    function ac_moved(self, event, kw, src)
    {
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

        // TODO cambia size of all views

        return 0;
    }

    /********************************************
     *
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

        let childs = self.gobj_match_childs();
        for(let i=0; i<childs.length; i++) {
            let child = childs[i];
            if(child.gobj_name() === self.gobj_name()) {
                // Inside container
                continue;
            }
            child.gobj_send_event(
                "EV_RESIZE",
                self.private._gobj_ka_scrollview.get_util_dimension(),
                self
            );
        }

        return 0;
    }




            /***************************
             *      GClass/Machine
             ***************************/




    let FSM = {
        "event_list": [
            "EV_KEYDOWN",
            "EV_ADD_VIEW",
            "EV_REMOVE_VIEW",
            "EV_ACTIVATE",
            "EV_DEACTIVATE",
            "EV_MOVING: output no_warn_subs",
            "EV_MOVED: output no_warn_subs",
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
                ["EV_ADD_VIEW",         ac_add_view,            undefined],
                ["EV_REMOVE_VIEW",      ac_remove_view,         undefined],
                ["EV_ACTIVATE",         ac_activate,            undefined],
                ["EV_DEACTIVATE",       ac_deactivate,          undefined],
                ["EV_MOVING",           ac_moving,              undefined],
                ["EV_MOVED",            ac_moved,               undefined],
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

    let Sw_multiview = GObj.__makeSubclass__();
    let proto = Sw_multiview.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Sw_multiview",
            kw,
            0 //gcflag_no_check_output_events
        );
        return this;
    };
    gobj_register_gclass(Sw_multiview, "Sw_multiview");




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

        let visible = self.config.visible;

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

                visible: visible,
                panning: false,                     // Enable (inner dragging) panning
                draggable: self.config.draggable,   // Enable (outer dragging) dragging

                autosize: false,
                fix_dimension_to_screen: self.config.fix_dimension_to_screen,
                center: self.config.center,
                show_overflow: false,

                enable_vscroll: false,
                enable_hscroll:  false,
                scroll_by_step: false,

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

        self.gobj_send_event("EV_ADD_VIEW", {views: self.config.views}, self);
        if(visible) {
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
     *  Framework Method mt_child_added
     ************************************************/
    proto.mt_child_added = function(child)
    {
        let self = this;

        if(self.private._gobj_ka_scrollview) {
            // When a view is added it will be resize to size of multiview
            self.gobj_send_event(
                "EV_ADD_VIEW",
                {
                    views: [child]
                },
                self
            );
        }
    };

    /************************************************
     *  Framework Method mt_child_added
     ************************************************/
    proto.mt_child_removed = function(child)
    {
        let self = this;
        self.gobj_send_event(
            "EV_REMOVE_VIEW",
            {
                views: [child]
            },
            self
        );
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
    exports.Sw_multiview = Sw_multiview;

})(this);
