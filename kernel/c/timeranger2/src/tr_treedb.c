/***********************************************************************
 *          TR_TREEDB.C
 *
 *          Tree Database with TimeRanger
 *          Hierarchical tree of objects (nodes, records, messages)
 *          linked by child-parent relation.
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include <kwid.h>
#include <helpers.h>
#include "timeranger2.h"
#include "tr_treedb.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE json_t *_md2json(
    const char *treedb_name,
    const char *topic_name,
    md2_record_t *md_record,
    json_int_t rowid
);

PRIVATE int load_all_links(
    hgobj gobj,
    json_t *tranger,
    const char *treedb_name
);
PRIVATE int load_id_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,       // iterator or rt_mem/rt_disk, don't own
    json_int_t rowid,   // in a rt_mem will be the relative rowid, in rt_disk the absolute rowid
    md2_record_t *md_record,
    json_t *jn_record  // must be owned
);
PRIVATE int load_pkey2_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,       // iterator or rt_mem/rt_disk, don't own
    json_int_t rowid,   // in a rt_mem will be the relative rowid, in rt_disk the absolute rowid
    md2_record_t *md_record,
    json_t *jn_record  // must be owned
);

PRIVATE json_t *get_fkey_refs(
    json_t *field_data // NOT owned
);
PRIVATE int _unlink_nodes(
    hgobj gobj,
    json_t *tranger,
    const char *hook_name,
    json_t *parent_node,    // NOT owned
    json_t *child_node      // NOT owned
);

PRIVATE json_t * treedb_get_activated_snap_tag(
    hgobj gobj,
    json_t *tranger,
    const char *treedb_name,
    uint32_t *user_flag
);

PRIVATE json_int_t json_size(json_t *value);

PRIVATE json_t *apply_parent_ref_options(
    hgobj gobj,
    json_t *refs,  // NOT owned
    json_t *jn_options // NOT owned
);
PRIVATE json_t *apply_child_list_options(
    hgobj gobj,
    json_t *child_list,  // NOT owned
    json_t *jn_options // NOT owned
);
PRIVATE json_t *_list_childs(
    hgobj gobj,
    json_t *tranger,
    const char *hook,
    json_t *node       // NOT owned, pure node
);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE json_t *topic_cols_desc = 0;
PRIVATE BOOL treedb_trace = 0;

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int current_snap_tag(json_t *tranger, const char *treedb_name)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    char path[NAME_MAX];
    snprintf(path, sizeof(path), "treedbs_snaps`%s`activated_snap_tag", treedb_name);

    return (int)kw_get_int(gobj, tranger, path, 0, KW_REQUIRED);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_topic_pkey2s( // Return list with pkey2s
    json_t *tranger,
    const char *topic_name
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *topic_desc = kw_get_subdict_value(gobj, tranger, "topics", topic_name, 0, 0);
    json_t *list = kw_get_list(gobj, topic_desc, "pkey2s", 0, 0);
    return json_incref(list);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_topic_pkey2s_filter(
    json_t *tranger,
    const char *topic_name,
    json_t *node, // NO owned
    const char *id
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *jn_filter = json_object();
    json_object_set_new(jn_filter, "id", json_string(id));

    json_t *iter_pkey2s = treedb_topic_pkey2s(tranger, topic_name);
    int idx; json_t *jn_pkey2_name;
    json_array_foreach(iter_pkey2s, idx, jn_pkey2_name) {
        const char *pkey2_name = json_string_value(jn_pkey2_name);
        if(empty_string(pkey2_name)) {
            continue;
        }
        json_t *jn_value = kw_get_dict_value(gobj, node, pkey2_name, 0, 0);
        if(jn_value) {
            const char *value = json_string_value(jn_value);
            if(!empty_string(value)) {
                json_object_set(jn_filter, pkey2_name, jn_value);
            }
        }
    }
    json_decref(iter_pkey2s);

    return jn_filter;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char *build_id_index_path(
    char *bf,
    int bfsize,
    const char *treedb_name,
    const char *topic_name
)
{
    snprintf(bf, bfsize, "treedbs`%s`%s`id", treedb_name, topic_name);
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char *build_pkey_index_path(
    char *bf,
    int bfsize,
    const char *treedb_name,
    const char *topic_name,
    const char *pkey2_name
)
{
    snprintf(bf, bfsize, "treedbs`%s`%s`%s", treedb_name, topic_name, pkey2_name);
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL treedb_is_treedbs_topic(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
)
{
    if(!treedb_get_id_index(tranger, treedb_name, topic_name)) {
        return FALSE;
    }
    return TRUE;
}

/***************************************************************************
 * PUBLIC to use in tests
 ***************************************************************************/
PUBLIC json_t *treedb_get_id_index( // WARNING Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    char path[NAME_MAX];
    build_id_index_path(path, sizeof(path), treedb_name, topic_name);
    json_t *indexx = kw_get_dict(
        gobj,
        tranger,
        path,
        0,
        0
    );
    return indexx;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *treedb_get_pkey2_index( // Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *pkey2_name
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    // De momento sin claves combinadas, el nombre de la pkey es el nombre del field
    char path[NAME_MAX];
    build_pkey_index_path(path, sizeof(path), treedb_name, topic_name, pkey2_name);
    json_t *indexy = kw_get_dict(
        gobj,
        tranger,
        path,
        0,
        0
    );
    return indexy;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *get_key2_value(
    json_t *tranger,
    const char *topic_name,
    const char *pkey2_name,
    json_t *kw
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    // combined no, de momento solo claves simples
    const char *pkey2_value = kw_get_str(gobj, kw, pkey2_name, "", 0);
    return pkey2_value;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *exist_primary_node(
    json_t *indexx,
    const char *key
)
{
    // HACK tranger keys have a maximum length
    char key_[RECORD_KEY_VALUE_MAX];
    snprintf(key_, sizeof(key_), "%s", key);
    return json_object_get(indexx, key_);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int add_primary_node(
    json_t *indexx,
    const char *key,
    json_t *node // incref
)
{
    // HACK tranger keys have a maximum length
    char key_[RECORD_KEY_VALUE_MAX];
    snprintf(key_, sizeof(key_), "%s", key);
    return json_object_set(indexx, key_, node);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int delete_primary_node(
    json_t *indexx,
    const char *key
)
{
    // HACK tranger keys have a maximum length
    char key_[RECORD_KEY_VALUE_MAX];
    snprintf(key_, sizeof(key_), "%s", key);
    return json_object_del(indexx, key_);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *exist_secondary_node(
    json_t *indexy,
    const char *key,
    const char *key2
)
{
    // HACK tranger keys have a maximum length
    char key_[RECORD_KEY_VALUE_MAX];
    snprintf(key_, sizeof(key_), "%s", key);

    return kw_get_subdict_value(
        0,
        indexy,
        key_,
        key2,
        0,
        0
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int add_secondary_node(
    json_t *indexy,
    const char *key,
    const char *key2,
    json_t *node // incref
)
{
    // HACK tranger keys have a maximum length
    char key_[RECORD_KEY_VALUE_MAX];
    snprintf(key_, sizeof(key_), "%s", key);

    JSON_INCREF(node)
    return kw_set_subdict_value(
        0,
        indexy,
        key_,
        key2,
        node
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int delete_secondary_node(
    json_t *indexy,
    const char *key,
    const char *key2
)
{
    // HACK tranger keys have a maximum length
    char key_[RECORD_KEY_VALUE_MAX];
    snprintf(key_, sizeof(key_), "%s", key);

    json_t *node = kw_get_subdict_value(
        0,
        indexy,
        key_,
        key2,
        0,
        KW_EXTRACT
    );
    if(!node) {
        return -1;
    }
    json_decref(node);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *_treedb_create_topic_cols_desc(void)
{
    /*
     *  WARNING any change in this must be reflected in treedb_schema_treedb (c-core)
     */
    json_t *topic_cols_desc = json_array();
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i, s:s, s:[s,s]}",
            "id", "id",
            "header", "Id",
            "fillspace", 10,
            "type",
                "string",
            "flag",
                "required",
                "persistent"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i, s:s, s:[s,s,s]}",
            "id", "header",
            "header", "Header",
            "fillspace", 10,
            "type",
                "string",
            "flag",
                "required",
                "persistent",
                "writable"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i, s:s, s:[s,s]}",
            "id", "fillspace",
            "header", "Fillspace",
            "fillspace", 3,
            "type",
                "integer",
            "flag",
                "persistent",
                "writable"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i, s:s, s:[s,s,s,s,s,s,s,s,s,s], s:[s,s,s,s,s]}",
            "id", "type",
            "header", "Type",
            "fillspace", 5,
            "type", "string",
            "enum",
                "string",       // Real types
                "integer",
                "object",
                "dict",
                "array",
                "list",
                "real",
                "boolean",
                "blob",
                "number",
            "flag",
                "enum",
                "required",
                "persistent",
                "notnull",
                "writable"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i, s:s, s:[s,s]}",
            "id", "placeholder",
            "header", "Placeholder",
            "fillspace", 10,
            "type",
                "string",
            "flag",
                "persistent",
                "writable"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i, s:s, s:[s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s], s:[s,s,s]}",
            "id", "flag",
            "header", "Flag",
            "fillspace", 14,
            "type", "array",
            "enum",
                "",
                "persistent",   // Field attributes
                "required",
                "notnull",
                "wild",
                "inherit",
                "readable",
                "writable",
                "stats",
                "rstats",
                "pstats",

                "fkey",         // Field types
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

            "flag",
                "enum",
                "persistent",
                "writable"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i, s:s, s:[s,s]}",
            "id", "hook",
            "header", "Hook",
            "fillspace", 8,
            "type",
                "blob",
            "flag",
                "persistent",
                "writable"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i, s:s, s:[s,s]}",
            "id", "pkey2s",
            "header", "Secondary Keys",
            "fillspace", 8,
            "type",
                "blob",
            "flag",
                "persistent",
                "writable"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i, s:s, s:[s,s]}",
            "id", "default",
            "header", "Default",
            "fillspace", 8,
            "type",
                "blob",
            "flag",
                "persistent",
                "writable"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i, s:s, s:[s,s]}",
            "id", "description",
            "header", "Description",
            "fillspace", 8,
            "type",
                "string",
            "flag",
                "persistent",
                "writable"
        )
    );
    json_array_append_new(
        topic_cols_desc,
        json_pack("{s:s, s:s, s:i, s:s, s:[s,s]}",
            "id", "properties",
            "header", "Properties",
            "fillspace", 8,
            "type",
                "blob",
            "flag",
                "persistent",
                "writable"
        )
    );

    return topic_cols_desc;
}

/***************************************************************************
    Open tree db (Remember previously open tranger_startup())

    Tree of nodes of data. Node's data contains attributes, in json.

    The nodes are linked with relation parent->child
    in pure-child or multi-parent mode.
    The tree in json too.

    HACK Conventions:
        1) the pkey of all topics must be "id".
        2) the "id" field (primary key) MUST be a string.

    Option "persistent"
        Try to load the schema from file
        File has precedence.
        Once saved,
            if you want to change the schema
            then you must change the schema_version and topic_version

    Return a dict inside of tranger with path "treedbs`{treedb_name}" DO NOT use it directly

 ***************************************************************************/
PUBLIC json_t *treedb_open_db( // WARNING Return IS NOT YOURS!
    json_t *tranger,
    const char *treedb_name_,
    json_t *jn_schema,  // owned
    const char *options // "persistent"
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    char treedb_name[NAME_MAX];
    snprintf(treedb_name, sizeof(treedb_name), "%s", treedb_name_);
    char *p = strstr(treedb_name, ".treedb_schema.json");
    if(p) {
        *p = 0;
    }

    BOOL master = kw_get_bool(gobj, tranger, "master", 0, KW_REQUIRED);

    if(empty_string(treedb_name)) {
        gobj_log_error(gobj,0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "treedb_name NULL",
            NULL
        );
        JSON_DECREF(jn_schema);
        return 0;
    }

    char schema_filename[NAME_MAX + sizeof(".treedb_schema.json")];
    snprintf(schema_filename, sizeof(schema_filename), "%s.treedb_schema.json",
        treedb_name
    );

    char schema_full_path[NAME_MAX*2];
    snprintf(schema_full_path, sizeof(schema_full_path), "%s/%s",
        kw_get_str(gobj, tranger, "directory", "", KW_REQUIRED),
        schema_filename
    );

    int schema_new_version = (int)kw_get_int(gobj, jn_schema, "schema_version", 0, KW_WILD_NUMBER);
    int schema_version = schema_new_version;

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
                int schema_old_version = (int)kw_get_int(
                    gobj,
                    old_jn_schema,
                    "schema_version",
                    -1,
                    KW_WILD_NUMBER
                );
                if(!master || schema_new_version <= schema_old_version) {
                    JSON_DECREF(jn_schema);
                    jn_schema = old_jn_schema;
                    schema_version = schema_old_version;
                    break; // Nothing to do
                } else {
                    recreating = TRUE;
                    schema_version = schema_new_version;
                    JSON_DECREF(old_jn_schema);
                }
            }
            gobj_log_info(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_INFO,
                "msg",              "%s", recreating?"Re-Creating TreeDB schema file":"Creating TreeDB schema file",
                "treedb_name",      "%s", treedb_name,
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
                (log_opt_t)kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
                TRUE, // Create file if not exists or overwrite.
                FALSE, // only_read
                jn_schema     // owned
            );

        } while(0);

        if(!jn_schema) {
            gobj_log_error(gobj,0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Cannot load TreeDB schema from file.",
                "treedb_name",  "%s", treedb_name,
                "schema_file",  "%s", schema_full_path,
                NULL
            );
            return 0;
        }
    } else if(!jn_schema) {
        gobj_log_error(gobj,0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDB without schema.",
            "treedb_name",  "%s", treedb_name,
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
        JSON_INCREF(topic_cols_desc);
    }

    /*
     *  At least 'topics' must be.
     */
    //json_t *jn_schema_topics = kw_get_list(jn_schema, "topics", 0, KW_REQUIRED);
    json_t *jn_schema_topics = kwid_new_list(gobj, jn_schema, 0, "topics");
    if(!jn_schema_topics) {
        gobj_log_error(gobj,0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "No topics found",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
        JSON_DECREF(jn_schema);
        return 0;
    }

    /*
     *  The tree is built in tranger, check if already exits
     */
    json_t *treedb = kwid_get(gobj, tranger, 0, "treedbs`%s", treedb_name);
    if(treedb) {
        gobj_log_error(gobj,0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDB ALREADY opened.",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
        JSON_DECREF(jn_schema_topics);
        JSON_DECREF(jn_schema);
        return 0;
    }

    /*------------------------------*
     *  Create the root of treedb
     *------------------------------*/
    json_t *treedbs = kw_get_dict(gobj, tranger, "treedbs", json_object(), KW_CREATE);
    treedb = kw_get_dict(gobj, treedbs, treedb_name, json_object(), KW_CREATE);
    kw_get_int(gobj, treedb, "__schema_version__", schema_version, KW_CREATE|KW_WILD_NUMBER);

    /*-------------------------------*
     *  Create "system" topics:
     *      __snaps__
     *-------------------------------*/
    char *snaps_topic_name = "__snaps__";
    int snaps_topic_version = 3;
    json_t *jn_snaps_topic_var = json_object();
    json_object_set_new(jn_snaps_topic_var, "topic_version", json_integer(snaps_topic_version));

    json_t *tag_snaps_schema = json_pack(
        "{s:{s:s, s:s, s:i, s:s, s:[s,s,s]},"       /* id */
            "s:{s:s, s:s, s:i, s:s, s:[s,s]},"      /* name */
            "s:{s:s, s:s, s:i, s:s, s:[s,s]},"      /* date (string!!!) */
            "s:{s:s, s:s, s:i, s:s, s:[s,s]},"      /* active */
            "s:{s:s, s:s, s:i, s:s, s:[s]}}",       /* description */
        "id",
            "id", "id",
            "header", "Id",
            "fillspace", 8,
            "type", "string",
            "flag",
                "persistent","required","rowid",

        "name",
            "id", "name",
            "header", "Name",
            "fillspace", 28,
            "type", "string",
            "flag",
                "persistent", "required",
        "date",
            "id", "date",
            "header", "Date",
            "fillspace", 28,
            "type", "string",
            "flag",
                "persistent", "required",
        "active",
            "id", "active",
            "header", "Active",
            "fillspace", 8,
            "type", "boolean",
            "flag",
                "persistent", "required",
        "description",
            "id", "description",
            "header", "Description",
            "fillspace", 40,
            "type", "string",
            "flag",
                "persistent"
    );
    if(!tag_snaps_schema) {
        gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Bad __snaps__ json schema",
            NULL
        );
    }

    json_t *snaps_topic = tranger2_create_topic( // System topic
        tranger,    // If topic exists then only needs (tranger,name) parameters
        snaps_topic_name,
        "id",
        "",
        NULL,
        sf_string_key,
        tag_snaps_schema,
        jn_snaps_topic_var
    );

    if(parse_schema_cols(
        topic_cols_desc,
        kwid_new_list(gobj, snaps_topic, KW_VERBOSE, "cols")
    )<0) {
        gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "parse_schema_cols failed",
            "schema",       "%s", "__snaps__",
            NULL
        );
    }

    /*-------------------------------*
     *  Create "system" topics:
     *      __graphs__
     *-------------------------------*/
    char *graphs_topic_name = "__graphs__";
    int graphs_topic_version = 11;
    json_t *jn_graphs_topic_var = json_object();
    json_object_set_new(jn_graphs_topic_var, "topic_version", json_integer(graphs_topic_version));

    json_t *tag_graphs_schema = json_pack(
        "{s:{s:s, s:s, s:i, s:s, s:[s,s,s]},"           /* id */
            "s:{s:s, s:s, s:i, s:s, s:[s,s,s]},"        /* topic */
            "s:{s:s, s:s, s:i, s:s, s:[s,s]},"          /* active */
            "s:{s:s, s:s, s:i, s:s, s:[s,s,s,s,s]},"    /* time */
            "s:{s:s, s:s, s:i, s:s, s:[s,s]}}",         /* properties */
        "id",
            "id", "id",
            "header", "RowId",
            "fillspace", 9,
            "type", "string",
            "flag",
                "persistent","required","rowid",

        "topic",
            "id", "topic",
            "header", "Topic",
            "fillspace", 28,
            "type", "string",
            "flag",
                "persistent", "required", "writable",
        "active",
            "id", "active",
            "header", "Active",
            "fillspace", 8,
            "type", "boolean",
            "flag",
                "persistent", "writable",
        "time",
            "id", "time",
            "header", "Time",
            "fillspace", 28,
            "type", "integer",
            "flag",
                "persistent", "required", "time", "now", "writable",
        "properties",
            "id", "properties",
            "header", "Properties",
            "fillspace", 40,
            "type", "blob",
            "flag",
                "persistent", "writable"
    );
    if(!tag_graphs_schema) {
        gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Bad __graphs__ json schema",
            NULL
        );
    }

    json_t *graphs_topic = tranger2_create_topic( // System topic
        tranger,    // If topic exists then only needs (tranger,name) parameters
        graphs_topic_name,
        "id",
        "",
        NULL,
        sf_string_key,
        tag_graphs_schema,
        jn_graphs_topic_var
    );

    if(parse_schema_cols(
        topic_cols_desc,
        kwid_new_list(gobj, graphs_topic, KW_VERBOSE, "cols")
    )<0) {
        gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "parse_schema_cols failed",
            "schema",       "%s", "__graphs__",
            NULL
        );
    }

    /*------------------------------*
     *  Open "system" lists:
     *      __snaps__
     *------------------------------*/
    if(1) {
        char path[NAME_MAX];
        build_id_index_path(path, sizeof(path), treedb_name, snaps_topic_name);
        kw_get_dict_value(gobj, tranger, path, json_object(), KW_CREATE);

        json_t *jn_filter = json_pack("{s:b, s:s, s:b, s:I}",
            "backward", 1,
            "rkey", "",
            "rt_by_mem", 1,
            "load_record_callback", (json_int_t)(size_t)load_id_callback
        );
        json_t *jn_extra = json_pack("{s:s, s:s, s:{}}",
            "id", path,
            "treedb_name", treedb_name,
            "deleted_records"
        );
        if(!tranger2_open_list( // OLD tranger_open_list
            tranger,
            snaps_topic_name,
            jn_filter,  // owned
            jn_extra    // owned
        )) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "tranger2_open_list() failed",
                "treedb_name",  "%s", treedb_name,
                "topic_name",   "%s", "__snaps__",
                NULL
            );
        }
    }

    /*------------------------------*
     *  Open "system" lists:
     *      __graphs__
     *------------------------------*/
    if(1) {
        char path[NAME_MAX];
        build_id_index_path(path, sizeof(path), treedb_name, graphs_topic_name);
        kw_get_dict_value(gobj, tranger, path, json_object(), KW_CREATE);

        json_t *jn_filter = json_pack("{s:b, s:s, s:b, s:I}",
            "backward", 1,
            "rkey", "",
            "rt_by_mem", 1,
            "load_record_callback", (json_int_t)(size_t)load_id_callback
        );

        json_t *jn_extra = json_pack("{s:s, s:s, s:{}}",
            "id", path,
            "treedb_name", treedb_name,
            "deleted_records"
        );
        if(!tranger2_open_list( // OLD tranger_open_list
            tranger,
            graphs_topic_name,
            jn_filter,  // owned
            jn_extra    // owned
        )) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function", "%s", __FUNCTION__,
                "msgset", "%s", MSGSET_TREEDB_ERROR,
                "msg", "%s", "tranger2_open_list() failed",
                "treedb_name", "%s", treedb_name,
                "topic_name", "%s", "__graphs__",
                NULL
            );
        }
    }

    /*------------------------------*
     *  Get snap tab
     *------------------------------*/
    json_t *treedb_snaps = kw_get_dict(gobj, tranger, "treedbs_snaps", json_object(), KW_CREATE);

    uint32_t snap_tag = 0;
    treedb_get_activated_snap_tag(
        gobj,
        tranger,
        treedb_name,
        &snap_tag
    );
    if(snap_tag) {
        char temp[80];
        snprintf(temp, sizeof(temp), "loading snap_tag %d", snap_tag);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", temp,
            "snap_tag",     "%d", (int)snap_tag,
            NULL
        );
    }

    // Save current snap tag
    json_t *treedb_snap = kw_get_dict(gobj, treedb_snaps, treedb_name, json_object(), KW_CREATE);
    json_object_set_new(treedb_snap, "activated_snap_tag", json_integer(snap_tag));

    /*------------------------------*
     *  Create "user" topics
     *------------------------------*/
    int idx;
    json_t *schema_topic;
    json_array_foreach(jn_schema_topics, idx, schema_topic) {
        const char *topic_name = kw_get_str(gobj, schema_topic, "topic_name", "", 0);
        if(empty_string(topic_name)) {
            topic_name = kw_get_str(gobj, schema_topic, "id", "", 0);
            if(empty_string(topic_name)) {
                gobj_log_error(gobj,0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Schema topic without id/topic_name",
                    "treedb_name",  "%s", treedb_name,
                    "schema_topic", "%j", schema_topic,
                    NULL
                );
                continue;
            }
        }
        const char *pkey = kw_get_str(gobj, schema_topic, "pkey", "", 0);
        if(strcmp(pkey, "id")!=0) {
            gobj_log_error(gobj,0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Schema topic without pkey=id",
                "treedb_name",  "%s", treedb_name,
                "schema_topic", "%j", schema_topic,
                NULL
            );
            continue;
        }
        const char *system_flag = kw_get_str(gobj, schema_topic, "system_flag", "", 0);
        if(strcmp(system_flag, "sf_string_key")!=0) {
            gobj_log_error(gobj,0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Schema topic without system_flag=sf_string_key",
                "treedb_name",  "%s", treedb_name,
                "schema_topic", "%j", schema_topic,
                NULL
            );
            continue;
        }
        int topic_version = (int)kw_get_int(gobj, schema_topic, "topic_version", 0, KW_WILD_NUMBER);
        const char *topic_tkey = kw_get_str(gobj, schema_topic, "tkey", "", 0);
        json_t *pkey2s = kw_get_dict_value(gobj, schema_topic, "pkey2s", 0, 0);

        treedb_create_topic(
            tranger,
            treedb_name,
            topic_name,
            topic_version,
            topic_tkey,
            json_incref(pkey2s),
            kwid_new_dict(gobj, schema_topic, KW_VERBOSE, "cols"), // owned
            snap_tag,
            FALSE // create_schema
        );
    }

    /*------------------------------*
     *  Parse hooks
     *------------------------------*/
    parse_hooks(tranger);

    /*------------------------------*
     *  Load links
     *------------------------------*/
    load_all_links(gobj, tranger, treedb_name);

    JSON_DECREF(jn_schema_topics)
    JSON_DECREF(jn_schema)

    return treedb;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_set_callback(
    json_t *tranger,
    const char *treedb_name,
    treedb_callback_t treedb_callback,
    void *user_data
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*------------------------------*
     *  Get treedb
     *------------------------------*/
    json_t *treedb = kw_get_subdict_value(gobj, tranger, "treedbs", treedb_name, 0, 0);
    if(!treedb) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDB not found.",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
        return -1;
    }
    json_object_set_new(
        treedb,
        "__treedb_callback__",
        json_integer((json_int_t)(size_t)treedb_callback)
    );
    json_object_set_new(
        treedb,
        "__treedb_callback_user_data__",
        json_integer((json_int_t)(size_t)user_data)
    );
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_close_db(
    json_t *tranger,
    const char *treedb_name
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*------------------------------*
     *  Close treedb topics
     *------------------------------*/
    json_t *topics = treedb_topics(tranger, treedb_name, 0);
    int idx; json_t *topic;
    json_array_foreach(topics, idx, topic) {
        const char *topic_name = json_string_value(topic);
        treedb_close_topic(tranger, treedb_name, topic_name);
    }
    JSON_DECREF(topics)

    /*------------------------------*
     *  Delete treedb
     *------------------------------*/
    json_t *treedb = kw_get_subdict_value(gobj, tranger, "treedbs", treedb_name, 0, KW_EXTRACT);
    JSON_DECREF(treedb)

    json_decref(topic_cols_desc);
    return 0;
}

/***************************************************************************
    Return is NOT YOURS, pkey MUST be "id"
    WARNING This function don't load hook links.
    HACK IDEMPOTENT function
 ***************************************************************************/
PUBLIC json_t *treedb_create_topic(  // WARNING Return is NOT YOURS
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    int topic_version,
    const char *topic_tkey,
    json_t *pkey2s, // owned, string or dict of string | [strings]
    json_t *cols, // owned
    uint32_t snap_tag,
    BOOL create_schema
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    if(empty_string(treedb_name)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "treedb_name empty",
            NULL
        );
        JSON_DECREF(pkey2s)
        JSON_DECREF(cols)
        return 0;
    }
    if(empty_string(topic_name)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "topic_name empty",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
        JSON_DECREF(pkey2s)
        JSON_DECREF(cols)
        return 0;
    }

    /*------------------------------*
     *  Get treedb
     *------------------------------*/
    json_t *treedb = kw_get_subdict_value(gobj, tranger, "treedbs", treedb_name, 0, 0);
    if(!treedb) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDB not found.",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(pkey2s)
        JSON_DECREF(cols)
        return 0;
    }

    /*------------------------------*
     *  Check if already exists
     *------------------------------*/
    json_t *topic = kw_get_dict(gobj, treedb, topic_name, 0, 0);
    if(topic) {
        JSON_DECREF(pkey2s)
        JSON_DECREF(cols)
        return topic;
    }

    /*------------------------------*
     *  Open/Create "user" topic
     *------------------------------*/
    // Topic version
    json_t *jn_topic_var = json_object();
    json_object_set_new(jn_topic_var, "topic_version", json_integer(topic_version));

    // Topic pkey2s
    if(pkey2s) {
        /*--------------------------------*
         *  Save pkey2s in jn_topic_var
         *--------------------------------*/
        json_t *pkey2s_list = kwid_get_ids(pkey2s);
        json_object_set_new(
            jn_topic_var,
            "pkey2s",
            pkey2s_list
        );
        JSON_DECREF(pkey2s)
    }

    JSON_INCREF(cols)
    JSON_INCREF(jn_topic_var)
    topic = tranger2_create_topic( // User topic
        tranger,    // If topic exists then only needs (tranger,name) parameters
        topic_name,
        "id",       // HACK pkey fixed
        topic_tkey, // tkey
        NULL,
        tranger2_str2system_flag("sf_string_key"), // HACK system_flag fixed
        cols,           // owned below
        jn_topic_var    // owned below
    );

    if(parse_schema_cols(
        topic_cols_desc,
        kwid_new_list(gobj, topic, KW_VERBOSE, "cols")
    )<0) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "parse_schema_cols failed",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
    }

    if(create_schema) {
        json_t *jn_schema = json_object();
        kw_get_str(gobj, jn_schema, "topic_name", topic_name, KW_CREATE);
        kw_get_str(gobj, jn_schema, "pkey", "id", KW_CREATE);
        kw_get_str(gobj, jn_schema, "system_flag", "sf_string_key", KW_CREATE);
        kw_get_dict(gobj, jn_schema, "cols", cols, KW_CREATE); // cols owned
        json_object_update(jn_schema, jn_topic_var);

        char schema_filename[NAME_MAX];
        snprintf(schema_filename, sizeof(schema_filename), "%s-%s.treedb_schema.json",
            treedb_name,
            topic_name
        );

        save_json_to_file(
            gobj,
            kw_get_str(gobj, tranger, "directory", 0, KW_REQUIRED),
            schema_filename,
            (int)kw_get_int(gobj, tranger, "xpermission", 0, KW_REQUIRED),
            (int)kw_get_int(gobj, tranger, "rpermission", 0, KW_REQUIRED),
            (log_opt_t)kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
            TRUE, // Create the file if not exists or overwrite.
            FALSE, // only_read
            jn_schema     // owned
        );
    } else {
        JSON_DECREF(cols)
    }

    /*------------------------------------*
     *      Open "user" lists
     *------------------------------------*/
    char path[NAME_MAX];

    /*----------------------*
     *   Main index: "id"
     *----------------------*/
    build_id_index_path(path, sizeof(path), treedb_name, topic_name);
    kw_get_dict_value(gobj, tranger, path, json_object(), KW_CREATE);

    json_t *jn_filter = json_pack("{s:b, s:s, s:b, s:I}",
        "backward", 1,
        "rkey", "",
        "rt_by_mem", 1,
        "load_record_callback", (json_int_t)(size_t)load_id_callback
    );

    if(snap_tag) {
        // pon el current tag
        json_object_set_new(
            jn_filter,
            "user_flag",
            json_integer(snap_tag)
        );
    }
    json_t *jn_extra = json_pack("{s:s, s:i, s:s, s:{}}",
        "id", path,
        "snap_tag", (int)snap_tag,
        "treedb_name", treedb_name,
        "deleted_records"
    );
    json_t *list = tranger2_open_list( // OLD tranger_open_list
        tranger,
        topic_name,
        jn_filter,  // owned
        jn_extra    // owned
    );
    if(!list) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "tranger2_open_list() failed",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
    }

    /*----------------------*
     *   Secondary indexes
     *----------------------*/
    json_t *iter_pkey2s = kw_get_list(gobj, jn_topic_var, "pkey2s", 0, 0);
    int idx; json_t *jn_pkey2_name;
    json_array_foreach(iter_pkey2s, idx, jn_pkey2_name) {
        const char *pkey2_name = json_string_value(jn_pkey2_name);
        if(empty_string(pkey2_name)) {
            continue;
        }

        /*------------------------------*
         *  Open pkey2 list
         *------------------------------*/
        build_pkey_index_path(path, sizeof(path), treedb_name, topic_name, pkey2_name);
        kw_get_dict_value(gobj, tranger, path, json_object(), KW_CREATE);

        json_t *jn_filter2 = json_pack("{s:b, s:s, s:b, s:I}",
            "backward", 1,
            "rkey", "",
            "rt_by_mem", 1,
            "load_record_callback", (json_int_t)(size_t)load_pkey2_callback
        );

//        if(snap_tag) { // TODO esto no está en timeranger1
//            // pon el current tag
//            json_object_set_new(
//                jn_filter,
//                "user_flag",
//                json_integer(snap_tag)
//            );
//        }
        json_t *jn_extra_ = json_pack("{s:s, s:i, s:s, s:s, s:{}}",
            "id", path,
            "snap_tag", (int)snap_tag,
            "treedb_name", treedb_name,
            "pkey2_name", pkey2_name,
            "deleted_records"
        );

        json_t *list2 = tranger2_open_list( // OLD tranger_open_list
            tranger,
            topic_name,
            jn_filter2,  // owned
            jn_extra_    // owned
        );
        if(!list2) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "tranger2_open_list() failed",
                "treedb_name",  "%s", treedb_name,
                "topic_name",   "%s", topic_name,
                NULL
            );
        }
    }
    JSON_DECREF(jn_topic_var)

    return topic;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_close_topic(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*------------------------------*
     *  Close id list
     *------------------------------*/
    json_t *treedb = kw_get_subdict_value(gobj, tranger, "treedbs", treedb_name, 0, 0);
    if(!treedb) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDB not found.",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
        return -1;
    }

    char path[NAME_MAX];
    build_id_index_path(path, sizeof(path), treedb_name, topic_name);
    json_t *list = tranger2_get_iterator_by_id(tranger, topic_name, path);
    if(list) {
        tranger2_close_iterator(tranger, list);
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "List not found",
            "treedb_name",  "%s", treedb_name,
            "list",         "%s", path,
            NULL
        );
    }

    /*------------------------------*
     *  Close pkey2 lists
     *------------------------------*/
    json_t *iter_pkey2s = treedb_topic_pkey2s(tranger, topic_name);
    int idx; json_t *jn_pkey2_name;
    json_array_foreach(iter_pkey2s, idx, jn_pkey2_name) {
        const char *pkey2_name = json_string_value(jn_pkey2_name);
        if(empty_string(pkey2_name)) {
            continue;
        }
        build_pkey_index_path(path, sizeof(path), treedb_name, topic_name, pkey2_name);

        json_t *list_ = tranger2_get_iterator_by_id(tranger, topic_name, path);
        if(list_) {
            tranger2_close_iterator(tranger, list_);
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "List not found.",
                "treedb_name",  "%s", treedb_name,
                "list",         "%s", path,
                NULL
            );
        }
    }
    JSON_DECREF(iter_pkey2s)

    /*----------------------*
     *  Remove topic data
     *----------------------*/
    snprintf(path, sizeof(path), "treedbs`%s`%s", treedb_name, topic_name);
    json_t *data = kw_get_dict(
        gobj,
        tranger,
        path,
        0,
        KW_REQUIRED|KW_EXTRACT
    );
    json_decref(data);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_delete_topic(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(tranger, treedb_name, topic_name)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        return -1;
    }

    treedb_close_topic(tranger, treedb_name, topic_name);
    return tranger2_delete_topic(tranger, topic_name);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_list_treedb(
    json_t *tranger,
    json_t *kw
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *treedb_list = json_array();

    json_t *treedbs = kw_get_dict(gobj, tranger, "treedbs", 0, 0);
    if(!treedbs) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "NO TreeDB found.",
            NULL
        );
        KW_DECREF(kw)
        return treedb_list;
    }
    const char *treedb_name; json_t *treedb;
    json_object_foreach(treedbs, treedb_name, treedb) {
        json_array_append_new(treedb_list, json_string(treedb_name));
    }

    KW_DECREF(kw)

    return treedb_list;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_topics(
    json_t *tranger,
    const char *treedb_name,
    json_t *jn_options // "dict" return list of dicts, otherwise return list of strings
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *treedb = kw_get_subdict_value(gobj, tranger, "treedbs", treedb_name, 0, 0);
    if(!treedb) {
        JSON_DECREF(jn_options)
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDB not found.",
            "treedb_name",  "%s", treedb_name,
            NULL
        );
        return 0;
    }

    json_t *topic_list = json_array();

    char list_id[NAME_MAX];
    const char *topic_name; json_t *topic_records;
    json_object_foreach(treedb, topic_name, topic_records) {
        if(!json_is_object(topic_records)) {
            continue;
        }
        build_id_index_path(list_id, sizeof(list_id), treedb_name, topic_name);
        if(kw_get_bool(gobj, jn_options, "dict", 0, KW_WILD_NUMBER)) {
            json_t *dict = json_object();
            json_object_set_new(dict, "id", json_string(list_id));
            json_object_set_new(dict, "value", json_string(topic_name));
            json_array_append_new(topic_list, dict);
        } else {
            json_array_append_new(topic_list, json_string(topic_name));
        }
    }

    JSON_DECREF(jn_options)
    return topic_list;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC size_t treedb_topic_size(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
)
{
    json_t *id_index = treedb_get_id_index( // Return is NOT YOURS
        tranger,
        treedb_name,
        topic_name
    );
    return json_object_size(id_index);
}




                    /*------------------------------------*
                     *      Testing
                     *------------------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *my_json_type(json_t *field)
{
    if(json_is_string(field)) {
        if(strcasecmp(json_string_value(field), "blob")==0) {
            return "blob";
        } else {
            return "string";
        }
    } else if(json_is_integer(field)) {
        return "integer";
    } else if(json_is_object(field)) {
        return "object";
    } else if(json_is_array(field)) {
        return "array";
    } else if(json_is_boolean(field)) {
        return "boolean";
    } else if(json_is_real(field)) {
        return "real";
    } else if(json_is_null(field)) {
        return "null";
    } else {
        return "merde";
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int check_desc_field(json_t *desc, json_t *dato)
{
    int ret = 0;
    hgobj gobj = 0;
    /*
     *  Get description of field
     */
    const char *desc_id = kw_get_str(gobj, desc, "id", 0, 0);
    json_t *desc_type = kw_get_dict_value(gobj, desc, "type", 0, 0);
    json_t *desc_flag = kw_get_dict_value(gobj, desc, "flag", 0, 0);

    if(!desc_id) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Field 'id' required",
            "desc",         "%j", desc,
            "dato",         "%j", dato,
            NULL
        );
        return -1;
    }
    if(!desc_type) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Field 'type' required",
            "desc",         "%j", desc,
            "dato",         "%j", dato,
            NULL
        );
        return -1;
    }

    /*
     *  Get value
     */
    json_t *value = kw_get_dict_value(gobj, dato, desc_id, 0, 0);

    /*
     *  Check required
     */
    if(kw_has_word(gobj, desc_flag, "required", 0)) {
        if(!value) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Field required",
                "desc",         "%j", desc,
                "dato",         "%j", dato,
                "field",        "%s", desc_id,
                NULL
            );
            return -1;
        }
    }

    /*
     *  Check value type
     */
    const char *my_desc_type = my_json_type(desc_type);
    if(kw_has_word(gobj, desc_flag, "enum", 0)) {
        my_desc_type = "enum";
    }

    SWITCHS(my_desc_type) {
        /*----------------------------*
         *      Enum
         *----------------------------*/
        CASES("enum")
            json_t *desc_enum = kw_get_list(gobj, desc, "enum", 0, 0);
            switch(json_typeof(value)) { // json_typeof NO CONTROLADO
            case JSON_ARRAY:
                {
                    int idx; json_t *v;
                    json_array_foreach(value, idx, v) {
                        switch(json_typeof(v)) {
                        case JSON_STRING:
                            if(!json_str_in_list(gobj, desc_enum, json_string_value(v), TRUE)) {
                                gobj_log_error(gobj, 0,
                                    "function",     "%s", __FUNCTION__,
                                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                                    "msg",          "%s", "Wrong enum type",
                                    "desc",         "%j", desc,
                                    "dato",         "%j", dato,
                                    "field",        "%s", desc_id,
                                    "value",        "%j", value,
                                    "v",            "%j", v,
                                    "value_to_be",  "%j", desc_enum,
                                    NULL
                                );
                                ret += -1;
                            }
                            break;
                        default:
                            gobj_log_error(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                                "msg",          "%s", "Case not implemented",
                                "desc",         "%j", desc,
                                "dato",         "%j", dato,
                                "field",        "%s", desc_id,
                                "value",        "%j", value,
                                "v",            "%j", v,
                                "value_to_be",  "%j", desc_enum,
                                NULL
                            );
                            ret += -1;
                            break;
                        }

                    }
                }
                break;
            case JSON_STRING:
                if(!json_str_in_list(gobj, desc_enum, json_string_value(value), TRUE)) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "Wrong enum type",
                        "desc",         "%j", desc,
                        "dato",         "%j", dato,
                        "field",        "%s", desc_id,
                        "value",        "%j", value,
                        "value_to_be",  "%j", desc_enum,
                        NULL
                    );
                    ret += -1;
                }
                break;
            default:
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Enum value must be string or string's array",
                    "desc",         "%j", desc,
                    "dato",         "%j", dato,
                    "field",        "%s", desc_id,
                    "value",        "%j", value,
                    "value_to_be",  "%j", desc_enum,
                    NULL
                );
                ret += -1;
                break;
            }
            break;

        /*----------------------------*
         *      Enum
         *----------------------------*/
        CASES("blob")
            break;

        /*----------------------------*
         *      Json basic types
         *----------------------------*/
        DEFAULTS
            if(value) {
                const char *value_type = my_json_type(value);
                if(!kw_has_word(gobj, desc_type, value_type, 0)) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "Wrong basic type",
                        "desc",         "%j", desc,
                        "dato",         "%j", dato,
                        "field",        "%s", desc_id,
                        "value_type",   "%s", value_type,
                        "value_to_be",  "%j", desc_type,
                        NULL
                    );
                    ret += -1;
                }
            }
            break;
    } SWITCHS_END;

    return ret;
}

/***************************************************************************
 *  Return 0 if ok or # of errors in negative
 ***************************************************************************/
PUBLIC int parse_schema(
    json_t *schema  // not owned
)
{
    hgobj gobj = 0;
    if(!schema) {
        return -1;
    }

    int ret = 0;

    json_t *cols_desc = _treedb_create_topic_cols_desc();

    json_t *topics = kwid_new_dict(gobj,  schema, KW_VERBOSE, "topics");
    const char *topic_name; json_t *topic;
    json_object_foreach(topics, topic_name, topic) {
        ret += parse_schema_cols(
            topic_cols_desc,
            kwid_new_list(gobj, topic, KW_VERBOSE, "cols")
        );
    }

    ret += parse_hooks(
        schema  // not owned
    );

    json_decref(topics);
    json_decref(cols_desc);

    return ret;
}

/***************************************************************************
 *  Return 0 if ok or # of errors in negative
 ***************************************************************************/
PUBLIC int parse_schema_cols(
    json_t *cols_desc,  // NOT owned
    json_t *dato  // owned
)
{
    hgobj gobj = 0;
    int ret = 0;

    if(!(json_is_object(dato) || json_is_array(dato))) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Data must be an {} or [{}]",
            "cols_desc",    "%j", cols_desc,
            NULL
        );
        JSON_DECREF(dato);
        return -1;
    }

    int idx; json_t *desc;

    if(json_is_object(dato)) {
        json_array_foreach(cols_desc, idx, desc) {
            ret += check_desc_field(desc, dato);
        }
    } else if(json_is_array(dato)) {
        int idx1; json_t *d;
        json_array_foreach(dato, idx1, d) {
            int idx2;
            json_array_foreach(cols_desc, idx2, desc) {
                ret += check_desc_field(desc, d);
            }
        }
    }

    JSON_DECREF(dato);

    return ret;
}

/***************************************************************************
 *  Return 0 if ok or # of errors in negative
 ***************************************************************************/
PUBLIC int parse_hooks(
    json_t *schema  // not owned
)
{
    hgobj gobj = 0;
    int ret = 0;

    json_t *topics = kwid_new_dict(gobj, schema, KW_VERBOSE, "topics");
    const char *topic_name; json_t *topic;
    json_object_foreach(topics, topic_name, topic) {
        json_t *cols = kwid_new_list(gobj, topic, KW_VERBOSE, "cols");
        if(!cols) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "topic without cols",
                "topic_name",   "%s", topic_name,
                NULL
            );
            ret += -1;
            continue;
        }
        int idx; json_t *col;
        json_array_foreach(cols, idx, col) {
            const char *id = kw_get_str(gobj, col, "id", 0, 0);
            json_t *flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
            /*-------------------------*
             *      Is a hook?
             *-------------------------*/
            if(kw_has_word(gobj, flag, "hook", 0)) {
                /*---------------------------------*
                 *  It's a hook, check the links
                 *---------------------------------*/
                json_t *hook = kw_get_dict(gobj, col, "hook", 0, 0);
                if(!hook) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "column def with hook field not found",
                        "topic_name",   "%s", topic_name,
                        "id",           "%s", id,
                        NULL
                    );
                    gobj_trace_json(gobj, col, "column def with hook field not found");
                    ret += -1;
                    continue;
                }

                /*----------------------------*
                 *  Check edges
                 *----------------------------*/
                const char *link_topic_name; json_t *link_field;
                json_object_foreach(hook, link_topic_name, link_field) {
                    /*------------------------------*
                     *  Check link: who child node
                     *------------------------------*/
                    json_t *link_topic = kwid_get(gobj,
                        schema,
                        0,
                        "topics`%s",
                            link_topic_name
                    );
                    if(!link_topic) {
                        gobj_log_error(gobj, 0,
                            "function",         "%s", __FUNCTION__,
                            "msgset",           "%s", MSGSET_TREEDB_ERROR,
                            "msg",              "%s", "link topic not found",
                            "topic_name",       "%s", topic_name,
                            "id",               "%s", id,
                            "link_topic_name",  "%s", link_topic_name,
                            NULL
                        );
                        ret += -1;
                        continue;
                    }
                    const char *s_link_field = json_string_value(link_field);
                    json_t *field = kwid_get(gobj,
                        link_topic,
                        0,
                        "cols`%s",
                            s_link_field
                    );
                    if(!field) {
                        gobj_log_error(gobj, 0,
                            "function",         "%s", __FUNCTION__,
                            "msgset",           "%s", MSGSET_TREEDB_ERROR,
                            "msg",              "%s", "link field not found",
                            "topic_name",       "%s", topic_name,
                            "id",               "%s", id,
                            "link_topic_name",  "%s", link_topic_name,
                            "link_field",       "%s", s_link_field,
                            NULL
                        );
                        gobj_trace_json(gobj, link_topic, "link field not found");
                        ret += -1;
                    }

                    // CHEQUEA que tiene el flag fkey
                    json_t *jn_flag = kwid_get(gobj, field, 0, "flag");
                    if(!kw_has_word(gobj, jn_flag, "fkey", 0)) {
                        gobj_log_error(gobj, 0,
                            "function",         "%s", __FUNCTION__,
                            "msgset",           "%s", MSGSET_TREEDB_ERROR,
                            "msg",              "%s", "link field must be a fkey",
                            "topic_name",       "%s", topic_name,
                            "id",               "%s", id,
                            "link_topic_name",  "%s", link_topic_name,
                            "link_field",       "%s", s_link_field,
                            NULL
                        );
                        ret += -1;
                        continue;
                    }

                    if(kw_has_word(gobj, field, "fkey", 0)) {
                        gobj_log_error(gobj, 0,
                            "function",         "%s", __FUNCTION__,
                            "msgset",           "%s", MSGSET_TREEDB_ERROR,
                            "msg",              "%s", "Only can be one fkey",
                            "topic_name",       "%s", topic_name,
                            "id",               "%s", id,
                            "link_topic_name",  "%s", link_topic_name,
                            "link_field",       "%s", s_link_field,
                            NULL
                        );
                        ret += -1;
                        continue;
                    }

                    json_object_set_new(
                        field,
                        "fkey",
                        json_pack("{s:s}",
                            topic_name, id
                        )
                    );
                }

            } else {
                /*---------------------------------*
                 *  It's not a hook,
                 *  check it hasn't link/reverse
                 *---------------------------------*/
                if(kw_has_key(col, "link")) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "Not hook with a link",
                        "topic_name",   "%s", topic_name,
                        "id",           "%s", id,
                        NULL
                    );
                    ret += -1;
                }
                if(kw_has_key(col, "reverse")) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "Not hook with a reverse",
                        "topic_name",   "%s", topic_name,
                        "id",           "%s", id,
                        NULL
                    );
                    ret += -1;
                }
            }
        }
        JSON_DECREF(cols)
    }

    JSON_DECREF(topics)

    return ret;
}

/***************************************************************************
 * Return a list of hook field names of the topic.
 * Return MUST be decref
 ***************************************************************************/
PUBLIC json_t *topic_desc_hook_names(
    json_t *topic_desc // owned
)
{
    hgobj gobj = 0;
    json_t *jn_hook_field_names = json_array();

    int idx; json_t *col;
    json_array_foreach(topic_desc, idx, col) {
        json_t *flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        /*-------------------------*
         *      Is a hook?
         *-------------------------*/
        if(kw_has_word(gobj, flag, "hook", 0)) {
            json_array_append(jn_hook_field_names, json_object_get(col, "id"));
        }
    }
    JSON_DECREF(topic_desc)
    return jn_hook_field_names;
}

/***************************************************************************
 * Return a list of fkeys field names of the topic.
 * Return MUST be decref
 ***************************************************************************/
PUBLIC json_t *topic_desc_fkey_names(
    json_t *topic_desc // owned
)
{
    hgobj gobj = 0;
    json_t *jn_fkey_field_names = json_array();

    int idx; json_t *col;
    json_array_foreach(topic_desc, idx, col) {
        json_t *flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        /*-------------------------*
         *      Is a fkey?
         *-------------------------*/
        if(kw_has_word(gobj, flag, "fkey", 0)) {
            json_array_append(jn_fkey_field_names, json_object_get(col, "id"));
        }
    }
    JSON_DECREF(topic_desc)
    return jn_fkey_field_names;
}




                    /*------------------------------------*
                     *      Write/Read to/from tranger
                     *------------------------------------*/




/***************************************************************************
 *  Usado en set_tranger_field_value(), to write the fkey in tranger file
 *  type: "list", "dict", "string"
 ***************************************************************************/
PRIVATE json_t *filtra_fkeys(
    const char *topic_name,
    const char *col_name,
    const char *type,
    json_t *value  // not owned
)
{
    hgobj gobj = 0;
    json_t *jn_list = json_array();

    switch(json_typeof(value)) {
    case JSON_ARRAY:
        {
            int idx; json_t *v;
            json_array_foreach(value, idx, v) {
                if(json_typeof(v)==JSON_STRING) {
                    // String format
                    const char *id = json_string_value(v);
                    if(empty_string(id)) {
                        continue;
                    }
                    if(count_char(id, '^')==2) {
                        json_array_append_new(jn_list, json_string(id));
                    } else {
                        gobj_log_error(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                            "msg",          "%s", "Wrong fkey reference: must be \"topic_name^id^hook_name\"",
                            "topic_name",   "%s", topic_name,
                            "col_name",     "%s", col_name,
                            "ref",          "%s", id,
                            NULL
                        );
                        json_decref(jn_list);
                        return 0;
                    }
                } else if(json_typeof(v)==JSON_OBJECT) {
                    char temp[NAME_MAX];
                    const char *id = kw_get_str(gobj, v, "id", 0, 0);
                    const char *topic_name_ = kw_get_str(gobj, v, "topic_name", 0, 0);
                    const char *hook_name = kw_get_str(gobj, v, "hook_name", 0, 0);
                    if(!id || !topic_name_ || !hook_name) {
                        gobj_log_error(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                            "msg",          "%s", "Wrong fkey reference: must be {topic_name,id,hook_name}",
                            "topic_name",   "%s", topic_name_,
                            "col_name",     "%s", col_name,
                            "ref",          "%j", v,
                            NULL
                        );
                        json_decref(jn_list);
                        return 0;
                    }
                    snprintf(temp, sizeof(temp), "%s^%s^%s",
                        topic_name_,
                        id,
                        hook_name
                    );
                    json_array_append_new(jn_list, json_string(temp));
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            const char *id; json_t *v;
            json_object_foreach(value, id, v) {
                if(count_char(id, '^')==2) {
                    json_array_append_new(jn_list, json_string(id));
                } else {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                        "msg",          "%s", "Wrong fkey reference: must be \"topic_name^id^hook_name\"",
                        "topic_name",   "%s", topic_name,
                        "col_name",     "%s", col_name,
                        "ref",          "%s", id,
                        "v",            "%j", v,
                        NULL
                    );
                    json_decref(jn_list);
                    return 0;
                }
            }
        }
        break;
    case JSON_STRING:
        {
            const char *id = json_string_value(value);
            if(empty_string(id)) {
                break;
            }
            if(count_char(id, '^')==2) {
                json_array_append_new(jn_list, json_string(id));
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "Wrong fkey reference: must be \"topic_name^id^hook_name\"",
                    "topic_name",   "%s", topic_name,
                    "col_name",     "%s", col_name,
                    "ref",          "%s", id,
                    NULL
                );
                json_decref(jn_list);
                return 0;
            }
        }
        break;
    default:
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Wrong fkey reference: type unknown",
            "topic_name",   "%s", topic_name,
            "col_name",     "%s", col_name,
            "v",            "%j", value,
            NULL
        );
        json_decref(jn_list);
        return 0;
    }

    json_t *mix_ids = 0;

    SWITCHS(type) {
        CASES("list")
            mix_ids = json_incref(jn_list);
            break;

        CASES("dict")
            mix_ids = json_object();
            int idx; json_t *v;
            json_array_foreach(jn_list, idx, v) {
                json_object_set_new(mix_ids, json_string_value(v), json_true());
            }
            break;

        CASES("string")
            if(json_array_size(jn_list) > 1) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "Wrong fkey reference: must be one string",
                    "topic_name",   "%s", topic_name,
                    "col_name",     "%s", col_name,
                    "list",         "%j", jn_list,
                    NULL
                );
                json_decref(jn_list);
                return 0;

            } else if(json_array_size(jn_list) == 1) {
                mix_ids = json_string(json_string_value(json_array_get(jn_list, 0)));

            } else {
                mix_ids = json_string("");
            }
            break;

        DEFAULTS
            break;
    } SWITCHS_END

    json_decref(jn_list);

    return mix_ids;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_tranger_field_value(
    const char *topic_name,
    const char *field,
    json_t *col,    // NOT owned
    json_t *record, // NOT owned
    json_t *value,  // NOT owned
    BOOL create
)
{
    hgobj gobj = 0;

    if(!field) {
        field = kw_get_str(gobj, col, "id", 0, KW_REQUIRED);
    }
    if(!field) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
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
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
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
        if(!value) { // WARNING efecto colateral? 16-oct-2020 || json_is_null(value)) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Field required",
                "topic_name",   "%s", topic_name,
                "field",        "%s", field,
                "value",        "%j", value,
                NULL
            );
            return -1;
        }
    }

    BOOL is_persistent = kw_has_word(gobj, desc_flag, "persistent", 0)?TRUE:FALSE;
    BOOL wild_conversion = kw_has_word(gobj, desc_flag, "wild", 0)?TRUE:FALSE;
    BOOL is_enum = kw_has_word(gobj, desc_flag, "enum", 0)?TRUE:FALSE;
    BOOL is_hook = kw_has_word(gobj, desc_flag, "hook", 0)?TRUE:FALSE;
    BOOL is_fkey = kw_has_word(gobj, desc_flag, "fkey", 0)?TRUE:FALSE;
    BOOL is_now = kw_has_word(gobj, desc_flag, "now", 0)?TRUE:FALSE;
    BOOL is_template = kw_has_word(gobj, desc_flag, "template", 0)?TRUE:FALSE;
    if(!(is_persistent || is_hook || is_fkey)) {
        // Not save to tranger
        return 0;
    }

    /*
     *  Null
     */
    if(json_is_null(value)) {
        if(!(is_hook || is_fkey || is_enum)) {
            if(kw_has_word(gobj, desc_flag, "notnull", 0)) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
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
    }

    const char *real_type = type;
    if(is_hook) {
        type = "hook";
    } else if(is_fkey) {
        type = "fkey";
    } else if(is_enum) {
        type = "enum";
    } else if(is_template) {
        type = "template";
    }

    SWITCHS(type) {
        CASES("hook")
            SWITCHS(real_type) {
                CASES("dict")
                CASES("object")
                    json_object_set_new(record, field, json_object());
                    break;

                CASES("string")
                    json_object_set_new(record, field, json_string(""));
                    break;

                CASES("list")   // re-enable lists for hooks
                CASES("array")
                    json_object_set_new(record, field, json_array());
                    break;

                //CASES("list")  // HACK disable lists for hooks TODO si dejo esto fallan los test!!
                //CASES("array")
                DEFAULTS
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "Bad hook col type ",
                        "topic_name",   "%s", topic_name,
                        "col",          "%j", col,
                        "field",        "%s", field,
                        "type",         "%s", type,
                        NULL
                    );
                    return -1;
            } SWITCHS_END;
            break;

        CASES("fkey")
            SWITCHS(real_type) {
                CASES("list")
                CASES("array")
                    if(create || !value) {
                        json_object_set_new(record, field, json_array());
                    } else {
                        json_t *mix_ids = filtra_fkeys(
                            topic_name,
                            field,
                            "list",
                            value
                        );
                        if(!mix_ids) {
                            // Error already logged
                            return -1;
                        }
                        json_object_set_new(record, field, mix_ids);
                    }
                    break;

                CASES("dict")
                CASES("object")
                    if(create || !value) {
                        // No dejes poner datos en la creación.
                        json_object_set_new(record, field, json_object());
                    } else {
                        json_t *mix_ids = filtra_fkeys(
                            topic_name,
                            field,
                            "dict",
                            value
                        );
                        if(!mix_ids) {
                            // Error already logged
                            return -1;
                        }
                        json_object_set_new(record, field, mix_ids);
                    }
                    break;

                CASES("string")
                    if(create || !value) {
                        // No dejes poner datos en la creación.
                        json_object_set_new(record, field, json_string(""));
                    } else {
                        json_t *mix_ids = filtra_fkeys(
                            topic_name,
                            field,
                            "string",
                            value
                        );
                        if(!mix_ids) {
                            // Error already logged
                            return -1;
                        }
                        json_object_set_new(record, field, mix_ids);
                    }
                    break;

                DEFAULTS
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "Bad fkey col type ",
                        "topic_name",   "%s", topic_name,
                        "col",          "%j", col,
                        "field",        "%s", field,
                        "type",         "%s", type,
                        NULL
                    );
                    return -1;
            } SWITCHS_END;
            break;

        CASES("enum")
            SWITCHS(real_type) {
                CASES("string")
                    if(!value) {
                        json_object_set_new(record, field, json_string(""));
                    } else if(json_is_string(value)) {
                        json_object_set(record, field, value);
                    } else if(wild_conversion) {
                        char *v = jn2string(value);
                        json_object_set_new(record, field, json_string(v));
                        GBMEM_FREE(v)
                    } else {
                        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_TREEDB_ERROR,
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

                CASES("list")
                CASES("array")
                    if(JSON_TYPEOF(value, JSON_ARRAY)) {
                        json_object_set(record, field, value);

                    } else if(JSON_TYPEOF(value, JSON_STRING)) {
                        json_t *v = legalstring2json(json_string_value(value), FALSE);
                        if(json_is_array(v)) {
                            json_object_set_new(record, field, v);
                        } else {
                            json_decref(v);
                            json_object_set_new(record, field, json_array());
                        }
                    } else {
                        json_object_set_new(record, field, json_array());
                    }
                    break;

                DEFAULTS
                    gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "Bad enum col type ",
                        "topic_name",   "%s", topic_name,
                        "col",          "%j", col,
                        "field",        "%s", field,
                        "type",         "%s", type,
                        NULL
                    );
                    return -1;
            } SWITCHS_END;
            break;

        CASES("list")
        CASES("array")
            if(JSON_TYPEOF(value, JSON_ARRAY)) {
                json_object_set(record, field, value);

            } else if(JSON_TYPEOF(value, JSON_STRING)) {
                json_t *v = legalstring2json(json_string_value(value), FALSE);
                if(json_is_array(v)) {
                    json_object_set_new(record, field, v);
                } else {
                    json_decref(v);
                    json_object_set_new(record, field, json_array());
                }
            } else {
                json_object_set_new(record, field, json_array());
            }
            break;

        CASES("template")
        CASES("dict")
        CASES("object")
            if(JSON_TYPEOF(value, JSON_OBJECT)) {
                json_object_set(record, field, value);

            } else if(JSON_TYPEOF(value, JSON_STRING)) {
                json_t *v = legalstring2json(json_string_value(value), FALSE);
                if(json_is_object(v)) {
                    json_object_set_new(record, field, v);
                } else {
                    json_decref(v);
                    json_object_set_new(record, field, json_object());
                }
            } else {
                json_object_set_new(record, field, json_object());
            }
            break;

        CASES("blob")
            json_object_set(record, field, value);
            break;

        CASES("string")
            if(!value) {
                json_object_set_new(record, field, json_string(""));
            } else if(json_is_string(value)) {
                json_object_set(record, field, value);
            } else if(wild_conversion) {
                char *v = jn2string(value);
                json_object_set_new(record, field, json_string(v));
                GBMEM_FREE(v)
            } else {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
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
            if(is_now) {
                time_t t_now;
                time(&t_now);
                json_object_set_new(record, field, json_integer(t_now));
            } else if(!value) {
                json_object_set_new(record, field, json_integer(0));
            } else if(json_is_integer(value)) {
                json_object_set(record, field, value);
            } else if(wild_conversion) {
                json_int_t v = jn2integer(value);
                json_object_set_new(record, field, json_integer(v));
            } else {
                json_int_t v = jn2integer(value);
                json_object_set_new(record, field, json_integer(v));
                gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
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
                gobj_log_warning(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
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
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Col type unknown",
                "topic_name",   "%s", topic_name,
                "col",          "%j", col,
                "field",        "%s", field,
                "type",         "%s", type,
                NULL
            );
            return -1;
    } SWITCHS_END

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_volatil_field_value(
    const char *type,
    const char *field,
    json_t *record, // NOT owned
    json_t *value   // NOT owned
)
{
    hgobj gobj = 0;

    SWITCHS(type) {
        CASES("enum")
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

        CASES("blob")
            if(value) {
                json_object_set(record, field, value);
            }
            break;

        DEFAULTS
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
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
PUBLIC int set_volatil_values(
    json_t *tranger,
    const char *topic_name,
    json_t *record,  // NOT owned
    json_t *kw // NOT owned
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name);
    if(!cols) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic without cols",
            "topic_name",   "%s", topic_name,
            NULL
        );
        return 0;
    }

    const char *field; json_t *col;
    json_object_foreach(cols, field, col) {
        json_t *value = kw_get_dict_value(
            gobj,
            kw,
            field,
            0,
            0
        );
        if(!value) {
            value = kw_get_dict_value(gobj, col, "default", 0, 0);
        }

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
            type,
            field_,
            record, // NOT owned
            value   // NOT owned
        );
    }

    JSON_DECREF(cols)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_missing_values(
    hgobj gobj,
    json_t *tranger,
    const char *topic_name,
    json_t *record  // NOT owned
)
{
    json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name);
    if(!cols) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic without cols",
            "topic_name",   "%s", topic_name,
            NULL
        );
        return 0;
    }
    const char *field; json_t *col;
    json_object_foreach(cols, field, col) {
        const char *field_ = kw_get_str(gobj, col, "id", 0, KW_REQUIRED);
        if(!field_) {
            continue;
        }
        const char *type = kw_get_str(gobj, col, "type", 0, KW_REQUIRED);
        if(!type) {
            continue;
        }
        json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        BOOL is_hook = kw_has_word(gobj, desc_flag, "hook", 0)?TRUE:FALSE;
        BOOL is_fkey = kw_has_word(gobj, desc_flag, "fkey", 0)?TRUE:FALSE;
        if((is_hook || is_fkey)) {
            continue;
        }

        json_t *value = kw_get_dict_value(
            gobj,
            record,
            field_,
            0,
            0
        );

        if(value) {
            continue;
        }

        value = kw_get_dict_value(gobj, col, "default", 0, 0);

        set_volatil_field_value(
            type,
            field_,
            record, // NOT owned
            value   // NOT owned
        );
    }

    JSON_DECREF(cols)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *record2tranger(
    hgobj gobj,
    json_t *tranger,
    const char *topic_name,
    json_t *kw,  // NOT owned
    BOOL create // create or update
)
{
    json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name);
    if(!cols) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
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
        if(!value) {
            if(create) {
                value = kw_get_dict_value(gobj, col, "default", 0, 0);
            }
        }
        if(set_tranger_field_value(
                topic_name,
                field,
                col,
                new_record,
                value,
                create
            )<0) {
            // Error already logged
            JSON_DECREF(new_record)
            JSON_DECREF(cols)
            return 0;
        }
    }

    json_object_del(new_record, "__md_treedb__");

    JSON_DECREF(cols)
    return new_record;
}

/***************************************************************************
 *  Return json object with record metadata
 ***************************************************************************/
PRIVATE json_t *_md2json(
    const char *treedb_name,
    const char *topic_name,
    md2_record_t *md_record,
    json_int_t rowid
)
{
    json_t *jn_md = json_object();
    json_object_set_new(
        jn_md,
        "treedb_name",
        json_string(treedb_name)
    );
    json_object_set_new(
        jn_md,
        "topic_name",
        json_string(topic_name)
    );
    json_object_set_new(jn_md, "__rowid__", json_integer(rowid));
    json_object_set_new(jn_md, "__t__", json_integer((json_int_t)md_record->__t__));
    json_object_set_new(jn_md, "__tm__", json_integer((json_int_t)md_record->__tm__));
    json_object_set_new(jn_md, "__tag__", json_integer(get_user_flag(md_record)));
    json_object_set_new(jn_md, "__pure_node__", json_true());

    return jn_md;
}

/***************************************************************************
 *  When record is loaded from disk then create the node
 *  when is loaded from memory then notify to subscribers
 ***************************************************************************/
PRIVATE int load_id_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,       // iterator or rt_mem/rt_disk, don't own
    json_int_t rowid,   // in a rt_mem will be the relative rowid, in rt_disk the absolute rowid
    md2_record_t *md_record,
    json_t *jn_record  // must be owned
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));
    const char *topic_name = json_string_value(json_object_get(topic, "topic_name"));
    json_t *deleted_records = json_object_get(list, "deleted_records");
    const char *treedb_name = json_string_value(json_object_get(list, "treedb_name"));

    /*----------------------------------*
     *  Get indexx: to load from disk
     *----------------------------------*/
    json_t *indexx = treedb_get_id_index(tranger, treedb_name, topic_name);
    if(!indexx) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "What topic name? TreeDb index not found",
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_record)
        return 0;  // Timeranger: does not load the record, it's mine.
    }

    system_flag2_t system_flag = get_system_flag(md_record);
    if(system_flag & sf_loading_from_disk) {
        /*---------------------------------*
         *  Loading treedb from disk
         *---------------------------------*/
        if(system_flag & (sf_deleted_record)) {
            /*---------------------------------*
             *      Record deleted
             *---------------------------------*/
            json_object_set_new(
                deleted_records,
                key,
                json_true()
            );
        } else {
            /*-------------------------------------*
             *  If not deleted record append node
             *-------------------------------------*/
            if(!json_object_get(deleted_records, key)) {
                /*-------------------------------*
                 *  Exists already the node?
                 *-------------------------------*/
                if(exist_primary_node(indexx, key)) {
                    // Ignore
                    // The node with this key already exists
                    // HACK using backward, the first record is the last record
                } else {
                    /*-------------------------------*
                     *  Append new node
                     *-------------------------------*/
                    /*---------------------------------------------*
                     *  Build metadata, loading node from tranger
                     *---------------------------------------------*/
                    json_t *jn_record_md = json_object_get(jn_record, "__md_treedb__");
                    json_object_set_new(jn_record_md, "__pending_links__", json_true());

                    /*--------------------------------------------*
                     *  Set missing data
                     *--------------------------------------------*/
                    set_missing_values( // fill empty fields
                        gobj,
                        tranger,
                        topic_name,
                        jn_record  // NOT owned
                    );

                    /*-------------------------------*
                     *  Write node in memory: id
                     *-------------------------------*/
                    add_primary_node(
                        indexx,
                        key,
                        jn_record
                    );
                }
            }
        }
    } else {
        /*---------------------------------*
         *      Working in memory
         *---------------------------------*/
        if(json_object_get(deleted_records, key)) {
            // This key is operative again
            json_object_del(deleted_records, key);
        }
    }

    JSON_DECREF(jn_record)
    return 0;  // Timeranger: does not load the record, it's mine.
}

/***************************************************************************
 *  When record is loaded from disk then create the node
 *  when is loaded from memory then notify to subscribers
 ***************************************************************************/
PRIVATE int load_pkey2_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,       // iterator or rt_mem/rt_disk, don't own
    json_int_t rowid,   // in a rt_mem will be the relative rowid, in rt_disk the absolute rowid
    md2_record_t *md_record,
    json_t *jn_record  // must be owned
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    const char *pkey2_name = kw_get_str(
        gobj,
        list, "pkey2_name", "", KW_REQUIRED
    );
    json_t *deleted_records = kw_get_dict(
        gobj,
        list, "deleted_records", 0, KW_REQUIRED
    );

    const char *treedb_name = kw_get_str(
        gobj,
        list, "treedb_name", 0, KW_REQUIRED
    );
    const char *topic_name = kw_get_str(
        gobj,
        list, "topic_name", 0, KW_REQUIRED
    );

    /*---------------------------------*
     *  Get indexy: to load from disk
     *---------------------------------*/
    json_t *indexy = treedb_get_pkey2_index(
        tranger,
        treedb_name,
        topic_name,
        pkey2_name
    );
    if(!indexy) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "TreeDb Topic indexy NOT FOUND",
            "topic_name",   "%s", topic_name,
            "pkey2_name",   "%s", pkey2_name,
            NULL
        );
        JSON_DECREF(jn_record)
        return 0;  // Timeranger: does not load the record, it's mine.
    }

    system_flag2_t system_flag = get_system_flag(md_record);
    if(system_flag & (sf_loading_from_disk)) {
        /*---------------------------------*
         *  Loading treedb from disk
         *---------------------------------*/
        if(system_flag & (sf_deleted_record)) {
            /*---------------------------------*
             *      Record deleted
             *---------------------------------*/
            json_object_set_new(
                deleted_records,
                key,
                json_true()
            );
        } else {
            /*-------------------------------------*
             *  If not deleted record append node
             *-------------------------------------*/
            if(!json_object_get(deleted_records, key)) {
                /*-------------------------------*
                 *  Exists already the node?
                 *-------------------------------*/
                const char *pkey2_value = get_key2_value(
                    tranger,
                    topic_name,
                    pkey2_name,
                    jn_record
                );
                if(exist_secondary_node(indexy, key, pkey2_value)) {
                    // Ignore
                    // The node with this key already exists
                    // HACK using backward, the first record is the last record
                } else {
                    /*-------------------------------*
                     *  Append new node
                     *-------------------------------*/
                    /*---------------------------------------------*
                     *  Build metadata, loading node from tranger
                     *---------------------------------------------*/

                    /*--------------------------------------------*
                     *  Set missing data
                     *--------------------------------------------*/
                    set_missing_values( // fill empty fields
                        gobj,
                        tranger,
                        topic_name,
                        jn_record // NOT owned
                    );

                    /*-------------------------------*
                     *  Write node in memory: pkey2
                     *-------------------------------*/
                    add_secondary_node(
                        indexy,
                        key,
                        pkey2_value,
                        jn_record
                    );
                }
            }
        }
    } else {
        /*---------------------------------*
         *      Working in memory
         *---------------------------------*/
        if(json_object_get(deleted_records, key)) {
            // This key is operative again
            json_object_del(deleted_records, key);
        }
    }

    JSON_DECREF(jn_record)
    return 0;  // Timeranger: does not load the record, it's mine.
}

/***************************************************************************
 *  Decode fkey
 ***************************************************************************/
PUBLIC BOOL decode_parent_ref(
    const char *pref,
    char *topic_name, int topic_name_size,
    char *id, int id_size,
    char *hook_name, int hook_name_size
)
{
    hgobj gobj = 0;

    if(topic_name) {
        *topic_name = 0;
    }
    if(id) {
        *id = 0;
    }
    if(hook_name) {
        *hook_name = 0;
    }

    if(!(pref && strchr(pref, '^'))) {
        return FALSE;
    }
    int list_size;
    const char **ss = split2(pref, "^", &list_size);
    if(list_size!=3) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Wrong fkey (parent) reference: must be \"parent_topic_name^parent_id^hook_name\"",
            "pref",         "%s", pref,
            NULL
        );
        split_free2(ss);
        return FALSE;
    }

    if(topic_name) {
        snprintf(topic_name, topic_name_size, "%s", ss[0]);
    }
    if(id) {
        snprintf(id, id_size, "%s", ss[1]);
    }
    if(hook_name) {
        snprintf(hook_name, hook_name_size, "%s", ss[2]);
    }

    split_free2(ss);
    return TRUE;

}

/***************************************************************************
 *  Decode child ref
 ***************************************************************************/
PUBLIC BOOL decode_child_ref(
    const char *pref,
    char *topic_name, int topic_name_size,
    char *id, int id_size
)
{
    hgobj gobj = 0;

    *topic_name = *id = 0;

    if(!(pref && strchr(pref, '^'))) {
        return FALSE;
    }
    int list_size;
    const char **ss = split2(pref, "^", &list_size);
    if(list_size!=2) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "Wrong hook (child) reference: must be \"child_topic_name^child_id\"",
            "pref",         "%s", pref,
            NULL
        );
        split_free2(ss);
        return FALSE;
    }

    snprintf(topic_name, topic_name_size, "%s", ss[0]);
    snprintf(id, id_size, "%s", ss[1]);

    split_free2(ss);
    return TRUE;

}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL is_fkey_registered(
    json_t *fkey_desc,
    const char *parent_topic_name_,
    const char *hook_name_
)
{
    const char *parent_topic_name; json_t *jn_parent_field_name;
    json_object_foreach(fkey_desc, parent_topic_name, jn_parent_field_name) {
        const char *hook_name = json_string_value(jn_parent_field_name);
        if(strcmp(parent_topic_name, parent_topic_name_)==0 && strcmp(hook_name, hook_name_)==0) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  Loading hook links
 ***************************************************************************/
PRIVATE int link_child_to_parent(
    json_t *tranger,
    const char *treedb_name,
    const char *pref,
    json_t *child_node,
    const char *fkey_col_name,
    json_t *fkey_desc,
    BOOL is_child_hook
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    char parent_topic_name[NAME_MAX];
    char parent_id[NAME_MAX];
    char hook_name[NAME_MAX];
    if(!decode_parent_ref(
        pref,
        parent_topic_name, sizeof(parent_topic_name),
        parent_id, sizeof(parent_id),
        hook_name, sizeof(hook_name)
    )) {
        /*
         *  It's not a fkey.
         *  It's not an error, it happens when it's a hook and fkey field.
         */
        return 0;
    }

    /*
     *  Check if it's a registered desc fkey
     */
    if(!is_fkey_registered(fkey_desc, parent_topic_name, hook_name)) {
        // It's not for us
        return 0;
    }

    const char *child_topic_name = json_string_value(
        kwid_get(gobj, child_node, 0, "__md_treedb__`topic_name")
    );
    const char *child_id = kw_get_str(gobj, child_node, "id", "", KW_REQUIRED);

    /*
     *  Find the parent node
     */
    json_t *parent_node = treedb_get_node( // Return is NOT YOURS, pure node
        tranger,
        treedb_name,
        parent_topic_name,
        parent_id
    );
    if(!parent_node) {
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_PARAMETER_ERROR,
            "msg",                  "%s", "Node not found",
            "treedb_name",          "%s", treedb_name,
            "child_topic_name",     "%s", child_topic_name,
            "child_id",             "%s", child_id,
            "parent_topic_name",    "%s", parent_topic_name,
            "parent_id",            "%s", parent_id,
            "fkey_col_name",        "%s", fkey_col_name,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  In parent hook data: save child content
     *  WARNING repeated in
     *      _link_nodes() and link_child_to_parent()
     *--------------------------------------------------*/
    json_t *parent_hook_data = kw_get_dict_value(gobj, parent_node, hook_name, 0, 0);
    if(!parent_hook_data) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_TREEDB_ERROR,
            "msg",              "%s", "hook field not found",
            "parent_node",      "%j", parent_node,
            "hook_name",        "%s", hook_name,
            NULL
        );
        gobj_trace_json(gobj, parent_node, "hook field not found");
        return -1;
    }

    json_t *child_data = kw_get_dict_value(gobj, child_node, fkey_col_name, 0, 0);
    if(!child_data) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "fkey field not found in the node",
            "child_topic_name",     "%s", child_topic_name,
            "child_id",             "%s", child_id,
            "parent_topic_name",    "%s", parent_topic_name,
            "parent_id",            "%s", parent_id,
            "fkey_col_name",        "%s", fkey_col_name,
            NULL
        );
        gobj_trace_json(gobj, child_node, "fkey field not found in the node");
        return -1;
    }

    switch(json_typeof(parent_hook_data)) { // json_typeof PROTECTED
    case JSON_ARRAY:
        {
            if(is_child_hook) {
                json_array_append(parent_hook_data, child_data);

            } else {
                json_array_append(parent_hook_data, child_node);
            }
        }
        break;
    case JSON_OBJECT:
        {
            char pref_[NAME_MAX];
            if(is_child_hook) {
                snprintf(pref_, sizeof(pref_), "%s~%s~%s",
                    child_topic_name,
                    child_id,
                    fkey_col_name
                );
                json_object_set(parent_hook_data, pref, child_data);
            } else {
                json_object_set(parent_hook_data, child_id, child_node);
            }
        }
        break;
    default:
        {
            gobj_log_error(gobj, 0,
                "function",             "%s", __FUNCTION__,
                "msgset",               "%s", MSGSET_TREEDB_ERROR,
                "msg",                  "%s", "Review the scheme: wrong parent hook type",
                "parent_topic_name",    "%s", parent_topic_name,
                "link",                 "%s", hook_name,
                NULL
            );
        }
        return -1;
    }

    return 0;
}

/***************************************************************************
 *  Load links (child's fkeys to parent's hooks)
 *  Use from disk to memory only
 ***************************************************************************/
PRIVATE int load_links(
    hgobj gobj,
    json_t *tranger,
    json_t *child_node
)
{
    int ret = 0;

    const char *treedb_name = kw_get_str(gobj, child_node, "__md_treedb__`treedb_name", 0, KW_REQUIRED);
    const char *topic_name = kw_get_str(gobj, child_node, "__md_treedb__`topic_name", 0, KW_REQUIRED);

    json_t *cols = tranger2_dict_topic_desc_cols(
        tranger,
        topic_name
    );

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        BOOL is_child_hook = kw_has_word(gobj, desc_flag, "hook", 0)?TRUE:FALSE;
        BOOL is_fkey = kw_has_word(gobj, desc_flag, "fkey", 0)?TRUE:FALSE;
        if(!is_fkey) {
            continue;
        }

        json_t *fkey_desc = kwid_get(gobj, col, 0, "fkey");
        if(!fkey_desc) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Child node without fkey field",
                "treedb_name",  "%s", treedb_name,
                "topic_name",   "%s", topic_name,
                "col_name",     "%s", col_name,
                NULL
            );
            gobj_trace_json(gobj, col, "Not explicit link done; child node without fkey field descriptor: col desc");
            gobj_trace_json(gobj, child_node, "Not explicit link done; child node without fkey field descriptor: node");
            ret += -1;
            continue;
        }

        /*
         *  Get fkeys from kw fkey field
         */
        // **FKEY**
        json_t *fkeys = kw_get_dict_value(gobj, child_node, col_name, 0, 0);
        if(!fkeys) {
            continue;
        }
        switch(json_typeof(fkeys)) {
        case JSON_ARRAY:
            {
                int idx; json_t *fkey;
                json_array_foreach(fkeys, idx, fkey) {
                    const char *pref = json_string_value(fkey);

                    ret += link_child_to_parent(
                        tranger,
                        treedb_name,
                        pref,
                        child_node,
                        col_name,
                        fkey_desc,
                        is_child_hook
                    );
                }
            }
            break;
        case JSON_OBJECT:
            {
                const char *pref; json_t *fkey;
                json_object_foreach(fkeys, pref, fkey) {
                    ret += link_child_to_parent(
                        tranger,
                        treedb_name,
                        pref,
                        child_node,
                        col_name,
                        fkey_desc,
                        is_child_hook
                    );
                }
            }
            break;
        case JSON_STRING:
            {
                const char *pref = json_string_value(fkeys);

                ret += link_child_to_parent(
                    tranger,
                    treedb_name,
                    pref,
                    child_node,
                    col_name,
                    fkey_desc,
                    is_child_hook
                );
            }
            break;
        default:
            continue;
        }
    }

    json_decref(cols);
    return ret;
}

/***************************************************************************
 *  Load links from childs to parents  (child's fkeys to parent's hooks)
 ***************************************************************************/
PRIVATE int load_all_links(
    hgobj gobj,
    json_t *tranger,
    const char *treedb_name
)
{
    int ret = 0;

    /*
     *  Loop topics, as child nodes
     */
    json_t *topics = treedb_topics(tranger, treedb_name, 0);
    int idx; json_t *jn_topic;
    json_array_foreach(topics, idx, jn_topic) {
        const char *topic_name = json_string_value(jn_topic);

        /*
         *  Loop nodes searching links
         */
        /*----------------------------------*
         *  Get indexx: to load links
         *----------------------------------*/
        json_t *indexx = treedb_get_id_index(tranger, treedb_name, topic_name);
        if(!indexx) {
            // It's not a treedb topic
            continue;
        }

        const char *child_id; json_t *child_node;
        json_object_foreach(indexx, child_id, child_node) {
            json_t *__md_treedb__ = kw_get_dict(gobj, child_node, "__md_treedb__", 0, KW_REQUIRED);
            BOOL __pending_links__ = kw_get_bool(
                gobj,
                __md_treedb__,
                "__pending_links__",
                0,
                KW_EXTRACT
            );
            if(!__pending_links__) {
                continue;
            }

            /*
             *  Loop desc cols searching fkey
             */
            ret += load_links(
                gobj,
                tranger,
                child_node
            );
        }
    }

    JSON_DECREF(topics)
    return ret;
}

/***************************************************************************
    Being `ids` a:

        {
            "$id": {
                "id": "$id",
                ...
            }
            ...
        }

        [
            {
                "id":$id,
                ...
            },
            ...
        ]

    return a new list of all node's mix_id
 ***************************************************************************/
PRIVATE json_t *get_hook_refs(
    hgobj gobj,
    json_t *hook_data // NOT owned
)
{
    char mix_id[NAME_MAX];
    json_t *refs = json_array();

    switch(json_typeof(hook_data)) {
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
            json_object_foreach(hook_data, id, jn_value) {
                int ampersands = count_char(id, '^');
                int tildes = count_char(id, '~');

                if(tildes == 2) {
                    json_array_append_new(refs, json_string(id));
                    continue;
                } else if(ampersands == 2) {
                    continue;
                }

                const char *topic_name = kw_get_str(
                    gobj,
                    jn_value,
                    "__md_treedb__`topic_name",
                    0,
                    0
                );
                if(!topic_name) {
                    gobj_log_error(gobj, 0,
                        "function",             "%s", __FUNCTION__,
                        "msgset",               "%s", MSGSET_TREEDB_ERROR,
                        "msg",                  "%s", "__md_treedb__ not found",
                        NULL
                    );
                    gobj_trace_json(gobj, jn_value, "__md_treedb__ not found: value");
                    gobj_trace_json(gobj, hook_data, "__md_treedb__ not found: hook_data");
                    continue;
                }
                snprintf(mix_id, sizeof(mix_id), "%s^%s", topic_name, id);
                json_array_append_new(refs, json_string(mix_id));
            }
        }
        break;

    case JSON_ARRAY:
        {
            int idx; json_t *jn_value;
            json_array_foreach(hook_data, idx, jn_value) {
                switch(json_typeof(jn_value)) {
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
                        const char *id = kw_get_str(gobj, jn_value, "id", 0, 0);
                        if(id) {
                            const char *topic_name = kw_get_str(
                                gobj,
                                jn_value,
                                "__md_treedb__`topic_name",
                                0,
                                0
                            );
                            if(!topic_name) {
                                gobj_log_error(gobj, 0,
                                    "function",             "%s", __FUNCTION__,
                                    "msgset",               "%s", MSGSET_TREEDB_ERROR,
                                    "msg",                  "%s", "__md_treedb__ not found",
                                    NULL
                                );
                                gobj_trace_json(gobj, jn_value, "__md_treedb__ not found: value");
                                gobj_trace_json(gobj, hook_data, "__md_treedb__ not found: hook_data");
                                break;
                            }
                            snprintf(mix_id, sizeof(mix_id), "%s^%s", topic_name, id);
                            json_array_append_new(refs, json_string(mix_id));
                        }
                    }
                    break;
                case JSON_STRING:
                    {
                        int ampersands = count_char(json_string_value(jn_value), '^');
                        int tildes = count_char(json_string_value(jn_value), '~');

                        if(tildes == 2) {
                            json_array_append(refs, jn_value);
                            break;
                        } else if(ampersands == 2) {
                            break;
                        }
                    }
                default:
                    gobj_log_error(gobj, 0,
                        "function",             "%s", __FUNCTION__,
                        "msgset",               "%s", MSGSET_TREEDB_ERROR,
                        "msg",                  "%s", "wrong array child hook type",
                        NULL
                    );
                    gobj_trace_json(gobj, jn_value, "wrong array child hook type: value");
                    gobj_trace_json(gobj, hook_data, "wrong array child hook type: hook data");
                    break;
                }
            }
        }
        break;

    default:
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "wrong child hook type",
            "hook_data",            "%j", hook_data,
            NULL
        );
        break;
    }

    return refs;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *get_fkey_refs(
    json_t *field_data // NOT owned
)
{
    json_t *refs = json_array();

    switch(json_typeof(field_data)) { // json_typeof PROTECTED
    case JSON_STRING:
        {
            if(count_char(json_string_value(field_data), '^')==2) {
                json_array_append(refs, field_data);
            }
        }
        break;
    case JSON_ARRAY:
        {
            int idx; json_t *ref;
            json_array_foreach(field_data, idx, ref) {
                if(json_typeof(ref)==JSON_STRING) {
                    if(count_char(json_string_value(ref), '^')==2) { // ==2 is 3 items!
                        json_array_append(refs, ref);
                    }
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            const char *ref; json_t *v;
            json_object_foreach(field_data, ref, v) {
                if(count_char(ref, '^')==2) {
                    json_array_append_new(refs, json_string(ref));
                }
            }
        }
        break;
    default:
        break;
    }

    return refs;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL json_empty(json_t *value)
{
    if(!value) {
        return TRUE;
    }
    switch(json_typeof(value)) {
    case JSON_ARRAY:
        return json_array_size(value)==0?TRUE:FALSE;
    case JSON_OBJECT:
        return json_object_size(value)==0?TRUE:FALSE;
    case JSON_STRING:
        return strlen(json_string_value(value))==0?TRUE:FALSE;
    default:
        return TRUE;
    }
}

/***************************************************************************
 *  Used in delete_node to get all down refs
 ***************************************************************************/
PRIVATE json_t *get_node_down_refs(  // Return MUST be decref
    hgobj gobj,
    json_t *tranger,
    json_t *node    // NOT owned
)
{
    json_t *refs = json_array();

    const char *treedb_name = kw_get_str(gobj, node, "__md_treedb__`treedb_name", 0, KW_REQUIRED);
    const char *topic_name = kw_get_str(gobj, node, "__md_treedb__`topic_name", 0, KW_REQUIRED);
    json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name);

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        BOOL is_hook = kw_has_word(gobj, desc_flag, "hook", 0)?TRUE:FALSE;
        if(!is_hook) {
            continue;
        }

        json_t *field_data = kw_get_dict_value(gobj, node, col_name, 0, 0);
        if(!field_data) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "field not found in the node 1",
                "treedb_name",  "%s", treedb_name,
                "topic_name",   "%s", topic_name,
                "field",        "%s", col_name,
                NULL
            );
            gobj_trace_json(gobj, node, "field not found in the node 1");
        }
        if(json_empty(field_data)) {
            continue;
        }

        json_t *child_list = get_hook_refs(gobj, field_data);
        json_array_extend(refs, child_list);
        json_decref(child_list);
    }

    json_decref(cols);
    return refs;
}

/***************************************************************************
 *  Used in delete_node to get all up refs
 ***************************************************************************/
PRIVATE json_t *get_node_up_refs(  // Return MUST be decref
    hgobj gobj,
    json_t *tranger,
    json_t *node    // NOT owned
)
{
    json_t *refs = json_array();

    const char *treedb_name = kw_get_str(gobj, node, "__md_treedb__`treedb_name", 0, KW_REQUIRED);
    const char *topic_name = kw_get_str(gobj, node, "__md_treedb__`topic_name", 0, KW_REQUIRED);
    json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name);

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        BOOL is_fkey = kw_has_word(gobj, desc_flag, "fkey", 0)?TRUE:FALSE;
        if(!is_fkey) {
            continue;
        }

        json_t *field_data = kw_get_dict_value(gobj, node, col_name, 0, 0);
        if(!field_data) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "field not found in the node 2",
                "treedb_name",  "%s", treedb_name,
                "topic_name",   "%s", topic_name,
                "field",        "%s", col_name,
                NULL
            );
            gobj_trace_json(gobj, node, "field not found in the node 2");
        }

        if(json_empty(field_data)) {
            continue;
        }

        json_t *ref = get_fkey_refs(field_data);
        json_array_extend(refs, ref);
        JSON_DECREF(ref)
    }

    JSON_DECREF(cols)
    return refs;
}

/***************************************************************************

 ***************************************************************************/
PUBLIC int treedb_set_trace(BOOL set)
{
    BOOL old = treedb_trace;
    treedb_trace = set?1:0;
    return old;
}

/***************************************************************************
    If path is empty then use kw
 ***************************************************************************/
PUBLIC json_t *get_hook_list(
    hgobj gobj,
    json_t *hook_data // NOT owned
)
{
    json_t *new_list = 0;

    switch(json_typeof(hook_data)) {
    case JSON_ARRAY:
        {
            new_list = hook_data;
            json_incref(new_list);
        }
        break;
    case JSON_OBJECT:
        {
            new_list = json_array();
            const char *key; json_t *v;
            json_object_foreach(hook_data, key, v) {
                json_array_append(new_list, v);
            }
        }
        break;
    case JSON_STRING:
// TODO un hook type string no funciona, revisa
//         {
//             new_list = json_array();
//             json_array_append_new(new_list, json_string(json_string_value(hook_data)));
//         }
//         break;
    default:
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "wrong type for hook list",
            NULL
        );
        break;
    }

    return new_list;
}




                    /*------------------------------------*
                     *      Manage the tree's nodes
                     *------------------------------------*/




/***************************************************************************
 *  Copy inherit fields FROM primary_node TO secondary_node
 ***************************************************************************/
PRIVATE BOOL copy_inherit_fields(
    hgobj gobj,
    json_t *tranger,
    const char *topic_name,
    json_t *secondary_node, // NOT owned
    json_t *primary_node  // NOT owned
)
{
    BOOL ret = FALSE;

    json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name);
    if(!cols) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic without cols",
            "topic_name",   "%s", topic_name,
            NULL
        );
        return FALSE;
    }

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        BOOL is_inherit = kw_has_word(gobj, desc_flag, "inherit", 0)?TRUE:FALSE;
        if(!is_inherit) {
            continue;
        }

        /*
         *  Copy inherit field
         */
        json_t *cell = kw_get_dict_value(gobj, primary_node, col_name, 0, 0);
        if(cell) {
            json_object_set(secondary_node, col_name, cell);
            ret = TRUE;
        }
    }

    JSON_DECREF(cols)
    return ret;
}

/***************************************************************************
 *  Inherit links FROM primary_node TO secondary_node. In collapse mode.
 ***************************************************************************/
PRIVATE BOOL inherit_links(
    hgobj gobj,
    json_t *tranger,
    const char *topic_name,
    json_t *secondary_node, // NOT owned
    json_t *primary_node  // NOT owned
)
{
    BOOL ret = FALSE;

    json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name);
    if(!cols) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic without cols",
            "topic_name",   "%s", topic_name,
            NULL
        );
        return FALSE;
    }

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        BOOL is_fkey = kw_has_word(gobj, desc_flag, "fkey", 0)?TRUE:FALSE;
        if(!is_fkey) {
            continue;
        }

        /*
         *  Copy fkeys
         */
        json_t *fkeys = treedb_parent_refs(
            tranger,
            col_name,
            primary_node,
            json_pack("{s:b}", "refs", 1)
        );
        json_t *cell = kw_get_dict_value(gobj, secondary_node, col_name, 0, KW_REQUIRED);
        if(cell) {
            int idx; json_t *fkey;
            json_array_foreach(fkeys, idx, fkey) {
                // **FKEY**
                const char *ref = json_string_value(fkey);
                if(json_is_array(cell)) {
                    json_array_append_new(cell, json_string(ref));
                    ret = TRUE;
                } else if(json_is_string(cell)) {
                    json_object_set_new(secondary_node, col_name, json_string(ref));
                    ret = TRUE;
                }
            }
        }

        json_decref(fkeys);
    }

    JSON_DECREF(cols)
    return ret;
}

/***************************************************************************
    Create a new node
 ***************************************************************************/
PUBLIC json_t *treedb_create_node( // WARNING Return is NOT YOURS, pure node
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *kw // owned
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(tranger, treedb_name, topic_name)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    /*-------------------------------*
     *  Get the id to create node
     *  it's mandatory
     *-------------------------------*/
    const char *id = kw_get_str(gobj, kw, "id", 0, 0);
    if(empty_string(id)) {
        json_t *id_col_flag = kwid_get(gobj,
            tranger,
            KW_VERBOSE,
            "topics`%s`cols`id`flag",
                topic_name
        );
        if(kw_has_word(gobj, id_col_flag, "uuid", 0)) {
            char uuid[RECORD_KEY_VALUE_MAX];
            create_uuid(uuid, sizeof(uuid));
            id = uuid;
            json_object_set_new(kw, "id", json_string(id));
        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Field 'id' required",
                "topic_name",   "%s", topic_name,
                NULL
            );
            gobj_trace_json(gobj, kw, "Field 'id' required");
            JSON_DECREF(kw)
            return 0;
        }
    }

    /*-------------------------------*
     *  Get indexx: to create node
     *-------------------------------*/
    json_t *indexx = treedb_get_id_index(tranger, treedb_name, topic_name);

    /*-----------------------------------*
     *  Single: exists already the id?
     *-----------------------------------*/
    BOOL save_pkey = FALSE;
    BOOL save_id = FALSE;
    json_t *pkey2_list = json_array(); // collect pkeys to save

    json_t *prev_record = exist_primary_node(indexx, id);
    if(!prev_record) {
        save_id = TRUE;
    }
    /*-----------------------------------*
     *  Look for a secondary key change
     *-----------------------------------*/
    json_t *iter_pkey2s = treedb_topic_pkey2s(tranger, topic_name);
    int idx; json_t *jn_pkey2_name;
    json_array_foreach(iter_pkey2s, idx, jn_pkey2_name) {
        const char *pkey2_name = json_string_value(jn_pkey2_name);
        if(empty_string(pkey2_name)) {
            continue;
        }
        /*--------------------------------------------*
         *  Get indexy: check exists to create node
         *--------------------------------------------*/
        json_t *indexy = treedb_get_pkey2_index(
            tranger,
            treedb_name,
            topic_name,
            pkey2_name
        );
        const char *pkey2_value = get_key2_value(
            tranger,
            topic_name,
            pkey2_name,
            kw
        );
        if(!exist_secondary_node(indexy, id, pkey2_value)) {
            // Not exist
            save_pkey = TRUE;
            json_array_append_new(pkey2_list, json_string(pkey2_name));
        }
    }
    JSON_DECREF(iter_pkey2s)

    if(!save_id && !save_pkey) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Node already exists",
            "topic_name",   "%s", topic_name,
            "id",           "%s", id,
            NULL
        );
        JSON_DECREF(pkey2_list)
        JSON_DECREF(kw)
        return 0;
    }

    /*----------------------------------------*
     *  Create the tranger record to create
     *----------------------------------------*/
    json_t *record = record2tranger(gobj, tranger, topic_name, kw, TRUE);
    if(!record) {
        // Error already logged
        JSON_DECREF(pkey2_list)
        JSON_DECREF(kw)
        return 0;
    }
    BOOL links_inherited = FALSE;
    if(save_pkey && prev_record) {
        /*
         *  Si es un nodo secundario, copia los links del primario.
         */
        links_inherited = inherit_links(gobj, tranger, topic_name, record, prev_record);

        /*
         *  Si es un nodo secundario, copia los inherit fields
         */
        copy_inherit_fields(gobj, tranger, topic_name, record, prev_record);
    }

    /*-------------------------------*
     *  Write to tranger (Creating)
     *-------------------------------*/
    md2_record_t md_record;
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
        JSON_DECREF(pkey2_list)
        JSON_DECREF(kw)
        JSON_DECREF(record)
        return 0;
    }

    /*--------------------------------------------------*
     *  Set volatil data
     *  HACK set volatil after append record:
     *      Volatil data must not be save in file!
     *--------------------------------------------------*/
    set_volatil_values( // crea campos vacios o con los valores de kw
        tranger,
        topic_name,
        record,  // NOT owned
        kw // NOT owned
    );

    /*--------------------------------------------*
     *  Build metadata, creating node in memory
     *--------------------------------------------*/
    json_int_t rowid = json_integer_value(
        json_object_get(
            json_object_get(record, "__md_tranger__"),
            "rowid"
        )
    );
    json_t *jn_record_md = _md2json(
        treedb_name,
        topic_name,
        &md_record,
        rowid
    );
    json_object_set_new(record, "__md_treedb__", jn_record_md);

    /*---------------------------------------------------*
     *  Si tienes la marca grupo, pasas, eres el activo.
     *  Si no tienes la marca,
     *  pasas a la cola secundaria de instancias,
     *  pasas a existir por la clave secundaria,
     *  si eres la última instancia claro.
     *  En este mundo sólo hay un activo,
     *  o el que tiene la marca de grupo,
     *  o el último en llegar si no hay marca de grupo.
     *  El resto son instancias,
     *  que viven por su identificación secundaria.
     *  En repetición de claves secundarias,
     *  la última es que prevalece y existe.
     *---------------------------------------------------*/

    /*-------------------------------*
     *  Build links to hooks
     *-------------------------------*/
    if(links_inherited) {
        load_links(gobj, tranger, record);
    }

    /*-------------------------------*
     *  Get callback
     *-------------------------------*/
    json_t *treedb = kwid_get(gobj, tranger, 0, "treedbs`%s", treedb_name);
    treedb_callback_t treedb_callback =
        (treedb_callback_t)(size_t)kw_get_int(gobj,
        treedb,
        "__treedb_callback__",
        0,
        0
    );
    void *user_data =
        (void *)(size_t)kw_get_int(gobj,
        treedb,
        "__treedb_callback_user_data__",
        0,
        0
    );

    /*-------------------------------*
     *  Write node in memory: id
     *-------------------------------*/
    if(save_id) {
        add_primary_node(indexx, id, record);

        /*----------------------------------*
         *  Call Callback
         *----------------------------------*/
        if(treedb_callback) {
            /*
             *  Inform user in real time
             */
            JSON_INCREF(record)
            treedb_callback(
                user_data,
                tranger,
                treedb_name,
                topic_name,
                "EV_TREEDB_NODE_CREATED",
                record
            );
            treedb_callback = 0; // Not inform more
        }
    }

    /*-------------------------------*
     *  Write node in memory: pkey2
     *-------------------------------*/
    if(save_pkey) {
        int idx2; json_t *jn_pkey2;
        json_array_foreach(pkey2_list, idx2, jn_pkey2) {
            const char *pkey2_name = json_string_value(jn_pkey2);
            /*---------------------------------*
             *  Get indexy: to create node
             *---------------------------------*/
            json_t *indexy = treedb_get_pkey2_index(
                tranger,
                treedb_name,
                topic_name,
                pkey2_name
            );
            if(!indexy) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "TreeDb Topic indexy NOT FOUND",
                    "topic_name",   "%s", topic_name,
                    "pkey2_name",   "%s", pkey2_name,
                    NULL
                );
                continue;
            }
            const char *pkey2_value = get_key2_value(
                tranger,
                topic_name,
                pkey2_name,
                kw
            );

            /*-------------------------------*
             *  Write node in memory: pkey2
             *-------------------------------*/
            add_secondary_node(indexy, id, pkey2_value, record);

            /*----------------------------------*
             *  Call Callback
             *----------------------------------*/
            if(treedb_callback) {
                /*
                 *  Inform user in real time
                 */
                JSON_INCREF(record)
                treedb_callback(
                    user_data,
                    tranger,
                    treedb_name,
                    topic_name,
                    "EV_TREEDB_NODE_CREATED",
                    record
                );
            }
        }
    }

    /*-------------------------------*
     *  Trace
     *-------------------------------*/
    if(treedb_trace) {
        gobj_trace_json(gobj, record, "treedb_create_node: Ok (%s, %s)", treedb_name, topic_name);
    }

    json_decref(record);
    JSON_DECREF(pkey2_list)
    JSON_DECREF(kw)
    return record;
}

/***************************************************************************
 *  Direct saving to tranger.
    Tag __tag__ (user_flag) is inherited.
 ***************************************************************************/
PUBLIC int treedb_save_node(
    json_t *tranger,
    json_t *node    // NOT owned, WARNING be care, must be a pure node.
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*------------------------------*
     *      Check original node
     *------------------------------*/
    if(!kw_get_bool(gobj, node, "__md_treedb__`__pure_node__", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Not a pure node",
            NULL
        );
        gobj_trace_json(gobj, node, "Not a pure node");
        return -1;
    }

    /*-------------------------------*
     *      Get node info
     *-------------------------------*/
    const char *treedb_name = kw_get_str(gobj, node, "__md_treedb__`treedb_name", 0, 0);
    const char *topic_name = kw_get_str(gobj, node, "__md_treedb__`topic_name", 0, 0);

    /*---------------------------------------*
     *  Create the tranger record to update
     *---------------------------------------*/
    json_t *record = record2tranger(gobj, tranger, topic_name, node, FALSE);
    if(!record) {
        // Error already logged
        return -1;
    }

    /*-------------------------------------*
     *  Write to tranger (save, updating)
     *-------------------------------------*/
    uint32_t tag = kw_get_int(gobj, node, "__md_treedb__`__tag__", 0, KW_REQUIRED);

    JSON_INCREF(record)
    md2_record_t md_record;
    int ret = tranger2_append_record(
        tranger,
        topic_name,
        0, // __t__,         // if 0 then the time will be set by TimeRanger with now time
        tag, // user_flag,
        &md_record, // md_record,
        record // owned
    );
    if(ret < 0) {
        // Error already logged
        return -1;
    }

    /*--------------------------------------------*
     *  Build metadata, update node in memory
     *  HACK only numeric fields! strings cannot
     *--------------------------------------------*/
    json_t *__md_treedb__ = json_object_get(node, "__md_treedb__");
    json_object_set_new(__md_treedb__,
        "__t__",
        json_integer(md_record.__t__)
    );
    json_object_set_new(__md_treedb__,
        "__tm__",
        json_integer(md_record.__tm__)
    );

    /*-------------------------------*
     *  Get callback
     *-------------------------------*/
    json_t *treedb = kwid_get(gobj, tranger, 0, "treedbs`%s", treedb_name);
    treedb_callback_t treedb_callback =
        (treedb_callback_t)(size_t)kw_get_int(gobj,
        treedb,
        "__treedb_callback__",
        0,
        0
    );
    void *user_data =
        (void *)(size_t)kw_get_int(gobj,
        treedb,
        "__treedb_callback_user_data__",
        0,
        0
    );

    /*----------------------------------*
     *  Call Callback
     *----------------------------------*/
    if(treedb_callback) {
        /*
         *  Inform user in real time
         */
        JSON_INCREF(node)
        treedb_callback(
            user_data,
            tranger,
            treedb_name,
            topic_name,
            "EV_TREEDB_NODE_UPDATED",
            node
        );
        treedb_callback = 0; // Not inform more
    }

    /*-------------------------------*
     *  Trace
     *-------------------------------*/
    if(treedb_trace) {
        gobj_trace_json(gobj, node, "treedb_save_node: Ok");
    }

    JSON_DECREF(record)

    return 0;
}

/***************************************************************************
    Update the existing current node with fields of kw
    HACK fkeys and hook fields are not updated!
 ***************************************************************************/
PUBLIC json_t *treedb_update_node( // WARNING Return is NOT YOURS, pure node
    json_t *tranger,
    json_t *node,   // NOT owned, WARNING be care, must be a pure node.
    json_t *kw,     // owned
    BOOL save
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*------------------------------*
     *      Check original node
     *------------------------------*/
    if(!kw_get_bool(gobj, node, "__md_treedb__`__pure_node__", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Not a pure node",
            NULL
        );
        gobj_trace_json(gobj, node, "Not a pure node");
        KW_DECREF(kw)
        return 0;
    }

    /*-------------------------------*
     *      Get node info
     *-------------------------------*/
    const char *topic_name = kw_get_str(gobj, node, "__md_treedb__`topic_name", 0, 0);

    /*-------------------------------*
     *  Update fields
     *-------------------------------*/
    json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name);
    if(!cols) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic without cols",
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(kw)
        return 0;
    }

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        BOOL is_fkey = kw_has_word(gobj, desc_flag, "fkey", 0)?TRUE:FALSE;
        BOOL is_hook = kw_has_word(gobj, desc_flag, "hook", 0)?TRUE:FALSE;
        if(!(is_fkey || is_hook)) {
            json_t *new_value = kw_get_dict_value(gobj, kw, col_name, 0, 0);
            if(new_value) {
                json_object_set(node, col_name, new_value);
            }
        }
    }

    JSON_DECREF(cols)

    /*-------------------------------*
     *  Write to tranger
     *-------------------------------*/
    if(save) {
        treedb_save_node(tranger, node);
    }

    KW_DECREF(kw)
    return node;
}

/***************************************************************************
    "force" delete links.
    If there are links and not force then delete_node will fail
    WARNING that kw can be node, the node to delete!!

    HACK: delete will be delete the record forever, for that reason,
          a node with snap tag cannot be delete!

 ***************************************************************************/
PUBLIC int treedb_delete_node(
    json_t *tranger,
    json_t *node,       // owned, pure node
    json_t *jn_options  // bool "force"
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*------------------------------*
     *      Check original node
     *------------------------------*/
    if(!kw_get_bool(gobj, node, "__md_treedb__`__pure_node__", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Not a pure node",
            NULL
        );
        gobj_trace_json(gobj, node, "Not a pure node");
        JSON_DECREF(jn_options)
        JSON_DECREF(node)
        return -1;
    }

    /*-------------------------------*
     *      Get node info
     *-------------------------------*/
    const char *treedb_name = kw_get_str(gobj, node, "__md_treedb__`treedb_name", 0, 0);
    const char *topic_name = kw_get_str(gobj, node, "__md_treedb__`topic_name", 0, 0);
    const char *id = kw_get_str(gobj, node, "id", "", 0);
    BOOL force = kw_get_bool(gobj, jn_options, "force", 0, KW_WILD_NUMBER);

    /*-------------------------------*
     *      Get record info
     *-------------------------------*/
    json_int_t __rowid__ = kw_get_int(gobj, node, "__md_treedb__`__rowid__", 0, KW_REQUIRED);
    json_int_t __tag__ = kw_get_int(gobj, node, "__md_treedb__`__tag__", 0, KW_REQUIRED);
    if(__tag__ && !force) {
        // añade opción de borrar un snap que desmarque los nodos?
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "cannot delete node, it has a tag",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            "id",           "%s", id,
            NULL
        );
        JSON_DECREF(jn_options)
        return -1;
    }

    /*-------------------------------*
     *  Check hooks and fkeys
     *-------------------------------*/
    BOOL to_delete = TRUE;

    /*-------------------------------*
     *      Childs
     *-------------------------------*/
    json_t *down_refs = get_node_down_refs(gobj, tranger, node);
    if(json_array_size(down_refs)>0) {
        if(force) {
            json_t *jn_hooks = treedb_get_topic_hooks(
                tranger,
                treedb_name,
                topic_name
            );
            int idx2; json_t *jn_hook;
            json_array_foreach(jn_hooks, idx2, jn_hook) {
                const char *hook = json_string_value(jn_hook);
                json_t *childs = _list_childs(
                    gobj,
                    tranger,
                    hook,
                    node
                );
                int idx3; json_t *child;
                json_array_foreach(childs, idx3, child) {
                    _unlink_nodes(gobj, tranger, hook, node, child);
                }
                JSON_DECREF(childs)
            }
            JSON_DECREF(jn_hooks)

            /*
             *  Re-checks down links
             */
            json_t *down_refs_ = get_node_down_refs(gobj, tranger, node);
            if(json_array_size(down_refs_)>0) {
                to_delete = FALSE;
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Cannot delete node: still has down links",
                    "topic_name",   "%s", topic_name,
                    "id",           "%s", id,
                    NULL
                );
            }
            JSON_DECREF(down_refs_);

        } else {
            to_delete = FALSE;
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Cannot delete node: has down links",
                "topic_name",   "%s", topic_name,
                "id",           "%s", id,
                NULL
            );
        }
    }
    JSON_DECREF(down_refs)

    /*-------------------------------*
     *      Parents
     *-------------------------------*/
    json_t *up_refs = get_node_up_refs(gobj, tranger, node);
    if(json_array_size(up_refs)>0) {
        if(force) {
            if(treedb_clean_node(tranger, node, FALSE)<0) {
                to_delete = FALSE;
            }
        } else {
            to_delete = FALSE;
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Cannot delete node: has up links",
                "topic_name",   "%s", topic_name,
                "id",           "%s", id,
                NULL
            );
        }
    }
    JSON_DECREF(up_refs)

    if(!to_delete) {
        // Error already logged
        JSON_DECREF(jn_options)
        return -1;
    }

    /*-------------------------------------------------*
     *  Delete the record
     *  HACK cannot use tranger_delete_record()
     *  List of deleted id's in memory
     *  (borrar un id record en tranger, y el resto?)
     *-------------------------------------------------*/
    if(tranger2_delete_soft_record(tranger, topic_name, __rowid__, TRUE)==0) {
        /*-------------------------------*
         *  Trace
         *-------------------------------*/
        if(treedb_trace) {
            gobj_trace_msg(gobj, "delete node, topic %s, id %s", topic_name, id);
        }

        /*-------------------------------*
         *  Maintain node live
         *-------------------------------*/
        JSON_INCREF(node)

        /*-------------------------------*
         *  Get indexx: to delete node
         *-------------------------------*/
        json_t *indexx = treedb_get_id_index(tranger, treedb_name, topic_name);
        if(delete_primary_node(indexx, id)<0) { // node owned
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "delete_primary_node() FAILED",
                "topic_name",   "%s", topic_name,
                "id",           "%s", id,
                NULL
            );
        }

        /*-------------------------------*
         *      Borra indexy data
         *-------------------------------*/
        json_t *iter_pkey2s = treedb_topic_pkey2s(tranger, topic_name);
        int idx; json_t *jn_pkey2_name;
        json_array_foreach(iter_pkey2s, idx, jn_pkey2_name) {
            const char *pkey2_name = json_string_value(jn_pkey2_name);
            if(empty_string(pkey2_name)) {
                continue;
            }

            /*---------------------------------*
             *  Get indexy: to delete node
             *---------------------------------*/
            json_t *indexy = treedb_get_pkey2_index(
                tranger,
                treedb_name,
                topic_name,
                pkey2_name
            );
            if(!indexy) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "TreeDb Topic indexy NOT FOUND",
                    "topic_name",   "%s", topic_name,
                    "pkey2_name",   "%s", pkey2_name,
                    NULL
                );
                continue;
            }

            // TODO estoy borrando todas las instancias. Y si tienen links?
            json_t * key2v = kw_get_dict_value(
                gobj,
                indexy,
                id,
                0,
                KW_EXTRACT
            );
            if(key2v) {
                json_decref(key2v);
            } else {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "delete pkey2 FAILED",
                    "topic_name",   "%s", topic_name,
                    "id",           "%s", id,
                    "pkey2_name",   "%s", pkey2_name,
                    NULL
                );
            }
        }
        JSON_DECREF(iter_pkey2s)

        /*
         *  Call Callback
         */
        json_t *treedb = kwid_get(gobj, tranger, 0, "treedbs`%s", treedb_name);

        treedb_callback_t treedb_callback =
            (treedb_callback_t)(size_t)kw_get_int(gobj,
            treedb,
            "__treedb_callback__",
            0,
            0
        );
        if(treedb_callback) {
            /*
             *  Inform user in real time
             */
            void *user_data =
                (treedb_callback_t)(size_t)kw_get_int(gobj,
                treedb,
                "__treedb_callback_user_data__",
                0,
                0
            );
            JSON_INCREF(node)
            treedb_callback(
                user_data,
                tranger,
                treedb_name,
                topic_name,
                "EV_TREEDB_NODE_DELETED",
                node
            );
        }

        /*-------------------------------*
         *  Kill the node
         *-------------------------------*/
        JSON_DECREF(node)

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot delete node",
            "topic_name",   "%s", topic_name,
            "id",           "%s", id,
            NULL
        );
        JSON_DECREF(jn_options)
        return -1;
    }

    JSON_DECREF(jn_options)
    return 0;
}

/***************************************************************************
    "force" delete links.
    If there are links and not force then delete_node will fail
    WARNING that kw can be node, the node to delete!!

    HACK: delete will be delete the record forever, for that reason,
          a node with snap tag cannot be delete!

TODO sin uso, a la espera de depurar bien los delete de instancias

 ***************************************************************************/
PUBLIC int treedb_delete_instance(
    json_t *tranger,
    json_t *node,       // owned, pure node
    const char *pkey2_name,
    json_t *jn_options  // bool "force"
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*------------------------------*
     *      Check original node
     *------------------------------*/
    if(!kw_get_bool(gobj, node, "__md_treedb__`__pure_node__", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Not a pure node",
            NULL
        );
        gobj_trace_json(gobj, node, "Not a pure node");
        JSON_DECREF(jn_options)
        JSON_DECREF(node)
        return -1;
    }

    /*-------------------------------*
     *      Get node info
     *-------------------------------*/
    const char *treedb_name = kw_get_str(gobj, node, "__md_treedb__`treedb_name", 0, 0);
    const char *topic_name = kw_get_str(gobj, node, "__md_treedb__`topic_name", 0, 0);
    const char *id = kw_get_str(gobj, node, "id", "", 0);
    BOOL force = kw_get_bool(gobj, jn_options, "force", 0, KW_WILD_NUMBER);

    /*-------------------------------*
     *      Get record info
     *-------------------------------*/
    json_int_t __rowid__ = kw_get_int(gobj, node, "__md_treedb__`__rowid__", 0, KW_REQUIRED);
    json_int_t __tag__ = kw_get_int(gobj, node, "__md_treedb__`__tag__", 0, KW_REQUIRED);
    if(__tag__ && !force) {
        // añade opción de borrar un snap que desmarque los nodos?
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "cannot delete node, it has a tag",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            "id",           "%s", id,
            NULL
        );
        JSON_DECREF(jn_options)
        JSON_DECREF(node)
        return -1;
    }

    /*-------------------------------*
     *  Check hooks and fkeys
     *-------------------------------*/
    BOOL to_delete = TRUE;

    /*-------------------------------*
     *      Childs
     *-------------------------------*/
    json_t *down_refs = get_node_down_refs(gobj, tranger, node);
    if(json_array_size(down_refs)>0) {
        if(force) {
            json_t *jn_hooks = treedb_get_topic_hooks(
                tranger,
                treedb_name,
                topic_name
            );
            int idx2; json_t *jn_hook;
            json_array_foreach(jn_hooks, idx2, jn_hook) {
                const char *hook = json_string_value(jn_hook);
                json_t *childs = _list_childs(
                    gobj,
                    tranger,
                    hook,
                    node
                );
                int idx3; json_t *child;
                json_array_foreach(childs, idx3, child) {
                    _unlink_nodes(gobj, tranger, hook, node, child);
                }
                JSON_DECREF(childs)
            }
            JSON_DECREF(jn_hooks)

            /*
             *  Re-checks down links
             */
            json_t *down_refs_ = get_node_down_refs(gobj, tranger, node);
            if(json_array_size(down_refs_)>0) {
                to_delete = FALSE;
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Cannot delete node: still has down links",
                    "topic_name",   "%s", topic_name,
                    "id",           "%s", id,
                    NULL
                );
            }
            JSON_DECREF(down_refs_)

        } else {
            to_delete = FALSE;
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Cannot delete node: has down links",
                "topic_name",   "%s", topic_name,
                "id",           "%s", id,
                NULL
            );
        }
    }
    JSON_DECREF(down_refs)

    /*-------------------------------*
     *      Parents
     *-------------------------------*/
    json_t *up_refs = get_node_up_refs(gobj, tranger, node);
    if(json_array_size(up_refs)>0) {
        if(force) {
            if(treedb_clean_node(tranger, node, FALSE)<0) {
                to_delete = FALSE;
            }
        } else {
            to_delete = FALSE;
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Cannot delete node: has up links",
                "topic_name",   "%s", topic_name,
                "id",           "%s", id,
                NULL
            );
        }
    }
    JSON_DECREF(up_refs)

    if(!to_delete) {
        // Error already logged
        JSON_DECREF(jn_options)
        JSON_DECREF(node)
        return -1;
    }

    /*-------------------------------------------------*
     *  Delete the record
     *  HACK cannot use tranger_delete_record()
     *  List of deleted id's in memory
     *  (borrar un id record en tranger, y el resto?)
     *-------------------------------------------------*/
    if(tranger2_delete_soft_record(tranger, topic_name, __rowid__, TRUE)==0) {
        /*-------------------------------*
         *  Trace
         *-------------------------------*/
        if(treedb_trace) {
            gobj_trace_msg(gobj, "delete instance node, topic %s, id %s", topic_name, id);
        }

        /*-------------------------------*
         *  Maintain node live
         *-------------------------------*/
        JSON_INCREF(node)

        /*---------------------------------*
            *  Get indexy: to delete node
            *---------------------------------*/
        json_t *indexy = treedb_get_pkey2_index(
            tranger,
            treedb_name,
            topic_name,
            pkey2_name
        );
        const char *pkey2_value = get_key2_value(
            tranger,
            topic_name,
            pkey2_name,
            node
        );
        if(delete_secondary_node(indexy, id, pkey2_value)<0) { // node owned
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "delete_secondary_node() FAILED",
                "topic_name",   "%s", topic_name,
                "pkey2_name",   "%s", pkey2_name,
                "id",           "%s", id,
                "key2",         "%s", pkey2_value,
                NULL
            );
        }

        /*
         *  Call Callback
         */
        json_t *treedb = kwid_get(gobj, tranger, 0, "treedbs`%s", treedb_name);

        treedb_callback_t treedb_callback =
            (treedb_callback_t)(size_t)kw_get_int(gobj,
            treedb,
            "__treedb_callback__",
            0,
            0
        );
        if(treedb_callback) {
            /*
             *  Inform user in real time
             */
            void *user_data =
                (treedb_callback_t)(size_t)kw_get_int(gobj,
                treedb,
                "__treedb_callback_user_data__",
                0,
                0
            );
            JSON_INCREF(node)
            treedb_callback(
                user_data,
                tranger,
                treedb_name,
                topic_name,
                "EV_TREEDB_NODE_DELETED",
                node
            );
        }

        /*-------------------------------*
         *  Kill the node
         *-------------------------------*/
        JSON_DECREF(node)

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot delete node",
            "topic_name",   "%s", topic_name,
            "id",           "%s", id,
            NULL
        );
        JSON_DECREF(jn_options)
        JSON_DECREF(node)
        return -1;
    }

    JSON_DECREF(jn_options)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int remove_wrong_up_ref(
    hgobj gobj,
    json_t *tranger,
    json_t *node,
    const char *topic_name,
    const char *col_name,
    const char *ref
)
{
    json_t *field_data = kw_get_dict_value(gobj, node, col_name, 0, 0);
    if(!field_data) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "field not found in the node 4",
            "topic_name",   "%s", topic_name,
            "field",        "%s", col_name,
            NULL
        );
        gobj_trace_json(gobj, node, "field not found in the node 4");
    }

    if(json_empty(field_data)) {
        return -1;
    }

    int ret = -1;

    switch(json_typeof(field_data)) { // json_typeof PROTECTED
    case JSON_STRING:
        {
            const char *ref_ = json_string_value(field_data);
            if(ref_ && ref && strcmp(ref_, ref)==0) {
                kw_set_dict_value(gobj, node, col_name, json_string(""));
                ret = 0;
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Removing wrong fkey ref",
                    "topic_name",   "%s", topic_name,
                    "field",        "%s", col_name,
                    "ref",          "%s", ref,
                    NULL
                );
            }
        }
        break;
    case JSON_ARRAY:
        {
            int idx; json_t *r;
            json_array_foreach(field_data, idx, r) {
                if(json_typeof(r)==JSON_STRING) {
                    const char *ref_ = json_string_value(r);
                    if(ref_ && ref && strcmp(ref_, ref)==0) {
                        json_array_remove(field_data, idx);
                        idx --;
                        ret = 0;
                        gobj_log_warning(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_TREEDB_ERROR,
                            "msg",          "%s", "Removing wrong fkey ref",
                            "topic_name",   "%s", topic_name,
                            "field",        "%s", col_name,
                            "ref",          "%s", ref,
                            NULL
                        );
                    }
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            const char *ref_; json_t *v; void *n;
            json_object_foreach_safe(field_data, n, ref_, v) {
                if(ref_ && ref && strcmp(ref_, ref)==0) {
                    json_object_del(field_data, ref_);
                    ret = 0;
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_TREEDB_ERROR,
                        "msg",          "%s", "Removing wrong fkey ref",
                        "topic_name",   "%s", topic_name,
                        "field",        "%s", col_name,
                        "ref",          "%s", ref,
                        NULL
                    );
                }
            }
        }
        break;
    default:
        break;
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int search_and_remove_wrong_up_ref(
    hgobj gobj,
    json_t *tranger,
    json_t *node,
    const char *topic_name,
    const char *ref
)
{
    json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name);
    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        BOOL is_fkey = kw_has_word(gobj, desc_flag, "fkey", 0)?TRUE:FALSE;
        if(!is_fkey) {
            continue;
        }
        remove_wrong_up_ref(
            gobj,
            tranger,
            node,
            topic_name,
            col_name,
            ref
        );
    }
    json_decref(cols);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _link_nodes(
    hgobj gobj,
    json_t *tranger,
    const char *hook_name,
    json_t *parent_node,    // NOT owned
    json_t *child_node      // NOT owned
)
{
    /*------------------------------*
     *      Check original node
     *------------------------------*/
    if(!kw_get_bool(gobj, parent_node, "__md_treedb__`__pure_node__", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot link nodes, Not a pure node",
            NULL
        );
        gobj_trace_json(gobj, parent_node, "Not a pure node");
        return -1;
    }

    /*------------------------------*
     *      Check original node
     *------------------------------*/
    if(!kw_get_bool(gobj, child_node, "__md_treedb__`__pure_node__", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot link nodes, Not a pure node",
            NULL
        );
        gobj_trace_json(gobj, child_node, "Not a pure node");
        return -1;
    }

    /*---------------------------------------*
     *  Check self link
     *---------------------------------------*/
    if(parent_node == child_node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot link self node",
            "parent_node",  "%j", parent_node,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  Check treedb_name's
     *--------------------------------------------------*/
    const char *parent_node_treedb_name = kw_get_str(gobj, parent_node, "__md_treedb__`treedb_name", 0, 0);
    const char *treedb_name = kw_get_str(gobj, child_node, "__md_treedb__`treedb_name", 0, 0);
    if(strcmp(parent_node_treedb_name, treedb_name)!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot link node's of different treedb",
            "treedb_parent","%s", parent_node_treedb_name,
            "treedb_child", "%s", treedb_name,
            NULL
        );
        return -1;
    }

    /*---------------------------------------*
     *  Check parent has `hook_name` field
     *---------------------------------------*/
    json_t *parent_hook_data = kw_get_dict_value(gobj, parent_node, hook_name, 0, 0);
    if(!parent_hook_data) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "hook field not found",
            "parent_node",  "%j", parent_node,
            "hook_name",    "%s", hook_name,
            NULL
        );
        gobj_trace_json(gobj, parent_node, "hook field not found");
        return -1;
    }

    /*--------------------------------------------------*
     *  Get info of parent/child from their metadata
     *--------------------------------------------------*/
    const char *parent_topic_name = json_string_value(
        kwid_get(gobj, parent_node, 0, "__md_treedb__`topic_name")
    );
    json_t *hook_col_desc = kwid_get(gobj,
        tranger,
        KW_VERBOSE,
        "topics`%s`%s`%s",
            parent_topic_name, "cols", hook_name
    );

    const char *child_topic_name = json_string_value(
        kwid_get(gobj, child_node, 0, "__md_treedb__`topic_name")
    );

    /*----------------------*
     *      Get hook desc
     *----------------------*/
    json_t *hook_desc = kw_get_dict(gobj, hook_col_desc, "hook", 0, 0);
    if(!hook_desc) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "hook field not define topic desc",
            "topic_name",   "%s", parent_topic_name,
            "hook",         "%s", hook_name,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  Check that this child topic is in the hook desc
     *
     *  Child topic name must be defined in hook links
     *  with his child_field (fkey or hook)
     *--------------------------------------------------*/
    const char *child_field = kw_get_str(gobj, hook_desc, child_topic_name, 0, 0);
    if(!child_field) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic child not defined in hook desc links",
            "topic_name",   "%s", parent_topic_name,
            "hook",         "%s", hook_name,
            "hook_desc",    "%j", hook_desc,
            "child_topic",  "%s", child_topic_name,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *      Get ids
     *--------------------------------------------------*/
    const char *parent_id = kw_get_str(gobj, parent_node, "id", 0, 0);
    if(!parent_id) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Parent 'id' not found",
            "parent_topic", "%s", parent_topic_name,
            "parent",       "%j", parent_node,
            NULL
        );
        return -1;
    }
    const char *child_id = kw_get_str(gobj, child_node, "id", 0, 0);
    if(!child_id) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Child 'id' not found",
            "child_topic",  "%s", child_topic_name,
            "child",        "%j", child_node,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  Reverse - child container with info of parent
     *--------------------------------------------------*/
    json_t *child_col_flag = kwid_get(gobj,
        tranger,
        KW_VERBOSE,
        "topics`%s`%s`%s`flag",
            child_topic_name, "cols", child_field
    );
    if(!child_col_flag) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "field child not define topic desc",
            "topic_name",   "%s", child_topic_name,
            "field",        "%s", child_field,
            NULL
        );
    }

    BOOL is_child_hook = kw_has_word(gobj, child_col_flag, "hook", 0)?TRUE:FALSE;

    json_t *child_data = kw_get_dict_value(gobj, child_node, child_field, 0, 0);
    if(!child_data) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "field not found in the node 5",
            "topic_name",   "%s", child_topic_name,
            "field",        "%s", child_field,
            NULL
        );
        gobj_trace_json(gobj, child_node, "field not found in the node 5");
        return -1;
    }

    /*--------------------------------------------------*
     *      Checks
     *--------------------------------------------------*/
    switch(json_typeof(parent_hook_data)) { // json_typeof PROTECTED
    case JSON_ARRAY:
        break;
    case JSON_OBJECT:
        break;
    default:
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "wrong parent hook type",
            "parent_topic_name",    "%s", parent_topic_name,
            "child_topic_name",     "%s", child_topic_name,
            "link",                 "%s", hook_name,
            NULL
        );
        return -1;
    }

    switch(json_typeof(child_data)) { // json_typeof PROTECTED
    case JSON_ARRAY:
        break;
    case JSON_OBJECT:
        break;
    case JSON_STRING:
        if(!is_child_hook) {
            break;
        }
    default:
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "wrong child hook type",
            "parent_topic_name",    "%s", parent_topic_name,
            "child_topic_name",     "%s", child_topic_name,
            "child_field",          "%j", child_field,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  In parent hook data: save child content
     *  WARNING repeated in
     *      _link_nodes() and link_child_to_parent()
     *--------------------------------------------------*/
    switch(json_typeof(parent_hook_data)) { // json_typeof PROTECTED
    case JSON_ARRAY:
        {
            if(is_child_hook) {
                json_array_append(parent_hook_data, child_data);

            } else {
                json_array_append(parent_hook_data, child_node);
            }
        }
        break;
    case JSON_OBJECT:
        {
            char pref[NAME_MAX];
            if(is_child_hook) {
                snprintf(pref, sizeof(pref), "%s~%s~%s",
                    child_topic_name,
                    child_id,
                    child_field
                );
                json_object_set(parent_hook_data, pref, child_data);
            } else {
                json_object_set(parent_hook_data, child_id, child_node);
            }
        }
        break;
    default:
        break;
    }

    /*--------------------------------------------------*
     *  In child field: save parent ref
     *--------------------------------------------------*/
    switch(json_typeof(child_data)) { // json_typeof PROTECTED
    case JSON_STRING:
        {
            char pref[NAME_MAX];
            snprintf(pref, sizeof(pref), "%s^%s^%s",
                parent_topic_name,
                parent_id,
                hook_name
            );

            json_object_set_new(
                child_node,
                child_field,
                json_string(pref)
            );
        }
        break;
    case JSON_ARRAY:
        {
            char pref[NAME_MAX];
            snprintf(pref, sizeof(pref), "%s^%s^%s",
                parent_topic_name,
                parent_id,
                hook_name
            );
            json_array_append_new(
                child_data,
                json_string(pref)
            );
        }
        break;
    case JSON_OBJECT:
        {
            char pref[NAME_MAX];
            snprintf(pref, sizeof(pref), "%s^%s^%s",
                parent_topic_name,
                parent_id,
                hook_name
            );

            json_object_set_new(
                child_data,
                pref,
                json_true()
            );
        }
        break;
    default:
        break;
    }

    /*--------------------------------------------------*
     *      Call Callback
     *      TODO implement EV_LINK_NODE/EV_UNLINK_NODE ?
     *--------------------------------------------------*/
    json_t *treedb = kwid_get(gobj, tranger, 0, "treedbs`%s", treedb_name);
    treedb_callback_t treedb_callback =
        (treedb_callback_t)(size_t)kw_get_int(gobj,
            treedb,
            "__treedb_callback__",
            0,
            0
    );
    if(treedb_callback) {
        /*
         *  Inform user in real time
         */
        void *user_data =
            (treedb_callback_t)(size_t)kw_get_int(gobj,
                treedb,
                "__treedb_callback_user_data__",
                0,
                0
            );

        JSON_INCREF(parent_node);
        treedb_callback(
            user_data,
            tranger,
            treedb_name,
            parent_topic_name,
            "EV_TREEDB_NODE_UPDATED",
            parent_node
        );
        JSON_INCREF(child_node);
        treedb_callback(
            user_data,
            tranger,
            treedb_name,
            child_topic_name,
            "EV_TREEDB_NODE_UPDATED",
            child_node
        );
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _unlink_nodes(
    hgobj gobj,
    json_t *tranger,
    const char *hook_name,
    json_t *parent_node,    // NOT owned
    json_t *child_node      // NOT owned
)
{
    /*------------------------------*
     *      Check original node
     *------------------------------*/
    if(!kw_get_bool(gobj, parent_node, "__md_treedb__`__pure_node__", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot unlink nodes, Not a pure node",
            NULL
        );
        gobj_trace_json(gobj, parent_node, "Not a pure node");
        return -1;
    }

    /*------------------------------*
     *      Check original node
     *------------------------------*/
    if(!kw_get_bool(gobj, child_node, "__md_treedb__`__pure_node__", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot unlink nodes, Not a pure node",
            NULL
        );
        gobj_trace_json(gobj, child_node, "Not a pure node");
        return -1;
    }

    /*---------------------------------------*
     *  Check self link
     *---------------------------------------*/
    if(parent_node == child_node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot unlink self node",
            "parent_node",  "%j", parent_node,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  Check treedb_name's
     *--------------------------------------------------*/
    const char *parent_node_treedb_name = kw_get_str(gobj, parent_node, "__md_treedb__`treedb_name", 0, 0);
    const char *treedb_name = kw_get_str(gobj, child_node, "__md_treedb__`treedb_name", 0, 0);
    if(strcmp(parent_node_treedb_name, treedb_name)!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot link node's of different treedb",
            "treedb_parent","%s", parent_node_treedb_name,
            "treedb_child", "%s", treedb_name,
            NULL
        );
        return -1;
    }

    /*---------------------------------------*
     *  Check parent has `hook_name` field
     *---------------------------------------*/
    json_t *parent_hook_data = kw_get_dict_value(gobj, parent_node, hook_name, 0, 0);
    if(!parent_hook_data) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "hook field not found",
            "parent_node",  "%j", parent_node,
            "hook_name",    "%s", hook_name,
            NULL
        );
        gobj_trace_json(gobj, parent_node, "hook field not found");
        return -1;
    }

    /*--------------------------------------------------*
     *  Get info of parent/child from their metadata
     *--------------------------------------------------*/
    const char *parent_topic_name = json_string_value(
        kwid_get(gobj, parent_node, 0, "__md_treedb__`topic_name")
    );
    json_t *hook_col_desc = kwid_get(gobj,
        tranger,
        KW_VERBOSE,
        "topics`%s`%s`%s",
            parent_topic_name, "cols", hook_name
    );

    const char *child_topic_name = json_string_value(
        kwid_get(gobj, child_node, 0, "__md_treedb__`topic_name")
    );

    /*----------------------*
     *      Get hook desc
     *----------------------*/
    json_t *hook_desc = kw_get_dict(gobj, hook_col_desc, "hook", 0, 0);
    if(!hook_desc) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "hook field not define topic desc",
            "topic_name",   "%s", parent_topic_name,
            "hook",         "%s", hook_name,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  Check that this child topic is in the hook desc
     *
     *  Child topic name must be defined in hook links
     *  with his child_field (fkey or hook)
     *--------------------------------------------------*/
    const char *child_field = kw_get_str(gobj, hook_desc, child_topic_name, 0, 0);
    if(!child_field) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic child not defined in hook desc links",
            "topic_name",   "%s", parent_topic_name,
            "hook",         "%s", hook_name,
            "hook_desc",    "%j", hook_desc,
            "child_topic",  "%s", child_topic_name,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *      Get ids
     *--------------------------------------------------*/
    const char *parent_id = kw_get_str(gobj, parent_node, "id", 0, 0);
    if(!parent_id) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Parent 'id' not found",
            "parent_topic", "%s", parent_topic_name,
            "parent",       "%j", parent_node,
            NULL
        );
        return -1;
    }
    const char *child_id = kw_get_str(gobj, child_node, "id", 0, 0);
    if(!child_id) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Child 'id' not found",
            "child_topic",  "%s", child_topic_name,
            "child",        "%j", child_node,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  Reverse - child container with info of parent
     *--------------------------------------------------*/
    json_t *child_col_flag = kwid_get(gobj,
        tranger,
        KW_VERBOSE,
        "topics`%s`%s`%s`flag",
            child_topic_name, "cols", child_field
    );
    if(!child_col_flag) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "field child not define topic desc",
            "topic_name",   "%s", child_topic_name,
            "field",        "%s", child_field,
            NULL
        );
    }

    BOOL is_child_hook = FALSE;
    if(kw_has_word(gobj, child_col_flag, "hook", 0)) {
        is_child_hook = TRUE;
    }

    json_t *child_data = kw_get_dict_value(gobj, child_node, child_field, 0, 0);
    if(!child_data) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "field not found in the node 6",
            "topic_name",   "%s", child_topic_name,
            "field",        "%s", child_field,
            NULL
        );
        gobj_trace_json(gobj, child_node, "field not found in the node 6");
    }

    /*--------------------------------------------------*
     *      Checks
     *--------------------------------------------------*/
    switch(json_typeof(parent_hook_data)) { // json_typeof PROTECTED
    case JSON_ARRAY:
        break;
    case JSON_OBJECT:
        break;
    default:
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "wrong parent hook type",
            "parent_topic_name",    "%s", parent_topic_name,
            "child_topic_name",     "%s", child_topic_name,
            "link",                 "%s", hook_name,
            NULL
        );
        return -1;
    }

    switch(json_typeof(child_data)) { // json_typeof PROTECTED
    case JSON_ARRAY:
        break;
    case JSON_OBJECT:
        break;
    case JSON_STRING:
        if(!is_child_hook) {
            break;
        }
    default:
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "wrong child hook type",
            "parent_topic_name",    "%s", parent_topic_name,
            "child_topic_name",     "%s", child_topic_name,
            "child_field",          "%s", child_field,
            NULL
        );
        return -1;
    }

    /*--------------------------------------------------*
     *  In parent hook data: save child content
     *--------------------------------------------------*/
    switch(json_typeof(parent_hook_data)) { // json_typeof PROTECTED
    case JSON_ARRAY:
        {
            if(is_child_hook) {
                // link: json_array_append(parent_hook_data, child_data);
                BOOL found = FALSE;
                int idx; json_t *data;
                json_array_foreach(parent_hook_data, idx, data) {
                    if(child_data == data) {
                        json_array_remove(parent_hook_data, idx);
                        found = TRUE;
                        break;
                    }
                }
                if(!found) {
                    gobj_log_error(gobj, 0,
                        "function",             "%s", __FUNCTION__,
                        "msgset",               "%s", MSGSET_TREEDB_ERROR,
                        "msg",                  "%s", "Child data not found in list parent hook",
                        "parent_topic_name",    "%s", parent_topic_name,
                        "hook_name",            "%s", hook_name,
                        "parent_id",            "%s", parent_id,
                        "child_topic_name",     "%s", child_topic_name,
                        "child_id",             "%s", child_id,
                        "child_field",          "%s", child_field,
                        NULL
                    );
                }
            } else {
                // link: json_array_append(parent_hook_data, child_node);
                BOOL found = FALSE;
                int idx; json_t *data;
                json_array_foreach(parent_hook_data, idx, data) {
                    if(child_node == data) {
                        json_array_remove(parent_hook_data, idx);
                        found = TRUE;
                        break;
                    }
                }
                if(!found) {
                    gobj_log_error(gobj, 0,
                        "function",             "%s", __FUNCTION__,
                        "msgset",               "%s", MSGSET_TREEDB_ERROR,
                        "msg",                  "%s", "Child data not found in dict parent hook",
                        "parent_topic_name",    "%s", parent_topic_name,
                        "hook_name",            "%s", hook_name,
                        "parent_id",            "%s", parent_id,
                        "child_topic_name",     "%s", child_topic_name,
                        "child_id",             "%s", child_id,
                        "child_field",          "%s", child_field,
                        NULL
                    );
                }
            }
        }
        break;
    case JSON_OBJECT:
        {
            if(is_child_hook) {
                char pref[NAME_MAX];
                snprintf(pref, sizeof(pref), "%s~%s~%s",
                    child_topic_name,
                    child_id,
                    child_field
                );
                json_object_del(parent_hook_data, pref);
            } else {
                json_object_del(parent_hook_data, child_id);
            }
        }
        break;
    default:
        break;
    }

    /*--------------------------------------------------*
     *  In child field: save parent ref
     *--------------------------------------------------*/
    switch(json_typeof(child_data)) { // json_typeof PROTECTED
    case JSON_STRING:
        {
            json_object_set_new(child_node, child_field, json_string(""));
        }
        break;
    case JSON_ARRAY:
        {
            char pref[NAME_MAX];
            snprintf(pref, sizeof(pref), "%s^%s^%s",
                parent_topic_name,
                parent_id,
                hook_name
            );
            BOOL found = FALSE;
            int idx; json_t *data;
            json_array_foreach(child_data, idx, data) {
                if(json_typeof(data)==JSON_STRING) {
                    if(strcmp(pref, json_string_value(data))==0) {
                        json_array_remove(child_data, idx);
                        found = TRUE;
                        break;
                    }
                }
            }
            if(!found) {
                gobj_log_error(gobj, 0,
                    "function",             "%s", __FUNCTION__,
                    "msgset",               "%s", MSGSET_TREEDB_ERROR,
                    "msg",                  "%s", "Parent ref not found in array child data",
                    "parent_topic_name",    "%s", parent_topic_name,
                    "hook_name",            "%s", hook_name,
                    "parent_id",            "%s", parent_id,
                    "child_topic_name",     "%s", child_topic_name,
                    "child_id",             "%s", child_id,
                    "child_field",          "%s", child_field,
                    NULL
                );
            }
        }
        break;
    case JSON_OBJECT:
        {
            char pref[NAME_MAX];
            snprintf(pref, sizeof(pref), "%s^%s^%s",
                parent_topic_name,
                parent_id,
                hook_name
            );
            if(kw_has_key(child_data, pref)) {
                json_object_del(child_data, pref);
            } else {
                gobj_log_error(gobj, 0,
                    "function",             "%s", __FUNCTION__,
                    "msgset",               "%s", MSGSET_TREEDB_ERROR,
                    "msg",                  "%s", "Parent ref not found in dict child data",
                    "parent_topic_name",    "%s", parent_topic_name,
                    "hook_name",            "%s", hook_name,
                    "parent_id",            "%s", parent_id,
                    "child_topic_name",     "%s", child_topic_name,
                    "child_id",             "%s", child_id,
                    "child_field",          "%s", child_field,
                    NULL
                );
            }
        }
        break;

    default:
        break;
    }

    /*--------------------------------------------------*
     *      Call Callback
     *      TODO implement EV_LINK_NODE/EV_UNLINK_NODE ?
     *--------------------------------------------------*/
    json_t *treedb = kwid_get(gobj, tranger, 0, "treedbs`%s", treedb_name);
    treedb_callback_t treedb_callback =
        (treedb_callback_t)(size_t)kw_get_int(gobj,
            treedb,
            "__treedb_callback__",
            0,
            0
    );
    if(treedb_callback) {
        /*
         *  Inform user in real time
         */
        void *user_data =
            (treedb_callback_t)(size_t)kw_get_int(gobj,
                treedb,
                "__treedb_callback_user_data__",
                0,
                0
            );

        JSON_INCREF(parent_node)
        treedb_callback(
            user_data,
            tranger,
            treedb_name,
            parent_topic_name,
            "EV_TREEDB_NODE_UPDATED",
            parent_node
        );
        JSON_INCREF(child_node)
        treedb_callback(
            user_data,
            tranger,
            treedb_name,
            child_topic_name,
            "EV_TREEDB_NODE_UPDATED",
            child_node
        );
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_clean_node(
    json_t *tranger,
    json_t *node,       // NOT owned, pure node
    BOOL save
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*------------------------------*
     *      Check original node
     *------------------------------*/
    if(!kw_get_bool(gobj, node, "__md_treedb__`__pure_node__", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Not a pure node",
            NULL
        );
        gobj_trace_json(gobj, node, "Not a pure node");
        return -1;
    }

    /*-------------------------------*
     *      Get node info
     *-------------------------------*/
    const char *treedb_name = kw_get_str(gobj, node, "__md_treedb__`treedb_name", 0, 0);
    const char *topic_name = kw_get_str(gobj, node, "__md_treedb__`topic_name", 0, 0);

    int ret = 0;
    BOOL to_save = FALSE;
    json_t *up_refs = get_node_up_refs(gobj, tranger, node);
    if(json_array_size(up_refs)>0) {
        int idx; json_t *old_fkey;
        json_array_foreach(up_refs, idx, old_fkey) {
            /*
             *  Delete link
             */
            const char *ref = json_string_value(old_fkey);

            /*
             *  Get parent info
             */
            char parent_topic_name[NAME_MAX];
            char parent_id[NAME_MAX];
            char hook_name[NAME_MAX];
            if(!decode_parent_ref(
                ref,
                parent_topic_name, sizeof(parent_topic_name),
                parent_id, sizeof(parent_id),
                hook_name, sizeof(hook_name)
            )) {
                // It's not a fkey
                gobj_log_error(gobj, 0,
                    "function",             "%s", __FUNCTION__,
                    "msgset",               "%s", MSGSET_TREEDB_ERROR,
                    "msg",                  "%s", "Wrong parent reference: must be \"parent_topic_name^parent_id^hook_name\"",
                    "ref",                  "%s", ref,
                    NULL
                );
                continue;
            }

            json_t *parent_node = treedb_get_node( // Return is NOT YOURS, pure node
                tranger,
                treedb_name,
                parent_topic_name,
                parent_id
            );
            if(parent_node) {
                ret += _unlink_nodes(
                    gobj,
                    tranger,
                    hook_name,
                    parent_node,    // NOT owned
                    node      // NOT owned
                );
            } else {
                search_and_remove_wrong_up_ref(
                    gobj,
                    tranger,
                    node,
                    topic_name,
                    ref
                );
            }
        }

        /*
         *  Re-checks up links
         */
        json_t *up_refs_ = get_node_up_refs(gobj, tranger, node);
        if(json_array_size(up_refs_)>0) {
            ret += -1;
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Cannot clean the links",
                "topic_name",   "%s", topic_name,
                NULL
            );
        } else {
            to_save = TRUE;
        }
        JSON_DECREF(up_refs_)
    }

    if(save) {
        if(to_save) {
            treedb_save_node(tranger, node);
        }
    }

    JSON_DECREF(up_refs)
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_autolink( // use fkeys fields of kw to auto-link
    json_t *tranger,
    json_t *node,           // NOT owned, pure node
    json_t *kw, // owned
    BOOL save
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*------------------------------*
     *      Check original node
     *------------------------------*/
    if(!kw_get_bool(gobj, node, "__md_treedb__`__pure_node__", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Not a pure node",
            NULL
        );
        gobj_trace_json(gobj, node, "Not a pure node");
        JSON_DECREF(kw)
        return -1;
    }

    /*-------------------------------*
     *      Get node info
     *-------------------------------*/
    const char *treedb_name = kw_get_str(gobj, node, "__md_treedb__`treedb_name", 0, 0);
    const char *topic_name = kw_get_str(gobj, node, "__md_treedb__`topic_name", 0, 0);

    json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name);
    if(!cols) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic without cols",
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(kw)
        return -1;
    }

    BOOL to_save = FALSE;
    char parent_topic_name[NAME_MAX];
    char parent_id[NAME_MAX];
    char hook_name[NAME_MAX];

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        BOOL is_fkey = kw_has_word(gobj, desc_flag, "fkey", 0)?TRUE:FALSE;
        if(!is_fkey) {
            continue; // Not a fkey, continue
        }

        /*
         *  HERE only fkeys
         *  link fkeys
         */
        json_t *fv = kw_get_dict_value(gobj, kw, col_name, 0, 0); // Not yours GILIPOLLAS
        if(!fv) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                "msg",          "%s", "fkey empty",
                "topic_name",   "%s", topic_name,
                "treedb_name",  "%s", treedb_name,
                "col",          "%s", col_name,
                "record",       "%j", kw,
                NULL
            );
            continue;
        }
        json_t *jn_fkeys = filtra_fkeys(
            topic_name,
            col_name,
            "list",
            fv
        );
        if(!jn_fkeys) {
            // Error already logged
            JSON_DECREF(cols)
            JSON_DECREF(kw)
            return -1;
        }

        /*-------------------------------*
         *      Add new links
         *-------------------------------*/
        int idx;
        json_t *new_fkey;
        json_array_foreach(jn_fkeys, idx, new_fkey) {
            // **FKEY**
            const char *ref = json_string_value(new_fkey);

            /*
             *  Get parent info
             */
            if(!decode_parent_ref(
                ref,
                parent_topic_name, sizeof(parent_topic_name),
                parent_id, sizeof(parent_id),
                hook_name, sizeof(hook_name)
            )) {
                // It's not a fkey
                gobj_log_error(gobj, 0,
                    "function",             "%s", __FUNCTION__,
                    "msgset",               "%s", MSGSET_TREEDB_ERROR,
                    "msg",                  "%s", "Wrong parent reference: must be \"parent_topic_name^parent_id^hook_name\"",
                    "treedb_name",          "%s", treedb_name,
                    "topic_name",           "%s", topic_name,
                    "ref",                  "%s", ref,
                    NULL
                );
                json_decref(jn_fkeys);
                json_decref(cols);
                JSON_DECREF(kw)
                return -1;
            }

            json_t *parent_node = treedb_get_node( // Return is NOT YOURS, pure node
                tranger,
                treedb_name,
                parent_topic_name,
                parent_id
            );
            if(!parent_node) {
                gobj_log_error(gobj, 0,
                    "function",             "%s", __FUNCTION__,
                    "msgset",               "%s", MSGSET_TREEDB_ERROR,
                    "msg",                  "%s", "update_node, new link: parent node not found",
                    "treedb_name",          "%s", treedb_name,
                    "topic_name",           "%s", topic_name,
                    "parent-ref",           "%s", ref,
                    NULL
                );
                json_decref(jn_fkeys);
                json_decref(cols);
                JSON_DECREF(kw)
                return -1;
            }

            if(_link_nodes(
                gobj,
                tranger,
                hook_name,
                parent_node,    // NOT owned
                node      // NOT owned
            )==0) {
                to_save = TRUE;
            } else {
                // Error already logged
                json_decref(jn_fkeys);
                json_decref(cols);
                JSON_DECREF(kw)
                return -1;
            }
        }
        json_decref(jn_fkeys);
    }

    if(save) {
        if(to_save) {
            treedb_save_node(tranger, node);
        }
    }

    JSON_DECREF(cols)
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_link_nodes(
    json_t *tranger,
    const char *hook_name,
    json_t *parent_node,    // NOT owned, pure node
    json_t *child_node      // NOT owned, pure node
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    if(_link_nodes(
        gobj,
        tranger,
        hook_name,
        parent_node,    // NOT owned
        child_node      // NOT owned
    ) < 0) {
        // Error already logged
        return -1;
    }

    /*----------------------------*
     *      Save persistent
     *  Only childs are saved
     *----------------------------*/
    return treedb_save_node(tranger, child_node);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_unlink_nodes(
    json_t *tranger,
    const char *hook_name,
    json_t *parent_node,    // NOT owned, pure node
    json_t *child_node      // NOT owned, pure node
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    if(_unlink_nodes(
        gobj,
        tranger,
        hook_name,
        parent_node,    // NOT owned
        child_node      // NOT owned
    ) < 0) {
        // Error already logged
        return -1;
    }

    /*----------------------------*
     *      Save persistents
     *  Only childs are saved
     *----------------------------*/
    return treedb_save_node(tranger, child_node);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL id_in_fkeys(const char *id, json_t *jn_fkey_value)
{
    char parent_id[NAME_MAX];

    if(json_is_string(jn_fkey_value)) {
        /*---------------*
            *  fkey: s
            *---------------*/
        const char *ref = json_string_value(jn_fkey_value);
        decode_parent_ref(
            ref,
            0, 0,
            parent_id, sizeof(parent_id),
            0, 0
        );
        if(strcmp(id, parent_id)==0) {
            return TRUE;
        } else {
            return FALSE;
        }

    } else if(json_is_array(jn_fkey_value)) {
        /*---------------*
         *  fkey: list
         *---------------*/
        int idx; json_t *jn_ref;
        json_array_foreach(jn_fkey_value, idx, jn_ref) {
            const char *ref = json_string_value(jn_ref);
            decode_parent_ref(
                ref,
                0, 0,
                parent_id, sizeof(parent_id),
                0, 0
            );
            if(strcmp(id, parent_id)==0) {
                return TRUE;
            }
        }
        return FALSE;

    } else if(json_is_object(jn_fkey_value)) {
        /*---------------*
         *  fkey: dict
         *---------------*/
        const char *ref; json_t *jn_ref;
        json_object_foreach(jn_fkey_value, ref, jn_ref) {
            decode_parent_ref(
                ref,
                0, 0,
                parent_id, sizeof(parent_id),
                0, 0
            );
            if(strcmp(id, parent_id)==0) {
                return TRUE;
            }
        }
        return FALSE;

    } else {
        // What fuck?
        return FALSE;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL match_fkey(json_t *jn_filter_value, json_t *jn_fkey_value)
{
    if(json_is_string(jn_filter_value)) {
        /*---------------------------*
         *  Filter: string
         *  Operation ==
         *---------------------------*/
        const char *id = json_string_value(jn_filter_value);
        BOOL match = id_in_fkeys(id, jn_fkey_value);
        return match;

    } else if(json_is_array(jn_filter_value)) {
        /*---------------------------*
         *  Filter: list
         *  Operation OR
         *---------------------------*/
        if(json_array_size(jn_filter_value)==0) {
            // Empty pass all
            return TRUE;
        }
        BOOL match = FALSE;
        int x; json_t *jn_filter_v;
        json_array_foreach(jn_filter_value, x, jn_filter_v) {
            const char *id = json_string_value(jn_filter_v);
            if(id_in_fkeys(id, jn_fkey_value)) {
                match = TRUE;
                break;
            }
        }
        return match;

    } else if(json_is_object(jn_filter_value)) {
        /*---------------------------*
         *  Filter: dict
         *  Operation AND
         *---------------------------*/
        if(json_object_size(jn_filter_value)==0) {
            // Empty pass all
            return TRUE;
        }
        BOOL match = TRUE;
        const char *id; json_t *jn_filter_v;
        json_object_foreach(jn_filter_value, id, jn_filter_v) {
            if(!id_in_fkeys(id, jn_fkey_value)) {
                match = FALSE;
                break;
            }
        }
        return match;

    } else {
        // What merde?
        return FALSE;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL match_hook(json_t *jn_filter_value, json_t *jn_hook_value)
{
    hgobj gobj = 0;

    if(json_is_string(jn_filter_value)) {
        /*---------------------------*
         *  Filter: string
         *  Operation ==
         *---------------------------*/
        const char *id = json_string_value(jn_filter_value);
        BOOL match = kwid_match_id(gobj, jn_hook_value, id);
        return match;

    } else if(json_is_array(jn_filter_value)) {
        /*---------------------------*
         *  Filter: list
         *  Operation OR
         *---------------------------*/
        if(json_array_size(jn_filter_value)==0) {
            // Empty pass all
            return TRUE;
        }
        BOOL match = FALSE;
        int x; json_t *jn_filter_v;
        json_array_foreach(jn_filter_value, x, jn_filter_v) {
            const char *id = json_string_value(jn_filter_v);
            if(kwid_match_id(gobj, jn_hook_value, id)) {
                match = TRUE;
                break;
            }
        }
        return match;

    } else if(json_is_object(jn_filter_value)) {
        /*---------------------------*
         *  Filter: dict
         *  Operation AND
         *---------------------------*/
        if(json_object_size(jn_filter_value)==0) {
            // Empty pass all
            return TRUE;
        }
        BOOL match = TRUE;
        const char *id; json_t *jn_filter_v;
        json_object_foreach(jn_filter_value, id, jn_filter_v) {
            if(!kwid_match_id(gobj, jn_hook_value, id)) {
                match = FALSE;
                break;
            }
        }
        return match;

    } else {
        // What fuck?
        return FALSE;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL match_node_simple(
    json_t *cols,       // NOT owned
    json_t *node,       // NOT owned
    json_t *jn_filter   // NOT owned
)
{
    hgobj gobj = 0;

    if(json_object_size(jn_filter)==0) {
        // A empty object at first level evaluate as true.
        return TRUE;
    }

    // Not Empty object evaluate as true, until a NOT match condition occurs.
    BOOL matched = TRUE;

    const char *col_name;
    json_t *jn_filter_value;
    json_object_foreach(jn_filter, col_name, jn_filter_value) {
        json_t *col = kw_get_dict(gobj, cols, col_name, 0, KW_REQUIRED);
        if(!col) {
            const char *topic_name = kw_get_str(gobj, node, "__md_treedb__`topic_name", 0, 0);
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Topic col not found",
                "topic_name",   "%s", topic_name,
                "col_name",     "%s", col_name,
                NULL
            );
            continue; // Never must occur
        }
        json_t *jn_record_value = kw_get_dict_value(gobj, node, col_name, 0, KW_REQUIRED);
        if(!jn_record_value) {
            const char *topic_name = kw_get_str(gobj, node, "__md_treedb__`topic_name", 0, 0);
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Topic col without value",
                "topic_name",   "%s", topic_name,
                "col_name",     "%s", col_name,
                NULL
            );
            continue; // Never must occur
        }
        json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        BOOL is_fkey = kw_has_word(gobj, desc_flag, "fkey", 0)?TRUE:FALSE;
        BOOL is_hook = kw_has_word(gobj, desc_flag, "hook", 0)?TRUE:FALSE;
        if(is_fkey) {
            matched = match_fkey(jn_filter_value, jn_record_value);
            if(!matched) {
                break;
            }
        } else if(is_hook) {
            matched = match_hook(jn_filter_value, jn_record_value);
            if(!matched) {
                break;
            }
        } else {
            // Here hook fields and the others
            if(json_is_object(jn_record_value)) {
                if(!kw_match_simple(
                    jn_record_value, // NOT owned
                    json_incref(jn_filter_value) // owned
                )) {
                    matched = FALSE;
                    break;
                }
            } else {
                if(cmp_two_simple_json(jn_record_value, jn_filter_value)!=0) {
                    matched = FALSE;
                    break;
                }
            }
        }
    }

    return matched;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_get_node( // WARNING Return is NOT YOURS, pure node
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *id  // using the primary key
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(tranger, treedb_name, topic_name)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        return 0;
    }

    /*-------------------------------*
     *  Get indexx: to get node
     *-------------------------------*/
    json_t *indexx = treedb_get_id_index(tranger, treedb_name, topic_name);

    /*-------------------------------*
     *      Get
     *-------------------------------*/
    json_t *node = exist_primary_node(indexx, id);
    if(!node) {
        // Silence
        return 0;
    }
    return node;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_get_instance( // WARNING Return is NOT YOURS, pure node
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *pkey2_name, // required
    const char *id,     // primary key
    const char *key2    // secondary key
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(tranger, treedb_name, topic_name)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        return 0;
    }

    /*-------------------------------*
     *  Get indexy: to get node
     *-------------------------------*/
    json_t *indexy = treedb_get_pkey2_index(tranger, treedb_name, topic_name, pkey2_name);
    if(!indexy) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Secondary key not found in topic",
            "topic_name",   "%s", topic_name,
            "pkey2_name",   "%s", pkey2_name,
            NULL
        );
        return 0;
    }

    /*-------------------------------*
     *      Get
     *-------------------------------*/
    json_t *node = exist_secondary_node(indexy, id, key2);
    if(!node) {
        // Silence
        return 0;
    }
    return node;
}

/***************************************************************************
    Return a view of node with hook fields being collapsed
    WARNING extra fields are ignored, only topic desc fields are used

    See 31_tr_treedb for options
 ***************************************************************************/
PUBLIC json_t *node_collapsed_view( // Return MUST be decref
    json_t *tranger, // NOT owned
    json_t *node, // NOT owned
    json_t *jn_options // owned fkey,hook options
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*------------------------------*
     *      Check original node
     *------------------------------*/
    if(!kw_get_bool(gobj, node, "__md_treedb__`__pure_node__", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Not a pure node",
            NULL
        );
        gobj_trace_json(gobj, node, "Not a pure node");
        JSON_DECREF(jn_options)
        return 0;
    }

    /*-------------------------------*
     *      Get node info
     *-------------------------------*/
    const char *topic_name = kw_get_str(gobj, node, "__md_treedb__`topic_name", 0, 0);

    json_t *topic_desc = tranger2_dict_topic_desc_cols(tranger, topic_name);

    BOOL with_metadata = kw_get_bool(gobj, jn_options, "with_metadata", 0, KW_WILD_NUMBER);
    BOOL without_rowid =  kw_get_bool(gobj, jn_options, "without_rowid", 0, KW_WILD_NUMBER);

    json_t *node_view = json_object();

    const char *col_name; json_t *col;
    json_object_foreach(topic_desc, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        BOOL is_hook = kw_has_word(gobj, desc_flag, "hook", 0)?TRUE:FALSE;
        BOOL is_fkey = kw_has_word(gobj, desc_flag, "fkey", 0)?TRUE:FALSE;
        BOOL is_rowid = kw_has_word(gobj, desc_flag, "rowid", 0)?TRUE:FALSE;
        BOOL is_required = kw_has_word(gobj, desc_flag, "required", 0)?TRUE:FALSE;
        json_t *field_data = kw_get_dict_value(gobj, node, col_name, 0, is_required?KW_REQUIRED:0);
        if(!field_data) {
            // Something wrong?
            continue;
        }
        if(strncmp(col_name, "__", 2)==0) {
            if(!with_metadata) {
                // Ignore metadata
                continue;
            }
        }
        if(is_rowid) {
            if(without_rowid) {
                // Ignore rowid
                continue;
            }
        }
        if(is_hook) {
            json_t *list = kw_get_dict_value(
                gobj,
                node_view,
                col_name,
                json_array(),
                KW_CREATE
            );
            json_t *child_list = get_hook_list(gobj, field_data);

            json_t *childs = apply_child_list_options(gobj, child_list, jn_options);
            json_array_extend(list, childs);
            json_decref(childs);

            json_decref(child_list);

        } else if(is_fkey) {
            json_t *list = kw_get_dict_value(
                gobj,
                node_view,
                col_name,
                json_array(),
                KW_CREATE
            );
            json_t *refs = get_fkey_refs(field_data);
            json_t *parents = apply_parent_ref_options(gobj, refs, jn_options);
            json_array_extend(list, parents);
            json_decref(parents);
            json_decref(refs);

        } else {
            json_object_set(
                node_view,
                col_name,
                field_data
            );
        }
    }

    if(with_metadata) {
        json_object_set_new(
            node_view,
            "__md_treedb__",
            json_deep_copy(json_object_get(node, "__md_treedb__"))
        );
        json_object_set_new(
            json_object_get(node_view, "__md_treedb__"),
            "__pure_node__",
            json_false()
        );
    }

    JSON_DECREF(topic_desc)
    JSON_DECREF(jn_options)
    return node_view;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_list_nodes( // Return MUST be decref
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    json_t *jn_filter_,  // owned
    BOOL (*match_fn) (
        json_t *topic_desc, // NOT owned
        json_t *node,       // NOT owned
        json_t *jn_filter   // NOT owned
    )
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(tranger, treedb_name, topic_name)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_filter_)
        return 0;
    }

    /*-----------------------------------*
     *  Use duplicate, will be modified
     *-----------------------------------*/
    json_t *jn_filter = jn_filter_?json_deep_copy(jn_filter_):0;
    JSON_DECREF(jn_filter_)

    /*-------------------------------*
     *  Get indexx: to list nodes
     *-------------------------------*/
    json_t *indexx = treedb_get_id_index(tranger, treedb_name, topic_name);

    /*--------------------------------------------*
     *      Search
     *  Extract from jn_filter
     *      the ids and the fields of topic
     *--------------------------------------------*/
    if(!match_fn) {
        match_fn = match_node_simple;
    }

    /*
     *  Extrae ids
     */
    json_t *ids_list = 0;
    json_t *jn_id = kw_get_dict_value(gobj, jn_filter, "id", 0, KW_EXTRACT);
    if(jn_id) {
        ids_list = kwid_get_ids(jn_id);
        JSON_DECREF(jn_id)
    }

    /*
     *  Usa __filter__ si existe
     */
    if(kw_has_key(jn_filter, "__filter__")) {
        json_t *jn_filter2_ = kw_get_dict_value(gobj, jn_filter, "__filter__", 0, KW_EXTRACT);
        JSON_DECREF(jn_filter)
        jn_filter = jn_filter2_;
    }

    /*
     *  Filtra de jn_filter solo las keys del topic
     */
    json_t *topic_desc = tranger2_dict_topic_desc_cols(tranger, topic_name);
    jn_filter = kw_clone_by_keys(
        gobj,
        jn_filter,     // owned
        kw_incref(topic_desc), // owned
        FALSE
    );

    /*-------------------------------*
     *      Create list
     *-------------------------------*/
    json_t *list = json_array();

    /*-------------------------------*
     *      Get indexx's
     *-------------------------------*/
    if(json_is_array(indexx)) {
        size_t idx; json_t *node;
        json_array_foreach(indexx, idx, node) {
            if(!kwid_match_nid(
                gobj,
                ids_list,
                kw_get_str(gobj, node, "id", 0, 0),
                tranger2_max_key_size())
            ) {
                continue;
            }
            if(match_fn(topic_desc, node, jn_filter)) {
                // Collapse records (hook fields)
                json_array_append(
                    list,
                    node
                );
            }
        }
    } else if(json_is_object(indexx)) {
        const char *id; json_t *node;
        json_object_foreach(indexx, id, node) {
            if(!kwid_match_nid(gobj, ids_list, id, tranger2_max_key_size())) {
                continue;
            }
            if(match_fn(topic_desc, node, jn_filter)) {
                json_array_append(
                    list,
                    node
                );
            }
        }

    } else  {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "kw MUST BE a json array or object",
            NULL
        );
        JSON_DECREF(list)
    }

    JSON_DECREF(topic_desc)
    JSON_DECREF(jn_filter)
    JSON_DECREF(ids_list)

    return list;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_list_instances( // Return MUST be decref
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *pkey2_name,
    json_t *jn_filter_,  // owned
    BOOL (*match_fn) (
        json_t *topic_desc, // NOT owned
        json_t *node,       // NOT owned
        json_t *jn_filter   // NOT owned
    )
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*-----------------------------------*
     *      Check appropriate topic
     *-----------------------------------*/
    if(!treedb_is_treedbs_topic(tranger, treedb_name, topic_name)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Topic name not found in treedbs",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_filter_);
        return 0;
    }

    /*-----------------------------------*
     *  Use duplicate, will be modified
     *-----------------------------------*/
    json_t *jn_filter = jn_filter_?json_deep_copy(jn_filter_):0;
    JSON_DECREF(jn_filter_)

    /*--------------------------------------------*
     *      Search
     *  Extract from jn_filter
     *      the ids and the fields of topic
     *--------------------------------------------*/
    if(!match_fn) {
        match_fn = match_node_simple;
    }

    /*
     *  Extrae ids
     */
    json_t *ids_list = 0;
    json_t *jn_id = kw_get_dict_value(gobj, jn_filter, "id", 0, KW_EXTRACT);
    if(jn_id) {
        ids_list = kwid_get_ids(jn_id);
        JSON_DECREF(jn_id)
    }

    /*
     *  Usa __filter__ si existe
     */
    if(kw_has_key(jn_filter, "__filter__")) {
        json_t *jn_filter2_ = kw_get_dict_value(gobj, jn_filter, "__filter__", 0, KW_EXTRACT);
        JSON_DECREF(jn_filter)
        jn_filter = jn_filter2_;
    }

    /*
     *  Filtra de jn_filter solo las keys del topic
     */
    json_t *topic_desc = tranger2_dict_topic_desc_cols(tranger, topic_name);
    jn_filter = kw_clone_by_keys(
        gobj,
        jn_filter,     // owned
        kw_incref(topic_desc), // owned
        FALSE
    );

    /*-------------------------------*
     *      Create list
     *-------------------------------*/
    json_t *list = json_array();

    /*-------------------------------*
     *      Get indexy's
     *-------------------------------*/
    json_t *iter_pkey2s = 0;
    if(empty_string(pkey2_name)) {
        iter_pkey2s = treedb_topic_pkey2s(tranger, topic_name);
    } else {
        iter_pkey2s = json_array();
        json_array_append_new(iter_pkey2s, json_string(pkey2_name));
    }

    int idx; json_t *jn_pkey2_name;
    json_array_foreach(iter_pkey2s, idx, jn_pkey2_name) {
        const char *pkey2_name = json_string_value(jn_pkey2_name);
        if(empty_string(pkey2_name)) {
            continue;
        }
        /*---------------------------------*
         *  Get indexy: to list instances
         *---------------------------------*/
        json_t *indexy = treedb_get_pkey2_index(
            tranger,
            treedb_name,
            topic_name,
            pkey2_name
        );
        if(!indexy) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "TreeDb Topic indexy NOT FOUND",
                "topic_name",   "%s", topic_name,
                "pkey2_name",   "%s", pkey2_name,
                NULL
            );
            continue;
        }
        if(json_is_object(indexy)) {
            const char *id; json_t *pkey2_dict;
            json_object_foreach(indexy, id, pkey2_dict) {
                if(!kwid_match_nid(gobj, ids_list, id, tranger2_max_key_size())) {
                    continue;
                }
                const char *pkey2; json_t *node;
                json_object_foreach(pkey2_dict, pkey2, node) {
                    if(match_fn(topic_desc, node, jn_filter)) {
                        json_array_append(
                            list,
                            node
                        );
                    }
                }
            }
        } else  {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "kw MUST BE a json array or object",
                NULL
            );
        }
    }

    JSON_DECREF(iter_pkey2s)
    JSON_DECREF(topic_desc)
    JSON_DECREF(ids_list)
    JSON_DECREF(jn_filter)

    return list;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_int_t json_size(json_t *value)
{
    if(!value) {
        return 0;
    }
    switch(json_typeof(value)) {
    case JSON_ARRAY:
        return json_array_size(value);
    case JSON_OBJECT:
        return json_object_size(value);
    case JSON_STRING:
        return 1;
    default:
        return 0;
    }
}

/***************************************************************************
 *
    fkey options
    -------------
    "refs"
    "fkey_refs"
        Return 'fkey ref'
            ["topic_name^id^hook_name", ...]


    "only_id"
    "fkey_only_id"
        Return the 'fkey ref' with only the 'id' field
            ["$id",...]


    "list_dict" (default)
    "fkey_list_dict"
        Return the kwid style:
            [{"id": "$id", "topic_name":"$topic_name", "hook_name":"$hook_name"}, ...]

 ***************************************************************************/
PRIVATE json_t *apply_parent_ref_options(
    hgobj gobj,
    json_t *refs,  // NOT owned
    json_t *jn_options // NOT owned
)
{
    json_t *parents = json_array();
    char parent_topic_name[NAME_MAX];
    char parent_id[NAME_MAX];
    char hook_name[NAME_MAX];

    int idx; json_t *jn_fkey;
    json_array_foreach(refs, idx, jn_fkey) {
        /*
         *  Get parent info
         */
        const char *ref = json_string_value(jn_fkey);
        if(!decode_parent_ref(
            ref,
            parent_topic_name, sizeof(parent_topic_name),
            parent_id, sizeof(parent_id),
            hook_name, sizeof(hook_name)
        )) {
            // It's not a fkey
            gobj_log_error(gobj, 0,
                "function",             "%s", __FUNCTION__,
                "msgset",               "%s", MSGSET_TREEDB_ERROR,
                "msg",                  "%s", "Wrong parent reference: must be \"parent_topic_name^parent_id^hook_name\"",
                "ref",                  "%s", ref,
                NULL
            );
            continue;
        }

        if(kw_get_bool(gobj, jn_options, "only_id", 0, KW_WILD_NUMBER) ||
            kw_get_bool(gobj, jn_options, "fkey_only_id", 0, KW_WILD_NUMBER)
        ) {
            /*
                Return the 'fkey ref' with only the 'id' field
                    ["$id",...]
             */
            json_array_append_new(parents, json_string(parent_id));

        } else if(kw_get_bool(gobj, jn_options, "list_dict", 0, KW_WILD_NUMBER) ||
            kw_get_bool(gobj, jn_options, "fkey_list_dict", 0, KW_WILD_NUMBER)
        ) {
            /*
                Return the kwid style:
                    [{"id": "$id", "topic_name":"$topic_name", "hook_name":"$hook_name"}, ...]
             */
            json_array_append_new(
                parents,
                json_pack("{s:s, s:s, s:s}",
                    "id", parent_id,
                    "topic_name", parent_topic_name,
                    "hook_name", hook_name
                )
            );

        } else if(kw_get_bool(gobj, jn_options, "refs", 0, KW_WILD_NUMBER) ||
            kw_get_bool(gobj, jn_options, "fkey_refs", 0, KW_WILD_NUMBER)
        ) {
            /*
                Return 'fkey ref'
                    ["topic_name^id^hook_name", ...]
            */
            json_array_append_new(parents, json_string(ref));

        } else {
            /*
             *  DEFAULT
             *
                Return the kwid style:
                    [{"id": "$id", "topic_name":"$topic_name", "hook_name":"$hook_name"}, ...]
             */
            json_array_append_new(
                parents,
                json_pack("{s:s, s:s, s:s}",
                    "id", parent_id,
                    "topic_name", parent_topic_name,
                    "hook_name", hook_name
                )
            );

        }
    }

    return parents;
}

/***************************************************************************
 *
    hook options
    ------------
    "refs"
    "hook_refs"
        Return 'hook ref'
            ["topic_name^id", ...]

    "only_id"
    "hook_only_id"
        Return the 'hook ref' with only the 'id' field
            ["$id",...]

    "list_dict" (default)
    "hook_list_dict"
        Return the kwid style:
            [{"id": "$id", "topic_name":"$topic_name"}, ...]

    "size"
    "hook_size"
        Return:
            [{"size": size}]

 ***************************************************************************/
PRIVATE json_t *apply_child_list_options(
    hgobj gobj,
    json_t *child_list,  // NOT owned
    json_t *jn_options // NOT owned
)
{
    json_t *childs = json_array();

    if(kw_get_bool(gobj, jn_options, "size", 0, KW_WILD_NUMBER) ||
        kw_get_bool(gobj, jn_options, "hook_size", 0, KW_WILD_NUMBER)
    ) {
        /*
            Return:
                [{"size": size}]
         */
        json_array_append_new(
            childs,
            json_pack("{s:I}",
                "size", json_size(child_list)
            )
        );
        return childs;
    }

    int idx; json_t *child;
    json_array_foreach(child_list, idx, child) {
        if(kw_get_bool(gobj, jn_options, "only_id", 0, KW_WILD_NUMBER) ||
            kw_get_bool(gobj, jn_options, "hook_only_id", 0, KW_WILD_NUMBER)
        ) {
            /*
                Return the 'hook ref' with only the 'id' field
                    ["$id",...]
             */
            const char *id = kw_get_str(gobj, child, "id", 0, KW_REQUIRED);
            json_array_append_new(childs, json_string(id));

        } else if(kw_get_bool(gobj, jn_options, "list_dict", 0, KW_WILD_NUMBER) ||
            kw_get_bool(gobj, jn_options, "hook_list_dict", 0, KW_WILD_NUMBER)
        ) {
            /*
                Return the kwid style:
                    [{"id": "$id", "topic_name":"$topic_name"}, ...]
             */
            const char *id = kw_get_str(gobj, child, "id", 0, KW_REQUIRED);
            const char *topic_name = kw_get_str(gobj, child, "__md_treedb__`topic_name", 0, 0);

            json_array_append_new(
                childs,
                json_pack("{s:s, s:s}",
                    "id", id,
                    "topic_name", topic_name
                )
            );

        } else if(kw_get_bool(gobj, jn_options, "refs", 0, KW_WILD_NUMBER) ||
            kw_get_bool(gobj, jn_options, "hook_refs", 0, KW_WILD_NUMBER)
        ) {
            /*
                    Return 'hook ref'
                        ["topic_name^id", ...]

            */
            const char *id = kw_get_str(gobj, child, "id", 0, KW_REQUIRED);
            const char *topic_name = kw_get_str(gobj, child, "__md_treedb__`topic_name", 0, 0);
            char ref[NAME_MAX];
            snprintf(ref, sizeof(ref), "%s^%s", topic_name, id);
            json_array_append_new(childs, json_string(ref));

        } else {
            /*
             *  DEFAULT
             *
                Return the kwid style:
                    [{"id": "$id", "topic_name":"$topic_name"}, ...]
             */
            const char *id = kw_get_str(gobj, child, "id", 0, KW_REQUIRED);
            const char *topic_name = kw_get_str(gobj, child, "__md_treedb__`topic_name", 0, 0);

            json_array_append_new(
                childs,
                json_pack("{s:s, s:s}",
                    "id", id,
                    "topic_name", topic_name
                )
            );

        }
    }

    return childs;
}

/***************************************************************************
 *  Return a list of parent **references** pointed by the link (fkey)
 ***************************************************************************/
PUBLIC json_t *treedb_parent_refs( // Return MUST be decref
    json_t *tranger,
    const char *fkey,
    json_t *node,       // NOT owned, pure node
    json_t *jn_options  // owned, fkey options
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*------------------------------*
     *      Check original node
     *------------------------------*/
    if(!kw_get_bool(gobj, node, "__md_treedb__`__pure_node__", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Not a pure node",
            NULL
        );
        gobj_trace_json(gobj, node, "Not a pure node");
        JSON_DECREF(jn_options)
        return 0;
    }

    /*-------------------------------*
     *      Get node info
     *-------------------------------*/
    const char *topic_name = kw_get_str(gobj, node, "__md_treedb__`topic_name", 0, 0);

    json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name);
    json_t *col = kw_get_dict_value(gobj, cols, fkey, 0, 0);
    if(!col) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "fkey not found in the desc",
            "topic_name",   "%s", topic_name,
            "fkey",         "%s", fkey,
            NULL
        );
        gobj_trace_json(gobj, cols, "fkey not found in the desc");
        JSON_DECREF(jn_options)
        json_decref(cols);
        return 0;
    }
    json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
    BOOL is_fkey = kw_has_word(gobj, desc_flag, "fkey", 0)?TRUE:FALSE;
    if(!is_fkey) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "not a fkey",
            "topic_name",   "%s", topic_name,
            "fkey",         "%s", fkey,
            NULL
        );
        gobj_trace_json(gobj, cols, "not a fkey");
        JSON_DECREF(jn_options)
        json_decref(cols);
        return 0;
    }

    json_t *field_data = kw_get_dict_value(gobj, node, fkey, 0, 0);
    if(!field_data) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "fkey data not found in the node",
            "topic_name",   "%s", topic_name,
            "fkey",         "%s", fkey,
            NULL
        );
        gobj_trace_json(gobj, node, "fkey data not found in the node");
        JSON_DECREF(jn_options)
        json_decref(cols);
        return 0;
    }

    json_t *refs = get_fkey_refs(field_data);
    json_t *parents = apply_parent_ref_options(gobj, refs, jn_options);
    json_decref(refs);

    JSON_DECREF(jn_options)
    json_decref(cols);
    return parents;
}
/***************************************************************************
 *  Return a list of parent nodes pointed by the link (fkey)
 ***************************************************************************/
PUBLIC json_t *treedb_list_parents( // Return MUST be decref
    json_t *tranger,
    const char *fkey, // must be a fkey field
    json_t *node, // not owned
    BOOL collapsed_view, // TRUE return collapsed views
    json_t *jn_options // owned, fkey,hook options when collapsed_view is true
) {
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    const char *treedb_name = kw_get_str(gobj, node, "__md_treedb__`treedb_name", 0, KW_REQUIRED);
    const char *topic_name = kw_get_str(gobj, node, "__md_treedb__`topic_name", 0, 0);

    json_t *refs = treedb_parent_refs( // Return MUST be decref
        tranger,
        fkey,
        node,       // NOT owned, pure node
        json_pack("{s:b, s:b}",
            "list_dict", 1,
            "with_metadata", 1
        )
    );
    if(!refs) {
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_TREEDB_ERROR,
            "msg",                  "%s", "treedb_parent_refs: no refs",
            "parent",               "%s", fkey,
            "topic_name",           "%s", topic_name,
            NULL
        );
        JSON_DECREF(jn_options)
        return NULL;
    }

    json_t *parents = json_array();

    int idx; json_t *parent_ref;
    json_array_foreach(refs, idx, parent_ref) {
        const char *parent_topic_name = kw_get_str(gobj, parent_ref, "topic_name", "", KW_REQUIRED);
        const char *parent_id = kw_get_str(gobj, parent_ref, "id", "", KW_REQUIRED);

        json_t *parent_node = treedb_get_node( // Return is NOT YOURS
            tranger,
            treedb_name,
            parent_topic_name,
            parent_id
        );
        if(!parent_node) {
            gobj_log_error(gobj, 0,
                "function",             "%s", __FUNCTION__,
                "msgset",               "%s", MSGSET_TREEDB_ERROR,
                "msg",                  "%s", "get_parent_nodes: parent node not found",
                "parent_topic_name",    "%s", parent_topic_name,
                "parent_id",            "%s", parent_id,
                NULL
            );
            continue;
        }

        if(collapsed_view) {
            json_t *view_parent_node = node_collapsed_view( // Return MUST be decref
                tranger, // not owned
                parent_node, // not owned
                json_incref(jn_options) // owned
            );
            json_array_append_new(parents, view_parent_node);
        } else {
            json_array_append(parents, parent_node);
        }
    }

    JSON_DECREF(jn_options)
    JSON_DECREF(refs)
    return parents;
}

/***************************************************************************
 *  Return a list of childs of the hook
 ***************************************************************************/
PRIVATE json_t *_list_childs(
    hgobj gobj,
    json_t *tranger,
    const char *hook,
    json_t *node       // NOT owned, pure node
)
{
    /*------------------------------*
     *      Check original node
     *------------------------------*/
    if(!kw_get_bool(gobj, node, "__md_treedb__`__pure_node__", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Not a pure node",
            NULL
        );
        gobj_trace_json(gobj, node, "Not a pure node");
        return 0;
    }

    /*-------------------------------*
     *      Get node info
     *-------------------------------*/
    const char *topic_name = kw_get_str(gobj, node, "__md_treedb__`topic_name", 0, 0);

    json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name);
    json_t *col = kw_get_dict_value(gobj, cols, hook, 0, 0);
    if(!col) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "hook not found",
            "topic_name",   "%s", topic_name,
            "hook",         "%s", hook,
            NULL
        );
        gobj_trace_json(gobj, cols, "hook not found");
        json_decref(cols);
        return 0;
    }
    json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
    BOOL is_hook = kw_has_word(gobj, desc_flag, "hook", 0)?TRUE:FALSE;
    if(!is_hook) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "not a hook",
            "topic_name",   "%s", topic_name,
            "hook",         "%s", hook,
            NULL
        );
        gobj_trace_json(gobj, cols, "not a hook");
        json_decref(cols);
        return 0;
    }

    json_t *field_data = kw_get_dict_value(gobj, node, hook, 0, 0);
    if(!field_data) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "hook data not found in the node",
            "topic_name",   "%s", topic_name,
            "hook",         "%s", hook,
            NULL
        );
        gobj_trace_json(gobj, node, "hook data not found in the node");
        json_decref(cols);
        return 0;
    }

    json_t *child_list = get_hook_list(gobj, field_data);

    json_decref(cols);
    return child_list;
}

/***************************************************************************
 *  Return a list of childs of the hook in the tree
 ***************************************************************************/
PRIVATE json_t *add_tree_childs(
    hgobj gobj,
    json_t *tranger,
    json_t *list,       // not owned
    const char *hook,
    json_t *node,       // not owned
    BOOL recursive,
    json_t *jn_filter   // not owned
)
{
    json_t *child_list = _list_childs(gobj, tranger, hook, node);
    if(!child_list) {
        // Error already logged
        return 0;
    }

    int idx; json_t *child;
    json_array_foreach(child_list, idx, child) {
        if(kw_match_simple(
            child, // NOT owned
            json_incref(jn_filter) // owned
        )){
            json_array_append(list, child);
            if(recursive) {
                add_tree_childs(gobj, tranger, list, hook, child, recursive, jn_filter);
            }
        }
    }
    json_decref(child_list);

    return list;
}

/***************************************************************************
 *  Return a list of childs of the hook
 ***************************************************************************/
PUBLIC json_t *treedb_node_childs(
    json_t *tranger,
    const char *hook,
    json_t *node,       // NOT owned, pure node
    json_t *jn_filter,  // filter to childs
    json_t *jn_options  // fkey,hook options, "recursive"
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*------------------------------*
     *      Check original node
     *------------------------------*/
    if(!kw_get_bool(gobj, node, "__md_treedb__`__pure_node__", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Not a pure node",
            NULL
        );
        gobj_trace_json(gobj, node, "Not a pure node");
        JSON_DECREF(jn_filter)
        JSON_DECREF(jn_options)
        return 0;
    }

    if(!kw_get_dict_value(gobj, node, hook, 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "hook not found",
            "topic_name",   "%s", kw_get_str(gobj, node, "__md_treedb__`treedb_name", 0, 0),
            "hook",         "%s", hook,
            NULL
        );
        JSON_DECREF(jn_filter)
        JSON_DECREF(jn_options)
        return 0;
    }

    BOOL recursive = kw_get_bool(gobj, jn_options, "recursive", 0, KW_WILD_NUMBER);
    json_t *list = json_array();
    add_tree_childs(gobj, tranger, list, hook, node, recursive, jn_filter);

    JSON_DECREF(jn_filter)
    JSON_DECREF(jn_options)
    return list;
}

/***************************************************************************
 *  Add path to child
 ***************************************************************************/
PUBLIC int add_jtree_path(
    json_t *parent,  // not owned
    json_t *child  // not owned
)
{
    hgobj gobj = 0;

    const char *parent_path = parent?kw_get_str(gobj, parent, "__path__", "", 0):0;
    const char *child_id = kw_get_str(gobj, child, "id", "", 0);

    if(parent_path) {
        char *path = str_concat3(parent_path, "`", child_id);
        if(path) {
            json_object_set_new(child, "__path__", json_string(path));
            str_concat_free(path);
        }
    } else {
        json_object_set_new(child, "__path__", json_string(child_id));
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *create_jchild(
    hgobj gobj,
    json_t *tranger,
    const char *hook,   // hook to build the hierarchical tree
    const char *rename_hook, // change the hook name in the tree response
    json_t *node,       // NOT owned, pure node
    json_t *jn_options  // fkey,hook options
)
{
    json_t *jchild_ = node_collapsed_view(
        tranger,
        node,
        json_incref(jn_options)
    );
    json_t *jchild = json_deep_copy(jchild_);
    json_decref(jchild_);

    if(!empty_string(rename_hook)) {
        json_t *jn_hook = kw_get_dict_value(gobj, jchild, hook, 0, KW_REQUIRED|KW_EXTRACT);
        json_decref(jn_hook);
        json_object_set_new(jchild, rename_hook, json_array());
    }
    return jchild;
}

/***************************************************************************
 *  Return a list of hierarchical childs of the hook in the tree
 ***************************************************************************/
PRIVATE json_t *add_jtree_childs(
    hgobj gobj,
    json_t *tranger,
    json_t *tree,       // not owned
    const char *hook,
    const char *rename_hook, // change the hook name in the tree response
    json_t *node,     // not owned
    json_t *parent,     // not owned
    json_t *jn_filter,  // not owned
    json_t *jn_options  // not owned
)
{
    json_t *child_list = _list_childs(gobj, tranger, hook, node);
    if(!child_list) {
        // Error already logged
        return 0;
    }

    int idx; json_t *child;
    json_array_foreach(child_list, idx, child) {
        if(!kw_match_simple(
            child, // NOT owned
            json_incref(jn_filter) // owned
        )){
            continue;
        }

        json_t *_child = create_jchild(gobj, tranger, hook, rename_hook, child, jn_options);
        add_jtree_path(parent, _child);

        json_t *list = kw_get_list(gobj, parent, rename_hook?rename_hook:hook, 0, KW_REQUIRED);
        json_array_append_new(list, _child);

        // recursive
        add_jtree_childs(
            gobj,
            tranger,
            tree,
            hook,
            rename_hook,
            child,
            _child,
            jn_filter,
            jn_options
        );
    }
    json_decref(child_list);

    return tree;
}

/***************************************************************************
 *  Return a list of childs of the hook
 ***************************************************************************/
PUBLIC json_t *treedb_node_jtree(
    json_t *tranger,
    const char *hook,   // hook to build the hierarchical tree
    const char *rename_hook, // change the hook name in the tree response
    json_t *node,       // NOT owned, pure node
    json_t *jn_filter,  // filter to childs tree
    json_t *jn_options  // fkey,hook options
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*------------------------------*
     *      Check original node
     *------------------------------*/
    if(!kw_get_bool(gobj, node, "__md_treedb__`__pure_node__", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Not a pure node",
            NULL
        );
        gobj_trace_json(gobj, node, "Not a pure node");
        JSON_DECREF(jn_filter)
        JSON_DECREF(jn_options)
        return 0;
    }

    if(!kw_get_dict_value(gobj, node, hook, 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "hook not found",
            "topic_name",   "%s", kw_get_str(gobj, node, "__md_treedb__`treedb_name", 0, 0),
            "hook",         "%s", hook,
            NULL
        );
        JSON_DECREF(jn_filter)
        JSON_DECREF(jn_options)
        return 0;
    }

    if(empty_string(rename_hook)) {
        rename_hook = 0;
    }

    json_t *root = create_jchild(gobj, tranger, hook, rename_hook, node, jn_options);
    add_jtree_path(0, root);

    json_t *tree = root;

    // recursive
    add_jtree_childs(gobj, tranger, tree, hook, rename_hook, node, root, jn_filter, jn_options);

    JSON_DECREF(jn_filter)
    JSON_DECREF(jn_options)
    return tree;
}




                /*----------------------------*
                 *          Schema
                 *----------------------------*/




/***************************************************************************
 *  Return a list with the cols that are links (fkeys) of the topic
 ***************************************************************************/
PUBLIC json_t *treedb_get_topic_links(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *topics = treedb_topics(tranger, treedb_name, 0);
    if(!json_str_in_list(gobj, topics, topic_name, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "topic not found",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(topics)
        return 0;
    }
    JSON_DECREF(topics)

    json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name);
    json_t *jn_list = json_array();

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        BOOL is_fkey = kw_has_word(gobj, desc_flag, "fkey", 0)?TRUE:FALSE;
        if(is_fkey) {
            json_array_append_new(jn_list, json_string(col_name));
        }
    }
    JSON_DECREF(cols)
    return jn_list;
}

/***************************************************************************
 *  Return a list with the cols that are hooks of the topic
 ***************************************************************************/
PUBLIC json_t *treedb_get_topic_hooks(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    json_t *topics = treedb_topics(tranger, treedb_name, 0);
    if(!json_str_in_list(gobj, topics, topic_name, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "topic not found",
            "treedb_name",  "%s", treedb_name,
            "topic_name",   "%s", topic_name,
            NULL
        );
        JSON_DECREF(topics)
        return 0;
    }
    JSON_DECREF(topics)

    json_t *cols = tranger2_dict_topic_desc_cols(tranger, topic_name);
    json_t *jn_list = json_array();

    const char *col_name; json_t *col;
    json_object_foreach(cols, col_name, col) {
        json_t *desc_flag = kw_get_dict_value(gobj, col, "flag", 0, 0);
        BOOL is_hook = kw_has_word(gobj, desc_flag, "hook", 0)?TRUE:FALSE;
        if(is_hook) {
            json_array_append_new(jn_list, json_string(col_name));
        }
    }
    JSON_DECREF(cols)
    return jn_list;
}




                        /*----------------------------*
                         *      Transactions
                         *----------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_begin_transaction(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *options
)
{
    // FUTURE?
    return 0;
}

PUBLIC int treedb_end_transaction(
    json_t *tranger,
    const char *treedb_name,
    const char *topic_name,
    const char *options
)
{
    // FUTURE?
    return 0;
}




                        /*----------------------------*
                         *          Snaps
                         *----------------------------*/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t * treedb_get_activated_snap_tag(
    hgobj gobj,
    json_t *tranger,
    const char *treedb_name,
    uint32_t *user_flag
)
{
    *user_flag = 0;

    json_t *jn_tag = json_pack("{s:b}",
        "active", 1
    );
    json_t *snaps = treedb_list_nodes( // Return MUST be decref
        tranger,
        treedb_name,
        "__snaps__",
        jn_tag,  // filter, owned
        0   // match_fn
    );
    int size = (int)json_array_size(snaps);
    if(size == 0) {
        JSON_DECREF(snaps)
        return 0;

    } else if(size > 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Too much actives tags",
            NULL
        );
        gobj_trace_json(gobj, snaps, "Too much actives tags");

        int idx; json_t *snap;
        json_array_foreach(snaps, idx, snap) {
            if(idx == size -1) {
                break;
            }
            json_object_set_new(snap, "active", json_false());
            treedb_save_node(tranger, snap);
        }

        snap = json_array_get(snaps, size-1);
        *user_flag = (uint32_t)kw_get_int(gobj, snap, "id", 0, KW_REQUIRED|KW_WILD_NUMBER);
        JSON_DECREF(snaps)
        return snap;

    } else {
        json_t *snap = json_array_get(snaps, 0);
        *user_flag = kw_get_int(gobj, snap, "id", 0, KW_REQUIRED|KW_WILD_NUMBER);
        JSON_DECREF(snaps)
        return snap;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_shoot_snap( // tag the current tree db
    json_t *tranger,
    const char *treedb_name,
    const char *snap_name,
    const char *description
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    /*-----------------------------------*
     *  Check if the tag already exists
     *-----------------------------------*/
    json_t *jn_tag = json_pack("{s:s}",
        "name", snap_name
    );
    json_t *snaps = treedb_list_nodes( // Return MUST be decref
        tranger,
        treedb_name,
        "__snaps__",
        jn_tag,  // filter, owned
        0   // match_fn
    );
    if(json_array_size(snaps)>0) {
        char temp[80];
        snprintf(temp, sizeof(temp), "Snap already exists: '%s'", snap_name);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", temp,
            "snap",         "%s", snap_name,
            NULL
        );
        JSON_DECREF(snaps)
        return -1;
    }
    JSON_DECREF(snaps)

    /*
     *  Register the tag
     */
    time_t t;
    time(&t);
    struct tm tm;

#ifdef WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif

    char date[80];
    strftime(date, sizeof(date), "%d %b %Y %T %z", &tm);
    json_t *jn_new_snap = json_pack("{s:s, s:s, s:s, s:b}",
        "name", snap_name,
        "date", date,
        "description", description,
        "active", 0
    );
    json_t *snap = treedb_create_node( // Return is NOT YOURS, pure node
        tranger,
        treedb_name,
        "__snaps__",
        jn_new_snap // owned
    );

    if(!snap) {
        gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "Cannot save record tag",
            "snap",         "%s", snap_name,
            NULL
        );
        return -1;
    }

    uint32_t user_flag = kw_get_int(gobj, snap, "id", 0, KW_REQUIRED|KW_WILD_NUMBER);
    if(user_flag==0 || user_flag >= 0xFFFFFFFF) {
        gobj_log_critical(gobj, kw_get_int(gobj, tranger, "on_critical_error", 0, KW_REQUIRED),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", "rowid for tag too big",
            "snap",         "%s", snap_name,
            NULL
        );
        return -1;
    }

    int ret = 0;
    json_t *topics = treedb_topics(tranger, treedb_name, 0);
    int idx; json_t *jn_topic;
    json_array_foreach(topics, idx, jn_topic) {
        const char *topic_name = json_string_value(jn_topic);
        if(strncmp(topic_name, "__", 2)==0) { // Ignore meta-tables
            continue;
        }

        /*---------------------------------*
         *  Tag each current key of topic
         *---------------------------------*/

        /*----------------------------------*
         *  Get indexx: to tag nodes
         *----------------------------------*/
        json_t *indexx = treedb_get_id_index(tranger, treedb_name, topic_name);
        if(!indexx) {
            // It's not a treedb topic
            continue;
        }

        /*
         *  Firstly create a new node. Last node can already have a snap tag
         */
        const char *node_id; json_t *node;
        json_object_foreach(indexx, node_id, node) {
            treedb_save_node(tranger, node);
            uint64_t __rowid__ = kw_get_int(gobj, node, "__md_treedb__`__rowid__", 0, KW_REQUIRED);

            ret += tranger2_write_user_flag(
                tranger,
                topic_name,
                __rowid__,
                user_flag
            );
            if(ret < 0) {
                gobj_log_critical(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Cannot write tag",
                    "topic_name",   "%s", topic_name,
                    "snap",         "%s", snap_name,
                    NULL
                );
            }
        }

        /*
         *  Mark
         */

    }

    JSON_DECREF(topics)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int treedb_activate_snap( // Activate tag, return the snap tag
    json_t *tranger,
    const char *treedb_name,
    const char *snap_name
)
{
    hgobj gobj = (hgobj)json_integer_value(json_object_get(tranger, "gobj"));

    if(strcmp(snap_name, "__clear__")==0) {
        /*-------------------------*
         *  Deactivate snap
         *-------------------------*/
        uint32_t user_flag = 0;
        json_t *old_snap = treedb_get_activated_snap_tag(
            gobj,
            tranger,
            treedb_name,
            &user_flag
        );
        if(old_snap) {
            // desactivate tag
            json_object_set_new(old_snap, "active", json_false());
            if(treedb_save_node(tranger, old_snap)<0) {
                gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_TREEDB_ERROR,
                    "msg",          "%s", "Cannot deactivate snap",
                    "snap",         "%s", snap_name,
                    NULL
                );
            }
        }
        return 0;
    }

    /*--------------------------------*
     *  Recover new activated snap
     *--------------------------------*/
    json_t *jn_tag = json_pack("{s:s}",
        "name", snap_name
    );
    json_t *snaps = treedb_list_nodes( // Return MUST be decref
        tranger,
        treedb_name,
        "__snaps__",
        jn_tag,  // filter, owned
        0   // match_fn
    );
    json_t *snap = json_array_get(snaps, 0);
    JSON_DECREF(snaps)

    if(!snap) {
        char temp[80];
        snprintf(temp, sizeof(temp), "Snap not found: '%s'", snap_name);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TREEDB_ERROR,
            "msg",          "%s", temp,
            "snap",         "%s", snap_name,
            NULL
        );
        return -1;
    }

    /*-------------------------------------*
     *  Deactivate current activated snap
     *-------------------------------------*/
    uint32_t user_flag = 0;
    json_t *old_snap = treedb_get_activated_snap_tag(
        gobj,
        tranger,
        treedb_name,
        &user_flag
    );
    if(old_snap) {
        if(snap == old_snap) {
            // Already active
            char temp[80];
            snprintf(temp, sizeof(temp), "Snap already activated: '%s'", snap_name);
            gobj_log_info(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", temp,
                "snap",         "%s", snap_name,
                NULL
            );
            return (int)user_flag;
        }
        // desactivate tag
        json_object_set_new(old_snap, "active", json_false());
        if(treedb_save_node(tranger, old_snap)<0) {
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TREEDB_ERROR,
                "msg",          "%s", "Cannot deactivate snap",
                "snap",         "%s", kw_get_str(gobj, old_snap, "name", "", KW_REQUIRED),
                NULL
            );
        }
    }

    // activate tag
    json_object_set_new(snap, "active", json_true());

    int ret = treedb_save_node(tranger, snap);
    if(ret < 0) {
        return ret;
    }

    return (int)user_flag;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *treedb_list_snaps( // Return MUST be decref, list of snaps
    json_t *tranger,
    const char *treedb_name,
    json_t *filter
)
{
    json_t *snaps = treedb_list_nodes( // Return MUST be decref
        tranger,
        treedb_name,
        "__snaps__",
        filter,  // filter, owned
        0   // match_fn
    );

    return snaps;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *create_template_record(
    const char *template_name,
    json_t *cols,       // NOT owned
    json_t *kw          // Owned
)
{
    hgobj gobj = 0;
    json_t *new_record = json_object();
    if(!template_name) {
        template_name = "";
    }
    if(!kw) {
        kw = json_object();
    }

    const char *field; json_t *col;
    json_object_foreach(cols, field, col) {
        json_t *value = kw_get_dict_value(gobj, kw, field, 0, 0);
        if(!value) {
            value = kw_get_dict_value(gobj, col, "default", 0, 0);
        }
        set_tranger_field_value(
            template_name,
            field,
            col,
            new_record,
            value,
            TRUE
        );
    }

    JSON_DECREF(kw)

    return new_record;
}
