/*********************************************************************************
 *  Objects with a simple Finite State Machine.
 *
 *      Licence: MIT (http://www.opensource.org/licenses/mit-license)
 *      Copyright (c) 2014,2024 Niyamaka.
 *      Copyright (c) 2025, ArtGins.
 *********************************************************************************/
import {
    is_string,
    log_error,
    log_warning,
    log_debug,
    empty_string,
    json_deep_copy,
    kw_extract_private, kw_has_key,
} from "./utils.js";
import {sprintf} from "./sprintf.js";

/**************************************************************************
 *        Private data
 **************************************************************************/
let __inside_event_loop__ = 0;
let __jn_global_settings__ =  null;
let __global_load_persistent_attrs_fn__ = null;
let __global_save_persistent_attrs_fn__ = null;
let __global_remove_persistent_attrs_fn__ = null;
let __global_list_persistent_attrs_fn__ = null;
let __global_command_parser_fn__ = null;
let __global_stats_parser_fn__ = null;

let trace_creation = false;

let _gclass_register = {};
let __jn_services__ = {};

/**************************************************************************
 *        Structures
 **************************************************************************/
function GClass(
    gclass_name,
    event_types,
    states,
    gmt,
    lmt,
    tattr_desc,
    priv_size,
    authz_table,
    command_table,
    s_user_trace_level,
    gclass_flag
) {
    this.gclass_name = gclass_name;
    this.event_types = event_types;
    this.states = states;
    this.gmt = gmt;
    this.lmt = lmt;
    this.tattr_desc = tattr_desc;
    this.priv_size = priv_size;
    this.authz_table = authz_table;
    this.command_table = command_table;
    this.s_user_trace_level = s_user_trace_level;
    this.gclass_flag = gclass_flag;
}

function GObj(
    gobj_name,
    gclass,
    kw,
    parent,
    gobj_flag
) {
    this.gobj_name = gobj_name;
    this.gclass = gclass;
    this.parent = parent;
    this.gobj_flag = gobj_flag;

    this.current_state = ""; // TODO dl_first(&gclass->dl_states);
    this.last_state = 0;

    this.dl_subscriptions = [];
    this.dl_subscribings = []; // TODO WARNING not implemented, subscribed events loss
    this.dl_childs = [];
    this.user_data = {};

    let config = {}; // TODO get from gclass

    this.config = json_deep_copy(config);
    _json_object_update_config(this.config, kw || {});
    this.private = kw_extract_private(this.config);

    this.tracing = 0;
    this.trace_timer = 0;
    this.running = false;
    this._destroyed = false;
    this.timer_id = -1; // for now, only one timer per fsm, and hardcoded.
    this.timer_event_name = "EV_TIMEOUT";
    // TODO this.fsm_create(fsm_desc); // Create this.fsm

}

/**************************************************************************
 *        gobj_flag_t
 *
    // Example usage:
    let gobj_flag = gobj_flag_t.gobj_flag_yuno | gobj_flag_t.gobj_flag_service;

    // Check if a flag is set
    if (gobj_flag & gobj_flag_t.gobj_flag_service) {
        console.log("Service flag is set");
    }
 **************************************************************************/
const gobj_flag_t = Object.freeze({
    gobj_flag_yuno:            0x0001,
    gobj_flag_default_service: 0x0002,
    gobj_flag_service:         0x0004,  // Interface (events, attrs, commands, stats)
    gobj_flag_volatil:         0x0008,
    gobj_flag_pure_child:      0x0010,  // Pure child sends events directly to parent
    gobj_flag_autostart:       0x0020,  // Set by gobj_create_tree0 too
    gobj_flag_autoplay:        0x0040,  // Set by gobj_create_tree0 too
});

// Convert to array of names for debugging
function print_gobj_flags(flagValue) {
    return Object.keys(gobj_flag_t).filter(key => flagValue & gobj_flag_t[key]);
}

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
function gobj_register_gclass(
    gclass,
    gclass_name
)
{
    if(!gclass || !(gclass instanceof GClass)) {
        log_error(`gclass undefined`);
        return -1;
    }
    if(!gclass_name) {
        log_error(`gclass_name undefined`);
        return -1;
    }

    let gclass_ = _gclass_register[gclass_name];
    if (gclass_) {
        log_error(`gclass ALREADY REGISTERED: ${gclass_name}`);
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
    let gclass = null;
    try {
        gclass = _gclass_register[gclass_name];
    } catch (e) {
    }
    if(!gclass) {
        if(verbose) {
            log_error(`gclass not found: ${gclass_name}`);
        }
    }
    return gclass;
}

/************************************************************
 *        find a service
 ************************************************************/
function gobj_find_service(
    service_name,
    verbose
)
{
    let service_gobj = __jn_services__[service_name];
    if(!service_gobj && verbose) {
        log_warning(`gobj service not found: ${service_name}`);
        return null;
    }
    return service_gobj;
}

function _register_service(gobj)
{
    let service_name = gobj.name;
    if(__jn_services__[service_name]) {
        log_error(`service ALREADY REGISTERED: ${service_name}. Will be UPDATED`);
    }
    __jn_services__[service_name] = gobj;
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
    if(gclass && gclass.prototype.mt_get_gclass_config) { // TODO review
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
 *  Update existing in first level, and all in next levels
 ************************************************************/
function _json_object_update_config(destination, source) {
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
 *      gobj_create factory.
 ************************************************************/
function gobj_create2(
    gobj_name,
    gclass_name,
    kw,
    parent,
    gobj_flag
) {
    if(empty_string(gclass_name)) {
        log_error(`gclass_name must be a string`);
        return null;
    }
    let gclass = gobj_find_gclass(gclass_name, false);
    if(!gclass) {
        log_error(`GClass not defined: ${gclass_name}`);
        return null;
    }
    if(!is_string(gobj_name)) {
        log_error(`gobj_name must be a string`);
        return null;
    }

    if(!empty_string(gobj_name)) {
        /*
         *  Check that the gobj_name: cannot contain `
         */
        if(gobj_name.indexOf("`")>=0) {
            log_error(`gobj_name cannot contain char \': ${gobj_name}`);
            return null;
        }
        /*
         *  Check that the gobj_name: cannot contain ^
         */
        if(gobj_name.indexOf("^")>=0) {
            log_error(`gobj_name cannot contain char ^: ${gobj_name}`);
            return null;
        }
    }

    if(!(typeof parent === 'string' || parent instanceof GObj)) {
        log_error(`BAD TYPE of parent: ${parent}`);
        return null;
    }

    if(is_string(parent)) {
        // find the named gobj
        parent = gobj_find_service(parent);
        if (!parent) {
            log_warning(`parent service not found: ${parent}`);
            return null;
        }
    }

    let is_service = (gobj_flag & gobj_flag_t.gobj_flag_service);
    if(is_service) {
        if(gobj_find_service(gobj_name)) {
            log_warning(`service already defined: ${gobj_name}`);
            return null;
        }
    }
    let gobj = new GObj(gobj_name, gclass, kw, parent, gobj_flag);

    if(trace_creation) {
        let gclass_name = gobj.gclass_name || '';
        log_debug(sprintf("💙💙⏩ creating: %s %s^%s",
            is_service?"service":"", gclass_name, gobj_name
        ));
    }

    if(is_service) {
        _register_service(gobj);
        gobj.__service__ = true;
    } else {
        gobj.__service__ = false;
    }
    if(is_service) {
        gobj_load_persistent_attrs(gobj);
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
}


//=======================================================================
//      Expose the class via the global object
//=======================================================================
export {
    gobj_flag_t,
    gobj_start_up,
    gobj_register_gclass,
    gobj_find_gclass,
    gobj_find_service,
    gobj_get_gclass_config,
    gobj_list_persistent_attrs,
    gobj_create2,
    // GObj,
    // gcflag_manual_start,
    // gcflag_no_check_output_events,
};
