/****************************************************************************
 *              kwid.c
 *
 *              kw helpers
 *
 *              Copyright (c) 2014,2023 Niyamaka.
 *              Copyright (c) 2024, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

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

PRIVATE json_t * _duplicate_object(json_t *kw, const char **keys, int underscores, BOOL serialize);

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
        /*
         *  Pop the binary field from kw
         */
        void *binary = (void *)(uintptr_t)json_object_get(kw, pf->binary_field_name);
        if(binary) {
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
    /*
     *  Check that no binary field exits
     */
    serialize_fields_t *pf = serialize_fields;
    while(pf->binary_field_name) {
        /*
         *  Don't ask if kw_has_key, rush!
         */
        json_object_del(kw, pf->binary_field_name);
        pf++;
    }

    /*
     *  Deserialize
     */
    pf = serialize_fields;
    while(pf->serialized_field_name) {
        /*
         *  Don't ask if kw_has_key, rush!
         */
        /*
         *  Pop the serialized json field from kw
         */
        json_t *jn_serialized = json_object_get(kw, pf->serialized_field_name);

        /*
         *  Deserialize
         */
        if(jn_serialized) {
            if(pf->deserialize_fn) {
                void *binary = pf->deserialize_fn(gobj, jn_serialized);

                /*
                 *  Save the binary field to kw
                 */
                json_object_set_new(
                    kw,
                    pf->binary_field_name,
                    json_integer((json_int_t)(uintptr_t)binary)
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

    serialize_fields_t *pf = serialize_fields;
    while(pf->binary_field_name) {
        /*
         *  Get the binary field from kw
         */
        void *binary = (void *)(uintptr_t)json_object_get(kw, pf->binary_field_name);
        if(binary) {
            /*
            *  Incref binary
            */
            if(pf->incref_fn) {
                pf->incref_fn(binary);
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

    serialize_fields_t *pf = serialize_fields;
    while(pf->binary_field_name) {
        /*
         *  Get the binary field from kw
         */
        void *binary = (void *)(uintptr_t)json_object_get(kw, pf->binary_field_name);
        if(binary) {
            /*
            *  Incref binary
            */
            if(pf->decref_fn) {
                pf->decref_fn(binary);
            }
        }

        pf++;
    }
    JSON_DECREF(kw)
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
 *  TODO WARNING function too slow, don't use in quick code
 ***************************************************************************/
PUBLIC json_t *kw_find_path_(hgobj gobj, json_t *kw, const char *path, BOOL verbose)
{
    if(!(json_is_object(kw) || json_is_array(kw))) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw must be list or dict",
            "path",         "%s", path?path:"",
            NULL
        );
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
 *  Search delimiter
 ***************************************************************************/
PRIVATE char *search_delimiter(const char *s, char delimiter_)
{
    if(!delimiter_) {
        return 0;
    }
    return strchr(s, delimiter_);
}

/***************************************************************************
    Return the json's value find by path, walking over lists and dicts
 ***************************************************************************/
PUBLIC json_t *kw_find_path(hgobj gobj, json_t *kw, const char *path, BOOL verbose)
{
    if(!(json_is_object(kw) || json_is_array(kw))) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw must be list or dict",
            "path",         "%s", path?path:"",
            NULL
        );
        return 0;
    }
    if(!path) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "path NULL",
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

    char *p = search_delimiter(path, delimiter[0]);
    if(!p) {
        if(json_is_object(kw)) {
            // Dict
            json_t *value = json_object_get(kw, path);
            if(!value && verbose) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "path not found",
                    "path",         "%s", path,
                    NULL
                );
            }
            return value;

        } else {
            // Array
            int idx = atoi(path);
            json_t *value = json_array_get(kw, idx);
            if(!value && verbose) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "path not found",
                    "path",         "%s", path,
                    "idx",          "%d", idx,
                    NULL
                );
            }
            return value;
        }
    }

    char segment[256];
    if(snprintf(segment, sizeof(segment), "%.*s", (int)(size_t)(p-path), path)>=sizeof(segment)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "buffer too small",
            "path",         "%s", path,
            NULL
        );
    }

    json_t *next_json = 0;
    if(json_is_object(kw)) {
        // Dict
        next_json = json_object_get(kw, segment);
        if(!next_json || json_is_null(next_json)) {
            if(verbose) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "Dict segment not found",
                    "path",         "%s", path,
                    "segment",      "%s", segment,
                    NULL
                );
            }
            return 0;
        }
    } else {
        // Array
        int idx = atoi(segment);
        next_json = json_array_get(kw, idx);
        if(!next_json || json_is_null(next_json)) {
            if(verbose) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "List segment not found",
                    "path",         "%s", path,
                    "segment",      "%s", segment,
                    "idx",          "%d", idx,
                    NULL
                );
            }
            return 0;
        }
    }

    return kw_find_path(gobj, next_json, p+1, verbose);
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
        JSON_DECREF(value)
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
                if(i < list_size-1) {
                    next = json_object();
                    json_object_set_new(v, segment, next);
                } else {
                    json_object_set(v, segment, value);
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
    char *s = gbmem_strdup(path);
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
    Utility for databases.
    Being field `kw` a list of id record [{id...},...] return the record idx with `id`
    Return -1 if not found
 ***************************************************************************/
int kwid_find_record_in_list(
    hgobj gobj,
    json_t *kw_list,
    const char *id,
    kw_flag_t flag
)
{
    if(!id || !json_is_array(kw_list)) {
        if(flag & KW_VERBOSE) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "id NULL or kw_list is not a list",
                NULL
            );
        }
        return -1;
    }

    int idx; json_t *record;
    json_array_foreach(kw_list, idx, record) {
        const char *id_ = kw_get_str(gobj, record, "id", 0, 0);
        if(!id_) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "list item is not a id record",
                NULL
            );
            return -1;
        }
        if(strcmp(id, id_)==0) {
            return idx;
        }
    }

    if(flag & KW_VERBOSE) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "record not found in this list",
            "id",           "%s", id,
            NULL
        );
    }
    return 0;
}

/***************************************************************************
    Get a the idx of simple json item in a json list.
    Return -1 if not found
 ***************************************************************************/
PUBLIC int kw_find_json_in_list(
    hgobj gobj,
    json_t *kw_list,  // not owned
    json_t *item,  // not owned
    kw_flag_t flag

)
{
    if(!item || !json_is_array(kw_list)) {
        if(flag & KW_VERBOSE) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "item NULL or kw_list is not a list",
                NULL
            );
        }
        return -1;
    }

    int idx;
    json_t *jn_item;

    json_array_foreach(kw_list, idx, jn_item) {
        if(json_is_identical(item, jn_item)) {
            return idx;
        }
    }

    if(flag & KW_VERBOSE) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "item not found in this list",
            "item",         "%j", item,
            NULL
        );
    }
    return -1;
}

/***************************************************************************
    Being `kw` a row's list or list of dicts [{},...],
    return a new list of **duplicated** kw filtering the rows by `jn_filter` (where),
    If match_fn is 0 then kw_match_simple is used.
 ***************************************************************************/
PUBLIC json_t *kw_select( // WARNING return **duplicated** objects
    hgobj gobj,
    json_t *kw,         // not owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
)
{
    if(!match_fn) {
        match_fn = kw_match_simple;
    }
    json_t *kw_new = json_array();

    if(json_is_array(kw)) {
        size_t idx;
        json_t *jn_value;
        json_array_foreach(kw, idx, jn_value) {
            KW_INCREF(jn_filter);
            if(match_fn(jn_value, jn_filter)) {
                json_t *jn_row = kw_duplicate(gobj, jn_value);
                json_array_append_new(kw_new, jn_row);
            }
        }
    } else if(json_is_object(kw)) {
        KW_INCREF(jn_filter);
        if(match_fn(kw, jn_filter)) {
            json_t *jn_row = kw_duplicate(gobj, kw);
            json_array_append_new(kw_new, jn_row);
        }

    } else  {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw MUST BE a json array or object",
            NULL
        );
        return kw_new;
    }

    KW_DECREF(jn_filter);
    return kw_new;
}

/***************************************************************************
    Being `kw` a row's list or list of dicts [{},...],
    return a new list of incref (clone) kw filtering the rows by `jn_filter` (where),
    If match_fn is 0 then kw_match_simple is used.
 ***************************************************************************/
PUBLIC json_t *kw_collect( // WARNING return **duplicated** objects
    hgobj gobj,
    json_t *kw,         // not owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
)
{
    if(!match_fn) {
        match_fn = kw_match_simple;
    }
    json_t *kw_new = json_array();

    if(json_is_array(kw)) {
        size_t idx;
        json_t *jn_value;
        json_array_foreach(kw, idx, jn_value) {
            KW_INCREF(jn_filter);
            if(match_fn(jn_value, jn_filter)) {
                json_t *jn_row = json_incref(jn_value);
                json_array_append_new(kw_new, jn_row);
            }
        }
    } else if(json_is_object(kw)) {
        KW_INCREF(jn_filter);
        if(match_fn(kw, jn_filter)) {
            json_t *jn_row = json_incref(kw);
            json_array_append_new(kw_new, jn_row);
        }

    } else  {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw MUST BE a json array or object",
            NULL
        );
        return kw_new;
    }

    KW_DECREF(jn_filter);
    return kw_new;
}

/***************************************************************************
    Compare deeply two json **records**. Can be disordered.
 ***************************************************************************/
PUBLIC BOOL kwid_compare_records(
    hgobj gobj,
    json_t *record_, // NOT owned
    json_t *expected_, // NOT owned
    BOOL without_metadata,
    BOOL without_private,
    BOOL verbose
)
{
    BOOL ret = TRUE;
    json_t *record = json_deep_copy(record_);
    json_t *expected = json_deep_copy(expected_);
    if(!record) {
        if(verbose) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "record NULL",
                NULL
            );
        }
        JSON_DECREF(record);
        JSON_DECREF(expected);
        return FALSE;
    }
    if(!expected) {
        if(verbose) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "expected NULL",
                NULL
            );
        }
        JSON_DECREF(record);
        JSON_DECREF(expected);
        return FALSE;
    }

    if(json_typeof(record) != json_typeof(expected)) { // json_typeof CONTROLADO
        ret = FALSE;
        if(verbose) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "different json type",
                "record",       "%j", record,
                "expected",     "%j", expected,
                NULL
            );
        }
    } else {
        switch(json_typeof(record)) {
            case JSON_ARRAY:
                {
                    if(!kwid_compare_lists(
                            gobj,
                            record,
                            expected,
                            without_metadata,
                            without_private,
                            verbose)) {
                        ret = FALSE;
                        if(verbose) {
                            gobj_log_error(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                "msg",          "%s", "list not match",
                                "record",       "%j", record,
                                "expected",     "%j", expected,
                                NULL
                            );
                        }
                    }
                }
                break;

            case JSON_OBJECT:
                {
                    if(without_metadata) {
                        kw_delete_metadata_keys(record);
                        kw_delete_metadata_keys(expected);
                    }
                    if(without_private) {
                        kw_delete_private_keys(record);
                        kw_delete_private_keys(expected);
                    }

                    void *n; const char *key; json_t *value;
                    json_object_foreach_safe(record, n, key, value) {
                        if(!kw_has_key(expected, key)) {
                            ret = FALSE;
                            if(verbose) {
                                gobj_log_error(gobj, 0,
                                    "function",     "%s", __FUNCTION__,
                                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                    "msg",          "%s", "key not found",
                                    "key",          "%s", key,
                                    "expected",     "%j", expected,
                                    NULL
                                );
                            }
                            break;
                        }
                        json_t *value2 = json_object_get(expected, key);
                        if(json_typeof(value)==JSON_OBJECT) {
                            if(!kwid_compare_records(
                                    gobj,
                                    value,
                                    value2,
                                    without_metadata,
                                    without_private,
                                    verbose
                                )) {
                                ret = FALSE;
                                if(verbose) {
                                    gobj_log_error(gobj, 0,
                                        "function",     "%s", __FUNCTION__,
                                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                        "msg",          "%s", "record not match",
                                        "record",       "%j", record,
                                        "expected",     "%j", expected,
                                        NULL
                                    );
                                }
                            }
                            if(ret == FALSE) {
                                break;
                            }

                            json_object_del(record, key);
                            json_object_del(expected, key);

                        } else if(json_typeof(value)==JSON_ARRAY) {
                            if(!kwid_compare_lists(
                                    gobj,
                                    value,
                                    value2,
                                    without_metadata,
                                    without_private,
                                    verbose
                                )) {
                                ret = FALSE;
                                if(verbose) {
                                    gobj_log_error(gobj, 0,
                                        "function",     "%s", __FUNCTION__,
                                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                        "msg",          "%s", "list not match",
                                        "record",       "%j", record,
                                        "expected",     "%j", expected,
                                        NULL
                                    );
                                }
                            }
                            if(ret == FALSE) {
                                break;
                            }

                            json_object_del(record, key);
                            json_object_del(expected, key);

                        } else {
                            if(cmp_two_simple_json(value, value2)!=0) {
                                ret = FALSE;
                                if(verbose) {
                                    gobj_log_error(gobj, 0,
                                        "function",     "%s", __FUNCTION__,
                                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                        "msg",          "%s", "items not match",
                                        "value",        "%j", value,
                                        "value2",       "%j", value2,
                                        NULL
                                    );
                                }
                                break;
                            } else {
                                json_object_del(record, key);
                                json_object_del(expected, key);
                            }
                        }
                    }

                    if(ret == TRUE) {
                        if(json_object_size(record)>0) {
                            ret = FALSE;
                            if(verbose) {
                                gobj_log_error(gobj, 0,
                                    "function",     "%s", __FUNCTION__,
                                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                    "msg",          "%s", "remain record items",
                                    "record",       "%j", record,
                                    NULL
                                );
                            }
                        }
                        if(json_object_size(expected)>0) {
                            ret = FALSE;
                            if(verbose) {
                                gobj_log_error(gobj, 0,
                                    "function",     "%s", __FUNCTION__,
                                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                    "msg",          "%s", "remain expected items",
                                    "expected",     "%j", expected,
                                    NULL
                                );
                            }
                        }
                    }
                }
                break;
            default:
                ret = FALSE;
                if(verbose) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "No list or not object",
                        "record",       "%j", record,
                        NULL
                    );
                }
                break;
        }
    }

    JSON_DECREF(record);
    JSON_DECREF(expected);
    return ret;
}

/***************************************************************************
    Compare deeply two json lists of **records**. Can be disordered.
 ***************************************************************************/
PUBLIC BOOL kwid_compare_lists(
    hgobj gobj,
    json_t *list_, // NOT owned
    json_t *expected_, // NOT owned
    BOOL without_metadata,
    BOOL without_private,
    BOOL verbose
)
{
    BOOL ret = TRUE;
    json_t *list = json_deep_copy(list_);
    json_t *expected = json_deep_copy(expected_);
    if(!list) {
        JSON_DECREF(list);
        JSON_DECREF(expected);
        if(verbose) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "record NULL",
                NULL
            );
        }
        return FALSE;
    }
    if(!expected) {
        JSON_DECREF(list);
        JSON_DECREF(expected);
        if(verbose) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "expected NULL",
                NULL
            );
        }
        return FALSE;
    }

    if(json_typeof(list) != json_typeof(expected)) { // json_typeof CONTROLADO
        ret = FALSE;
        if(verbose) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "different json type",
                "list",         "%j", list,
                "expected",     "%j", expected,
                NULL
            );
        }
    } else {
        switch(json_typeof(list)) {
        case JSON_ARRAY:
            {
                int idx1; json_t *r1;
                json_array_foreach(list, idx1, r1) {
                    const char *id1 = kw_get_str(gobj, r1, "id", 0, 0);
                    /*--------------------------------*
                     *  List with id records
                     *--------------------------------*/
                    if(id1) {
                        size_t idx2 = kwid_find_record_in_list(gobj, expected, id1, 0);
                        if(idx2 < 0) {
                            ret = FALSE;
                            if(verbose) {
                                gobj_log_error(gobj, 0,
                                    "function",     "%s", __FUNCTION__,
                                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                    "msg",          "%s", "record not found in expected list",
                                    "record",       "%j", r1,
                                    "expected",     "%j", expected,
                                    NULL
                                );
                            }
                            continue;
                        }
                        json_t *r2 = json_array_get(expected, idx2);

                        if(!kwid_compare_records(
                            gobj,
                            r1,
                            r2,
                            without_metadata,
                            without_private,
                            verbose)
                        ) {
                            ret = FALSE;
                            if(verbose) {
                                gobj_log_error(gobj, 0,
                                    "function",     "%s", __FUNCTION__,
                                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                    "msg",          "%s", "record not match",
                                    "r1",           "%j", r1,
                                    "r2",           "%j", r2,
                                    NULL
                                );
                            }
                        }
                        if(ret == FALSE) {
                            break;
                        }

                        if(json_array_remove(list, idx1)==0) {
                            idx1--;
                        }
                        json_array_remove(expected, idx2);
                    } else {
                        /*--------------------------------*
                         *  List with any json items
                         *--------------------------------*/
                        int idx2 = kw_find_json_in_list(gobj, expected, r1, 0);
                        if(idx2 < 0) {
                            ret = FALSE;
                            if(verbose) {
                                gobj_log_error(gobj, 0,
                                    "function",     "%s", __FUNCTION__,
                                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                    "msg",          "%s", "record not found in expected list",
                                    "record",       "%j", r1,
                                    "expected",     "%j", expected,
                                    NULL
                                );
                            }
                            break;
                        }
                        if(json_array_remove(list, idx1)==0) {
                            idx1--;
                        }
                        json_array_remove(expected, idx2);
                    }
                }

                if(ret == TRUE) {
                    if(json_array_size(list)>0) {
                        if(verbose) {
                            gobj_log_error(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                "msg",          "%s", "remain list items",
                                "list",         "%j", list,
                                NULL
                            );
                        }
                        ret = FALSE;
                    }
                    if(json_array_size(expected)>0) {
                        if(verbose) {
                            gobj_log_error(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                                "msg",          "%s", "remain expected items",
                                "expected",     "%j", expected,
                                NULL
                            );
                        }
                        ret = FALSE;
                    }
                }
            }
            break;

        case JSON_OBJECT:
            {
                if(!kwid_compare_records(
                    gobj,
                    list,
                    expected,
                    without_metadata,
                    without_private,
                    verbose)
                ) {
                    ret = FALSE;
                    if(verbose) {
                        gobj_trace_msg(gobj, "ERROR: object not match");
                    }
                }
            }
            break;
        default:
            {
                ret = FALSE;
                if(verbose) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "No list or not object",
                        "list",         "%j", list,
                        NULL
                    );
                }
            }
            break;
        }
    }

    JSON_DECREF(list);
    JSON_DECREF(expected);
    return ret;
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
 *
 ***************************************************************************/
PUBLIC json_t *kw_get_list_value(
    hgobj gobj,
    json_t *kw,
    int idx,
    kw_flag_t flag)
{
    if(!json_is_array(kw)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw MUST BE a json array",
            NULL
        );
        return 0;
    }
    if(idx >= json_array_size(kw)) {
        if(flag & KW_REQUIRED) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "list idx NOT FOUND",
                "idx",          "%d", (int)idx,
                "array_size",   "%d", (int)json_array_size(kw),
                NULL
            );
        }
        return 0;
    }

    json_t *v = json_array_get(kw, idx);
    if(v && (flag & KW_EXTRACT)) {
        JSON_INCREF(v);
        json_array_remove(kw, idx);
    }
    return v;
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
        if(strcasecmp(v, "TRUE")==0) {
            value = 1;
        } else if(strcasecmp(v, "FALSE")==0) {
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
    JSON_DECREF(default_value)

    if(flag & KW_EXTRACT) {
        json_incref(jn_value);
        kw_delete(gobj, kw, path);
    }
    return jn_value;
}

/***************************************************************************
 *  Get any value from an json object searched by path and subdict
 ***************************************************************************/
PUBLIC json_t *kw_get_subdict_value(
    hgobj gobj,
    json_t *kw,
    const char *path,
    const char *key,
    json_t *default_value,  // owned
    kw_flag_t flag)
{
    json_t *jn_subdict = kw_find_path(gobj, kw, path, FALSE);
    if(!jn_subdict) {
        if((flag & KW_CREATE) && kw) {
            jn_subdict = json_object();
            kw_set_dict_value(gobj, kw, path, jn_subdict);
        } else {
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
            JSON_DECREF(default_value)
            return NULL;
        }
    }

    if(empty_string(key)) {
        if(flag & KW_REQUIRED) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "key NULL",
                "path",         "%s", path,
                NULL
            );
        }
        JSON_DECREF(default_value)
        return NULL;
    }
    return kw_get_dict_value(gobj, jn_subdict, key, default_value, flag);
}

/***************************************************************************
    Update kw with other, except keys in except_keys
    First level only
 ***************************************************************************/
PUBLIC void kw_update_except(
    hgobj gobj,
    json_t *kw,  // not owned
    json_t *other,  // owned
    const char **except_keys
)
{
    const char *key;
    json_t *jn_value;
    json_object_foreach(other, key, jn_value) {
        if(str_in_list(except_keys, key, FALSE)) {
            continue;
        }
        json_object_set(kw, key, jn_value);
    }
}

/***************************************************************************
 *  if binary is inside of kw, incref binary
 ***************************************************************************/
PRIVATE serialize_fields_t * get_serialize_field(const char *binary_field_name)
{
    serialize_fields_t * pf = serialize_fields;
    while(pf->binary_field_name) {
        if(strcmp(pf->binary_field_name, binary_field_name)==0) {
            return pf;
        }
        pf++;
    }
    return 0;
}

/***************************************************************************
 *  Make a twin of a array
 ***************************************************************************/
PRIVATE json_t * _duplicate_array(json_t *kw, const char **keys, int underscores, BOOL serialize)
{
    json_t *kw_dup_ = json_array();

    size_t idx;
    json_t *value;
    json_array_foreach(kw, idx, value) {
        json_t *new_value = 0;
        json_type type = json_typeof(value);
        switch(type) {
        case JSON_OBJECT:
            new_value = _duplicate_object(value, keys, underscores, serialize);
            break;
        case JSON_ARRAY:
            new_value = _duplicate_array(value, keys, underscores, serialize);
            break;
        case JSON_STRING:
            new_value = json_string(json_string_value(value));
            break;
        case JSON_INTEGER:
            {
                /*
                 *  WARNING binary fields in array cannot be managed!
                 */
                json_int_t binary = json_integer_value(value);
                new_value = json_integer(binary);
            }
            break;
        case JSON_REAL:
            new_value = json_real(json_real_value(value));
            break;
        case JSON_TRUE:
            new_value = json_true();
            break;
        case JSON_FALSE:
            new_value = json_false();
            break;
        case JSON_NULL:
            new_value = json_null();
            break;
        }
        json_array_append_new(kw_dup_, new_value);
    }

    return kw_dup_;
}
/***************************************************************************
 *  Make a twin of a object
 ***************************************************************************/
PRIVATE json_t * _duplicate_object(json_t *kw, const char **keys, int underscores, BOOL serialize)
{
    json_t *kw_dup_ = json_object();

    const char *key;
    json_t *value;
    json_object_foreach(kw, key, value) {
        if(underscores) {
            int u;
            for(u=0; u<strlen(key); u++) {
                if(key[u] != '_') {
                    break;
                }
            }
            if(u == underscores) {
                continue;
            }
        }
        if(keys && !str_in_list(keys, key, FALSE)) {
            continue;
        }
        json_t *new_value = 0;
        json_type type = json_typeof(value);
        switch(type) {
        case JSON_OBJECT:
            new_value = _duplicate_object(value, keys, underscores, serialize);
            break;
        case JSON_ARRAY:
            new_value = _duplicate_array(value, keys, underscores, serialize);
            break;
        case JSON_STRING:
            new_value = json_string(json_string_value(value));
            break;
        case JSON_INTEGER:
            {
                json_int_t binary = json_integer_value(value);
                new_value = json_integer(binary);
                if(serialize) {
                    serialize_fields_t * pf = get_serialize_field(key);
                    if(pf) {
                        pf->incref_fn((void *)(uintptr_t)binary);
                    }
                }
            }
            break;
        case JSON_REAL:
            new_value = json_real(json_real_value(value));
            break;
        case JSON_TRUE:
            new_value = json_true();
            break;
        case JSON_FALSE:
            new_value = json_false();
            break;
        case JSON_NULL:
            new_value = json_null();
            break;
        }
        json_object_set_new(kw_dup_, key, new_value);
    }

    return kw_dup_;
}

/***************************************************************************
 *  Make a duplicate of kw
 *  WARNING clone of json_deep_copy(), but processing serialized fields
 ***************************************************************************/
PUBLIC json_t *kw_duplicate(
    hgobj gobj,
    json_t *kw  // NOT owned
)
{
    if(json_is_object(kw)) {
        return _duplicate_object(kw, 0, 0, TRUE);
    } else if(json_is_array(kw)) {
        return _duplicate_array(kw, 0, 0, TRUE);
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "json to duplicate must be an object or array",
            NULL
        );
        return 0;
    }
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
        JSON_DECREF(keys)
        return kw;
    }

    JSON_DECREF(keys)
    JSON_DECREF(kw)
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
        JSON_DECREF(keys)
        JSON_DECREF(kw)
        return json_object();
    }

    JSON_DECREF(keys)
    JSON_DECREF(kw)
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
    char *prefix_path
)
{
    char path[NAME_MAX];
    BOOL matched = FALSE;

    if(json_is_array(jn_filter)) {
        // Empty array evaluate as FALSE, until a match condition occurs.
        matched = FALSE;
        size_t idx;
        json_t *jn_filter_value;
        json_array_foreach(jn_filter, idx, jn_filter_value) {
            /*
             *  Complex variable, recursive
             */
            if(empty_string(prefix_path)) {
                snprintf(path, sizeof(path), "%s", prefix_path);
            } else {
                snprintf(path, sizeof(path), "%s`%d", prefix_path, (int)idx);
            }

            matched = _kw_match_simple(
                kw,                 // not owned
                json_incref(jn_filter_value),    // owned
                path
            );
            if(matched) {
                break;
            }
        }

    } else if(json_is_object(jn_filter)) {
        // Object evaluate as TRUE, until a NOT match condition occurs.
        matched = TRUE;
        const char *key;
        json_t *jn_filter_value;
        json_object_foreach(jn_filter, key, jn_filter_value) {
            if(empty_string(prefix_path)) {
                snprintf(path, sizeof(path), "%s", key);
            } else {
                snprintf(path, sizeof(path), "%s`%s", prefix_path, key);
            }

            if(json_is_array(jn_filter_value) || json_is_object(jn_filter_value)) {
                /*
                 *  Complex variable, recursive
                 */
                matched = _kw_match_simple(
                    kw, // not owned
                    json_incref(jn_filter_value),  // owned
                    path
                );
                if(!matched) {
                    break;
                }
                continue;
            }

            /*
             *  Get the record value, firstly by path else by name
             */
            // Firstly try the key as pointers
            json_t *jn_record_value = kw_get_dict_value(0, kw, path, 0, 0);
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
            int cmp = cmp_two_simple_json(jn_record_value, jn_filter_value);
            if(cmp!=0) {
                matched = FALSE;
                break;
            }
        }
    }

    JSON_DECREF(jn_filter);
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
    if(json_size(jn_filter)==0) {
        // No filter, or an empty object or empty array evaluates as TRUE.
        json_decref(jn_filter);
        return TRUE;
    }

    return _kw_match_simple(kw, jn_filter, "");
}

/***************************************************************************
    HACK Convention: private data begins with "_".
    Delete private keys
 ***************************************************************************/
PUBLIC int kw_delete_private_keys(
    json_t *kw  // NOT owned
)
{
    const int underscores = 1;

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
    Return a new json with all arrays or dicts greater than `limit`
        with [{"__collapsed__": {"path": path, "size": size}}]
        or   {{"__collapsed__": {"path": path, "size": size}}}
 ***************************************************************************/
PRIVATE json_t *collapse(
    hgobj gobj,
    json_t *kw,         // not owned
    char *path,
    int collapse_lists_limit,
    int collapse_dicts_limit
)
{
    json_t *new_kw = json_object();
    const char *key; json_t *jn_value;
    json_object_foreach(kw, key, jn_value) {
        char *new_path = gbmem_strndup(path, strlen(path)+strlen(key)+2);
        if(strlen(new_path)>0) {
            strcat(new_path, delimiter);
        }
        strcat(new_path, key);

        if(json_is_object(jn_value)) {
            if(collapse_dicts_limit>0 && json_object_size(jn_value)>collapse_dicts_limit) {
                json_object_set_new(
                    new_kw,
                    key,
                    json_pack("{s:{s:s, s:I}}",
                        "__collapsed__",
                            "path", new_path,
                            "size", (json_int_t)json_object_size(jn_value)
                    )
                );
            } else {
                json_object_set_new(
                    new_kw,
                    key,
                    collapse(gobj, jn_value, new_path, collapse_lists_limit, collapse_dicts_limit)
                );
            }

        } else if(json_is_array(jn_value)) {
            if(collapse_lists_limit > 0 && json_array_size(jn_value)>collapse_lists_limit) {
                json_object_set_new(
                    new_kw,
                    key,
                    json_pack("[{s:{s:s, s:I}}]",
                        "__collapsed__",
                            "path", new_path,
                            "size", (json_int_t)json_array_size(jn_value)
                    )
                );
            } else {
                json_t *new_list = json_array();
                json_object_set_new(new_kw, key, new_list);
                int idx; json_t *v;
                json_array_foreach(jn_value, idx, v) {
                    char s_idx[40];
                    snprintf(s_idx, sizeof(s_idx), "%d", idx);
                    char *new_path2 = gbmem_strndup(new_path, strlen(new_path)+strlen(s_idx)+2);
                    if(strlen(new_path2)>0) {
                        strcat(new_path2, delimiter);
                    }
                    strcat(new_path2, s_idx);

                    if(json_is_object(v)) {
                        json_array_append_new(
                            new_list,
                            collapse(gobj, v, new_path2, collapse_lists_limit, collapse_dicts_limit)
                        );
                    } else if(json_is_array(v)) {
                        json_array_append(new_list, v); // ???
                    } else {
                        json_array_append(new_list, v);
                    }
                    GBMEM_FREE(new_path2);
                }
            }

        } else {
            json_object_set(
                new_kw,
                key,
                jn_value
            );
        }
        GBMEM_FREE(new_path);
    }

    return new_kw;
}
PUBLIC json_t *kw_collapse(
    hgobj gobj,
    json_t *kw,         // not owned
    int collapse_lists_limit,
    int collapse_dicts_limit
)
{
    if(!json_is_object(kw)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw_collapse() kw must be a dictionary",
            NULL
        );
        return 0;
    }
    char *path = GBMEM_MALLOC(1);
    *path = 0;
    json_t *new_kw = collapse(gobj, kw, path, collapse_lists_limit, collapse_dicts_limit);
    GBMEM_FREE(path)

    return new_kw;
}

/***************************************************************************
    Utility for databases.
    Get a json item walking by the tree (routed by path)
    Convention:
        - all arrays are list of records (dicts) with "id" field as primary key
        - delimiter are '`' and '.'
 ***************************************************************************/
PRIVATE json_t *_kwid_get(
    hgobj gobj,
    json_t *kw,  // NOT owned
    char *path,
    kw_flag_t flag
)
{
    BOOL backward = flag & KW_BACKWARD;
    BOOL verbose = flag & KW_VERBOSE;


    if(flag & KW_LOWER) {
        strntolower(path, strlen(path));
    }

    int list_size = 0;
    const char **segments = split2(path, "`.", &list_size);

    json_t *v = kw;
    BOOL fin = FALSE;
    for(int i=0; i<list_size && !fin; i++) {
        const char *segment = *(segments +i);

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
            v = json_object_get(v, segment);
            if(!v) {
                fin = TRUE;
            }
            break;
        case JSON_ARRAY:
            {
                int idx; json_t *v_;
                BOOL found = FALSE;
                if(!backward) {
                    json_array_foreach(v, idx, v_) {
                        const char *id = json_string_value(json_object_get(v_, "id"));
                        if(id && strcmp(id, segment)==0) {
                            v = v_;
                            found = TRUE;
                            break;
                        }
                    }
                    if(!found) {
                        v = 0;
                        fin = TRUE;
                    }
                } else {
                    json_array_backward(v, idx, v_) {
                        const char *id = json_string_value(json_object_get(v_, "id"));
                        if(id && strcmp(id, segment)==0) {
                            v = v_;
                            found = TRUE;
                            break;
                        }
                    }
                    if(!found) {
                        v = 0;
                        fin = TRUE;
                    }
                }

            }
            break;
        default:
            fin = TRUE;
            break;
        }
    }

    split_free2(segments);

    return v;
}

/***************************************************************************
    Utility for databases.
    Get a json item walking by the tree (routed by path)
    Convention:
        - all arrays are list of records (dicts) with "id" field as primary key
        - delimiter are '`' and '.'
 ***************************************************************************/
PUBLIC json_t *kwid_get( // Return is NOT YOURS
    hgobj gobj,
    json_t *kw,  // NOT owned
    kw_flag_t flag,
    const char *path,
    ...
)
{
    va_list ap;
    char temp[4*1024]; temp[0] = 0;

    va_start(ap, path);
    vsnprintf(
        temp,
        sizeof(temp),
        path,
        ap
    );

    json_t *jn = _kwid_get(gobj, kw, temp, flag);

    va_end(ap);

    return jn;
}

/***************************************************************************
    Utility for databases.
    Return a new list from a "dict of records" or "list of records"
    WARNING the "id" of a dict's record is hardcorded to his key.
    Convention:
        - all arrays are list of records (dicts) with "id" field as primary key
        - delimiter is '`' and '.'

    If path is empty then use kw
 ***************************************************************************/
PUBLIC json_t *kwid_new_list(
    hgobj gobj,
    json_t *kw,  // NOT owned
    kw_flag_t flag,
    const char *path,
    ...
)
{
    va_list ap;
    char temp[4*1024]; temp[0] = 0;

    va_start(ap, path);
    vsnprintf(
        temp,
        sizeof(temp),
        path,
        ap
    );

    json_t *jn = _kwid_get(gobj, kw, temp, flag);

    va_end(ap);

    if(!jn) {
        // Error already logged if verbose
        return 0;
    }
    json_t *new_list = 0;

    switch(json_typeof(jn)) {
    case JSON_ARRAY:
        {
            new_list = jn;
            json_incref(new_list);
        }
        break;
    case JSON_OBJECT:
        {
            new_list = json_array();
            const char *key; json_t *v;
            json_object_foreach(jn, key, v) {
                json_object_set_new(v, "id", json_string(key)); // WARNING id hardcorded
                json_array_append(new_list, v);
            }
        }
        break;
    default:
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "wrong type for list",
            "path",         "%s", temp,
            "jn",           "%j", jn,
            NULL
        );
        break;
    }

    return new_list;
}

/***************************************************************************
    Utility for databases.
    Return a new dict from a "dict of records" or "list of records"
    WARNING the "id" of a dict's record is hardcorded to his key.
    Convention:
        - all arrays are list of records (dicts) with "id" field as primary key
        - delimiter is '`' and '.'
    If path is empty then use kw
 ***************************************************************************/
PUBLIC json_t *kwid_new_dict(
    hgobj gobj,
    json_t *kw,  // NOT owned
    kw_flag_t flag,
    const char *path,
    ...
)
{
    va_list ap;
    char temp[4*1024]; temp[0] = 0;

    va_start(ap, path);
    vsnprintf(
        temp,
        sizeof(temp),
        path,
        ap
    );

    json_t *jn = _kwid_get(gobj, kw, temp, flag);

    va_end(ap);

    if(!jn) {
        // Error already logged if verbose
        return 0;
    }

    json_t *new_dict = 0;

    switch(json_typeof(jn)) {
    case JSON_ARRAY:
        {
            new_dict = json_object();
            int idx; json_t *v;
            json_array_foreach(jn, idx, v) {
                const char *id = kw_get_str(gobj, v, "id", "", KW_REQUIRED);
                if(!empty_string(id)) {
                    json_object_set(new_dict, id, v);
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            new_dict = jn;
            json_incref(new_dict);
        }
        break;
    default:
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "wrong type for dict",
            "path",         "%s", temp,
            "jn",           "%j", jn,
            NULL
        );
        break;
    }

    return new_dict;
}

/***************************************************************************
    HACK Convention: private data begins with "_".
    This function return a duplicate of kw removing all private data
 ***************************************************************************/
PUBLIC json_t *kw_filter_private(
    hgobj gobj,
    json_t *kw  // owned
)
{
    json_t *new_kw = 0;
    if(json_is_object(kw)) {
        new_kw =  _duplicate_object(kw, 0, 1, TRUE);
    } else if(json_is_array(kw)) {
        new_kw = _duplicate_array(kw, 0, 1, TRUE);
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "json to duplicate must be an object or array",
            NULL
        );
    }
    KW_DECREF(kw)
    return new_kw;
}

/***************************************************************************
    HACK Convention: all metadata begins with "__".
    This function return a duplicate of kw removing all metadata
 ***************************************************************************/
PUBLIC json_t *kw_filter_metadata(
    hgobj gobj,
    json_t *kw  // owned
)
{
    json_t *new_kw = 0;
    if(json_is_object(kw)) {
        new_kw =  _duplicate_object(kw, 0, 2, TRUE);
    } else if(json_is_array(kw)) {
        new_kw = _duplicate_array(kw, 0, 2, TRUE);
    } else {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "json to duplicate must be an object or array",
            NULL
        );
    }
    KW_DECREF(kw)
    return new_kw;
}

/***************************************************************************
    Utility for databases of json records.
    Get a json list or dict, get the **first** record that match `id`
    WARNING `id` is the first key of json_desc
    Convention:
        - If it's a list of dict: the records have "id" field as primary key
        - If it's a dict, the key is the `id`
 ***************************************************************************/
PUBLIC json_t *kwjr_get( // Return is NOT yours, unless use of KW_EXTRACT
    hgobj gobj,
    json_t *kw,  // NOT owned
    const char *id,
    json_t *new_record,  // owned
    const json_desc_t *json_desc,
    size_t *idx_,      // If not null set the idx in case of array
    kw_flag_t flag
)
{
    json_t *v = NULL;
    BOOL backward = flag & KW_BACKWARD;

    if(idx_) { // error case
        *idx_ = 0;
    }

    if(!(json_is_array(kw) || json_is_object(kw))) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "kw must be dict or list",
            NULL
        );
        JSON_DECREF(new_record)
        return NULL;
    }
    if(empty_string(id)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "id NULL",
            NULL
        );
        JSON_DECREF(new_record)
        return NULL;
    }

    switch(json_typeof(kw)) {
    case JSON_OBJECT:
        v = json_object_get(kw, id);
        if(!v) {
            if((flag & KW_CREATE)) {
                json_t *jn_record = create_json_record(gobj, json_desc);
                json_object_update_new(jn_record, json_deep_copy(new_record));
                json_object_set_new(kw, id, jn_record);
                JSON_DECREF(new_record)
                return jn_record;
            }
            if(flag & KW_REQUIRED) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "record NOT FOUND",
                    "id",           "%s", id,
                    NULL
                );
                gobj_trace_json(gobj, kw, "record NOT FOUND");
            }
            JSON_DECREF(new_record)
            return NULL;
        }
        if(flag & KW_EXTRACT) {
            json_incref(v);
            json_object_del(kw, id);
        }

        JSON_DECREF(new_record)
        return v;

    case JSON_ARRAY:
        {
            const char *pkey = json_desc->name;
            if(!backward) {
                size_t idx;
                json_array_foreach(kw, idx, v) {
                    const char *id_ = json_string_value(json_object_get(v, pkey));
                    if(id_ && strcmp(id_, id)==0) {
                        if(idx_) {
                            *idx_ = idx;
                        }
                        JSON_DECREF(new_record)
                        if(flag & KW_EXTRACT) {
                            json_incref(v);
                            json_array_remove(kw, idx);
                        }
                        return v;
                    }
                }
            } else {
                int idx;
                json_array_backward(kw, idx, v) {
                    const char *id_ = json_string_value(json_object_get(v, pkey));
                    if(id_ && strcmp(id_, id)==0) {
                        if(idx_) {
                            *idx_ = idx;
                        }
                        JSON_DECREF(new_record)
                        if(flag & KW_EXTRACT) {
                            json_incref(v);
                            json_array_remove(kw, idx);
                        }
                        return v;
                    }
                }
            }

            if((flag & KW_CREATE)) {
                json_t *jn_record = create_json_record(gobj, json_desc);
                json_object_update_new(jn_record, json_deep_copy(new_record));
                json_array_append_new(kw, jn_record);
                JSON_DECREF(new_record)
                return jn_record;
            }

            if(flag & KW_REQUIRED) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "record NOT FOUND",
                    "id",           "%s", id,
                    NULL
                );
                gobj_trace_json(gobj, kw, "record NOT FOUND");
            }
            JSON_DECREF(new_record)
            return NULL;
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

    JSON_DECREF(new_record)
    return NULL;
}

/***************************************************************************
    Utility for databases.
    Being `ids` a:

        "$id"

        {
            "$id": {
                "id": "$id",
                ...
            }
            ...
        }

        ["$id", ...]

        [
            "$id",
            {
                "id":$id,
                ...
            },
            ...
        ]

    return a list of all ids (all duplicated items)
 ***************************************************************************/
PUBLIC json_t *kwid_get_ids(
    json_t *ids // not owned
)
{
    if(!ids) {
        return 0;
    }

    json_t *new_ids = json_array();

    switch(json_typeof(ids)) {
        case JSON_STRING:
            /*
                "$id"
             */
            json_array_append_new(new_ids, json_string(json_string_value(ids)));
            break;

        case JSON_INTEGER:
            /*
                $id
             */
            json_array_append_new(
                new_ids,
                json_sprintf("%"JSON_INTEGER_FORMAT, json_integer_value(ids))
            );
            break;

        case JSON_OBJECT:
            /*
                {
                    "$id": {
                        "id": "$id",
                        ...
                    }
                    ...
                }
            */
        {
            const char *id; json_t *jn_value;
            json_object_foreach(ids, id, jn_value) {
                json_array_append_new(new_ids, json_string(id));
            }
        }
            break;

        case JSON_ARRAY:
        {
            int idx; json_t *jn_value;
            json_array_foreach(ids, idx, jn_value) {
                switch(json_typeof(jn_value)) {
                    case JSON_STRING:
                        /*
                            ["$id", ...]
                        */
                    {
                        const char *id = json_string_value(jn_value);
                        if(!empty_string(id)) {
                            json_array_append_new(new_ids, json_string(id));
                        }
                    }
                        break;

                    case JSON_INTEGER:
                        /*
                            $id
                        */
                        json_array_append_new(
                            new_ids,
                            json_sprintf("%"JSON_INTEGER_FORMAT, json_integer_value(jn_value))
                        );
                        break;

                    case JSON_OBJECT:
                        /*
                            [
                                {
                                    "id":$id,
                                    ...
                                },
                                ...
                            ]
                        */
                    {
                        const char *id = json_string_value(json_object_get(jn_value, "id"));
                        if(!empty_string(id)) {
                            json_array_append_new(new_ids, json_string(id));
                        }
                    }
                        break;
                    default:
                        break;
                }
            }
        }
            break;

        default:
            break;
    }

    return new_ids;
}

/***************************************************************************
    Has word? Can be in string, list or dict.
    options: "recursive", "verbose"

    Use to configurate:

        "opt2"              No has word "opt1"
        "opt1|opt2"         Yes, has word "opt1"
        ["opt1", "opt2"]    Yes, has word "opt1"
        {
            "opt1": TRUE,   Yes, has word "opt1"
            "opt2": FALSE   No has word "opt1"
        }

 ***************************************************************************/
PUBLIC BOOL kw_has_word(
    hgobj gobj,
    json_t *kw,  // NOT owned
    const char *word,
    kw_flag_t flag
)
{
    if(!kw) {
        if(flag & KW_VERBOSE) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "kw_has_word() kw NULL",
                NULL
            );
        }
        return FALSE;
    }

    switch(json_typeof(kw)) {
    case JSON_OBJECT:
        if(kw_has_key(kw, word)) {
            return json_is_true(json_object_get(kw, word))?TRUE:FALSE;
        } else {
            return FALSE;
        }
    case JSON_ARRAY:
        {
            int idx; json_t *jn_value;
            json_array_foreach(kw, idx, jn_value) {
                if(json_is_string(jn_value)) {
                    if(strstr(json_string_value(jn_value), word)) {
                        return TRUE;
                    }
                } else if(flag & KW_REQUIRED) {
                    if(kw_has_word(gobj, jn_value, word, flag)) {
                        return TRUE;
                    }
                }
            }
            return FALSE;
        }
    case JSON_STRING:
        if(strstr(json_string_value(kw), word)) {
            return TRUE;
        } else {
            return FALSE;
        }
    default:
        if(flag & KW_VERBOSE) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Searching word needs object,array,string",
                "word",         "%s", word,
                NULL
            );
            gobj_trace_json(gobj, kw, "Searching word needs object,array,string");
        }
        return FALSE;
    }
}

/***************************************************************************
    Utility for databases.
    Return TRUE if `id` is in the list/dict/str `ids`
 ***************************************************************************/
PUBLIC BOOL kwid_match_id(hgobj gobj, json_t *ids, const char *id)
{
    if(!ids || !id) {
        // Si no hay filtro pasan todos.
        return TRUE;
    }
    if(json_is_object(ids) && json_object_size(ids)==0) {
        // A empty object at first level evaluate as TRUE.
        return TRUE;
    }
    if(json_is_array(ids) && json_array_size(ids)==0) {
        // A empty object at first level evaluate as TRUE.
        return TRUE;
    }

    switch(json_typeof(ids)) {
    case JSON_ARRAY:
        {
            int idx; json_t *jn_value;
            json_array_foreach(ids, idx, jn_value) {
                if(json_is_string(jn_value)) {
                    const char *value = json_string_value(jn_value);
                    if(value && strcmp(id, value)==0)  {
                        return TRUE;
                    }
                } else if(json_is_object(jn_value)) {
                    const char *value = kw_get_str(gobj, jn_value, "id", 0, 0);
                    if(value && strcmp(id, value)==0)  {
                        return TRUE;
                    }
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            const char *key; json_t *jn_value;
            json_object_foreach(ids, key, jn_value) {
                if(strcmp(id, key)==0)  {
                    return TRUE;
                }
            }
        }
        break;

    case JSON_STRING:
        if(strcmp(id, json_string_value(ids))==0) {
            return TRUE;
        }
        break;

    default:
        break;
    }
    return FALSE;
}

/***************************************************************************
    Utility for databases.
    Return TRUE if `id` WITH LIMITED SIZE is in the list/dict/str `ids`
 ***************************************************************************/
PUBLIC BOOL kwid_match_nid(hgobj gobj, json_t *ids, const char *id, int max_id_size)
{
    if(!ids || !id) {
        // Si no hay filtro pasan todos.
        return TRUE;
    }
    if(json_is_object(ids) && json_object_size(ids)==0) {
        // A empty object at first level evaluate as TRUE.
        return TRUE;
    }
    if(json_is_array(ids) && json_array_size(ids)==0) {
        // A empty object at first level evaluate as TRUE.
        return TRUE;
    }

    switch(json_typeof(ids)) {
    case JSON_ARRAY:
        {
            int idx; json_t *jn_value;
            json_array_foreach(ids, idx, jn_value) {
                if(json_is_string(jn_value)) {
                    const char *value = json_string_value(jn_value);
                    if(value && strncmp(id, value, max_id_size)==0)  {
                        return TRUE;
                    }
                } else if(json_is_object(jn_value)) {
                    const char *value = kw_get_str(gobj, jn_value, "id", 0, 0);
                    if(value && strncmp(id, value, max_id_size)==0)  {
                        return TRUE;
                    }
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            const char *key; json_t *jn_value;
            json_object_foreach(ids, key, jn_value) {
                if(strncmp(id, key, max_id_size)==0)  {
                    return TRUE;
                }
            }
        }
        break;

    case JSON_STRING:
        if(strncmp(id, json_string_value(ids), max_id_size)==0) {
            return TRUE;
        }
        break;

    default:
        break;
    }
    return FALSE;
}
