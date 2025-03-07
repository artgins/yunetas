/*********************************************************************************
 *      dbsimple.js
 *
 *      Licence: MIT (http://www.opensource.org/licenses/mit-license)
 *      Copyright (c) 2014,2024 Niyamaka.
 *      Copyright (c) 2025, ArtGins.
 *********************************************************************************/

import {
    kw_get_local_storage_value,
    kw_remove_local_storage_value,
    kw_set_local_storage_value,
    json_object_update_existing,
    is_object,
    kw_clone_by_keys,
} from "./helpers.js";

import {
    gobj_short_name
} from "./gobj.js";

/************************************************************
 *
 ************************************************************/
function _get_persistent_path(gobj)
{
    return "persistent-attrs-" + gobj_short_name(gobj);
}

/************************************************************
 *
 ************************************************************/
function db_load_persistent_attrs(gobj)
{
    let attrs = kw_get_local_storage_value(_get_persistent_path(gobj), null, false);
    if(attrs && is_object(attrs)) {
        json_object_update_existing(
            gobj.config,
            kw_clone_by_keys(attrs, gobj.gobj_get_writable_attrs())
        );
    }
}

/************************************************************
 *
 ************************************************************/
function db_save_persistent_attrs(gobj)
{
    kw_set_local_storage_value(
        _get_persistent_path(gobj),
        kw_clone_by_keys(gobj.config, gobj.gobj_get_writable_attrs())
    );
}

/************************************************************
 *
 ************************************************************/
function db_remove_persistent_attrs(gobj)
{
    kw_remove_local_storage_value(_get_persistent_path(gobj));
}

/************************************************************
 *
 ************************************************************/
function db_list_persistent_attrs()
{
    // TODO
}

//=======================================================================
//      Expose the class via the global object
//=======================================================================
export {
    db_load_persistent_attrs,
    db_save_persistent_attrs,
    db_remove_persistent_attrs,
    db_list_persistent_attrs,
};
