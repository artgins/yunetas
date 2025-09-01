/***********************************************************************
 *          C_RESOURCE2.C
 *          Resource2 GClass.
 *
 *          Resource Controller using json files
 *
 *          Get persistent messages of a "resource" or "topic".
 *          Each resource/topic has a unique own flat file to save his record
 *          One file per resource
 *
 *          If you want history then use queues, history, series/time.
 *
 *          Copyright (c) 2022 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include "yunetas_environment.h"
#include "c_resource2.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int save_record(
    hgobj gobj,
    const char *resource,
    json_t *record, // not owned
    json_t *jn_options // not owned
);
PRIVATE int delete_record(
    hgobj gobj,
    const char *resource
);

PRIVATE int load_persistent_resources(hgobj gobj);
PRIVATE int build_resource_path(hgobj gobj, char *bf, int bflen, const char *resource);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default---------description---------- */
SDATA (DTP_BOOLEAN,     "strict",           SDF_RD,             "0",            "Only fields of schema are saved"),
SDATA (DTP_BOOLEAN,     "ignore_private",   SDF_RD,             "1",            "Don't save fields beginning by _"),
SDATA (DTP_POINTER,     "json_desc",        SDF_RD,             0,              "C struct json_desc_t with the schema of records. Empty is no schema"),
SDATA (DTP_BOOLEAN,     "persistent",       SDF_RD,             "1",            "Resources are persistent"),
SDATA (DTP_STRING,      "service",          SDF_RD,             "",             "Service name for global store, for example 'mqtt'"),
SDATA (DTP_STRING,      "database",         SDF_RD,             0,              "Database name. Path is store/resources/{service}/yuno_role_plus_name()/{database}/"),

SDATA (DTP_POINTER,     "user_data",        0,                  0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,              "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,              "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",       "Trace messages"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    json_t *db_resources;
    char path_database[PATH_MAX];
    const char *pkey;

    BOOL strict;
    BOOL ignore_private;
    json_desc_t *json_desc;
    const char *service;
    const char *database;
    BOOL persistent;
} PRIVATE_DATA;





            /******************************
             *      Framework Methods
             ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->db_resources = json_object();

    /*
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    } else if(gobj_is_pure_child(gobj)) {
        subscriber = gobj_parent(gobj);
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(strict,                gobj_read_bool_attr)
    SET_PRIV(ignore_private,        gobj_read_bool_attr)
    SET_PRIV(json_desc,             gobj_read_pointer_attr)
    SET_PRIV(persistent,            gobj_read_bool_attr)
    SET_PRIV(service,               gobj_read_str_attr)
    SET_PRIV(database,              gobj_read_str_attr)

    char path_service[NAME_MAX];
    build_path(path_service, sizeof(path_service),
        priv->service,
        gobj_yuno_role_plus_name(),
        priv->database,
        NULL
    );
    if(priv->persistent) {
        yuneta_store_dir(
            priv->path_database, sizeof(priv->path_database), "resources", path_service, TRUE
        );
    }
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    //PRIVATE_DATA *priv = gobj_priv_data(gobj);

    //IF_EQ_SET_PRIV(persistent,             gobj_read_bool_attr)
    //END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    JSON_DECREF(priv->db_resources);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->db_resources) {
        priv->db_resources = json_object();
    }
    if(priv->persistent) {
        load_persistent_resources(gobj);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    JSON_DECREF(priv->db_resources);
    return 0;
}

/***************************************************************************
 *      Framework Method create_resource
 ***************************************************************************/
PRIVATE json_t *mt_create_resource(hgobj gobj, const char *resource, json_t *kw, json_t *jn_options)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    BOOL volatil = jn_options?kw_get_bool(gobj, jn_options, "volatil", 0, 0):FALSE;
    BOOL update = jn_options?kw_get_bool(gobj, jn_options, "update", 0, 0):FALSE;

    if(!kw) {
        kw = json_object();
    }

    if(empty_string(resource)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Resource name empty",
            "service",      "%s", priv->service,
            "database",     "%s", priv->database,
            NULL
        );
        KW_DECREF(kw)
        JSON_DECREF(jn_options)
        return 0;
    }

    json_t *jn_resource = kw_get_dict(gobj, priv->db_resources, resource, 0, 0);
    if(jn_resource) {
        char path[PATH_MAX];
        build_resource_path(gobj, path, sizeof(path), resource);

        if(!update) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "Resource already exists",
                "path",         "%s", path,
                "service",      "%s", priv->service,
                "database",     "%s", priv->database,
                "resource",     "%s", resource,
                NULL
            );
            KW_DECREF(kw)
            JSON_DECREF(jn_options)
            return 0;
        } else {
            if(jn_resource == kw) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "gobj",         "%s", gobj_full_name(gobj),
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "Creating same resource as kw, use gobj_save_resource()",
                    "path",         "%s", path,
                    "service",      "%s", priv->service,
                    "database",     "%s", priv->database,
                    "resource",     "%s", resource,
                    NULL
                );
                JSON_DECREF(jn_options)
                return 0;
            }
            JSON_INCREF(jn_resource) // Will be decref in kw_set_dict_value(gobj, )->json_object_set_new()
        }
    }

    /*----------------------------------*
     *  Free content or with schema
     *----------------------------------*/
    json_t *record = jn_resource;
    if(priv->json_desc) {
        if(!record) {
            record = create_json_record(gobj, priv->json_desc);
        }
        if(priv->strict) {
            json_object_update_existing_new(record, kw); // kw owned
        } else {
            json_object_update_new(record, kw); // kw owned
        }
    } else {
        if(!record) {
            record = kw; // kw owned
        } else {
            json_object_update_new(record, kw); // kw owned
        }
    }

    /*------------------------------------------*
     *      Create resource
     *------------------------------------------*/
    kw_set_dict_value(gobj, priv->db_resources, resource, record);

    /*------------------------------------------*
     *      Save if persistent
     *------------------------------------------*/
    if(priv->persistent) {
        if(!volatil) {
            save_record(gobj, resource, record, jn_options);
        }
    }

    JSON_DECREF(jn_options)
    return record;
}

/***************************************************************************
 *      Framework Method update_resource
 ***************************************************************************/
PRIVATE int mt_save_resource(
    hgobj gobj,
    const char *resource,
    json_t *record,  // NOT owned
    json_t *jn_options // owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(empty_string(resource)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Resource name empty",
            "service",      "%s", priv->service,
            "database",     "%s", priv->database,
            NULL
        );
        JSON_DECREF(jn_options)
        return -1;
    }

    json_t *jn_resource = kw_get_dict(gobj, priv->db_resources, resource, 0, 0);
    if(jn_resource != record) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Saving a different resource",
            "service",      "%s", priv->service,
            "database",     "%s", priv->database,
            NULL
        );
        JSON_DECREF(jn_options)
        return -1;
    }

    if(priv->persistent) {
        int ret = save_record(gobj, resource, record, jn_options);
        JSON_DECREF(jn_options)
        return ret;
    }

    JSON_DECREF(jn_options)
    return 0;
}

/***************************************************************************
 *      Framework Method delete_resource
 ***************************************************************************/
PRIVATE int mt_delete_resource(
    hgobj gobj,
    const char *resource,
    json_t *record,  // owned
    json_t *jn_options // owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(empty_string(resource)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Resource name empty",
            "service",      "%s", priv->service,
            "database",     "%s", priv->database,
            NULL
        );
        JSON_DECREF(record)
        JSON_DECREF(jn_options)
        return -1;
    }

    json_t *jn_resource = kw_get_dict(gobj, priv->db_resources, resource, 0, 0);
    if(!jn_resource) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "No resource found",
            "service",      "%s", priv->service,
            "database",     "%s", priv->database,
            "resource",     "%s", resource,
            NULL
        );
        JSON_DECREF(record)
        JSON_DECREF(jn_options)
        return -1;
    }

    if(record && record != jn_resource) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Deleting a different resource",
            "service",      "%s", priv->service,
            "database",     "%s", priv->database,
            NULL
        );
        JSON_DECREF(record)
        JSON_DECREF(jn_options)
        return -1;
    }

    if(priv->persistent) {
        delete_record(gobj, resource);
    }

    json_object_del(priv->db_resources, resource);

    JSON_DECREF(jn_options)
    return 0;
}

/***************************************************************************
 *      Framework Method list_resource
 ***************************************************************************/
PRIVATE void *mt_list_resource(
    hgobj gobj,
    const char *resource_,
    json_t *jn_filter,  // owned
    json_t *jn_options  // owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *list = json_array();

    if(!empty_string(resource_)) {
        // Like get_resource
        json_t *record = kw_get_dict(gobj, priv->db_resources, resource_, 0, 0);
        if(record) {
            json_array_append(list, record);
        }
        JSON_DECREF(jn_filter)
        JSON_DECREF(jn_options)
        return list;
    }

    const char *resource; json_t *record;
    json_object_foreach(priv->db_resources, resource, record) {
        if(jn_filter) {
            if(kw_match_simple(record, json_incref(jn_filter))) {
                if(json_is_array(jn_options)) {
                    json_t *new_record = kw_clone_by_keys(
                        gobj,
                        json_incref(record),     // owned
                        json_incref(jn_options),   // owned
                        FALSE
                    );
                    json_array_append_new(list, new_record);
                } else {
                    json_array_append(list, record);
                }
            }
        } else {
            if(json_is_array(jn_options)) {
                json_t *new_record = kw_clone_by_keys(
                    gobj,
                    json_incref(record),     // owned
                    json_incref(jn_options),   // owned
                    FALSE
                );
                json_array_append_new(list, new_record);
            } else {
                json_array_append(list, record);
            }
        }
    }

    JSON_DECREF(jn_filter)
    JSON_DECREF(jn_options)
    return list;
}

/***************************************************************************
 *      Framework Method get_resource
 ***************************************************************************/
PRIVATE json_t *mt_get_resource(
    hgobj gobj,
    const char *resource,
    json_t *jn_filter,  // owned, NOT USED
    json_t *jn_options  // owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(empty_string(resource)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Resource name empty",
            "service",      "%s", priv->service,
            "database",     "%s", priv->database,
            NULL
        );
        JSON_DECREF(jn_filter)
        JSON_DECREF(jn_options)
        return 0;
    }

    json_t *record = kw_get_dict(gobj, priv->db_resources, resource, 0, 0);

    JSON_DECREF(jn_filter)
    JSON_DECREF(jn_options)
    return record;
}




            /***************************
             *      Commands
             ***************************/




            /***************************
             *      Private Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL load_resource_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    wd_option opt           // option parameter
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t flags = 0;
    json_error_t error;
    json_t *record = json_load_file(fullpath, flags, &error);
    if(record) {
        char *p = strstr(name, ".json");
        if(p) {
            *p = 0;
        }
        json_object_set_new(priv->db_resources, name, record);
    } else {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "Wrong json file",
            "path",         "%s", fullpath,
            NULL
        );
    }
    return TRUE; // to continue
}

PRIVATE int load_persistent_resources(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return walk_dir_tree(
        gobj,
        priv->path_database,
        ".*\\.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        load_resource_cb,
        gobj
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int build_resource_path(hgobj gobj, char *bf, int bflen, const char *resource)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.json", resource);

    build_path(bf, bflen, priv->path_database, filename, NULL);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int save_record(
    hgobj gobj,
    const char *resource,
    json_t *record, // not owned
    json_t *jn_options // not owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char path[PATH_MAX];
    build_resource_path(gobj, path, sizeof(path), resource);

    int ret;
    if(priv->ignore_private) {
        json_t *_record = kw_filter_private(gobj, kw_incref(record));
        ret = json_dump_file(
            _record,
            path,
            JSON_INDENT(4)
        );
        JSON_DECREF(_record)
    } else {
        ret = json_dump_file(
            record,
            path,
            JSON_INDENT(4)
        );
    }

    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", __FILE__,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_JSON_ERROR,
            "msg",          "%s", "Cannot save record",
            "path",         "%s", path,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int delete_record(
    hgobj gobj,
    const char *resource
)
{
    char path[PATH_MAX];
    build_resource_path(gobj, path, sizeof(path), resource);

    int ret = unlink(path);
    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "Cannot delete record file",
            "path",         "%s", path,
            NULL
        );
        return -1;
    }
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/

                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_create_resource = mt_create_resource,
    .mt_list_resource = mt_list_resource,
    .mt_save_resource = mt_save_resource,
    .mt_delete_resource = mt_delete_resource,
    .mt_get_resource = mt_get_resource,
};


/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_RESOURCE2);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,  // s_user_trace_level
        0   // gclass_flag
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_resource2(void)
{
    return create_gclass(C_RESOURCE2);
}
