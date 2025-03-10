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
    is_object,
    kw_clone_by_keys,
    json_object_update_missing,
} from "./helpers.js";

import {
    sdata_flag_t,
    gobj_read_attrs,
    gobj_short_name,
    gobj_write_attrs,
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
function db_load_persistent_attrs(
    gobj,
    keys  // str, list or dict.
)
{
    let jn_file = kw_get_local_storage_value(_get_persistent_path(gobj), null, false);
    if(jn_file && is_object(jn_file)) {
        let attrs = kw_clone_by_keys(
            gobj,
            jn_file,    // owned
            keys,       // owned
            false
        );

        gobj_write_attrs(gobj, attrs, sdata_flag_t.SDF_PERSIST, 0);
    }
}

/************************************************************
 *
 ************************************************************/
function db_save_persistent_attrs(
    gobj,
    keys  // str, list or dict.
)
{
    let jn_attrs = gobj_read_attrs(gobj, sdata_flag_t.SDF_PERSIST, 0);
    let attrs = kw_clone_by_keys(
        gobj,
        jn_attrs,   // owned
        keys,       // owned
        false
    );

    let jn_file = kw_get_local_storage_value(_get_persistent_path(gobj), null, false);
    if(jn_file && is_object(jn_file)) {
        json_object_update_missing(attrs, jn_file);
    }

    kw_set_local_storage_value(
        _get_persistent_path(gobj),
        attrs
    );
}

/************************************************************
 *
 ************************************************************/
function db_remove_persistent_attrs(
    gobj,
    keys  // str, list or dict.
)
{
    let jn_file = kw_get_local_storage_value(_get_persistent_path(gobj), null, false);

    let attrs = kw_clone_by_not_keys(
        gobj,
        jn_file,
        keys,
        false
    );

    kw_set_local_storage_value(
        _get_persistent_path(gobj),
        attrs
    );
}

/************************************************************
 *
 ************************************************************/
function db_list_persistent_attrs(gobj)
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
