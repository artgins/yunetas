/****************************************************************************
 *              kwid.c
 *
 *              kw helpers
 *
 *              Copyright (c) 2014,2023 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include "helpers.h"
#include "kwid.h"

/***************************************************************
 *              Constants
 ***************************************************************/
#define MAX_SERIALIZED_FIELDS 4

/***************************************************************
 *              Log Structures
 ***************************************************************/

/***************************************************************
 *              KW0 Structures
 ***************************************************************/
typedef struct serialize_fields_s {
    const char *binary_field_name;
    const char *serialized_field_name;
    serialize_fn_t serialize_fn;
    deserialize_fn_t deserialize_fn;
    incref_fn_t incref_fn;
    decref_fn_t decref_fn;
} serialize_fields_t;

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE char delimiter[2] = {'`',0};
PRIVATE int max_serialize_slot = 0;
PRIVATE serialize_fields_t serialize_fields[MAX_SERIALIZED_FIELDS+1];




                    /*---------------------------------*
                     *      SECTION: KW0
                     *---------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int kw_add_binary_type(
    const char *binary_field_name,
    const char *serialized_field_name,
    serialize_fn_t serialize_fn,
    deserialize_fn_t deserialize_fn,
    incref_fn_t incref_fn,
    decref_fn_t decref_fn)
{
    if(max_serialize_slot >= MAX_SERIALIZED_FIELDS) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "table serialize fields FULL",
            NULL
        );
        return -1;
    }
    serialize_fields_t * pf = serialize_fields + max_serialize_slot;
    pf->binary_field_name = binary_field_name;
    pf->serialized_field_name = serialized_field_name;
    pf->serialize_fn = serialize_fn;
    pf->deserialize_fn = deserialize_fn;
    pf->incref_fn = incref_fn;
    pf->decref_fn = decref_fn;

    max_serialize_slot++;
    return 0;
}

/***************************************************************************
 *  Serialize fields
 ***************************************************************************/
PUBLIC json_t *kw_serialize( // return the same kw
    hgobj gobj,
    json_t *kw
)
{
    serialize_fields_t * pf = serialize_fields;
    while(pf->binary_field_name) {
        if(kw_has_key(kw, pf->binary_field_name)) {
            /*
             *  Pop the binary field from kw
             */
            void *binary = (void *)(size_t)kw_get_int(
                gobj,
                kw,
                pf->binary_field_name,
                0,
                0
            );

            /*
             *  Serialize
             */
            if(pf->serialize_fn) {
                json_t *jn_serialized = pf->serialize_fn(gobj, binary);

                /*
                 *  Decref binary
                 */
                if(pf->decref_fn) {
                    pf->decref_fn(binary);
                }

                /*
                 *  Save the serialized json field to kw
                 */
                if(jn_serialized) {
                    json_object_set_new(
                        kw,
                        pf->serialized_field_name,
                        jn_serialized
                    );
                } else {
                    gobj_log_error(0, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                        "msg",          "%s", "serialize_fn() FAILED",
                        "key",          "%s", pf->binary_field_name,
                        NULL
                    );
                }
            }
            json_object_del(kw, pf->binary_field_name);
        }
        pf++;
    }
    return kw;
}

/***************************************************************************
 *  Deserialize fields
 ***************************************************************************/
PUBLIC json_t *kw_deserialize( // return the same kw
    hgobj gobj,
    json_t *kw
)
{
    serialize_fields_t * pf = serialize_fields;
    while(pf->serialized_field_name) {
        if(kw_has_key(kw, pf->serialized_field_name)) {
            /*
             *  Pop the serialized json field from kw
             */
            json_t *jn_serialized = kw_get_dict(
                gobj,
                kw,
                pf->serialized_field_name,
                0,
                0
            );

            /*
             *  Deserialize
             */
            if(pf->deserialize_fn) {
                void *binary = pf->deserialize_fn(gobj, jn_serialized);

                /*
                 *  Decref json
                 */
                // JSON_DECREF(jn_serialized); TODO CHECK creo que no hace falta

                /*
                 *  Save the binary field to kw
                 */
                json_object_set_new(
                    kw,
                    pf->binary_field_name,
                    json_integer((json_int_t)(size_t)binary)
                );
            }
            json_object_del(kw, pf->serialized_field_name);

        }
        pf++;
    }
    return kw;
}

/***************************************************************************
 *  Incref json kw and his binary fields
 ***************************************************************************/
PUBLIC json_t *kw_incref(json_t *kw)
{
    if(!kw) {
        return 0;
    }
    if((int)kw->refcount <= 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "BAD kw_incref()",
            "refcount",     "%d", (int)(kw->refcount),
            "type",         "%d", (int)(kw->type),
            NULL
        );
        return 0;
    }

    serialize_fields_t * pf = serialize_fields;
    while(pf->binary_field_name) {
        if(kw_has_key(kw, pf->binary_field_name)) {
            /*
             *  Get the binary field from kw
             */
            void *binary = (void *)(size_t)kw_get_int(
                0,
                kw,
                pf->binary_field_name,
                0,
                0
            );

            if(binary) {
                /*
                *  Incref binary
                */
                if(pf->incref_fn) {
                    pf->incref_fn(binary);
                }
            }
        }
        pf++;
    }
    JSON_INCREF(kw);
    return kw;
}

/***************************************************************************
 *  Decref json kw and his binary fields
 ***************************************************************************/
PUBLIC json_t *kw_decref(json_t* kw)
{
    if(!kw) {
        return 0;
    }
    if((int)(kw->refcount) <= 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "BAD kw_decref()",
            "refcount",     "%d", (int)(kw->refcount),
            "type",         "%d", (int)(kw->type),
            NULL
        );
        return 0;
    }

    serialize_fields_t * pf = serialize_fields;
    while(pf->binary_field_name) {
        if(kw_has_key(kw, pf->binary_field_name)) {
            /*
             *  Get the binary field from kw
             */
            void *binary = (void *)(size_t)kw_get_int(
                0,
                kw,
                pf->binary_field_name,
                0,
                0
            );
            if(binary) {
                /*
                *  Incref binary
                */
                if(pf->decref_fn) {
                    pf->decref_fn(binary);
                }
            }
        }
        pf++;
    }
    JSON_DECREF(kw);
    return kw;
}

/***************************************************************************
 *  Check if the dictionary ``kw`` has the key ``key``
 ***************************************************************************/
PUBLIC BOOL kw_has_key(json_t *kw, const char *key)
{
    if(!json_is_object(kw)) {
        return FALSE;
    }
    if(json_object_get(kw, key)) {
        return TRUE;
    }
    return FALSE;
}

/***************************************************************************
   Set delimiter. Default is '`'
 ***************************************************************************/
PUBLIC char kw_set_path_delimiter(char delimiter_)
{
    char old_delimiter = delimiter[0];
    delimiter[0] = delimiter_;
    return old_delimiter;
}

/***************************************************************************
 *  Return the json value find by path
 *  Walk over dicts and lists
 ***************************************************************************/
PUBLIC json_t *kw_find_path(hgobj gobj, json_t *kw, const char *path, BOOL verbose)
{
    if(!(json_is_object(kw) || json_is_array(kw))) {
        if(verbose) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "kw must be list or dict",
                NULL
            );
        }
        return 0;
    }
    if(empty_string(path)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "path EMPTY",
            NULL
        );
        return 0;
    }
    if(kw->refcount <=0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "json refcount 0",
            "path",         "%s", path,
            NULL
        );
        return 0;
    }

    int list_size = 0;
    const char **segments = split2(path, delimiter, &list_size);

    json_t *v = kw;
    BOOL fin = FALSE;
    int i;
    json_t *next = 0;
    const char *segment = 0;
    for(i=0; i<list_size && !fin; i++) {
        segment = *(segments +i);
        if(!v) {
            if(verbose) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "short path",
                    "path",         "%s", path,
                    "segment",      "%s", segment,
                    NULL
                );
            }
            break;
        }

        switch(json_typeof(v)) {
        case JSON_OBJECT:
            next = json_object_get(v, segment);
            if(!next) {
                fin = TRUE;
                if(verbose) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "path not found",
                        "path",         "%s", path,
                        "segment",      "%s", segment,
                        NULL
                    );
                    gobj_trace_json(gobj, v, "path not found");
                }
            }
            v = next;
            break;
        case JSON_ARRAY:
            {
                int idx = atoi(segment);
                next = json_array_get(v, (size_t)idx);
                if(!next) {
                    fin = TRUE;
                    if(verbose) {
                        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                            "msg",          "%s", "path not found",
                            "path",         "%s", path,
                            "segment",      "%s", segment,
                            "idx",          "%d", idx,
                            NULL
                        );
                        gobj_trace_json(gobj, v, "path not found");
                    }
                }
                v = next;
            }
            break;
        default:
            fin = TRUE;
            break;
        }
    }

    if(i<list_size) {
        if(verbose) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "long path",
                "path",         "%s", path,
                "segment",      "%s", segment,
                NULL
            );
        }
    }

    split_free2(segments);
    return v;
}

/***************************************************************************
 *  Like json_object_set but with a path
 *  (doesn't create arrays, only objects)
 ***************************************************************************/
PUBLIC int kw_set_dict_value(
    hgobj gobj,
    json_t *kw,
    const char *path,   // The last word after delimiter is the key
    json_t *value) // owned
{
    if(!json_is_object(kw)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw MUST BE a json object",
            "path",         "%s", path,
            NULL
        );
        JSON_DECREF(value);
        return -1;
    }

    if(kw->refcount <=0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "json refcount 0",
            "path",         "%s", path,
            NULL
        );
        JSON_DECREF(value);
        return 0;
    }

    int list_size = 0;
    const char **segments = split2(path, delimiter, &list_size);

    json_t *v = kw;
    BOOL fin = FALSE;
    int i;
    const char *segment = 0;
    json_t *next = 0;
    for(i=0; i<list_size && !fin; i++) {
        segment = *(segments +i);
        if(!v) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "short path",
                "path",         "%s", path,
                "segment",      "%s", segment,
                NULL
            );
            break;
        }

        switch(json_typeof(v)) {
        case JSON_OBJECT:
            next = json_object_get(v, segment);
            if(!next) {
                // TODO if not the last segment, write a json_object()
                json_object_set(v, segment, value);
                next = value;
            }
            v = next;
            break;

        case JSON_ARRAY:
            {
                int idx = atoi(segment);
                next = json_array_get(v, (size_t)idx);
                if(!next) {
                    fin = TRUE;
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "path not found",
                        "path",         "%s", path,
                        "segment",      "%s", segment,
                        "idx",          "%d", idx,
                        NULL
                    );
                    gobj_trace_json(gobj, v, "path not found");
                }
                v = next;
            }
            break;
        default:
            fin = TRUE;
            break;
        }
    }

    if(i<list_size) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "long path",
            "path",         "%s", path,
            "segment",      "%s", segment,
            NULL
        );
    }
    JSON_DECREF(value)
    split_free2(segments);

    return 0;
}

/***************************************************************************
 *  Like json_object_set but with a path and subdict.
 ***************************************************************************/
PUBLIC int kw_set_subdict_value(
    hgobj gobj,
    json_t* kw,
    const char *path,
    const char *key,
    json_t *value // owned
) {
    json_t *jn_subdict = kw_get_dict(gobj, kw, path, json_object(), KW_CREATE);
    if(!jn_subdict) {
        // Error already logged
        KW_DECREF(value);
        return -1;
    }
    if(json_object_set_new(jn_subdict, key, value)<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "json_object_set_new() FAILED",
            "path",         "%s", path,
            "key",          "%s", key,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Delete the value searched by path
 ***************************************************************************/
PUBLIC int kw_delete(
    hgobj gobj,
    json_t *kw,
    const char *path
) {
    int ret = 0;
    char *s = GBMEM_STRDUP(path);
    char *k = strrchr(s, delimiter[0]);
    if(k) {
        *k = 0;
        k++;
        json_t *v = kw_find_path(gobj, kw, s, TRUE);
        json_t *jn_item = json_object_get(v, k);
        if(jn_item) {
            json_object_del(v, k);
        } else {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path not found",
                "path",         "%s", s,
                NULL
            );
            gobj_trace_json(gobj, kw, "path not found");
            ret = -1;
        }

    } else {
        json_t *jn_item = json_object_get(kw, path);
        if(jn_item) {
            json_object_del(kw, path);
        } else {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path not found",
                "path",         "%s", path,
                NULL
            );
            gobj_trace_json(gobj, kw, "path not found");
            ret = -1;
        }
    }

    GBMEM_FREE(s);
    return ret;
}

/***************************************************************************
 *   Delete subkey
 ***************************************************************************/
PUBLIC int kw_delete_subkey(hgobj gobj, json_t *kw, const char *path, const char *key)
{
    json_t *jn_dict = kw_get_dict(gobj, kw, path, 0, 0);
    if(!jn_dict) {
        gobj_log_error(gobj,  LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "subdict not found",
            "subdict",      "%s", path,
            NULL
        );
        return -1;
    }
    if(!kw_has_key(jn_dict, key)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "key not found",
            "subdict",      "%s", path,
            "key",          "%s", key,
            NULL
        );
        gobj_trace_json(gobj, kw, "key not found");
        return -1;
    }
    return json_object_del(jn_dict, key);
}

/***************************************************************************
    Get a the idx of simple json item in a json list.
    Return -1 if not found
 ***************************************************************************/
PUBLIC int kw_find_str_in_list(
    hgobj gobj,
    json_t *kw_list,
    const char *str
)
{
    if(!str || !json_is_array(kw_list)) {
        return -1;
    }

    size_t idx;
    json_t *jn_str;
    json_array_foreach(kw_list, idx, jn_str) {
        const char *str_ = json_string_value(jn_str);
        if(!str_) {
            continue;
        }
        if(strcmp(str, str_)==0) {
            return (int)idx;
        }
    }

    return -1;
}

/***************************************************************************
    Get a the idx of simple json item in a json list.
    Return -1 if not found
 ***************************************************************************/
PUBLIC int kw_find_json_in_list(
    json_t *kw_list,  // not owned
    json_t *item  // not owned
)
{
    if(!item || !json_is_array(kw_list)) {
        return -1;
    }

    size_t idx;
    json_t *jn_item;
    json_array_foreach(kw_list, idx, jn_item) {
        if(item == jn_item) {
            return (int)idx;
        }
    }

    return -1;
}

/***************************************************************************
 *  Get an dict value from an json object searched by path
 ***************************************************************************/
PUBLIC json_t *kw_get_dict(
    hgobj gobj,
    json_t *kw,
    const char *path,
    json_t *default_value,
    kw_flag_t flag)
{
    json_t *jn_dict = kw_find_path(gobj, kw, path, FALSE);
    if(!jn_dict) {
        if((flag & KW_CREATE) && default_value && kw) {
            kw_set_dict_value(gobj, kw, path, default_value);
            return default_value;
        }
        if(flag & KW_REQUIRED) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path NOT FOUND, default value returned",
                "path",         "%s", path,
                NULL
            );
            gobj_trace_json(gobj, kw, "path NOT FOUND, default value returned of '%s'", path);
        }
        return default_value;
    }
    if(!json_is_object(jn_dict)) {
        if(flag & KW_REQUIRED) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path MUST BE a json dict, default value returned",
                "path",         "%s", path,
                NULL
            );
            gobj_trace_json(gobj, kw, "path MUST BE a json dict, default value returned of '%s'", path);
        }
        return default_value;
    }
    JSON_DECREF(default_value);

    if(flag & KW_EXTRACT) {
        json_incref(jn_dict);
        kw_delete(gobj, kw, path);
    }

    return jn_dict;
}

/***************************************************************************
 *  Get an list value from an json object searched by path
 ***************************************************************************/
PUBLIC json_t *kw_get_list(
    hgobj gobj,
    json_t *kw,
    const char *path,
    json_t *default_value,
    kw_flag_t flag)
{
    json_t *jn_list = kw_find_path(gobj, kw, path, FALSE);
    if(!jn_list) {
        if((flag & KW_CREATE) && default_value && kw) {
            kw_set_dict_value(gobj, kw, path, default_value);
            return default_value;
        }
        if(flag & KW_REQUIRED) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path NOT FOUND, default value returned",
                "path",         "%s", path,
                NULL
            );
            gobj_trace_json(gobj, kw, "path NOT FOUND, default value returned '%s'", path);
        }
        return default_value;
    }
    if(!json_is_array(jn_list)) {
        if(flag & KW_REQUIRED) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path MUST BE a json list, default value returned",
                "path",         "%s", path,
                NULL
            );
            gobj_trace_json(gobj, kw, "path '%s' MUST BE a json list, default value returned", path);
        }
        return default_value;
    }
    JSON_DECREF(default_value);

    if(flag & KW_EXTRACT) {
        json_incref(jn_list);
        kw_delete(gobj, kw, path);
    }

    return jn_list;
}

/***************************************************************************
 *  Get an int value from an json object searched by path
 ***************************************************************************/
PUBLIC json_int_t kw_get_int(
    hgobj gobj,
    json_t *kw,
    const char *path,
    json_int_t default_value,
    kw_flag_t flag)
{
    json_t *jn_int = kw_find_path(gobj, kw, path, FALSE);
    if(!jn_int) {
        if((flag & KW_CREATE) && kw) {
            json_t *jn_new = json_integer(default_value);
            kw_set_dict_value(gobj, kw, path, jn_new);
            return default_value;
        }
        if(flag & KW_REQUIRED) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path NOT FOUND, default value returned",
                "path",         "%s", path,
                NULL
            );
            gobj_trace_json(gobj, kw, "path NOT FOUND, default value returned '%s'", path);
        }
        return default_value;
    }
    if(!(json_is_real(jn_int) || json_is_integer(jn_int))) {
        if(!(flag & KW_WILD_NUMBER)) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path MUST BE a json integer",
                "path",         "%s", path,
                NULL
            );
            gobj_trace_json(gobj, kw, "path MUST BE a json integer '%s'", path);
            return default_value;
        }
    }

    json_int_t value = 0;
    if(json_is_integer(jn_int)) {
        value = json_integer_value(jn_int);
    } else if(json_is_real(jn_int)) {
        value = (json_int_t)json_real_value(jn_int);
    } else if(json_is_boolean(jn_int)) {
        value = json_is_true(jn_int)?1:0;
    } else if(json_is_string(jn_int)) {
        const char *v = json_string_value(jn_int);
//         if(*v == '0') {
//             value = strtoll(v, 0, 8);
//         } else if(*v == 'x' || *v == 'X') {
//             value = strtoll(v, 0, 16);
//         } else {
//             value = strtoll(v, 0, 10);
//         }
        value = strtoll(v, 0, 0); // WARNING change by this: with base 0 strtoll() does all
    } else if(json_is_null(jn_int)) {
        value = 0;
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "path MUST BE a simple json element",
            "path",         "%s", path,
            NULL
        );
        gobj_trace_json(gobj, kw, "path MUST BE a simple json element '%s'", path);
        return 0;
    }
    if(flag & KW_EXTRACT) {
        kw_delete(gobj, kw, path);
    }
    return value;
}

/***************************************************************************
 *  Get an real value from an json object searched by path
 ***************************************************************************/
PUBLIC double kw_get_real(
    hgobj gobj,
    json_t *kw,
    const char *path,
    double default_value,
    kw_flag_t flag)
{
    json_t *jn_real = kw_find_path(gobj, kw, path, FALSE);
    if(!jn_real) {
        if((flag & KW_CREATE) && kw) {
            json_t *jn_new = json_real(default_value);
            kw_set_dict_value(gobj, kw, path, jn_new);
            return default_value;
        }
        if(flag & KW_REQUIRED) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path NOT FOUND, default value returned",
                "path",         "%s", path,
                NULL
            );
            gobj_trace_json(gobj, kw, "path NOT FOUND, default value returned '%s'", path);
        }
        return default_value;
    }
    if(!(json_is_real(jn_real) || json_is_integer(jn_real))) {
        if(!(flag & KW_WILD_NUMBER)) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path MUST BE a json real",
                "path",         "%s", path,
                NULL
            );
            gobj_trace_json(gobj, kw, "path MUST BE a json real '%s'", path);
            return default_value;
        }
    }

    double value = 0;
    if(json_is_real(jn_real)) {
        value = json_real_value(jn_real);
    } else if(json_is_integer(jn_real)) {
        value = (double)json_integer_value(jn_real);
    } else if(json_is_boolean(jn_real)) {
        value = json_is_true(jn_real)?1.0:0.0;
    } else if(json_is_string(jn_real)) {
        value = atof(json_string_value(jn_real));
    } else if(json_is_null(jn_real)) {
        value = 0;
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "path MUST BE a simple json element",
            "path",         "%s", path,
            NULL
        );
        gobj_trace_json(gobj, kw, "path MUST BE a simple json element '%s'", path);
        return 0;
    }
    if(flag & KW_EXTRACT) {
        kw_delete(gobj, kw, path);
    }
    return value;
}

/***************************************************************************
 *  Get a bool value from an json object searched by path
 ***************************************************************************/
PUBLIC BOOL kw_get_bool(
    hgobj gobj,
    json_t *kw,
    const char *path,
    BOOL default_value,
    kw_flag_t flag)
{
    json_t *jn_bool = kw_find_path(gobj, kw, path, FALSE);
    if(!jn_bool) {
        if((flag & KW_CREATE) && kw) {
            json_t *jn_new = json_boolean(default_value);
            kw_set_dict_value(gobj, kw, path, jn_new);
            return default_value;
        }
        if(flag & KW_REQUIRED) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path NOT FOUND, default value returned",
                "path",         "%s", path,
                NULL
            );
            gobj_trace_json(gobj, kw, "path NOT FOUND, default value returned '%s'", path);
        }
        return default_value;
    }
    if(!json_is_boolean(jn_bool)) {
        if(!(flag & KW_WILD_NUMBER)) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path MUST BE a json boolean",
                "path",         "%s", path,
                NULL
            );
            gobj_trace_json(gobj, kw, "path MUST BE a json boolean '%s'", path);
            return default_value;
        }
    }

    BOOL value = 0;
    if(json_is_boolean(jn_bool)) {
        value = json_is_true(jn_bool)?1:0;
    } else if(json_is_integer(jn_bool)) {
        value = json_integer_value(jn_bool)?1:0;
    } else if(json_is_real(jn_bool)) {
        value = json_real_value(jn_bool)?1:0;
    } else if(json_is_string(jn_bool)) {
        const char *v = json_string_value(jn_bool);
        if(strcasecmp(v, "true")==0) {
            value = 1;
        } else if(strcasecmp(v, "false")==0) {
            value = 0;
        } else {
            value = atoi(v)?1:0;
        }
    } else if(json_is_null(jn_bool)) {
        value = 0;
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "path MUST BE a simple json element",
            "path",         "%s", path,
            NULL
        );
        gobj_trace_json(gobj, kw, "path MUST BE a simple json element '%s'", path);
        return 0;
    }
    if(flag & KW_EXTRACT) {
        kw_delete(gobj, kw, path);
    }
    return value;
}

/***************************************************************************
 *  Get a string value from an json object searched by path
 ***************************************************************************/
PUBLIC const char *kw_get_str(
    hgobj gobj,
    json_t *kw,
    const char *path,
    const char *default_value,
    kw_flag_t flag)
{
    json_t *jn_str = kw_find_path(gobj, kw, path, FALSE);
    if(!jn_str) {
        if((flag & KW_CREATE) && kw) {
            json_t *jn_new;
            if(!default_value) {
                jn_new = json_null();
            } else {
                jn_new = json_string(default_value);
            }
            kw_set_dict_value(gobj, kw, path, jn_new);
            return default_value;
        }
        if(flag & KW_REQUIRED) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path NOT FOUND, default value returned",
                "path",         "%s", path,
                NULL
            );
            gobj_trace_json(gobj, kw, "path NOT FOUND, default value returned '%s'", path);
        }
        return default_value;
    }
    if(!json_is_string(jn_str)) {
        if(!json_is_null(jn_str)) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path MUST BE a json str",
                "path",         "%s", path,
                NULL
            );
            gobj_trace_json(gobj, kw, "path MUST BE a json str");
        }
        return default_value;
    }

    if(flag & KW_EXTRACT) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Cannot extract a string",
            "path",         "%s", path,
            NULL
        );
    }

    const char *str = json_string_value(jn_str);
    return str;
}

/***************************************************************************
 *  Get any value from an json object searched by path
 ***************************************************************************/
PUBLIC json_t *kw_get_dict_value(
    hgobj gobj,
    json_t *kw,
    const char *path,
    json_t *default_value,  // owned
    kw_flag_t flag)
{
    json_t *jn_value = kw_find_path(gobj, kw, path, FALSE);
    if(!jn_value) {
        if((flag & KW_CREATE) && default_value && kw) {
            kw_set_dict_value(gobj, kw, path, default_value);
            return default_value;
        }
        if(flag & KW_REQUIRED) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "path NOT FOUND, default value returned",
                "path",         "%s", path,
                NULL
                );
            gobj_trace_json(gobj, kw, "path NOT FOUND, default value returned '%s'", path);
        }
        return default_value;
    }
    JSON_DECREF(default_value);

    if(flag & KW_EXTRACT) {
        json_incref(jn_value);
        kw_delete(gobj, kw, path);
    }
    return jn_value;
}

/***************************************************************************
    Return a new kw only with the keys got by path.
    It's not a deep copy, new keys are the paths.
    Not valid with lists.
    If paths are empty return kw
 ***************************************************************************/
PUBLIC json_t *kw_clone_by_path(
    hgobj gobj,
    json_t *kw,         // owned
    const char **paths
)
{
    if(!paths || !*paths) {
        return kw;
    }
    json_t *kw_clone = json_object();
    while(*paths) {
        json_t *jn_value = kw_get_dict_value(gobj, kw, *paths, 0, 0);
        if(jn_value) {
            json_object_set(kw_clone, *paths, jn_value);
        }
        paths++;
    }
    KW_DECREF(kw);
    return kw_clone;
}

/***************************************************************************
    Return a new kw only with the keys got by dict's keys or list's keys (strings).
    Keys:
        "$key"
        ["$key1", "$key2", ...]
        {"$key1":*, "$key2":*, ...}

    It's not a deep copy, new keys are the paths.
    If paths are empty return kw
 ***************************************************************************/
PUBLIC json_t *kw_clone_by_keys(
    hgobj gobj,
    json_t *kw,     // owned
    json_t *keys,   // owned
    BOOL verbose
)
{
    json_t *kw_clone = json_object();
    if(json_is_string(keys) && !empty_string(json_string_value(keys))) {
        const char *key = json_string_value(keys);
        json_t *jn_value = kw_get_dict_value(gobj, kw, key, 0, 0);
        if(jn_value) {
            json_object_set(kw_clone, key, jn_value);
        } else {
            if(verbose) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "key not found",
                    "key",          "%s", key,
                    NULL
                );
            }
        }
    } else if(json_is_object(keys) && json_object_size(keys)>0) {
        const char *key;
        json_t *jn_v;
        json_object_foreach(keys, key, jn_v) {
            json_t *jn_value = kw_get_dict_value(gobj, kw, key, 0, 0);
            if(jn_value) {
                json_object_set(kw_clone, key, jn_value);
            } else {
                if(verbose) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "key not found",
                        "key",          "%s", key,
                        NULL
                    );
                }
            }
        }
    } else if(json_is_array(keys) && json_array_size(keys)>0) {
        size_t idx;
        json_t *jn_v;
        json_array_foreach(keys, idx, jn_v) {
            const char *key = json_string_value(jn_v);
            json_t *jn_value = kw_get_dict_value(gobj, kw, key, 0, 0);
            if(jn_value) {
                json_object_set(kw_clone, key, jn_value);
            } else {
                if(verbose) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "key not found",
                        "key",          "%s", key,
                        NULL
                    );
                }
            }
        }
    } else {
        json_decref(kw_clone);
        JSON_DECREF(keys);
        return kw;
    }

    JSON_DECREF(keys);
    JSON_DECREF(kw);
    return kw_clone;
}

/***************************************************************************
    Return a new kw except the keys got by dict's keys or list's keys (strings).
    Keys:
        "$key"
        ["$key1", "$key2", ...]
        {"$key1":*, "$key2":*, ...}

    It's not a deep copy, new keys are the paths.
    If paths are empty return empty
 ***************************************************************************/
PUBLIC json_t *kw_clone_by_not_keys(
    hgobj gobj,
    json_t *kw,     // owned
    json_t *keys,   // owned
    BOOL verbose
)
{
    json_t *kw_clone = json_object();
    json_object_update(kw_clone, kw);

    if(json_is_string(keys) && !empty_string(json_string_value(keys))) {
        const char *key = json_string_value(keys);
        json_t *jn_value = kw_get_dict_value(gobj, kw, key, 0, 0);
        if(jn_value) {
            json_object_del(kw_clone, key);
        } else {
            if(verbose) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "key not found",
                    "key",          "%s", key,
                    NULL
                );
            }
        }
    } else if(json_is_object(keys) && json_object_size(keys)>0) {
        const char *key;
        json_t *jn_v;
        json_object_foreach(keys, key, jn_v) {
            json_t *jn_value = kw_get_dict_value(gobj, kw, key, 0, 0);
            if(jn_value) {
                json_object_del(kw_clone, key);
            } else {
                if(verbose) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "key not found",
                        "key",          "%s", key,
                        NULL
                    );
                }
            }
        }
    } else if(json_is_array(keys) && json_array_size(keys)>0) {
        size_t idx;
        json_t *jn_v;
        json_array_foreach(keys, idx, jn_v) {
            const char *key = json_string_value(jn_v);
            json_t *jn_value = kw_get_dict_value(gobj, kw, key, 0, 0);
            if(jn_value) {
                json_object_del(kw_clone, key);
            } else {
                if(verbose) {
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "key not found",
                        "key",          "%s", key,
                        NULL
                    );
                }
            }
        }
    } else {
        json_decref(kw_clone);
        JSON_DECREF(keys);
        JSON_DECREF(kw);
        return json_object();
    }

    JSON_DECREF(keys);
    JSON_DECREF(kw);
    return kw_clone;
}

/***************************************************************************
    Remove from kw1 all keys in kw2
    kw2 can be a string, dict, or list.
 ***************************************************************************/
PUBLIC int kw_pop(
    json_t *kw1, // not owned
    json_t *kw2  // not owned
)
{
    if(json_is_object(kw2)) {
        const char *key;
        json_t *jn_value;
        json_object_foreach(kw2, key, jn_value) {
            json_object_del(kw1, key);
        }
    } else if(json_is_array(kw2)) {
        size_t idx;
        json_t *jn_value;
        json_array_foreach(kw2, idx, jn_value) {
            kw_pop(kw1, jn_value);
        }
    } else if(json_is_string(kw2)) {
        json_object_del(kw1, json_string_value(kw2));
    }
    return 0;
}

/***************************************************************************
    Match a json dict with a json filter (only compare str/number)
 ***************************************************************************/
PRIVATE BOOL _kw_match_simple(
    json_t *kw,         // not owned
    json_t *jn_filter,  // owned
    int level
)
{
    BOOL matched = FALSE;

    level++;

    if(json_is_array(jn_filter)) {
        // Empty array evaluate as false, until a match condition occurs.
        matched = FALSE;
        size_t idx;
        json_t *jn_filter_value;
        json_array_foreach(jn_filter, idx, jn_filter_value) {
            json_incref(jn_filter_value);
            matched = _kw_match_simple(
                kw,                 // not owned
                jn_filter_value,    // owned
                level
            );
            if(matched) {
                break;
            }
        }

    } else if(json_is_object(jn_filter)) {
        if(json_object_size(jn_filter)==0) {
            // Empty object evaluate as false.
            matched = FALSE;
        } else {
            // Not Empty object evaluate as true, until a NOT match condition occurs.
            matched = TRUE;
        }

        const char *filter_path;
        json_t *jn_filter_value;
        json_object_foreach(jn_filter, filter_path, jn_filter_value) {
            /*
             *  Variable compleja, recursivo
             */
            if(json_is_array(jn_filter_value) || json_is_object(jn_filter_value)) {
                json_incref(jn_filter_value);
                matched = _kw_match_simple(
                    kw,                 // not owned
                    jn_filter_value,    // owned
                    level
                );
                break;
            }

            /*
             *  Variable sencilla
             */
            /*
             * TODO get the name and op.
             */
            const char *path = filter_path; // TODO
            const char *op = "__equal__";

            /*
             *  Get the record value, firstly by path else by name
             */
            json_t *jn_record_value;
            // Firstly try the key as pointers
            jn_record_value = kw_get_dict_value(0, kw, path, 0, 0);
            if(!jn_record_value) {
                // Secondly try the key with points (.) as full key
                jn_record_value = json_object_get(kw, path);
            }
            if(!jn_record_value) {
                matched = FALSE;
                break;
            }

            /*
             *  Do simple operation
             */
            if(strcasecmp(op, "__equal__")==0) { // TODO __equal__ by default
                int cmp = cmp_two_simple_json(jn_record_value, jn_filter_value);
                if(cmp!=0) {
                    matched = FALSE;
                    break;
                }
            } else {
                // TODO op: __lower__ __higher__ __re__ __equal__
            }
        }
    }

    json_decref(jn_filter);
    return matched;
}

/***************************************************************************
    Match a json dict with a json filter (only compare str/number)
 ***************************************************************************/
PUBLIC BOOL kw_match_simple(
    json_t *kw,         // not owned
    json_t *jn_filter   // owned
)
{
    if(!jn_filter) {
        // Si no hay filtro pasan todos.
        return TRUE;
    }
    if(json_is_object(jn_filter) && json_object_size(jn_filter)==0) {
        // A empty object at first level evaluate as true.
        json_decref(jn_filter);
        return TRUE;
    }

    return _kw_match_simple(kw, jn_filter, 0);
}

/***************************************************************************
    HACK Convention: private data begins with "_".
    Delete private keys
 ***************************************************************************/
PUBLIC int kw_delete_private_keys(
    json_t *kw  // NOT owned
)
{
    int underscores = 1;

    const char *key;
    json_t *value;
    void *n;
    json_object_foreach_safe(kw, n, key, value) {
        if(underscores) {
            int u;
            for(u=0; u<(int)strlen(key); u++) {
                if(key[u] != '_') {
                    break;
                }
            }
            if(u == underscores) {
                json_object_del(kw, key);
            }
        }
    }

    return 0;
}

/***************************************************************************
    HACK Convention: metadata begins with "__".
    Delete metadata keys
 ***************************************************************************/
PUBLIC int kw_delete_metadata_keys(
    json_t *kw  // NOT owned
)
{
    int underscores = 2;

    const char *key;
    json_t *value;
    void *n;
    json_object_foreach_safe(kw, n, key, value) {
        if(underscores) {
            int u;
            for(u=0; u<(int)strlen(key); u++) {
                if(key[u] != '_') {
                    break;
                }
            }
            if(u == underscores) {
                json_object_del(kw, key);
            }
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _walk_object(
    hgobj gobj,
    json_t *kw,
    int (*callback)(hgobj gobj, json_t *kw, const char *key, json_t *value)
)
{
    const char *key;
    json_t *value;
    json_object_foreach(kw, key, value) {
        json_type type = json_typeof(value);
        switch(type) {
        case JSON_OBJECT:
            if(_walk_object(gobj, value, callback)<0) {
                return -1;
            }
            break;
        case JSON_ARRAY:
        default:
            if(callback(gobj, kw, key, value)<0) {
                return -1;
            }
            break;
        }
    }

    return 0;
}

/***************************************************************************
 *  Walk over an object
 ***************************************************************************/
PUBLIC int kw_walk(
    hgobj gobj,
    json_t *kw, // not owned
    int (*callback)(hgobj gobj, json_t *kw, const char *key, json_t *value)
)
{
    if(json_is_object(kw)) {
        return _walk_object(gobj, kw, callback);
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "json to duplicate must be an object or array",
            NULL
        );
        return -1;
    }
}




                    /*---------------------------------*
                     *          KWID
                     *---------------------------------*/




/***************************************************************************
    Utility for databases.
    Get a json list or dict, get the **first** record that match `id`
    Convention:
        - If it's a list of dict: the records have "id" field as primary key
        - If it's a dict, the key is the `id`
 ***************************************************************************/
PUBLIC json_t *kwid_get(
    hgobj gobj,
    json_t *kw,  // NOT owned
    const char *id,
    json_t *default_value,
    kw_flag_t flag,
    size_t *idx_     // If not null set the idx in case of array
)
{
    json_t *v = NULL;
    BOOL backward = flag & KW_BACKWARD;

    if(idx_) { // error case
        *idx_ = 0;
    }

    if(empty_string(id)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "id NULL",
            NULL
        );
        return NULL;
    }

    switch(json_typeof(kw)) {
    case JSON_OBJECT:
        v = json_object_get(kw, id);
        if(!v) {
            if(flag & KW_REQUIRED) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "record NOT FOUND, default value returned",
                    "id",           "%s", id,
                    NULL
                );
                gobj_trace_json(gobj, kw, "record NOT FOUND, default value returned, id='%s'", id);
            }
            return default_value;
        }
        return v;

    case JSON_ARRAY:
        {
            if(!backward) {
                size_t idx;
                json_array_foreach(kw, idx, v) {
                    const char *id_ = json_string_value(json_object_get(v, "id"));
                    if(id_ && strcmp(id_, id)==0) {
                        if(idx_) {
                            *idx_ = idx;
                        }
                        return v;
                    }
                }
            } else {
                int idx;
                json_array_backward(kw, idx, v) {
                    const char *id_ = json_string_value(json_object_get(v, "id"));
                    if(id_ && strcmp(id_, id)==0) {
                        if(idx_) {
                            *idx_ = idx;
                        }
                        return v;
                    }
                }
            }

            if(flag & KW_REQUIRED) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "record NOT FOUND, default value returned",
                    "id",           "%s", id,
                    NULL
                );
                gobj_trace_json(gobj, kw, "record NOT FOUND, default value returned, id='%s'", id);
            }
            return default_value;
        }
        break;
    default:
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw must be dict or list",
            "id",           "%s", id,
            NULL
        );
        break;
    }

    return NULL;
}
