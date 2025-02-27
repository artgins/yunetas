/*********************************************************************************
 *  Objects with a simple Finite State Machine.
 *
 *      Licence: MIT (http://www.opensource.org/licenses/mit-license)
 *      Copyright (c) 2014,2024 Niyamaka.
 *      Copyright (c) 2025, ArtGins.
 *********************************************************************************/
/* jshint bitwise: false */

import {
    is_string,
    is_object,
    log_error,
    log_warning,
    log_debug,
    empty_string,
    json_deep_copy,
    json_object_update,
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

let __yuno__ = null;
let __default_service__ = null;

/**************************************************************************
 *              SData - Structured Data
 **************************************************************************/
const data_type_t = Object.freeze({
    DTP_STRING:  1,
    DTP_BOOLEAN: 2,
    DTP_INTEGER: 3,
    DTP_REAL:    4,
    DTP_LIST:    5,
    DTP_DICT:    6,
    DTP_JSON:    7,
    DTP_POINTER: 8
});

// Macros as functions
const DTP_IS_STRING  = (type) => type === data_type_t.DTP_STRING;
const DTP_IS_BOOLEAN = (type) => type === data_type_t.DTP_BOOLEAN;
const DTP_IS_INTEGER = (type) => type === data_type_t.DTP_INTEGER;
const DTP_IS_REAL    = (type) => type === data_type_t.DTP_REAL;
const DTP_IS_LIST    = (type) => type === data_type_t.DTP_LIST;
const DTP_IS_DICT    = (type) => type === data_type_t.DTP_DICT;
const DTP_IS_JSON    = (type) => type === data_type_t.DTP_JSON;
const DTP_IS_POINTER = (type) => type === data_type_t.DTP_POINTER;
const DTP_IS_SCHEMA  = (type) => type === DTP_SCHEMA;
const DTP_SCHEMA     = data_type_t.DTP_POINTER;
const DTP_IS_NUMBER  = (type) => DTP_IS_INTEGER(type) || DTP_IS_REAL(type) || DTP_IS_BOOLEAN(type);

const sdata_flag_t = Object.freeze({
    SDF_NOTACCESS:  0x00000001,
    SDF_RD:         0x00000002,
    SDF_WR:         0x00000004,
    SDF_REQUIRED:   0x00000008,
    SDF_PERSIST:    0x00000010,
    SDF_VOLATIL:    0x00000020,
    SDF_RESOURCE:   0x00000040,
    SDF_PKEY:       0x00000080,
    SDF_FUTURE1:    0x00000100,
    SDF_FUTURE2:    0x00000200,
    SDF_WILD_CMD:   0x00000400,
    SDF_STATS:      0x00000800,
    SDF_FKEY:       0x00001000,
    SDF_RSTATS:     0x00002000,
    SDF_PSTATS:     0x00004000,
    SDF_AUTHZ_R:    0x00008000,
    SDF_AUTHZ_W:    0x00010000,
    SDF_AUTHZ_X:    0x00020000,
    SDF_AUTHZ_P:    0x00040000,
    SDF_AUTHZ_S:    0x00080000,
    SDF_AUTHZ_RS:   0x00100000
});

const SDF_PUBLIC_ATTR = sdata_flag_t.SDF_RD | sdata_flag_t.SDF_WR | sdata_flag_t.SDF_STATS |
                        sdata_flag_t.SDF_PERSIST | sdata_flag_t.SDF_VOLATIL |
                        sdata_flag_t.SDF_RSTATS | sdata_flag_t.SDF_PSTATS;
const ATTR_WRITABLE   = sdata_flag_t.SDF_WR | sdata_flag_t.SDF_PERSIST;
const ATTR_READABLE   = SDF_PUBLIC_ATTR;

class SDataDesc {
    constructor(type, name, flag = 0, default_value = null, description = "",
                alias = null, header = null, fillspace = 0, json_fn = null, schema = null, authpth = null) {
        this.type         = type;
        this.name         = name;
        this.alias        = alias;
        this.flag         = flag;
        this.default_value= default_value;
        this.header       = header;
        this.fillspace    = fillspace;
        this.description  = description;
        this.json_fn      = json_fn;
        this.schema       = schema;
        this.authpth      = authpth;
    }
}

const SDATA_END   = () => new SDataDesc(0, null);
const SDATA       = (type, name, flag, default_value, description) => new SDataDesc(type, name, flag, default_value, description);
const SDATACM     = (type, name, alias, items, json_fn, description) => new SDataDesc(type, name, 0, null, description, alias, null, 0, json_fn, items, null);
const SDATACM2    = (type, name, flag, alias, items, json_fn, description) => new SDataDesc(type, name, flag, null, description, alias, null, 0, json_fn, items, null);
const SDATAPM     = (type, name, flag, default_value, description) => new SDataDesc(type, name, flag, default_value, description);
const SDATAAUTHZ  = (type, name, flag, alias, items, description) => new SDataDesc(type, name, flag, null, description, alias, null, 0, null, items, null);
const SDATAPM0    = (type, name, flag, authpth, description) => new SDataDesc(type, name, flag, null, description, null, null, 0, null, null, authpth);
const SDATADF     = (type, name, flag, header, fillspace, description) => new SDataDesc(type, name, flag, null, description, null, header, fillspace);

/***************************************************************
 *              Structures
 ***************************************************************/
class GClass {
    constructor(
        gclass_name,
        gmt,
        lmt,
        tattr_desc,
        priv,
        authz_table,
        command_table,
        s_user_trace_level,
        gclass_flag
    ) {
        this.gclass_name = gclass_name;
        this.dl_states = {};  // FSM list states and their ev/action/next
        this.dl_events = {};  // FSM list of events (gobj_event_t, event_flag_t)
        this.gmt = gmt;        // Global methods
        this.lmt = lmt;

        this.tattr_desc = tattr_desc;
        this.priv = priv;
        this.authz_table = authz_table; // acl
        this.command_table = command_table; // if it exits then mt_command is not used.

        this.s_user_trace_level = s_user_trace_level;
        this.gclass_flag = gclass_flag;

        this.instances = 0;              // instances of this gclass
        this.trace_level = 0;
        this.no_trace_level = 0;
        this.jn_trace_filter = null;
        this.fsm_checked = false;
    }
}

class GObj {
    constructor(gobj_name, gclass, kw, parent, gobj_flag) {
        this.__refs__ = 0;
        this.gclass = gclass;
        this.parent = parent;
        this.dl_childs = [];

        this.current_state = null;
        this.last_state = null;

        this.gobj_flag = gobj_flag;
        this.obflag = 0;

        this.dl_subscriptions = []; // subscriptions of this gobj to events of others gobj.
        this.dl_subscribings = []; // TODO WARNING not implemented in v6, subscribed events loss

        // Data allocated
        this.gobj_name = gobj_name;
        this.jn_attrs = null;   // Set in gobj_create2
        this.jn_stats = {};
        this.jn_user_data = {};
        this.full_name = null;
        this.short_name = null;
        this.priv = null;       // Set in gobj_create2

        this.running = false;       // set by gobj_start/gobj_stop
        this.playing = false;       // set by gobj_play/gobj_pause
        this.disabled = false;      // set by gobj_enable/gobj_disable
        this.bottom_gobj = null;

        this.trace_level = 0;
        this.no_trace_level = 0;
    }
}

/*
 *        gobj_flag_t
 *
    // Example usage:

    let gobj_flag = gobj_flag_t.gobj_flag_yuno | gobj_flag_t.gobj_flag_service;

    // Check if a flag is set

    if (gobj_flag & gobj_flag_t.gobj_flag_service) {
        console.log("Service flag is set");
    }

    // Convert to array of names for debugging

    function print_gobj_flags(flagValue) {
        return Object.keys(gobj_flag_t).filter(key => flagValue & gobj_flag_t[key]);
    }

 */
const gobj_flag_t = Object.freeze({
    gobj_flag_yuno:            0x0001,
    gobj_flag_default_service: 0x0002,
    gobj_flag_service:         0x0004,  // Interface (events, attrs, commands, stats)
    gobj_flag_volatil:         0x0008,
    gobj_flag_pure_child:      0x0010,  // Pure child sends events directly to parent
    gobj_flag_autostart:       0x0020,  // Set by gobj_create_tree0 too
    gobj_flag_autoplay:        0x0040,  // Set by gobj_create_tree0 too
});

const gclass_flag_t = Object.freeze({
    gcflag_manual_start:             0x0001,   // gobj_start_tree() doesn't start automatically
    gcflag_no_check_output_events:   0x0002,   // When publishing, don't check events in output_event_list
    gcflag_ignore_unknown_attrs:     0x0004,   // When creating a gobj, ignore non-existing attrs
    gcflag_required_start_to_play:   0x0008,   // Don't play if start wasn't done
    gcflag_singleton:                0x0010,   // Can only have one instance
});

/*
 *  Usage

    let flags = gclass_flag_t.gcflag_manual_start | gclass_flag_t.gcflag_singleton;

    if (flags & gclass_flag_t.gcflag_manual_start) {
        console.log("Manual start flag is set");
    }

 */

const obflag_t = Object.freeze({
    obflag_destroying:  0x0001,
    obflag_destroyed:   0x0002,
    obflag_created:     0x0004,
});


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
function _register_gclass(
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
 *  create and register gclass
 ************************************************************/
function gclass_create(
    gclass_name,
    event_types,
    states,
    gmt,
    lmt,
    tattr_desc,
    priv,
    authz_table,
    command_table,
    s_user_trace_level,
    gclass_flag
) {
    if(empty_string(gclass_name)) {
        log_error(`gclass_name EMPTY`);
        return null;
    }
    if (gclass_name.includes('`') ||
        gclass_name.includes('^') ||
        gclass_name.includes('.'))
    {
        log_error(`GClass name CANNOT have '.' or '^' or '\`' char: ${gclass_name}`);
        return null;
    }
    if(gclass_find_by_name(gclass_name)) {
        log_error(`GClass ALREADY exists: ${gclass_name}`);
        return null;
    }
    let gclass = new GClass(
        gclass_name,
        gmt,
        lmt,
        tattr_desc,
        priv,
        authz_table,
        command_table,
        s_user_trace_level,
        gclass_flag
    );

    _register_gclass(gclass, gclass_name);

    /*----------------------------------------*
     *          Build States
     *----------------------------------------*/
    const states_size = states.length;
    for (let i = 0; i < states_size; i++) {
        let state_name = states[i][0];      // First element is state name
        let ev_action_list = states[i][1];  // Second element is the array of actions

        if(gclass_add_state(gclass, state_name)<0) {
            // Error already logged
            gclass_unregister(gclass);
            return null;
        }

        const ev_action_size = ev_action_list.length;
        for (let j = 0; j < ev_action_size; j++) {
            let event_name = ev_action_list[j][0];
            let action = ev_action_list[j][1];
            let next_state = ev_action_list[j][2];
            gclass_add_ev_action(
                gclass,
                state_name,
                event_name,
                action,
                next_state
            );
        }
    }

    /*----------------------------------------*
     *          Build Events
     *----------------------------------------*/
    let event_types_size = event_types.length;
    for (let i = 0; i < event_types_size; i++) {
        let event_name = event_types[i][0];
        let event_type = event_types[i][1];

        if(gclass_find_event_type(gclass, event_name)) {
            log_error(`SMachine: event repeated in input_events: ${event_name}`);
            gclass_unregister(gclass);
            return null;
        }

        gclass_add_event_type(gclass, event_type);
    }

    /*----------------------------------------*
     *          Check FSM
     *----------------------------------------*/
    if(!gclass.fsm_checked) {
        if(gclass_check_fsm(gclass)<0) {
            gclass_unregister(gclass);
            return null;
        }
        gclass.fsm_checked = true;
    }

    return gclass;
}

/************************************************************
 *
 ************************************************************/
function gclass_unregister(gclass)
{
    let gclass_name;
    if(is_string(gclass)) {
        gclass_name = gclass;
    } else if(gclass instanceof GClass) {
        gclass_name = gclass.gclass_name;
    } else {
        log_error(`Cannot unregister gclass, type unknown`);
        return -1;
    }

    let gclass_ = _gclass_register[gclass_name];
    if(!gclass_) {
        log_error(`Cannot unregister gclass, not found: ${gclass_name}`);
        return -1;
    }
    if(gclass_.instances > 0) {
        log_error(`Cannot unregister gclass, instances in use: ${gclass_name}`);
        return -1;
    }

    delete _gclass_register[gclass_name];
    return 0;
}

/************************************************************
 *
 ************************************************************/
function gclass_add_state(gclass, state_name)
{
    if(!(gclass instanceof GClass)) {
        log_error(`Cannot add state, typeof not GClass`);
        return -1;
    }

    return 0;
}

/************************************************************
 *
 ************************************************************/
function gclass_add_ev_action(
    gclass,
    state_name,
    event_name,
    action,
    next_state
)
{
    if(!(gclass instanceof GClass)) {
        log_error(`Cannot add state, typeof not GClass`);
        return -1;
    }

    return 0;
}

/************************************************************
 *
 ************************************************************/
function gclass_add_event_type(gclass, event_type)
{
    if(!(gclass instanceof GClass)) {
        log_error(`Cannot add state, typeof not GClass`);
        return -1;
    }

    return 0;
}

/************************************************************
 *
 ************************************************************/
function gclass_find_event_type(gclass, event_name)
{
    if(!(gclass instanceof GClass)) {
        log_error(`Cannot add state, typeof not GClass`);
        return -1;
    }

    return 0;
}

/************************************************************
 *
 ************************************************************/
function gclass_check_fsm(gclass)
{
    if(!(gclass instanceof GClass)) {
        log_error(`Cannot add state, typeof not GClass`);
        return -1;
    }

    return 0;

}

/************************************************************
 *      Find gclass
 ************************************************************/
function gclass_find_by_name(gclass_name, verbose)
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
    let service_name = gobj.gobj_name;
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
    let gclass = gclass_find_by_name(gclass_name, verbose);
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
 *
 ************************************************************/
function gobj_create2(
    gobj_name,
    gclass_name,
    kw,
    parent,
    gobj_flag
) {
    /*--------------------------------*
     *      Check parameters
     *--------------------------------*/
    /*
     *  gobj_name to lower, make case-insensitive
     */
    gobj_name = gobj_name.toLowerCase();

    if(gobj_flag & (gobj_flag_t.gobj_flag_yuno)) {
        if(__yuno__) {
            log_error(`__yuno__ already created`);
            return null;
        }
        gobj_flag |= gobj_flag_t.gobj_flag_service;

    } else {
        if(!parent) {
            log_error(`gobj NEEDS a parent!`);
            return null;
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
    }

    if(gobj_flag & (gobj_flag_t.gobj_flag_service)) {
        if(gobj_find_service(gobj_name, false)) {
            log_error(`service ALREADY registered: ${gclass_name}`);
            return null;
        }
    }

    if(gobj_flag & (gobj_flag_t.gobj_flag_default_service)) {
        if(gobj_find_service(gobj_name, false) || __default_service__) {
            log_error(`default service ALREADY registered: ${gclass_name}`);
            return null;
        }
        gobj_flag |= gobj_flag_t.gobj_flag_service;
    }

    if(empty_string(gclass_name)) {
        log_error(`gclass_name must be a string`);
        return null;
    }
    let gclass = gclass_find_by_name(gclass_name, false);
    if(!gclass) {
        log_error(`GClass not registered: ${gclass_name}`);
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
        if(gobj_name.includes('`')) {
            log_error(`gobj_name cannot contain char \': ${gobj_name}`);
            return null;
        }
        /*
         *  Check that the gobj_name: cannot contain ^
         */
        if(gobj_name.includes('^')) {
            log_error(`gobj_name cannot contain char ^: ${gobj_name}`);
            return null;
        }
    }

    /*--------------------------------*
     *      Alloc memory
     *--------------------------------*/
    let gobj = new GObj(gobj_name, gclass, kw, parent, gobj_flag);

    /*--------------------------------*
     *      Initialize variables
     *--------------------------------*/
    // TODO
    gobj.jn_attrs =  json_deep_copy(gclass.config); // jn_attrs sdata_create tattr_desc;
    gobj.priv = json_deep_copy(gclass.private); // kw_extract_private(gobj.config);
    gobj.current_state = null; // TODO dl_first(&gclass->dl_states);

    if(trace_creation) { // if(__trace_gobj_create_delete__(gobj))
        log_debug(sprintf("💙💙⏩ creating: %s^%s",
            gclass.gclass_name, gobj_name
        ));
    }

    // from gobj.js 6
    // gobj.tracing = 0;
    // gobj.trace_timer = 0;
    // gobj.running = false;
    // gobj._destroyed = false;
    // gobj.timer_id = -1; // for now, only one timer per fsm, and hardcoded.
    // gobj.timer_event_name = "EV_TIMEOUT";
    // gobj.__volatil__ = (gobj_flag & gobj_flag_t.gobj_flag_volatil);
    // TODO gobj.fsm_create(fsm_desc); // Create gobj.fsm

    /*--------------------------------*
     *  Write configuration
     *--------------------------------*/
    _json_object_update_config(gobj.config, kw || {});
    //write_json_parameters(gobj, kw, __jn_global_settings__);

    /*--------------------------------------*
     *  Load writable and persistent attrs
     *  of services and __root__
     *--------------------------------------*/
    if(gobj.gobj_flag & (gobj_flag_t.gobj_flag_service)) {
        if(__global_load_persistent_attrs_fn__) {
            __global_load_persistent_attrs_fn__(gobj, 0);
        }
    }

    /*--------------------------*
     *  Register service
     *--------------------------*/
    if(gobj.gobj_flag & (gobj_flag_t.gobj_flag_service)) {
        _register_service(gobj);
    }
    if(gobj.gobj_flag & (gobj_flag_t.gobj_flag_yuno)) {
        __yuno__ = gobj;
    }
    if(gobj.gobj_flag & (gobj_flag_t.gobj_flag_default_service)) {
        __default_service__ = gobj;
    }

    /*--------------------------------------*
     *      Add to parent
     *--------------------------------------*/
    if(!(gobj.gobj_flag & (gobj_flag_t.gobj_flag_yuno))) {
        parent.dl_childs.push(gobj);
    }

    /*--------------------------------*
     *      Exec mt_create
     *--------------------------------*/
    gobj.obflag |= obflag_t.obflag_created;
    gobj.gclass.instances++;

    if(gobj.gclass.gmt.mt_create2) {
        gobj.gclass.gmt.mt_create2(gobj, kw);
    } else if(gobj.gclass.gmt.mt_create) {
        gobj.gclass.gmt.mt_create(gobj);
    }

    /*--------------------------------------*
     *  Inform to parent
     *  when the child is full operative
     *-------------------------------------*/
    if(parent && parent.gclass.gmt.mt_child_added) {
        if(trace_creation) { // if(__trace_gobj_create_delete__(gobj))
            log_debug(sprintf(
                "👦👦🔵 child_added(%s): %s",
                parent.gobj_full_name(),
                gobj.gobj_short_name())
            );
        }
        parent.gclass.gmt.mt_child_added(parent, gobj);
    }

    if(trace_creation) { // if(__trace_gobj_create_delete__(gobj))
        log_debug("💙💙⏪ created: " + gobj.gobj_full_name());
    }

    return gobj;
}

/************************************************************
 *
 ************************************************************/
function gobj_create_yuno(
    gobj_name,
    gclass_name,
    kw
) {
    return gobj_create2(gobj_name, gclass_name, kw, null, gobj_flag_t.gobj_flag_yuno);
}

function gobj_create_service(
    gobj_name,
    gclass_name,
    kw,
    parent
) {
    return gobj_create2(gobj_name, gclass_name, kw, parent, gobj_flag_t.gobj_flag_service);
}

function gobj_create_default_service(
    gobj_name,
    gclass_name,
    kw,
    parent
) {
    return gobj_create2(
        gobj_name,
        gclass_name,
        kw,
        parent,
        gobj_flag_t.gobj_flag_default_service|gobj_flag_t.gobj_flag_autostart
    );
}

function gobj_create_volatil(
    gobj_name,
    gclass_name,
    kw,
    parent
) {
    return gobj_create2(
        gobj_name,
        gclass_name,
        kw,
        parent,
        gobj_flag_t.gobj_flag_volatil
    );
}

function gobj_create_pure_child(
    gobj_name,
    gclass_name,
    kw,
    parent
) {
    return gobj_create2(
        gobj_name,
        gclass_name,
        kw,
        parent,
        gobj_flag_t.gobj_flag_pure_child
    );
}

function gobj_create(
    gobj_name,
    gclass_name,
    kw,
    parent
) {
    return gobj_create2(
        gobj_name,
        gclass_name,
        kw,
        parent,
        0
    );
}

/************************************************************
 *
 ************************************************************/
function gobj_destroy(gobj)
{
    if(!gobj || gobj.obflag & (obflag_t.obflag_destroyed|obflag_t.obflag_destroying)) {
        log_error("gobj NULL or DESTROYED");
        return false;
    }

    let full_name = gobj_full_name(gobj);

    if(trace_creation) { // if(__trace_gobj_create_delete__(gobj))
        log_debug("💔💔⏩ destroying: " + gobj_short_name(gobj));
    }
    if (gobj._destroyed) {
        // Already deleted
        log_error("<========== ALREADY DESTROYED! " + gobj_short_name(gobj));
        return;
    }
    if(gobj_is_running(gobj)) {
        gobj_stop(gobj);
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

    if (this.config.trace_creation) {
        log_debug("💔💔⏪ destroyed: " + full_name);
    }
}

/************************************************************
 *
 ************************************************************/
function gobj_start(gobj)
{
    if(!gobj || gobj.obflag & (obflag_t.obflag_destroyed|obflag_t.obflag_destroying)) {
        log_error("gobj NULL or DESTROYED");
        return -1;
    }
    if(gobj.running) {
        log_error("GObj ALREADY RUNNING");
        return -1;
    }
    if(gobj.disabled) {
        log_error("GObj DISABLED");
        return -1;
    }

    /*
     *  TODO Check required attributes.
     */
    // json_t *jn_required_attrs = gobj_check_required_attrs(gobj);
    // if(jn_required_attrs) {
    //     gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
    //         "gobj",         "%s", gobj_full_name(gobj),
    //         "function",     "%s", __FUNCTION__,
    //         "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
    //         "msg",          "%s", "Cannot start without all required attributes",
    //         "attrs",        "%j", jn_required_attrs,
    //         NULL
    //     );
    //     JSON_DECREF(jn_required_attrs)
    //     return -1;
    // }

    // if(__trace_gobj_start_stop__(gobj)) {
    //     trace_machine("⏺ ⏺ start: %s",
    //         gobj_full_name(gobj)
    //     );
    // }

    gobj.running = true;

    let ret = 0;
    if(gobj.gclass.gmt.mt_start) {
        ret = gobj.gclass.gmt.mt_start(gobj);
    }
    return ret;
}

/************************************************************
 *
 ************************************************************/
function gobj_stop(gobj)
{
    if(!gobj || gobj.obflag & (obflag_t.obflag_destroyed|obflag_t.obflag_destroying)) {
        log_error("gobj NULL or DESTROYED");
        return -1;
    }
    if(!gobj.running) {
        log_error("GObj NOT RUNNING");
        return -1;
    }
    if(gobj.playing) {
        // It's auto-stopping but display error (magic but warn!).
        log_warning("GObj stopping without previous pause");
        gobj_pause(gobj);
    }

    // if(__trace_gobj_start_stop__(gobj)) {
    //     trace_machine("⏹ ⏹ stop: %s",
    //         gobj_full_name(gobj)
    //     );
    // }

    gobj.running = false;

    let ret = 0;
    if(gobj.gclass.gmt.mt_stop) {
        ret = gobj.gclass.gmt.mt_stop(gobj);
    }

    return ret;
}

/************************************************************
 *
 ************************************************************/
function gobj_play(gobj)
{
    if(!gobj || gobj.obflag & (obflag_t.obflag_destroyed|obflag_t.obflag_destroying)) {
        log_error("gobj NULL or DESTROYED");
        return -1;
    }
    if(gobj.playing) {
        log_error("GObj ALREADY PLAYING");
        return -1;
    }
    if(gobj.disabled) {
        log_error("GObj DISABLED");
        return -1;
    }
    if(!gobj_is_running(gobj)) {
        if(!(gobj.gclass.gclass_flag & gclass_flag_t.gcflag_required_start_to_play)) {
            // Default: It's auto-starting but display error (magic but warn!).
            log_warning("GObj playing without previous start");
            gobj_start(gobj);
        } else {
            log_error("Cannot play, start not done");
            return -1;
        }
    }

    // if(__trace_gobj_start_stop__(gobj)) {
    //     if(!is_machine_not_tracing(gobj, 0)) {
    //         trace_machine("⏯ ⏯ play: %s",
    //             gobj_full_name(gobj)
    //         );
    //     }
    // }

    gobj.playing = true;

    let ret = 0;
    if(gobj.gclass.gmt.mt_play) {
        ret = gobj.gclass.gmt.mt_play(gobj);
        if(ret < 0) {
            gobj.playing = false;
        }
    }
    return ret;
}

/************************************************************
 *
 ************************************************************/
function gobj_pause(gobj)
{
    if(!gobj || gobj.obflag & (obflag_t.obflag_destroyed|obflag_t.obflag_destroying)) {
        log_error("gobj NULL or DESTROYED");
        return -1;
    }
    if(!gobj.playing) {
        log_error("GObj NOT PLAYING");
        return -1;
    }

    // if(__trace_gobj_start_stop__(gobj)) {
    //     if(!is_machine_not_tracing(gobj, 0)) {
    //         trace_machine("⏸ ⏸ pause: %s",
    //             gobj_full_name(gobj)
    //         );
    //     }
    // }

    gobj.playing = false;

    if(gobj.gclass.gmt.mt_pause) {
        return gobj.gclass.gmt.mt_pause(gobj);
    } else {
        return 0;
    }
}

/************************************************************
 *
 ************************************************************/
function gobj_is_running(gobj)
{
    if(!gobj || gobj.obflag & (obflag_t.obflag_destroyed|obflag_t.obflag_destroying)) {
        log_error("gobj NULL or DESTROYED");
        return false;
    }
    return gobj.running;
}

/************************************************************
 *
 ************************************************************/
function gobj_is_playing(gobj)
{
    if(!gobj || gobj.obflag & (obflag_t.obflag_destroyed|obflag_t.obflag_destroying)) {
        log_error("gobj NULL or DESTROYED");
        return false;
    }
    return gobj.playing;
}

/************************************************************
 *
 ************************************************************/
function gobj_yuno()
{
    if(!__yuno__ || (__yuno__.obflag & obflag_t.obflag_destroyed)) {
        return null;
    }
    return __yuno__;
}

function gobj_default_service()
{
    if(!__default_service__ || (__default_service__.obflag & obflag_t.obflag_destroyed)) {
        return null;
    }
    return __default_service__;
}

/************************************************************
 *      get short name (gclass^name)
 ************************************************************/
function gobj_short_name(gobj)
{
    return gobj.gclass.gclass_name + '^' + gobj.gobj_name;
}

/************************************************************
 *      Get full name
 ************************************************************/
function gobj_full_name(gobj)
{
    let full_name = gobj_short_name(gobj);
    let parent = gobj.parent;
    while(parent) {
        let prefix = parent.gobj_short_name();
        full_name = prefix + '`' + full_name;
        parent = parent.parent;
    }
    return full_name;
}

/************************************************************
 *
 ************************************************************/
function gobj_parent(gobj)
{
    return gobj.parent;
}


//=======================================================================
//      Expose the class via the global object
//=======================================================================
export {
    data_type_t,
    sdata_flag_t,
    SDataDesc,
    SDATA,
    SDATACM,
    SDATACM2,
    SDATAPM,
    SDATAAUTHZ,
    SDATAPM0,
    SDATADF,
    SDATA_END,
    gobj_flag_t,
    gclass_flag_t,
    gobj_start_up,
    gclass_create,
    gclass_unregister,
    gclass_add_state,
    gclass_add_ev_action,
    gclass_find_event_type,
    gclass_add_event_type,
    gclass_check_fsm,
    gclass_find_by_name,
    gobj_find_service,
    gobj_list_persistent_attrs,
    gobj_create2,
    gobj_create_yuno,
    gobj_create_service,
    gobj_create_default_service,
    gobj_create_volatil,
    gobj_create_pure_child,
    gobj_create,
    gobj_destroy,
    gobj_start,
    gobj_stop,
    gobj_play,
    gobj_pause,
    gobj_is_running,
    gobj_is_playing,
    gobj_yuno,
    gobj_default_service,
    gobj_short_name,
    gobj_full_name,
    gobj_parent,
};
