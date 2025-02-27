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
    is_array,
    is_boolean,
    is_number,
    log_error,
    log_warning,
    log_debug,
    trace_machine,
    empty_string,
    json_deep_copy,
    json_object_update,
    index_in_list,
    json_array_size,
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
        attrs_table,
        priv,
        authz_table,
        command_table,
        s_user_trace_level,
        gclass_flag
    ) {
        this.gclass_name = gclass_name;
        this.dl_states = [];  // FSM list states and their ev/action/next
        this.dl_events = [];  // FSM list of events (gobj_event_t, event_flag_t)
        this.gmt = gmt;        // Global methods
        this.lmt = lmt;

        this.attrs_table = attrs_table;
        this.priv = priv;
        this.authz_table = authz_table; // acl
        this.command_table = command_table; // if it exits then mt_command is not used.

        this.s_user_trace_level = s_user_trace_level;
        this.gclass_flag = Number(gclass_flag) || 0;

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
        this.dl_subscriptions = []; // subscriptions of this gobj to events of others gobj.
        this.dl_subscribings = []; // TODO WARNING not implemented in v6, subscribed events loss
        this.current_state = null; // TODO dl_first(&gclass->dl_states);
        this.last_state = null;
        this.obflag = 0;
        this.gobj_flag = Number(gobj_flag) || 0;

        // Data allocated
        this.gobj_name = gobj_name;
        this.jn_attrs =  sdata_create(this, gclass.attrs_table);
        this.jn_stats = {};
        this.jn_user_data = {};
        this.full_name = null;
        this.short_name = null;
        this.priv = json_deep_copy(gclass.priv);

        this.running = false;       // set by gobj_start/gobj_stop
        this.playing = false;       // set by gobj_play/gobj_pause
        this.disabled = false;      // set by gobj_enable/gobj_disable
        this.bottom_gobj = null;

        this.trace_level = 0;
        this.no_trace_level = 0;
    }
}

/*
 *  gclass_flag_t
 *  Features of the gclass
 *
    // Example usage:

    let flags = gclass_flag_t.gcflag_manual_start | gclass_flag_t.gcflag_singleton;

    if (flags & gclass_flag_t.gcflag_manual_start) {
        console.log("Manual start flag is set");
    }

 */

const gclass_flag_t = Object.freeze({
    gcflag_manual_start:             0x0001,   // gobj_start_tree() doesn't start automatically
    gcflag_no_check_output_events:   0x0002,   // When publishing, don't check events in output_event_list
    gcflag_ignore_unknown_attrs:     0x0004,   // When creating a gobj, ignore non-existing attrs
    gcflag_required_start_to_play:   0x0008,   // Don't play if start wasn't done
    gcflag_singleton:                0x0010,   // Can only have one instance
});

/*
 *  gobj_flag_t
 *  Features of the gobj
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

/*
 *  obflag_t
 *  Features realtime of the gobj
 */
const obflag_t = Object.freeze({
    obflag_destroying:  0x0001,
    obflag_destroyed:   0x0002,
    obflag_created:     0x0004,
});

const event_flag_t = Object.freeze({
    EVF_NO_WARN_SUBS    : 0x0001,   // Don't warn of "Publish event WITHOUT subscribers"
    EVF_OUTPUT_EVENT    : 0x0002,   // Output Event
    EVF_SYSTEM_EVENT    : 0x0004,   // System Event
    EVF_PUBLIC_EVENT    : 0x0008,   // You should document a public event, it's the API
});

/*---------------------------*
 *          FSM
 *---------------------------*/
class Event_Action { // C event_action_t
    constructor(event_name, action, next_state) {
        this.event_name = event_name;
        this.action = action;
        this.next_state = next_state;
    }
}

class State { // C state_t
    constructor(state_name) {
        this.state_name = state_name;
        this.dl_actions = [];
    }
}

class Event_Type { // C event_type_t
    constructor(event_name, event_flag) {
        this.event_name = event_name;
        this.event_flag = Number(event_flag) || 0;
    }
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
    __jn_global_settings__ =  jn_global_settings;
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

/***************************************************************************
 *  WRITE - high level -
 ***************************************************************************/
function sdata_write_default_values(
    gobj,
    include_flag,   // sdata_flag_t
    exclude_flag    // sdata_flag_t
)
{
    let sdata_desc = gobj.gclass.attrs_table;

    for(let i=0; i < sdata_desc.length; i++) {
        const it = sdata_desc[i];

        if(exclude_flag && (it.flag & exclude_flag)) {
            continue;
        }
        if(include_flag === -1 || (it.flag & include_flag)) {
            set_default(gobj, gobj.jn_attrs, it);

            if((gobj.obflag & obflag_t.obflag_created) &&
                !(gobj.obflag & obflag_t.obflag_destroyed))
            {
                // Avoid call to mt_writing before mt_create!
                gobj.gclass.gmt.mt_writing(gobj, it.name);
            }
        }
    }
}

/***************************************************************************
 *  Build default values
 ***************************************************************************/
function sdata_create(gobj, sdata_desc)
{
    let sdata = {};
    for(let i=0; i < sdata_desc.length; i++) {
        const it = sdata_desc[i];
        set_default(gobj, sdata, it);
    }

    return sdata;
}

/***************************************************************************
 *  Set array values to sdata, from json or binary
 ***************************************************************************/
function set_default(gobj, sdata, it)
{
    let jn_value;
    let svalue = it.default_value;

    switch(it.type) {
        case data_type_t.DTP_STRING:
            jn_value = String(svalue);
            break;
        case data_type_t.DTP_BOOLEAN:
            jn_value = Boolean(svalue);
            if(is_string(svalue)) {
                svalue = svalue.toLowerCase();
                if(svalue === "true") {
                    jn_value = true;
                } else if(svalue === "false") {
                    jn_value = false;
                } else {
                    jn_value = !!parseInt(svalue, 10);
                }
            }
            break;
        case data_type_t.DTP_INTEGER:
            jn_value = Number(svalue);
            break;
        case data_type_t.DTP_REAL:
            jn_value = Number(svalue);
            break;
        case data_type_t.DTP_LIST:
            jn_value = JSON.parse(svalue);
            if(!is_array(jn_value)) {
                jn_value = [];
            }
            break;
        case data_type_t.DTP_DICT:
            jn_value = JSON.parse(svalue);
            if(!is_object(jn_value)) {
                jn_value = {};
            }
            break;
        case data_type_t.DTP_JSON:
            if(!empty_string(svalue)) {
                jn_value = JSON.parse(svalue);
            } else {
                jn_value = null;
            }
            break;
        case data_type_t.DTP_POINTER:
            jn_value = BigInt(svalue);
            break;
    }

    if(jn_value === undefined) {
        log_error(`default_value WRONG, it: ${it.name}, value: ${it.name}`);
        jn_value = null;
    }

    sdata[it.name] = jn_value;
    return 0;
}

/***************************************************************************
 *  Set array values to sdata, from json or binary
 ***************************************************************************/
function json2item(gobj, sdata, it, jn_value_)
{
    if(!it) {
        return -1;
    }
    let jn_value2;

    switch(it.type) {
        case data_type_t.DTP_STRING:
            if(is_string(jn_value_)) {
                jn_value2 = String(jn_value_);
            } else {
                let s = JSON.stringify(jn_value_);
                if(s) {
                    jn_value2 = String(s);
                }
            }
            break;
        case data_type_t.DTP_BOOLEAN:
            if(is_boolean(jn_value_)) {
                jn_value2 = jn_value_;
            } else if(is_string(jn_value_)) {
                let s = jn_value_.toLowerCase();
                if(s) {
                    if(s === "true") {
                        jn_value2 = true;
                    } else if(s === "false") {
                        jn_value2 = false;
                    } else {
                        jn_value2 = !!parseInt(s, 10);
                    }
                }
            }
            break;
        case data_type_t.DTP_INTEGER:
            if(is_number(jn_value_)) {
                jn_value2 = jn_value_;
            } else {
                jn_value2 = Number(jn_value_);
            }
            break;
        case data_type_t.DTP_REAL:
            if(is_number(jn_value_)) {
                jn_value2 = jn_value_;
            } else {
                jn_value2 = Number(jn_value_);
            }
            break;
        case data_type_t.DTP_LIST:
            if(is_array(jn_value_)) {
                jn_value2 = jn_value_;
            } else if(is_string(jn_value_)) {
                jn_value2 = JSON.parse(jn_value_);
            }

            if(!is_array(jn_value2)) {
                log_error(`attr must be an array: ${it.name}`);
                return -1;
            }
            break;

        case data_type_t.DTP_DICT:
            if(is_object(jn_value_)) {
                jn_value2 = jn_value_;
            } else if(is_string(jn_value_)) {
                jn_value2 = JSON.parse(jn_value_);
            }

            if(!is_object(jn_value2)) {
                log_error(`attr must be an object: ${it.name}`);
                return -1;
            }
            break;

        case data_type_t.DTP_JSON:
            jn_value2 = jn_value_;
            break;

        case data_type_t.DTP_POINTER:
            jn_value2 = BigInt(jn_value_);
            break;
    }

    sdata[it.name] = jn_value2;

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
function gobj_hsdata(gobj)
{
    return gobj.jn_attrs;
}

/***************************************************************************
 *  Return the data description of the attribute `attr`
 *  If `attr` is null returns full attr's table
 ***************************************************************************/
function gclass_attr_desc(gclass, attr, verbose)
{
    if(!gclass) {
        if(verbose) {
            log_error(`gclass NULL`);
        }
        return null;
    }

    if(!attr) {
        return gclass.attrs_table;
    }

    for(let i=0; i < gclass.attrs_table.length; i++) {
        const it = gclass.attrs_table[i];
        if (it.name === attr) {
            return it;
        }
    }

    if(verbose) {
        log_error(`${gclass.gclass_name}: GClass Attribute NOT FOUND: ${attr}`);
    }
    return null;
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
    attrs_table,
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
        attrs_table,
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

        gclass_add_event_type(gclass, event_name, event_type);
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

/***************************************************************************
 *
 ***************************************************************************/
function _find_state(gclass, state_name)
{
    const size = gclass.dl_states.length;
    const dl = gclass.dl_states;

    for(let i=0; i<size; i++) {
        let state = dl[i];
        if(state.state_name === state_name) {
            return state;
        }
    }
    return null;
}

/************************************************************
 *
 ************************************************************/
function _find_event_action(state, event_name)
{
    const size = state.dl_actions.length;
    const dl = state.dl_actions;

    for(let i=0; i<size; i++) {
        let event_action = dl[i];
        if(event_action.event_name === event_name) {
            return event_action;
        }
    }
    return null;
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

    if(_find_state(gclass, state_name)) {
        log_error(`state already exists: ${state_name}`);
        return -1;
    }

    gclass.dl_states.push(new State(state_name));

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

    if(empty_string(state_name)) {
        log_error(`state_name EMPTY`);
        return -1;
    }

    let state = _find_state(gclass, state_name);
    if(!state) {
        log_error(`state not found: ${state_name}`);
        return -1;
    }

    if(_find_event_action(state, event_name)) {
        log_error(`event already exists: gclass ${gclass.gclass_name}, state ${state_name}, event ${event_name}`);
        return -1;
    }

    state.dl_actions.push(new Event_Action(event_name, action, next_state));

    return 0;
}

/************************************************************
 *
 ************************************************************/
function gclass_add_event_type(gclass, event_name, event_flag)
{
    if(!(gclass instanceof GClass)) {
        log_error(`Cannot add state, typeof not GClass`);
        return -1;
    }

    gclass.dl_events.push(new Event_Type(event_name, event_flag));
    return 0;
}

/************************************************************
 *
 ************************************************************/
function gclass_find_event_type(gclass, event_name)
{
    if(!(gclass instanceof GClass)) {
        log_error(`Cannot add state, typeof not GClass`);
        return 0;
    }

    const size = gclass.dl_events.length;
    const dl = gclass.dl_events;

    for(let i=0; i<size; i++) {
        let event_type = dl[i];
        if(event_type.event_name === event_name) {
            return event_type;
        }
    }

    return null;
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

    let ret = 0;

    /*
     *  check states
     */
    if(!json_array_size(gclass.dl_states)) {
        log_error(`GClass without states: ${gclass.gclass_name}`);
        ret += -1;
    }

    /*
     *  check state's event in input_list
     */
    for(let i=0; i<gclass.dl_states.length; i++) {
        let state = gclass.dl_states[i];
        // gobj_state_t state_name;
        // dl_list_t dl_actions;

        for(let j=0; j<state.dl_actions.length; j++) {
            let event_action = state.dl_actions[j];
            // gobj_event_t event;
            // gobj_action_fn action;
            // gobj_state_t next_state;
            let event_type = gclass_find_event_type(gclass, event_action.event_name);
            if(!event_type) {
                log_error(`SMachine: state's event NOT in input_events: gclass ${gclass.gclass_name}, state ${state.state_name}, event ${event_action.event_name}`);
                ret += -1;
            }

            if(event_action.next_state) {
                let next_state = _find_state(gclass, event_action.next_state);
                if(!next_state) {
                    log_error(`SMachine: next state NOT in state names: gclass ${gclass.gclass_name}, state ${state.state_name}, next_state ${event_action.next_state}`);
                    ret += -1;
                }
            }
        }
    }

    /*
     *  check input_list's event in state
     */
    for(let i=0; i<gclass.dl_events.length; i++) {
        let event_type = gclass.dl_events[i];
        // gobj_event_t event_type.event_name;
        // event_flag_t event_type.event_flag;

        if(!(event_type.event_flag & event_flag_t.EVF_OUTPUT_EVENT)) {
            let found = false;

            for(let j=0; j<gclass.dl_states.length; j++) {
                let state = gclass.dl_states[j];
                let event_action = _find_event_action(state, event_type.event_name);
                if(event_action) {
                    found = true;
                }
            }

            if(!found) {
                log_error(`SMachine: input_list's event NOT in state: gclass ${gclass.gclass_name}, event ${event_type.event_name}`);
                ret += -1;
            }
        }
    }

    return ret;
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

/************************************************************
 *        register a service
 ************************************************************/
function _register_service(gobj)
{
    let service_name = gobj.gobj_name;
    if(__jn_services__[service_name]) {
        log_error(`service ALREADY REGISTERED: ${service_name}. Will be UPDATED`);
    }
    __jn_services__[service_name] = gobj;
    return 0;
}

/************************************************************
 *        deregister a service
 ************************************************************/
function _deregister_service(gobj)
{
    let service_name = gobj.gobj_name;
    if(!__jn_services__[service_name]) {
        log_error(`"service NOT found in register": ${service_name}`);
        return -1;
    }
    delete  __jn_services__[service_name];
    return 0;
}

/************************************************************
 *        add child.
 ************************************************************/
function _add_child(parent, child)
{
    if(child.parent) {
        log_error(`_add_child() ALREADY HAS PARENT: ${child.gobj_name}`);
    }
    parent.dl_childs.push(child);
    child.parent = parent;
}

/************************************************************
 *        remove child
 ************************************************************/
function _remove_child(parent, child)
{
    let index = index_in_list(parent.dl_childs, child);
    if (index >= 0) {
        parent.dl_childs.remove(index);
        child.parent = null;
    }
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

/***************************************************************************
 *  ATTR:
 ***************************************************************************/
function print_attr_not_found(gobj, attr)
{
    if(attr !== "__json_config_variables__") {
        log_error(`GClass Attribute NOT FOUND: ${attr}`);
    }
}

/***************************************************************************
 *  Update sdata fields from a json dict.
 *  Constraint: if flag is -1
 *                  => all fields!
 *              else
 *                  => matched flag
 ***************************************************************************/
function json2sdata(
    hsdata,
    kw,  // not owned
    flag,
    not_found_cb, // Called when the key not exist in hsdata
    gobj)
{
    let ret = 0;
    if(!hsdata || !kw) {
        log_error(`hsdata or kw NULL`);
        return -1;
    }

    for (const [key, jn_value] of Object.entries(kw)) {
        const it = gclass_attr_desc(gobj.gclass, key, false);
        if(!it) {
            if(not_found_cb) {
                not_found_cb(gobj, key);
                ret--;
            }
            continue;
        }
        if(!(flag === -1 || (it.flag & flag))) {
            continue;
        }
        ret += json2item(gobj, hsdata, it, jn_value);
    }

    return ret;
}

/************************************************************
 *  ATTR:
 *  Get data from json kw parameter,
 *  and put the data in config sdata.
 ************************************************************/
function write_json_parameters(gobj, kw, jn_global)
{
    let hs = gobj_hsdata(gobj);
    let new_kw = kw; // kw_apply_json_config_variables(kw, jn_global_mine);

    let ret = json2sdata(
        hs,
        new_kw,
        -1,
        (gobj.gclass.gclass_flag & gclass_flag_t.gcflag_ignore_unknown_attrs)?0:print_attr_not_found,
        gobj
    );
    if(ret < 0) {
        log_error(`json2data() FAILED, new_kw: ${new_kw}`);
    }
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
     *      Initialize variables
     *--------------------------------*/
    let gobj = new GObj(gobj_name, gclass, kw, parent, gobj_flag);

    if(trace_creation) { // if(__trace_gobj_create_delete__(gobj))
        trace_machine(sprintf("💙💙⏩ creating: %s^%s",
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
    write_json_parameters(gobj, kw, __jn_global_settings__);

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
        _add_child(parent, gobj);
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
            trace_machine(sprintf(
                "👦👦🔵 child_added(%s): %s",
                parent.gobj_full_name(),
                gobj.gobj_short_name())
            );
        }
        parent.gclass.gmt.mt_child_added(parent, gobj);
    }

    if(trace_creation) { // if(__trace_gobj_create_delete__(gobj))
        trace_machine("💙💙⏪ created: " + gobj.gobj_full_name());
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
        return;
    }

    /*--------------------------------*
     *      Check parameters
     *--------------------------------*/
    if(gobj.__refs__ > 0) {
        log_error(`gobj DESTROYING with references: ${gobj.__refs__}`);
    }
    gobj.obflag |= obflag_t.obflag_destroying;

    if(trace_creation) { // if(__trace_gobj_create_delete__(gobj))
        trace_machine("💔💔⏩ destroying: " + gobj_full_name(gobj));
    }

    /*----------------------------------------------*
     *  Inform to parent now,
     *  when the child is NOT still full operative
     *----------------------------------------------*/
    let parent = gobj.parent;
    if(parent && parent.gclass.gmt.mt_child_removed) {
        if(trace_creation) { // if(__trace_gobj_create_delete__(gobj))
            trace_machine(sprintf("👦👦🔴 child_removed(%s): %s",
                gobj_full_name(parent),
                gobj_short_name(gobj)
            ));
        }
        parent.gclass.gmt.mt_child_removed(parent, gobj);
    }

    /*--------------------------------*
     *  Deregister if service
     *--------------------------------*/
    if(gobj.gobj_flag & gobj_flag_t.gobj_flag_service) {
        _deregister_service(gobj);
    }

    /*--------------------------------*
     *      Pause
     *--------------------------------*/
    if(gobj.playing) {
        log_error(`Destroying a PLAYING gobj: ${gobj_full_name(gobj)}`);
        gobj_pause(gobj);
    }

    /*--------------------------------*
     *      Stop
     *--------------------------------*/
    if(gobj.running) {
        log_error(`Destroying a RUNNING gobj: ${gobj_full_name(gobj)}`);
        gobj_stop(gobj);
    }

    /*--------------------------------*
     *      Delete subscriptions
     *--------------------------------*/
    // TODO gobj.gobj_unsubscribe_list(gobj.gobj_find_subscriptions(), true);

    // json_t *dl_subs;
    // dl_subs = json_copy(gobj.dl_subscriptions);
    // gobj_unsubscribe_list(dl_subs, TRUE);
    // dl_subs = json_copy(gobj.dl_subscribings);
    // gobj_unsubscribe_list(dl_subs, TRUE);

    /*--------------------------------*
     *      Delete from parent
     *--------------------------------*/
    if(gobj.parent) {
        _remove_child(gobj.parent, gobj);
        if(gobj_is_volatil(gobj)) {
            if(gobj_bottom_gobj(gobj.parent) === gobj &&
                !gobj_is_destroying(gobj.parent))
            {
                gobj_set_bottom_gobj(gobj.parent, null);
            }
        }
    }

    /*--------------------------------*
     *      Delete childs
     *--------------------------------*/
    gobj_destroy_childs(gobj);

    /*-------------------------------------------------*
     *  Exec mt_destroy
     *  Call this after all childs are destroyed.
     *  Then you can delete resources used by childs
     *  (example: event_loop in main/threads)
     *-------------------------------------------------*/
    if(gobj.obflag & obflag_t.obflag_created) {
        if(gobj.gclass.gmt.mt_destroy) {
            gobj.gclass.gmt.mt_destroy(gobj);
        }
    }

    /*--------------------------------*
     *      Mark as destroyed
     *--------------------------------*/
    if(trace_creation) { // if(__trace_gobj_create_delete__(gobj))
        trace_machine(`💔💔⏪ destroyed: ${gobj_full_name(gobj)}`);
    }
    gobj.obflag |= obflag_t.obflag_destroyed;

    /*--------------------------------*
     *      Dealloc data
     *--------------------------------*/
    if(gobj.obflag & obflag_t.obflag_created) {
        gobj.gclass.instances--;
    }



    // TODO var dl_childs = gobj.dl_childs.slice();
    //
    // for (var i=0; i < dl_childs.length; i++) {
    //     var child = dl_childs[i];
    //     if (!child._destroyed) {
    //         self.gobj_destroy(child);
    //     }
    // }
    //
    // gobj.clear_timeout();
    //

}

/************************************************************
 *
 ************************************************************/
function gobj_destroy_childs(gobj)
{
    // TODO
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

/************************************************************
 *
 ************************************************************/
function gobj_is_volatil(gobj)
{
    if(gobj.gobj_flag & gobj_flag_t.gobj_flag_volatil) {
        return true;
    }
    return false;
}

/************************************************************
 *
 ************************************************************/
function gobj_is_destroying(gobj)
{
    if(gobj.obflag & (obflag_t.obflag_destroyed|obflag_t.obflag_destroying)) {
        return true;
    }
    return false;
}

/************************************************************
 *
 ************************************************************/
function gobj_bottom_gobj(gobj)
{
    // TODO
}

/************************************************************
 *
 ************************************************************/
function gobj_set_bottom_gobj(gobj)
{
    // TODO
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
    gobj_hsdata,
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
    gobj_is_volatil,
    gobj_is_destroying,
    gobj_bottom_gobj,
    gobj_set_bottom_gobj,
};
