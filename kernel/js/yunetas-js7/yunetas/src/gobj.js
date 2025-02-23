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
import {log_error} from "./utils.js";

/**************************************************************************
 *        GObj
 **************************************************************************/
let _gclass_register = {};
let __inside_event_loop__ = 0;
let __jn_global_settings__ =  null;
let __global_load_persistent_attrs_fn__ = null;
let __global_save_persistent_attrs_fn__ = null;
let __global_remove_persistent_attrs_fn__ = null;
let __global_list_persistent_attrs_fn__ = null;
let __global_command_parser_fn__ = null;
let __global_stats_parser_fn__ = null;


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
    let gclass = null;
    try {
        gclass = _gclass_register[gclass_name];
    } catch (e) {
    }
    if(!gclass) {
        if(verbose) {
            log_error("Yuno.gobj_find_gclass(): '" + gclass_name + "' gclass not found");
        }
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


//=======================================================================
//      Expose the class via the global object
//=======================================================================
export {
    gobj_start_up,
    gobj_register_gclass,
    gobj_find_gclass,

    gobj_get_gclass_config,
    // GObj,
    // gcflag_manual_start,
    // gcflag_no_check_output_events,
};
