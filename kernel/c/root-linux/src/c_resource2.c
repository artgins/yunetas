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
#include <stdio.h>
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
PRIVATE int build_resouce_path(hgobj gobj, char *bf, int bflen, const char *resource);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag----------------default---------description---------- */
SDATA (ASN_BOOLEAN,     "strict",           SDF_RD,             FALSE,          "Only fields of schema are saved"),
SDATA (ASN_BOOLEAN,     "ignore_private",   SDF_RD,             TRUE,           "Don't save fields beginning by _"),
SDATA (ASN_POINTER,     "json_desc",        SDF_RD,             0,              "C struct json_desc_t with the schema of records. Empty is no schema"),
SDATA (ASN_BOOLEAN,     "persistent",       SDF_RD,             TRUE,           "Resources are persistent"),
SDATA (ASN_OCTET_STR,   "service",          SDF_RD,             "",             "Service name for global store, for example 'mqtt'"),
SDATA (ASN_OCTET_STR,   "database",         SDF_RD,             0,              "Database name. Path is store/resources/{service}/yuno_role_plus_name()/{database}/"),

SDATA (ASN_POINTER,     "user_data",        0,                  0,              "user data"),
SDATA (ASN_POINTER,     "user_data2",       0,                  0,              "more user data"),
SDATA (ASN_POINTER,     "subscriber",       0,                  0,              "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messagess",       "Trace messages"},
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
    build_path3(path_service, sizeof(path_service),
        priv->service,
        gobj_yuno_role_plus_name(),
        priv->database
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
    BOOL volatil = kw_get_bool(jn_options, "volatil", 0, 0);
    BOOL update = kw_get_bool(jn_options, "update", 0, 0);

    if(!kw) {
        kw = json_object();
    }

    if(empty_string(resource)) {
        log_error(LOG_OPT_TRACE_STACK,
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

    json_t *jn_resource = kw_get_dict(priv->db_resources, resource, 0, 0);
    if(jn_resource) {
        char path[PATH_MAX];
        build_resouce_path(gobj, path, sizeof(path), resource);

        if(!update) {
            log_error(LOG_OPT_TRACE_STACK,
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
                log_error(LOG_OPT_TRACE_STACK,
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
            JSON_INCREF(jn_resource) // Will be decref in kw_set_dict_value()->json_object_set_new()
        }
    }

    /*----------------------------------*
     *  Free content or with schema
     *----------------------------------*/
    json_t *record = jn_resource;
    if(priv->json_desc) {
        if(!record) {
            record = create_json_record(priv->json_desc);
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
    kw_set_dict_value(priv->db_resources, resource, record);

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
        log_error(LOG_OPT_TRACE_STACK,
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

    json_t *jn_resource = kw_get_dict(priv->db_resources, resource, 0, 0);
    if(jn_resource != record) {
        log_error(LOG_OPT_TRACE_STACK,
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
        log_error(LOG_OPT_TRACE_STACK,
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

    json_t *jn_resource = kw_get_dict(priv->db_resources, resource, 0, 0);
    if(!jn_resource) {
        log_error(LOG_OPT_TRACE_STACK,
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
        log_error(LOG_OPT_TRACE_STACK,
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
        json_t *record = kw_get_dict(priv->db_resources, resource_, 0, 0);
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
        log_error(LOG_OPT_TRACE_STACK,
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

    json_t *record = kw_get_dict(priv->db_resources, resource, 0, 0);

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
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    int index               // index of file inside of directory, relative to 0
)
{
    hgobj gobj = user_data;
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
        log_error(0,
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
PRIVATE int build_resouce_path(hgobj gobj, char *bf, int bflen, const char *resource)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char filename[NAME_MAX];
    snprintf(filename, sizeof(filename), "%s.json", resource);

    build_path2(bf, bflen, priv->path_database, filename);

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
    build_resouce_path(gobj, path, sizeof(path), resource);

    int ret;
    if(priv->ignore_private) {
        json_t *_record = kw_filter_private(kw_incref(record));
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
        log_error(0,
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
    build_resouce_path(gobj, path, sizeof(path), resource);

    int ret = unlink(path);
    if(ret < 0) {
        log_error(0,
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




/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    {NULL, 0, 0, 0}
};
PRIVATE const EVENT output_events[] = {
    {NULL, 0, 0, 0}
};
PRIVATE const char *state_names[] = {
    "ST_IDLE",
    NULL
};

PRIVATE EV_ACTION ST_IDLE[] = {
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_IDLE,
    NULL
};

PRIVATE FSM fsm = {
    input_events,
    output_events,
    state_names,
    states,
};

/***************************************************************************
 *              GClass
 ***************************************************************************/
/*---------------------------------------------*
 *              Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {0, 0, 0}
};

/*---------------------------------------------*
 *              GClass
 *---------------------------------------------*/
PRIVATE GCLASS _gclass = {
    0,  // base
    GCLASS_RESOURCE2_NAME,
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        mt_destroy,
        mt_start,
        mt_stop,
        0, //mt_play,
        0, //mt_pause,
        mt_writing,
        0, //mt_reading,
        0, //mt_subscription_added,
        0, //mt_subscription_deleted,
        0, //mt_child_added,
        0, //mt_child_removed,
        0,
        0, //mt_command,
        0, //mt_inject_event,
        mt_create_resource,
        mt_list_resource,
        mt_save_resource,
        mt_delete_resource,
        0, //mt_future21,
        0, //mt_future22,
        mt_get_resource,
        0, //mt_state_changed,
        0, //mt_authenticate,
        0, //mt_list_childs,
        0, //mt_stats_updated,
        0, //mt_disable,
        0, //mt_enable,
        0, //mt_trace_on,
        0, //mt_trace_off,
        0, //mt_gobj_created,
        0, //mt_future33,
        0, //mt_future34,
        0, //mt_publish_event,
        0, //mt_publication_pre_filter,
        0, //mt_publication_filter,
        0, //mt_authz_checker,
        0, //mt_future39,
        0, //mt_create_node,
        0, //mt_update_node,
        0, //mt_delete_node,
        0, //mt_link_nodes,
        0, //mt_future44,
        0, //mt_unlink_nodes,
        0, //mt_topic_jtree,
        0, //mt_get_node,
        0, //mt_list_nodes,
        0, //mt_shoot_snap,
        0, //mt_activate_snap,
        0, //mt_list_snaps,
        0, //mt_treedbs,
        0, //mt_treedb_topics,
        0, //mt_topic_desc,
        0, //mt_topic_links,
        0, //mt_topic_hooks,
        0, //mt_node_parents,
        0, //mt_node_childs,
        0, //mt_list_instances,
        0, //mt_node_tree,
        0, //mt_topic_size,
        0, //mt_future62,
        0, //mt_future63,
        0, //mt_future64
    },
    lmt,
    tattr_desc,
    sizeof(PRIVATE_DATA),
    0,  // acl
    s_user_trace_level,
    0,  // command_table
    0,  // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_resource2(void)
{
    return &_gclass;
}
