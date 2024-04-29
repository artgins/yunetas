/*********************************************************************************
 *  Objects with a simple Finite State Machine.
 *
 *  Author: Niyamaka
 *  Email: Niyamaka at yuneta.io
 *  Licence: MIT (http://www.opensource.org/licenses/mit-license)
 *
 *  Last revision:
 *      20 Junio 2014 - Upgraded to yuneta api.
 *          - changes in send_inter_event.
 *          - removed Event class. The even now is a simple string like C Yuneta.
 *
 *      15 Julio 2015 - Upgraded to yuneta 1.0.0.
 *
 *********************************************************************************/

/**************************************************************************
 *        GObj
 **************************************************************************/
let __inside_event_loop__ = 0;

(function(exports) {
    // Place the script in strict mode
    "use strict";

    /************************************************************
     *  Update existing in first level, and all in next levels
     ************************************************************/
    function json_object_update_config(destination, source) {
        if(!source) {
            return destination;
        }
        for (let property in source) {
            if (source.hasOwnProperty(property) && destination.hasOwnProperty(property)) {
                if(is_object(destination[property]) && is_object(source[property])) {
                    json_object_update(
                        destination[property],
                        source[property]
                    );
                } else {
                    destination[property] = source[property];
                }
            }
        }
        return destination;
    }

    /************************************************************
     *      GObj class.
     ************************************************************/
    let GObj = Object.__makeSubclass__();
    let proto = GObj.prototype; // Easy access to the prototype

    proto.__init__ = function(fsm_desc, config, name, gclass_name, kw, gcflag) {
        this.name = name || '';
        this.gclass_name = gclass_name || '';
        this.config = __duplicate__(config);
        json_object_update_config(this.config, kw || {});
        this.private = kw_extract_private(this.config);
        this.command_table = {};
        this.gcflag = gcflag;

        this.user_data = {};
        this.yuno = undefined;
        this.parent = undefined;
        this.dl_subscriptions = [];
        this.dl_childs = [];
        this.tracing = 0;
        this.trace_timer = 0;
        this.running = false;
        this._destroyed = false;
        this.timer_id = -1; // for now, only one timer per fsm, and hardcoded.
        this.timer_event_name = "EV_TIMEOUT";
        this.fsm_create(fsm_desc); // Create this.fsm

        return this;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_start = function()
    {
        if(this.running) {
            log_error("gobj_start() ALREADY RUNNING");
            return -1;
        }
        this.running = true;
        if(this.mt_start) {
            if(this.is_tracing()) {
                log_debug(sprintf("⏺ ⏺ start: %s", this.gobj_full_name()));
            }
            return this.mt_start();
        }
        return 0;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_stop = function()
    {
        if(!this.running) {
            log_error("gobj_stop() NOT RUNNING");
            return -1;
        }
        this.running = false;
        if(this.mt_stop) {
            if(this.is_tracing()) {
                log_debug(sprintf("⏺ ⏺ stop: %s", this.gobj_full_name()));
            }
            this.mt_stop();
        }
        if(this.__volatil__) {
            return this.yuno.gobj_destroy(this);
        }
        return 0;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_show = function()
    {
        if(this.mt_show) {
            return this.mt_show();
        }
        return -1;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_hide = function()
    {
        if(this.mt_hide) {
            return this.mt_hide();
        }
        return -1;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_is_running = function()
    {
        return this.running;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_start_tree = function()
    {
        if((this.gcflag & gcflag_manual_start)) {
            return 0;
        }

        if(this.is_tracing()) {
            log_debug(sprintf("⏺ ⏺ ⏺ ⏺ start_tree: %s", this.gobj_full_name()));
        }
        if(!this.running) {
            this.gobj_start();
        }
        let set = this.dl_childs;
        for(let i=0; i<set.length; i++) {
            let child = set[i];
            if(child) {
                child.gobj_start_tree();
            }
        }

        return 0;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_stop_tree = function()
    {
        if((this.gcflag & gcflag_manual_start)) {
            return 0;
        }
        if(this.is_tracing()) {
            log_debug(sprintf("⏺ ⏺ ⏺ ⏺ stop_tree: %s", this.gobj_full_name()));
        }
        if(this.running) {
            this.gobj_stop();
        }
        let set = this.dl_childs;
        for(let i=0; i<set.length; i++) {
            let child = set[i];
            if(child) {
                child.gobj_stop_tree();
            }
        }
        return 0;
    };

    /************************************************************
     *        add child.
     ************************************************************/
    proto._add_child = function(gobj) {
        if (gobj.parent) {
            log_error("GObj._add_child(): ALREADY HAS PARENT");
        }
        this.dl_childs.push(gobj);
        gobj.parent = this;
    };

    /************************************************************
     *        remove child
     ************************************************************/
    proto._remove_child = function(gobj) {
        let index = index_in_list(this.dl_childs, gobj);
        if (index >= 0) {
            this.dl_childs.remove(index);
        }
    };

    /************************************************************
     *      Return child in index position
     ************************************************************/
    proto.gobj_child_by_index = function(index) {
        return this.dl_childs[index];
    };

    /************************************************************
     *      Return the size of childs of gobj
     ************************************************************/
    proto.gobj_child_size = function()
    {
        return this.dl_childs.length;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_is_service = function()
    {
        return this.__service__;
    };

    /************************************************************
     *  HACK Los servicios se registran como únicos
     ************************************************************/
    proto.gobj_is_unique = function()
    {
        return this.__unique__ || this.__service__;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_is_volatil = function()
    {
        return this.__volatil__;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_load_persistent_attrs = function()
    {
        if(this.gobj_is_service() || this.gobj_is_unique()) {
            if(this.yuno.__global_load_persistent_attrs_fn__) {
                return this.yuno.__global_load_persistent_attrs_fn__(this);
            }
        }
        return -1;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_save_persistent_attrs = function()
    {
        if(this.gobj_is_service() || this.gobj_is_unique()) {
            if(this.yuno.__global_save_persistent_attrs_fn__) {
                return this.yuno.__global_save_persistent_attrs_fn__(this);
            }
        }
        return -1;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_remove_persistent_attrs = function(recursive)
    {
        if(!this.yuno.__global_remove_persistent_attrs_fn__) {
            return -1;
        }
        this.yuno.__global_remove_persistent_attrs_fn__(this, recursive);

        if(recursive)  {
            let set = this.dl_childs;
            for(let i=0; i<set.length; i++) {
                let child = set[i];
                if(child) {
                    child.gobj_remove_persistent_attrs(recursive);
                }
            }
        }

        return 0;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_get_writable_attrs = function()
    {
        // TODO tabla de atributos como en C. De momento una lista aparte
        if(this.config && this.config.__writable_attrs__) {
            return this.config.__writable_attrs__;
        } else {
            return [];
        }
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_update_writable_attrs = function(attrs)
    {
        if(this.config && this.config.__writable_attrs__) {
            for(let attr in attrs) {
                if(elm_in_list(attr, this.config.__writable_attrs__)) {
                    let new_value = attrs[attr];
                    this.config[attr] = new_value;
                }
            }
        }
    };

    /************************************************************
     *
     ************************************************************/
    function _gobj_search_path(gobj, path)
    {
        if(!is_gobj(gobj)) {
            // silence
            return 0;
        }

        return gobj;
    }

    /************************************************************
     *
     ************************************************************/
    proto.gobj_write_attr = function(path, value)
    {
        // TODO implement inherited attributes
        // TODO if attribute not found then find in bottom gobj

        if(!is_string(path)) {
            log_error(sprintf(
                "gobj_write_attr(%s): path must be a string",
                this.gobj_short_name())
            );
            return -1;
        }
        let ss = path.split("`");
        if(ss.length<=1) {
            let key = path;
            if(key in this.config) {
                if (this.config.hasOwnProperty(key)) {
                    this.config[key] = value;

                    if(this.mt_writing) {
                        this.mt_writing(key);
                    }
                    return 0;
                }
            }
            log_error(sprintf("gobj_write_attr(%s): attr not found '%s'", this.gobj_short_name(), key));
            return -1;
        }

        let gobj = this;
        let len = ss.length;
        for(let i=0; i<len; i++) {
            let key = ss[i];
            let child = gobj.gobj_find_child({"__gobj_name__": key});
            if(child && i<len-1) {
                gobj = child;
                continue;
            }

            if(key in gobj.config) {
                if (gobj.config.hasOwnProperty(key)) {
                    gobj.config[key] = value;

                    if(gobj.mt_writing) {
                        gobj.mt_writing(key);
                    }
                    return 0;
                }
            }
            break;
        }

        log_error(sprintf("gobj_write_attr(%s): attr not found '%s'", this.gobj_short_name(), key));
        return -1;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_read_attr = function(key)
    {
        // TODO implement inherited attributes
        // TODO if attribute not found then find in bottom gobj
        if(key in this.config) {
            if(this.config.hasOwnProperty(key)) {
                let value = this.config[key];
                if(this.mt_reading) {
                    return this.mt_reading(key, value);
                }
                return value;
            }
        }
        return null;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_write_user_data = function(key, value)
    {
        this.user_data[key] = value;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_kw_set_user_data = function(path, value)
    {
        return kw_set_dict_value(this.user_data, path, value);
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_read_user_data = function(key)
    {
        return this.user_data[key];
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_kw_get_user_data = function(path, default_value, create)
    {
        return kw_get_dict_value(this.user_data, path, default_value, create);
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_has_attr = function(key)
    {
        if(key in this.config) {
            if (this.config.hasOwnProperty(key)) {
                return true;
            }
        }
        return false;
    };

    /************************************************************
     *  Match child.
     ************************************************************/
    proto._match_child = function(properties)
    {
        let __gclass_name__ = properties.__gclass_name__;
        let __gobj_name__ = properties.__gobj_name__;
        let __prefix_gobj_name__ = properties.__prefix_gobj_name__;
        let __state__ = properties.__state__;

        /*
         *  Check the system keys of the jn_filter used in find loop
         */
        if(__gclass_name__) {
            if(__gclass_name__ !== this.gclass_name) {
                return false;
            }
        }
        if(__gobj_name__) {
            if(__gobj_name__ !== this.name) {
                return false;
            }
        }
        if(__prefix_gobj_name__) {
            let l = __prefix_gobj_name__.length;
            let name = this.name.substring(0, l);
            if(__prefix_gobj_name__ !== name) {
                return false;
            }
        }
        if(__state__) {
            let state = this.gobj_current_state();
            if(__state__ !== state) {
                return false;
            }
        }

        for(let key in properties) {
            if(key === "__gclass_name__" ||
                    key === "__gobj_name__" ||
                    key === "__prefix_gobj_name__" ||
                    key === "__state__") {
                continue;
            }
            if(this.gobj_has_attr(key)) {
                if(this.config[key] !== properties[key]) {
                    return false;
                }
            }
        }
        return true;
    };

    /************************************************************
     *  Returns the first matched child.
     ************************************************************/
    proto.gobj_find_child = function(kw)
    {
        kw = kw || {};
        let set = this.dl_childs;
        for(let i=0; i<set.length; i++) {
            let child = set[i];
            if(child._match_child(kw)) {
                return child;
            }
        }
        return null;
    };

    /***************************************************************************
     *  Return the child of gobj by name.
     *  The first found is returned.
     ***************************************************************************/
    proto.gobj_child_by_name = function(name)
    {
        return this.gobj_find_child({"__gobj_name__": name});
    };

    /************************************************************
     *  Return child's list matched with kw attributes
     ************************************************************/
    proto.gobj_match_childs = function(kw)
    {
        kw = kw || {};
        let childs = [];
        let set = this.dl_childs;
        for(let i=0; i<set.length; i++) {
            let child = set[i];
            if(child._match_child(kw)) {
                childs.push(child);
            }
        }
        return childs;
    };

    /************************************************************
     *  Format
     ************************************************************/
    proto.gobj_build_webix_answer = function(
        gobj,
        result,
        comment,
        schema,
        data,
        kw        // to extract ONLY __md_iev__ of source kw.
    ) {
        let webix = {
            "result": result,
            "comment": comment,
            "schema": schema,
            "data": data
        };
        let webix_answer = msg_iev_answer(gobj, kw, webix);
        return webix_answer;
    };

    /************************************************************
     *  Exec gobj command. Return a webix
     ************************************************************/
    proto.gobj_command = function(command, kw, src) {
        /*-----------------------------------------------*
         *  The local mt_command_parser has preference
         *-----------------------------------------------*/
        if(this.mt_command) {
            let tracing = this.is_tracing();
            if (tracing) {
                let hora = get_current_datetime();
                let msg = sprintf("%s%s!> cmd: %s, cmd: %s, src: %s",
                    hora,
                    this._tab(),
                    this.gobj_short_name(),
                    command,
                    src?src.gobj_short_name():"undefined"
                );
                log_debug(msg);
                if(tracing > 1) {
                    trace_msg(kw);
                }
            }
            return this.mt_command(command, kw, src);
        }

        /*-----------------------------------------------*
         *  If it has command_table
         *  then use the global command parser
         *-----------------------------------------------*/
        // TODO

        return this.gobj_build_webix_answer(
            this,
            -1,
            this.gobj_short_name() + " has no mt_command method.",
            null,
            null,
            kw
        );
    };

    /************************************************************
     *  Exec gobj stats. Return a webix
     ************************************************************/
    proto.gobj_stats = function(stat, kw, src) {
        /*--------------------------------------*
         *  The local mt_stats has preference
         *--------------------------------------*/
        if(this.mt_stats) {
            return this.mt_stats(stat, kw, src);
        }

        /*-----------------------------------------------*
         *  Then use the global stats parser
         *-----------------------------------------------*/
        // TODO

        return this.gobj_build_webix_answer(
            this,
            -1,
            this.gobj_short_name() + " has no mt_stats method.",
            null,
            null,
            kw
        );
    };

    /************************************************************
     *        set tracing for this gobj.
     ************************************************************/
    proto.set_machine_trace = function(value)
    {
        this.tracing = value;
    };
    proto.is_tracing = function(event)
    {
        if(event && event == this.timer_event_name &&
                (this.yuno && !this.yuno.config.trace_timer)) {
            return false;
        }
        return this.tracing || (this.yuno && this.yuno.config.tracing);
    };

    /************************************************************
     *        Timeout functions
     ************************************************************/
    proto.set_timeout = function(msec)
    {
        let gobj = this;
        if (this.timer_id !== -1) {
            clearTimeout(this.timer_id);
            this.timer_id = -1;
        }
        if(msec !== -1) {
            this.timer_id = setTimeout(
                function() {
                    gobj.timer_id = -1;
                    gobj.inject_event(gobj.timer_event_name);
                },
                msec
            );
        }
    };
    proto.clear_timeout = function()
    {
        if (this.timer_id !== -1) {
            clearTimeout(this.timer_id);
            this.timer_id = -1;
        }
    };

    /************************************************************
     *      send_event
     ************************************************************/
    proto.gobj_send_event = function(event, kw, src)
    {
        if(!src) {
            // Let events without src, from yuneta outside world.
            //log_error("gobj_send_event('" + event + "') with no src");
        }
        if(!kw) {
            kw = {};
        }
        if(this.mt_inject_event) {
            if(!this.gobj_event_in_input_event_list(event)) {
                let tracing = this.is_tracing();
                if(tracing) {
                    let hora = get_current_datetime();
                    let msg = sprintf("%s%s+> mach: %s, st: %s, ev: %s, src: %s",
                        hora,
                        this._tab(),
                        this.gobj_short_name(),
                        this.fsm.state_list[this.fsm.current_state-1],
                        event,
                        src?src.gobj_short_name():"undefined"
                    );
                    log_debug(msg);
                    if(tracing > 1) {
                        trace_msg(kw);
                    }
                }

                return this.mt_inject_event(event, kw, src);
            }
        }

        return this.inject_event(event, kw, src);
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_event_in_input_event_list = function(event)
    {
        let input_events = this.get_input_event_list();

        return elm_in_list(event, input_events);
    };

    /************************************************************
     *  only to childs supporting the event
     ************************************************************/
    proto.gobj_send_event_to_childs = function(event, kw, src)
    {
        let set = this.dl_childs;
        for(let i=0; i<set.length; i++) {
            let child = set[i];
            if(child) {
                if(child.gobj_event_in_input_event_list(event)) {
                    child.gobj_send_event(event, kw, src);
                }
            }
        }
    };

    /************************************************************
     *      _Subscription class.
     ************************************************************/
    function _Subscription(publisher, event, kw, subscriber) {
        this.publisher = publisher;
        this.subscriber = subscriber;
        this.event = event;
        this.renamed_event = null;
        this.hard_subscription = false;
        this.own_event = false;
        this.share_kw = false;
        this.__config__ = null;
        this.__global__ = null;
        this.__filter__ = null;
        this.__service__ = null;

        if(kw) {
            let __global__ = kw["__global__"];
            let __config__ = kw["__config__"];
            let __filter__ = kw["__filter__"];
            let __service__ = kw["__service__"];

            if(__global__) {
                this.__global__ = __duplicate__(__global__);
            }
            if(__config__) {
                this.__config__ = __duplicate__(__config__);
                if(kw_has_key(this.__config__, "__rename_event_name__")) {
                    let renamed_event = kw_get_str(this.__config__, "__rename_event_name__", 0);
                    this.renamed_event = renamed_event;
                    delete this.__config__["__rename_event_name__"];

                    // Get/Create __global__
                    let kw_global = this.__global__;
                    if(!kw_global) {
                        kw_global = {};
                        this.__global__ = kw_global;
                    }
                    kw_global["__original_event_name__"] = event;
                }
                if(kw_has_key(this.__config__, "__hard_subscription__")) {
                    this.hard_subscription = kw_get_bool(this.__config__, "__hard_subscription__", 0);
                    delete this.__config__["__hard_subscription__"];
                }
                if(kw_has_key(this.__config__, "__own_event__")) {
                    this.own_event = kw_get_bool(this.__config__, "__own_event__", 0);
                    delete this.__config__["__own_event__"];
                }
                if(kw_has_key(this.__config__, "__share_kw__")) {
                    this.share_kw = kw_get_bool(this.__config__, "__share_kw__", 0);
                    delete this.__config__["__share_kw__"];
                }
            }
            if(__filter__) {
                this.__filter__ = __duplicate__(__filter__);
            }
            if(__service__) {
                this.__service__ = __service__;
            }
        }
    }

    /************************************************************
     *  Return a iter of subscriptions (sdata),
     *  filtering by matching:
     *      event,kw (__config__, __global__, __filter__),subscriber
     ************************************************************/
    function _find_subscription(dl_subs, publisher, event, kw, subscriber)
    {
        let sub_list = [];

        let __global__ = null;
        let __config__ = null;
        let __filter__ = null;
        if(kw) {
            __global__ = kw["__global__"];
            __config__ = kw["__config__"];
            __filter__ = kw["__filter__"];
        }

        for(let i=0; i<dl_subs.length; i++) {
            let subs = dl_subs[i];
            let match = true;

            if(subscriber) {
                if(subscriber != subs.subscriber) {
                    match = false;
                }
            }

            if(event) {
                if(event != subs.event) {
                    match = false;
                }
            }

            if(__config__) {
                let kw_config = subs["__config__"];
                if(kw_config) {
                    if(!kw_is_identical(kw_config, __config__)) {
                        match = false;
                    }
                } else {
                    match = false;
                }
            }
            if(__global__) {
                let kw_global = subs["__global__"];
                if(kw_global) {
                    if(!kw_is_identical(kw_global, __global__)) {
                        match = false;
                    }
                } else {
                    match = false;
                }
            }
            if(__filter__) {
                let kw_filter = subs["__filter__"];
                if(kw_filter) {
                    if(!kw_is_identical(kw_filter, __filter__)) {
                        match = false;
                    }
                } else {
                    match = false;
                }
            }

            if(match) {
                sub_list.push(subs);
            }
        }
        return sub_list;
    }

    /************************************************************
     *  Return a iter of subscriptions (sdata),
     *  filtering by matching:
     *      event,kw (__config__, __global__, __filter__),subscriber
     ************************************************************/
    proto.gobj_find_subscriptions = function(event, kw, subscriber)
    {
        return _find_subscription(this.dl_subscriptions, this, event, kw, subscriber);
    };

    /************************************************************
     *  Delete subscription
     ************************************************************/
    proto._delete_subscription = function(subs, force)
    {
        if(subs.hard_subscription) {
            if(!force) {
                return;
            }
        }

        /*-----------------------------*
         *  Trace
         *-----------------------------*/
        // TODO tracea subs

        /*
         *  Avisa de la unsubscription si quieren
         */
        if(this.mt_subscription_deleted) {
            this.mt_subscription_deleted(subs);
        }
        delete_from_list(this.dl_subscriptions, subs);
    };

    /************************************************************
     *      subscribe_event
     *  event can be string or string's list.
     *
     *  Possible values for **kw** arguments:
            __config__,
            __global__,
            __filter__
     *
     ************************************************************/
    proto.gobj_subscribe_event = function(event, kw, subscriber)
    {
        /*
         *  Check subscriber
         */
        if(!subscriber) {
            log_error("GObj.gobj_subscribe_event(): subscriber NULL");
            return;
        }

        /*
         *  Si el subscriber es un string, búsca su object
         */
        if (!(typeof subscriber === 'string' || subscriber instanceof GObj)) {
            log_error("GObj.gobj_subscribe_event(): BAD TYPE subscriber");
        }

        if (typeof subscriber === 'string') {
            let new_subscriber = this.yuno.gobj_find_unique_gobj(subscriber);
            if (!new_subscriber) {
                log_error("GObj.gobj_subscribe_event(): '" + subscriber + "' gobj NOT FOUND");
                return;
            }
            subscriber = new_subscriber;
        } else if(subscriber instanceof GObj) {
        } else {
            log_error("GObj.gobj_subscribe_event(): BAD TYPE subscriber");
            return;
        }

        /*
         *  Comprueba que el evento está declarado en output_event_list,
         *  solo para warning, no cortes el procedimiento.
         */
        let output_events = this.get_output_event_list();
        /*
         *  Event can be null or undefined
         */
        if(event) {
            if (typeof event !== 'string') {
                let msg = "GObj.gobj_subscribe_event('" +
                    this.gobj_short_name() +
                    "') from " + subscriber.gobj_short_name() + ": '" +
                    event + "' is not a string";
                log_error(msg);
                return 0;
            }

            if(!(this.gcflag & gcflag_no_check_output_events)) {
                if (!elm_in_list(event, output_events)) {
                    let msg = "GObj.gobj_subscribe_event('" +
                        this.gobj_short_name() +
                        "') from " + subscriber.gobj_short_name() + ": '" +
                        event + "' not in output-event list";
                    log_error(msg);
                    return 0;
                }
            }
        }

        /*------------------------------*
         *  Find repeated subscription
         *------------------------------*/
        let dl_subs = this.gobj_find_subscriptions(event, kw, subscriber);
        if(dl_subs.length > 0) {
            log_error(sprintf(
                "subscription(s) REPEATED, event %s, publisher %s, subscriber %s: NEW IGNORED",
                event,
                this.gobj_short_name(),
                subscriber.gobj_short_name()
            ));
            return 0;
        }


        /*
         *  Crea una instancia de subscription
         */
        let subscription = new _Subscription(this, event, kw, subscriber);
        this.dl_subscriptions.push(subscription);

        /*-----------------------------*
         *  Trace
         *-----------------------------*/
        // TODO tracea subs
        // trace_msg("subscribe list " + this.gobj_short_name());
        // trace_msg(this.gobj_print_subscriptions());

        /*
         *  Avisa de la nueva subscription si quieren
         */
        if(this.mt_subscription_added) {
            this.mt_subscription_added(subscription);
        }

        return subscription;
    };

    /************************************************************
     *  Delete subscription by name and subscriber gobj
     *  event must be the original string or string's list.
     ************************************************************/
    proto.gobj_unsubscribe_event = function(event, kw, subscriber)
    {
        let sub_list = this.gobj_find_subscriptions(event, kw, subscriber);

        if(sub_list.length) {
            for (let i=0; i<sub_list.length; i++) {
                this._delete_subscription(sub_list[i], false);
            }

        } else {
            log_error(
                sprintf("%s: gobj_unsubscribe_event(): event '%s', subscriber %s, NOT FOUND",
                    this.gobj_short_name(),
                    event,
                    subscriber.gobj_short_name()
                )
            );
            return -1;
        }
        return 0;
    };

    /************************************************************
     *  Debug: return list of subscriptions
     ************************************************************/
    proto.gobj_print_subscriptions = function()
    {
        let lista = [];
        let subscriptions = this.dl_subscriptions;
        let len = subscriptions.length;

        for(let i=0; i<len; i++) {
            let subs = subscriptions[i];
            if (!subs) {
                continue;
            }
            let item = {
                publisher: subs.publisher.gobj_short_name(),
                subscriber: subs.subscriber.gobj_short_name(),
                event: subs.event,
                renamed_event: subs.renamed_event,
                hard_subscription: subs.hard_subscription,
                own_event: subs.own_event,
                __config__: __duplicate__(subs.__config__),
                __global__: __duplicate__(subs.__global__),
                __filter__: __duplicate__(subs.__filter__),
                __service__: __duplicate__(subs.__service__)

            };
            lista.push(item);
        }
        return lista;
    };

    /************************************************************
     *  Delete subscription by handler
     *  (handler returned by subscribe_event()
     ************************************************************/
    proto.gobj_unsubscribe_event2 = function(hsub)
    {
        return this._delete_subscription(hsub, false);
    };

    /************************************************************
     *  Delete subscription list
     *  (handler returned by gobj_find_subscriptions)
     ************************************************************/
    proto.gobj_unsubscribe_list = function(dl_subs, force)
    {
        if(dl_subs.length) {
            for (let i=0; i<dl_subs.length; i++) {
                this._delete_subscription(dl_subs[i], force);
            }
        }
    };

    /************************************************************
     *      publish_event
     *      Return the number of sent events
     ************************************************************/
    proto.gobj_publish_event = function(event, kw)
    {
        let sent_count = 0;

        if(!kw) {
            kw = {};
        }
        if(empty_string(event)) {
            let msg = sprintf("GObj.gobj_publish_event('%s'): event NULL", this.gobj_short_name());
            log_error(msg);
            return 0;
        }

        /*
         *  Chequea que el evento existe en la output_event_list
         */
        let output_events = this.get_output_event_list();
        if(!(this.gcflag & gcflag_no_check_output_events)) {
            if (!elm_in_list(event, output_events)) {
                let msg = sprintf("GObj.gobj_publish_event('%s'): event '%s' not in output-event list",
                    this.gobj_short_name(),
                    event
                );
                log_error(msg);
                return 0;
            }
        }

        let tracing = this.is_tracing(event);
        if (tracing) {
            let hora = get_current_datetime();
            let msg = sprintf("%s%s**> mach: %s, st: %s, ev: %s",
                hora,
                this._tab(),
                this.gobj_short_name(),
                this.fsm.state_list[this.fsm.current_state-1],
                event
            );
            log_debug(msg);
            if(tracing > 1) {
                trace_msg(kw);
            }
        }

        /*-------------------------------------*
         *  Own publication method
         *  Return:
         *     -1  (broke),
         *      0  continue without publish,
         *      1  continue and publish
         *-------------------------------------*/
        if(this.mt_publish_event) {
            let topublish = this.mt_publish_event(event, kw);
            if(topublish<=0) {
                return 0;
            }
        }

        /*--------------------------------------------------------------*
         *  Default publication method
         *--------------------------------------------------------------*/
        let global_kw_shared = kw_get_bool(kw, "__share_kw__", false);
        let subscriptions = this.dl_subscriptions;
        let len = subscriptions.length;
        for(let i=0; i<len; i++) {
            let subs = subscriptions[i];
            if(!subs) {
                continue;
            }
            if(this.mt_publication_pre_filter) {
                let topublish = this.mt_publication_pre_filter(subs, event, kw);
                if(topublish<0) {
                    break;
                } else if(topublish==0) {
                    continue;
                }
            }
            let subscriber = subs.subscriber;

            /*
             *  Check if event null or event in event_list
             */
            if (subs.event===null ||subs.event===undefined || event === subs.event) {
                let kw2publish = null;
                let __global__ = subs.__global__;
                let __filter__ = subs.__filter__;

                /*
                 *  Check renamed_event
                 */
                let event_name = subs.renamed_event;
                if (empty_string(event_name)) {
                    event_name = event;
                }

                /*
                 *  If kw_global exists then clone and update over it the kw content
                 *  (__extend_dict__): add new keys and overwrite existing keys.
                 */
                if(__global__) {
                    kw2publish = __duplicate__(__global__);
                    __extend_dict__(kw2publish, kw);
                } else {
                    if(global_kw_shared || subs.share_kw) {
                        kw2publish = kw;
                    } else {
                        kw2publish = __duplicate__(kw); // Native js objects don't duplicate well
                    }
                }

                /*
                 *  User filter or configured filter
                 */
                let topublish = 1;
                if(this.mt_publication_filter) {
                    topublish = this.mt_publication_filter(
                        this,
                        event,
                        kw2publish,  // not owned
                        subscriber
                    );
                } else if(__filter__) {
                    topublish = kw_match_simple(kw2publish , __filter__);
                }
                if(topublish<0) {
                    break;
                } else if(topublish==0) {
                    /*
                     *  Must not be published
                     *  Next subs
                     */
                    continue;
                }

                /*
                 *  Send event
                 */
                let ret = subscriber.gobj_send_event(
                    event_name,
                    kw2publish,
                    this
                );
                if(ret < 0 && subs.own_event) {
                    sent_count = -1; // Return of -1 indicates that someone owned the event
                    break;
                }
                sent_count++;

                if(this._destroyed) {
                    /*
                     *  break all, self publisher deleted
                     */
                    break;
                }
            }
        }

        if(!sent_count) {
            let event_id = this.fsm.event_index[event] || 0;
            let attrs = this.fsm.event_attrs[event_id-1] || {};
            if(!elm_in_list("no_warn_subs", attrs)) {
                if(!this._destroyed) {
                    log_warning(
                        "Publish event WITHOUT subscribers: " +
                        this.gobj_short_name() + ", " + event
                    );
                }
            }
        }
        return sent_count;
    };


    /************************************************************
     *      get parent
     ************************************************************/
    proto.gobj_parent = function()
    {
        return this.parent;
    };

    /************************************************************
     *      Get full name
     ************************************************************/
    proto.gobj_full_name = function()
    {
        let self=this;

        let full_name = self.gobj_short_name();
        let parent = self.parent;
        while(parent) {
            let prefix = parent.gobj_short_name();
            full_name = prefix + '`' + full_name;
            parent = parent.parent;
        }
        return full_name;
    };

    /************************************************************
     *      get short name (gclass^name)
     ************************************************************/
    proto.gobj_short_name = function()
    {
        return this.gclass_name + '^' + this.name;
    };

    /************************************************************
     *      get escape short name (gclass-name)
     ************************************************************/
    proto.gobj_escaped_short_name = function()
    {
        return this.gclass_name + '-' + this.name;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_gclass_name = function()
    {
        return this.gclass_name;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_name = function()
    {
        return this.name;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_is_destroying = function()
    {
        if(this._destroyed) {
            return true;
        }
        return false;
    };

    /************************************************************
     *          Automata create
     ************************************************************/
    proto.fsm_create = function(fsm_desc)
    {
        let self = this.fsm = {};
        self.event_list = fsm_desc.event_list || [];
        self.event_attrs = [];
        self.state_list = fsm_desc.state_list || [];
        self.states = [];
        self.last_state = 0;
        self.current_state = 1;

        // check state names
        let state_names = __duplicate__(self.state_list); // dup list
        for (let st_name in fsm_desc.machine) {
            if (elm_in_list(st_name, state_names)) {
                delete_from_list(state_names, st_name);
            } else {
                log_error(
                    this.gclass_name + ": machine state: " + st_name + " is NOT in state-list"
                );
            }
        }
        if (state_names.length > 0) {
            log_error(this.gclass_name + ": state-list OVERFILLED: " + state_names);
        }

        // remove attributes from event_list and move attrs to _event_attr list
        let event_list = [];
        let event_attrs = [];
        for (let i=0; i<self.event_list.length; i++) {
            let ev = self.event_list[i];
            let name = ev.split(":");
            let ev_name = name[0];
            ev_name = __strip__(ev_name);
            event_list.push(ev_name);

            let ev_attrs = name[1];
            ev_attrs = __strip__(ev_attrs);
            let attrs_list = ev_attrs.split(" "); // TODO
            event_attrs.push(attrs_list);
        }
        self.event_list = event_list;
        self.event_attrs = event_attrs;

        // build _output_events from event attributes
        let output_events = []; //__set__()
        for(let idx=0; idx < self.event_list.length; idx++) {
            let ev_name = self.event_list[idx];
            let attrs = self.event_attrs[idx];
            if (elm_in_list("output", attrs)) {
                output_events.push(ev_name);
            }
        }
        self.output_events = __set__(output_events);

        // check event names
        let event_names = __duplicate__(self.event_list);
        let set_event_names = __duplicate__(self.output_events);  // start with output_events!
        for (let st in fsm_desc.machine) {
            if (fsm_desc.machine.hasOwnProperty(st)) {
                let st_desc = fsm_desc.machine[st];
                let len = st_desc.length;
                for (let idx=0; idx < len; idx++) {
                    let ev_ac_nt = st_desc[idx];
                    if (!ev_ac_nt) {
                        // In IE the last comma in a list [] include a new undefined element.
                        continue;
                    }
                    let ev_name = ev_ac_nt[0];
                    let ac = ev_ac_nt[1];
                    let nt = ev_ac_nt[2];

                    if (elm_in_list(ev_name, event_names)) {
                        set_event_names.push(ev_name);
                    } else {
                        log_error(this.gclass_name + ": event NOT in event-list: " + ev_name);
                    }
                }
              }
        }
        set_event_names = __set__(set_event_names);
        if (event_names.length !== set_event_names.length) {
            let diff = get_str_list_difference(event_names, set_event_names);
            log_error(sprintf("%s: event-list OVERFILLED: %j", this.gclass_name, diff));
        }

        // check next state names and actions
        state_names = __duplicate__(self.state_list);
        for (let st in fsm_desc.machine) {
            if (fsm_desc.machine.hasOwnProperty(st)) {
                let st_desc = fsm_desc.machine[st];
                let len = st_desc.length;
                for (let idx=0; idx < len; idx++) {
                    let ev_ac_nt = st_desc[idx];
                    if (!ev_ac_nt) {
                        // In IE the last comma in a list [] include a new undefined element.
                        continue;
                    }
                    let ev_name = ev_ac_nt[0];
                    let ac = ev_ac_nt[1];
                    let nt = ev_ac_nt[2];

                    if (nt && !elm_in_list(nt, state_names)) {
                        log_error(this.gclass_name + ": next statename: "+ nt + " is NOT in state-list");
                    }
                    if(ac && typeof ac !== 'function') {
                        log_error(this.gclass_name + ": action: "+ ac + " is NOT a FUNCTION");
                    }
                }
            }
        }

        // Build constant names (like C enum) for states: dict of name:id
        self.state_index = {'': 0};
        for(let i=0; i<self.state_list.length; i++) {
            let elm = self.state_list[i];
            self.state_index[elm] = i+1;
        }

        // Build constant names (like C enum) for events: dict of name:id
        self.event_index = {'': 0};
        for(let i=0; i<self.event_list.length; i++) {
            let elm = self.event_list[i];
            self.event_index[elm] = i+1;
        }

        /*
        #   Build list of states
        #   self._states is organized as:
        #
        #   [0]
        #       [0] [1] [2]... [n.events-1]
        #   [1]
        #       [0] [1] [2]... [n.events-1]
        #   [2]
        #       [0] [1] [2]... [n.events-1]
        #   ...
        #   [n.states-1]
        #       [0] [1] [2]... [n.events-1]
        #            |
        #            `--> [action, next_state]
        #
        #
        #   If a event is defined in a state, then,
        #   the element is a list([action,next_state]) instead of int.
        #
        #   This organization occupies more memory than necessary,
        #   but the execution is faster.
        #
        */

        self.states = new Array(self.state_list.length+1);
        for(let i=0; i<self.state_list.length; i++) {
            let st = self.state_list[i];
            let st_idx = self.state_index[st];
            let st_desc = fsm_desc.machine[st];
            self.states[st_idx] = new Array(self.event_list.length + 1);

            for(let j=0; j<st_desc.length; j++) {
                let ev_ac_nt = st_desc[j];
                if (!ev_ac_nt) {
                    // In IE the last comma in a list [] include a new undefined element.
                    continue;
                }

                let iev = self.event_index[ev_ac_nt[0]];
                // Get the action
                let ac = ev_ac_nt[1];
                // Save the next state
                let next_state_id;
                if (ev_ac_nt[2]) {
                    next_state_id = self.state_index[ev_ac_nt[2]];
                } else {
                    next_state_id = undefined;
                }
                // Save action/next-state
                self.states[st_idx][iev] = [ac, next_state_id];
            }
        }
    };

    /************************************************************
     *        State functions
     ************************************************************/
    proto.gobj_in_this_state = function(state)
    {
        if(this.gobj_current_state() == state) {
            return true;
        }
        return false;
    };

    /************************************************************
     *        public change_state
     ************************************************************/
    proto.gobj_change_state = function(new_state)
    {
        let state_id = this.fsm.state_index[new_state] || 0;
        if (state_id <= 0) {
            log_error(this.gclass_name + ": change_state() state UNKNOWN: " + new_state);
            return;
        }
        this._change_state(state_id);
    };

    /************************************************************
     *        private change_state
     ************************************************************/
    proto._change_state = function(state_id)
    {
        let fsm = this.fsm;

        if (state_id <= 0 || state_id > fsm.state_list.length) {
            log_error(this.gclass_name + ": _change_state() state_id INVALID " + state_id);
        }
        let tracing = this.is_tracing();
        fsm.last_state = fsm.current_state;
        fsm.current_state = state_id;
        if (tracing) {
            if (fsm.last_state !== state_id) {
                let hora = get_current_datetime();
                let msg = sprintf("%s%s - mach: %s, new_st: %s",
                    hora,
                    this._tab(),
                    this.gobj_short_name(),
                    this.fsm.state_list[this.fsm.current_state-1]
                );
                log_debug(msg);
            }
        }
    };

    /************************************************************
     *        Return the name of the current state.
     ************************************************************/
    proto.gobj_current_state = function()
    {
        let fsm = this.fsm;
        if (fsm.current_state <= 0 || fsm.state_list.length == 0) {
            return null;
        }
        return fsm.state_list[fsm.current_state - 1];
    };

    /************************************************************
     *        Inject event.
     ************************************************************/
    proto.inject_event = function(event, kw, src)
    {
        let fsm = this.fsm;
        let result = -1;
        let action = null;

        if (typeof event !== 'string') {
            log_error("inject_event() invalid event TYPE");
            return -1;  //# EventNotAcceptedError
        }

        let tracing = this.is_tracing(event);
        let hora = null;
        if(tracing) {
            hora = get_current_datetime();
        }

        let event_id = fsm.event_index[event] || 0;
        if (event_id <= 0) {
            let msg = sprintf("EVENT UNKNOWN -> mach: %s, st: %s, ev: %s",
                this.gobj_short_name(),
                this.fsm.state_list[this.fsm.current_state-1],
                event
            );
            log_error(msg);
            if(tracing > 1) {
                trace_msg(kw);
            }
            return -1;  //# EventNotAcceptedError
        }

        this._increase_inside();

        if (fsm.states) {
            action = fsm.states[fsm.current_state][event_id];
        }

        if (!action) {
            let msg = sprintf("%s<> mach: %s, st: %s, ev: %s, src: %s (REFUSED, no match action)",
                hora,
                this.gobj_short_name(),
                this.fsm.state_list[this.fsm.current_state-1],
                event,
                src?src.gobj_short_name():"undefined"
            );
            log_warning(msg);
            if(tracing > 1) {
                trace_msg(kw);
            }
            this._decrease_inside();
            return -1;  //# EventNotAcceptedError
        }

        if (tracing) {
            let action_name = "";
            if (action[0]) {
                action_name = get_function_name(action[0]);
            }
            let msg = sprintf("%s%s-> mach: %s, st: %s, ev: %s, ac: %s, src: %s",
                hora,
                this._tab(),
                this.gobj_short_name(),
                this.fsm.state_list[this.fsm.current_state-1],
                event,
                action_name,
                src?src.gobj_short_name():"undefined"
            );
            log_debug(msg);
            if(tracing > 1) {
                trace_msg(kw);
            }
        }

        if (action[1]) {
            this._change_state(action[1]);
        }

        if(tracing) {
            // In develop is better not to catch the interrupt for have more info.
            if (action[0]) {
                //# Action found, execute
                result = action[0](this, event, kw, src);
            }
        } else {
            try {
                if (action[0]) {
                    //# Action found, execute
                    result = action[0](this, event, kw, src);
                }
            } catch (e) {
                log_error(e);
            }
        }

        if (tracing) {
            let msg = sprintf("%s%s<- mach: %s, st: %s, ev: %s, ret: %d",
                hora,
                this._tab(),
                this.gobj_short_name(),
                this.fsm.state_list[this.fsm.current_state-1],
                event,
                Number(result)
            );
            log_debug(msg);
        }

        this._decrease_inside();
        return result;
    };

    /************************************************************
     *        get event lists.
     ************************************************************/
    proto.get_input_event_list = function()
    {
        return this.fsm.event_list || [];
    };

    proto.get_output_event_list = function()
    {
        return this.fsm.output_events || [];
    };

    /************************************************************
     *        return current indent.
     ************************************************************/
    proto.current_state = function()
    {
        if (this.fsm.current_state <= 0 || this.fsm.state_list.length === 0) {
            return undefined;
        }
        return this.fsm.state_list[this.fsm.current_state - 1];
    };

    /************************************************************
     *        indentation functions.
     ************************************************************/
    proto._tab = function()
    {
        let spaces, pad;
        if (__inside_event_loop__ <= 0) {
            spaces = 1;
        } else {
            spaces = __inside_event_loop__ * 2;
        }
        pad = '';
        while (spaces--) {
            pad += ' ';
        }
        return pad;
    };

    proto._increase_inside = function()
    {
        __inside_event_loop__ += 1;
    };
    proto._decrease_inside = function()
    {
        __inside_event_loop__ -= 1;
    };

    let gcflag_manual_start = 0x0001;   // gobj_start_tree() don't start gobjs of this gclass.
    let gcflag_no_check_output_events = 0x0002;   // When publishing don't check events in output_event_list.


    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.GObj = GObj;
    exports.get_current_datetime = get_current_datetime;
    exports.gcflag_manual_start = gcflag_manual_start;
    exports.gcflag_no_check_output_events = gcflag_no_check_output_events;
})(this);
