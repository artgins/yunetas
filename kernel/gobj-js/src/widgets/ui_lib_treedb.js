/***********************************************************************
 *          ui_lib_treedb.js
 *
 *          Treedb common helpers
 *
 *  - El treedb center se registra,
 *
 *  - Los formtable con treedb_name
 *      se subcriben masivamente a las tablas de los hooks y fkeys.
 *
 *  - Los hook se subscriben a todos las modificaciones-z de los hijos,
 *    ( solo de los create y delete realmente, solo se necesita el id del que existe)
 *    en cada una redefinirán sus option2list, así un padre puede cortar el link.
 *    pero puede ser un proceso lento.
 *
 *  - Los fkeys se subscriben a todas las modificaciones de los padres,
 *    realmente solo se necesita a las creaciones y deletes de los registros de la tabla,
 *    updates y link/unlink le da igual. Con cada publicación redefinirán sus option2list.
 *
 * OJO con los unsubscribe!! necesarios!
 *
 *  El formtable publicará los eventos: CREATE_NODE, DELETE_NODE, UPDATE_NODE
 *  en el UPDATE_NODE irá un reseteo/creación de los links.
 *  OJO que un link/unlink implica un update de dos nodos, el padre y el hijo. Quién primero?
 *
 *
 *  Version
 *  -------
 *  1.0     Initial release
 *
 *          Copyright (c) 2020 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    "use strict";

    /********************************************
     *      Data
     ********************************************/
    var treedb_register = { // Dictionary with data of registered global topics
    };

    /********************************************
     *
     ********************************************/
    function treedb_register_formtable(treedb_name, topic_name, gobj_formtable)
    {
        var treedb = kw_get_dict_value(treedb_register, treedb_name, {}, true);
        var gobjs = kw_get_dict_value(treedb, "gobjs", {}, true);
        kw_set_dict_value(gobjs, gobj_formtable.name, gobj_formtable);
    }

    /********************************************
     *
     ********************************************/
    function treedb_unregister_formtable(treedb_name, topic_name, gobj_formtable)
    {
        var treedb = kw_get_dict_value(treedb_register, treedb_name, {}, true);
        var gobjs = kw_get_dict_value(treedb, "gobjs", {}, true);
        if(kw_has_key(gobjs, gobj_formtable.name)) {
            delete gobjs[gobj_formtable.name];
        } else {
            log_error("treedb_unregister_formtable() gobj not found: " + gobj_formtable.name);
        }
    }

    /********************************************
     *
     ********************************************/
    function treedb_register_nodes(treedb_name, topic_name, nodes)
    {
        var treedb = kw_get_dict_value(treedb_register, treedb_name, {}, true);
        var data = kw_get_dict_value(treedb, "data", {}, true);
        kw_set_dict_value(data, topic_name, kwid_new_dict(nodes));
    }

    /********************************************
     *
     ********************************************/
    function treedb_register_update_node(treedb_name, topic_name, node)
    {
        var treedb = kw_get_dict_value(treedb_register, treedb_name, {}, true);
        var data = kw_get_dict_value(treedb, "data", {}, true);
        var topic_data = kw_get_dict_value(data, topic_name, {}, true);

        kw_set_dict_value(topic_data, node.id, node);
    }

    /********************************************
     *
     ********************************************/
    function treedb_register_del_node(treedb_name, topic_name, node)
    {
        var treedb = kw_get_dict_value(treedb_register, treedb_name, {}, true);
        var data = kw_get_dict_value(treedb, "data", {}, true);
        var topic_data = kw_get_dict_value(data, topic_name, {}, true);

        delete topic_data[node.id];
    }

    /********************************************
     *  Used by ui_treedb to update options
     ********************************************/
    function treedb_get_register(treedb_name)
    {
        var treedb = kw_get_dict_value(treedb_register, treedb_name, {}, false);
        return treedb;
    }

    /********************************************
     *  Used by formtable to get combo options
     ********************************************/
    function treedb_get_topic_data(treedb_name, topic_name)
    {
        var treedb = kw_get_dict_value(treedb_register, treedb_name, {}, false);
        var data = kw_get_dict_value(treedb, "data", {}, false);
        return kw_get_dict_value(data, topic_name, {}, false);
    }

    /********************************************
     *
     ********************************************/
    function treedb_hook_data_size(value)
    {
        if(is_array(value) && json_size(value)==1) {
            var o = value[0];
            if(kw_has_key(o, "size")) {
                return o.size;
            }
            return 1;
        }
        return json_size(value);
    }

    /************************************************************
     *  fkey can be:
     *
     *      "$id"
     *
     *      "$topic_name^$id^$hook_name
     *
     *      {
     *          topic_name: $topic_name,
     *          id: $id,
     *          hook_name: $hook_name
     *      }
     *
     *  Return
     *      {
     *          topic_name:
     *          id:
     *          hook_name:
     *      }
     ************************************************************/
    function decoder_fkey(col, fkey)
    {
        if(is_string(fkey)) {
            if(fkey.indexOf("^") == -1) {
                // "$id"
                var fkey_desc = col.fkey;
                if(json_object_size(fkey_desc)!=1) {
                    log_error("bad fkey 1");
                    log_error(col);
                    log_error(fkey);
                    return null;
                }

                for(var k in fkey_desc) {
                    var topic_name = k;
                    var hook_name = fkey_desc[k];
                    break;
                }

                return {
                    topic_name: topic_name,
                    id: fkey,
                    hook_name: hook_name
                };

            } else {
                // "$topic_name^$id^$hook_name
                var tt = fkey.split("^");
                if(tt.length != 3) {
                    log_error("bad fkey 2");
                    log_error(col);
                    log_error(fkey);
                    return null;
                }
                return {
                    topic_name: tt[0],
                    id: tt[1],
                    hook_name: tt[2]
                };
            }

        } else if(is_object(fkey)) {
            return {
                topic_name: fkey.topic_name,
                id: fkey.id,
                hook_name: fkey.hook_name
            };

        } else {
            log_error("bad fkey 3");
            log_error(col);
            log_error(fkey);
            return null;
        }
    }

    /********************************************
     *  hook can be:
     *
     *      "$id"
     *
     *      "$topic_name^$id
     *
     *      {
     *          topic_name: $topic_name,
     *          id: $id
     *      }
     *
     *  Return
     *      {
     *          topic_name:
     *          id:
     *      }
     ********************************************/
    function decoder_hook(col, hook)
    {
        if(is_string(hook)) {
            if(hook.indexOf("^") == -1) {
                // "$id"
                var hook_desc = col.hook;
                if(json_object_size(hook_desc)!=1) {
                    log_error("bad hook 1");
                    log_error(col);
                    log_error(hook);
                    return null;
                }

                for(var k in hook_desc) {
                    var topic_name = k;
                    var fkey_name = hook_desc[k];
                    break;
                }

                return {
                    topic_name: topic_name,
                    id: hook
                };

            } else {
                // "$topic_name^$id
                var tt = hook.split("^");
                if(tt.length != 2) {
                    log_error("bad hook 2");
                    log_error(col);
                    log_error(hook);
                    return null;
                }
                return {
                    topic_name: tt[0],
                    id: tt[1]
                };
            }

        } else if(is_object(hook)) {
            return {
                topic_name: hook.topic_name,
                id: hook.id
            };

        } else {
            log_error("bad hook 3");
            log_error(col);
            log_error(hook);
            return null;
        }
    }

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.treedb_register_formtable = treedb_register_formtable;
    exports.treedb_unregister_formtable = treedb_unregister_formtable;
    exports.treedb_register_nodes = treedb_register_nodes;
    exports.treedb_register_update_node = treedb_register_update_node;
    exports.treedb_register_del_node = treedb_register_del_node;
    exports.treedb_get_register = treedb_get_register;
    exports.treedb_get_topic_data = treedb_get_topic_data;
    exports.treedb_hook_data_size = treedb_hook_data_size;

    exports.decoder_fkey = decoder_fkey;
    exports.decoder_hook = decoder_hook;

})(this);
