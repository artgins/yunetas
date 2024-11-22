/***********************************************************************
 *          TR_MSG2.C
 *
 *          Messages (ordered by key (id),pkey2) with TimeRanger
 *
 *          Double dict of messages
 *          Load in memory a iter of topic's messages ordered by a sub-key
 *
 *          Copyright (c) 2019 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 *
***********************************************************************/
#include <string.h>
#include <stdio.h>

#include <kwid.h>
#include "tr_treedb.h"
#include "tr_msg2db.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/


/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE json_t *record2tranger(
    json_t *tranger,
    const char *topic_name,
    json_t *kw,  // not owned
    const char *options
);
PRIVATE json_t *md2json(
    const char *msg2db_name,
    const char *topic_name,
    md2_record_ex_t *md_record
);
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list, // iterator or rt_list/rt_disk id, don't own
    json_int_t rowid,   // in a rt_mem will be the relative rowid, in rt_disk the absolute rowid
    md2_record_ex_t *md_record,
    json_t *jn_record // must be owned, can be null if only_md
);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE json_t *topic_cols_desc = 0;

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC char *build_msg2db_index_path(
    char *bf,
    int bfsize,
    const char *msg2db_name,
    const char *topic_name,
    const char *key
)
{
    snprintf(bf, bfsize, "msg2dbs`%s`%s`%s", msg2db_name, topic_name, key);
    return bf;
}

/***************************************************************************
    Open a message2 db (Remember previously open tranger_startup())

    List of pkey/pkey2 of nodes of data.
    Node's data contains attributes, in json (record, node, message, ... what you want)


    HACK Conventions:
        1) the pkey of all topics must be "id".
        2) the "id" field (primary key) MUST be a string.
        3) define a second index `pkey2`, MUST be a string

    Option "persistent"
        Try to load the schema from file
        File has precedence.
        Once saved,
            if you want to change the schema
            then you must change the schema version and topic_version
 ***************************************************************************/
PUBLIC json_t *msg2db_open_db(
    json_t *tranger,
    const char *msg2db_name_,
    json_t *jn_schema,  // owned
    const char *options // "persistent"
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    char msg2db_name[NAME_MAX];
    snprintf(msg2db_name, sizeof(msg2db_name), "%s", kw_get_str(gobj, jn_schema, "id", msg2db_name_, KW_REQUIRED));

    BOOL master = kw_get_bool(gobj, tranger, "master", 0, KW_REQUIRED);

    if(empty_string(msg2db_name)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MSG2DB_ERROR,
            "msg",          "%s", "msg2db_name NULL",
            NULL
        );
        JSON_DECREF(jn_schema)
        return 0;
    }

    char schema_filename[NAME_MAX + sizeof(".msg2db_schema.json")];
    if(strstr(msg2db_name, ".msg2db_schema.json")) {
        snprintf(schema_filename, sizeof(schema_filename), "%s",
            msg2db_name
        );
    } else {
        snprintf(schema_filename, sizeof(schema_filename), "%s.msg2db_schema.json",
            msg2db_name
        );
    }

    char schema_full_path[NAME_MAX*2];
    snprintf(schema_full_path, sizeof(schema_full_path), "%s/%s",
        kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
        schema_filename
    );

    json_int_t schema_new_version = kw_get_int(gobj, jn_schema, "schema_version", 0, KW_WILD_NUMBER);
    json_int_t schema_version = schema_new_version;

    if(options && strstr(options,"persistent")) {
        do {
            BOOL recreating = FALSE;
            if(file_exists(schema_full_path, 0)) {
                json_t *old_jn_schema = load_json_from_file(
                    gobj,
                    schema_full_path,
                    "",
                    kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED)
                );
                if(!master) {
                    JSON_DECREF(jn_schema)
                    jn_schema = old_jn_schema;
                    break; // Nothing to do
                }
                json_int_t schema_old_version = kw_get_int(gobj,
                    old_jn_schema,
                    "schema_version",
                    -1,
                    KW_WILD_NUMBER
                );
                if(schema_new_version <= schema_old_version) {
                    JSON_DECREF(jn_schema)
                    jn_schema = old_jn_schema;
                    schema_version = schema_old_version;
                    break; // Nothing to do
                } else {
                    recreating = TRUE;
                    schema_version = schema_old_version;
                    JSON_DECREF(old_jn_schema);
                }
            }
            gobj_log_info(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_INFO,
                "msg",              "%s", recreating?"Re-Creating Msg2DB schema file":"Creating Msg2DB schema file",
                "msg2db_name",      "%s", msg2db_name,
                "schema_version",   "%d", schema_new_version,
                "schema_file",      "%s", schema_full_path,
                NULL
            );
            JSON_INCREF(jn_schema)
            save_json_to_file(
                gobj,
                kw_get_str(gobj, tranger, "directory", 0, KW_REQUIRED),
                schema_filename,
                (int)kw_get_int(gobj, tranger, "xpermission", 0, KW_REQUIRED),
                (int)kw_get_int(gobj, tranger, "rpermission", 0, KW_REQUIRED),
                kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                TRUE, // Create file if not exists or overwrite.
                FALSE, // only_read
                jn_schema     // owned
            );

        } while(0);

        if(!jn_schema) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MSG2DB_ERROR,
                "msg",          "%s", "Cannot load TreeDB schema from file.",
                "msg2db_name",  "%s", msg2db_name,
                "schema_file",  "%s", schema_full_path,
                NULL
            );
            return 0;
        }
    } else if(!jn_schema) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MSG2DB_ERROR,
            "msg",          "%s", "TreeDB without schema.",
            "msg2db_name",  "%s", msg2db_name,
            NULL
        );
        return 0;
    }

    /*--------------------------------*
     *      Create desc of cols
     *--------------------------------*/
    if(!topic_cols_desc) {
        topic_cols_desc = _treedb_create_topic_cols_desc();
    } else {
        JSON_INCREF(topic_cols_desc)
    }

    /*
     *  At least 'topics' must be.
     */
    json_t *jn_schema_topics = kw_get_list(gobj, jn_schema, "topics", 0, KW_REQUIRED);
    if(!jn_schema_topics) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MSG2DB_ERROR,
            "msg",          "%s", "No topics found",
            "msg2db_name",  "%s", msg2db_name,
            NULL
        );
        JSON_DECREF(jn_schema)
        return 0;
    }

    /*
     *  The tree is built in tranger, check if already exits
     */
    json_t *msg2db = kwid_get(gobj, tranger, 0, "msg2dbs`%s", msg2db_name);
    if(msg2db) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MSG2DB_ERROR,
            "msg",          "%s", "TreeDB ALREADY opened.",
            "msg2db_name",  "%s", msg2db_name,
            NULL
        );
        JSON_DECREF(jn_schema)
        return 0;
    }

    /*------------------------------*
     *  Open/Create "user" topics
     *------------------------------*/
    int idx;
    json_t *schema_topic;
    json_array_foreach(jn_schema_topics, idx, schema_topic) {
        const char *topic_name = kw_get_str(gobj, schema_topic, "id", kw_get_str(gobj, schema_topic, "topic_name", "", 0), 0);
        if(empty_string(topic_name)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MSG2DB_ERROR,
                "msg",          "%s", "Schema topic without topic_name",
                "msg2db_name",  "%s", msg2db_name,
                "schema_topic", "%j", schema_topic,
                NULL
            );
            continue;
        }
        const char *pkey = kw_get_str(gobj, schema_topic, "pkey", "", 0);
        if(strcmp(pkey, "id")!=0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MSG2DB_ERROR,
                "msg",          "%s", "Schema topic without pkey=id",
                "msg2db_name",  "%s", msg2db_name,
                "schema_topic", "%j", schema_topic,
                NULL
            );
            continue;
        }
        const char *pkey2_col = kw_get_str(gobj, schema_topic, "pkey2", "", 0);
        if(empty_string(pkey2_col)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MSG2DB_ERROR,
                "msg",          "%s", "Schema topic without pkey2",
                "msg2db_name",  "%s", msg2db_name,
                "schema_topic", "%j", schema_topic,
                NULL
            );
            continue;
        }
        const char *topic_version = kw_get_str(gobj, schema_topic, "topic_version", "", 0);
        json_t *jn_topic_var = json_object();
        json_object_set_new(jn_topic_var, "pkey2", json_string(pkey2_col));
        json_object_set_new(jn_topic_var, "topic_version", json_string(topic_version));
        json_t *topic = tranger2_create_topic(
            tranger,    // If the topic exists, then only needs (tranger,name) parameters
            topic_name,
            pkey,
            kw_get_str(gobj, schema_topic, "tkey", "", 0),
            NULL, // jn_topic_ext
            tranger2_str2system_flag(kw_get_str(gobj, schema_topic, "system_flag", "", 0)),
            kwid_new_dict(gobj, schema_topic, 0, "cols"),
            jn_topic_var
        );

        parse_schema_cols(
            topic_cols_desc,
            kwid_new_list(gobj, topic, 0, "cols")
        );
    }

    /*------------------------------*
     *  Create the root of msg2db
     *------------------------------*/
    json_t *msg2dbs = kw_get_dict(gobj, tranger, "msg2dbs", json_object(), KW_CREATE);
    msg2db = kw_get_dict(gobj, msg2dbs, msg2db_name, json_object(), KW_CREATE);
    kw_get_int(gobj, msg2db, "__schema_version__", schema_version, KW_CREATE|KW_WILD_NUMBER);

    /*------------------------------*
     *  Open "system" lists
     *------------------------------*/
    char path[NAME_MAX];

    /*------------------------------*
     *  Open "user" lists
     *------------------------------*/
    json_array_foreach(jn_schema_topics, idx, schema_topic) {
        const char *topic_name = kw_get_str(
            gobj,
            schema_topic,
            "id",
            kw_get_str(gobj, schema_topic, "topic_name", "", 0),
            0
        );
        if(empty_string(topic_name)) {
            continue;
        }

        build_msg2db_index_path(path, sizeof(path), msg2db_name, topic_name, "id");

        kw_get_subdict_value(gobj, msg2db, topic_name, "id", json_object(), KW_CREATE);

        char rt_id[2*NAME_MAX];

        if(gobj) {
            snprintf(rt_id, sizeof(rt_id), "msg2db-%s-%s-%s-%s",
                gobj_gclass_name(gobj),
                gobj_name(gobj),
                msg2db_name,
                topic_name
            );
        } else {
            snprintf(rt_id, sizeof(rt_id), "msg2db-%s-%s",
                msg2db_name,
                topic_name
            );
        }
        json_t *match_cond = json_pack("{s:I}",
            "load_record_callback", (json_int_t)(size_t)load_record_callback
        );

        json_t *jn_extra = json_pack("{s:s, s:s, s:s}",
            "id", path,
            "topic_name", topic_name,
            "msg2db_name", msg2db_name
        );
        if(!tranger2_open_list(
            tranger,
            topic_name,
            match_cond,     // owned
            jn_extra,       // owned
            master?"":rt_id, // rt_id, master goes by mem, don't use rt_id
            !master,        // rt_by_disk
            msg2db_name     // creator
        )) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MSG2DB_ERROR,
                "msg",          "%s", "tranger2_open_list() failed",
                "topic_name",   "%s", topic_name,
                NULL
            );
        }
    }

    JSON_DECREF(jn_schema)
    return msg2db;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int msg2db_close_db(
    json_t *tranger,
    const char *msg2db_name
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*------------------------------*
     *  Close msg2db lists
     *------------------------------*/
    json_t *msg2db = kw_get_subdict_value(gobj, tranger, "msg2dbs", msg2db_name, 0, KW_EXTRACT);

    if(!msg2db) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MSG2DB_ERROR,
            "msg",          "%s", "Msg2db not found",
            "msg2db_name",  "%s", msg2db_name,
            NULL
        );
        gobj_trace_json(gobj, tranger, "Msg2db not found");
        return -1;
    }

    const char *topic_name; json_t *topic_records;
    json_object_foreach(msg2db, topic_name, topic_records) {
        if(strcmp(topic_name, "__schema_version__")==0) {
            continue;
        }

        tranger2_close_all_lists(
            tranger,
            topic_name,
            msg2db_name,    // creator
            ""              // rt_id
        );
    }

    JSON_DECREF(msg2db)
    JSON_DECREF(topic_cols_desc)
    return 0;
}





                    /*------------------------------------*
                     *      Write/Read to/from tranger
                     *------------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_tranger_field_value(
    hgobj gobj,
    const char *topic_name,
    json_t *col,    // not owned
    json_t *record, // not owned
    json_t *value   // not owned
)
{
    const char *field = kw_get_str(gobj, col, "id", 0, KW_REQUIRED);
    if(!field) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MSG2DB_ERROR,
            "msg",          "%s", "Col desc without 'id'",
            "topic_name",   "%s", topic_name,
            "col",          "%j", col,
            NULL
        );
        return -1;
    }
    const char *type = kw_get_str(gobj, col, "type", 0, KW_REQUIRED);
    if(!type) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MSG2DB_ERROR,
            "msg",          "%s", "Col desc without 'type'",
            "topic_name",   "%s", topic_name,
            "col",          "%j", col,
            "field",        "%s", field,
            NULL
        );
        return -1;
    }
    json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);

    /*
     *  Required
     */
    if(kw_has_word(gobj, desc_flag, "required", 0)) {
        if(!value || json_is_null(value)) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MSG2DB_ERROR,
                "msg",          "%s", "Field required",
                "topic_name",   "%s", topic_name,
                "field",        "%s", field,
                "value",        "%j", value,
                NULL
            );
            return -1;
        }
    }

    /*
     *  Null
     */
    if(json_is_null(value)) {
        if(kw_has_word(gobj, desc_flag, "notnull", 0)) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MSG2DB_ERROR,
                "msg",          "%s", "Field cannot be null",
                "topic_name",   "%s", topic_name,
                "field",        "%s", field,
                "value",        "%j", value,
                NULL
            );
            return -1;
        } else {
            json_object_set(record, field, value);
            return 0;
        }
    }

    BOOL is_persistent = kw_has_word(gobj, desc_flag, "persistent", 0)?TRUE:FALSE;
    BOOL wild_conversion = kw_has_word(gobj, desc_flag, "wild", 0)?TRUE:FALSE;
    if(!(is_persistent)) {
        // Not save to tranger
        return 0;
    }

    SWITCHS(type) {
        CASES("list")
        CASES("array")
            if(JSON_TYPEOF(value, JSON_ARRAY)) {
                json_object_set(record, field, value);
            } else {
                json_object_set_new(record, field, json_array());
            }
            break;

        CASES("dict")
        CASES("object")
            if(JSON_TYPEOF(value, JSON_OBJECT)) {
                json_object_set(record, field, value);
            } else {
                json_object_set_new(record, field, json_object());
            }
            break;

        CASES("string")
            if(!value) {
                json_object_set_new(record, field, json_string(""));
            } else if(json_is_string(value)) {
                json_object_set(record, field, value);
            } else if(wild_conversion) {
                char *v = jn2string(value);
                json_object_set_new(record, field, json_string(v));
                GBMEM_FREE(v);
            } else {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MSG2DB_ERROR,
                    "msg",          "%s", "Value must be string",
                    "topic_name",   "%s", topic_name,
                    "col",          "%j", col,
                    "field",        "%s", field,
                    "value",        "%j", value,
                    NULL
                );
                return -1;
            }
            break;

        CASES("integer")
            if(!value) {
                json_object_set_new(record, field, json_integer(0));
            } else if(json_is_integer(value)) {
                json_object_set(record, field, value);
            } else if(wild_conversion) {
                json_int_t v = jn2integer(value);
                json_object_set_new(record, field, json_integer(v));
            } else {
                json_int_t v = jn2integer(value);
                json_object_set_new(record, field, json_integer(v));
                gobj_log_info(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MSG2DB_ERROR,
                    "msg",          "%s", "Value must be integer",
                    "topic_name",   "%s", topic_name,
                    "col",          "%j", col,
                    "field",        "%s", field,
                    "value",        "%j", value,
                    "v",            "%d", (int)v,
                    NULL
                );
            }
            break;
        CASES("real")
            if(!value) {
                json_object_set_new(record, field, json_real(0.0));
            } else if(json_is_real(value)) {
                json_object_set(record, field, value);
            } else if(wild_conversion) {
                double v = jn2real(value);
                json_object_set_new(record, field, json_real(v));
            } else {
                double v = jn2real(value);
                json_object_set_new(record, field, json_real(v));
                gobj_log_info(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_MSG2DB_ERROR,
                    "msg",          "%s", "Value must be real",
                    "topic_name",   "%s", topic_name,
                    "col",          "%j", col,
                    "field",        "%s", field,
                    "value",        "%j", value,
                    "v",            "%f", v,
                    NULL
                );
            }
            break;

        CASES("boolean")
            BOOL v = jn2bool(value);
            json_object_set_new(record, field, v?json_true():json_false());
            break;

        DEFAULTS
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MSG2DB_ERROR,
                "msg",          "%s", "Col type unknown",
                "topic_name",   "%s", topic_name,
                "col",          "%j", col,
                "field",        "%s", field,
                "type",         "%s", type,
                NULL
            );
            return -1;
    } SWITCHS_END;

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_volatil_field_value(
    hgobj gobj,
    const char *type,
    const char *field,
    json_t *record, // not owned
    json_t *value   // not owned
)
{
    SWITCHS(type) {
        CASES("list")
        CASES("array")
            if(JSON_TYPEOF(value, JSON_ARRAY)) {
                json_object_set(record, field, value);
            } else {
                json_object_set_new(record, field, json_array());
            }
            break;

        CASES("dict")
        CASES("object")
            if(JSON_TYPEOF(value, JSON_OBJECT)) {
                json_object_set(record, field, value);
            } else {
                json_object_set_new(record, field, json_object());
            }
            break;

        CASES("string")
            if(!value) {
                json_object_set_new(record, field, json_string(""));
            } else if(json_is_string(value)) {
                json_object_set(record, field, value);
            } else {
                char *v = jn2string(value);
                json_object_set_new(record, field, json_string(v));
                GBMEM_FREE(v);
            }
            break;

        CASES("integer")
            if(!value) {
                json_object_set_new(record, field, json_integer(0));
            } else if(json_is_integer(value)) {
                json_object_set(record, field, value);
            } else {
                json_int_t v = jn2integer(value);
                json_object_set_new(record, field, json_integer(v));
            }
            break;
        CASES("real")
            if(!value) {
                json_object_set_new(record, field, json_real(0.0));
            } else if(json_is_real(value)) {
                json_object_set(record, field, value);
            } else {
                double v = jn2real(value);
                json_object_set_new(record, field, json_real(v));
            }
            break;

        CASES("boolean")
            BOOL v = jn2bool(value);
            json_object_set_new(record, field, v?json_true():json_false());
            break;

        DEFAULTS
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MSG2DB_ERROR,
                "msg",          "%s", "Col type unknown",
                "field",        "%s", field,
                "type",         "%s", type,
                NULL
            );
            return -1;
    } SWITCHS_END;

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _set_volatil_values(
    json_t *tranger,
    const char *topic_name,
    json_t *record,  // not owned
    json_t *kw // not owned
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name);
    if(!cols) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MSG2DB_ERROR,
            "msg",          "%s", "Topic without cols",
            "topic_name",   "%s", topic_name,
            NULL
        );
        return 0;
    }

    const char *field; json_t *col;
    json_object_foreach(cols, field, col) {
        json_t *default_ = kw_get_dict_value(gobj, col, "default", 0, 0);
        json_t *value = kw_get_dict_value(gobj,
            kw,
            field,
            default_,
            0
        );

        const char *field_ = kw_get_str(gobj, col, "id", 0, KW_REQUIRED);
        if(!field_) {
            continue;
        }
        const char *type = kw_get_str(gobj, col, "type", 0, KW_REQUIRED);
        if(!type) {
            continue;
        }
        json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        BOOL is_persistent = kw_has_word(gobj, desc_flag, "persistent", 0)?TRUE:FALSE;
        BOOL is_hook = kw_has_word(gobj, desc_flag, "hook", 0)?TRUE:FALSE;
        BOOL is_fkey = kw_has_word(gobj, desc_flag, "fkey", 0)?TRUE:FALSE;
        if((is_persistent || is_hook || is_fkey)) {
            continue;
        }

        set_volatil_field_value(
            gobj,
            type,
            field,
            record, // not owned
            value   // not owned
        );
    }

    JSON_DECREF(cols)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *record2tranger(
    json_t *tranger,
    const char *topic_name,
    json_t *kw,  // not owned
    const char *options // "permissive"
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name);
    if(!cols) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MSG2DB_ERROR,
            "msg",          "%s", "Topic without cols",
            "topic_name",   "%s", topic_name,
            NULL
        );
        return 0;
    }
    json_t *new_record = json_object();

    const char *field; json_t *col;
    json_object_foreach(cols, field, col) {
        json_t *value = kw_get_dict_value(gobj, kw, field, 0, 0);
        if(set_tranger_field_value(
                gobj,
                topic_name,
                col,
                new_record,
                value
            )<0) {
            // Error already logged
            JSON_DECREF(new_record)
            JSON_DECREF(cols)
            return 0;
        }
    }

    if(options && strstr(options, "permissive")) {
        json_object_update_missing(new_record, kw);
    }
    json_object_del(new_record, "__md_msg2db__");

    JSON_DECREF(cols)
    return new_record;
}

/***************************************************************************
 *  Return json object with record metadata
 ***************************************************************************/
PRIVATE json_t *md2json(
    const char *msg2db_name,
    const char *topic_name,
    md2_record_ex_t *md_record
)
{
    json_t *jn_md = json_object();
    json_object_set_new(
        jn_md,
        "msg2db_name",
        json_string(msg2db_name)
    );
    json_object_set_new(
        jn_md,
        "topic_name",
        json_string(topic_name)
    );
    json_object_set_new(jn_md, "t", json_integer((json_int_t)md_record->__t__));
    json_object_set_new(jn_md, "tm", json_integer((json_int_t)md_record->__tm__));
    json_object_set_new(jn_md, "user_flag", json_integer((json_int_t)md_record->user_flag));
    json_object_set_new(jn_md, "__pure_node__", json_true());

    return jn_md;
}

/***************************************************************************
 *  When record is loaded from disk then create the node
 *  when is loaded from memory then notify to subscribers
 ***************************************************************************/
PRIVATE int load_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list, // iterator or rt_list/rt_disk id, don't own
    json_int_t rowid,   // in a rt_mem will be the relative rowid, in rt_disk the absolute rowid
    md2_record_ex_t *md_record,
    json_t *jn_record // must be owned, can be null if only_md
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    system_flag2_t system_flag = md_record->system_flag;
    if(system_flag & sf_loading_from_disk) {
        /*---------------------------------*
         *  Loading from disk
         *---------------------------------*/
        /*-------------------------------*
         *      Get indexx
         *-------------------------------*/
        const char *msg2db_name = kw_get_str(gobj, list, "msg2db_name", 0, KW_REQUIRED);
        const char *topic_name = kw_get_str(gobj, list, "topic_name", 0, KW_REQUIRED);

        char path[NAME_MAX];
        build_msg2db_index_path(path, sizeof(path), msg2db_name, topic_name, "id");
        json_t *indexx = kw_get_dict(gobj,
            tranger,
            path,
            0,
            KW_REQUIRED
        );
        if(!indexx) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MSG2DB_ERROR,
                "msg",          "%s", "Msg2Db Topic indexx NOT FOUND",
                "path",         "%s", path,
                "topic_name",   "%s", topic_name,
                NULL
            );
            JSON_DECREF(jn_record);
            return 0;  // Timeranger: does not load the record, it's mine.
        }

        /*-------------------------------*
         *  Append new node
         *-------------------------------*/
        /*---------------------------------------------*
         *  Build metadata, loading node from tranger
         *---------------------------------------------*/
        json_t *jn_record_md = md2json(
            msg2db_name,
            topic_name,
            md_record
        );
        json_object_set_new(jn_record, "__md_msg2db__", jn_record_md);
        const char *pkey2_col = kw_get_str(gobj, topic, "pkey2", 0, 0);
        json_object_set_new(jn_record_md, "pkey2", json_string(pkey2_col));

        const char *pkey2_value = kw_get_str(gobj, jn_record, pkey2_col, 0, 0);
        if(empty_string(pkey2_value)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MSG2DB_ERROR,
                "msg",          "%s", "Field 'pkey2' required",
                "path",         "%s", path,
                "topic_name",   "%s", topic_name,
                "pkey2",        "%s", pkey2_col,
                "jn_record",    "%j", jn_record,
                NULL
            );
            JSON_DECREF(jn_record);
            return 0;  // Timeranger: does not load the record, it's mine.
        }

        /*--------------------------------------------*
         *  Set volatil data
        *--------------------------------------------*/
        _set_volatil_values(
            tranger,
            topic_name,
            jn_record,  // not owned
            jn_record // not owned
        );

        /*-------------------------------*
         *  Write node
         *-------------------------------*/
        JSON_INCREF(jn_record)
        kw_set_subdict_value(
            gobj,
            indexx,
            key,
            pkey2_value,
            jn_record
        );
    } else {
        /*---------------------------------*
         *      Working in memory
         *---------------------------------*/
    }

    JSON_DECREF(jn_record)
    return 0;  // Timeranger: does not load the record, it's mine.
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *msg2db_append_message( // Return is NOT YOURS
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    json_t *kw,    // owned
    const char *options // "permissive"
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    /*-------------------------------*
     *      Get indexx
     *-------------------------------*/
    char path[NAME_MAX];
    build_msg2db_index_path(path, sizeof(path), msg2db_name, topic_name, "id");
    json_t *indexx = kw_get_dict(gobj,
        tranger,
        path,
        0,
        KW_REQUIRED
    );
    if(!indexx) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MSG2DB_ERROR,
            "msg",          "%s", "Msg2Db Topic indexx NOT FOUND",
            "path",         "%s", path,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    /*-------------------------------*
     *  Get the id, it's mandatory
     *-------------------------------*/
    char uuid[RECORD_KEY_VALUE_MAX+1];
    const char *id = kw_get_str(gobj, kw, "id", 0, 0);
    if(empty_string(id)) {
        json_t *id_col_flag = kwid_get(gobj,
            tranger,
            KW_VERBOSE,
            "topics`%s`cols`id`flag",
                topic_name
        );
        if(kw_has_word(gobj, id_col_flag, "uuid", 0)) {
            create_uuid(uuid, sizeof(uuid));
            id = uuid;
            json_object_set_new(kw, "id", json_string(id));
        } else {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MSG2DB_ERROR,
                "msg",          "%s", "Field 'id' required",
                "path",         "%s", path,
                "topic_name",   "%s", topic_name,
                "kw",           "%j", kw,
                NULL
            );
            JSON_DECREF(kw)
            return 0;
        }
    }

    /*-----------------------------------*
     *  Get the pkey2, it's mandatory
     *-----------------------------------*/
    json_t *topic = tranger2_topic(tranger, topic_name);
    const char *pkey2_col = kw_get_str(gobj, topic, "pkey2", 0, 0);
    if(!kw_has_key(kw, pkey2_col)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MSG2DB_ERROR,
            "msg",          "%s", "Field 'pkey2' required",
            "path",         "%s", path,
            "topic_name",   "%s", topic_name,
            "pkey2",        "%s", pkey2_col,
            "kw",           "%j", kw,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }
    const char *pkey2_value = kw_get_str(gobj, kw, pkey2_col, 0, 0);

    /*----------------------------------------*
     *  Create the tranger record to create
     *----------------------------------------*/
    json_t *record = record2tranger(tranger, topic_name, kw, options);
    if(!record) {
        // Error already logged
        JSON_DECREF(kw)
        return 0;
    }

    /*-------------------------------*
     *  Write to tranger
     *-------------------------------*/
    md2_record_ex_t md_record;
    JSON_INCREF(record)
    int ret = tranger2_append_record(
        tranger,
        topic_name,
        0, // __t__,         // if 0 then the time will be set by TimeRanger with now time
        0, // user_flag,
        &md_record, // md_record,
        record // owned
    );
    if(ret < 0) {
        // Error already logged
        JSON_DECREF(kw)
        JSON_DECREF(record)
        return 0;
    }

    /*--------------------------------------------*
     *  Set volatil data
     *--------------------------------------------*/
    _set_volatil_values(
        tranger,
        topic_name,
        record,  // not owned
        kw // not owned
    );

    /*--------------------------------------------*
     *  Build metadata, creating node in memory
     *--------------------------------------------*/
    json_t *jn_record_md = md2json(
        msg2db_name,
        topic_name,
        &md_record
    );
    json_object_set_new(jn_record_md, "pkey2", json_string(pkey2_col));
    json_object_set_new(record, "__md_msg2db__", jn_record_md);

    /*-------------------------------*
     *  Write node
     *-------------------------------*/
    kw_set_subdict_value(
        gobj,
        indexx,
        id,
        pkey2_value,
        record
    );

    JSON_DECREF(kw)
    return record;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *msg2db_list_messages( // Return MUST be decref
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    json_t *jn_ids,     // owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    /*-------------------------------*
     *      Get indexx
     *-------------------------------*/
    char path[NAME_MAX];
    build_msg2db_index_path(path, sizeof(path), msg2db_name, topic_name, "id");
    json_t *indexx = kw_get_dict(gobj,
        tranger,
        path,
        0,
        0
    );

    if(!indexx) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MSG2DB_ERROR,
            "msg",          "%s", "Msg2Db Topic indexx NOT FOUND",
            "path",         "%s", path,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_ids)
        JSON_DECREF(jn_filter)
        return 0;
    }

    /*-------------------------------*
     *      Read
     *-------------------------------*/
    if(!match_fn) {
        match_fn = kw_match_simple;
    }

    json_t *list = json_array();

    if(json_is_object(indexx)) {
        const char *id; json_t *pkey2_dict;
        json_object_foreach(indexx, id, pkey2_dict) {
            if(!kwid_match_id(gobj, jn_ids, id)) {
                continue;
            }
            const char *pkey2; json_t *node;
            json_object_foreach(pkey2_dict, pkey2, node) {
                JSON_INCREF(jn_filter)
                if(match_fn(node, jn_filter)) {
                    json_array_append(list, node);
                }
            }
        }
    } else  {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "kw MUST BE a json object",
            NULL
        );
        JSON_DECREF(list)
    }

    JSON_DECREF(jn_ids)
    JSON_DECREF(jn_filter)

    return list;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *msg2db_get_message( // Return is NOT YOURS
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    const char *id,
    const char *id2
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*-------------------------------*
     *      Get indexx
     *-------------------------------*/
    char path[NAME_MAX];
    build_msg2db_index_path(path, sizeof(path), msg2db_name, topic_name, "id");
    json_t *indexx = kw_get_dict(gobj,
        tranger,
        path,
        0,
        0
    );

    if(!indexx) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MSG2DB_ERROR,
            "msg",          "%s", "Msg2Db Topic indexx NOT FOUND",
            "path",         "%s", path,
            "topic_name",   "%s", topic_name,
            NULL
        );
        return 0;
    }

    /*-------------------------------*
     *      Read
     *-------------------------------*/
    if(json_is_object(indexx)) {
        json_t *record;
        if(!empty_string(id2)) {
            record = kw_get_subdict_value(gobj, indexx, id, id2, 0, 0);
        } else {
            record = kw_get_dict_value(gobj, indexx, id, 0, 0);
        }
        return record;
    } else  {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "kw MUST BE a json object",
            NULL
        );
    }

    return 0;
}
