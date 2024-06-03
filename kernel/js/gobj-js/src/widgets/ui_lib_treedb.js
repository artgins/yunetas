/***********************************************************************
 *          ui_lib_treedb.js
 *
 *          Treedb common helpers
 *
 *  - El treedb center se registra,
 *
 *  - Los formtable con treedb_name
 *      se suscriben masivamente a las tablas de los hooks y fkeys.
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
 *  2.0
 *
 *          Copyright (c) 2020-2024 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    "use strict";

    /********************************************
     *      Data
     ********************************************/
    let treedb_register = { // Dictionary with data of registered global topics
    };

    /********************************************
     *
     ********************************************/
    function treedb_register_formtable(treedb_name, topic_name, gobj_formtable)
    {
        let treedb = kw_get_dict_value(treedb_register, treedb_name, {}, true);
        let gobjs = kw_get_dict_value(treedb, "gobjs", {}, true);
        kw_set_dict_value(gobjs, gobj_formtable.name, gobj_formtable);
    }

    /********************************************
     *
     ********************************************/
    function treedb_unregister_formtable(treedb_name, topic_name, gobj_formtable)
    {
        let treedb = kw_get_dict_value(treedb_register, treedb_name, {}, true);
        let gobjs = kw_get_dict_value(treedb, "gobjs", {}, true);
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
        let treedb = kw_get_dict_value(treedb_register, treedb_name, {}, true);
        let data = kw_get_dict_value(treedb, "data", {}, true);
        kw_set_dict_value(data, topic_name, kwid_new_dict(nodes));
    }

    /********************************************
     *
     ********************************************/
    function treedb_register_update_node(treedb_name, topic_name, node)
    {
        let treedb = kw_get_dict_value(treedb_register, treedb_name, {}, true);
        let data = kw_get_dict_value(treedb, "data", {}, true);
        let topic_data = kw_get_dict_value(data, topic_name, {}, true);

        kw_set_dict_value(topic_data, node.id, node);
    }

    /********************************************
     *
     ********************************************/
    function treedb_register_del_node(treedb_name, topic_name, node)
    {
        let treedb = kw_get_dict_value(treedb_register, treedb_name, {}, true);
        let data = kw_get_dict_value(treedb, "data", {}, true);
        let topic_data = kw_get_dict_value(data, topic_name, {}, true);

        delete topic_data[node.id];
    }

    /********************************************
     *  Used by ui_treedb to update options
     ********************************************/
    function treedb_get_register(treedb_name)
    {
        let treedb = kw_get_dict_value(treedb_register, treedb_name, {}, false);
        return treedb;
    }

    /********************************************
     *  Used by formtable to get combo options
     ********************************************/
    function treedb_get_topic_data(treedb_name, topic_name)
    {
        let treedb = kw_get_dict_value(treedb_register, treedb_name, {}, false);
        let data = kw_get_dict_value(treedb, "data", {}, false);
        return kw_get_dict_value(data, topic_name, {}, false);
    }

    /********************************************
     *  Used by formtable to get combo options
     ********************************************/
    function treedb_get_topic_field_data(treedb_name, topic_name)
    {
        let treedb = kw_get_dict_value(treedb_register, treedb_name, {}, false);
        let data = kw_get_dict_value(treedb, "data", {}, false);
        return kw_get_dict_value(data, topic_name, {}, false);
    }

    /********************************************
     *
     ********************************************/
    function treedb_hook_data_size(value)
    {
        if(is_array(value) && json_size(value)===1) {
            let o = value[0];
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
    function treedb_decoder_fkey(col, fkey)
    {
        if(is_string(fkey)) {
            if(fkey.indexOf("^") === -1) {
                // "$id"
                let fkey_desc = col.fkey;
                if(json_object_size(fkey_desc)!==1) {
                    log_error("bad fkey 1");
                    log_error(col);
                    log_error(fkey);
                    return null;
                }

                let topic_name;
                let hook_name;
                for(let k in fkey_desc) {
                    topic_name = k;
                    hook_name = fkey_desc[k];
                    break;
                }

                return {
                    topic_name: topic_name,
                    id: fkey,
                    hook_name: hook_name
                };

            } else {
                // "$topic_name^$id^$hook_name
                let tt = fkey.split("^");
                if(tt.length !== 3) {
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
    function treedb_decoder_hook(col, hook)
    {
        if(is_string(hook)) {
            if(hook.indexOf("^") === -1) {
                // "$id"
                let hook_desc = col.hook;
                if(json_object_size(hook_desc)!==1) {
                    log_error("bad hook 1");
                    log_error(col);
                    log_error(hook);
                    return null;
                }

                let topic_name;
                let fkey_name;

                for(let k in hook_desc) {
                    topic_name = k;
                    fkey_name = hook_desc[k];
                    break;
                }

                return {
                    topic_name: topic_name,
                    id: hook
                };

            } else {
                // "$topic_name^$id
                let tt = hook.split("^");
                if(tt.length !== 2) {
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

    /************************************************************
     *  Return [type, real_type, enum_list]
     *
     *  type:
     *      "string"
     *      "integer"
     *      "real"
     *      "boolean"
     *      "object" "dict"
     *      "array" "list"
     *      "blob"
     *      "rowid"
     *      "enum"
     *              real_type:  "string"
     *                          "object" "dict"
     *                          "array" "list"
     *      "hook"
     *              real_type:  "string"
     *                          "object" "dict"
     *                          "array" "list"
     *      "fkey"
     *              real_type:  "string"
     *                          "object" "dict"
     *                          "array" "list"
     *      "time"
     *      "now"
     *              real_type:  "string"
     *                          "integer"
     *      "color"
     *              real_type:  "string"
     *                          "integer"
     *      "password"
     *      "email"
     *      "url"
     *      "image"
     *      "tel"
     *
     ************************************************************/
    function treedb_get_type(col)
    {
        let flag = col.flag;
        let is_rowid = elm_in_list("rowid", flag);
        let is_enum = elm_in_list("enum", flag);
        let is_hook = elm_in_list("hook", flag);
        let is_fkey = elm_in_list("fkey", flag);
        let is_time = elm_in_list("time", flag);
        let is_now = elm_in_list("now", flag);
        let is_color = elm_in_list("color", flag);
        let is_password = elm_in_list("password", flag);
        let is_email = elm_in_list("email", flag);
        let is_url = elm_in_list("url", flag);
        let is_image = elm_in_list("image", flag);
        let is_tel = elm_in_list("tel", flag);

        let enum_list = null;
        let real_type = col.type;
        let type = col.type; // By default, is basic type
        if(is_hook) {
            type = "hook";
        } else if(is_fkey) {
            type = "fkey";
        } else if(is_enum) {
            type = "enum";
            enum_list = col.enum;
        } else if(is_rowid) {
            type = "rowid";
        } else if(is_time) {
            type = "time";
            if(is_now) {
                type = "now";
            }
        } else if(is_color) {
            type = "color";
        } else if(is_email) {
            type = "email";
        } else if(is_password) {
            type = "password";
        } else if(is_url) {
            type = "url";
        } else if(is_image) {
            type = "image";
        } else if(is_tel) {
            type = "tel";
        }

        return [type, real_type, enum_list];
    }


    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    // WARNING DEPRECATED estas funciones se usaban para actualizar enlaces, ya no hacen falta
    //                    el backend publica tb los nodos cuyos links cambian
    // exports.treedb_register_formtable = treedb_register_formtable;          // WARNING DEPRECATED
    // exports.treedb_unregister_formtable = treedb_unregister_formtable;      // WARNING DEPRECATED
    // exports.treedb_register_nodes = treedb_register_nodes;                  // WARNING DEPRECATED
    // exports.treedb_register_update_node = treedb_register_update_node;      // WARNING DEPRECATED
    // exports.treedb_register_del_node = treedb_register_del_node;            // WARNING DEPRECATED
    // exports.treedb_get_register = treedb_get_register;                      // WARNING DEPRECATED
    // exports.treedb_get_topic_data = treedb_get_topic_data;                  // WARNING DEPRECATED

    exports.treedb_hook_data_size = treedb_hook_data_size;
    exports.treedb_decoder_fkey = treedb_decoder_fkey;
    exports.treedb_decoder_hook = treedb_decoder_hook;
    exports.treedb_get_type = treedb_get_type;

})(this);
