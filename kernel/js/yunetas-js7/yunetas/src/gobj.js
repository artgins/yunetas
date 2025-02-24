/*********************************************************************************
 *  Objects with a simple Finite State Machine.
 *
 *      Licence: MIT (http://www.opensource.org/licenses/mit-license)
 *      Copyright (c) 2014,2024 Niyamaka.
 *      Copyright (c) 2025, ArtGins.
 *********************************************************************************/
import {
    log_error,
    trace_msg,
} from "./utils.js";

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

    console.log("MERDE");
    trace_msg("MERDE 2");
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

}

proto._gobj_create = function(gobj_name, gclass_name, kw, parent, is_service, is_unique, is_volatil)
{
    if(is_string(gclass_name)) {
        let gclass_ = gclass_name;
        gclass_name = gobj_find_gclass(gclass_, false);
        if(!gclass_name) {
            log_error("GClass not found: '" + gclass_ +"'");
            return null;
        }
    }

    if(!empty_string(gobj_name)) {
        /*
         *  Check that the gobj_name: cannot contain `
         */
        if(gobj_name.indexOf("`")>=0) {
            log_error("GObj gobj_name cannot contain \"`\" char: '" + gobj_name + "'");
            return null;
        }
        /*
         *  Check that the gobj_name: cannot contain ^
         */
        if(gobj_name.indexOf("^")>=0) {
            log_error("GObj gobj_name cannot contain \"^\" char: '" + gobj_name + "'");
            return null;
        }
    } else {
        /*
         *  To facility the work with jquery, I generate all gobjs as named gobjs.
         *  If a gobj has no gobj_name, generate a unique gobj_name with uniqued_id.
         */
        // force all gobj to have a gobj_name.
        // useful to make DOM elements with id depending of his gobj.
        // WARNING danger change, 13/Ago/2020, now anonymous gobjs in js
        gobj_name = ""; // get_unique_id('gobj');
    }

    if (!(typeof parent === 'string' || parent instanceof GObj)) {
        log_error("Yuno.gobj_create() BAD TYPE of parent: " + parent);
        return null;
    }

    if (typeof parent === 'string') {
        // find the named gobj
        parent = this.gobj_find_unique_gobj(parent);
        if (!parent) {
            let msg = "Yuno.gobj_create('" + gobj_name + "'): " +
                "WITHOUT registered named PARENT: '" + parent + "'";
            log_warning(msg);
            return null;
        }
    }

    let gobj = new gclass_name(gobj_name, kw);
    gobj.yuno = this;

    if (this.config.trace_creation) {
        let gclass_name = gobj.gclass_name || '';
        log_debug(sprintf("💙💙⏩ creating: %s%s %s^%s",
            is_service?"service":"", is_unique?"unique":"", gclass_name, gobj_name
        ));
    }

    if(!gobj.gobj_load_persistent_attrs) {
        let msg = "Check GClass of '" + gobj_name + "': don't look a GClass";
        log_error(msg);
        return null;
    }
    if(gobj_name) {
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
