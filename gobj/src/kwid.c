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
PUBLIC json_t *load_json_from_file(
    const char *directory,
    const char *filename,
    log_opt_t on_critical_error
)
{
    /*
     *  Full path
     */
    char full_path[PATH_MAX];
    build_path(full_path, sizeof(full_path), directory, filename);

    if(access(full_path, 0)!=0) {
        return 0;
    }

    int fd = open(full_path, O_RDONLY|O_NOFOLLOW);
    if(fd<0) {
        gobj_log_critical(0, on_critical_error,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot open json file",
            "path",         "%s", full_path,
            "errno",        "%s", strerror(errno),
            NULL
        );
        return 0;
    }

    json_t *jn = json_loadfd(fd, 0, 0);
    if(!jn) {
        gobj_log_critical(0, on_critical_error,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "Cannot load json file, bad json",
            NULL
        );
    }
    close(fd);
    return jn;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int save_json_to_file(
    const char *directory,
    const char *filename,
    int xpermission,
    int rpermission,
    log_opt_t on_critical_error,
    BOOL create,        // Create file if not exists or overwrite.
    BOOL only_read,
    json_t *jn_data     // owned
)
{
    /*-----------------------------------*
     *  Check parameters
     *-----------------------------------*/
    if(!directory || !filename) {
        gobj_log_critical(0, on_critical_error|LOG_OPT_TRACE_STACK,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Parameter 'directory' or 'filename' NULL",
            NULL
        );
        JSON_DECREF(jn_data);
        return -1;
    }

    /*-----------------------------------*
     *  Create directory if not exists
     *-----------------------------------*/
    if(!is_directory(directory)) {
        if(!create) {
            JSON_DECREF(jn_data);
            return -1;
        }
        if(mkrdir(directory, 0, xpermission)<0) {
            gobj_log_critical(0, on_critical_error,
                "gobj",         "%s", __FILE__,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "Cannot create directory",
                "directory",    "%s", directory,
                "errno",        "%s", strerror(errno),
                NULL
            );
            JSON_DECREF(jn_data);
            return -1;
        }
    }

    /*
     *  Full path
     */
    char full_path[PATH_MAX];
    if(empty_string(filename)) {
        snprintf(full_path, sizeof(full_path), "%s", directory);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", directory, filename);
    }

    /*
     *  Create file
     */
    int fp = newfile(full_path, rpermission, create);
    if(fp < 0) {
        gobj_log_critical(0, on_critical_error,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot create json file",
            "filename",     "%s", full_path,
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        JSON_DECREF(jn_data);
        return -1;
    }

    if(json_dumpfd(jn_data, fp, JSON_INDENT(2))<0) {
        gobj_log_critical(0, on_critical_error,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "Cannot write in json file",
            "errno",        "%s", strerror(errno),
            NULL
        );
        JSON_DECREF(jn_data);
        return -1;
    }
    close(fp);
    if(only_read) {
        chmod(full_path, 0440);
    }
    JSON_DECREF(jn_data);

    return 0;
}

/***************************************************************************
 *  fields: DESC str array with: key, type, defaults
 *  type can be: str, int, real, bool, null, dict, list
 ***************************************************************************/
PUBLIC json_t *create_json_record(
    hgobj gobj,
    const json_desc_t *json_desc
)
{
    if(!json_desc) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "DESC null",
            NULL
        );
        return 0;
    }
    json_t *jn = json_object();

    while(json_desc->name) {
        if(empty_string(json_desc->name)) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "DESC without key field",
                NULL
            );
            break;
        }
        if(empty_string(json_desc->type)) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "DESC without type field",
                NULL
            );
            break;
        }
        const char *name = json_desc->name;
        const char *defaults = json_desc->defaults;

        SWITCHS(json_desc->type) {
            CASES("str")
            CASES("string")
                json_object_set_new(jn, name, json_string(defaults));
                break;
            CASES("int")
            CASES("integer")
                unsigned long v=0;
                if(*defaults == '0') {
                    v = strtoul(defaults, 0, 8);
                } else if(*defaults == 'x') {
                    v = strtoul(defaults, 0, 16);
                } else {
                    v = strtoul(defaults, 0, 10);
                }
                json_object_set_new(jn, name, json_integer(v));
                break;
            CASES("real")
                json_object_set_new(jn, name, json_real(strtod(defaults, NULL)));
                break;
            CASES("bool")
            CASES("boolean")
                if(strcasecmp(defaults, "true")==0) {
                    json_object_set_new(jn, name, json_true());
                } else if(strcasecmp(defaults, "false")==0) {
                    json_object_set_new(jn, name, json_false());
                } else {
                    json_object_set_new(jn, name, atoi(defaults)?json_true():json_false());
                }
                break;
            CASES("null")
                json_object_set_new(jn, name, json_null());
                break;
            CASES("dict")
                char desc_name[80+1];
                if(sscanf(defaults, "{%80s}", desc_name)==1) {
                    //get_fields(db_tranger_desc, desc_name, TRUE); // only to test fields
                } else if(!empty_string(defaults)) {
                    //get_fields(db_tranger_desc, defaults, TRUE); // only to test fields
                }
                json_object_set_new(jn, name, json_object());
                break;
            CASES("list")
                char desc_name[80+1];
                if(sscanf(defaults, "{%80s}", desc_name)==1) {
                    //get_fields(db_tranger_desc, desc_name, TRUE); // only to test fields
                }
                json_object_set_new(jn, name, json_array());
                break;
            DEFAULTS
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "Type UNKNOWN",
                    "type",         "%s", json_desc->type,
                    NULL
                );
                break;
        } SWITCHS_END;

        json_desc++;
    }

    return jn;
}

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
PUBLIC json_t *kw_serialize(
    hgobj gobj,
    json_t *kw // owned
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
    KW_DECREF(kw);
    return kw;
}

/***************************************************************************
 *  Deserialize fields
 ***************************************************************************/
PUBLIC json_t *kw_deserialize(
    hgobj gobj,
    json_t *kw // owned
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
                JSON_DECREF(jn_serialized); // TODO ??? check it

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
    KW_DECREF(kw);
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
    if((int)kw->refcount <= 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "BAD kw_decref()",
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
    const char **segments = split3(path, delimiter, &list_size);

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

    split_free3(segments);
    return v;
}

/***************************************************************************
 *  Like json_object_set but with a path
 *  (doesn't create arrays, only objects)
 ***************************************************************************/
PUBLIC int kw_set_dict_value(
    hgobj gobj,
    json_t *kw,
    const char *path,   // The last word after '.' is the key
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
        return 0;
    }

    int list_size = 0;
    const char **segments = split3(path, delimiter, &list_size);

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
                json_object_set_new(v, segment, json_object());
                next = json_object_get(v, segment);
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

    split_free3(segments);

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




                    /*---------------------------------*
                     *          KWID
                     *---------------------------------*/




/***************************************************************************
    Utility for databases.
    Get a json item walking by the tree (routed by path)
    options:  "verbose", "backward", "lower", "upper"
    Convention:
        - all arrays are list of records (dicts) with "id" field as primary key
        - delimiter are '`' by default, can be changed by kw_set_path_delimiter
 ***************************************************************************/
PUBLIC json_t *kwid_get(
    hgobj gobj,
    json_t *kw,  // NOT owned
    char *path,
    const char *options // "verbose", "backward", "lower", "upper"
)
{
    BOOL verbose = (options && strstr(options, "verbose"))?TRUE:FALSE;
    BOOL backward = (options && strstr(options, "backward"))?TRUE:FALSE;

    if(options && strstr(options, "lower")) {
        strntolower(path, strlen(path));
    }
    if(options && strstr(options, "upper")) {
        strntoupper(path, strlen(path));
    }

    int list_size;
    const char **segments = split3(path, delimiter, &list_size);

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

    split_free3(segments);

    return v;
}
