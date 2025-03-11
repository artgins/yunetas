/*********************************************************************************
 *  Objects with a simple Finite State Machine.
 *
 *      Licence: MIT (http://www.opensource.org/licenses/mit-license)
 *      Copyright (c) 2014,2024 Niyamaka.
 *      Copyright (c) 2025, ArtGins.
 *********************************************************************************/
/* jshint bitwise: false */

import {
    kw_flag_t,
    is_string,
    is_object,
    is_array,
    is_number,
    log_error,
    log_warning,
    empty_string,
    json_deep_copy,
    json_object_update,
    json_object_get,
    json_object_del,
    json_object_set,
    json_object_set_new,
    index_in_list,
    json_array_size,
    strcasecmp,
    kw_get_dict,
    kw_get_bool,
    kw_match_simple,
    kw_get_int,
    json_array_append,
    kw_get_dict_value,
    trace_json,
    json_array_remove,
    kw_find_json_in_list,
    kw_get_str,
    kw_has_key,
    kw_pop,
    json_is_identical,
    kw_get_pointer,
    json_object_size,
    log_debug,
    json_array_append_new,
    parseBoolean,
} from "./helpers.js";

import {sprintf} from "./sprintf.js";
import {build_command_response} from "./command_parser.js";
import {build_stats_response} from "./stats_parser.js";

/**************************************************************************
 *        Constants
 **************************************************************************/
let YUNETA_VERSION = "7.0.0a";

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

let __publish_event_match__ = kw_match_simple;

/*
 *  System (gobj) output events
 */
let dl_global_event_types = [];

let _gclass_register = {};
let __jn_services__ = {};

let __yuno__ = null;
let __default_service__ = null;
let __inside__ = 0;

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
        this.parent = null;     // assign in _add_child()
        this.dl_childs = [];
        this.dl_subscriptions = []; // subscriptions of this gobj to events of others gobj.
        this.dl_subscribings = [];  // subscribed events
        this.current_state = gclass.dl_states[0];
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

const walk_type_t = Object.freeze({
    /*
     * One must be selected
     */
    WALK_TOP2BOTTOM : 0x0001,
    WALK_BOTTOM2TOP : 0x0002,
    WALK_BYLEVEL    : 0x0004,

    /*
     * One must be selected when WALK_BYLEVEL is selected.
     */
    WALK_FIRST2LAST : 0x0010,
    WALK_LAST2FIRST : 0x0020,
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

function tab()
{
    let bf = '';
    for(let i=1; i<__inside__*2; i++) {
        bf += ' ';
    }
    return bf;
}

/************************************************************
 *
 ************************************************************/
function trace_machine(msg)
{
    if(is_object(msg)) {
        msg = JSON.stringify(msg);
    }
    log_debug(tab() + String(msg), "color:lime");
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

    /*----------------------------------------*
     *          Build Global Events
     *----------------------------------------*/
    const global_events = [
        ["EV_STATE_CHANGED",    event_flag_t.EVF_SYSTEM_EVENT|
                                event_flag_t.EVF_OUTPUT_EVENT|
                                event_flag_t.EVF_NO_WARN_SUBS],
        [0, 0]
    ];

    for (let i = 0; i < global_events.length; i++) {
        let event_name = global_events[i][0];
        let event_flag = global_events[i][1];

        _add_event_type(dl_global_event_types, event_name, event_flag);
    }


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
        if(!it.name) {
            continue;
        }

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

    if(!is_array(sdata_desc)) {
        log_error(`sdata_desc MUST be an array`);
        trace_json(sdata_desc);
        return sdata;
    }
    for(let i=0; i < sdata_desc.length; i++) {
        const it = sdata_desc[i];
        if(!it.name) {
            continue;
        }
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
            jn_value = parseBoolean(svalue);
            break;
        case data_type_t.DTP_INTEGER:
            jn_value = parseInt(svalue);
            if(isNaN(jn_value)) {
                log_error(`${gobj_short_name(gobj)}: attr NaN: ${it.name}`);
            }
            break;
        case data_type_t.DTP_REAL:
            jn_value = Number(svalue);
            if(isNaN(jn_value)) {
                log_error(`${gobj_short_name(gobj)}: attr NaN: ${it.name}`);
            }
            break;
        case data_type_t.DTP_LIST:
            if(is_string(svalue)) {
                jn_value = JSON.parse(svalue);
            } else {
                jn_value = svalue;
            }
            if(!is_array(jn_value)) {
                jn_value = [];
            }
            break;
        case data_type_t.DTP_DICT:
            if(is_string(svalue)) {
                jn_value = JSON.parse(svalue);
            } else {
                jn_value = svalue;
            }
            if(!is_object(jn_value)) {
                jn_value = {};
            }
            break;
        case data_type_t.DTP_JSON:
            if(is_string(svalue)) {
                jn_value = JSON.parse(svalue);
            } else {
                jn_value = svalue;
            }
            break;
        case data_type_t.DTP_POINTER:
            jn_value = svalue;
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
            jn_value2 = parseBoolean(jn_value_);
            break;
        case data_type_t.DTP_INTEGER:
            jn_value2 = parseInt(jn_value_);
            if(isNaN(jn_value2)) {
                log_error(`${gobj_short_name(gobj)}: attr NaN: ${it.name}`);
            }
            break;
        case data_type_t.DTP_REAL:
            jn_value2 = Number(jn_value_);
            if(isNaN(jn_value2)) {
                log_error(`${gobj_short_name(gobj)}: attr NaN: ${it.name}`);
            }
            break;
        case data_type_t.DTP_LIST:
            if(is_string(jn_value_)) {
                jn_value2 = JSON.parse(jn_value_);
            } else {
                jn_value2 = jn_value_;
            }
            if(!is_array(jn_value2)) {
                log_error(`attr must be an array: ${it.name}`);
                return -1;
            }
            break;

        case data_type_t.DTP_DICT:
            if(is_string(jn_value_)) {
                jn_value2 = JSON.parse(jn_value_);
            } else {
                jn_value2 = jn_value_;
            }
            if(!is_array(jn_value2)) {
                log_error(`attr must be an array: ${it.name}`);
                return -1;
            }
            if(!is_object(jn_value2)) {
                log_error(`attr must be an object: ${it.name}`);
                return -1;
            }
            break;

        case data_type_t.DTP_JSON:
            if(is_string(jn_value_)) {
                jn_value2 = JSON.parse(jn_value_);
            } else {
                jn_value2 = jn_value_;
            }
            if(!is_array(jn_value2) && !is_object(jn_value2)) {
                log_error(`attr must be a json dict/list: ${it.name}`);
                return -1;
            }
            break;

        case data_type_t.DTP_POINTER:
            jn_value2 = jn_value_;
            break;
    }

    if(jn_value2 === undefined) {
        log_error(`${gobj_short_name(gobj)}: attr undefined: ${it.name}`);
    }

    sdata[it.name] = jn_value2;

    if(gobj.gclass.gmt.mt_writing) {
        if((gobj.obflag & obflag_t.obflag_created) && !(gobj.obflag & obflag_t.obflag_destroyed)) {
            // Avoid call to mt_writing before mt_create!
            gobj.gclass.gmt.mt_writing(gobj, it.name);
        }
    }

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
 *
 ***************************************************************************/
function gobj_hsdata2(gobj, name, verbose)
{
    if(gobj_has_attr(gobj, name)) {
        return gobj_hsdata(gobj);
    } else if(gobj && gobj.bottom_gobj) {
        return gobj_hsdata2(gobj.bottom_gobj, name, verbose);
    }
    if(verbose) {
        log_error(`GClass Attribute NOT FOUND: ${gobj_short_name(gobj)}, attr ${name}`);
    }
    return 0;
}

/***************************************************************************
 *  Return the data description of the attribute `attr`
 *  If `attr` is null returns full attr's table
 ***************************************************************************/
function gclass_attr_desc(gclass, attr, verbose)
{
    if(!gclass || !(gclass instanceof GClass)) {
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
        if (it.name && it.name === attr) {
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

        if(empty_string(state_name)) {
            continue;
        }
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

            if(empty_string(event_name)) {
                continue;
            }
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
    for (let i = 0; i < event_types.length; i++) {
        let event_name = event_types[i][0];
        let event_type = event_types[i][1];

        if(empty_string(event_name)) {
            continue;
        }
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
        if(state.state_name && state.state_name === state_name) {
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
        if(event_action.event_name && event_action.event_name === event_name) {
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
function _add_event_type(dl, event_name, event_flag)
{
    if(!empty_string(event_name)) {
        dl.push(new Event_Type(event_name, event_flag));
    }
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

    return _add_event_type(gclass.dl_events, event_name, event_flag);
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
        if(event_type.event_name && event_type.event_name === event_name) {
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
        if(empty_string(state.state_name)) {
            continue;
        }
        // gobj_state_t state_name;
        // dl_list_t dl_actions;

        for(let j=0; j<state.dl_actions.length; j++) {
            let event_action = state.dl_actions[j];
            if(empty_string(event_action.event_name)) {
                continue;
            }
            // gobj_event_t event_name;
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
        if(empty_string(event_type.event_name)) {
            continue;
        }
        // gobj_event_t event_type.event_name;
        // event_flag_t event_type.event_flag;
        if(!is_number(event_type.event_flag)) {
            log_error(`SMachine: event flag is NOT number: gclass ${gclass.gclass_name}, event ${event_type.event_name}`);
            ret += -1;
        }
        if(!(event_type.event_flag & event_flag_t.EVF_OUTPUT_EVENT)) {
            let found = false;

            for(let j=0; j<gclass.dl_states.length; j++) {
                let state = gclass.dl_states[j];
                if(empty_string(state.state_name)) {
                    continue;
                }
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

/***************************************************************************
 *
 ***************************************************************************/
function gobj_services()
{
    let jn_register = [];

    for(const key of Object.keys(__jn_services__)) {
        json_array_append_new(jn_register, key);
    }

    return jn_register;
}

/************************************************************
 *
 ************************************************************/
function gobj_default_service()
{
    if(!__default_service__ || (__default_service__.obflag & obflag_t.obflag_destroyed)) {
        return null;
    }
    return __default_service__;
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
    let service_name = gobj_name(gobj);
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
    let service_name = gobj_name(gobj);
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
        json_array_remove(parent.dl_childs, index);
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
 *  Only gobj services can load/save persistent attrs.
 *  WARNING: persistent attrs are loaded automatically in the creation of gobjs
 ************************************************************/
function gobj_load_persistent_attrs(
    gobj,
    keys  // str, list or dict.
) {
    if(!gobj_is_service(gobj)) {
        log_error(`${gobj_short_name(gobj)}: Only gobj services can load/save writable-persistent`);
        return -1;
    }
    if(__global_load_persistent_attrs_fn__) {
        return __global_load_persistent_attrs_fn__(gobj, keys);
    }
    return -1;
}

/************************************************************
 *  Only gobj services can load/save persistent attrs.
 ************************************************************/
function gobj_save_persistent_attrs(
    gobj,
    keys  // str, list or dict.
) {
    if(!gobj_is_service(gobj)) {
        log_error(`${gobj_short_name(gobj)}: Only gobj services can load/save writable-persistent`);
        return -1;
    }
    if(__global_save_persistent_attrs_fn__) {
        return __global_save_persistent_attrs_fn__(gobj, keys);
    }
    return -1;
}

/************************************************************
 *  Only gobj services can load/save persistent attrs.
 ************************************************************/
function gobj_remove_persistent_attrs(
    gobj,
    keys  // str, list or dict.
) {

    if(!gobj_is_service(gobj)) {
        log_error(`${gobj_short_name(gobj)}: Only gobj services can load/save writable-persistent`);
        return -1;
    }
    if(!__global_remove_persistent_attrs_fn__) {
        return -1;
    }
    return __global_remove_persistent_attrs_fn__(gobj, keys);
}

/************************************************************
 *
 ************************************************************/
function gobj_list_persistent_attrs(gobj, keys)
{
    if(!__global_list_persistent_attrs_fn__) {
        return null;
    }
    return __global_list_persistent_attrs_fn__(gobj, keys);
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
function print_attr_not_found(gobj, name)
{
    if(name !== "__json_config_variables__") {
        log_error(`GClass Attribute NOT FOUND: ${gobj_short_name(gobj)}, attr ${name}`);
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
        log_error(`${gobj_short_name(gobj)} json2data() FAILED`);
        trace_json(new_kw);
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
        if(__default_service__) {
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

    let trace_creation = __yuno__ && gobj_read_bool_attr(__yuno__, "trace_creation");
    if(trace_creation) { // if(__trace_gobj_create_delete__(gobj))
        trace_machine(sprintf("💙💙⏩ creating: %s^%s",
            gclass.gclass_name, gobj_name
        ));
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
     *     Mark as created
     *--------------------------------*/
    gobj.obflag |= obflag_t.obflag_created;
    gobj.gclass.instances++;

    /*--------------------------------*
     *  Write configuration
     *  WARNING collateral damage:
     *      write_json_parameters was before Mark as created!
     *--------------------------------*/
    write_json_parameters(gobj, kw, __jn_global_settings__);

    /*--------------------------------------*
     *  Load writable and persistent attrs
     *  of services and __root__
     *--------------------------------------*/
    if(gobj.gobj_flag & (gobj_flag_t.gobj_flag_service)) {
        if(__global_load_persistent_attrs_fn__) {
            __global_load_persistent_attrs_fn__(gobj, null);
        }
    }

    /*--------------------------------*
     *      Exec mt_create
     *--------------------------------*/
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
                gobj_short_name(gobj)
            ));
        }
        parent.gclass.gmt.mt_child_added(parent, gobj);
    }

    if(trace_creation) { // if(__trace_gobj_create_delete__(gobj))
        trace_machine(sprintf("💙💙⏪ created: %s", gobj_full_name(gobj)));
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
    if(gobj_is_destroying(gobj)) {
        log_error("gobj NULL or DESTROYED");
        return;
    }

    /*--------------------------------*
     *      Check parameters
     *--------------------------------*/
    if(gobj.__refs__ > 0) {
        log_error(`${gobj_short_name(gobj)}: gobj DESTROYING with references: ${gobj.__refs__}`);
    }
    gobj.obflag |= obflag_t.obflag_destroying;

    let trace_creation = __yuno__ && gobj_read_bool_attr(__yuno__, "trace_creation");
    if(trace_creation) { // if(__trace_gobj_create_delete__(gobj))
        trace_machine(sprintf("💔💔⏩ destroying: %s", gobj_full_name(gobj)));
    }

    /*----------------------------------------------*
     *  Inform to parent now,
     *  when the child is NOT still full operative
     *----------------------------------------------*/
    let parent = gobj_parent(gobj);
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
    gobj_unsubscribe_list(gobj.dl_subscriptions, true);
    gobj_unsubscribe_list(gobj.dl_subscribings, true);

    /*--------------------------------*
     *      Delete from parent
     *--------------------------------*/
    if(parent) {
        _remove_child(parent, gobj);
        if(gobj_is_volatil(gobj)) {
            if(gobj_bottom_gobj(parent) === gobj &&
                !gobj_is_destroying(parent))
            {
                gobj_set_bottom_gobj(parent, null);
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
        trace_machine(sprintf("💔💔⏪ destroyed: %s", gobj_full_name(gobj)));
    }
    gobj.obflag |= obflag_t.obflag_destroyed;

    /*--------------------------------*
     *      Dealloc data
     *--------------------------------*/
    if(gobj.obflag & obflag_t.obflag_created) {
        gobj.gclass.instances--;
    }
}

/************************************************************
 *
 ************************************************************/
function gobj_destroy_childs(gobj)
{
    let dl_childs = gobj.dl_childs.slice();

    for(let i=0; i < dl_childs.length; i++) {
        let child = dl_childs[i];
        if (!gobj_is_destroying(child)) {
            gobj_destroy(child);
        }
    }
}

/************************************************************
 *
 ************************************************************/
function gobj_start(gobj)
{
    if(gobj_is_destroying(gobj)) {
        log_error("gobj NULL or DESTROYED");
        return -1;
    }
    if(gobj.running) {
        log_error(`${gobj_short_name(gobj)}: GObj ALREADY RUNNING`);
        return -1;
    }
    if(gobj.disabled) {
        log_error(`${gobj_short_name(gobj)}: GObj DISABLED`);
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

    //if(__trace_gobj_start_stop__(gobj)) {
    let tracea = __yuno__ && gobj_read_bool_attr(__yuno__, "trace_start_stop");
    if(tracea) {
        trace_machine(sprintf("⏺ ⏺ start: %s",
            gobj_full_name(gobj)
        ));
    }

    gobj.running = true;

    let ret = 0;
    if(gobj.gclass.gmt.mt_start) {
        ret = gobj.gclass.gmt.mt_start(gobj);
    }
    return ret;
}

/***************************************************************************
 *  Start all childs of the gobj.
 ***************************************************************************/
function cb_start_child(child, user_data, user_data2)
{
    if(child.gclass.gclass_flag & gclass_flag_t.gcflag_manual_start) {
        return 0;
    }
    if(!gobj_is_destroying(child) && !gobj_is_running(child) && !gobj_is_disabled(child)) {
        gobj_start(child);
    }
    return 0;
}
function gobj_start_childs(gobj)
{
    if(gobj_is_destroying(gobj)) {
        log_error("gobj NULL or DESTROYED");
        return -1;
    }

    return gobj_walk_gobj_childs(gobj, walk_type_t.WALK_FIRST2LAST, cb_start_child, 0, 0);
}

/***************************************************************************
 *  Start this gobj and all childs tree of the gobj.
 ***************************************************************************/
function cb_start_child_tree(child, user_data, user_data2)
{
    if(child.gclass.gclass_flag & gclass_flag_t.gcflag_manual_start) {
        return 1;
    }
    if(gobj_is_disabled(child)) {
        return 1;
    }
    if(!gobj_is_running(child)) {
        gobj_start(child);
    }
    return 0;
}
function gobj_start_tree(gobj)
{
    if(gobj_is_destroying(gobj)) {
        log_error("gobj NULL or DESTROYED");
        return -1;
    }

    // if(__trace_gobj_start_stop__(gobj)) {
    let tracea = __yuno__ && gobj_read_bool_attr(__yuno__, "trace_start_stop");
    if(tracea) {
        trace_machine(sprintf("⏺ ⏺ ⏺ ⏺ start_tree: %s",
            gobj_full_name(gobj)
        ));
    }

    if((gobj.gclass.gclass_flag & gclass_flag_t.gcflag_manual_start)) {
        return 0;
    } else if(gobj.disabled) {
        return 0;
    } else {
        if(!gobj.running) {
            gobj_start(gobj);
        }
    }
    return gobj_walk_gobj_childs_tree(gobj, walk_type_t.WALK_TOP2BOTTOM, cb_start_child_tree, 0, 0);
}

/************************************************************
 *
 ************************************************************/
function gobj_stop(gobj)
{
    if(gobj_is_destroying(gobj)) {
        log_error("gobj NULL or DESTROYED");
        return -1;
    }
    if(!gobj.running) {
        log_error(`${gobj_short_name(gobj)}: GObj NOT RUNNING`);
        return -1;
    }
    if(gobj.playing) {
        // It's auto-stopping but display error (magic but warn!).
        log_warning(`${gobj_short_name(gobj)}: GObj stopping without previous pause`);
        gobj_pause(gobj);
    }

    // if(__trace_gobj_start_stop__(gobj)) {
    let tracea = __yuno__ && gobj_read_bool_attr(__yuno__, "trace_start_stop");
    if(tracea) {
        trace_machine(sprintf("⏹ ⏹ stop: %s",
            gobj_full_name(gobj)
        ));
    }

    gobj.running = false;

    let ret = 0;
    if(gobj.gclass.gmt.mt_stop) {
        ret = gobj.gclass.gmt.mt_stop(gobj);
    }

    return ret;
}

/***************************************************************************
 *  Stop all childs of the gobj.
 ***************************************************************************/
function cb_stop_child(child, user_data, user_data2)
{
    if(gobj_is_running(child)) {
        gobj_stop(child);
    }
    return 0;
}
function gobj_stop_childs(gobj)
{
    if(gobj_is_destroying(gobj)) {
        log_error("gobj NULL or DESTROYED");
        return -1;
    }

    return gobj_walk_gobj_childs(gobj, walk_type_t.WALK_FIRST2LAST, cb_stop_child, 0, 0);
}

/***************************************************************************
 *  Stop this gobj and all childs tree of the gobj.
 ***************************************************************************/
function gobj_stop_tree(gobj)
{
    if(gobj_is_destroying(gobj)) {
        log_error("gobj NULL or DESTROYED");
        return -1;
    }

    let tracea = __yuno__ && gobj_read_bool_attr(__yuno__, "trace_start_stop");
    // if(__trace_gobj_start_stop__(gobj)) {
    if(tracea) {
        trace_machine(sprintf("⏹ ⏹ ⏹ ⏹ stop_tree: %s",
            gobj_full_name(gobj)
        ));
    }

    if(gobj_is_running(gobj)) {
        gobj_stop(gobj);
    }
    return gobj_walk_gobj_childs_tree(gobj, walk_type_t.WALK_TOP2BOTTOM, cb_stop_child, 0, 0);
}

/************************************************************
 *
 ************************************************************/
function gobj_play(gobj)
{
    if(gobj_is_destroying(gobj)) {
        log_error(`gobj NULL or DESTROYED: ${gobj_short_name(gobj)}`);
        return -1;
    }
    if(gobj.playing) {
        log_error(`${gobj_short_name(gobj)}: GObj ALREADY PLAYING`);
        return -1;
    }
    if(gobj.disabled) {
        log_error(`${gobj_short_name(gobj)}: GObj DISABLED`);
        return -1;
    }
    if(!gobj_is_running(gobj)) {
        if(!(gobj.gclass.gclass_flag & gclass_flag_t.gcflag_required_start_to_play)) {
            // Default: It's auto-starting but display error (magic but warn!).
            log_warning(`${gobj_short_name(gobj)}: GObj playing without previous start`);
            gobj_start(gobj);
        } else {
            log_error(`${gobj_short_name(gobj)}: Cannot play, start not done`);
            return -1;
        }
    }

    let tracea = __yuno__ && gobj_read_bool_attr(__yuno__, "trace_start_stop");
    if(tracea) {
    // if(__trace_gobj_start_stop__(gobj)) {
        trace_machine(sprintf("⏯ ⏯ play: %s",
            gobj_full_name(gobj)
        ));
    }

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
    if(gobj_is_destroying(gobj)) {
        log_error("gobj NULL or DESTROYED");
        return -1;
    }
    if(!gobj.playing) {
        log_error(`${gobj_short_name(gobj)}: GObj NOT PLAYING`);
        return -1;
    }

    let tracea = __yuno__ && gobj_read_bool_attr(__yuno__, "trace_start_stop");
    if(tracea) {
    // if(__trace_gobj_start_stop__(gobj)) {
        trace_machine(sprintf("⏸ ⏸ pause: %s",
            gobj_full_name(gobj)
        ));
    }

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
    if(gobj_is_destroying(gobj)) {
        return false;
    }
    return gobj.running;
}

/************************************************************
 *
 ************************************************************/
function gobj_is_disabled(gobj)
{
    if(gobj_is_destroying(gobj)) {
        return true;
    }
    return gobj.disabled;
}

/************************************************************
 *
 ************************************************************/
function gobj_is_service(gobj)
{
    if(gobj && (gobj.gobj_flag & (gobj_flag_t.gobj_flag_service))) {
        return true;
    } else {
        return false;
    }
}

/************************************************************
 *
 ************************************************************/
function gobj_is_playing(gobj)
{
    if(gobj_is_destroying(gobj)) {
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

/************************************************************
 *
 ************************************************************/
function gobj_yuno_name()
{
    let yuno = gobj_yuno();
    if(!yuno) {
        return "";
    }
    return gobj_name(yuno);
}

/************************************************************
 *
 ************************************************************/
function gobj_yuno_role()
{
    let yuno = gobj_yuno();
    if(!yuno) {
        return "";
    }
    return gobj_read_str_attr(yuno, "yuno_role");
}

/************************************************************
 *
 ************************************************************/
function gobj_yuno_id()
{
    let yuno = gobj_yuno();
    if(!yuno) {
        return "";
    }
    return gobj_read_str_attr(yuno, "yuno_id");
}

/***************************************************************************
 *  Return name
 ***************************************************************************/
function gobj_name(gobj)
{
    if(!gobj || !(gobj instanceof GObj)) {
        log_error(`gobj NULL of bad type`);
        return null;
    }

    return gobj.gobj_name;
}

/***************************************************************************
 *  Return mach name. Same as gclass name.
 ***************************************************************************/
function gobj_gclass_name(gobj)
{
    if(!gobj || !(gobj instanceof GObj)) {
        log_error(`gobj NULL of bad type`);
        return null;
    }

    return gobj.gclass.gclass_name;
}

/************************************************************
 *      get short name (gclass^name)
 ************************************************************/
function gobj_short_name(gobj)
{
    if(!gobj || !(gobj instanceof GObj)) {
        log_error(`gobj NULL of bad type`);
        return null;
    }

    return gobj.gclass.gclass_name + '^' + gobj_name(gobj);
}

/************************************************************
 *      Get full name
 ************************************************************/
function gobj_full_name(gobj)
{
    if(!gobj || !(gobj instanceof GObj)) {
        log_error(`gobj NULL of bad type`);
        return null;
    }

    let full_name = gobj_short_name(gobj);
    let parent = gobj_parent(gobj);
    while(parent) {
        let prefix = gobj_short_name(parent);
        full_name = prefix + '`' + full_name;
        parent = gobj_parent(parent);
    }
    return full_name;
}

/************************************************************
 *
 ************************************************************/
function gobj_parent(gobj)
{
    if(!gobj || !(gobj instanceof GObj)) {
        log_error(`gobj NULL of bad type`);
        return null;
    }

    return gobj.parent;
}

/************************************************************
 *
 ************************************************************/
function gobj_is_volatil(gobj)
{
    if(!gobj || !(gobj instanceof GObj)) {
        log_error(`gobj NULL of bad type`);
        return null;
    }

    if(gobj.gobj_flag & gobj_flag_t.gobj_flag_volatil) {
        return true;
    }
    return false;
}

/***************************************************************************
 *
 ***************************************************************************/
function gobj_is_pure_child(gobj)
{
    if(!gobj || !(gobj instanceof GObj)) {
        log_error(`gobj NULL of bad type`);
        return null;
    }

    if(gobj.gobj_flag & gobj_flag_t.gobj_flag_pure_child) {
        return true;
    }
    return false;
}

/************************************************************
 *
 ************************************************************/
function gobj_is_destroying(gobj)
{
    if(!gobj) {
        return true;
    }
    if(!(gobj instanceof GObj)) {
        return true;
    }

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
    if(!gobj || !(gobj instanceof GObj)) {
        log_error(`gobj NULL of bad type`);
        return null;
    }
    return gobj.bottom_gobj;
}

/************************************************************
 *
 ************************************************************/
function gobj_set_bottom_gobj(gobj, bottom_gobj)
{
    if(!gobj || !(gobj instanceof GObj)) {
        log_error(`gobj NULL of bad type`);
        return null;
    }

    if(gobj.bottom_gobj) {
        log_warning(`${gobj_short_name(gobj)}: bottom_gobj already set`);
    }
    gobj.bottom_gobj = bottom_gobj;
}

/***************************************************************************
 *  Return the data description of the attribute `attr`
 *  If `attr` is null returns full attr's table
 ***************************************************************************/
function gobj_attr_desc(gobj, attr, verbose)
{
    if(!gobj || !(gobj instanceof GObj)) {
        log_error(`gobj NULL of bad type`);
        return null;
    }

    if(!attr) {
        return gobj.gclass.attrs_table;
    }

    return gclass_attr_desc(gobj.gclass, attr, verbose);
}

/***************************************************************************
 *
 ***************************************************************************/
function gobj_walk_gobj_childs(
    gobj,
    walk_type,  // walk_type_t
    cb_walking,
    user_data,
    user_data2
) {
    if(gobj_is_destroying(gobj)) {
        log_error(`gobj NULL, destroying, or bad type`);
        return 0;
    }

    return rc_walk_by_list(gobj.dl_childs, walk_type, cb_walking, user_data, user_data2);
}

/***************************************************************************
 *
 ***************************************************************************/
function gobj_walk_gobj_childs_tree(
    gobj,
    walk_type,  // walk_type_t
    cb_walking,
    user_data,
    user_data2
) {
    if(gobj_is_destroying(gobj)) {
        log_error(`gobj NULL, destroying, or bad type`);
        return 0;
    }

    return rc_walk_by_tree(gobj.dl_childs, walk_type, cb_walking, user_data, user_data2);
}

/***************************************************************
 *  Walking
 ***************************************************************/
function rc_walk_by_list(
    iter_,              // dl_list_t
    walk_type,          // walk_type_t
    cb_walking,         // cb_walking_t
    user_data,
    user_data2
) {
    /*
     *  This let destroy the current item
     */
    let iter = [...iter_];

    let child;
    if(walk_type & walk_type_t.WALK_LAST2FIRST) {
        for(let i=iter.length; i>0; i--) {
            child = iter[i-1];
            let ret = (cb_walking)(child, user_data, user_data2);
            if(ret < 0) {
                return ret;
            }
        }
    } else {
        for(let i=0; i<iter.length; i++) {
            child = iter[i];
            let ret = (cb_walking)(child, user_data, user_data2);
            if(ret < 0) {
                return ret;
            }
        }
    }

    return 0;
}

/***************************************************************
 *  Walking
 *  If cb_walking return negative, the iter will stop.
 *  Valid options:
 *      WALK_BYLEVEL:
 *          WALK_FIRST2LAST
 *          or
 *          WALK_LAST2FIRST
 ***************************************************************/
function rc_walk_by_level(
    iter,              // dl_list_t
    walk_type,          // walk_type_t
    cb_walking,         // cb_walking_t
    user_data,
    user_data2
) {
    /*
     *  First my childs
     */
    let ret = rc_walk_by_list(iter, walk_type, cb_walking, user_data, user_data2);
    if(ret < 0) {
        return ret;
    }

    /*
     *  Now child's childs
     */
    let child;
    if(walk_type & walk_type_t.WALK_LAST2FIRST) {
        for(let i=iter.length; i>0; i--) {
            child = iter[i-1];

            let dl_child_list = [...child.dl_childs];
            ret = rc_walk_by_level(dl_child_list, walk_type, cb_walking, user_data, user_data2);
            if(ret < 0) {
                return ret;
            }
        }
    } else {
        for(let i=0; i<iter.length; i++) {
            child = iter[i];

            let dl_child_list = [...child.dl_childs];
            ret = rc_walk_by_level(dl_child_list, walk_type, cb_walking, user_data, user_data2);
            if(ret < 0) {
                return ret;
            }
        }
    }

    return ret;
}

/***************************************************************
 *  Walking
 *  If cb_walking return negative, the iter will stop.
 *                return 0, continue
 *                return positive, step current branch (only in WALK_TOP2BOTTOM)
 *  Valid options:
 *      WALK_TOP2BOTTOM
 *      or
 *      WALK_BOTTOM2TOP
 *      or
 *      WALK_BYLEVEL:
 *          WALK_FIRST2LAST
 *          or
 *          WALK_LAST2FIRST
 ***************************************************************/
function rc_walk_by_tree(
    iter,              // dl_list_t
    walk_type,          // walk_type_t
    cb_walking,         // cb_walking_t
    user_data,
    user_data2
) {
    if(walk_type & walk_type_t.WALK_BYLEVEL) {
        return rc_walk_by_level(iter, walk_type, cb_walking, user_data, user_data2);
    }

    let ret;
    let child;
    if(walk_type & walk_type_t.WALK_BOTTOM2TOP) {
        for(let i=iter.length; i>0; i--) {
            child = iter[i-1];

            let dl_child_list = [...child.dl_childs];
            ret = rc_walk_by_tree(dl_child_list, walk_type, cb_walking, user_data, user_data2);
            if(ret < 0) {
                return ret;
            }

            ret = (cb_walking)(child, user_data, user_data2);
            if(ret < 0) {
                return ret;
            }

        }
    } else {
        for(let i=0; i<iter.length; i++) {
            child = iter[i];

            ret = (cb_walking)(child, user_data, user_data2);
            if(ret < 0) {
                return ret;
            } else if(ret === 0) {
                let dl_child_list = [...child.dl_childs];
                ret = rc_walk_by_tree(dl_child_list, walk_type, cb_walking, user_data, user_data2);
                if(ret < 0) {
                    return ret;
                }
            } else {
                // positive, continue next
            }
        }
    }

    return 0;
}

/************************************************************
 *
 ************************************************************/
function its_me(gobj, shortname)
{
    let n = shortname.split("^");
    let gobj_name_ =null;
    let gclass_name_ = null;
    if(n.length === 2) {
        gclass_name_ = n[0];
        gobj_name_ = n[1];
        if(gclass_name_ !== gobj_gclass_name(gobj)) {
            return false;
        }
    } else if(n.length === 1) {
        gobj_name_ = n[0];
    } else {
        return false;
    }
    return gobj_name_ === gobj_name(gobj);
}

/************************************************************
 *
 ************************************************************/
function gobj_find_gobj(gobj, path)
{
    if(empty_string(path)) {
        return null;
    }
    /*
     *  WARNING code repeated
     */
    const path_ = path.toLowerCase();
    if(path_ === "__default_service__") {
        return __default_service__;
    }
    if(path_ === "__yuno__" || path_ === "__root__") {
        return __yuno__;
    }

    return gobj_search_path(__yuno__, path);
}

/***************************************************************************
 *  Match child.
 ***************************************************************************/
function gobj_match_gobj(gobj, jn_filter)
{
    if(!gobj || !(gobj instanceof GObj)) {
        log_error(`gobj NULL of bad type`);
        return false;
    }

    let __gclass_name__ = jn_filter.__gclass_name__;
    let __gobj_name__ = jn_filter.__gobj_name__;
    let __prefix_gobj_name__ = jn_filter.__prefix_gobj_name__;
    let __disabled__ = jn_filter.__disabled__;
    let __state__ = jn_filter.__state__;

    /*
     *  Check the system keys of the jn_filter used in find loop
     */
    if(__gclass_name__) {
        if(__gclass_name__ !== gobj_gclass_name(gobj)) {
            return false;
        }
    }
    if(__gobj_name__) {
        if(__gobj_name__ !== gobj_name(gobj)) {
            return false;
        }
    }
    if(__prefix_gobj_name__) {
        let l = __prefix_gobj_name__.length;
        let name = gobj_name(gobj).substring(0, l);
        if(__prefix_gobj_name__ !== name) {
            return false;
        }
    }
    if(__state__) {
        let state = gobj_current_state(gobj);
        if(__state__ !== state) {
            return false;
        }
    }

    let jn_attrs = gobj.jn_attrs;

    for(const key of Object.keys(jn_filter)) {
        if(key === "__gclass_name__" ||
                key === "__gobj_name__" ||
                key === "__prefix_gobj_name__" ||
                key === "__disabled__" ||
                key === "__state__") {
            continue;
        }
        if(gobj_has_attr(gobj, key)) {
            if(jn_attrs[key] !== jn_filter[key]) {
                return false;
            }
        }
    }
    return true;
}

/***************************************************************************
 *  Returns the first matched child.
 ***************************************************************************/
function gobj_find_child(gobj, jn_filter)
{
    if(gobj_is_destroying(gobj)) {
        log_error("gobj NULL or DESTROYED");
        return null;
    }

    const dl_childs = gobj.dl_childs;
    for(let i=0; i < dl_childs.length; i++) {
        const child = dl_childs[i];
        if(gobj_match_gobj(child, jn_filter)) {
            return child;
        }
    }

    return null;
}

/************************************************************
 *  Returns the first matched child searching in the tree.
 *  TODO not exist in C
 ************************************************************/
function gobj_find_child_by_tree(gobj, jn_filter)
{
    jn_filter = jn_filter || {};

    function _walk_child_tree(d, o)
    {
        let dl_childs = o.dl_childs;
        for (let i=0; i < dl_childs.length; i++) {
            let child = dl_childs[i];
            if(gobj_match_gobj(child, jn_filter)) {
                d.push(child);
            }
            _walk_child_tree(d, child);
        }
    }

    let list = [];
    _walk_child_tree(list, gobj);
    if(list.length > 0) {
        return list[0];
    }
    return null;
}

/***************************************************************************
 *  Return the object searched by path.
 *  The separator of tree's gobj must be '`'
 ***************************************************************************/
function gobj_search_path(gobj, path)
{
    if(!path) {
        return null;
    }
    /*
     *  Get node and compare with this
     */
    let p = path.split("`");
    let shortname = p[0];
    if(!its_me(gobj, shortname)) {
        return null;
    }
    if(p.length===1) {
        // No more nodes
        return gobj;
    }

    /*
     *  Get next node and compare with childs
     */
    let n = p[1];
    let nn = n.split("^");
    let filter = {};
    if(nn.length === 1) {
        filter.__gobj_name__ = nn;
    } else {
        filter.__gclass_name__ = nn[0];
        filter.__gobj_name__ = nn[1];
    }

    /*
     *  Search in childs
     */
    let child = gobj_find_child(gobj, filter);
    if(!child) {
        return null;
    }
    p.splice(0, 1);
    p = p.join("`");
    return gobj_search_path(child, p);
}

/***************************************************************************
 *
 ***************************************************************************/
function gobj_has_attr(gobj, name)
{
    if(gobj_is_destroying(gobj)) {
        return false; // WARNING must be a silence function!
    }
    if(empty_string(name)) {
        return false; // WARNING must be a silence function!
    }
    return !!gclass_attr_desc(gobj.gclass, name, false);
}

/************************************************************
 *
 ************************************************************/
function gobj_read_attr(gobj, name, src)
{
    let jn_attrs = gobj.jn_attrs;

    // TODO implement inherited attributes
    // TODO if attribute not found then find in bottom gobj
    if(name in jn_attrs) {
        if(jn_attrs.hasOwnProperty(name)) {
            // TODO must be a item2json, to call mt_reading
            let value = jn_attrs[name];
            if(gobj.gclass.gmt.mt_reading) {
                return gobj.gclass.gmt.mt_reading(name, value);
            }
            return value;
        }
    }
    return null;
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
function gobj_read_attrs(
    gobj,
    include_flag, // sdata_flag_t
    src
) {
    let jn_attrs = {};

    let sdata_desc = gobj.gclass.attrs_table;

    for(let i=0; i < sdata_desc.length; i++) {
        const it = sdata_desc[i];
        if(!it.name) {
            continue;
        }

        if(include_flag === -1 || (it.flag & include_flag)) {
            // TODO must be a item2json, to call mt_reading
            let jn = json_object_get(gobj.jn_attrs, it.name);
            json_object_set(jn_attrs, it.name, jn);
        }
    }

    return jn_attrs;
}

/************************************************************
 *
 ************************************************************/
function gobj_write_attr(
    gobj,
    path, // If it has ` then segments are gobj and leaf is the attribute (+bottom)
    value,
    src
) {
    // TODO implement inherited attributes
    // TODO if attribute not found then find in bottom gobj

    if(!is_string(path)) {
        log_error(sprintf(
            "gobj_write_attr(%s): path must be a string",
            gobj_short_name(gobj))
        );
        return -1;
    }
    let jn_attrs = gobj.jn_attrs;

    let ss = path.split("`");
    if(ss.length<=1) {
        let key = path;
        if(key in jn_attrs) {
            if(jn_attrs.hasOwnProperty(key)) {
                jn_attrs[key] = value;

                if(gobj.gclass.gmt.mt_writing) {
                    gobj.gclass.gmt.mt_writing(key);
                }
                return 0;
            }
        }
        log_error(sprintf("gobj_write_attr(%s): attr not found '%s'", gobj_short_name(gobj), key));
        return -1;
    }

    let key;
    let len = ss.length;
    for(let i=0; i<len; i++) {
        key = ss[i];
        let child = gobj_find_child(gobj, {"__gobj_name__": key});
        if(child && i<len-1) {
            gobj = child;
            continue;
        }

        if(key in jn_attrs) {
            if (jn_attrs.hasOwnProperty(key)) {
                jn_attrs[key] = value;

                if(gobj.mt_writing) {
                    gobj.mt_writing(key);
                }
                return 0;
            }
        }
        break;
    }

    log_error(sprintf("gobj_write_attr(%s): attr not found '%s'", gobj_short_name(gobj), key));
    return -1;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
function gobj_write_attrs(
    gobj,
    kw,
    include_flag,  // sdata_flag_t
    src
) {
    let hs = gobj_hsdata(gobj);

    let ret = 0;

    for (const [attr, jn_value] of Object.entries(kw)) {
        let it = gobj_attr_desc(gobj, attr, true);
        if(!it) {
            continue;
        }
        if(!(include_flag === -1 || (it.flag & include_flag))) {
            continue;
        }
        ret += json2item(gobj, hs, it, jn_value);
    }

    return ret;
}

/***************************************************************************
 *  ATTR: read bool
 ***************************************************************************/
function gobj_read_bool_attr(gobj, name)
{
    if(name) {
        if(strcasecmp(name, "__disabled__")===0) {
            return gobj_is_disabled(gobj);
        } else if(strcasecmp(name, "__running__")===0) {
            return gobj_is_running(gobj);
        } else if(strcasecmp(name, "__playing__")===0) {
            return gobj_is_playing(gobj);
        } else if(strcasecmp(name, "__service__")===0) {
            return gobj_is_service(gobj);
        }
    }

    let hs = gobj_hsdata2(gobj, name, false);
    if(hs) {
        if(gobj.gclass.gmt.mt_reading) {
            if(!(gobj.obflag & obflag_t.obflag_destroyed)) {
                return gobj.gclass.gmt.mt_reading(gobj, name);
            }
        }
        return json_object_get(hs, name);
    }

    log_error(`${gobj_short_name(gobj)}: GClass Attribute NOT FOUND, attr ${name}`);
    return 0;
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
function gobj_read_integer_attr(gobj, name)
{
    if(name && strcasecmp(name, "__trace_level__")===0) {
        // TODO return gobj_trace_level(gobj);
    }

    let hs = gobj_hsdata2(gobj, name, false);
    if(hs) {
        if(gobj.gclass.gmt.mt_reading) {
            if(!(gobj.obflag & obflag_t.obflag_destroyed)) {
                return gobj.gclass.gmt.mt_reading(gobj, name);
            }
        }
        return json_object_get(hs, name);
    }

    log_error(`${gobj_short_name(gobj)}: GClass Attribute NOT FOUND, attr ${name}`);
    return 0;
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
function gobj_read_str_attr(gobj, name)
{
    if(name && strcasecmp(name, "__state__")===0) {
        return gobj_current_state(gobj);
    }

    let hs = gobj_hsdata2(gobj, name, false);
    if(hs) {
        if(gobj.gclass.gmt.mt_reading) {
            if(!(gobj.obflag & obflag_t.obflag_destroyed)) {
                return gobj.gclass.gmt.mt_reading(gobj, name);
            }
        }
        return json_object_get(hs, name);
    }

    log_error(`${gobj_short_name(gobj)}: GClass Attribute NOT FOUND, attr ${name}`);
    return 0;
}

/***************************************************************************
 *  ATTR: read
 ***************************************************************************/
function gobj_read_pointer_attr(gobj, name)
{
    let hs = gobj_hsdata2(gobj, name, false);
    if(hs) {
        if(gobj.gclass.gmt.mt_reading) {
            if(!(gobj.obflag & obflag_t.obflag_destroyed)) {
                return gobj.gclass.gmt.mt_reading(gobj, name);
            }
        }
        return json_object_get(hs, name);
    }

    log_error(`${gobj_short_name(gobj)}: GClass Attribute NOT FOUND, attr ${name}`);
    return 0;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
function gobj_write_bool_attr(gobj, name, value)
{
    let hs = gobj_hsdata2(gobj, name, false);
    if(hs) {
        hs[name] = value;
        if(gobj.gclass.gmt.mt_writing) {
            if((gobj.obflag & obflag_t.obflag_created) && !(gobj.obflag & obflag_t.obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj.gclass.gmt.mt_writing(gobj, name);
            }
        }
        return 0;
    }

    log_error(`${gobj_short_name(gobj)}: GClass Attribute NOT FOUND, attr ${name}`);
    return -1;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
function gobj_write_integer_attr(gobj, name, value)
{
    let hs = gobj_hsdata2(gobj, name, false);
    if(hs) {
        hs[name] = parseInt(value);
        if(gobj.gclass.gmt.mt_writing) {
            if((gobj.obflag & obflag_t.obflag_created) && !(gobj.obflag & obflag_t.obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj.gclass.gmt.mt_writing(gobj, name);
            }
        }
        return 0;
    }

    log_error(`${gobj_short_name(gobj)}: GClass Attribute NOT FOUND, attr ${name}`);
    return -1;
}

/***************************************************************************
 *  ATTR: write
 ***************************************************************************/
function gobj_write_str_attr(gobj, name, value)
{
    let hs = gobj_hsdata2(gobj, name, false);
    if(hs) {
        hs[name] = String(value);
        if(gobj.gclass.gmt.mt_writing) {
            if((gobj.obflag & obflag_t.obflag_created) && !(gobj.obflag & obflag_t.obflag_destroyed)) {
                // Avoid call to mt_writing before mt_create!
                gobj.gclass.gmt.mt_writing(gobj, name);
            }
        }
        return 0;
    }

    log_error(`${gobj_short_name(gobj)}: GClass Attribute NOT FOUND, attr ${name}`);
    return -1;
}

/***************************************************************************
 *
 ***************************************************************************/
function gobj_change_state(gobj, state_name)
{
    if(gobj_is_destroying(gobj)) {
        log_error("gobj NULL or DESTROYED");
        return null;
    }

    if(gobj.current_state.state_name === state_name) {
        return false;
    }
    let new_state = _find_state(gobj.gclass, state_name);
    if(!new_state) {
        log_error(`${gobj_short_name(gobj)}: state unknown, state ${state_name}`);
        return false;
    }
    gobj.last_state = gobj.current_state;
    gobj.current_state = new_state;

    // let tracea = is_machine_tracing(gobj, EV_STATE_CHANGED);
    // let tracea_states = __trace_gobj_states__(gobj)?true:false;
    // if(tracea || tracea_states) {
    //     trace_machine(sprintf("🔀🔀 mach(%s%s^%s), new st(%s%s%s), prev st(%s%s%s)",
    //         (!gobj.running)?"!!":"",
    //         gobj_gclass_name(gobj), gobj_name(gobj),
    //         On_Black RGreen,
    //         gobj_current_state(gobj),
    //         Color_Off,
    //         On_Black RGreen,
    //         gobj.last_state.state_name,
    //         Color_Off
    //     ));
    // }

    // TODO let kw_st = {};
    // kw_st["previous_state"] = gobj.last_state.state_name;
    // kw_st["current_state"] = gobj.current_state.state_name;
    //
    // if(gobj.gclass.gmt.mt_state_changed) {
    //     gobj.gclass.gmt.mt_state_changed(gobj, "EV_STATE_CHANGED", kw_st);
    // } else {
    //     gobj_publish_event(gobj, "EV_STATE_CHANGED", kw_st);
    // }

    return true;
}

/***************************************************************************
 *
 ***************************************************************************/
function gobj_current_state(gobj)
{
    if(gobj_is_destroying(gobj)) {
        log_error("gobj NULL or DESTROYED");
        return null;
    }

    return gobj.current_state.state_name;
}

/************************************************************
 *        Change parent
 ************************************************************/
function gobj_change_parent(gobj, parent)
{
    if (!(parent instanceof GObj)) {
        log_error("Yuno.gobj_change_parent() BAD TYPE of parent: " + parent);
        return -1;
    }

    /*--------------------------------------*
     *      Remove from current parent
     *--------------------------------------*/
    if (gobj.parent && gobj.parent.mt_child_removed) {
        gobj.parent.mt_child_removed(gobj);
    }
    if (gobj.parent) {
        gobj.parent._remove_child(gobj);
        gobj.parent = null;
    }

    /*--------------------------------------*
     *      Add to new parent
     *--------------------------------------*/
    parent._add_child(gobj);

    /*--------------------------------------*
     *  Inform to parent
     *-------------------------------------*/
    if(parent.mt_child_added) {
        let trace_creation = __yuno__ && gobj_read_bool_attr(__yuno__, "trace_creation");
        if(trace_creation) { // if(__trace_gobj_create_delete__(gobj))
            log_debug(sprintf("👦👦🔵 child_added(%s): %s", parent.gobj_full_name(), gobj.gobj_short_name()));
        }
        parent.mt_child_added(gobj);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
function gobj_event_type( // silent function
    gobj,
    event,
    include_system_events
)
{
    if(!gobj || !(gobj instanceof GObj)) {
        log_error(`gobj NULL of bad type`);
        return null;
    }

    let event_type = gclass_find_event_type(gobj.gclass, event);
    if(event_type) {
        return event_type;
    }

    if(include_system_events) {
        /*
         *  Check global (gobj) output events
         */
        const size = dl_global_event_types.length;
        const dl = dl_global_event_types;

        for(let i=0; i<size; i++) {
            let event_type = dl[i];
            if(event_type.event_name && event_type.event_name === event) {
                return event_type;
            }
        }
    }

    return null;
}

/***************************************************************************
 *
 ***************************************************************************/
function gobj_has_event(gobj, event, event_flag)
{
    let event_type = gobj_event_type(gobj, event, false);
    if(!event_type) {
        return false;
    }

    if(event_flag && !(event_type.event_flag & event_flag)) {
        return false;
    }

    return true;
}

/***************************************************************************
 *
 ***************************************************************************/
function gobj_has_output_event(gobj, event, event_flag)
{
    let event_type = gobj_event_type(gobj, event, false);
    if(!event_type) {
        return false;
    }

    if(event_flag && !(event_type.event_flag & event_flag)) {
        return false;
    }

    if(!(event_type.event_flag & event_flag_t.EVF_OUTPUT_EVENT)) {
        return false;
    }

    return true;
}

/************************************************************
 *      post_event, by now only in js
 ************************************************************/
function gobj_post_event(gobj, event, kw, src)
{
    setTimeout(() => {
        gobj_send_event(gobj, event, kw, src);
    }, 10);
}

/************************************************************
 *      send_event
 ************************************************************/
function gobj_send_event(dst, event, kw, src)
{
    if(!dst || !(dst instanceof GObj)) {
        log_error(`gobj dst NULL`);
        return -1;
    }

    if(dst.obflag & (obflag_t.obflag_destroyed|obflag_t.obflag_destroying)) {
        log_error(`gobj dst DESTROYED`);
        return -1;
    }

    let state = dst.current_state;
    if(!state) {
        log_error(`${gobj_short_name(dst)}: current_state NULL`);
        return -1;
    }

    /*----------------------------------*
     *  Find the event/action in state
     *----------------------------------*/
    let tracea = 0; // TODO is_machine_tracing(dst, event) && !is_machine_not_tracing(src, event);
    tracea = __yuno__ && gobj_read_integer_attr(__yuno__, "tracing");
    if(gobj_gclass_name(src) === "C_TIMER") {
        tracea = __yuno__ && gobj_read_integer_attr(__yuno__, "trace_timer");
    }

    __inside__ ++;

    let event_action = _find_event_action(state, event);
    if(!event_action) {
        if(dst.gclass.gmt.mt_inject_event) {
            __inside__ --;
            if(tracea) {
                trace_machine(sprintf("🔃 mach(%s%s), st: %s, ev: %s, from(%s%s)",
                    (!dst.running)?"!!":"",
                    gobj_short_name(dst),
                    state.state_name,
                    event?event:"",
                    (src && !src.running)?"!!":"",
                    gobj_short_name(src)
                ));
                if(kw) {
                    //if(__trace_gobj_ev_kw__(dst)) {
                    if(tracea > 1) {
                        if(json_object_size(kw)) {
                            trace_json(kw);
                        }
                    }
                }
            }
            return dst.gclass.gmt.mt_inject_event(dst, event, kw, src);
        }

        if(tracea) {
            trace_machine(sprintf(
                "📛 mach(%s%s^%s), st: %s, ev: %s, 📛📛ERROR Event NOT DEFINED in state📛📛, from(%s%s^%s)",
                (!dst.running)?"!!":"",
                gobj_gclass_name(dst), gobj_name(dst),
                state.state_name,
                event?event:"",
                (src && !src.running)?"!!":"",
                gobj_gclass_name(src), gobj_name(src)
            ));
        }
        log_error(`📛📛 Event NOT DEFINED in state 📛📛: ${gobj_short_name(dst)}, ${state.state_name}, ${event}`);

        __inside__ --;

        return -1;
    }

    /*----------------------------------*
     *      Check AUTHZ
     *----------------------------------*/
//     if(ev_desc.authz & EV_AUTHZ_INJECT) {
//     }

    /*----------------------------------*
     *      Exec the event
     *----------------------------------*/
    if(tracea) {
        trace_machine(sprintf("🔄 mach(%s%s^%s), st: %s, ev: %s%s%s, from(%s%s^%s)",
            (!dst.running)?"!!":"",
            gobj_gclass_name(dst), gobj_name(dst),
            state.state_name,
            "", //On_Black RBlue,
            event?event:"",
            "", //Color_Off,
            (src && !src.running)?"!!":"",
            gobj_gclass_name(src), gobj_name(src)
        ));
        if(kw) {
            if(tracea > 1) { //if(__trace_gobj_ev_kw__(dst)) {
                if(json_object_size(kw)) {
                    trace_json(kw);
                }
            }
        }
    }

    /*
     *  IMPORTANT HACK
     *  Set new state BEFORE run 'action'
     *
     *  The next state is changed before executing the action.
     *  If you don’t like this behavior, set the next-state to NULL
     *  and use change_state() to change the state inside the actions.
     */
    if(event_action.next_state) {
        gobj_change_state(dst, event_action.next_state);
    }

    let ret = -1;
    if(event_action.action) {
        // Execute the action
        ret = (event_action.action)(dst, event, kw, src);
    } else {
        // No action, there is nothing amiss!.
    }

    if(tracea && !(dst.obflag & obflag_t.obflag_destroyed)) {
        trace_machine(sprintf("<- mach(%s%s^%s), st: %s, ev: %s, ret: %d",
            (!dst.running)?"!!":"",
            gobj_gclass_name(dst), gobj_name(dst),
            dst.current_state.state_name,
            event?event:"",
            ret
        ));
    }

    __inside__ --;

    return ret;
}

/*
Schema of subs (subscription)
=============================
"publisher":            (pointer)int    // publisher gobj
"subscriber:            (pointer)int    // subscriber gobj
"event":                (pointer)str    // event name subscribed
"subs_flag":            int             // subscription flag. See subs_flag_t
"__config__":           json            // subscription config.
"__global__":           json            // global event kw. This json is extended with publishing kw.
"__local__":            json            // local event kw. The keys in this json are removed from publishing kw.

 */

const subs_flag_t = Object.freeze({
__hard_subscription__   : 0x00000001,
__own_event__           : 0x00000002,   // If gobj_send_event return -1 don't continue publishing
});

/*
 *
 */
const subscription_desc = [
/*-ATTR-type--------------------name-----------flag-default-description---------- */
SDATA(data_type_t.DTP_POINTER,  "publisher",    0,  null,   "publisher gobj"),
SDATA(data_type_t.DTP_POINTER,  "subscriber",   0,  null,   "subscriber gobj"),
SDATA(data_type_t.DTP_STRING,   "event",        0,  "",     "event name subscribed"),
SDATA(data_type_t.DTP_STRING,   "renamed_event",0,  "",     "rename event name"),
SDATA(data_type_t.DTP_INTEGER,  "subs_flag",    0,  0,      "subscription flag"),
SDATA_END()
];

/***************************************************************************
 *
 ***************************************************************************/
function _create_subscription(
    publisher,
    event,
    kw,
    subscriber)
{
    let subs = sdata_create(publisher, subscription_desc);
    json_object_set_new(subs, "event", event);
    json_object_set_new(subs, "subscriber", subscriber);
    json_object_set_new(subs, "publisher", publisher);

    let subs_flag = 0;

    if(kw) {
        let __config__ = kw_get_dict(publisher, kw, "__config__", null);
        let __global__ = kw_get_dict(publisher, kw, "__global__", null);
        let __local__ = kw_get_dict(publisher, kw, "__local__", null);
        let __filter__ = kw_get_dict_value(publisher, kw, "__filter__", null);

        if(__global__) {
            let kw_clone = json_deep_copy(__global__);
            json_object_set_new(subs, "__global__", kw_clone);
        }
        if(__config__) {
            let kw_clone = json_deep_copy(__config__);
            json_object_set_new(subs, "__config__", kw_clone);

// TODO
//            if(kw_has_key(kw_clone, "__rename_event_name__")) {
//                const char *renamed_event = kw_get_str(gobj?, kw_clone, "__rename_event_name__", 0, 0);
//                sdata_write_str(subs, "renamed_event", renamed_event);
//                json_object_del(kw_clone, "__rename_event_name__");
//                subs_flag |= __rename_event_name__;
//
//                // Get/Create __global__
//                json_t *kw_global = sdata_read_json(subs, "__global__");
//                if(!kw_global) {
//                    kw_global = json_object();
//                    sdata_write_json(subs, "__global__", kw_global);
//                    kw_decref(kw_global); // Incref above
//                }
//                kw_set_dict_value(
//                    kw_global,
//                    "__original_event_name__",
//                    json_string(event)
//                );
//            }

            if(kw_has_key(kw_clone, "__hard_subscription__")) {
                let hard_subscription = kw_get_bool(publisher,
                    kw_clone, "__hard_subscription__", false
                );
                json_object_del(kw_clone, "__hard_subscription__");
                if(hard_subscription) {
                    subs_flag |= subs_flag_t.__hard_subscription__;
                }
            }
            if(kw_has_key(kw_clone, "__own_event__")) {
                let own_event= kw_get_bool(publisher,
                    kw_clone, "__own_event__", false
                );
                json_object_del(kw_clone, "__own_event__");
                if(own_event) {
                    subs_flag |= subs_flag_t.__own_event__;
                }
            }
        }
        if(__local__) {
            let kw_clone = json_deep_copy(__local__);
            json_object_set_new(subs, "__local__", kw_clone);
        }
        if(__filter__) {
            let kw_clone = json_deep_copy(__filter__);
            json_object_set_new(subs, "__filter__", kw_clone);
        }
        //if(__service__) {
        //    json_object_set_new(subs, "__service__", json_string(__service__));
        //}
    }
    json_object_set_new(subs, "subs_flag", subs_flag);

    return subs;
}

/***************************************************************************
 *  Return a iter of subscriptions (sdata),
 *  filtering by matching:
 *      event,kw (__config__, __global__, __local__, __filter__),subscriber
 ***************************************************************************/
function _find_subscription(
    dl_subs,
    publisher,
    event,
    kw,
    subscriber,
    strict
) {
    let _match;
    if(strict) {
        _match = json_is_identical; // WARNING don't decref anything
    } else {
        _match = kw_match_simple; // WARNING decref second parameter
    }

    let __config__ = kw_get_dict(publisher, kw, "__config__", null);
    let __global__ = kw_get_dict(publisher, kw, "__global__", null);
    let __local__ = kw_get_dict(publisher, kw, "__local__", null);
    let __filter__ = kw_get_dict_value(publisher, kw, "__filter__", null);

    let iter = [];

    for(let i=0; i<dl_subs.length; i++) {
        let subs = dl_subs[i];
        let match = true;

        if(publisher) {
            let publisher_ = kw_get_pointer(null, subs, "publisher", null);
            if(publisher !== publisher_) {
                match = false;
            }
        }

        if(subscriber) {
            let subscriber_ = kw_get_pointer(null, subs, "subscriber", null);
            if(subscriber !== subscriber_) {
                match = false;
            }
        }

        if(event) {
            let event_ = kw_get_int(null, subs, "event", "");
            if(empty_string(event_) || event !== event_) {
                match = false;
            }
        }

        if(__config__) {
            let kw_config = kw_get_dict(null, subs, "__config__", null);
            if(kw_config) {
                if(!strict) { // HACK decref when calling _match (kw_match_simple)
                    //KW_INCREF(__config__);
                }
                if(!_match(kw_config, __config__)) {
                    match = false;
                }
            } else {
                match = false;
            }
        }
        if(__global__) {
            let kw_global = kw_get_dict(null, subs, "__global__", null);
            if(kw_global) {
                if(!strict) { // HACK decref when calling _match (kw_match_simple)
                    //KW_INCREF(__global__);
                }
                if(!_match(kw_global, __global__)) {
                    match = false;
                }
            } else {
                match = false;
            }
        }
        if(__local__) {
            let kw_local = kw_get_dict(null, subs, "__local__", null);
            if(kw_local) {
                if(!strict) { // HACK decref when calling _match (kw_match_simple)
                    //KW_INCREF(__local__);
                }
                if(!_match(kw_local, __local__)) {
                    match = false;
                }
            } else {
                match = false;
            }
        }
        if(__filter__) {
            let kw_filter = kw_get_dict_value(null, subs, "__filter__", null);
            if(kw_filter) {
                if(!strict) { // HACK decref when calling _match (kw_match_simple)
                    //KW_INCREF(__filter__);
                }
                if(!_match(kw_filter, __filter__)) {
                    match = false;
                }
            } else {
                match = false;
            }
        }

        if(match) {
            json_array_append(iter, subs);
        }
    }

    // KW_DECREF(kw)
    return iter;
}

/***************************************************************************
 *  Delete subscription
 ***************************************************************************/
function _delete_subscription(
    gobj,
    subs,
    force,
    not_inform
) {
    let publisher = kw_get_pointer(gobj, subs, "publisher", null, kw_flag_t.KW_REQUIRED);
    let subscriber = kw_get_pointer(gobj, subs, "subscriber", null, kw_flag_t.KW_REQUIRED);
    let event = kw_get_str(gobj, subs, "event", "", kw_flag_t.KW_REQUIRED);
    let subs_flag = kw_get_int(gobj, subs, "subs_flag", null, kw_flag_t.KW_REQUIRED);
    let hard_subscription = (subs_flag & subs_flag_t.__hard_subscription__)?1:0;

    /*-------------------------------*
     *  Check if hard subscription
     *-------------------------------*/
    if(hard_subscription) {
        if(!force) {
            return -1;
        }
    }

    /*-----------------------------*
     *          Trace
     *-----------------------------*/
    let tracea = __yuno__ && gobj_read_bool_attr(__yuno__, "trace_subscriptions");
    if(tracea) {
    // if(__trace_gobj_subscriptions__(subscriber) || __trace_gobj_subscriptions__(publisher) ) {
        trace_machine(sprintf(
            "💜💜👎 unsubscribing event '%s': subscriber'%s', publisher %s",
            event?event:"",
            gobj_short_name(subscriber),
            gobj_short_name(publisher)
        ));
        trace_json(subs);
    }

    /*------------------------------------------------*
     *      Inform (BEFORE) of subscription removed
     *------------------------------------------------*/
    if(!(publisher.obflag & obflag_t.obflag_destroyed)) {
        if(!not_inform) {
            if(publisher.gclass.gmt.mt_subscription_deleted) {
                publisher.gclass.gmt.mt_subscription_deleted(
                    publisher,
                    subs
                );
            }
        }
    }

    /*--------------------------------*
     *      Delete subscription
     *--------------------------------*/
    let idx = kw_find_json_in_list(gobj, publisher.dl_subscriptions, subs, 0);
    if(idx >= 0) {
        json_array_remove(publisher.dl_subscriptions, idx);
    } else {
        log_error(`${gobj_short_name(gobj)}: subscription in publisher not found`);
        trace_json(subs);
    }

    idx = kw_find_json_in_list(gobj, subscriber.dl_subscribings, subs, 0);
    if(idx >= 0) {
        json_array_remove(subscriber.dl_subscribings, idx);
    } else {
        log_error(`${gobj_short_name(gobj)}: subscription in subscriber not found`);
        trace_json(subs);
    }

    return 0;
}

/***************************************************************************
 *  Subscribe to an event
 *
 *  event is only one event (not a string list like ginsfsm).
 *
 *  See .h
 ***************************************************************************/
function gobj_subscribe_event(
    publisher,
    event,
    kw,
    subscriber)
{
    /*---------------------*
     *  Check something
     *---------------------*/
    if(!publisher) {
        log_error(`publisher NULL: ev ${event}`);
        return 0;
    }
    if(!subscriber) {
        log_error(`subscriber NULL: ev ${event}`);
        return 0;
    }
    if(event == null) {
        event = "";
    }

    /*--------------------------------------------------------------*
     *  Event must be in output event list
     *  You can avoid this with gcflag_no_check_output_events flag
     *--------------------------------------------------------------*/
    if(!empty_string(event)) {
        if(!gobj_has_output_event(publisher, event, event_flag_t.EVF_OUTPUT_EVENT)) {
            if(!(publisher.gclass.gclass_flag & gclass_flag_t.gcflag_no_check_output_events)) {
                log_error(`event NOT in output event list: ${gobj_short_name(publisher)}, ${event}`);
                return 0;
            }
        }
    }

    /*-------------------------------------------------*
     *  Check AUTHZ
     *-------------------------------------------------*/

    /*------------------------------*
     *  Find repeated subscription
     *------------------------------*/
    let dl_subs = _find_subscription(
        publisher.dl_subscriptions,
        publisher,
        event,
        kw,
        subscriber,
        true
    );
    if(json_array_size(dl_subs) > 0) {
        log_warning(`${gobj_short_name(publisher)}: subscription(s) REPEATED, will be deleted and override`);
        gobj_unsubscribe_list(dl_subs, false);
    }

    /*-----------------------------*
     *  Create subscription
     *-----------------------------*/
    let subs = _create_subscription(
        publisher,
        event,
        kw,
        subscriber
    );
    if(!subs) {
        log_error(`${gobj_short_name(publisher)}: _create_subscription() FAILED`);
        return 0;
    }

    json_array_append(publisher.dl_subscriptions, subs);
    json_array_append(subscriber.dl_subscribings, subs);

    /*-----------------------------*
     *          Trace
     *-----------------------------*/
    // if(__trace_gobj_subscriptions__(subscriber) || __trace_gobj_subscriptions__(publisher)) {
    let tracea = __yuno__ && gobj_read_bool_attr(__yuno__, "trace_subscriptions");
    if(tracea) {
        trace_machine(sprintf(
            "💜💜👍 subscribing event '%s', subscriber'%s', publisher %s",
            event?event:"",
            gobj_short_name(subscriber),
            gobj_short_name(publisher)
        ));
        if(kw) {
            trace_json(kw);
        }
    }

    /*--------------------------------*
     *      Inform new subscription
     *--------------------------------*/
    if(publisher.gclass.gmt.mt_subscription_added) {
        let result = publisher.gclass.gmt.mt_subscription_added(
            publisher,
            subs
        );
        if(result < 0) {
            _delete_subscription(publisher, subs, true, true);
            subs = 0;
        }
    }

    return subs;
}

/***************************************************************************
 *
 ***************************************************************************/
function gobj_unsubscribe_event(
    publisher,
    event,
    kw,
    subscriber)
{
    /*---------------------*
     *  Check something
     *---------------------*/
    if(!publisher) {
        log_error(`publisher NULL: ev ${event}`);
        return 0;
    }
    if(!subscriber) {
        log_error(`subscriber NULL: ev ${event}`);
        return 0;
    }
    if(event == null) {
        event = "";
    }

    /*--------------------------------------------------------------*
     *  Event must be in output event list
     *  You can avoid this with gcflag_no_check_output_events flag
     *--------------------------------------------------------------*/
    if(!empty_string(event)) {
        if(!gobj_has_output_event(publisher, event, event_flag_t.EVF_OUTPUT_EVENT)) {
            if(!(publisher.gclass.gclass_flag & gclass_flag_t.gcflag_no_check_output_events)) {
                log_error(`event NOT in output event list: ${gobj_short_name(publisher)}, ev ${event}`);
                return 0;
            }
        }
    }

    /*-----------------------------*
     *      Find subscription
     *-----------------------------*/
    let dl_subs = _find_subscription(
        publisher.dl_subscriptions,
        publisher,
        event,
        kw,
        subscriber,
        true
    );

    let deleted = 0;
    for(let i=0; i<dl_subs.length; i++) {
        let subs = dl_subs[i];
        _delete_subscription(publisher, subs, false, false);
        deleted++;
    }

    if(!deleted) {
        log_error(`${gobj_short_name(publisher)}: No subscription found`);
        trace_json(kw);
    }

    return 0;
}

/***************************************************************************
 *  Unsubscribe a list of subscription hsdata
 ***************************************************************************/
function gobj_unsubscribe_list(
    dl_subs,
    force // delete hard_subscription subs too
)
{
    let dl = [...dl_subs];
    for(let i=0; i<dl.length; i++) {
        let subs = dl[i];
        _delete_subscription(0, subs, force, false);
    }
    return 0;
}

/***************************************************************************
 *  Return a iter of subscriptions (sdata) in the publisher gobj,
 *  filtering by matching: event,kw (__config__, __global__, __local__, __filter__),subscriber
 *  Free return with rc_free_iter(iter, true, false);


 *  gobj_find_subscriptions()
 *  Return the list of subscriptions of the publisher gobj,
 *  filtering by matching: event,kw (__config__, __global__, __local__, __filter__),subscriber

    USO
    ---
    TO resend subscriptions in ac_identity_card_ack() in c_ievent_cli.c

        PRIVATE int resend_subscriptions(hgobj gobj)
        {
            dl_list_t *dl_subs = gobj_find_subscriptions(gobj, 0, 0, 0);

            rc_instance_t *i_subs; hsdata subs;
            i_subs = rc_first_instance(dl_subs, (rc_resource_t **)&subs);
            while(i_subs) {
                send_remote_subscription(gobj, subs);
                i_subs = rc_next_instance(i_subs, (rc_resource_t **)&subs);
            }
            rc_free_iter(dl_subs, true, false);
            return 0;
        }

        que son capturadas en:

             PRIVATE int mt_subscription_added(
                hgobj gobj,
                hsdata subs)
            {
                if(!gobj_in_this_state(gobj, ST_SESSION)) {
                    // on_open will send all subscriptions
                    return 0;
                }
                return send_remote_subscription(gobj, subs);
            }


    EN mt_stop de c_counter.c (???)

        PRIVATE int mt_stop(hgobj gobj)
        {
            PRIVATE_DATA *priv = gobj_priv_data(gobj);

            gobj_unsubscribe_list(gobj_find_subscriptions(gobj, 0, 0, 0), true, false);

            clear_timeout(priv.timer);
            if(gobj_is_running(priv.timer))
                gobj_stop(priv.timer);
            return 0;
        }


 ***************************************************************************/
function gobj_find_subscriptions(
    publisher,
    event,
    kw,
    subscriber)
{
    return _find_subscription(
        publisher.dl_subscriptions,
        publisher,
        event,
        kw,
        subscriber,
        false
    );
}

/***************************************************************************
 *
 ***************************************************************************/
function gobj_list_subscriptions(gobj2view)
{
    let subscriptions = gobj_find_subscriptions(
        gobj2view,
        0,      // event,
        null,   // kw (__config__, __global__, __local__)
        0       // subscriber
    );
    for(let i=0; i<subscriptions.length; i++) {
        let sub = subscriptions[i];
        let publisher = kw_get_pointer(gobj2view, sub, "publisher", null, kw_flag_t.KW_REQUIRED);
        let subscriber = kw_get_pointer(gobj2view, sub, "subscriber", null, kw_flag_t.KW_REQUIRED);
        let event_ = kw_get_str(gobj2view, sub, "event", "", kw_flag_t.KW_REQUIRED);

        json_object_set_new(sub, "s_event", event_);
        json_object_set_new(sub, "s_publisher", gobj_short_name(publisher));
        json_object_set_new(sub, "s_subscriber", gobj_short_name(subscriber));
    }

    let subscribings = gobj_find_subscribings( // Return is YOURS
        gobj2view,
        0,      // event,
        null,   // kw (__config__, __global__, __local__)
        0       // subscriber
    );
    for(let i=0; i<subscribings.length; i++) {
        let sub = subscribings[i];
        let publisher = kw_get_pointer(gobj2view, sub, "publisher", null, kw_flag_t.KW_REQUIRED);
        let subscriber = kw_get_pointer(gobj2view, sub, "subscriber", null, kw_flag_t.KW_REQUIRED);
        let event_ = kw_get_str(gobj2view, sub, "event", "", kw_flag_t.KW_REQUIRED);

        json_object_set_new(sub, "s_event", event_);
        json_object_set_new(sub, "s_publisher", gobj_short_name(publisher));
        json_object_set_new(sub, "s_subscriber", gobj_short_name(subscriber));
    }

    return {
        "subscriptions": subscriptions,
        "subscribings": subscribings,
        "total subscriptions": json_array_size(subscriptions),
        "total_subscribings": json_array_size(subscribings)
    };
}

/***************************************************************************
 *  Return a iter of subscribings (sdata) of the subscriber gobj,
 *  filtering by matching: event,kw (__config__, __global__, __local__, __filter__), publisher
 *  Free return with rc_free_iter(iter, true, false);
 *
 *  gobj_find_subscribings()
 *  Return a list of subscribings of the subscriber gobj,
 *  filtering by matching: event,kw (__config__, __global__, __local__, __filter__), publisher
 ***************************************************************************/
function gobj_find_subscribings(
    subscriber,
    event,
    kw,             // kw (__config__, __global__, __local__, __filter__)
    publisher
)
{
    return _find_subscription(
        subscriber.dl_subscribings,
        publisher,
        event,
        kw,
        subscriber,
        false
    );
}

/***************************************************************************
 *  Return the sum of returns of gobj_send_event
 ***************************************************************************/
function gobj_publish_event(
    publisher,
    event,
    kw)
{
    if(!is_object(kw)) {
        kw = {};
    }

    /*---------------------*
     *  Check something
     *---------------------*/
    if(!publisher || publisher.obflag & (obflag_t.obflag_destroyed|obflag_t.obflag_destroying)) {
        log_error("gobj NULL or DESTROYED");
        return -1;
    }
    if(empty_string(event)) {
        log_error(`${gobj_short_name(publisher)}: event EMPTY`);
        return -1;
    }

    /*--------------------------------------------------------------*
     *  Event must be in output event list
     *  You can avoid this with gcflag_no_check_output_events flag
     *--------------------------------------------------------------*/
    let ev = gobj_event_type(publisher, event, true);
    if(!(ev && (ev.event_flag & (event_flag_t.EVF_SYSTEM_EVENT|event_flag_t.EVF_OUTPUT_EVENT)))) {
        /*
         *  HACK ev can be null,
         *  there are gclasses like c_iogate that are intermediate of other events.
         */
        if(!(publisher.gclass.gclass_flag & gclass_flag_t.gcflag_no_check_output_events)) {
            log_error(`event NOT in output event list: ${gobj_short_name(publisher)}, ev ${event}`);
            return -1;
        }
    }

    event = ev.event_name;

    let tracea = __yuno__ && gobj_read_bool_attr(__yuno__, "trace_subscriptions");
    // let tracea = __trace_gobj_subscriptions__(publisher) &&
    //     !is_machine_not_tracing(publisher, event);
    if(tracea) {
        trace_machine(sprintf("🔝🔝 mach(%s%s^%s), st: %s, ev: %s%s%s",
            (!publisher.running)?"!!":"",
            gobj_gclass_name(publisher), gobj_name(publisher),
            publisher.current_state.state_name,
            "", //On_Black BBlue,
            event?event:"",
            "" //Color_Off
        ));
        trace_json(kw);
    }

    /*----------------------------------------------------------*
     *  Own publication method
     *  Return:
     *     -1  broke                        . return
     *      0  continue without publish     . return
     *      1  publish and continue         . continue below
     *----------------------------------------------------------*/
    if(publisher.gclass.gmt.mt_publish_event) {
        let topublish = publisher.gclass.gmt.mt_publish_event(
            publisher,
            event,
            kw  // not owned
        );
        if(topublish<=0) {
            return topublish;
        }
    }

    /*--------------------------------------------------------------*
     *      Default publication method
     *--------------------------------------------------------------*/
    let dl_subs = [...publisher.dl_subscriptions]; // Create a shallow copy
    let sent_count = 0;
    let ret = 0;

    for(let idx=0; idx<dl_subs.length; idx++) {
        let subs = dl_subs[idx];
        /*-------------------------------------*
         *  Pre-filter
         *  kw NOT owned! you can modify the publishing kw
         *  Return:
         *     -1  broke
         *      0  continue without publish
         *      1  continue to publish
         *-------------------------------------*/
        if(publisher.gclass.gmt.mt_publication_pre_filter) {
            let topublish = publisher.gclass.gmt.mt_publication_pre_filter(
                publisher,
                subs,
                event,
                kw  // not owned
            );
            if(topublish<0) {
                break;
            } else if(topublish===0) {
                continue;
            }
        }
        let subscriber = kw_get_pointer(publisher, subs, "subscriber", null, kw_flag_t.KW_REQUIRED);
        if(!(subscriber && !gobj_is_destroying(subscriber))) {
            continue;
        }

        /*
         *  Check if event null or event in event_list
         */
        let subs_flag = kw_get_int(publisher, subs, "subs_flag", 0, kw_flag_t.KW_REQUIRED);
        let event_ = kw_get_str(publisher, subs, "event", "", kw_flag_t.KW_REQUIRED);
        if(empty_string(event_) || strcasecmp(event_, event)===0) {
            let __config__ = kw_get_dict(publisher, subs, "__config__", null, 0);
            let __global__ = kw_get_dict(publisher, subs, "__global__", null, 0);
            let __local__ = kw_get_dict(publisher, subs, "__local__", null, 0);
            let __filter__ = kw_get_dict_value(publisher, subs, "__filter__", null, 0);

            /*
             *  Check renamed_event
             */
//   TODO review        const char *event_name = sdata_read_str(subs, "renamed_event");
//            if(empty_string(event_name)) {
//                event_name = event;
//            }

            /*
             *  Duplicate the kw to publish if not shared
             *  NOW always shared
             */
            let kw2publish = kw;

            /*-------------------------------------*
             *  User filter method or filter parameter
             *  Return:
             *     -1  (broke),
             *      0  continue without publish,
             *      1  continue and publish
             *-------------------------------------*/
            let topublish = 1;
            if(publisher.gclass.gmt.mt_publication_filter) {
                topublish = publisher.gclass.gmt.mt_publication_filter(
                    publisher,
                    event,
                    kw2publish,  // not owned
                    subscriber
                );
            } else if(__filter__) {
                if(__publish_event_match__) {
                    topublish = __publish_event_match__(kw2publish , __filter__);
                }
                if(tracea) {
                    trace_machine(sprintf(
                        "💜💜🔄%s publishing with filter, event '%s', subscriber'%s', publisher %s",
                        topublish?"👍":"👎",
                        event?event:"",
                        gobj_short_name(subscriber),
                        gobj_short_name(publisher)
                    ));
                    trace_json(__filter__);
                }
            }

            if(topublish<0) {
                break;
            } else if(topublish===0) {
                /*
                 *  Must not be published
                 *  Next subs
                 */
                continue;
            }

            /*
             *  Check if System event: don't send it if subscriber has not it
             */
            let ev_ = gobj_event_type(subscriber, event, true);
            if(ev_) {
                if(ev_.event_flag & event_flag_t.EVF_SYSTEM_EVENT) {
                    if(!gobj_has_event(subscriber, ev_.event_name, 0)) {
                        continue;
                    }
                }
            }

            /*
             *  Remove local keys
             */
            if(__local__) {
                kw_pop(kw2publish,
                    __local__ // not owned
                );
            }

            /*
             *  Apply transformation filters
             *   TODO review, I think it was used only from c_agent
             */
            if(__config__) {
//                json_t *jn_trans_filters = kw_get_dict_value(
//                    publisher, __config__, "__trans_filter__", 0, 0
//                );
//                if(jn_trans_filters) {
//                    kw2publish = apply_trans_filters(publisher, kw2publish, jn_trans_filters);
//                }
            }

            /*
             *  Add global keys
             */
            if(__global__) {
                json_object_update(kw2publish, __global__);
            }

            /*
             *  Send event
             */
            if(tracea) {
                trace_machine(sprintf("🔝🔄 mach(%s%s), st: %s, ev: %s, from(%s%s)",
                    (!subscriber.running)?"!!":"",
                    gobj_short_name(subscriber),
                    gobj_current_state(subscriber),
                    event?event:"",
                    (publisher && !publisher.running)?"!!":"",
                    gobj_short_name(publisher)
                ));
                trace_json(kw2publish);
            }

            let ret_ = gobj_send_event(
                subscriber,
                event,
                kw2publish,
                publisher
            );
            if(ret_ < 0 && (subs_flag & subs_flag_t.__own_event__)) {
                sent_count = -1; // Return of -1 indicates that someone owned the event
                break;
            }
            ret += ret_;
            sent_count++;

            if(gobj_is_destroying(publisher)) {
                /*
                 *  break all, self publisher deleted
                 */
                break;
            }
        }
    }

    if(!sent_count) {
        if(!ev || !(ev.event_flag & event_flag_t.EVF_NO_WARN_SUBS)) {
            log_warning(`${gobj_short_name(publisher)}: Publish event WITHOUT subscribers, ev ${event}`);
            // if(__trace_gobj_ev_kw__(publisher)) {
            //     if(json_object_size(kw)) {
            //         gobj_trace_json(publisher, kw, "Publish event WITHOUT subscribers");
            //     }
            // }
        }
    }

    return ret;
}

/************************************************************
 *  Exec gobj command. Return a webix
 ************************************************************/
function gobj_command(gobj, command, kw, src)
{
    if(gobj_is_destroying(gobj)) {
        log_error("gobj NULL or DESTROYED");
        return -1;
    }

    /*-----------------------------------------------*
     *  Trace
     *-----------------------------------------------*/
    let tracea = __yuno__ && gobj_read_integer_attr(__yuno__, "tracing");
    if(tracea) {
        trace_machine("🌀🌀 mach(%s%s), cmd: %s, src: %s",
            (!gobj_is_running(gobj))?"!!":"",
            gobj_short_name(gobj),
            command,
            gobj_short_name(src)
        );
        if(tracea > 1) {
            trace_json(kw);
        }
    }

    /*-----------------------------------------------*
     *  The local mt_command_parser has preference
     *-----------------------------------------------*/
    if(gobj.gclass.gmt.mt_command_parser) {
        return gobj.gclass.gmt.mt_command_parser(gobj, command, kw, src);
    }

    /*-----------------------------------------------*
     *  If it has command_table
     *  then use the global command parser
     *-----------------------------------------------*/
    if(gobj.gclass.command_table) {
        if(__global_command_parser_fn__) {
            return __global_command_parser_fn__(gobj, command, kw, src);
        } else {
            return build_command_response(
                gobj,
                -1,     // result
                sprintf("%s: global command parser not available", gobj_short_name(gobj)), // jn_comment
                0,      // jn_schema
                0       // jn_data
            );
        }
    } else {
        return build_command_response(
            gobj,
            -1,     // result
            sprintf("%s: command table not available",gobj_short_name(gobj)), // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
    }
}

/************************************************************
 *  Exec gobj stats. Return a webix
 ************************************************************/
function gobj_stats(gobj, stats, kw, src)
{
    if(gobj_is_destroying(gobj)) {
        log_error("gobj NULL or DESTROYED");
        return -1;
    }

    /*--------------------------------------*
     *  The local mt_stats has preference
     *--------------------------------------*/
    if(gobj.gclass.gmt.mt_stats) {
        return gobj.gclass.gmt.mt_stats(gobj, stats, kw, src);
    }

    /*-----------------------------------------------*
     *  Then use the global stats parser
     *-----------------------------------------------*/
    if(__global_stats_parser_fn__) {
        return __global_stats_parser_fn__(gobj, stats, kw, src);
    } else {
        return build_stats_response(
            gobj,
            -1,     // result
            sprintf("%s, stats parser not available", gobj_short_name(gobj)),// jn_comment
            0,      // jn_schema
            0       // jn_data
        );
    }
}


//=======================================================================
//      Expose the class via the global object
//=======================================================================
export {
    __yuno__,
    YUNETA_VERSION,
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
    event_flag_t,
    GObj,
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
    gobj_services,
    gobj_default_service,
    gobj_find_service,
    gobj_load_persistent_attrs,
    gobj_save_persistent_attrs,
    gobj_remove_persistent_attrs,
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
    gobj_start_childs,
    gobj_start_tree,
    gobj_stop,
    gobj_stop_childs,
    gobj_stop_tree,
    gobj_play,
    gobj_pause,
    gobj_is_running,
    gobj_is_playing,
    gobj_is_service,
    gobj_yuno,
    gobj_yuno_name,
    gobj_yuno_role,
    gobj_yuno_id,
    gobj_name,
    gobj_gclass_name,
    gobj_short_name,
    gobj_full_name,
    gobj_parent,
    gobj_is_volatil,
    gobj_is_pure_child,
    gobj_is_destroying,
    gobj_bottom_gobj,
    gobj_set_bottom_gobj,
    gobj_walk_gobj_childs,
    gobj_walk_gobj_childs_tree,
    gobj_find_gobj,
    gobj_match_gobj,
    gobj_find_child,
    gobj_find_child_by_tree,
    gobj_search_path,
    gobj_has_attr,
    gobj_read_attr,
    gobj_read_attrs,
    gobj_write_attr,
    gobj_write_attrs,
    gobj_read_bool_attr,
    gobj_read_integer_attr,
    gobj_read_str_attr,
    gobj_read_pointer_attr,
    gobj_write_bool_attr,
    gobj_write_integer_attr,
    gobj_write_str_attr,
    gobj_change_state,
    gobj_current_state,
    gobj_change_parent,

    gobj_has_event,
    gobj_has_output_event,
    gobj_post_event,
    gobj_send_event,

    gobj_subscribe_event,
    gobj_unsubscribe_event,
    gobj_unsubscribe_list,
    gobj_find_subscriptions,
    gobj_list_subscriptions,
    gobj_find_subscribings,
    gobj_publish_event,
    gobj_command,
    gobj_stats,
};
