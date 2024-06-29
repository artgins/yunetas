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
     *  Return [name, type, real_type, enum_list, is_required, is_writable]
     *
     *  type:
     *      "string"
     *      "integer"
     *      "real"
     *      "number" ("integer" or "real")
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
     *      "template"
     *      "id"
     *      "currency"
     *      "hex"
     *      "binary"
     *      "percent"
     *      "base64"
     *
     ************************************************************/
    const treedb_real_types = [
        "string",
        "integer",
        "object", "dict",
        "array","list",
        "real",
        "boolean",
        "blob",
        "number"
    ];

    const treedb_field_attributes = [
        "persistent",   // implicit readable
        "required",
        "notnull",
        "wild",
        "inherit",
        "readable",
        "writable",     // implicit readable
        "stats",        // implicit readable
        "rstats",       // implicit stats
        "pstats"        // implicit stats
    ];

    const treedb_field_types = [
        "fkey",
        "hook",
        "enum",
        "uuid",
        "rowid",
        "password",
        "email",
        "url",
        "time",
        "now",
        "date",
        "color",
        "image",
        "tel",
        "template",
        "table",
        "id",
        "currency",
        "hex",
        "binary",
        "percent",
        "base64"
    ];

    function treedb_get_type(col)
    {
        let name = col.id;
        let real_type = col.type;
        let type = col.type; // By default, is basic type
        let enum_list = null;
        let is_required = false;
        let is_writable = false;    // Must be explicitly writable

        for(let i=0; i<col.flag.length; i++) {
            let f = col.flag[i];

            // Code repeated
            if(elm_in_list(f, treedb_field_types)) {
                switch(f) {
                    case "enum":
                        enum_list = col.enum;
                        break;
                    case "template":
                        enum_list = col.template;
                        break;
                }
                type = f;

            } else if(elm_in_list(f, treedb_field_attributes)) {
                switch(f) {
                    case "writable":
                        is_writable = true;
                        break;
                    case "notnull":
                        is_required = true;
                        is_writable = true;
                        break;
                    case "required":
                        is_required = true;
                        is_writable = true;
                        break;
                }
            }
        }

        return [name, type, real_type, enum_list, is_required, is_writable];
    }

    /******************************************************************
     *          Extract from template the field description
     *
     *      Return [
     *          name,           // name/id of field (table column name)
     *          type,           // yuneta type (basic and tranger/treedb types)
     *          real_type,      // json type
     *          enum_list,      // List of options to <select> or similar
     *          is_required,    // field required
     *          is_writable     // field writable (no readonly)
     *      ]
     *
     *  The template is recursive, in values you can set dictionaries, or arrays,
     *  in some context will define the real_type,
     *  in other will define a subform.
     *
     *  There are many combinations to write what you want:
     *
     *  key:    name
     *          name.real_type
     *          name.real_type.type
     *          name.real_type.type.required
     *          name.real_type.type.required.readonly
     *
     *  value:  if string
     *              real_type
     *              real_type.type
     *              real_type.type.required
     *              real_type.type.required.readonly
     *
     *          real_type |type | enum_list
     *
     ******************************************************************/
    function template_get_type(key, value)
    {
        let name = null;
        let real_type = null;
        let type = null;
        let enum_list = null;
        let is_writable = false;
        let is_required = false;

        /*---------------------------*
         *      process key
         *---------------------------*/
        let keys = key.split('.');
        for(let i=0; i<keys.length; i++) {
            let f = keys[i];
            if(i===0) {
                name = f;
            } else {
                // Code repeated
                if(elm_in_list(f, treedb_real_types)) {
                    real_type = f;
                } else if(elm_in_list(f, treedb_field_types)) {
                    type = f;
                } else if(elm_in_list(f, treedb_field_attributes)) {
                    switch(f) {
                        case "writable":
                            is_writable = true;
                            break;
                        case "notnull":
                            is_writable = true;
                            is_required = true;
                            break;
                        case "required":
                            is_writable = true;
                            is_required = true;
                            break;
                    }
                }
            }
        }

        /*---------------------------*
         *      process value
         *---------------------------*/
        if(is_string(value)) {
            let values = value.split('.');
            for(let i=0; i<values.length; i++) {
                let f = values[i];

                // Code repeated
                if(elm_in_list(f, treedb_real_types)) {
                    if(!real_type) {
                        real_type = f;
                    }
                } else if(elm_in_list(f, treedb_field_types)) {
                    if(!type) {
                        type = f;
                    }
                } else if(elm_in_list(f, treedb_field_attributes)) {
                    switch(f) {
                        case "writable":
                            is_writable = true;
                            break;
                        case "notnull":
                            is_writable = true;
                            is_required = true;
                            break;
                        case "required":
                            is_writable = true;
                            is_required = true;
                            break;
                    }
                }
            }
            if(!real_type) {
                real_type = "string";
            }

        } else if(is_object(value)) {
            if(!real_type) {
                real_type = "object";
            }
            enum_list = value;

        } else if(is_array(value)) {
            if(!real_type) {
                real_type = "array";
            }
            for(let j=0; j<value.length; j++) {
                let ff = value[j];
                if(is_string(ff)) {
                    if(elm_in_list(ff, treedb_field_types)) {
                        if(!type) {
                            type = ff;
                        }
                    } else if(elm_in_list(ff, treedb_field_attributes)) {
                        switch(ff) {
                            case "writable":
                                is_writable = true;
                                break;
                            case "notnull":
                                is_writable = true;
                                is_required = true;
                                break;
                            case "required":
                                is_writable = true;
                                is_required = true;
                                break;
                        }
                    }
                }
            }
            enum_list = value;
        }

        if(!type) {
            type = real_type;
        }
        return [name, type, real_type, enum_list, is_required, is_writable];
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
    exports.template_get_type = template_get_type;
})(this);
