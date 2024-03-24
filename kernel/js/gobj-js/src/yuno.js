/*********************************************************************************
 *  The Default Yuno
 *
 *  Author: Niyamaka
 *  Email: Niyamaka at yuneta.io
 *  Licence: MIT (http://www.opensource.org/licenses/mit-license)
 *
 *  Last revision:
 *      20 Junio 2014 - Upgraded to yuneta api.
 *                      changes in send_inter_event.
 *
 *      15 Julio 2015 - Upgraded to yuneta 1.0.0.
 *********************************************************************************/

/**************************************************************************
 *        Yuno
 **************************************************************************/
(function (exports) {
    "use strict";

    /************************************************************
     *      Global system of yunos
     ************************************************************/
    var __jn_global_settings__ =  null;
    var __global_load_persistent_attrs_fn__ = null;
    var __global_save_persistent_attrs_fn__ = null;
    var __global_remove_persistent_attrs_fn__ = null;
    var __global_list_persistent_attrs_fn__ = null;
    var __global_command_parser_fn__ = null;
    var __global_stats_parser_fn__ = null;

    /************************************************************
     *      Yuno class.
     ************************************************************/
    var CONFIG = {
        changesLost: false, // Use with window.onbeforeunload in __yuno__.
                            // Set true to warning about leaving page.
        tracing: 0,
        no_poll: 0,
        trace_timer: 0,
        trace_inter_event: false,
        trace_creation: false,
        trace_ievent_callback: null
    };
    var FSM = {
        'event_list': [
            'EV_TIMEOUT: top output'
        ],
        'state_list': ['ST_IDLE'],
        'machine': {
            'ST_IDLE':
            [
                ['EV_TIMEOUT', null, 'ST_IDLE']
            ]
        }
    };
    var _gclass_register = {};

    var Yuno = GObj.__makeSubclass__();
    var proto = Yuno.prototype; // Easy access to the prototype
    proto.__init__= function(yuno_name, yuno_role, yuno_version, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            yuno_name,
            "Yuno",
            kw,
            0
        );
        this.yuno_role = yuno_role;
        this.yuno_name = yuno_name;
        this.yuno_version = yuno_version;
        this.config.yuno_role = yuno_role;
        this.config.yuno_name = yuno_name;
        this.config.yuno_version = yuno_version;
        this.__service__ = false;
        this.__unique__ = false;
        this.__volatil__ = false;
        this.parent = null;
        this.yuno = this;
        this._inside = 0;
        this._unique_gobjs = {};
        this._service_gobjs = {};

        this.__jn_global_settings__ =  __jn_global_settings__;
        this.__global_load_persistent_attrs_fn__ = __global_load_persistent_attrs_fn__;
        this.__global_save_persistent_attrs_fn__ = __global_save_persistent_attrs_fn__;
        this.__global_remove_persistent_attrs_fn__ = __global_remove_persistent_attrs_fn__;
        this.__global_list_persistent_attrs_fn__ = __global_list_persistent_attrs_fn__;
        this.__global_command_parser_fn__ = null;
        this.__global_stats_parser_fn__ = null;

        this.mt_create(kw);   // auto-create
    };

    proto.mt_create = function(kw)
    {
        /**********************************
         *          Start up
         **********************************/
    };

    /************************************************************
     *      Start up
     ************************************************************/
    function gobj_start_up(
        jn_global_settings,
        load_persistent_attrs_fn,
        save_persistent_attrs_fn,
        remove_persistent_attrs_fn,
        list_persistent_attrs_fn,
        global_command_parser_fn,
        global_stats_parser_fn
    )
    {
        __jn_global_settings__ =  0; // TODO kw_apply_json_config_variables(jn_global_settings, 0);
        __global_load_persistent_attrs_fn__ = load_persistent_attrs_fn;
        __global_save_persistent_attrs_fn__ = save_persistent_attrs_fn;
        __global_remove_persistent_attrs_fn__ = remove_persistent_attrs_fn;
        __global_list_persistent_attrs_fn__ = list_persistent_attrs_fn;
        __global_command_parser_fn__ = global_command_parser_fn;
        __global_stats_parser_fn__ = global_stats_parser_fn;

        return 0;
    }

    /************************************************************
     *      Register gclass
     ************************************************************/
    function gobj_register_gclass(gclass, gclass_name)
    {
        if(!gclass || !gclass_name) {
            let msg = "Yuno.gobj_register_gclass(): gclass undefined";
            log_error(msg);
            return -1;
        }
        let gclass_ = _gclass_register[gclass_name];
        if (gclass_) {
            let msg = "Yuno.gobj_register_gclass(): '" +
                gclass_name +
                "' ALREADY REGISTERED";
            log_error(msg);
            return -1;
        }
        _gclass_register[gclass_name] = gclass;
        return 0;
    }

    /************************************************************
     *      Find gclass
     ************************************************************/
    function gobj_find_gclass(gclass_name, verbose)
    {
        let gclass;
        try {
            gclass = _gclass_register[gclass_name];
        } catch (e) {
            if(verbose) {
                log_error("Yuno.gobj_find_gclass(): '" + gclass_name + "' gclass not found");
            }
            return null;
        }
        return gclass;
    }

    /************************************************************
     *  Example how change CONFIG of a gclass (temporarily)
     *
            let CONFIG = gobj_get_gclass_config("Ka_scrollview", true);
            let old_dragging = CONFIG.draggable;
            CONFIG.draggable = true;

            let gobj_ka_scrollview = self.yuno.gobj_create(
                "xxx",
                Ka_scrollview,
                {
                    ...
                },
                self // this will provoke EV_SHOWED,EV_KEYDOWN
            );

            CONFIG.draggable = old_dragging;

     ************************************************************/
    function gobj_get_gclass_config(gclass_name, verbose)
    {
        let gclass = gobj_find_gclass(gclass_name, verbose);
        if(gclass && gclass.prototype.mt_get_gclass_config) {
            return gclass.prototype.mt_get_gclass_config.call();
        } else {
            if(verbose) {
                log_error(sprintf(
                    "gobj_get_gclass_config: '%s' gclass without mt_get_gclass_config", gclass_name
                ));
            }
            return null;
        }
    }


    /************************************************************
     *
     ************************************************************/
    function gobj_list_persistent_attrs()
    {
        // TODO
        if(!__global_list_persistent_attrs_fn__) {
            return null;
        }
        return __global_list_persistent_attrs_fn__();
    }

    /************************************************************
     *      gobj_create factory.
     ************************************************************/
    proto._gobj_create = function(name, gclass, kw, parent, is_service, is_unique, is_volatil)
    {
        if(is_string(gclass)) {
            let gclass_ = gclass;
            gclass = gobj_find_gclass(gclass_, false);
            if(!gclass) {
                log_error("GClass not found: '" + gclass_ +"'");
                return null;
            }
        }

        if(!empty_string(name)) {
            /*
             *  Check that the name: cannot contain `
             */
            if(name.indexOf("`")>=0) {
                log_error("GObj name cannot contain \"`\" char: '" + name + "'");
                return null;
            }
            /*
             *  Check that the name: cannot contain ^
             */
            if(name.indexOf("^")>=0) {
                log_error("GObj name cannot contain \"^\" char: '" + name + "'");
                return null;
            }
        } else {
            /*
             *  To facility the work with jquery, I generate all gobjs as named gobjs.
             *  If a gobj has no name, generate a unique name with uniqued_id.
             */
            // force all gobj to have a name.
            // useful to make DOM elements with id depending of his gobj.
            // WARNING danger change, 13/Ago/2020, now anonymous gobjs in js
            name = ""; // get_unique_id('gobj');
        }

        if (!(typeof parent === 'string' || parent instanceof GObj)) {
            log_error("Yuno.gobj_create() BAD TYPE of parent: " + parent);
            return null;
        }

        if (typeof parent === 'string') {
            // find the named gobj
            parent = this.gobj_find_unique_gobj(parent);
            if (!parent) {
                let msg = "Yuno.gobj_create('" + name + "'): " +
                    "WITHOUT registered named PARENT: '" + parent + "'";
                log_warning(msg);
                return null;
            }
        }

        let gobj = new gclass(name, kw);
        gobj.yuno = this;

        if (this.config.trace_creation) {
            let gclass_name = gobj.gclass_name || '';
            log_debug(sprintf("💙💙⏩ creating: %s%s %s^%s",
                is_service?"service":"", is_unique?"unique":"", gclass_name, name
            ));
        }

        if(!gobj.gobj_load_persistent_attrs) {
            let msg = "Check GClass of '" + name + "': don't look a GClass";
            log_error(msg);
            return null;
        }
        if(name) {
            // All js gobjs are unique-named!
            // WARNING danger change, 13/Ago/2020, now anonymous gobjs in js
            //if(!this._register_unique_gobj(gobj)) {
            //    return null;
            //}
        }

        if(is_unique) {
            if(!this._register_unique_gobj(gobj)) {
               return null;
            }
            gobj.__unique__ = true;
        } else {
            gobj.__unique__ = false;
        }
        if(is_service) {
            if(!this._register_service_gobj(gobj)) {
               return null;
            }
            gobj.__service__ = true;
        } else {
            gobj.__service__ = false;
        }
        if(is_service || is_unique) {
            gobj.gobj_load_persistent_attrs();
        }
        if(is_volatil) {
            gobj.__volatil__ = true;
        } else {
            gobj.__volatil__ = false;
        }

        /*--------------------------------------*
         *      Add to parent
         *--------------------------------------*/
        if(parent) {
            parent._add_child(gobj);
        }

        /*--------------------------------*
         *      Exec mt_create
         *--------------------------------*/
        if(gobj.mt_create) {
            gobj.mt_create(kw);
        }

        /*--------------------------------------*
         *  Inform to parent
         *  when the child is full operative
         *-------------------------------------*/
        if(parent && parent.mt_child_added) {
            if (this.config.trace_creation) {
                log_debug(sprintf("👦👦🔵 child_added(%s): %s", parent.gobj_full_name(), gobj.gobj_short_name()));
            }
            parent.mt_child_added(gobj);
        }

        if (this.config.trace_creation) {
            log_debug("💙💙⏪ created: " + gobj.gobj_full_name());
        }

        return gobj;
    };

    /************************************************************
     *      gobj_create factory.
     ************************************************************/
    proto.gobj_create = function(name, gclass, kw, parent)
    {
        return this._gobj_create(
            name,
            gclass,
            kw,
            parent,
            false,
            false,
            false
        );
    };

    /************************************************************
     *      gobj_create factory.
     ************************************************************/
    proto.gobj_create_unique = function(name, gclass, kw, parent)
    {
        if(this._exist_unique_gobj(name)) {
            var msg = "GObj unique ALREADY exits: " + name;
            log_error(msg);
            return null;
        }

        return this._gobj_create(
            name,
            gclass,
            kw,
            parent,
            false,
            true,
            false
        );
    };

    /************************************************************
     *      gobj_create factory.
     ************************************************************/
    proto.gobj_create_service = function(name, gclass, kw, parent)
    {
        if(this._exist_service_gobj(name)) {
            var msg = "GObj service ALREADY exists: " + name;
            log_error(msg);
            return null;
        }

        return this._gobj_create(
            name,
            gclass,
            kw,
            parent,
            true,
            false,
            false
        );
    };

    /************************************************************
     *      gobj_create factory.
     ************************************************************/
    proto.gobj_create_volatil = function(name, gclass, kw, parent)
    {
        return this._gobj_create(
            name,
            gclass,
            kw,
            parent,
            false,
            false,
            true
        );
    };

    /************************************************************
     *        Destroy a gobj
     ************************************************************/
    proto.gobj_destroy = function (gobj) {
        var self = this;

        if(!gobj) {
            log_error("gobj_destroy(): gobj NULL");
            return;
        }
        if (this.config.trace_creation) {
            log_debug("<========== DESTROYED " + gobj.gclass_name + "^" + gobj.name);
        }
        if (gobj._destroyed) {
            // Already deleted
            log_debug("<========== ALREADY DESTROYED! " + gobj.gclass_name + "^" + gobj.name);
            try {
                // Code throwing an exception
                throw new Error();
            } catch(e) {
                console.log(e.stack);
            }
            return;
        }
        if(gobj.gobj_is_running()) {
            gobj.gobj_stop();
        }
        gobj._destroyed = true;

        if (gobj.parent && gobj.parent.mt_child_removed) {
            gobj.parent.mt_child_removed(gobj);
        }
        if (gobj.parent) {
            gobj.parent._remove_child(gobj);
        }
        if(gobj.gobj_is_unique()) {
            this._deregister_unique_gobj(gobj);
        }
        if(gobj.gobj_is_service()) {
            this._deregister_service_gobj(gobj);
        }

        var dl_childs = gobj.dl_childs.slice();

        for (var i=0; i < dl_childs.length; i++) {
            var child = dl_childs[i];
            if (!child._destroyed) {
                self.gobj_destroy(child);
            }
        }

        gobj.clear_timeout();

        gobj.gobj_unsubscribe_list(gobj.gobj_find_subscriptions(), true);

        if (gobj.mt_destroy) {
            gobj.mt_destroy();
        }
    };

    /************************************************************
     *        exist a unique gobj?
     ************************************************************/
    proto._exist_unique_gobj = function(name) {
        var self = this;
        if(kw_has_key(self._unique_gobjs, name)) {
            return true;
        }
        return false;
    };

    /************************************************************
     *        register a unique gobj
     ************************************************************/
    proto._register_unique_gobj = function(gobj) {
        var self = this;
        var named_gobj = self._unique_gobjs[gobj.name];
        if (named_gobj) {
            var msg = "GObj unique ALREADY REGISTERED: " + gobj.name;
            log_error(msg);
            return false;
        }

        self._unique_gobjs[gobj.name] = gobj;
        return true;
    };

    /************************************************************
     *        deregister a unique gobj
     ************************************************************/
    proto._deregister_unique_gobj = function (gobj) {
        var self = this;
        var named_gobj = self._unique_gobjs[gobj.name];
        if (named_gobj) {
            delete self._unique_gobjs[gobj.name];
            return true;
        }
        return false;
    };

    /************************************************************
     *        exist a service gobj?
     ************************************************************/
    proto._exist_service_gobj = function(name) {
        var self = this;
        if(kw_has_key(self._service_gobjs, name)) {
            return true;
        }
        return false;
    };

    /************************************************************
     *        register a service gobj
     ************************************************************/
    proto._register_service_gobj = function(gobj) {
        var self = this;
        var named_gobj = self._service_gobjs[gobj.name];
        if (named_gobj) {
            var msg = "GObj service ALREADY REGISTERED: " + gobj.name;
            log_error(msg);
            return false;
        }
        self._service_gobjs[gobj.name] = gobj;
        return true;
    };

    /************************************************************
     *        deregister a service gobj
     ************************************************************/
    proto._deregister_service_gobj = function (gobj) {
        var self = this;
        var named_gobj = self._service_gobjs[gobj.name];
        if (named_gobj) {
            delete self._service_gobjs[gobj.name];
            return true;
        }
        return false;
    };

    /************************************************************
     *        find a unique gobj
     ************************************************************/
    proto.gobj_find_unique_gobj = function (gobj_name, verbose)
    {
        let named_gobj = this._unique_gobjs[gobj_name];
        if(named_gobj) {
            return named_gobj;
        }
        named_gobj = this._service_gobjs[gobj_name];
        if(!named_gobj && verbose) {
            log_warning("gobj unique not found: '" + gobj_name + "'" );
        }
        return named_gobj;
    };

    /************************************************************
     *        find a service
     ************************************************************/
    proto.gobj_find_service = function (service_name, verbose)
    {
        let service_gobj = this._service_gobjs[service_name];
        if(!service_gobj && verbose) {
            log_warning("gobj service not found: '" + service_name + "'" );
        }
        return service_gobj;
    };

    /************************************************************
     *
     ************************************************************/
    function its_me(gobj, shortname)
    {
        let n = shortname.split("^");
        let gobj_name =null;
        let gclass_name = null;
        if(n.length === 2) {
            gclass_name = n[0];
            gobj_name = n[1];
            if(gclass_name !== gobj.gobj_gclass_name()) {
                return false;
            }
        } else if(n.length === 1){
            gobj_name = n[0];
        } else {
            return false;
        }
        if(gobj_name === gobj.gobj_name()) {
            return true;
        }
        return false;
    }

    /************************************************************
     *
     ************************************************************/
    function _gobj_search_path(gobj, path)
    {
        if(!path) {
            return null;
        }
        /*
         *  Get node and compare with this
         */
        var p = path.split("`");
        var shortname = p[0];
        if(!its_me(gobj, shortname)) {
            return null;
        }
        if(p.length==1) {
            // No more nodes
            return gobj;
        }

        /*
         *  Get next node and compare with childs
         */
        var n = p[1];
        var nn = n.split("^");
        var filter = {};
        if(nn.length == 1) {
            filter.__gobj_name__ = nn;
        } else {
            filter.__gclass_name__ = nn[0];
            filter.__gobj_name__ = nn[1];
        }

        /*
         *  Search in childs
         */
        var child = gobj.gobj_find_child(filter);
        if(!child) {
            return null;
        }
        p.splice(0, 1);
        p = p.join("`");
        return _gobj_search_path(child, p);
    }

    /************************************************************
     *        Find a gobj by path
     ************************************************************/
    proto.gobj_find_gobj = function(path)
    {
        return _gobj_search_path(this, path);
    };

    /************************************************************
     *      find the unique gobj with the public event
     ************************************************************/
    proto.gobj_find_public_event_service = function (event)
    {
        for (let gobj_name in this._unique_gobjs) {
            if (!this._unique_gobjs.hasOwnProperty(gobj_name)) {
                //The current property is not a direct property.
                continue;
            }
            let gobj = this._unique_gobjs[gobj_name];
            if(gobj.gobj_event_in_input_event_list(event)) {
                return gobj;
            }
        }
        return null;
    };

    /************************************************************
     *  Webix tree
            {
                "id":
                "value":
                "data": [] hook
            }
     ************************************************************/
    proto.gobj_list_gobj_tree = function (gobj)
    {
        function _add_gobj_to_webix_tree(d, o)
        {
            let content = {
                "id": o.gobj_full_name(),
                "value": o.gobj_short_name()
            };
            d.push(content);
            let dl_childs = o.dl_childs;
            if(dl_childs.length>0) {
                let d2 = [];
                content['data'] = d2;

                for (let i=0; i < dl_childs.length; i++) {
                    let child = dl_childs[i];
                    _add_gobj_to_webix_tree(d2, child);
                }
            }
        }

        let data = [];
        _add_gobj_to_webix_tree(data, gobj);
        return data;
    };

    /************************************************************
     *
     ************************************************************/
    proto.gobj_list_gobj_attr = function (gobj)
    {
        let data = [];
        if(!gobj || !gobj.config) {
            return data;
        }
        for (let attr in gobj.config) {
            if (gobj.config.hasOwnProperty(attr)) {
                data.push({
                    name: attr,
                    type: typeof(gobj.config[attr]),
                    description: '',
                    stats: 0,
                    value: gobj.config[attr]
                });
            }
        }

        data.push({
            name: '__state__',
            type: 'string',
            description: '',
            stats: 0,
            value: gobj.gobj_current_state()
        });
        data.push({
            name: '__running__',
            type: 'boolean',
            description: '',
            stats: 0,
            value: gobj.gobj_is_running()
        });

        return data;
    };

    /************************************************
     *          Expose to the global object
     ************************************************/
    exports.Yuno = Yuno;
    exports.gobj_register_gclass = gobj_register_gclass;
    exports.gobj_find_gclass = gobj_find_gclass;
    exports.gobj_get_gclass_config = gobj_get_gclass_config;
    exports.gobj_start_up = gobj_start_up;

})(this);
