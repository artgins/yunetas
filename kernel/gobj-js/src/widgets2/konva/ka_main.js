/***********************************************************************
 *          Ka_main.js
 *
 *          KA Main
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
        subscriber: null,       // Subscriber of published events, by default the parent.

        container: "gui_canvas",  // id of html canvas tag
        background_color: "#cccccc",
        // HACK set maximum size(w or h) to work well in mobiles on orientation changes
        width: window.innerWidth,
        height: window.innerHeight,
        stage: null,        // Main Konva Stage
        main_layer: null,   // Main Konva Layer
        static_layer: null, // Top Konva Layer
        modal_layer: null,  // Modal Konva Layer
        debug_dimensions: false, // paint dimension on mouseenter to konva's nodes

        // Private data
        _ka_container: null,    // Set the stage as _ka_container
        _dl_activation: [], // List of active windows.
    };

    let activation_service_json_desc = {
        id: "",
        modal: false,       // Activation SERVICE: (webix) Outside disabled but Esc or clicking out will close
        super_modal: false, // Activation SERVICE: Outside disabled and only inside action will close
        autoclose: false,   // Activation SERVICE: Close window on pointerup bubbling to stage or Esc key
    };

    let first_resize = true;


            /***************************
             *      Local Methods
             ***************************/




    /********************************************
     *  Colors from zimjs project
     ********************************************/
    // zim.orange 	= "#f58e25";
    // zim.green  	= "#acd241";
    // zim.pink  	= "#e472c4";
    // zim.blue  	= "#50c4b7";
    // zim.brown 	= "#d1a170";
    // zim.yellow	= "#ebcb35";
    // zim.purple	= "#993399";
    // zim.red   	= "#fb4758"; // red dedicated to Alexa
    // this.black  	= "#000000";
    // this.darker 	= "#111111";
    // this.dark   	= "#333333";
    // this.grey   	= "#555555";
    // this.gray   	= "#555555";
    // this.tin    	= "#777777";
    // this.silver 	= "#999999";
    // this.fog    	= "#aaaaaa";
    // this.mist   	= "#bbbbbb";
    // this.light   = "#cccccc";
    // this.moon   	= "#dddddd";
    // this.lighter	= "#eeeeee";
    // this.white  	= "#ffffff";
    // this.clear  	= "rgba(0,0,0,0)";
    // this.faint   = "rgba(0,0,0,.01)";

    /********************************************
     *
     ********************************************/
    function get_e_src(target)
    {
        while(target) {
            if(target.gobj) {
                return target.gobj;
            }
            target = target.parent;
        }
        return null;
    }

    /********************************************
     *
     ********************************************/
    function get_current_active(self)
    {
        if(self.private._dl_activation.length > 0) {
            return self.private._dl_activation[self.private._dl_activation.length - 1];
        }
        return null;
    }

    /********************************************
     *
     ********************************************/
    function find_bind_gobj(k)
    {
        if(k.gobj) {
            return k.gobj;
        }
        let k_parent = k.getParent?k.getParent():null;
        if(k_parent) {
            return find_bind_gobj(k_parent);
        }
    }

    /********************************************
     *
     ********************************************/
    function create_canvas(self)
    {
        self.config.width = window.innerWidth;
        self.config.height = window.innerHeight;

        /*--------------------------------------*
         *  Firstly we need to create a stage
         *--------------------------------------*/
        let stage = self.config.stage = new Konva.Stage({
            id: self.config.container,
            container: self.config.container,
            width: self.config.width,
            height: self.config.height
        });
        self.private._ka_container = stage;
        stage.gobj = self;

        /*--------------------------------------*
         *          Events
         *--------------------------------------*/
        let container = stage.container();

        /*------------------*
         *      Pointer
         *-------------------*/
        stage.on("pointerup", function (e) {
            // No uses pointerdown, sigue con pointerup, así es como trabajan otras aplicaciones (webix,google)
            let src = get_e_src(e.target);
            if(self.is_tracing()) {
                log_warning(sprintf("%s.%s ==> (%s), cancelBubble: %s, gobj: %s, ka_id: %s, ka_name: %s",
                    "Ka_main", "stage",
                    e.type,
                    (e.cancelBubble)?"Y":"N",
                    src?src.gobj_short_name():"",
                    kw_get_str(e.target.attrs, "id", ""),
                    kw_get_str(e.target.attrs, "name", "")
                ));
            }
            let current_active_gobj = get_current_active(self);
            if(!current_active_gobj) {
                return;
            }

            if(src) {
                /*
                 *  If come from other window in list then activate it
                 */
                let id = src.gobj_full_name();
                if(kwid_find_one_record(self.private._dl_activation, id)) {
                    if(current_active_gobj.gobj != src) {
                        // EV_ACTIVATE/EV_DEACTIVATE must be idempotent
                        self.gobj_send_event("EV_ACTIVATE", {}, src);  // auto-activation click on new window
                    }
                }
            }

            if(current_active_gobj.modal && !current_active_gobj.super_modal) {
                /*
                 *  If modal and from other window then close it
                 */
                if(current_active_gobj.gobj != src) {
                    /*
                     *  (in super_modal window only clicks inside are permitted)
                     */
                    current_active_gobj.gobj.gobj_send_event("EV_HIDE", {}, self);
                }
            }

            if(current_active_gobj.autoclose) {
                /*
                 *  If autoclose and event bubbling to stage then close it
                 */
                current_active_gobj.gobj.gobj_send_event("EV_HIDE", {}, self);
            }
        });

        /*------------------*
         *      Keydown
         *-------------------*/
        container.addEventListener("keydown", function (e) {
            // if(self.is_tracing() || 0) {
            //     log_warning("konva stage container ===> ON KEYDOWN");
            // }
            let kw = {
                code: e.code,
                key: e.key,
                keyCode: e.keyCode,
                ctrlKey: e.ctrlKey,
                altKey: e.altKey,
                shiftKey: e.shiftKey,
                repeat: e.repeat
            };
            /*
             *  WARNING:
             *      ; is 59 in Firefox but 186 in other browsers.
             *      = is 61 in Firefox but 187 in other browsers.
             *      - is 173 in Firefox but 189 in other browsers.
             *
             *  WARNING: don't use e as kw, it's not an object and __duplicate__ will fail
             */

            /*
             *  Ya no es un servicio abierto a cualquiera (casi).
             *  Ahora solo se enviará a quien se registre y además sea el obj activado.
             *  Bueno, si no hay lista entonces publica.
             */
            let current_active_gobj = get_current_active(self);
            let ret = 0;
            if(current_active_gobj) {
                switch(kw.keyCode) {
                    case 27: // Escape
                        if((current_active_gobj.modal && !current_active_gobj.super_modal)
                                || current_active_gobj.autoclose) {
                            // Escape close the window
                            current_active_gobj.gobj.gobj_send_event("EV_HIDE", {}, self);
                            ret = -1; // No es mejor que se propague?
                        }
                        break;
                    default:
                        if(current_active_gobj.gobj.gobj_event_in_input_event_list("EV_KEYDOWN")) {
                            ret = current_active_gobj.gobj.gobj_send_event("EV_KEYDOWN", kw);
                        }
                        break;
                }
            } else {
                // Ignore return of publish
                self.gobj_publish_event("EV_KEYDOWN", kw);
            }
            if(ret < 0) {
                /*
                 *  Event has been owned
                 *  Ex: Esc
                 *      keyCode: 27,
                 *      code: "Escape",
                 *      ctrlKey: false,
                 *      altKey: false,
                 *      isComposing: false,
                 */
                e.preventDefault();
                e.stopPropagation();
            }
        });

        /*------------------*
         *      Focus
         *-------------------*/
        // make it focusable
        container.tabIndex = 1;
        // focus it
        // also stage will be in focus on its click
        container.focus();
        container.addEventListener("focus", function (e) {
            if(self.is_tracing()) {
                log_warning(sprintf("%s.%s ==> (%s), cancelBubble: %s, gobj: %s, ka_id: %s, ka_name: %s",
                    "Ka_main", "stage.container",
                    e.type,
                    (e.cancelBubble)?"Y":"N",
                    self.gobj_short_name(),
                    kw_get_str(e.target.attrs, "id", ""),
                    kw_get_str(e.target.attrs, "name", "")
                ));
            }
            let current_active_gobj = get_current_active(self);
            if(current_active_gobj) {
                // EV_ACTIVATE/EV_DEACTIVATE must be idempotent
                current_active_gobj.gobj.gobj_send_event("EV_ACTIVATE", {}, self); // deactivate on focus
            }
        });

        /*------------------*
         *      Blur
         *-------------------*/
        container.addEventListener("blur", function (e) {
            if(self.is_tracing()) {
                log_warning(sprintf("%s.%s ==> (%s), cancelBubble: %s, gobj: %s, ka_id: %s, ka_name: %s",
                    "Ka_main", "stage.container",
                    e.type,
                    (e.cancelBubble)?"Y":"N",
                    self.gobj_short_name(),
                    kw_get_str(e.target.attrs, "id", ""),
                    kw_get_str(e.target.attrs, "name", "")
                ));
            }
            let current_active_gobj = get_current_active(self);
            if(current_active_gobj) {
                // EV_ACTIVATE/EV_DEACTIVATE must be idempotent
                current_active_gobj.gobj.gobj_send_event("EV_DEACTIVATE", {}, self); // deactivate on blur
            }
        });

        /*--------------------------*
         *  Set background color
         *--------------------------*/
        stage.container().style.backgroundColor = self.config.background_color;
    }

    /********************************************
     *
     ********************************************/
    function create_layers(self)
    {
        let stage = self.config.stage;

        /*--------------------------*
         *  Create main layers
         *--------------------------*/
        let modal_layer = self.config.modal_layer = new Konva.Layer({
            id: "modal_layer"
        });
        modal_layer.gobj = self;

        let main_layer = self.config.main_layer = new Konva.Layer({
            id: "main_layer"
        });
        main_layer.gobj = self;

        let static_layer = self.config.static_layer = new Konva.Layer({
            id: "static_layer"
        });
        static_layer.gobj = self;

        /*--------------------------*
         *  add layers to the stage
         *--------------------------*/
        stage.add(main_layer);
        stage.add(modal_layer);
        stage.add(static_layer);

        /*-----------------------*
         *      Debugging
         *-----------------------*/
        if(1) {
            stage.on("contextmenu", function (e) {
                // WARNING No va en mobiles
                let jn = null;
                let gobj = find_bind_gobj(e.target);
                if(gobj) {
                    let k = gobj.get_konva_container? gobj.get_konva_container():null;
                    if(k) {
                        if(e.evt.ctrlKey) {
                            jn = log_konva_tree0(k, false);
                        } else if(e.evt.altKey) {
                            jn = log_konva_tree0(self.get_konva_container(), false);
                        } else {
                            jn = __yuno__.gobj_list_gobj_tree(__yuno__);
                        }
                        create_json_editor_window({
                            props: {
                                indentation: 4,
                                content: {
                                    text: undefined,
                                    json: jn
                                }
                            }
                        });

                        e.evt.preventDefault();
                        e.evt.stopPropagation();
                    }
                } else {
                    jn = e.target;
                }
                //console.log(jn);
            });
        }

        if(self.config.debug_dimensions) { // DEBUGGING
            let node_info = null;
            if (!node_info) {
                node_info = new Konva.Text({
                    id: "__debug_dimensions__",
                    text: "()",
                    fontSize: 14,
                    draggable: true,
                    x: 100,
                    y: 100,
                    // stroke: "black"
                });
                modal_layer.add(node_info);
            }

            stage.on("pointerenter pointerover", function (e) {
                if(node_info) {
                    let id = e.target.id && e.target.id();
                    if(id !== "__debug_dimensions__") {
                        let gobj = find_bind_gobj(e.target);
                        let gobj_name = gobj?gobj.gobj_short_name():"";
                        let dim_abs = e.target.getClientRect({skipShadow:true, skipStroke:true});
                        let parent_container;
                        try {
                            parent_container = gobj.gobj_parent().get_konva_container();
                        } catch(xx) {
                            parent_container = e.target.getParent();
                        }
                        let dim_rel = e.target.getClientRect({
                            skipShadow:true, skipStroke:true, relativeTo: parent_container
                        });
                        let info = sprintf("(x %f, y %f, w %f, h %f),(x %f, y %f, w %f, h %f) stroke %d, blur %d, %s %s %s",
                            dim_rel.x, dim_rel.y, dim_rel.width, dim_rel.height,
                            dim_abs.x, dim_abs.y, dim_abs.width, dim_abs.height,
                            e.target.strokeWidth?e.target.strokeWidth():0,
                            e.target.shadowBlur?e.target.shadowBlur():0,
                            e.target.id(),
                            e.target.name(),
                            gobj_name
                        );
                        node_info.moveToTop();
                        dim_abs = e.target.getClientRect(); // Get absolute coords
                        node_info.position({
                            x: dim_abs.x<0?0:dim_abs.x,
                            y: dim_abs.y<0?0:dim_abs.y
                        });
                        node_info.text(info);
                    }
                }
            });
        }
    }

    /********************************************
     *
     ********************************************/
    function subscribe_to_system_resize(self)
    {
        /*
         *  Subscribe to resize event
         */
        window.onresize = resize;

        function resize() {
            //trace_msg(sprintf("inner w %d h %d", window.innerWidth, window.innerHeight));
            //trace_msg(sprintf("inner w %d h %d", window.innerWidth, window.innerHeight));

            document.body.style.width = window.innerWidth + "px";
            document.body.style.height = window.innerHeight + "px";

            self.config.width = window.innerWidth;
            self.config.height = window.innerHeight;
            self.gobj_send_event(
                "EV_RESIZE",
                {
                    width: self.config.width,
                    height: self.config.height
                },
                self
            );
        }

        // resize();
    }




            /***************************
             *      Actions
             ***************************/




    /*
     *  Activation is a central service, where a window must register as being the main input interface for user.
     *  The window register to service, and the service is responsible to send the EV_ACTIVATE to window,
     *  if considerate correct. It's a filter to centralize the managing of activate/deactivate o reactivate
     *  the window in the list of registered windows.
     */
    /********************************************
     *  Activation service
     *  src wants to activate his window
     ********************************************/
    function ac_activate(self, event, kw, src)
    {
        let id = src && src.gobj_full_name();
        if(empty_string(id)) {
            log_error(sprintf("%s: ac_activate() src null", self.gobj_short_name()));
            return -1;
        }
        /*
         *  Get last data (currently active record)
         */
        let prev_active_record = get_current_active(self);
        if(prev_active_record && id !==  prev_active_record.id) {
            // EV_ACTIVATE/EV_DEACTIVATE must be idempotent
            prev_active_record.gobj.gobj_send_event("EV_DEACTIVATE", {}, self);  // deactivate on new activation
        }

        /*
         *  System Queue: Last record is the active record.
         *  Append actions:
         *      Record is deleted from queue.
         *      Record is append to queue
         */
        if(kwid_find_one_record(self.private._dl_activation, id)) {
            // Id already in the queue: delete it
            kwid_delete_record(self.private._dl_activation, id);
        }

        /*
         *  Build user data (new or in-use)
         *      - data from parameters filtered by schema
         *      - data from source attributes
         */
        let new_record = create_json_record(activation_service_json_desc, kw); // from parameters (schema filtered)
        json_object_update_existing(new_record, src.config);    // data from source attributes
        new_record["id"] = id;
        new_record["gobj"] = src;

        // Append data
        self.private._dl_activation.push(new_record);

        if(new_record.modal || new_record.super_modal) {
            /*
             *  Technique to opaque all screen minus modal window:
             *      - Move window to modal_layer
             *      - Move modal_layer to top of layers
             *      - Opacity main_layer
             *      - Disable input in main_layer
             */
            src.get_konva_container().moveTo(self.config.modal_layer);
            self.config.modal_layer.moveToTop();

            self.config.main_layer.opacity(0.2);
            self.config.static_layer.opacity(0.2);
            self.config.main_layer.listening(false);
            self.config.static_layer.listening(false);
        }

        // EV_ACTIVATE/EV_DEACTIVATE must be idempotent
        new_record.gobj.gobj_send_event("EV_ACTIVATE", {}, self);

        return 0;
    }

    /********************************************
     *  Activation service
     *  src wants to deactivate his window
     ********************************************/
    function ac_deactivate(self, event, kw, src)
    {
        let id = src && src.gobj_full_name();
        if(empty_string(id)) {
            log_error(sprintf("%s: ac_deactivate(), src NULL", self.gobj_short_name()));
            return -1;
        }

        /*
         *  Get last data (currently active record)
         */
        let prev_active_record = get_current_active(self);

        /*
         *  System Queue: Last record is the active record.
         *  Delete actions:
         *      Record is deleted from queue.
         */
        let record_to_deactivate = kwid_find_one_record(self.private._dl_activation, id);
        if(record_to_deactivate) {
            if((record_to_deactivate.modal || record_to_deactivate.super_modal)) {
                // Un-Opaque main_layer
                src.get_konva_container().moveTo(self.config.main_layer);
                self.config.main_layer.moveToTop();
                self.config.static_layer.moveToTop();

                self.config.main_layer.opacity(1);
                self.config.static_layer.opacity(1);
                self.config.main_layer.listening(true);
                self.config.static_layer.listening(true);
            }

            // Id already in the queue: delete it
            kwid_delete_record(self.private._dl_activation, id);
        } else {
            log_error(sprintf("%s: deactivate src not found: %s", self.gobj_short_name(), id));
        }

        /*
         *  DeActivation
         */
        src.gobj_send_event("EV_DEACTIVATE", {}, self);  // deactivate on deactivation

        /*
         *  Activation current new
         */
        let new_active_record = get_current_active(self);
        if(new_active_record) {
            if(new_active_record.id != prev_active_record.id) {
                // EV_ACTIVATE/EV_DEACTIVATE must be idempotent
                new_active_record.gobj.gobj_send_event("EV_ACTIVATE", {}, self);
            }
        }

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_resize(self, event, kw, src)
    {
        // draw both scene and hit graphs.
        // If the node being drawn is the stage, all the layers will be cleared and redrawn
        // set size of stage: HACK all their layers will be set at the same size

        let new_width = kw.width;
        let new_height = kw.height;

        if(first_resize || 1) {
            if(new_width > 0 && new_height > 0) {
                self.config.stage.size({
                    width: new_width,
                    height: new_height
                });

                // Send directly to childs events and publish for others
                self.gobj_send_event_to_childs(event, kw, src);
                self.gobj_publish_event(event, kw, src);
                self.config.stage.draw();
                first_resize = false;
            }
        } else {
            if(new_width !== self.config.stage.width() || new_height !== self.config.stage.height()) {
                let old_size = self.config.stage.size();
                self.config.stage.size({
                    width: new_width,
                    height: new_height
                });

                self.config.stage.scale({
                    x: new_width / old_size.width,
                    y: new_height / old_size.height
                });
            }
        }

        return 0;
    }




            /***************************
             *      GClass/Machine
             ***************************/




    let FSM = {
        "event_list": [
            "EV_ACTIVATE",
            "EV_DEACTIVATE",
            "EV_RESIZE: output no_warn_subs",
            "EV_KEYDOWN: output no_warn_subs"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["EV_ACTIVATE",             ac_activate,            undefined],
                ["EV_DEACTIVATE",           ac_deactivate,          undefined],
                ["EV_RESIZE",               ac_resize,              undefined]
            ]
        }
    };

    let Ka_main = GObj.__makeSubclass__();
    let proto = Ka_main.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Ka_main",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Ka_main, "Ka_main");




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

        document.body.style.backgroundColor = self.config.background_color;

        create_canvas(self);
        create_layers(self);
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

        setTimeout(
            function() {
                self.config.stage.draw();
                subscribe_to_system_resize(self);
            },
            200
        );

    };

    /************************************************
     *      Framework Method stop
     ************************************************/
    proto.mt_stop = function(kw)
    {
        let self = this;
    };

    /************************************************
     *      Framework Method child_added
     ************************************************/
    proto.mt_child_added = function(kw)
    {
        let self = this;

        let kw_size = {
            width: self.config.width,
            height: self.config.height
        };

        self.gobj_send_event("EV_RESIZE", kw_size, self);
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
    proto.get_stage = function()
    {
        let self = this;
        return self.config.stage;
    };

    /************************************************
     *      Local Method
     ************************************************/
    proto.get_main_layer = function()
    {
        let self = this;
        return self.config.main_layer;
    };

    /************************************************
     *      Local Method
     ************************************************/
    proto.get_static_layer = function()
    {
        let self = this;
        return self.config.static_layer;
    };

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.Ka_main = Ka_main;
    exports.find_bind_gobj = find_bind_gobj;
})(this);
