/***********************************************************************
 *          C_TEST_SYSTEM_SCHEMA.C
 *
 *          GClass to test the __system__ meta-treedb of C_TREEDB
 *
 *          A treedb schema lives in two places: the literal compiled in C
 *          and the __system__ treedb, where the same schema is stored AS
 *          DATA (topics `treedbs` -> `topics` -> `cols`). This test covers
 *          the second one, which is what makes a schema listable and
 *          editable at runtime:
 *
 *              1) opening a treedb projects its schema into __system__,
 *                 with every column carrying its name in `value` (the
 *                 column pkey2 — `id` is the qualified name),
 *              2) that projection alone can rebuild the schema: with the
 *                 schema file deleted, re-opening the treedb reconstructs
 *                 the very same topics and columns from __system__.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <limits.h>

#include "c_test_system_schema.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TREEDB_NAME     "treedb_test1"
#define DELETE_TREEDB_NAME "treedb_to_delete"
#define SYSTEM_TREEDB   "treedb_system_schema"

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "level of help"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-------json_fn---------description--*/
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,    cmd_help,       "Command's help"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name--------------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *      Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj gobj_treedbs;
    hgobj timer;
    char path_database[PATH_MAX];
} PRIVATE_DATA;

/***************************************************************************
 *  Client schema: three topics, one of them marked system_topic and the
 *  self-referent one (`fidelity`) marked main_topic, so the round-trip of
 *  both flags is covered too.
 ***************************************************************************/
PRIVATE char schema_test1[] = "\
{                                                                   \n\
    'id': '"TREEDB_NAME"',                                          \n\
    'schema_version': 1,                                            \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'users',                                          \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': 1,                                     \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'username': {                                       \n\
                    'header': 'User Name',                          \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'departments': {                                    \n\
                    'header': 'Department',                         \n\
                    'fillspace': 20,                                \n\
                    'type': 'array',                                \n\
                    'flag': ['fkey']                                \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'id': 'departments',                                    \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': 1,                                     \n\
            'system_topic': true,                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Name',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'users': {                                          \n\
                    'header': 'Users',                              \n\
                    'fillspace': 20,                                \n\
                    'type': 'array',                                \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'users': 'departments'                      \n\
                    }                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'id': 'fidelity',                                       \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': 1,                                     \n\
            'main_topic': true,                                     \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'parent': {                                         \n\
                    'header': 'Parent',                             \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'probe': {                                          \n\
                    'header': 'Probe',                              \n\
                    'fillspace': 7,                                 \n\
                    'type': 'dict',                                 \n\
                    'placeholder': 'a placeholder',                 \n\
                    'flag': ['hook'],                               \n\
                    'hook': {'fidelity': 'parent'},                 \n\
                    'enum': ['one','two'],                          \n\
                    'template': {'shape': 1},                       \n\
                    'pkey2s': 'a_key',                              \n\
                    'default': 'a scalar default',                  \n\
                    'description': 'every attribute at once',       \n\
                    'properties': {'p': 1}                          \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************************
 *  Same treedb, schema moved forward: `schema_version` 1 -> 2, `users`
 *  gains a column and re-headers another (its `topic_version` moves too, or
 *  the persisted topic_cols.json would mask the change). `departments` is
 *  left untouched on purpose.
 ***************************************************************************/
/*
 *  One treedb, two topics, so the delete check sees more than one child
 *  at each level: a loop that deletes the first and stops would pass a
 *  one-topic treedb.
 */
PRIVATE char schema_to_delete[] = "\
{                                                                   \n\
    'id': '"DELETE_TREEDB_NAME"',                                   \n\
    'schema_version': 1,                                            \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'alfa',                                           \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                          \n\
            'topic_version': 1,                                     \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Name',                               \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'id': 'beta',                                           \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                          \n\
            'topic_version': 1,                                     \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

PRIVATE char schema_test2[] = "\
{                                                                   \n\
    'id': '"TREEDB_NAME"',                                          \n\
    'schema_version': 2,                                            \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'users',                                          \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': 2,                                     \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'username': {                                       \n\
                    'header': 'Login',                              \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'email': {                                          \n\
                    'header': 'Email',                              \n\
                    'fillspace': 30,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'departments': {                                    \n\
                    'header': 'Department',                         \n\
                    'fillspace': 20,                                \n\
                    'type': 'array',                                \n\
                    'flag': ['fkey']                                \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'id': 'departments',                                    \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': 1,                                     \n\
            'system_topic': true,                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Name',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'users': {                                          \n\
                    'header': 'Users',                              \n\
                    'fillspace': 20,                                \n\
                    'type': 'array',                                \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'users': 'departments'                      \n\
                    }                                               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Prepare paths
     */
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    build_path(
        priv->path_database,
        sizeof(priv->path_database),
        path_root,
        "c_treedb_system_schema",
        NULL
    );
    rmrdir(priv->path_database);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Opened from __system__: the projection is what this test is about
     */
    json_t *kw_treedbs = json_pack("{s:s, s:s, s:b, s:i, s:i, s:i, s:b}",
        "path", priv->path_database,
        "filename_mask", "%Y",
        "master", 1,
        "xpermission", 02770,
        "rpermission", 0660,
        "exit_on_error", LOG_OPT_TRACE_STACK,
        "impose_c_schema", 0
    );
    priv->gobj_treedbs = gobj_create_service(
        "treedbs",
        C_TREEDB,
        kw_treedbs,
        gobj
    );
    gobj_start_tree(priv->gobj_treedbs);

    set_timeout(priv->timer, 100);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_find_service(TREEDB_NAME, FALSE)) {
        json_t *jn_resp = gobj_command(
            priv->gobj_treedbs,
            "close-treedb",
            json_pack("{s:s}", "treedb_name", TREEDB_NAME),
            gobj
        );
        JSON_DECREF(jn_resp)
    }
    gobj_stop_tree(priv->gobj_treedbs);

    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  How many nodes of `topic_name` in __system__ have an id that begins
 *  with `prefix`. -1 if __system__ is not there.
 *
 *  By PREFIX because the ids of the projection are qualified -- a topic
 *  is `<treedb>.<topic>`, a column `<treedb>.<topic>.<col>` -- so one
 *  prefix counts everything a treedb owns at any level. Counting by the
 *  BARE name instead makes the answer depend on the other treedbs of the
 *  store: `value == "name"` found the column of a different treedb and
 *  read a clean delete as a leftover.
 ***************************************************************************/
PRIVATE int system_count_under(hgobj gobj, const char *topic_name, const char *prefix)
{
    hgobj gobj_node_system = gobj_find_service(SYSTEM_TREEDB, FALSE);
    if(!gobj_node_system) {
        return -1;
    }

    json_t *nodes = gobj_list_nodes(gobj_node_system, topic_name, 0, 0, gobj);
    int count = 0;
    size_t len = strlen(prefix);
    int idx; json_t *node;
    json_array_foreach(nodes, idx, node) {
        const char *id = kw_get_str(gobj, node, "id", "", 0);
        if(strncmp(id, prefix, len)==0) {
            count++;
        }
    }
    JSON_DECREF(nodes)

    return count;
}

/***************************************************************************
 *  Open the test treedb. With `forced_by_code` the open imposes the schema
 *  from C whatever C_TREEDB's impose_c_schema says, the way a yuno's code
 *  does it; without it the attribute decides (off in this test, so the
 *  __system__ treedb is the schema source).
 ***************************************************************************/
PRIVATE int open_test_treedb_with(hgobj gobj, json_t *jn_schema, BOOL forced_by_code) // owned
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw_treedb = json_pack("{s:s, s:i, s:s}",
        "filename_mask", "%Y",
        "exit_on_error", 0,
        "treedb_name", TREEDB_NAME
    );
    if(forced_by_code) {
        json_object_set_new(kw_treedb, "impose_c_schema", json_true());
    }
    if(jn_schema) {
        json_object_set_new(kw_treedb, "treedb_schema", jn_schema);
    }

    json_t *jn_resp = gobj_command(priv->gobj_treedbs, "open-treedb", kw_treedb, gobj);
    int result = (int)kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED);
    if(result < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: open-treedb failed",
            "comment",      "%s", kw_get_str(gobj, jn_resp, "comment", "", 0),
            NULL
        );
    }
    JSON_DECREF(jn_resp)

    return result;
}

/***************************************************************************
 *  Open the test treedb as C_TREEDB's impose_c_schema says
 ***************************************************************************/
PRIVATE int open_test_treedb(hgobj gobj, json_t *jn_schema) // owned
{
    return open_test_treedb_with(gobj, jn_schema, FALSE);
}

/***************************************************************************
 *  The schema_version of the test treedb's schema file on disk, or -1.
 ***************************************************************************/
PRIVATE json_int_t disk_schema_version(hgobj gobj)
{
    hgobj gobj_client_tranger = gobj_find_service("tranger_" TREEDB_NAME, FALSE);
    json_t *tranger = gobj_client_tranger?
        gobj_read_pointer_attr(gobj_client_tranger, "tranger"):
        NULL;
    if(!tranger) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: client tranger not found",
            NULL
        );
        return -1;
    }
    json_t *schema_file = load_json_from_file(
        gobj,
        kw_get_str(gobj, tranger, "directory", "", 0),
        TREEDB_NAME ".treedb_schema.json",
        0
    );
    json_int_t version = kw_get_int(gobj, schema_file, "schema_version", -1, KW_WILD_NUMBER);
    JSON_DECREF(schema_file)
    return version;
}

/***************************************************************************
 *  The topic_version a topic of the test treedb carries on disk, or -1.
 ***************************************************************************/
PRIVATE json_int_t disk_topic_version(hgobj gobj, const char *topic_name)
{
    hgobj gobj_client_tranger = gobj_find_service("tranger_" TREEDB_NAME, FALSE);
    json_t *tranger = gobj_client_tranger?
        gobj_read_pointer_attr(gobj_client_tranger, "tranger"):
        NULL;
    if(!tranger) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: client tranger not found",
            NULL
        );
        return -1;
    }
    json_t *topic = kw_get_dict(gobj, kw_get_dict(gobj, tranger, "topics", 0, 0), topic_name, 0, 0);
    return kw_get_int(gobj, topic, "topic_version", -1, KW_WILD_NUMBER);
}

/***************************************************************************
 *  Return the set of column names of a topic of the open client treedb,
 *  as a dict {col_name: type}. Return MUST be decref'd.
 ***************************************************************************/
PRIVATE json_t *client_topic_cols(hgobj gobj, const char *topic_name)
{
    hgobj gobj_client_node = gobj_find_service(TREEDB_NAME, FALSE);
    if(!gobj_client_node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: client treedb service not found",
            "treedb_name",  "%s", TREEDB_NAME,
            NULL
        );
        return NULL;
    }

    json_t *desc = gobj_topic_desc(gobj_client_node, topic_name);
    if(!desc) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: topic desc not found",
            "topic_name",   "%s", topic_name,
            NULL
        );
        return NULL;
    }

    json_t *cols = json_object();
    int idx; json_t *col;
    json_array_foreach(json_object_get(desc, "cols"), idx, col) {
        const char *id = kw_get_str(gobj, col, "id", "", 0);
        const char *type = kw_get_str(gobj, col, "type", "", 0);
        if(!empty_string(id)) {
            json_object_set_new(cols, id, json_string(type));
        }
    }
    JSON_DECREF(desc)

    return cols;
}

/***************************************************************************
 *  The two marks a topic carries beyond its columns reach a viewer: the
 *  desc (`desc` / `descs`) of `fidelity`, the self-referent topic the
 *  schema marks, says `main_topic`; the one of `departments` says
 *  `system_topic` and nothing about the tree.
 ***************************************************************************/
PRIVATE int check_main_topic_desc(hgobj gobj)
{
    int result = 0;
    hgobj gobj_client_node = gobj_find_service(TREEDB_NAME, FALSE);
    if(!gobj_client_node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: client treedb service not found",
            "treedb_name",  "%s", TREEDB_NAME,
            NULL
        );
        return -1;
    }

    const char *topics[] = {"fidelity", "departments", "users", 0};
    for(int i = 0; topics[i]; i++) {
        json_t *desc = gobj_topic_desc(gobj_client_node, topics[i]);
        if(!desc) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: topic desc not found",
                "topic_name",   "%s", topics[i],
                NULL
            );
            result += -1;
            continue;
        }
        BOOL main_topic = kw_get_bool(gobj, desc, "main_topic", 0, 0);
        BOOL system_topic = kw_get_bool(gobj, desc, "system_topic", 0, 0);
        BOOL main_expected = strcmp(topics[i], "fidelity")==0? TRUE:FALSE;
        BOOL system_expected = strcmp(topics[i], "departments")==0? TRUE:FALSE;
        if(main_topic != main_expected || system_topic != system_expected) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: topic desc does not carry the topic marks",
                "topic_name",   "%s", topics[i],
                "main_topic",   "%d", (int)main_topic,
                "system_topic", "%d", (int)system_topic,
                NULL
            );
            result += -1;
        }
        JSON_DECREF(desc)
    }

    return result;
}

/***************************************************************************
 *  The id of a topic by its name: `topics` is keyed by the qualified name
 *  and holds the bare one in `value`, so a topic is addressed the same way
 *  a column is.
 ***************************************************************************/
PRIVATE int system_topic_id(hgobj gobj, const char *topic_name, char *bf, int bfsize)
{
    hgobj gobj_node_system = gobj_find_service(SYSTEM_TREEDB, FALSE);
    *bf = 0;
    if(!gobj_node_system) {
        return -1;
    }

    json_t *nodes = gobj_list_nodes(
        gobj_node_system,
        "topics",
        json_pack("{s:s}", "value", topic_name),
        0,
        gobj
    );
    int idx; json_t *node;
    json_array_foreach(nodes, idx, node) {
        if(strcmp(kw_get_str(gobj, node, "value", "", 0), topic_name)==0) {
            snprintf(bf, bfsize, "%s", kw_get_str(gobj, node, "id", "", 0));
            break;
        }
    }
    JSON_DECREF(nodes)

    return empty_string(bf)? -1: 0;
}

/***************************************************************************
 *  The topic_version a topic carries in __system__, or -1.
 ***************************************************************************/
PRIVATE json_int_t system_topic_version(hgobj gobj, const char *topic_name)
{
    hgobj gobj_node_system = gobj_find_service(SYSTEM_TREEDB, FALSE);
    if(!gobj_node_system) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: __system__ treedb service not found",
            NULL
        );
        return -1;
    }

    char topic_id[NAME_MAX];
    if(system_topic_id(gobj, topic_name, topic_id, sizeof(topic_id)) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: topic not projected in __system__",
            "topic_name",   "%s", topic_name,
            NULL
        );
        return -1;
    }

    json_t *topic = gobj_get_node(
        gobj_node_system, "topics", json_pack("{s:s}", "id", topic_id), 0, gobj
    );
    json_int_t topic_version = kw_get_int(gobj, topic, "topic_version", -1, KW_WILD_NUMBER);
    JSON_DECREF(topic)

    return topic_version;
}

/***************************************************************************
 *  Return {col_name: id} of a topic as projected in __system__, plus
 *  {col_name__header: header} so a content change can be checked too.
 *  Return MUST be decref'd.
 ***************************************************************************/
PRIVATE json_t *system_topic_cols(hgobj gobj, const char *topic_name)
{
    hgobj gobj_node_system = gobj_find_service(SYSTEM_TREEDB, FALSE);
    if(!gobj_node_system) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: __system__ treedb service not found",
            NULL
        );
        return NULL;
    }

    json_t *tree = gobj_node_tree(
        gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", TREEDB_NAME),
        json_object(),
        gobj
    );
    if(!tree) {
        return NULL;    // Error already logged
    }

    json_t *topic = NULL;
    const char *k; json_t *t;
    json_object_foreach(kw_get_dict(gobj, tree, "topics", 0, 0), k, t) {
        if(strcmp(kw_get_str(gobj, t, "value", "", 0), topic_name)==0) {
            topic = t;
            break;
        }
    }

    json_t *cols = json_object();
    const char *col_id; json_t *col;
    json_object_foreach(json_object_get(topic, "cols"), col_id, col) {
        const char *value = kw_get_str(gobj, col, "value", "", 0);
        if(empty_string(value)) {
            continue;
        }
        json_object_set_new(cols, value, json_string(col_id));

        char header_key[NAME_MAX];
        snprintf(header_key, sizeof(header_key), "%s__header", value);
        json_object_set_new(
            cols,
            header_key,
            json_string(kw_get_str(gobj, col, "header", "", 0))
        );
    }
    JSON_DECREF(tree)

    return cols;
}

/***************************************************************************
 *  Return a version field of the treedb node stored in __system__
 ***************************************************************************/
PRIVATE json_int_t system_schema_version(hgobj gobj, const char *field)
{
    hgobj gobj_node_system = gobj_find_service(SYSTEM_TREEDB, FALSE);
    if(!gobj_node_system) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: __system__ treedb service not found",
            NULL
        );
        return -1;
    }

    json_t *treedbs = gobj_list_nodes(
        gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", TREEDB_NAME),
        0,
        gobj
    );
    json_int_t version = kw_get_int(
        gobj,
        json_array_get(treedbs, 0),
        field,
        -1,
        KW_WILD_NUMBER
    );
    JSON_DECREF(treedbs)

    return version;
}

/***************************************************************************
 *  Count the __md_treedb__ of a tree copy, and how many say pure_node true.
 ***************************************************************************/
PRIVATE void count_md_pure(json_t *jn, int *total, int *pure)
{
    if(json_is_object(jn)) {
        const char *key; json_t *value;
        json_object_foreach(jn, key, value) {
            if(strcmp(key, "__md_treedb__")==0) {
                (*total)++;
                if(json_is_true(json_object_get(value, "pure_node"))) {
                    (*pure)++;
                }
                continue;
            }
            count_md_pure(value, total, pure);
        }
    } else if(json_is_array(jn)) {
        size_t idx; json_t *value;
        json_array_foreach(jn, idx, value) {
            count_md_pure(value, total, pure);
        }
    }
}

/***************************************************************************
 *  node-tree with_metadata is a deep COPY of the tree: every __md_treedb__
 *  in it must say pure_node false, or a pure_node guard would take a copy
 *  for the node of the index. The node of the index keeps true.
 ***************************************************************************/
PRIVATE int check_node_tree_copy_not_pure(hgobj gobj)
{
    hgobj gobj_node_system = gobj_find_service(SYSTEM_TREEDB, FALSE);
    if(!gobj_node_system) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: __system__ treedb service not found",
            NULL
        );
        return -1;
    }

    json_t *tree = gobj_node_tree(
        gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", TREEDB_NAME),
        json_pack("{s:b}", "with_metadata", 1),
        gobj
    );
    if(!tree) {
        return -1;  // Error already logged
    }

    int result = 0;
    int total = 0, pure = 0;
    count_md_pure(tree, &total, &pure);
    JSON_DECREF(tree)

    if(total < 2 || pure != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: node-tree copy carries pure_node true",
            "total_md",     "%d", total,
            "pure",         "%d", pure,
            NULL
        );
        result = -1;
    }

    json_t *tranger = gobj_read_pointer_attr(gobj_node_system, "tranger");
    json_t *node = treedb_get_node(
        tranger,
        gobj_read_str_attr(gobj_node_system, "treedb_name"),
        "treedbs",
        TREEDB_NAME
    );
    if(!kw_get_bool(gobj, node, "__md_treedb__`pure_node", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: the node of the index lost pure_node",
            NULL
        );
        result = -1;
    }

    return result;
}

/***************************************************************************
 *  Check the __system__ treedb holds the projection of the test schema
 ***************************************************************************/
PRIVATE int check_system_treedb(hgobj gobj)
{
    int result = 0;

    hgobj gobj_node_system = gobj_find_service(SYSTEM_TREEDB, FALSE);
    if(!gobj_node_system) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: __system__ treedb service not found",
            NULL
        );
        return -1;
    }

    /*-----------------------------------------------*
     *  One treedb node, with its schema_version
     *-----------------------------------------------*/
    json_t *treedbs = gobj_list_nodes(gobj_node_system, "treedbs", 0, 0, gobj);
    if(json_array_size(treedbs) != 1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: wrong number of treedbs in __system__",
            "size",         "%d", (int)json_array_size(treedbs),
            NULL
        );
        result += -1;
    } else {
        json_t *treedb = json_array_get(treedbs, 0);
        const char *id = kw_get_str(gobj, treedb, "id", "", 0);
        json_int_t schema_version = kw_get_int(gobj, treedb, "schema_version", 0, 0);
        if(strcmp(id, TREEDB_NAME)!=0 || schema_version != 1) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_INTERNAL,
                "msg",              "%s", "TEST FAIL: wrong treedb node in __system__",
                "id",               "%s", id,
                "schema_version",   "%d", (int)schema_version,
                NULL
            );
            result += -1;
        }
    }
    JSON_DECREF(treedbs)

    /*-----------------------------------------------*
     *  Two topic nodes, system_topic preserved
     *-----------------------------------------------*/
    json_t *topics = gobj_list_nodes(gobj_node_system, "topics", 0, 0, gobj);
    if(json_array_size(topics) != 3) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: wrong number of topics in __system__",
            "size",         "%d", (int)json_array_size(topics),
            NULL
        );
        result += -1;
    } else {
        int idx; json_t *topic;
        json_array_foreach(topics, idx, topic) {
            const char *id = kw_get_str(gobj, topic, "value", "", 0);
            BOOL main_topic = kw_get_bool(gobj, topic, "main_topic", 0, 0);
            BOOL main_expected = strcmp(id, "fidelity")==0? TRUE:FALSE;
            if(main_topic != main_expected) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "TEST FAIL: main_topic not preserved in __system__",
                    "topic_name",   "%s", id,
                    "main_topic",   "%d", (int)main_topic,
                    NULL
                );
                result += -1;
            }
            BOOL system_topic = kw_get_bool(gobj, topic, "system_topic", 0, 0);
            BOOL expected = strcmp(id, "departments")==0? TRUE:FALSE;
            if(system_topic != expected) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "TEST FAIL: system_topic not preserved in __system__",
                    "topic_name",   "%s", id,
                    "system_topic", "%d", (int)system_topic,
                    NULL
                );
                result += -1;
            }
        }
    }
    JSON_DECREF(topics)

    /*-----------------------------------------------*
     *  Six column nodes, each carrying its name in
     *  `value`. This is the regression guard: with
     *  `value` missing from the meta-schema the nodes
     *  are stored nameless and no schema can be
     *  rebuilt from them.
     *-----------------------------------------------*/
    json_t *cols = gobj_list_nodes(gobj_node_system, "cols", 0, 0, gobj);
    if(json_array_size(cols) != 9) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: wrong number of cols in __system__",
            "size",         "%d", (int)json_array_size(cols),
            NULL
        );
        result += -1;
    }
    int idx; json_t *col;
    json_array_foreach(cols, idx, col) {
        const char *value = kw_get_str(gobj, col, "value", "", 0);
        const char *header = kw_get_str(gobj, col, "header", "", 0);
        if(empty_string(value) || empty_string(header)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: col node without value/header in __system__",
                "col",          "%j", col,
                NULL
            );
            result += -1;
        }
    }
    JSON_DECREF(cols)

    /*-----------------------------------------------*
     *  Every node is addressed by its qualified
     *  name: the id of its parent, a dot, and its
     *  own name. It is what makes the id reproduce —
     *  a rowid handed out from the topic size does
     *  not — and it is what a second treedb of the
     *  same store declaring `users` needs so as not
     *  to collide with the first.
     *-----------------------------------------------*/
    char topic_id[NAME_MAX];
    if(system_topic_id(gobj, "users", topic_id, sizeof(topic_id))<0 ||
        strcmp(topic_id, TREEDB_NAME ".users")!=0
    ) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: topic not keyed by its qualified name",
            "id",           "%s", topic_id,
            "expected",     "%s", TREEDB_NAME ".users",
            NULL
        );
        result += -1;
    }

    json_t *users_cols = system_topic_cols(gobj, "users");
    const char *username_id = json_string_value(json_object_get(users_cols, "username"));
    if(!username_id || strcmp(username_id, TREEDB_NAME ".users.username")!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: column not keyed by its qualified name",
            "id",           "%s", username_id?username_id:"",
            "expected",     "%s", TREEDB_NAME ".users.username",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(users_cols)

    return result;
}

/***************************************************************************
 *  Every attribute a column MAY declare has to survive the projection.
 *
 *  The list is not written here on purpose: it is read from the descriptor
 *  a user column answers to (_treedb_create_topic_cols_desc), so an
 *  attribute added there without storage behind it fails this test instead
 *  of disappearing from every schema in silence. That is how `enum` and
 *  `template` were lost — a column kept its `enum` FLAG while its list
 *  evaporated, so it declared an enumeration it no longer had.
 ***************************************************************************/
PRIVATE int check_column_fidelity(hgobj gobj)
{
    int result = 0;

    hgobj gobj_node_system = gobj_find_service(SYSTEM_TREEDB, FALSE);
    hgobj gobj_client_node = gobj_find_service(TREEDB_NAME, FALSE);
    if(!gobj_node_system || !gobj_client_node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: services not found for the fidelity check",
            NULL
        );
        return -1;
    }

    /*
     *  What the treedb serves now is what the projection rebuilt
     */
    json_t *desc = gobj_topic_desc(gobj_client_node, "fidelity");
    json_t *rebuilt = NULL;
    int idx; json_t *col;
    json_array_foreach(json_object_get(desc, "cols"), idx, col) {
        if(strcmp(kw_get_str(gobj, col, "id", "", 0), "probe")==0) {
            rebuilt = col;
            break;
        }
    }
    if(!rebuilt) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: the probe column did not survive at all",
            NULL
        );
        JSON_DECREF(desc)
        return -1;
    }

    json_t *cols_desc = _treedb_create_topic_cols_desc();
    json_t *entry;
    json_array_foreach(cols_desc, idx, entry) {
        const char *attr = kw_get_str(gobj, entry, "id", "", 0);
        if(empty_string(attr) || strcmp(attr, "id")==0) {
            continue;   /*  the column's own name, checked by finding it  */
        }
        json_t *value = json_object_get(rebuilt, attr);
        if(!value || json_is_null(value)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: a declared column attribute was lost",
                "attr",         "%s", attr,
                NULL
            );
            result += -1;
            continue;
        }
        /*
         *  An attribute the probe sets must come back with its value, not
         *  with the empty shape a lost value leaves behind.
         */
        if(json_is_object(value) && json_object_size(value)==0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: a declared column attribute came back empty",
                "attr",         "%s", attr,
                NULL
            );
            result += -1;
        }
    }
    JSON_DECREF(cols_desc)
    JSON_DECREF(desc)

    return result;
}

/***************************************************************************
 *  The order of the columns is part of a schema: it is the order a table
 *  paints them in and the order a form asks for them.
 *
 *  A schema rebuilt from its __system__ projection must come back in the
 *  order it was DECLARED in. The projection is a treedb like any other, and
 *  the order its nodes come back in belongs to the filesystem -- so the
 *  index has to be stored, and read back, for the schema to keep its shape.
 ***************************************************************************/
PRIVATE int check_schema_order(hgobj gobj, json_t *jn_schema)
{
    int result = 0;

    hgobj gobj_client_node = gobj_find_service(TREEDB_NAME, FALSE);
    if(!gobj_client_node) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: client treedb service not found",
            NULL
        );
        return -1;
    }

    /*
     *  The topics, in the order the schema declares them
     */
    json_t *declared_topics = json_array();
    int idx; json_t *jn_topic;
    json_array_foreach(json_object_get(jn_schema, "topics"), idx, jn_topic) {
        json_array_append_new(
            declared_topics,
            json_string(kw_get_str(gobj, jn_topic, "id", "", 0))
        );
    }

    /*
     *  What is served and NOT declared is not a mismatch: `__snaps__` and
     *  `__graphs__` are the treedb's own, and a topic the projection keeps
     *  after C stopped declaring it is an operator's. What is checked is
     *  that everything the schema declares is there, in its order.
     */
    json_t *all_topics = gobj_treedb_topics(gobj_client_node, TREEDB_NAME, 0, gobj);
    json_t *served_topics = json_array();
    int idx1; json_t *jn_name;
    json_array_foreach(all_topics, idx1, jn_name) {
        if(json_str_in_list(gobj, declared_topics, json_string_value(jn_name), FALSE)) {
            json_array_append(served_topics, jn_name);
        }
    }
    JSON_DECREF(all_topics)

    if(!json_equal(declared_topics, served_topics)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: the topics came back in another order",
            "declared",     "%j", declared_topics,
            "served",       "%j", served_topics,
            NULL
        );
        result += -1;
    }
    JSON_DECREF(served_topics)
    JSON_DECREF(declared_topics)

    /*
     *  And the columns of each topic
     */
    json_array_foreach(json_object_get(jn_schema, "topics"), idx, jn_topic) {
        const char *topic_name = kw_get_str(gobj, jn_topic, "id", "", 0);

        json_t *declared_cols = json_array();
        const char *col_name; json_t *jn_col;
        json_object_foreach(json_object_get(jn_topic, "cols"), col_name, jn_col) {
            json_array_append_new(declared_cols, json_string(col_name));
        }

        json_t *served_cols = json_array();
        json_t *desc = gobj_topic_desc(gobj_client_node, topic_name);
        int idx2; json_t *col;
        json_array_foreach(json_object_get(desc, "cols"), idx2, col) {
            const char *served_name = kw_get_str(gobj, col, "id", "", 0);
            if(json_str_in_list(gobj, declared_cols, served_name, FALSE)) {
                json_array_append_new(served_cols, json_string(served_name));
            }
        }
        JSON_DECREF(desc)

        if(!json_equal(declared_cols, served_cols)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: the columns came back in another order",
                "topic_name",   "%s", topic_name,
                "declared",     "%j", declared_cols,
                "served",       "%j", served_cols,
                NULL
            );
            result += -1;
        }
        JSON_DECREF(declared_cols)
        JSON_DECREF(served_cols)
    }

    return result;
}

/***************************************************************************
 *  A schema write that cannot produce a working schema must be refused at
 *  the point of writing: stored, it breaks the treedb at its next open,
 *  far from whoever wrote it.
 ***************************************************************************/
PRIVATE int check_refused_writes(hgobj gobj, json_t *col_ids)
{
    int result = 0;

    hgobj gobj_node_system = gobj_find_service(SYSTEM_TREEDB, FALSE);
    if(!gobj_node_system) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: __system__ treedb service not found",
            NULL
        );
        return -1;
    }

    /*
     *  A column is created under its topic: `cols` keys by the qualified
     *  name, so the fkey is what the id is composed from. Without it the
     *  create fails for want of a parent and never reaches the check below.
     */
    char users_topic_id[NAME_MAX];
    system_topic_id(gobj, "users", users_topic_id, sizeof(users_topic_id));
    char users_fkey[NAME_MAX + sizeof("topics^^cols")];
    snprintf(users_fkey, sizeof(users_fkey), "topics^%s^cols", users_topic_id);

    /*
     *  A type outside the enum the meta-schema declares
     */
    json_t *bad = gobj_create_node(
        gobj_node_system,
        "cols",
        json_pack("{s:s, s:s, s:s, s:s}",
            "value", "bad_col",
            "header", "Bad",
            "type", "xinteger",
            "topics", users_fkey
        ),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    if(bad) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a column with a type outside the enum was stored",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(bad)

    /*
     *  The same on an update, which used to store anything it was handed.
     *  The stored value must be untouched afterwards.
     */
    const char *username_id = json_string_value(json_object_get(col_ids, "username"));
    json_t *updated = gobj_update_node(
        gobj_node_system,
        "cols",
        json_pack("{s:s, s:s}",
            "id", username_id?username_id:"",
            "type", "xinteger"
        ),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    if(updated) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: an update to a type outside the enum was stored",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(updated)

    json_t *cols_now = system_topic_cols(gobj, "users");
    json_t *username = gobj_get_node(
        gobj_node_system,
        "cols",
        json_pack("{s:s}", "id", username_id?username_id:""),
        0,
        gobj
    );
    const char *type_now = kw_get_str(gobj, username, "type", "", 0);
    if(strcmp(type_now, "string")!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a refused update left the node changed",
            "type",         "%s", type_now,
            NULL
        );
        result += -1;
    }
    JSON_DECREF(username)
    JSON_DECREF(cols_now)

    /*
     *  The per-column rules an open applies (M7 of the 2026-09-21 review):
     *  stored anyway, each of these lost the topic at the next open, or
     *  refused every record of it.
     */
    {
        const char *names[] = {"bad_file", "bad_hook_fkey", "bad_hook_type", NULL};
        json_t *defs[] = {
            json_pack("{s:s, s:s, s:s, s:[s,s], s:s}",
                "value", "bad_file", "header", "Bad file", "type", "integer",
                "flag", "fkey", "file", "topics", users_fkey),
            json_pack("{s:s, s:s, s:s, s:[s,s], s:s}",
                "value", "bad_hook_fkey", "header", "Bad", "type", "array",
                "flag", "hook", "fkey", "topics", users_fkey),
            json_pack("{s:s, s:s, s:s, s:[s], s:s}",
                "value", "bad_hook_type", "header", "Bad", "type", "integer",
                "flag", "hook", "topics", users_fkey),
        };
        for(int i = 0; names[i]; i++) {
            json_t *stored = gobj_create_node(
                gobj_node_system, "cols", defs[i], json_pack("{s:b}", "refs", 1), gobj
            );
            if(stored) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "TEST FAIL: a column an open refuses was stored",
                    "col",          "%s", names[i],
                    NULL
                );
                result += -1;
            }
            JSON_DECREF(stored)
        }
    }

    /*
     *  A topic's pkey cannot change once the topic exists: topic_desc.json
     *  is written at creation and never rewritten, so the change would be
     *  stored, shown by every reader, and ignored by the topic for good.
     */
    json_t *retopic = gobj_update_node(
        gobj_node_system,
        "topics",
        json_pack("{s:s, s:s}",
            "id", users_topic_id,
            "pkey", "username"
        ),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    if(retopic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: the pkey of an existing topic was changed",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(retopic)

    /*
     *  Two columns with the same name in one topic: the name is the key a
     *  schema is rebuilt by, so the duplicate would drop one definition.
     *
     *  The qualified key refuses it where it is born — same topic, same
     *  name, same id — so the create never returns a node.
     */
    json_t *twin = gobj_create_node(
        gobj_node_system,
        "cols",
        json_pack("{s:s, s:s, s:s, s:s}",
            "value", "username",
            "header", "Twin",
            "type", "string",
            "topics", users_fkey
        ),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    if(twin) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a duplicate column name was created in a topic",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(twin)

    /*
     *  The guard behind that one is still the link: a column born under
     *  another topic carries an id of its own, and nothing about the id
     *  says the NAME is already taken where it is being hooked.
     */
    char departments_topic_id[NAME_MAX];
    system_topic_id(gobj, "departments", departments_topic_id, sizeof(departments_topic_id));
    char departments_fkey[NAME_MAX + sizeof("topics^^cols")];
    snprintf(departments_fkey, sizeof(departments_fkey),
        "topics^%s^cols", departments_topic_id
    );

    json_t *stranger = gobj_create_node(
        gobj_node_system,
        "cols",
        json_pack("{s:s, s:s, s:s, s:s}",
            "value", "username",
            "header", "Stranger",
            "type", "string",
            "topics", departments_fkey
        ),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    json_t *users_topic = gobj_get_node(
        gobj_node_system,
        "topics",
        json_pack("{s:s}", "id", users_topic_id),
        0,
        gobj
    );
    if(stranger && users_topic) {
        int ret = gobj_link_nodes(
            gobj_node_system,
            "cols",                     // hook
            "topics",                   // parent_topic_name
            json_incref(users_topic),   // parent_record, owned
            "cols",                     // child_topic_name
            json_incref(stranger),      // child_record, owned
            gobj
        );
        if(ret == 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: a duplicate column name was linked into a topic",
                NULL
            );
            result += -1;
        }
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: cannot set up the duplicate column check",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(users_topic)

    /*
     *  Take it away again: it is a column of `departments` now, and no
     *  later check should have to know this one ran.
     */
    if(stranger) {
        gobj_delete_node(
            gobj_node_system,
            "cols",
            json_pack("{s:s}", "id", kw_get_str(gobj, stranger, "id", "", 0)),
            json_pack("{s:b}", "force", 1),
            gobj
        );
    }
    JSON_DECREF(stranger)

    return result;
}

/***************************************************************************
 *  A change to a schema publishes itself.
 *
 *  Raising `topic_version` and `schema_version` is what makes a change
 *  visible, and forgetting either does nothing and says nothing. Leaving
 *  that to whoever writes means every editor carries the rule; so the write
 *  carries it, and this checks that a caller who only edits a column ends
 *  up with both numbers moved.
 ***************************************************************************/
PRIVATE int check_autopublished_versions(hgobj gobj, json_t *col_ids)
{
    int result = 0;

    hgobj gobj_node_system = gobj_find_service(SYSTEM_TREEDB, FALSE);
    if(!gobj_node_system) {
        return -1;  // Error already logged elsewhere
    }

    char users_id[NAME_MAX];
    system_topic_id(gobj, "users", users_id, sizeof(users_id));
    json_t *topic_before = gobj_get_node(
        gobj_node_system, "topics", json_pack("{s:s}", "id", users_id), 0, gobj
    );
    json_int_t topic_v0 = kw_get_int(gobj, topic_before, "topic_version", 0, KW_WILD_NUMBER);
    json_int_t schema_v0 = system_schema_version(gobj, "schema_version");
    JSON_DECREF(topic_before)

    const char *username_id = json_string_value(json_object_get(col_ids, "username"));
    json_t *updated = gobj_update_node(
        gobj_node_system,
        "cols",
        json_pack("{s:s, s:s}",
            "id", username_id?username_id:"",
            "header", "Login name"
        ),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    if(!updated) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a legal column edit was refused",
            NULL
        );
        return -1;
    }
    JSON_DECREF(updated)

    json_t *topic_after = gobj_get_node(
        gobj_node_system, "topics", json_pack("{s:s}", "id", users_id), 0, gobj
    );
    json_int_t topic_v1 = kw_get_int(gobj, topic_after, "topic_version", 0, KW_WILD_NUMBER);
    json_int_t schema_v1 = system_schema_version(gobj, "schema_version");
    JSON_DECREF(topic_after)

    if(topic_v1 <= topic_v0 || schema_v1 <= schema_v0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "TEST FAIL: a column edit did not publish itself",
            "topic_version",    "%d", (int)topic_v1,
            "was",              "%d", (int)topic_v0,
            "schema_version",   "%d", (int)schema_v1,
            "schema_was",       "%d", (int)schema_v0,
            NULL
        );
        result += -1;
    }

    return result;
}

/***************************************************************************
 *  A column CREATED with its link, and a column DELETED, publish
 *  themselves too (M8 of the 2026-09-21 review): only an update and a
 *  link did, so the docs' "a change to a schema publishes itself" was
 *  false for both.
 ***************************************************************************/
PRIVATE int versions_of_users(hgobj gobj, json_int_t *topic_v, json_int_t *schema_v)
{
    hgobj gobj_node_system = gobj_find_service(SYSTEM_TREEDB, FALSE);
    char users_id[NAME_MAX];
    system_topic_id(gobj, "users", users_id, sizeof(users_id));
    json_t *topic = gobj_get_node(
        gobj_node_system, "topics", json_pack("{s:s}", "id", users_id), 0, gobj
    );
    *topic_v = kw_get_int(gobj, topic, "topic_version", 0, KW_WILD_NUMBER);
    *schema_v = system_schema_version(gobj, "schema_version");
    JSON_DECREF(topic)
    return 0;
}

PRIVATE int check_create_and_delete_publish(hgobj gobj)
{
    int result = 0;
    hgobj gobj_node_system = gobj_find_service(SYSTEM_TREEDB, FALSE);
    if(!gobj_node_system) {
        return -1;  // Error already logged elsewhere
    }
    char users_topic_id[NAME_MAX];
    system_topic_id(gobj, "users", users_topic_id, sizeof(users_topic_id));
    char users_fkey[NAME_MAX + sizeof("topics^^cols")];
    snprintf(users_fkey, sizeof(users_fkey), "topics^%s^cols", users_topic_id);

    json_int_t t0, s0, t1, s1, t2, s2;
    versions_of_users(gobj, &t0, &s0);

    json_t *created = gobj_update_node(
        gobj_node_system,
        "cols",
        json_pack("{s:s, s:s, s:s, s:[s], s:s}",
            "value", "published_col", "header", "Published", "type", "string",
            "flag", "persistent", "topics", users_fkey),
        json_pack("{s:b, s:b, s:b}", "create", 1, "autolink", 1, "refs", 1),
        gobj
    );
    if(!created) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a legal column create was refused",
            NULL
        );
        return -1;
    }
    const char *created_id = kw_get_str(gobj, created, "id", "", 0);
    char col_id[NAME_MAX];
    snprintf(col_id, sizeof(col_id), "%s", created_id);
    JSON_DECREF(created)

    versions_of_users(gobj, &t1, &s1);
    if(t1 <= t0 || s1 <= s0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a column created with its link did not publish itself",
            NULL
        );
        result += -1;
    }

    /*  force: the column is linked to its topic ("has up links")  */
    if(gobj_delete_node(
            gobj_node_system, "cols", json_pack("{s:s}", "id", col_id),
            json_pack("{s:b}", "force", 1), gobj) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a column delete was refused",
            NULL
        );
        return -1;
    }
    versions_of_users(gobj, &t2, &s2);
    if(t2 <= t1 || s2 <= s1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a column delete did not publish itself",
            NULL
        );
        result += -1;
    }
    return result;
}

/***************************************************************************
 *  `diff-schema` names what the stored schema says and C does not.
 *
 *  The projector never deletes, and a version says that SOMETHING was
 *  published, never what. Run here, the answer must be exactly the three
 *  things this test left between the literal and the projection, and
 *  nothing else:
 *
 *      - the column edit of check_autopublished_versions,
 *      - the `fidelity` topic, declared by the first schema and dropped by
 *        the second, which the projection keeps because removing a topic is
 *        a deliberate action, never a side effect of an upgrade,
 *      - the `email` header the last literal changed without raising the
 *        topic_version of `users`, so it was never published.
 *
 *  Nothing else, above all: the store fills every column of a record with
 *  the empty value of its type, and reading those as differences drowns the
 *  ones somebody made.
 ***************************************************************************/
PRIVATE int check_schema_diff(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    json_t *jn_resp = gobj_command(
        priv->gobj_treedbs,
        "diff-schema",
        json_pack("{s:s}", "treedb_name", TREEDB_NAME),
        gobj
    );
    if(kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED) < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: diff-schema failed",
            "comment",      "%s", kw_get_str(gobj, jn_resp, "comment", "", 0),
            NULL
        );
        JSON_DECREF(jn_resp)
        return -1;
    }

    json_t *rows = kw_get_list(gobj, jn_resp, "data", 0, KW_REQUIRED);

    BOOL found_edit = FALSE;
    BOOL found_dropped_topic = FALSE;
    BOOL found_unpublished = FALSE;

    int idx; json_t *row;
    json_array_foreach(rows, idx, row) {
        const char *kind = kw_get_str(gobj, row, "kind", "", 0);
        const char *topic = kw_get_str(gobj, row, "topic", "", 0);
        const char *col = kw_get_str(gobj, row, "col", "", 0);
        const char *attr = kw_get_str(gobj, row, "attr", "", 0);
        const char *stored = kw_get_str(gobj, row, "stored", "", 0);
        const char *from_c = kw_get_str(gobj, row, "from_c", "", 0);

        if(strcmp(kind, "changed")==0 && strcmp(topic, "users")==0 &&
            strcmp(col, "username")==0 && strcmp(attr, "header")==0 &&
            strcmp(stored, "Login name")==0 && strcmp(from_c, "Login")==0
        ) {
            found_edit = TRUE;
            continue;
        }
        if(strcmp(kind, "only_in_stored")==0 && strcmp(topic, "fidelity")==0 &&
            empty_string(col) && empty_string(attr)
        ) {
            found_dropped_topic = TRUE;
            continue;
        }
        if(strcmp(kind, "changed")==0 && strcmp(topic, "users")==0 &&
            strcmp(col, "email")==0 && strcmp(attr, "header")==0 &&
            strcmp(stored, "E-mail")==0 && strcmp(from_c, "Mail")==0
        ) {
            found_unpublished = TRUE;
            continue;
        }

        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: diff-schema reports a difference nobody made",
            "row",          "%j", row,
            NULL
        );
        result += -1;
    }

    if(!found_edit || !found_dropped_topic || !found_unpublished) {
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_INTERNAL,
            "msg",                  "%s", "TEST FAIL: diff-schema misses a difference",
            "column_edit",          "%d", (int)found_edit,
            "dropped_topic",        "%d", (int)found_dropped_topic,
            "unpublished_change",   "%d", (int)found_unpublished,
            "rows",                 "%j", rows,
            NULL
        );
        result += -1;
    }

    JSON_DECREF(jn_resp)

    return result;
}

/***************************************************************************
 *  Drive C_TREEDB's impose_c_schema through its command.
 ***************************************************************************/
PRIVATE int set_impose_c_schema(hgobj gobj, const char *set)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_resp = gobj_command(
        priv->gobj_treedbs,
        "set-impose-c-schema",
        json_pack("{s:s}", "set", set),
        gobj
    );
    int ret = (int)kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED);
    JSON_DECREF(jn_resp)
    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: set-impose-c-schema failed",
            "set",          "%s", set,
            NULL
        );
    }
    return ret;
}

/***************************************************************************
 *  impose_c_schema: the schema from C wins, and the edit in __system__ is
 *  left alone.
 *
 *  By now the treedb went through dynamic edits: its schema file and the
 *  `users` topic on disk are NEWER than `schema_test2`, and __system__ holds
 *  the `email` header "E-mail" where the literal says "Email". Imposing
 *  opens it with the literal anyway -- the disk comes back to the literal's
 *  numbers and columns -- while __system__ keeps every change, so they can
 *  still be read, or taken back by clearing the flag.
 *
 *  Imposing DOES project into __system__ when it has no projection of the
 *  treedb or a lower schema_version. Here it has a higher one, because the
 *  edit published itself by raising it, so nothing is written and the log
 *  says the literal is behind. That is the case this test pins.
 ***************************************************************************/
PRIVATE int check_impose_c_schema(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    json_int_t system_version0 = system_schema_version(gobj, "schema_version");
    json_int_t system_users0 = system_topic_version(gobj, "users");

    if(set_impose_c_schema(gobj, "1") < 0) {
        return -1;  // Error already logged
    }

    json_t *jn_resp = gobj_command(
        priv->gobj_treedbs,
        "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", TREEDB_NAME, "force", 1),
        gobj
    );
    JSON_DECREF(jn_resp)

    json_t *jn_literal = legalstring2json(schema_test2, TRUE);
    json_int_t literal_version = kw_get_int(gobj, jn_literal, "schema_version", 0, KW_WILD_NUMBER);
    if(open_test_treedb(gobj, jn_literal) < 0) {
        set_impose_c_schema(gobj, "0");
        return -1;  // Error already logged
    }

    /*
     *  The disk: the schema file, the topic and its columns are the literal's
     */
    json_int_t file_version = disk_schema_version(gobj);
    json_int_t disk_users = disk_topic_version(gobj, "users");

    const char *disk_header = "";
    hgobj gobj_client_node = gobj_find_service(TREEDB_NAME, FALSE);
    json_t *desc = gobj_client_node? gobj_topic_desc(gobj_client_node, "users"): NULL;
    int idx; json_t *col;
    json_array_foreach(json_object_get(desc, "cols"), idx, col) {
        if(strcmp(kw_get_str(gobj, col, "id", "", 0), "email")==0) {
            disk_header = kw_get_str(gobj, col, "header", "", 0);
            break;
        }
    }

    if(file_version != literal_version || disk_users != 2 || strcmp(disk_header, "Email")!=0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "TEST FAIL: the schema from C was not imposed on disk",
            "schema_file",      "%d", (int)file_version,
            "literal_version",  "%d", (int)literal_version,
            "users_version",    "%d", (int)disk_users,
            "email_header",     "%s", disk_header,
            NULL
        );
        result += -1;
    }
    JSON_DECREF(desc)

    /*
     *  __system__: untouched, the changes are still there to be read
     */
    json_int_t system_version1 = system_schema_version(gobj, "schema_version");
    json_int_t system_users1 = system_topic_version(gobj, "users");
    json_t *system_cols = system_topic_cols(gobj, "users");
    const char *system_header = json_string_value(json_object_get(system_cols, "email__header"));
    if(system_version1 != system_version0 || system_users1 != system_users0 ||
        !system_header || strcmp(system_header, "E-mail")!=0
    ) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "TEST FAIL: imposing touched __system__",
            "schema_version",   "%d", (int)system_version1,
            "was",              "%d", (int)system_version0,
            "users_version",    "%d", (int)system_users1,
            "users_was",        "%d", (int)system_users0,
            "email_header",     "%s", system_header?system_header:"",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(system_cols)

    /*
     *  The flag is persistent: leave it off, or the next run opens imposing
     */
    if(set_impose_c_schema(gobj, "0") < 0) {
        result += -1;   // Error already logged
    }

    return result;
}

/***************************************************************************
 *  The yuno's code imposes: open-treedb impose_c_schema=1 wins over the
 *  attribute, which Test 9 left off -- the persistent value a command set.
 *
 *  First an ordinary open, from __system__, takes the disk ahead of the
 *  literal again; then the forced one brings it back, and the command says
 *  the treedb is forced.
 ***************************************************************************/
PRIVATE int check_impose_forced_by_code(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    json_t *jn_resp = gobj_command(
        priv->gobj_treedbs,
        "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", TREEDB_NAME, "force", 1),
        gobj
    );
    JSON_DECREF(jn_resp)
    if(open_test_treedb(gobj, legalstring2json(schema_test2, TRUE)) < 0) {
        return -1;  // Error already logged
    }
    json_int_t dynamic_version = disk_schema_version(gobj);

    jn_resp = gobj_command(
        priv->gobj_treedbs,
        "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", TREEDB_NAME, "force", 1),
        gobj
    );
    JSON_DECREF(jn_resp)
    json_t *jn_literal = legalstring2json(schema_test2, TRUE);
    json_int_t literal_version = kw_get_int(gobj, jn_literal, "schema_version", 0, KW_WILD_NUMBER);
    if(open_test_treedb_with(gobj, jn_literal, TRUE) < 0) {
        return -1;  // Error already logged
    }

    json_int_t file_version = disk_schema_version(gobj);
    json_int_t disk_users = disk_topic_version(gobj, "users");
    if(dynamic_version <= literal_version || file_version != literal_version || disk_users != 2) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "TEST FAIL: the code did not impose the schema from C",
            "dynamic_version",  "%d", (int)dynamic_version,
            "schema_file",      "%d", (int)file_version,
            "literal_version",  "%d", (int)literal_version,
            "users_version",    "%d", (int)disk_users,
            NULL
        );
        result += -1;
    }

    /*
     *  The attribute is still off, and the command says who overrides it
     */
    jn_resp = gobj_command(
        priv->gobj_treedbs,
        "set-impose-c-schema",
        json_object(),
        gobj
    );
    BOOL attr = kw_get_bool(gobj, jn_resp, "data`impose_c_schema", 1, 0);
    json_t *jn_forced = kw_get_list(gobj, jn_resp, "data`forced_by_code", 0, 0);
    if(attr || json_list_str_index(jn_forced, TREEDB_NAME, FALSE) < 0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "TEST FAIL: set-impose-c-schema does not say the treedb is forced",
            "response",         "%j", jn_resp,
            NULL
        );
        result += -1;
    }
    JSON_DECREF(jn_resp)

    return result;
}

/***************************************************************************
 *  `delete-treedb` deletes the PROJECTION of a treedb from __system__:
 *  the `treedbs` node, its `topics` and their `cols`. It never touches
 *  the client treedb's own data -- that is `delete-topic`'s business,
 *  and a store on disk is nobody's to remove by command.
 *
 *  It was inert while __system__ was empty and is reachable now, so
 *  what it leaves behind is what this checks: a projection half
 *  deleted is worse than one not deleted at all, because the next
 *  open reconstructs the schema FROM it.
 *
 *  A treedb of its own, opened for this and closed before the delete:
 *  the point is the projection, and taking the one every other check
 *  uses would decide the order of this file.
 ***************************************************************************/
PRIVATE int check_delete_treedb(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    /*  In place, as the other schemas of this file: legalstring2json()
     *  wants double quotes and the literal is written with single ones. */
    helper_quote2doublequote(schema_to_delete);

    /*
     *  TWICE, and the second time is the point: an operator deletes the
     *  treedb they were just looking at, which is OPEN. The command does
     *  not ask for it to be closed, so both ways have to leave the same
     *  nothing behind.
     */
    int pass;
    for(pass = 0; pass < 2; pass++) {
        BOOL close_first = (pass == 0);

        json_t *jn_resp = gobj_command(
            priv->gobj_treedbs,
            "open-treedb",
            json_pack("{s:s, s:s, s:i, s:o}",
                "filename_mask", "%Y",
                "treedb_name", DELETE_TREEDB_NAME,
                "exit_on_error", 0,
                "treedb_schema", legalstring2json(schema_to_delete, TRUE)
            ),
            gobj
        );
        if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: cannot open the treedb to delete",
                "close_first",  "%d", close_first?1:0,
                "comment",      "%s", kw_get_str(gobj, jn_resp, "comment", "", 0),
                NULL
            );
            JSON_DECREF(jn_resp)
            return -1;
        }
        JSON_DECREF(jn_resp)

        /*
         *  What the projection holds BEFORE, so the check measures a
         *  removal and not an absence: one treedbs node, two topics,
         *  three columns.
         */
        int treedbs_before = system_count_under(gobj, "treedbs", DELETE_TREEDB_NAME);
        int topics_before = system_count_under(gobj, "topics", DELETE_TREEDB_NAME ".");
        int cols_before = system_count_under(gobj, "cols", DELETE_TREEDB_NAME ".");

        if(treedbs_before != 1 || topics_before != 2 || cols_before != 3) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: the projection to delete is not there",
                "close_first",  "%d", close_first?1:0,
                "treedbs",      "%d", treedbs_before,
                "topics",       "%d", topics_before,
                "cols",         "%d", cols_before,
                NULL
            );
            result += -1;
        }

        if(close_first) {
            /*  `force`, because the yuno PLAYS here and close-treedb
             *  refuses that without it -- and this test holds none of
             *  the treedb's handles, which is what force is for.  */
            jn_resp = gobj_command(
                priv->gobj_treedbs,
                "close-treedb",
                json_pack("{s:s, s:b}", "treedb_name", DELETE_TREEDB_NAME, "force", 1),
                gobj
            );
            if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "TEST FAIL: cannot close the treedb to delete",
                    "comment",      "%s", kw_get_str(gobj, jn_resp, "comment", "", 0),
                    NULL
                );
                result += -1;
            }
            JSON_DECREF(jn_resp)
        }

        jn_resp = gobj_command(
            priv->gobj_treedbs,
            "delete-treedb",
            json_pack("{s:s, s:b}", "treedb_name", DELETE_TREEDB_NAME, "force", 1),
            gobj
        );
        int deleted = (int)kw_get_int(gobj, jn_resp, "result", -1, 0);
        if(close_first && deleted < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: delete-treedb of a CLOSED treedb answered an error",
                "comment",      "%s", kw_get_str(gobj, jn_resp, "comment", "", 0),
                NULL
            );
            result += -1;
        }
        if(!close_first && deleted >= 0) {
            /*
             *  An OPEN treedb must be refused. Deleting its schema
             *  leaves it running with none, and the damage lands at the
             *  next open-treedb: the C_TRANGER of the old one is still
             *  alive under its name, so the create collides and the open
             *  dies with an internal "tranger client NULL". The store is
             *  orphaned -- data with no schema to read it by.
             */
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: delete-treedb deleted the schema of an OPEN treedb",
                NULL
            );
            result += -1;
        }
        JSON_DECREF(jn_resp)

        /*
         *  The system schema itself is refused by name, as its siblings
         *  refuse it: it is not projected in `treedbs`, so without the
         *  refusal the command fell through to a "not found" error.
         */
        jn_resp = gobj_command(
            priv->gobj_treedbs,
            "delete-treedb",
            json_pack("{s:s, s:b}", "treedb_name", SYSTEM_TREEDB, "force", 1),
            gobj
        );
        if(kw_get_int(gobj, jn_resp, "result", -1, 0) >= 0 ||
                !strstr(kw_get_str(gobj, jn_resp, "comment", "", 0), "system schema")) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: delete-treedb of the system schema not refused by name",
                "comment",      "%s", kw_get_str(gobj, jn_resp, "comment", "", 0),
                NULL
            );
            result += -1;
        }
        JSON_DECREF(jn_resp)

        /*
         *  Closed: nothing of it may remain -- the columns matter as
         *  much as the treedbs node, they are what a reconstruction
         *  reads. Open: the refusal must have touched nothing.
         */
        int treedbs_after = system_count_under(gobj, "treedbs", DELETE_TREEDB_NAME);
        int topics_after = system_count_under(gobj, "topics", DELETE_TREEDB_NAME ".");
        int cols_after = system_count_under(gobj, "cols", DELETE_TREEDB_NAME ".");
        int want_treedbs = close_first? 0 : 1;
        int want_topics = close_first? 0 : 2;
        int want_cols = close_first? 0 : 3;

        if(treedbs_after != want_treedbs || topics_after != want_topics ||
                cols_after != want_cols) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", close_first?
                    "TEST FAIL: delete-treedb left the projection behind":
                    "TEST FAIL: a refused delete-treedb changed the projection",
                "treedbs",      "%d", treedbs_after,
                "topics",       "%d", topics_after,
                "cols",         "%d", cols_after,
                NULL
            );
            result += -1;
        }

        /*  Left closed either way, so the next pass opens it again.  */
        if(!close_first) {
            jn_resp = gobj_command(
                priv->gobj_treedbs,
                "close-treedb",
                json_pack("{s:s, s:b}", "treedb_name", DELETE_TREEDB_NAME, "force", 1),
                gobj
            );
            JSON_DECREF(jn_resp)
        }
    }

    return result;
}

/***************************************************************************
 *  Imposing PROJECTS the schema into __system__ when nothing of that
 *  treedb is there, or when what is there is behind.
 *
 *  __system__ is the only place a schema can be ASKED for -- from ytreedb,
 *  from gui_agent, from any node command -- so a treedb that only ever
 *  opened with `impose` had no projection at all, and the schema it runs
 *  could be read from its binary and nowhere else.
 *
 *  The three cases, on the treedb `check_delete_treedb` leaves behind with
 *  its projection deleted: nothing there, behind, and ahead. The last one
 *  is the one that must NOT be written: an edit publishes itself by raising
 *  the version, and imposing does not take that away from __system__ -- it
 *  is what `diff-schema` reads, and what clearing the flag brings back.
 ***************************************************************************/
PRIVATE int open_treedb_to_delete_imposing(hgobj gobj, json_t *jn_schema) // owned
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_resp = gobj_command(
        priv->gobj_treedbs,
        "open-treedb",
        json_pack("{s:s, s:s, s:i, s:b, s:o}",
            "filename_mask", "%Y",
            "treedb_name", DELETE_TREEDB_NAME,
            "exit_on_error", 0,
            "impose_c_schema", 1,
            "treedb_schema", jn_schema
        ),
        gobj
    );
    int ret = (int)kw_get_int(gobj, jn_resp, "result", -1, 0);
    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: cannot open imposing the treedb to project",
            "comment",      "%s", kw_get_str(gobj, jn_resp, "comment", "", 0),
            NULL
        );
    }
    JSON_DECREF(jn_resp)

    return ret<0? -1: 0;
}

PRIVATE int close_treedb_to_delete(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_resp = gobj_command(
        priv->gobj_treedbs,
        "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", DELETE_TREEDB_NAME, "force", 1),
        gobj
    );
    JSON_DECREF(jn_resp)

    return 0;
}

PRIVATE json_int_t projected_schema_version(hgobj gobj, const char *treedb_name)
{
    hgobj gobj_node_system = gobj_find_service(SYSTEM_TREEDB, FALSE);
    if(!gobj_node_system) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: __system__ treedb service not found",
            NULL
        );
        return -1;
    }

    json_t *treedbs = gobj_list_nodes(
        gobj_node_system,
        "treedbs",
        json_pack("{s:s}", "id", treedb_name),
        0,
        gobj
    );
    json_int_t version = kw_get_int(
        gobj,
        json_array_get(treedbs, 0),
        "schema_version",
        -1,
        KW_WILD_NUMBER
    );
    JSON_DECREF(treedbs)

    return version;
}

PRIVATE int check_impose_projects_into_system(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    /*
     *  Leave it as check_delete_treedb found it the first time: closed,
     *  and with no projection of its own in __system__.
     */
    close_treedb_to_delete(gobj);
    json_t *jn_resp = gobj_command(
        priv->gobj_treedbs,
        "delete-treedb",
        json_pack("{s:s, s:b}", "treedb_name", DELETE_TREEDB_NAME, "force", 1),
        gobj
    );
    JSON_DECREF(jn_resp)

    if(system_count_under(gobj, "treedbs", DELETE_TREEDB_NAME) != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: the projection to seed is still there",
            NULL
        );
        return -1;
    }

    /*
     *  Nothing there: the open SEEDS it, whole -- the treedbs node, its
     *  two topics and their three columns, which are what a reconstruction
     *  reads.
     */
    if(open_treedb_to_delete_imposing(gobj, legalstring2json(schema_to_delete, TRUE)) < 0) {
        return -1;  // Error already logged
    }

    int treedbs1 = system_count_under(gobj, "treedbs", DELETE_TREEDB_NAME);
    int topics1 = system_count_under(gobj, "topics", DELETE_TREEDB_NAME ".");
    int cols1 = system_count_under(gobj, "cols", DELETE_TREEDB_NAME ".");
    json_int_t version1 = projected_schema_version(gobj, DELETE_TREEDB_NAME);

    if(treedbs1 != 1 || topics1 != 2 || cols1 != 3 || version1 != 1) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "TEST FAIL: imposing did not seed the projection",
            "treedbs",          "%d", treedbs1,
            "topics",           "%d", topics1,
            "cols",             "%d", cols1,
            "schema_version",   "%d", (int)version1,
            NULL
        );
        result += -1;
    }

    /*
     *  Behind: a literal ahead of it re-makes it. The topic moves with it,
     *  and here it moves although its own topic_version does NOT -- which
     *  is what imposing means, on disk and here alike.
     */
    close_treedb_to_delete(gobj);

    json_t *jn_ahead = legalstring2json(schema_to_delete, TRUE);
    json_object_set_new(jn_ahead, "schema_version", json_integer(7));
    json_t *jn_alfa = json_array_get(json_object_get(jn_ahead, "topics"), 0);
    json_object_set_new(
        json_object_get(json_object_get(jn_alfa, "cols"), "name"),
        "header",
        json_string("Imposed")
    );

    if(open_treedb_to_delete_imposing(gobj, jn_ahead) < 0) {
        return result - 1;  // Error already logged
    }

    json_int_t version2 = projected_schema_version(gobj, DELETE_TREEDB_NAME);
    json_t *cols2 = gobj_list_nodes(
        gobj_find_service(SYSTEM_TREEDB, FALSE),
        "cols",
        json_pack("{s:s}", "id", DELETE_TREEDB_NAME ".alfa.name"),
        0,
        gobj
    );
    const char *header2 = kw_get_str(gobj, json_array_get(cols2, 0), "header", "", 0);

    if(version2 != 7 || strcmp(header2, "Imposed")!=0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "TEST FAIL: imposing did not re-make a projection behind the literal",
            "schema_version",   "%d", (int)version2,
            "name_header",      "%s", header2,
            NULL
        );
        result += -1;
    }
    JSON_DECREF(cols2)

    /*
     *  Ahead: nothing is written. The literal goes back to 1 and the
     *  projection keeps the 7 it published, header included.
     */
    close_treedb_to_delete(gobj);

    if(open_treedb_to_delete_imposing(gobj, legalstring2json(schema_to_delete, TRUE)) < 0) {
        return result - 1;  // Error already logged
    }

    json_int_t version3 = projected_schema_version(gobj, DELETE_TREEDB_NAME);
    json_t *cols3 = gobj_list_nodes(
        gobj_find_service(SYSTEM_TREEDB, FALSE),
        "cols",
        json_pack("{s:s}", "id", DELETE_TREEDB_NAME ".alfa.name"),
        0,
        gobj
    );
    const char *header3 = kw_get_str(gobj, json_array_get(cols3, 0), "header", "", 0);

    if(version3 != 7 || strcmp(header3, "Imposed")!=0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "TEST FAIL: imposing overwrote a projection ahead of the literal",
            "schema_version",   "%d", (int)version3,
            "name_header",      "%s", header3,
            NULL
        );
        result += -1;
    }
    JSON_DECREF(cols3)

    close_treedb_to_delete(gobj);

    return result;
}

/***************************************************************************
 *  Only the MASTER writes __system__. A replica reads the treedb from disk
 *  as it is at that moment and reconciles nothing.
 *
 *  The master's C_TREEDB is destroyed first -- with it, its `__system__`
 *  tranger and node services, whose names the replica needs -- and the
 *  store is opened again as a replica. Sequentially, never twice at once.
 *
 *  Then an open that WOULD write: the literal is far ahead of the
 *  projection test 13 left, and imposing on top of that. Nothing may move.
 ***************************************************************************/
PRIVATE int check_replica_writes_nothing(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    json_int_t version_before = projected_schema_version(gobj, DELETE_TREEDB_NAME);

    gobj_stop_tree(priv->gobj_treedbs);
    gobj_destroy(priv->gobj_treedbs);
    priv->gobj_treedbs = 0;

    json_t *kw_replica = json_pack("{s:s, s:s, s:b, s:i, s:i, s:i, s:b}",
        "path", priv->path_database,
        "filename_mask", "%Y",
        "master", 0,
        "xpermission", 02770,
        "rpermission", 0660,
        "exit_on_error", LOG_OPT_TRACE_STACK,
        "impose_c_schema", 0
    );
    priv->gobj_treedbs = gobj_create_service(
        "treedbs",
        C_TREEDB,
        kw_replica,
        gobj
    );
    if(!priv->gobj_treedbs) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: cannot create the replica C_TREEDB",
            NULL
        );
        return -1;
    }
    gobj_start_tree(priv->gobj_treedbs);

    /*
     *  It reads what the master wrote: the projection is there to begin with
     */
    json_int_t version_read = projected_schema_version(gobj, DELETE_TREEDB_NAME);
    if(version_read != version_before) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "TEST FAIL: the replica does not read the projection",
            "schema_version",   "%d", (int)version_read,
            "was",              "%d", (int)version_before,
            NULL
        );
        result += -1;
    }

    /*
     *  And writes nothing: a literal far ahead, imposed, moves neither the
     *  projection nor the topic the master left in it.
     */
    json_t *jn_ahead = legalstring2json(schema_to_delete, TRUE);
    json_object_set_new(jn_ahead, "schema_version", json_integer(99));
    json_t *jn_alfa = json_array_get(json_object_get(jn_ahead, "topics"), 0);
    json_object_set_new(jn_alfa, "topic_version", json_integer(99));
    json_object_set_new(
        json_object_get(json_object_get(jn_alfa, "cols"), "name"),
        "header",
        json_string("Replica")
    );

    open_treedb_to_delete_imposing(gobj, jn_ahead);

    json_int_t version_after = projected_schema_version(gobj, DELETE_TREEDB_NAME);
    json_t *cols = gobj_list_nodes(
        gobj_find_service(SYSTEM_TREEDB, FALSE),
        "cols",
        json_pack("{s:s}", "id", DELETE_TREEDB_NAME ".alfa.name"),
        0,
        gobj
    );
    const char *header = kw_get_str(gobj, json_array_get(cols, 0), "header", "", 0);

    if(version_after != version_before || strcmp(header, "Imposed")!=0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "TEST FAIL: a replica wrote __system__",
            "schema_version",   "%d", (int)version_after,
            "was",              "%d", (int)version_before,
            "name_header",      "%s", header,
            NULL
        );
        result += -1;
    }
    JSON_DECREF(cols)

    close_treedb_to_delete(gobj);

    return result;
}

/***************************************************************************
 *  A projection made with rowid keys moves to qualified ones.
 *
 *  `topics` and `cols` used to be keyed by a rowid handed out from the
 *  topic size. Every deployed store holds one of those, and the two
 *  conventions cannot live side by side: the schema is rebuilt by `value`,
 *  so a legacy node and its qualified twin are the same topic twice.
 *
 *  This builds a legacy projection by hand — numeric ids, the name in
 *  `value`, linked as the old projector linked them — and opens a treedb
 *  over it. What comes back must be keyed by the qualified name, with the
 *  legacy nodes gone and the content intact: a column NOT in the schema
 *  from C is an operator's, and moving it is the only way it survives its
 *  parent changing address.
 ***************************************************************************/
#define LEGACY_TREEDB   "treedb_legacy"

PRIVATE int check_legacy_ids_migrated(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    hgobj gobj_node_system = gobj_find_service(SYSTEM_TREEDB, FALSE);
    if(!gobj_node_system) {
        return -1;  // Error already logged elsewhere
    }

    /*
     *  A projection as the old projector left it
     */
    json_t *treedb = gobj_create_node(
        gobj_node_system,
        "treedbs",
        json_pack("{s:s, s:i, s:i, s:i}",
            "id", LEGACY_TREEDB,
            "schema_version", 1,
            "c_schema_version", 1,
            "system_schema_version", 1
        ),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    json_t *topic = gobj_create_node(
        gobj_node_system,
        "topics",
        json_pack("{s:s, s:s, s:s, s:s, s:i}",
            "id", "7",
            "value", "users",
            "pkey", "id",
            "system_flag", "sf_string_key",
            "topic_version", 1
        ),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    if(!treedb || !topic) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: cannot build the legacy projection",
            NULL
        );
        JSON_DECREF(treedb)
        JSON_DECREF(topic)
        return -1;
    }
    gobj_link_nodes(
        gobj_node_system,
        "topics", "treedbs", json_incref(treedb), "topics", json_incref(topic), gobj
    );

    static const char *legacy_cols[][3] = {
        {"70", "id",           "Id"},
        {"71", "username",     "Login"},
        {"72", "operator_col", "Operator"},     /*  not in the schema from C  */
        {NULL, NULL, NULL}
    };
    for(int i=0; legacy_cols[i][0]; i++) {
        json_t *col = gobj_create_node(
            gobj_node_system,
            "cols",
            json_pack("{s:s, s:s, s:s, s:s, s:[s]}",
                "id", legacy_cols[i][0],
                "value", legacy_cols[i][1],
                "header", legacy_cols[i][2],
                "type", "string",
                "flag", "persistent"
            ),
            json_pack("{s:b}", "refs", 1),
            gobj
        );
        if(!col) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: cannot build a legacy column",
                "col",          "%s", legacy_cols[i][1],
                NULL
            );
            result += -1;
            continue;
        }
        gobj_link_nodes(
            gobj_node_system,
            "cols", "topics", json_incref(topic), "cols", json_incref(col), gobj
        );
        JSON_DECREF(col)
    }
    JSON_DECREF(treedb)
    JSON_DECREF(topic)

    /*
     *  Opening it re-projects — the stored meta version is behind — and the
     *  re-projection is what the move rides on.
     */
    json_t *jn_resp = gobj_command(
        priv->gobj_treedbs,
        "open-treedb",
        json_pack("{s:s, s:i, s:s, s:o}",
            "filename_mask", "%Y",
            "exit_on_error", 0,
            "treedb_name", LEGACY_TREEDB,
            "treedb_schema", json_pack("{s:s, s:i, s:[{s:s, s:s, s:s, s:i, s:{s:{s:s, s:s, s:[s,s]}}}]}",
                "id", LEGACY_TREEDB,
                "schema_version", 1,
                "topics",
                    "id", "users",
                    "pkey", "id",
                    "system_flag", "sf_string_key",
                    "topic_version", 1,
                    "cols",
                        "id",
                            "header", "Id",
                            "type", "string",
                            "flag", "persistent", "required"
            )
        ),
        gobj
    );
    int ret = (int)kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED);
    JSON_DECREF(jn_resp)
    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: cannot open the legacy treedb",
            NULL
        );
        return -1;
    }

    /*
     *  The legacy ids are gone and the qualified ones answer
     */
    static const char *gone[] = {"7", NULL};
    for(int i=0; gone[i]; i++) {
        json_t *stale = gobj_get_node(
            gobj_node_system, "topics", json_pack("{s:s}", "id", gone[i]),
            json_pack("{s:b}", "no_verbose", 1), gobj
        );
        if(stale) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: a legacy topic id survived the move",
                "id",           "%s", gone[i],
                NULL
            );
            result += -1;
        }
        JSON_DECREF(stale)
    }

    json_t *moved = gobj_get_node(
        gobj_node_system,
        "topics",
        json_pack("{s:s}", "id", LEGACY_TREEDB ".users"),
        json_pack("{s:b}", "no_verbose", 1),
        gobj
    );
    if(!moved) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: the topic did not move to its qualified id",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(moved)

    /*
     *  The operator's column moved with its content: it is in no schema
     *  from C, so the projector would never have written it again.
     */
    json_t *operator_col = gobj_get_node(
        gobj_node_system,
        "cols",
        json_pack("{s:s}", "id", LEGACY_TREEDB ".users.operator_col"),
        json_pack("{s:b}", "no_verbose", 1),
        gobj
    );
    const char *header = kw_get_str(gobj, operator_col, "header", "", 0);
    if(!operator_col || strcmp(header, "Operator")!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: an operator column did not survive the move",
            "header",       "%s", header,
            NULL
        );
        result += -1;
    }
    JSON_DECREF(operator_col)

    /*
     *  And the treedb opens with it, which is the point of keeping it
     */
    hgobj gobj_legacy_node = gobj_find_service(LEGACY_TREEDB, FALSE);
    json_t *desc = gobj_legacy_node? gobj_topic_desc(gobj_legacy_node, "users"): NULL;
    BOOL found = FALSE;
    int idx; json_t *col;
    json_array_foreach(json_object_get(desc, "cols"), idx, col) {
        if(strcmp(kw_get_str(gobj, col, "id", "", 0), "operator_col")==0) {
            found = TRUE;
            break;
        }
    }
    if(!found) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: the moved column did not reach the open treedb",
            "desc",         "%j", desc,
            NULL
        );
        result += -1;
    }
    JSON_DECREF(desc)

    return result;
}

/***************************************************************************
 *  Run all tests — called from the timer action, inside the event loop
 ***************************************************************************/
PRIVATE int run_tests(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    /*-----------------------------------------------*
     *  Test 1: opening a treedb projects its schema
     *  into the __system__ treedb
     *-----------------------------------------------*/
    helper_quote2doublequote(schema_test1);
    json_t *jn_schema = legalstring2json(schema_test1, TRUE);
    if(!jn_schema) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: cannot parse the test schema",
            NULL
        );
        return -1;
    }

    if(open_test_treedb(gobj, json_incref(jn_schema)) < 0) {
        JSON_DECREF(jn_schema)
        return -1;  // Error already logged
    }

    result += check_system_treedb(gobj);
    result += check_column_fidelity(gobj);
    result += check_main_topic_desc(gobj);
    result += check_node_tree_copy_not_pure(gobj);
    result += check_schema_order(gobj, jn_schema);

    /*-----------------------------------------------*
     *  Test 1b: close-treedb, create-topic and
     *  delete-topic act only on a treedb THIS
     *  C_TREEDB opened. The __system__ treedb is its
     *  own child too, but its handles live in priv:
     *  closing it left them dangling. Even with force.
     *-----------------------------------------------*/
    {
        struct {
            const char *command;
            json_t *kw;
        } foreign[] = {
            {"close-treedb", json_pack("{s:s, s:b}", "treedb_name", SYSTEM_TREEDB, "force", 1)},
            {"close-treedb", json_pack("{s:s, s:b}", "treedb_name", "tranger_" TREEDB_NAME, "force", 1)},
            {"create-topic", json_pack("{s:s, s:s, s:{s:{s:s, s:s, s:[s]}}}",
                "treedb_name", SYSTEM_TREEDB, "topic_name", "intruder",
                "cols", "id", "header", "Id", "type", "string", "flag", "persistent")},
            {"delete-topic", json_pack("{s:s, s:s}", "treedb_name", SYSTEM_TREEDB, "topic_name", "cols")},
        };
        for(size_t i = 0; i < sizeof(foreign)/sizeof(foreign[0]); i++) {
            const char *target = kw_get_str(gobj, foreign[i].kw, "treedb_name", "", 0);
            char target_[NAME_MAX];
            snprintf(target_, sizeof(target_), "%s", target);
            json_t *jn_r = gobj_command(priv->gobj_treedbs, foreign[i].command, foreign[i].kw, gobj);
            if(kw_get_int(gobj, jn_r, "result", -1, KW_REQUIRED) >= 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL,
                    "msg",          "%s", "TEST FAIL: a command acted on a treedb this service did not open",
                    "command",      "%s", foreign[i].command,
                    "treedb_name",  "%s", target_,
                    NULL
                );
                result += -1;
            }
            JSON_DECREF(jn_r)
        }
        if(!gobj_find_service(SYSTEM_TREEDB, FALSE) || !gobj_find_service("tranger_" TREEDB_NAME, FALSE)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: a refused close-treedb destroyed the service",
                NULL
            );
            result += -1;
        }
    }

    json_t *cols_before = client_topic_cols(gobj, "users");

    /*-----------------------------------------------*
     *  Test 2: the projection alone rebuilds the
     *  schema. Close the treedb and remove its schema
     *  file, so that on re-open neither the file nor
     *  an input schema can supply it: what comes back
     *  must come from __system__.
     *
     *  HACK The tests run from a timer, so the yuno is playing and
     *  close-treedb refuses — it destroys services whose handles a normal
     *  owner has cached. This driver opened the treedb and holds nothing of
     *  it, which is what `force` is for. The refusal itself is checked below.
     *-----------------------------------------------*/
    json_t *jn_resp = gobj_command(
        priv->gobj_treedbs,
        "close-treedb",
        json_pack("{s:s}", "treedb_name", TREEDB_NAME),
        gobj
    );
    if(kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED) == 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a playing yuno closed its treedb",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(jn_resp)

    jn_resp = gobj_command(
        priv->gobj_treedbs,
        "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", TREEDB_NAME, "force", 1),
        gobj
    );
    int ret = (int)kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED);
    JSON_DECREF(jn_resp)
    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: close-treedb failed",
            NULL
        );
        JSON_DECREF(cols_before)
        JSON_DECREF(jn_schema)
        return -1;
    }

    char schema_dir[PATH_MAX];
    build_path(schema_dir, sizeof(schema_dir), priv->path_database, TREEDB_NAME, NULL);
    if(file_remove(schema_dir, TREEDB_NAME ".treedb_schema.json")<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: cannot remove the treedb schema file",
            "path",         "%s", schema_dir,
            NULL
        );
        JSON_DECREF(cols_before)
        JSON_DECREF(jn_schema)
        return -1;
    }

    if(open_test_treedb(gobj, json_incref(jn_schema)) < 0) {
        JSON_DECREF(cols_before)
        JSON_DECREF(jn_schema)
        return -1;  // Error already logged
    }

    json_t *cols_after = client_topic_cols(gobj, "users");

    result += check_schema_order(gobj, jn_schema);

    if(!cols_before || !cols_after || !json_equal(cols_before, cols_after)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: schema rebuilt from __system__ differs",
            "before",       "%j", cols_before,
            "after",        "%j", cols_after,
            NULL
        );
        result += -1;
    }

    JSON_DECREF(cols_before)
    JSON_DECREF(cols_after)
    JSON_DECREF(jn_schema)

    /*-----------------------------------------------*
     *  Test 3: a schema that moved forward updates
     *  the projection, and the columns keep their
     *  identity: an update appends a version of the
     *  same node, a re-create would be a second one
     *  under the same name.
     *-----------------------------------------------*/
    json_t *ids_before = system_topic_cols(gobj, "users");
    json_int_t users_v0 = system_topic_version(gobj, "users");
    json_int_t departments_v0 = system_topic_version(gobj, "departments");

    helper_quote2doublequote(schema_test2);
    json_t *jn_schema2 = legalstring2json(schema_test2, TRUE);
    if(!jn_schema2) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: cannot parse the second test schema",
            NULL
        );
        JSON_DECREF(ids_before)
        return -1;
    }

    jn_resp = gobj_command(
        priv->gobj_treedbs,
        "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", TREEDB_NAME, "force", 1),
        gobj
    );
    JSON_DECREF(jn_resp)

    if(open_test_treedb(gobj, json_incref(jn_schema2)) < 0) {
        JSON_DECREF(jn_schema2)
        JSON_DECREF(ids_before)
        return -1;  // Error already logged
    }

    /*
     *  `email` is declared in the MIDDLE of `users`: a column added to a
     *  schema takes the place the schema gives it, it is not appended.
     */
    result += check_schema_order(gobj, jn_schema2);
    JSON_DECREF(jn_schema2)

    /*
     *  The projection records WHICH literal it came from in
     *  `c_schema_version`, and publishes under the literal's own
     *  `schema_version`: the version belongs to whoever changed the
     *  schema, and the projector invents none.
     */
    json_int_t from_c = system_schema_version(gobj, "c_schema_version");
    json_int_t version = system_schema_version(gobj, "schema_version");
    if(from_c != 2 || version != 2) {
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_INTERNAL,
            "msg",                  "%s", "TEST FAIL: wrong versions after re-projection",
            "c_schema_version",     "%d", (int)from_c,
            "schema_version",       "%d", (int)version,
            NULL
        );
        result += -1;
    }

    /*
     *  Only the topic that moved is raised. `departments` is the same in
     *  both literals: raising it anyway rewrites its topic_cols.json in the
     *  client store for nothing, and that is what every release did.
     */
    json_int_t users_v1 = system_topic_version(gobj, "users");
    json_int_t departments_v1 = system_topic_version(gobj, "departments");
    if(users_v1 <= users_v0 || departments_v1 != departments_v0 || departments_v0 < 0) {
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_INTERNAL,
            "msg",                  "%s", "TEST FAIL: re-projection raised the wrong topics",
            "users_was",            "%d", (int)users_v0,
            "users",                "%d", (int)users_v1,
            "departments_was",      "%d", (int)departments_v0,
            "departments",          "%d", (int)departments_v1,
            NULL
        );
        result += -1;
    }

    json_t *ids_after = system_topic_cols(gobj, "users");

    /*
     *  The columns that were already there keep their id
     */
    const char *col_name; json_t *jn_id;
    json_object_foreach(ids_before, col_name, jn_id) {
        if(strstr(col_name, "__header")) {
            continue;   // content, checked below; only the id is identity
        }
        json_t *now = json_object_get(ids_after, col_name);
        if(!now || !json_equal(now, jn_id)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: column identity changed on update",
                "col",          "%s", col_name,
                "before",       "%j", jn_id,
                "after",        "%j", now,
                NULL
            );
            result += -1;
        }
    }

    /*
     *  The new column is there, the re-headered one carries its new header
     */
    if(!json_object_get(ids_after, "email")) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: new column not projected in __system__",
            "cols",         "%j", ids_after,
            NULL
        );
        result += -1;
    }
    const char *header = json_string_value(json_object_get(ids_after, "username__header"));
    if(!header || strcmp(header, "Login")!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: column change not projected in __system__",
            "header",       "%s", header?header:"",
            NULL
        );
        result += -1;
    }

    /*
     *  And the running treedb opened with the new column
     */
    json_t *client_cols = client_topic_cols(gobj, "users");
    if(!client_cols || !json_object_get(client_cols, "email")) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: new column did not reach the treedb",
            "cols",         "%j", client_cols,
            NULL
        );
        result += -1;
    }

    /*-----------------------------------------------*
     *  Test 4: an edit made here raises the published
     *  version, and from then on the schema is changed
     *  dynamically: a literal BEHIND it is not applied
     *  (and says so), one AHEAD of it is, under its own
     *  number. Nobody invents a version.
     *-----------------------------------------------*/
    hgobj gobj_node_system = gobj_find_service(SYSTEM_TREEDB, FALSE);
    json_t *edited = gobj_update_node(
        gobj_node_system,
        "treedbs",
        json_pack("{s:s, s:I}", "id", TREEDB_NAME, "schema_version", (json_int_t)10),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    JSON_DECREF(edited)

    static const json_int_t literal_versions[] = {3, 11};
    for(int i=0; i<2; i++) {
        json_int_t literal_version = literal_versions[i];
        BOOL must_land = literal_version > 10? TRUE: FALSE;

        /*
         *  jn_schema2 was consumed by the open above; parse the literal again
         */
        json_t *jn_schema3 = legalstring2json(schema_test2, TRUE);
        json_object_set_new(jn_schema3, "schema_version", json_integer(literal_version));
        json_t *jn_users = json_array_get(json_object_get(jn_schema3, "topics"), 0);
        json_object_set_new(jn_users, "topic_version", json_integer(3));
        json_object_set_new(
            json_object_get(json_object_get(jn_users, "cols"), "email"),
            "header",
            json_string("E-mail")
        );

        jn_resp = gobj_command(
            priv->gobj_treedbs,
            "close-treedb",
            json_pack("{s:s, s:b}", "treedb_name", TREEDB_NAME, "force", 1),
            gobj
        );
        JSON_DECREF(jn_resp)

        if(open_test_treedb(gobj, jn_schema3) < 0) {
            result += -1;
            continue;   // Error already logged
        }
        /*  jn_schema3 was consumed by open_test_treedb  */

        json_int_t from_c3 = system_schema_version(gobj, "c_schema_version");
        json_int_t version3 = system_schema_version(gobj, "schema_version");
        json_t *cols3 = system_topic_cols(gobj, "users");
        const char *email_header = json_string_value(
            json_object_get(cols3, "email__header")
        );
        BOOL landed = (email_header && strcmp(email_header, "E-mail")==0)? TRUE: FALSE;

        json_int_t expected_from_c = must_land? literal_version: 2;
        json_int_t expected_version = must_land? literal_version: 10;
        if(from_c3 != expected_from_c || version3 != expected_version || landed != must_land) {
            gobj_log_error(gobj, 0,
                "function",             "%s", __FUNCTION__,
                "msgset",               "%s", MSGSET_INTERNAL,
                "msg",                  "%s", must_land?
                    "TEST FAIL: a literal ahead of the schema in use did not land":
                    "TEST FAIL: a literal behind the schema in use was applied",
                "literal_version",      "%d", (int)literal_version,
                "c_schema_version",     "%d", (int)from_c3,
                "schema_version",       "%d", (int)version3,
                "email_header",         "%s", email_header?email_header:"",
                NULL
            );
            result += -1;
        }
        JSON_DECREF(cols3)
    }

    /*
     *  And a topic is published by ITS version: a literal ahead of the
     *  treedb, whose `users` changes a column without raising its
     *  topic_version, publishes the treedb and leaves `users` as it is.
     */
    json_t *jn_schema4 = legalstring2json(schema_test2, TRUE);
    json_object_set_new(jn_schema4, "schema_version", json_integer(12));
    json_t *jn_users4 = json_array_get(json_object_get(jn_schema4, "topics"), 0);
    json_object_set_new(jn_users4, "topic_version", json_integer(3));
    json_object_set_new(
        json_object_get(json_object_get(jn_users4, "cols"), "email"),
        "header",
        json_string("Mail")
    );

    jn_resp = gobj_command(
        priv->gobj_treedbs,
        "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", TREEDB_NAME, "force", 1),
        gobj
    );
    JSON_DECREF(jn_resp)

    if(open_test_treedb(gobj, jn_schema4) == 0) {
        json_int_t version4 = system_schema_version(gobj, "schema_version");
        json_int_t users_v4 = system_topic_version(gobj, "users");
        json_t *cols4 = system_topic_cols(gobj, "users");
        const char *email_header = json_string_value(
            json_object_get(cols4, "email__header")
        );
        if(version4 != 12 || users_v4 != 3 ||
            !email_header || strcmp(email_header, "E-mail")!=0
        ) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_INTERNAL,
                "msg",              "%s", "TEST FAIL: a topic whose version did not move was applied",
                "schema_version",   "%d", (int)version4,
                "users_version",    "%d", (int)users_v4,
                "email_header",     "%s", email_header?email_header:"",
                NULL
            );
            result += -1;
        }
        JSON_DECREF(cols4)
    } else {
        result += -1;   // Error already logged
    }
    /*  jn_schema4 was consumed by open_test_treedb  */

    /*-----------------------------------------------*
     *  Test 5: writes that would define a broken
     *  schema are refused where they are written,
     *  not at the next open
     *-----------------------------------------------*/
    result += check_refused_writes(gobj, ids_after);

    /*-----------------------------------------------*
     *  Test 6: a change to a schema publishes itself
     *-----------------------------------------------*/
    result += check_autopublished_versions(gobj, ids_after);
    result += check_create_and_delete_publish(gobj);

    /*-----------------------------------------------*
     *  Test 7: and `diff-schema` says WHAT it changed
     *-----------------------------------------------*/
    result += check_schema_diff(gobj);

    /*-----------------------------------------------*
     *  Test 8: a projection keyed by rowid moves to
     *  qualified ids on the next re-projection
     *-----------------------------------------------*/
    result += check_legacy_ids_migrated(gobj);

    /*-----------------------------------------------*
     *  Test 9: impose_c_schema reverts what was
     *  changed outside the code: the treedb opens
     *  with the literal, over a newer schema on
     *  disk, and __system__ keeps the changes.
     *-----------------------------------------------*/
    result += check_impose_c_schema(gobj);

    /*-----------------------------------------------*
     *  Test 10: the yuno's code imposes the schema
     *  from C over the attribute: a persistent value
     *  set by command cannot undo what the binary
     *  decides.
     *-----------------------------------------------*/
    result += check_impose_forced_by_code(gobj);

    /*-----------------------------------------------*
     *  Test 11: gobj_treedbs() answers the LIST its
     *  contract promises, and a refusal is NULL.
     *  C_TREEDB's mt_treedbs used to answer a
     *  msg_iev_build_response envelope when the user
     *  had no `read`: a dict, which a caller reads as
     *  a treedb named "result".
     *-----------------------------------------------*/
    {
        json_t *treedbs = gobj_treedbs(priv->gobj_treedbs, json_object(), gobj);
        if(!json_is_array(treedbs)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: gobj_treedbs() did not answer a list",
                "answer",       "%j", treedbs,
                NULL
            );
            result += -1;
        }
        JSON_DECREF(treedbs)

        json_t *refused = gobj_treedbs(
            priv->gobj_treedbs,
            json_pack("{s:s}", "__username__", "denied@test"),
            gobj
        );
        if(refused) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: a refused gobj_treedbs() answered something",
                "answer",       "%j", refused,
                NULL
            );
            result += -1;
        }
        JSON_DECREF(refused)
    }

    /*-----------------------------------------------*
     *  Test 12: `delete-treedb` takes the whole
     *  projection with it -- the treedbs node, its
     *  topics and their columns. It used to leave
     *  every one of them behind.
     *-----------------------------------------------*/
    result += check_delete_treedb(gobj);

    /*-----------------------------------------------*
     *  Test 13: imposing does not READ __system__,
     *  and it does WRITE it: seeded when nothing of
     *  the treedb is there, re-made when it is
     *  behind, untouched when it is ahead.
     *-----------------------------------------------*/
    result += check_impose_projects_into_system(gobj);

    /*-----------------------------------------------*
     *  Test 14: only the MASTER writes __system__.
     *  A replica reads the treedb from disk as it is
     *  at that moment and reconciles nothing. LAST:
     *  it takes the master's C_TREEDB down to open
     *  the same store as a replica.
     *-----------------------------------------------*/
    result += check_replica_writes_nothing(gobj);

    JSON_DECREF(client_cols)
    JSON_DECREF(ids_before)
    JSON_DECREF(ids_after)

    if(result == 0) {
        gobj_log_info(gobj, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "All treedb system schema tests PASSED",
            NULL
        );
    }

    return result;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  EV_TIMEOUT — runs the test logic inside the event loop, then exits
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    run_tests(gobj);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "Exit to die",
        NULL
    );
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}




                    /***************************
                     *      Commands
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw)
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_SYSTEM_SCHEMA);

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
            "msgset",       "%s", MSGSET_INTERNAL,
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
        {EV_TIMEOUT,                ac_timeout,         0},
        {0,0,0}
    };
    states_t states[] = {
        {ST_IDLE,       st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,        0},
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
        0,  //lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        command_table,
        s_user_trace_level,
        0  // gclass_flag
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_test_system_schema(void)
{
    return create_gclass(C_TEST_SYSTEM_SCHEMA);
}
