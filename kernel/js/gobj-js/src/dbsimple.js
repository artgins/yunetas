/*********************************************************************************
 *          dbsimple.js
 *
 *          Author: Niyamaka
 *          Email: Niyamaka at yuneta.io
 *          Licence: MIT (http://www.opensource.org/licenses/mit-license)
 *********************************************************************************/

(function (exports) {
    "use strict";

    /************************************************************
     *
     ************************************************************/
    function _get_persistent_path(gobj)
    {
        var path = "persistent-attrs-" + gobj.gobj_short_name();
        return path;
    }

    /************************************************************
     *
     ************************************************************/
    function db_load_persistent_attrs(gobj)
    {
        var attrs = kw_get_local_storage_value(_get_persistent_path(gobj), null, false);
        if(attrs && is_object(attrs)) {
            __update_dict__(
                gobj.config,
                filter_dict(attrs, gobj.gobj_get_writable_attrs())
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
            filter_dict(gobj.config, gobj.gobj_get_writable_attrs())
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
    exports.db_load_persistent_attrs = db_load_persistent_attrs;
    exports.db_save_persistent_attrs = db_save_persistent_attrs;
    exports.db_remove_persistent_attrs = db_remove_persistent_attrs;
    exports.db_list_persistent_attrs = db_list_persistent_attrs;

})(this);
