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
import {
    kw_get_dict_value,
    is_array,
    kw_set_dict_value,
    kw_has_key,
    log_error,
    is_string,
    is_object,
    json_object_size,
    json_size,
    elm_in_list,
    kwid_new_dict,
} from "./helpers.js";

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
    let treedb = kw_get_dict_value(null, treedb_register, treedb_name, {}, true);
    let gobjs = kw_get_dict_value(null, treedb, "gobjs", {}, true);
    kw_set_dict_value(null, gobjs, gobj_formtable.name, gobj_formtable);
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
 *      "$topic_name^$id^$hook_name
 *
 *      or
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
function treedb_decoder_fkey(fkey)
{
    if(is_string(fkey)) {
        // "$topic_name^$id^$hook_name
        let tt = fkey.split("^");
        if(tt.length !== 3) {
            log_error("bad fkey 2");
            log_error(fkey);
            return null;
        }
        return {
            topic_name: tt[0],
            id: tt[1],
            hook_name: tt[2]
        };

    } else if(is_object(fkey)) {
        return {
            topic_name: fkey.topic_name,
            id: fkey.id,
            hook_name: fkey.hook_name
        };

    } else {
        log_error("bad fkey 3");
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

            for(let k of Object.keys(hook_desc)) {
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
 *      Return [
 *          name,           // name/id of field (table column name)
 *          header,         // header of field
 *          type,           // yuneta type (basic and tranger/treedb types)
 *          real_type,      // json type
 *          enum_list,      // List of options to <select> or template or table or ...
 *          is_required,    // field required
 *          is_writable     // field writable (no readonly)
 *          default_value
 *          placeholder
 *      ]
 *
 *  The template is recursive, in values you can set dictionaries, or arrays,
 *  in some context will define the real_type,
 *  in other will define a subform.
 *
 ************************************************************/
const treedb_real_types = [ // WARNING check 31_tr_treedb.h to update new real types
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
    "hidden",
    "stats",        // implicit readable
    "rstats",       // implicit stats
    "pstats"        // implicit stats
];

const treedb_field_types = [ // WARNING check 31_tr_treedb.h to update new types
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
    "base64",
    "coordinates"
];

function treedb_get_field_desc(col)
{
    let field_desc = {
        name: col.id,
        header: col.header,
        real_type: col.type,
        type: col.type, // By default, is basic type
        enum_list: null,
        is_required: false,
        is_writable: false,    // Must be explicitly writable
        is_hidden: false,
        default_value: col.default,
        placeholder: col.placeholder,
    };

    if(!col.flag) {
        // It can happen if col is not a column descriptor
        return field_desc;
    }

    for(let i=0; i<col.flag.length; i++) {
        let f = col.flag[i];

        if(elm_in_list(f, treedb_field_types)) {
            switch(f) {
                case "enum":
                    field_desc.enum_list = col.enum;
                    break;
                case "template":
                    field_desc.enum_list = col.template;
                    break;
                case "table":
                    field_desc.enum_list = col.table;
                    break;
            }
            field_desc.type = f;

        } else if(elm_in_list(f, treedb_field_attributes)) {
            switch(f) {
                case "writable":
                    field_desc.is_writable = true;
                    break;
                case "notnull":
                    field_desc.is_required = true;
                    break;
                case "required":
                    field_desc.is_required = true;
                    break;
                case "hidden":
                    field_desc.is_hidden = true;
                    break;
            }
        }
    }

    return field_desc;
}

function template_get_field_desc(key, value)
{
    let field_desc = {
        name : key,
        header: key,
        real_type: null,
        type: null,
        enum_list: null,
        is_required: false,
        is_writable: false,    // Must be explicitly writable
        is_hidden: false,
        default_value: undefined,
        placeholder: undefined,
    };

    /*---------------------------*
     *      process value
     *---------------------------*/
    if(is_string(value)) {
        let values = value.split('.');
        for(let i=0; i<values.length; i++) {
            let f = values[i];

            if(elm_in_list(f, treedb_real_types)) {
                if(!field_desc.real_type) {
                    field_desc.real_type = f;
                }
            } else if(elm_in_list(f, treedb_field_types)) {
                if(!field_desc.type) {
                    field_desc.type = f;
                }
            } else if(elm_in_list(f, treedb_field_attributes)) {
                switch(f) {
                    case "writable":
                        field_desc.is_writable = true;
                        break;
                    case "notnull":
                        field_desc.is_required = true;
                        break;
                    case "required":
                        field_desc.is_required = true;
                        break;
                    case "hidden":
                        field_desc.is_hidden = true;
                        break;
                }
            }
        }
        if(!field_desc.real_type) {
            field_desc.real_type = "string";
        }

    } else if(is_object(value)) {
        field_desc = treedb_get_field_desc(value);
        if(!field_desc.name) {
            field_desc.name = key;
        }
        if(!field_desc.header) {
            field_desc.header = key;
        }

    } else if(is_array(value)) {
        // TODO case impossible?
        field_desc.real_type = "array";
        field_desc.enum_list = value;
    }

    if(!field_desc.type) {
        field_desc.type = field_desc.real_type;
    }
    return field_desc;
}

/********************************************
 *
 ********************************************/
function create_template_record(template, kw)
{
    let new_record = {};
    if(!kw) {
        kw = {};
    }

    Object.entries(template).forEach(([field, col]) => {
        let value = kw_get_dict_value(null, kw, field, kw_get_dict_value(col, "default"));
        new_record[field] = value;
    });

    return new_record;
}

export {
    treedb_hook_data_size,
    treedb_decoder_fkey,
    treedb_decoder_hook,
    treedb_get_field_desc,
    template_get_field_desc,
    create_template_record,
};
