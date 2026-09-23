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
#include <sys/stat.h>

#include "c_test_system_schema.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TREEDB_NAME     "treedb_test1"
#define DELETE_TREEDB_NAME "treedb_to_delete"
#define B_TREEDB_NAME   "treedb_test_b"     /*  the second treedb of an apply-schema of all  */
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
        const char *names[] = {"bad_file", "bad_hook_fkey", "bad_hook_type", "bad_hook_string", NULL};
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
            /*  A string hook was blessed here and refused by every link
             *  into it ("wrong parent hook type"), after the unlink of a
             *  replace: the M1 shape again (a low of the 2026-09-22 review).  */
            json_pack("{s:s, s:s, s:s, s:[s], s:s}",
                "value", "bad_hook_string", "header", "Bad", "type", "string",
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
 *  An edit of a schema is a DRAFT: it moves no version.
 *
 *  It used to publish itself (M8 of the 2026-09-21 review, then the
 *  owner's decision on M36): every write raised `topic_version` and
 *  `schema_version`, so an edit half made was already the schema the next
 *  start would take. `save-schema` publishes now, once, for the topics
 *  that changed -- see check_save_and_apply().
 ***************************************************************************/
PRIVATE int check_edits_are_drafts(hgobj gobj, json_t *col_ids)
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

    if(topic_v1 != topic_v0 || schema_v1 != schema_v0) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "TEST FAIL: a column edit published itself",
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
 *  A column CREATED with its link, and a column DELETED, are drafts too:
 *  no version moves until save-schema.
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

PRIVATE int check_create_and_delete_are_drafts(hgobj gobj)
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
    if(t1 != t0 || s1 != s0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a column created with its link published itself",
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
    if(t2 != t1 || s2 != s1) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a column delete published itself",
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
 *      - the column edit of check_edits_are_drafts,
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
 *  Configure C_TREEDB the way a yuno does. Neither attribute is persistent
 *  and no command changes them: they are configuration (the yuno's main.c
 *  or its config file), read when a treedb opens. The test is that
 *  configuration, between two opens.
 ***************************************************************************/
PRIVATE void set_impose_c_schema(hgobj gobj, BOOL impose)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_bool_attr(priv->gobj_treedbs, "impose_c_schema", impose);
}

PRIVATE void set_dynamic_schema_treedbs(hgobj gobj, json_t *treedbs) // owned
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_new_json_attr(priv->gobj_treedbs, "dynamic_schema_treedbs", treedbs);
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

    set_impose_c_schema(gobj, TRUE);

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
        set_impose_c_schema(gobj, FALSE);
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
     *  Leave it off, as the tests after this one expect
     */
    set_impose_c_schema(gobj, FALSE);

    return result;
}

/***************************************************************************
 *  The row `treedbs` answers for one treedb: whether it imposes, and who
 *  decided it.
 ***************************************************************************/
PRIVATE BOOL treedbs_row(hgobj gobj, const char *treedb_name, BOOL *impose, const char **decided_by)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL found = FALSE;
    json_t *jn_resp = gobj_command(priv->gobj_treedbs, "treedbs", json_object(), gobj);
    int idx; json_t *row;
    json_array_foreach(kw_get_list(gobj, jn_resp, "data", 0, 0), idx, row) {
        if(strcmp(kw_get_str(gobj, row, "treedb_name", "", 0), treedb_name)==0) {
            *impose = kw_get_bool(gobj, row, "impose_c_schema", 0, 0);
            static char decided[NAME_MAX];
            snprintf(decided, sizeof(decided), "%s", kw_get_str(gobj, row, "decided_by", "", 0));
            *decided_by = decided;
            found = TRUE;
            break;
        }
    }
    JSON_DECREF(jn_resp)
    return found;
}

/***************************************************************************
 *  saved-schema and `treedbs` on a treedb with NO saved schema -- the normal
 *  state of every treedb nobody has edited -- answer version 0 and log
 *  nothing: the console asks it of every treedb it shows, and a missing file
 *  used to reach the kw_* readers as NULL and log "kw must be list or dict"
 *  on each call. The strict list of expected logs is the check.
 ***************************************************************************/
PRIVATE int check_no_saved_schema(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    json_t *jn_resp = gobj_command(priv->gobj_treedbs, "saved-schema",
        json_pack("{s:s}", "treedb_name", TREEDB_NAME), gobj);
    int ret = (int)kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED);
    BOOL saved = kw_get_bool(gobj, jn_resp, "data`saved", 1, 0);
    json_int_t version = kw_get_int(gobj, jn_resp, "data`saved_schema_version", -1, 0);
    if(ret < 0 || saved || version != 0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: saved-schema with nothing saved",
            "response",     "%j", jn_resp,
            NULL
        );
        result += -1;
    }
    JSON_DECREF(jn_resp)

    BOOL impose = FALSE;
    const char *decided_by = "";
    if(!treedbs_row(gobj, TREEDB_NAME, &impose, &decided_by)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: treedbs does not list the open treedb",
            NULL
        );
        result += -1;
    }
    return result;
}

/***************************************************************************
 *  dynamic_schema_treedbs: with impose_c_schema ON, the treedb it names
 *  opens from its schema FILE, and `treedbs` says so and says who decided.
 *  Removing it from the list puts the default back.
 ***************************************************************************/
PRIVATE int check_dynamic_schema_treedbs(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    set_impose_c_schema(gobj, TRUE);
    set_dynamic_schema_treedbs(gobj, json_pack("[s]", TREEDB_NAME));

    json_t *jn_resp = gobj_command(priv->gobj_treedbs, "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", TREEDB_NAME, "force", 1), gobj);
    JSON_DECREF(jn_resp)
    if(open_test_treedb(gobj, legalstring2json(schema_test2, TRUE)) < 0) {
        result += -1;  // Error already logged
    }

    BOOL impose = TRUE;
    const char *decided_by = "";
    BOOL found = treedbs_row(gobj, TREEDB_NAME, &impose, &decided_by);
    if(!found || impose || strcmp(decided_by, "dynamic_schema_treedbs")!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a treedb in dynamic_schema_treedbs is imposed",
            "found",        "%d", (int)found,
            "impose",       "%d", (int)impose,
            "decided_by",   "%s", decided_by,
            NULL
        );
        result += -1;
    }

    set_dynamic_schema_treedbs(gobj, json_array());
    found = treedbs_row(gobj, TREEDB_NAME, &impose, &decided_by);
    if(!found || !impose || strcmp(decided_by, "impose_c_schema")!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: out of dynamic_schema_treedbs, the default does not decide",
            "found",        "%d", (int)found,
            "impose",       "%d", (int)impose,
            "decided_by",   "%s", decided_by,
            NULL
        );
        result += -1;
    }

    set_impose_c_schema(gobj, FALSE);

    return result;
}

/***************************************************************************
 *  The yuno's code imposes: open-treedb impose_c_schema=1 wins over the
 *  attribute, which Test 9 left off.
 *
 *  First the disk is taken ahead of the literal the legitimate way --
 *  save-schema + apply-schema of what __system__ holds, and an ordinary
 *  open; then the forced one brings it back, and `treedbs` says the code
 *  decided.
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
    const char *steps[] = {"save-schema", "apply-schema", NULL};
    for(int i = 0; steps[i]; i++) {
        jn_resp = gobj_command(priv->gobj_treedbs, steps[i],
            json_pack("{s:s}", "treedb_name", TREEDB_NAME), gobj);
        JSON_DECREF(jn_resp)
    }
    jn_resp = gobj_command(priv->gobj_treedbs, "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", TREEDB_NAME, "force", 1), gobj);
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
     *  The attribute is still off, and `treedbs` says the code decided
     */
    BOOL impose = FALSE;
    const char *decided_by = "";
    BOOL found = treedbs_row(gobj, TREEDB_NAME, &impose, &decided_by);
    if(gobj_read_bool_attr(priv->gobj_treedbs, "impose_c_schema") ||
        !found || !impose || strcmp(decided_by, "code")!=0
    ) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "TEST FAIL: treedbs does not say the code imposes",
            "found",            "%d", (int)found,
            "impose",           "%d", (int)impose,
            "decided_by",       "%s", decided_by,
            NULL
        );
        result += -1;
    }

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

        /*
         *  A saved schema of the treedb goes with its schema: left in
         *  saved_schemas/, a treedb created again under the name found a
         *  save of the one deleted (review of the second fix round,
         *  2026-09-23)
         */
        char saved_dir[PATH_MAX];
        build_path(saved_dir, sizeof(saved_dir), priv->path_database, "__system__", "saved_schemas", NULL);
        if(close_first) {
            mkrdir(saved_dir, 02770);
            save_json_to_file(gobj, saved_dir, DELETE_TREEDB_NAME ".treedb_schema.json",
                02770, 0660, 0, TRUE, FALSE,
                json_pack("{s:s, s:i, s:[]}", "id", DELETE_TREEDB_NAME, "schema_version", 50, "topics")
            );
        }

        jn_resp = gobj_command(
            priv->gobj_treedbs,
            "delete-treedb",
            json_pack("{s:s, s:b}", "treedb_name", DELETE_TREEDB_NAME, "force", 1),
            gobj
        );
        int deleted = (int)kw_get_int(gobj, jn_resp, "result", -1, 0);
        if(close_first && file_exists(saved_dir, DELETE_TREEDB_NAME ".treedb_schema.json")) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: delete-treedb left the saved schema of the treedb",
                NULL
            );
            result += -1;
        }
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

    /*
     *  Every write of C_TREEDB answers READ-ONLY on a replica, and the
     *  unnamed apply has nothing it could apply: 0 and no row
     */
    struct {
        const char *command;
        json_t *kw;
    } writes[] = {
        {"save-schema",   json_pack("{s:s}", "treedb_name", DELETE_TREEDB_NAME)},
        {"apply-schema",  json_pack("{s:s}", "treedb_name", DELETE_TREEDB_NAME)},
        {"create-topic",  json_pack("{s:s, s:s, s:{s:{s:s, s:s, s:[s]}}}",
            "treedb_name", DELETE_TREEDB_NAME, "topic_name", "replica_topic",
            "cols", "id", "header", "Id", "type", "string", "flag", "persistent")},
        {"delete-topic",  json_pack("{s:s, s:s}", "treedb_name", DELETE_TREEDB_NAME, "topic_name", "alfa")},
        {"delete-treedb", json_pack("{s:s, s:b}", "treedb_name", TREEDB_NAME, "force", 1)},
    };
    for(size_t i = 0; i < sizeof(writes)/sizeof(writes[0]); i++) {
        json_t *jn_resp = gobj_command(priv->gobj_treedbs, writes[i].command, writes[i].kw, gobj);
        const char *comment = kw_get_str(gobj, jn_resp, "comment", "", 0);
        if(kw_get_int(gobj, jn_resp, "result", 0, 0) != -1 || !strstr(comment, "READ-ONLY")) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: a C_TREEDB write on a replica was not answered READ-ONLY",
                "command",      "%s", writes[i].command,
                "comment",      "%s", comment,
                NULL
            );
            result += -1;
        }
        JSON_DECREF(jn_resp)
    }
    {
        json_t *jn_resp = gobj_command(priv->gobj_treedbs, "apply-schema", json_object(), gobj);
        if(kw_get_int(gobj, jn_resp, "result", -1, 0) != 0 ||
                json_array_size(kw_get_list(gobj, jn_resp, "data", 0, 0)) != 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: an apply of all on a replica answered a row",
                "answer",       "%j", jn_resp,
                NULL
            );
            result += -1;
        }
        JSON_DECREF(jn_resp)
    }

    close_treedb_to_delete(gobj);

    return result;
}

#define LEGACY_TREEDB   "treedb_legacy"

/***************************************************************************
 *  Does the OPEN legacy treedb have that column?
 ***************************************************************************/
PRIVATE BOOL legacy_treedb_has_col(hgobj gobj, const char *col_name)
{
    hgobj gobj_legacy_node = gobj_find_service(LEGACY_TREEDB, FALSE);
    json_t *desc = gobj_legacy_node? gobj_topic_desc(gobj_legacy_node, "users"): NULL;
    BOOL found = FALSE;
    int idx; json_t *col;
    json_array_foreach(json_object_get(desc, "cols"), idx, col) {
        if(strcmp(kw_get_str(gobj, col, "id", "", 0), col_name)==0) {
            found = TRUE;
        }
    }
    JSON_DECREF(desc)
    return found;
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
    json_t *legacy_open_kw = json_pack("{s:s, s:i, s:s, s:o}",
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
        );
    json_t *jn_resp = gobj_command(
        priv->gobj_treedbs,
        "open-treedb",
        json_deep_copy(legacy_open_kw),
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
     *  In __system__ it is a DRAFT: the treedb opens from its schema file,
     *  and the column reaches it through save-schema + apply-schema and the
     *  next open -- which is the point of keeping it.
     */
    if(legacy_treedb_has_col(gobj, "operator_col")) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a draft column reached the open treedb",
            NULL
        );
        result += -1;
    }
    const char *steps[] = {"save-schema", "apply-schema", NULL};
    for(int i = 0; steps[i]; i++) {
        jn_resp = gobj_command(priv->gobj_treedbs, steps[i],
            json_pack("{s:s}", "treedb_name", LEGACY_TREEDB), gobj);
        if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: the legacy draft could not be saved and applied",
                "command",      "%s", steps[i],
                "answer",       "%j", jn_resp,
                NULL
            );
            result += -1;
        }
        JSON_DECREF(jn_resp)
    }
    jn_resp = gobj_command(priv->gobj_treedbs, "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", LEGACY_TREEDB, "force", 1), gobj);
    JSON_DECREF(jn_resp)
    jn_resp = gobj_command(priv->gobj_treedbs, "open-treedb",
        json_deep_copy(legacy_open_kw), gobj);
    JSON_DECREF(jn_resp)
    if(!legacy_treedb_has_col(gobj, "operator_col")) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: the moved column did not reach the open treedb",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(legacy_open_kw)

    return result;
}

/***************************************************************************
 *  A command of C_TREEDB for the test treedb; the answer is YOURS.
 ***************************************************************************/
PRIVATE json_t *treedbs_command(hgobj gobj, const char *command, json_t *kw) // kw owned
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_object_set_new(kw, "treedb_name", json_string(TREEDB_NAME));
    return gobj_command(priv->gobj_treedbs, command, kw, gobj);
}

/***************************************************************************
 *  The header the OPEN client treedb gives a column, or "".
 ***************************************************************************/
PRIVATE void client_col_header(hgobj gobj, const char *topic_name, const char *col_name,
    char *bf, size_t bfsize)
{
    bf[0] = 0;
    hgobj gobj_client_node = gobj_find_service(TREEDB_NAME, FALSE);
    json_t *desc = gobj_client_node? gobj_topic_desc(gobj_client_node, topic_name): NULL;
    int idx; json_t *col;
    json_array_foreach(json_object_get(desc, "cols"), idx, col) {
        if(strcmp(kw_get_str(gobj, col, "id", "", 0), col_name)==0) {
            snprintf(bf, bfsize, "%s", kw_get_str(gobj, col, "header", "", 0));
        }
    }
    JSON_DECREF(desc)
}

PRIVATE int reopen_test_treedb(hgobj gobj, BOOL forced_by_code)
{
    json_t *jn_resp = treedbs_command(gobj, "close-treedb", json_pack("{s:b}", "force", 1));
    JSON_DECREF(jn_resp)
    return open_test_treedb_with(gobj, legalstring2json(schema_test2, TRUE), forced_by_code);
}

PRIVATE int save_fail(hgobj gobj, const char *what, json_t *jn_resp)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", "check_save_and_apply",
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", what,
        "answer",       "%j", jn_resp?jn_resp:json_null(),
        NULL
    );
    return -1;
}

/***************************************************************************
 *  Save and apply: the owner's design of M36 (2026-09-21 review).
 *
 *  An edit of __system__ is a draft. `save-schema` publishes it ONCE: the
 *  topics that differ from the schema file IN USE get its topic_version
 *  + 1, the treedb its schema_version + 1, and the result is written to
 *  `saved_schemas/` under the __system__ tranger -- never over the file in
 *  use. `saved-schema` says what that would change. `apply-schema` puts it
 *  in place, and only for a treedb whose schema C does not impose. With
 *  impose_c_schema off the treedb opens from the FILE, so the next open
 *  runs the saved schema although its literal is older.
 ***************************************************************************/
PRIVATE int check_save_and_apply(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    json_t *jn_resp;

    if(reopen_test_treedb(gobj, FALSE) < 0) {
        return -1;  // Error already logged
    }
    json_int_t in_use_v = disk_schema_version(gobj);
    json_int_t users_in_use_v = disk_topic_version(gobj, "users");
    json_int_t departments_v = system_topic_version(gobj, "departments");
    json_int_t users_v0, schema_v0;
    versions_of_users(gobj, &users_v0, &schema_v0);

    /*
     *  A save publishes the version in use + 1, or the draft's own when
     *  it is already ahead: a number of __system__ never goes down.
     */
    json_int_t expected_v = (schema_v0 > in_use_v + 1)? schema_v0: in_use_v + 1;
    json_int_t expected_users_v = (users_v0 > users_in_use_v + 1)? users_v0: users_in_use_v + 1;

    /*
     *  The draft
     */
    json_t *ids = system_topic_cols(gobj, "users");
    const char *username_id = json_string_value(json_object_get(ids, "username"));
    json_t *edited = gobj_update_node(
        gobj_find_service(SYSTEM_TREEDB, FALSE),
        "cols",
        json_pack("{s:s, s:s}", "id", username_id?username_id:"", "header", "Saved header"),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    JSON_DECREF(edited)
    JSON_DECREF(ids)

    /*
     *  dry_run answers the schema a save would write, and writes nothing
     */
    char saved_dir[PATH_MAX];
    build_path(saved_dir, sizeof(saved_dir), priv->path_database, "__system__", "saved_schemas", NULL);
    char saved_file[PATH_MAX];
    build_path(saved_file, sizeof(saved_file), saved_dir, TREEDB_NAME ".treedb_schema.json", NULL);

    file_remove(saved_dir, TREEDB_NAME ".treedb_schema.json");   /*  a save of an earlier check  */
    jn_resp = treedbs_command(gobj, "save-schema", json_pack("{s:b}", "dry_run", 1));
    json_t *dry_schema = kw_get_dict(gobj, jn_resp, "data`schema", 0, 0);
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 || !dry_schema ||
            kw_get_int(gobj, dry_schema, "schema_version", 0, KW_WILD_NUMBER) != expected_v) {
        result += save_fail(gobj, "TEST FAIL: save-schema dry_run did not answer the schema to save", jn_resp);
    }
    JSON_DECREF(jn_resp)
    json_int_t users_v1, schema_v1;
    versions_of_users(gobj, &users_v1, &schema_v1);
    if(file_exists(saved_file, 0) || users_v1 != users_v0 || schema_v1 != schema_v0) {
        result += save_fail(gobj, "TEST FAIL: save-schema dry_run wrote something", NULL);
    }

    /*
     *  Before the save, saved-schema names the topic the DRAFT changes (the
     *  mark the schema editor rebuilds after a reload, N13 of the 2026-09-22
     *  review): `users` was edited above, `departments` not
     */
    jn_resp = treedbs_command(gobj, "saved-schema", json_object());
    {
        json_t *draft_changed = kw_get_dict(gobj, jn_resp, "data`draft_changed", 0, 0);
        if(!draft_changed || !json_is_true(json_object_get(draft_changed, "users")) ||
                json_object_get(draft_changed, "departments")) {
            result += save_fail(gobj, "TEST FAIL: saved-schema did not say what the draft changes", jn_resp);
        }
    }
    JSON_DECREF(jn_resp)

    /*
     *  Save: twice, the second changes nothing
     */
    for(int i = 0; i < 2; i++) {
        jn_resp = treedbs_command(gobj, "save-schema", json_object());
        if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
            result += save_fail(gobj, "TEST FAIL: save-schema failed", jn_resp);
        }
        JSON_DECREF(jn_resp)
        json_int_t users_v2, schema_v2;
        versions_of_users(gobj, &users_v2, &schema_v2);
        if(schema_v2 != expected_v || users_v2 != expected_users_v ||
                system_topic_version(gobj, "departments") != departments_v) {
            gobj_log_error(gobj, 0,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_INTERNAL,
                "msg",              "%s", "TEST FAIL: save-schema published the wrong versions",
                "round",            "%d", i,
                "schema_version",   "%d", (int)schema_v2,
                "users",            "%d", (int)users_v2,
                "in_use",           "%d", (int)in_use_v,
                "users_in_use",     "%d", (int)users_in_use_v,
                NULL
            );
            result += -1;
        }
    }
    if(!file_exists(saved_file, 0) || disk_schema_version(gobj) != in_use_v) {
        result += save_fail(gobj, "TEST FAIL: save-schema did not write beside the file in use", NULL);
    }

    /*
     *  The file in use of a node opened with impose off before the draft
     *  model held its `topics` as a DICT keyed by name (what
     *  get_treedb_schema() answers, written as it was). It is the same
     *  schema, and the diff of a save must read it as such (N11 of the
     *  2026-09-22 review): read as no topic at all, every topic was
     *  "changed" and every version was bumped and written again.
     */
    {
        char in_use_dir[PATH_MAX];
        build_path(in_use_dir, sizeof(in_use_dir), priv->path_database, TREEDB_NAME, NULL);

        jn_resp = treedbs_command(gobj, "save-schema", json_pack("{s:b}", "dry_run", 1));
        json_t *dry_data = kw_get_dict(gobj, jn_resp, "data", 0, 0);
        json_t *changes_list = dry_data? kw_get_list(gobj, dry_data, "changes", 0, 0) : NULL;
        int list_changes = 0;
        int idx_c; json_t *row_c;
        json_array_foreach(changes_list, idx_c, row_c) {
            if(strcmp(kw_get_str(gobj, row_c, "kind", "", 0), "version")!=0) {
                list_changes++;
            }
        }
        JSON_DECREF(jn_resp)

        json_t *in_use = load_json_from_file(gobj, in_use_dir, TREEDB_NAME ".treedb_schema.json", 0);
        json_t *as_list = json_deep_copy(in_use);
        json_t *as_dict = json_object();
        int idx_t; json_t *jn_topic;
        json_array_foreach(json_object_get(in_use, "topics"), idx_t, jn_topic) {
            json_object_set(as_dict, kw_get_str(gobj, jn_topic, "id", "", 0), jn_topic);
        }
        json_object_set_new(in_use, "topics", as_dict);
        save_json_to_file(gobj, in_use_dir, TREEDB_NAME ".treedb_schema.json",
            02770, 0660, 0, TRUE, FALSE, in_use  // owned
        );

        jn_resp = treedbs_command(gobj, "save-schema", json_pack("{s:b}", "dry_run", 1));
        dry_data = kw_get_dict(gobj, jn_resp, "data", 0, 0);
        changes_list = dry_data? kw_get_list(gobj, dry_data, "changes", 0, 0) : NULL;
        int dict_changes = 0;
        BOOL departments_changed = FALSE;
        json_array_foreach(changes_list, idx_c, row_c) {
            if(strcmp(kw_get_str(gobj, row_c, "kind", "", 0), "version")!=0) {
                dict_changes++;
            }
            if(strcmp(kw_get_str(gobj, row_c, "topic", "", 0), "departments")==0) {
                departments_changed = TRUE;
            }
        }
        if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 || dict_changes != list_changes ||
                departments_changed) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: a file in use with dict topics reads as another schema",
                "list_changes", "%d", list_changes,
                "dict_changes", "%d", dict_changes,
                NULL
            );
            result += -1;
        }
        JSON_DECREF(jn_resp)

        /*  ...and the VERSIONS read from it are the same: a real save over
         *  the dict file publishes what it published over the list one --
         *  `users` only, at the same numbers (counting rows alone would not
         *  see a topic_version read as 0 from a dict).  */
        jn_resp = treedbs_command(gobj, "save-schema", json_object());
        json_t *topic_versions = kw_get_dict(gobj, jn_resp, "data`topic_versions", 0, 0);
        if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 ||
                kw_get_int(gobj, jn_resp, "data`schema_version", 0, KW_WILD_NUMBER) != expected_v ||
                json_integer_value(json_object_get(topic_versions, "users")) != expected_users_v ||
                json_object_get(topic_versions, "departments") ||
                system_topic_version(gobj, "departments") != departments_v) {
            result += save_fail(gobj, "TEST FAIL: a save over a dict file in use published other versions", jn_resp);
        }
        JSON_DECREF(jn_resp)

        save_json_to_file(gobj, in_use_dir, TREEDB_NAME ".treedb_schema.json",
            02770, 0660, 0, TRUE, FALSE, as_list  // owned: the file as it was
        );
    }

    /*
     *  saved-schema says what it would change
     */
    jn_resp = treedbs_command(gobj, "saved-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 ||
            kw_get_int(gobj, jn_resp, "data`saved_schema_version", 0, KW_WILD_NUMBER) != expected_v ||
            kw_get_bool(gobj, jn_resp, "data`impose_c_schema", 1, 0) ||
            json_object_size(kw_get_dict(gobj, jn_resp, "data`diff`changed", 0, 0)) == 0) {
        result += save_fail(gobj, "TEST FAIL: saved-schema did not say what the save changes", jn_resp);
    }
    /*  ...and, once saved, the draft is no longer "unsaved": it is diffed
     *  against the SAVED schema, not the file in use. Diffed against the
     *  file in use, `users` stayed "changed" after a successful save until
     *  an Apply -- for ever on an imposed treedb (M1 of the 2026-09-23
     *  review).  */
    {
        json_t *draft_changed = kw_get_dict(gobj, jn_resp, "data`draft_changed", 0, 0);
        if(!draft_changed || json_object_size(draft_changed) != 0) {
            result += save_fail(gobj, "TEST FAIL: a saved draft still reads as unsaved", jn_resp);
        }
    }
    /*  `default: {}` is the meta-schema's placeholder for "no default", not
     *  something the literal says: it must not reach the saved file nor
     *  read as a difference of the array columns (`departments`, `users`)  */
    {
        int idx_k;
        const char *parts[] = {"added", "removed", "changed", NULL};
        for(idx_k = 0; parts[idx_k]; idx_k++) {
            json_t *part = kw_get_dict(gobj, jn_resp, "data`diff", 0, 0);
            part = part? json_object_get(part, parts[idx_k]) : NULL;
            const char *id_k; json_t *v_k;
            json_object_foreach(part, id_k, v_k) {
                size_t len = strlen(id_k);
                json_t *v_def = json_object_get(v_k, "to")? json_object_get(v_k, "to") : v_k;
                if(len >= 8 && strcmp(id_k + len - 8, "`default")==0 &&
                        json_is_object(v_def) && json_object_size(v_def)==0) {
                    result += save_fail(gobj, "TEST FAIL: saved-schema reads a placeholder default as a change", jn_resp);
                }
            }
        }
        json_t *saved_now = load_json_from_file(gobj, saved_dir, TREEDB_NAME ".treedb_schema.json", 0);
        int idx_t; json_t *jn_topic;
        json_array_foreach(json_object_get(saved_now, "topics"), idx_t, jn_topic) {
            const char *col_name; json_t *col;
            json_object_foreach(json_object_get(jn_topic, "cols"), col_name, col) {
                json_t *def = json_object_get(col, "default");
                if(json_is_object(def) && json_object_size(def)==0) {
                    result += save_fail(gobj, "TEST FAIL: the saved schema carries a placeholder default", col);
                }
            }
        }
        JSON_DECREF(saved_now)
    }
    JSON_DECREF(jn_resp)

    /*
     *  Unnamed, the command answers for every treedb opened here: what a
     *  console holding a whole yuno asks
     */
    jn_resp = gobj_command(priv->gobj_treedbs, "saved-schema", json_object(), gobj);
    BOOL listed = FALSE;
    int idx; json_t *jn_one;
    json_array_foreach(kw_get_list(gobj, jn_resp, "data", 0, 0), idx, jn_one) {
        if(strcmp(kw_get_str(gobj, jn_one, "treedb_name", "", 0), TREEDB_NAME)==0 &&
                kw_get_bool(gobj, jn_one, "data`can_apply", 0, 0)) {
            listed = TRUE;
        }
    }
    if(!listed) {
        result += save_fail(gobj, "TEST FAIL: an unnamed saved-schema did not list the treedb", jn_resp);
    }
    JSON_DECREF(jn_resp)

    /*
     *  Refused while C imposes the schema; applied when it does not
     */
    if(reopen_test_treedb(gobj, TRUE) < 0) {
        return result - 1;
    }
    jn_resp = treedbs_command(gobj, "apply-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0 ||
            kw_get_bool(gobj, jn_resp, "data`applied", 1, 0)) {
        result += save_fail(gobj, "TEST FAIL: apply-schema applied over an imposed schema", jn_resp);
    }
    JSON_DECREF(jn_resp)

    if(reopen_test_treedb(gobj, FALSE) < 0) {
        return result - 1;
    }

    /*
     *  Saving asks `write`; applying replaces the schema in use whole and
     *  asks `create-delete` (7.25.3 had the two the other way round)
     */
    jn_resp = treedbs_command(gobj, "save-schema",
        json_pack("{s:b, s:s}", "dry_run", 1, "__username__", "writer@test"));
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) == -403) {
        result += save_fail(gobj, "TEST FAIL: save-schema refused a user with `write`", jn_resp);
    }
    JSON_DECREF(jn_resp)
    jn_resp = treedbs_command(gobj, "apply-schema",
        json_pack("{s:s}", "__username__", "writer@test"));
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) != -403 || disk_schema_version(gobj) == expected_v) {
        result += save_fail(gobj, "TEST FAIL: apply-schema served a user without `create-delete`", jn_resp);
    }
    JSON_DECREF(jn_resp)

    jn_resp = treedbs_command(gobj, "apply-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 || disk_schema_version(gobj) != expected_v ||
            !kw_get_bool(gobj, jn_resp, "data`applied", 0, 0)) {
        result += save_fail(gobj, "TEST FAIL: apply-schema did not put the saved schema in place", jn_resp);
    }
    JSON_DECREF(jn_resp)

    /*
     *  Applied, the saved schema IS the file in use: it goes from
     *  saved_schemas/. Kept, saved-schema went on answering `saved: true`
     *  with a diff for a schema that was already in use (review of the
     *  second fix round, 2026-09-23).
     */
    jn_resp = treedbs_command(gobj, "saved-schema", json_object());
    if(file_exists(saved_file, 0) ||
            kw_get_bool(gobj, jn_resp, "data`saved", 1, 0) ||
            kw_get_bool(gobj, jn_resp, "data`stale", 1, 0) ||
            json_object_size(kw_get_dict(gobj, jn_resp, "data`diff`changed", 0, 0)) != 0) {
        result += save_fail(gobj, "TEST FAIL: an applied saved schema is still there, or still reads as saved", jn_resp);
    }
    JSON_DECREF(jn_resp)

    /*
     *  A saved schema that is not newer than the file in use (one left by
     *  an older release, or a remove that failed) is STALE: saved-schema
     *  says so, and does not diff it as a pending save
     */
    {
        char in_use_dir[PATH_MAX];
        build_path(in_use_dir, sizeof(in_use_dir), priv->path_database, TREEDB_NAME, NULL);
        json_t *stale = load_json_from_file(gobj, in_use_dir, TREEDB_NAME ".treedb_schema.json", 0);
        json_object_set_new(stale, "schema_version", json_integer(expected_v - 1));
        save_json_to_file(gobj, saved_dir, TREEDB_NAME ".treedb_schema.json",
            02770, 0660, 0, TRUE, FALSE, stale  // owned
        );
        jn_resp = treedbs_command(gobj, "saved-schema", json_object());
        if(kw_get_bool(gobj, jn_resp, "data`saved", 1, 0) ||
                !kw_get_bool(gobj, jn_resp, "data`stale", 0, 0) ||
                kw_get_bool(gobj, jn_resp, "data`can_apply", 1, 0) ||
                json_object_size(kw_get_dict(gobj, jn_resp, "data`diff`changed", 0, 0)) != 0) {
            result += save_fail(gobj, "TEST FAIL: saved-schema did not report a stale saved schema", jn_resp);
        }
        JSON_DECREF(jn_resp)
        file_remove(saved_dir, TREEDB_NAME ".treedb_schema.json");
    }

    /*
     *  The file in use carries no derived `fkey` mark: parse_schema() adds
     *  one to every column a hook points at, apply-schema parsed the saved
     *  schema to validate it and wrote the parsed copy (L-3 of the
     *  2026-09-23 independent review); treedb_open_db() never writes them.
     */
    {
        char in_use_dir[PATH_MAX];
        build_path(in_use_dir, sizeof(in_use_dir), priv->path_database, TREEDB_NAME, NULL);
        json_t *in_use = load_json_from_file(gobj, in_use_dir, TREEDB_NAME ".treedb_schema.json", 0);
        int idx_t; json_t *jn_topic;
        json_array_foreach(json_object_get(in_use, "topics"), idx_t, jn_topic) {
            const char *col_name; json_t *col;
            json_object_foreach(json_object_get(jn_topic, "cols"), col_name, col) {
                if(json_is_object(json_object_get(col, "fkey"))) {
                    result += save_fail(gobj, "TEST FAIL: apply-schema wrote a derived fkey mark", col);
                }
            }
        }
        JSON_DECREF(in_use)
    }

    /*
     *  Put in place by a rename of a flushed temporary: none is left behind,
     *  and the file keeps the mode the tranger gives it (0660, never 0440)
     */
    {
        char in_use_dir[PATH_MAX];
        build_path(in_use_dir, sizeof(in_use_dir), priv->path_database, TREEDB_NAME, NULL);
        char in_use_file[PATH_MAX];
        build_path(in_use_file, sizeof(in_use_file), in_use_dir, TREEDB_NAME ".treedb_schema.json", NULL);
        struct stat st;
        if(file_exists(in_use_dir, "." TREEDB_NAME ".treedb_schema.json.tmp") ||
                stat(in_use_file, &st) < 0 || (st.st_mode & 0777) != 0660) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: apply-schema left a temporary or changed the mode",
                "mode",         "%o", (unsigned)(st.st_mode & 0777),
                NULL
            );
            result += -1;
        }
    }

    /*
     *  The file decides: the literal is older, the saved header runs
     */
    if(reopen_test_treedb(gobj, FALSE) < 0) {
        return result - 1;
    }
    char header[NAME_MAX];
    client_col_header(gobj, "users", "username", header, sizeof(header));
    if(strcmp(header, "Saved header")!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: the applied schema did not open",
            "header",       "%s", header,
            NULL
        );
        result += -1;
    }

    return result;
}

/***************************************************************************
 *  A saved draft that is REVERTED in the editor is withdrawn by the next
 *  save (M-A of the 2026-09-23 independent review).
 *
 *  saved-schema diffs the draft against the SAVED schema (it is the newer
 *  one), save-schema against the file IN USE. So after "edit, save, edit
 *  back" saved-schema said `users` was unsaved, save-schema answered
 *  "nothing to save" and left the saved file in place: the mark never
 *  cleared, and Apply would have installed the change the operator had
 *  taken back. The draft is the file in use again, so the save withdraws
 *  the saved schema: removed, logged, said in the answer.
 ***************************************************************************/
PRIVATE int set_draft_username_header(hgobj gobj, const char *header)
{
    json_t *ids = system_topic_cols(gobj, "users");
    const char *username_id = json_string_value(json_object_get(ids, "username"));
    json_t *edited = gobj_update_node(
        gobj_find_service(SYSTEM_TREEDB, FALSE),
        "cols",
        json_pack("{s:s, s:s}", "id", username_id?username_id:"", "header", header),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    int ret = edited? 0 : -1;
    JSON_DECREF(edited)
    JSON_DECREF(ids)
    return ret;
}

PRIVATE int check_reverted_draft_withdraws_the_save(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    json_t *jn_resp;

    if(reopen_test_treedb(gobj, FALSE) < 0) {
        return -1;  // Error already logged
    }
    char saved_dir[PATH_MAX];
    build_path(saved_dir, sizeof(saved_dir), priv->path_database, "__system__", "saved_schemas", NULL);
    file_remove(saved_dir, TREEDB_NAME ".treedb_schema.json");   /*  a save of an earlier check  */

    /*
     *  The header the draft and the file in use agree on now
     */
    char original[NAME_MAX];
    {
        json_t *ids = system_topic_cols(gobj, "users");
        snprintf(original, sizeof(original), "%s",
            json_string_value(json_object_get(ids, "username__header"))?
            json_string_value(json_object_get(ids, "username__header")) : "");
        JSON_DECREF(ids)
    }

    /*
     *  Edit and save: a saved schema newer than the file in use
     */
    if(set_draft_username_header(gobj, "Taken back") < 0) {
        return result + save_fail(gobj, "TEST FAIL: the draft edit was refused", NULL);
    }
    jn_resp = treedbs_command(gobj, "save-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 ||
            !file_exists(saved_dir, TREEDB_NAME ".treedb_schema.json")) {
        result += save_fail(gobj, "TEST FAIL: the draft was not saved", jn_resp);
    }
    JSON_DECREF(jn_resp)

    /*
     *  Edit back: the draft is the file in use again
     */
    if(set_draft_username_header(gobj, original) < 0) {
        return result + save_fail(gobj, "TEST FAIL: the draft revert was refused", NULL);
    }

    /*
     *  dry_run says it would withdraw the saved schema, and removes nothing
     */
    jn_resp = treedbs_command(gobj, "save-schema", json_pack("{s:b}", "dry_run", 1));
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 ||
            !kw_get_bool(gobj, jn_resp, "data`withdrawn", 0, 0) ||
            !file_exists(saved_dir, TREEDB_NAME ".treedb_schema.json")) {
        result += save_fail(gobj, "TEST FAIL: a dry_run save of a reverted draft", jn_resp);
    }
    JSON_DECREF(jn_resp)

    /*
     *  The save withdraws it
     */
    jn_resp = treedbs_command(gobj, "save-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 ||
            !kw_get_bool(gobj, jn_resp, "data`withdrawn", 0, 0) ||
            file_exists(saved_dir, TREEDB_NAME ".treedb_schema.json")) {
        result += save_fail(gobj, "TEST FAIL: the save of a reverted draft left the saved schema", jn_resp);
    }
    JSON_DECREF(jn_resp)

    /*
     *  Nothing is unsaved, nothing applies
     */
    jn_resp = treedbs_command(gobj, "saved-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 ||
            kw_get_bool(gobj, jn_resp, "data`can_apply", 1, 0) ||
            kw_get_bool(gobj, jn_resp, "data`saved", 1, 0) ||
            json_object_size(kw_get_dict(gobj, jn_resp, "data`draft_changed", 0, 0)) != 0) {
        result += save_fail(gobj, "TEST FAIL: a withdrawn save still reads as saved or unsaved", jn_resp);
    }
    JSON_DECREF(jn_resp)

    json_int_t in_use_v = disk_schema_version(gobj);
    jn_resp = treedbs_command(gobj, "apply-schema", json_object());
    if(kw_get_bool(gobj, jn_resp, "data`applied", 0, 0) || disk_schema_version(gobj) != in_use_v) {
        result += save_fail(gobj, "TEST FAIL: apply-schema installed a withdrawn save", jn_resp);
    }
    JSON_DECREF(jn_resp)

    /*
     *  With nothing saved, the answer is the plain one
     */
    jn_resp = treedbs_command(gobj, "save-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 ||
            kw_get_bool(gobj, jn_resp, "data`withdrawn", 1, 0)) {
        result += save_fail(gobj, "TEST FAIL: a save with nothing to save or withdraw", jn_resp);
    }
    JSON_DECREF(jn_resp)

    /*
     *  The withdraw leaves __system__ at the number of the save (it never
     *  goes down). A literal that IS the file in use (its version, its
     *  content) must not be told "behind the schema in use" at every open
     *  (review of the second fix round, 2026-09-23). The expected log list
     *  pins that nothing is said.
     */
    {
        char in_use_dir[PATH_MAX];
        build_path(in_use_dir, sizeof(in_use_dir), priv->path_database, TREEDB_NAME, NULL);
        json_t *literal = load_json_from_file(gobj, in_use_dir, TREEDB_NAME ".treedb_schema.json", 0);
        jn_resp = treedbs_command(gobj, "close-treedb", json_pack("{s:b}", "force", 1));
        JSON_DECREF(jn_resp)
        if(!literal || open_test_treedb(gobj, literal) < 0) {   // literal owned
            result += save_fail(gobj, "TEST FAIL: cannot reopen with the file in use as the literal", NULL);
        }
    }

    return result;
}

/***************************************************************************
 *  A column MOVED in the draft reads as a change in saved-schema's diff
 *  (review of the second fix round, 2026-09-23). The order of the columns
 *  is part of a schema -- the order a table paints them in -- and the diff
 *  keyed them by name: a save that only moved a column showed the versions
 *  raised and nothing else.
 ***************************************************************************/
PRIVATE int set_draft_col_order(hgobj gobj, const char *col_name, json_t *order) // owned
{
    json_t *ids = system_topic_cols(gobj, "users");
    const char *col_id = json_string_value(json_object_get(ids, col_name));
    json_t *edited = gobj_update_node(
        gobj_find_service(SYSTEM_TREEDB, FALSE),
        "cols",
        json_pack("{s:s, s:o}", "id", col_id?col_id:"", "order", order),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    int ret = edited? 0 : -1;
    JSON_DECREF(edited)
    JSON_DECREF(ids)
    return ret;
}

PRIVATE int check_reorder_reads_as_a_change(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    json_t *jn_resp;

    char saved_dir[PATH_MAX];
    build_path(saved_dir, sizeof(saved_dir), priv->path_database, "__system__", "saved_schemas", NULL);
    file_remove(saved_dir, TREEDB_NAME ".treedb_schema.json");

    /*
     *  `email` is declared before `departments`; the draft swaps them, the
     *  way an editor renumbers the columns it moves
     */
    json_int_t email_order = -1;
    json_int_t departments_order = -1;
    {
        json_t *ids = system_topic_cols(gobj, "users");
        json_t *col = gobj_get_node(gobj_find_service(SYSTEM_TREEDB, FALSE), "cols",
            json_pack("{s:s}", "id", json_string_value(json_object_get(ids, "email"))), 0, gobj);
        email_order = kw_get_int(gobj, col, "order", -1, KW_WILD_NUMBER);
        JSON_DECREF(col)
        col = gobj_get_node(gobj_find_service(SYSTEM_TREEDB, FALSE), "cols",
            json_pack("{s:s}", "id", json_string_value(json_object_get(ids, "departments"))), 0, gobj);
        departments_order = kw_get_int(gobj, col, "order", -1, KW_WILD_NUMBER);
        JSON_DECREF(col)
        JSON_DECREF(ids)
    }
    if(email_order < 0 || departments_order <= email_order ||
            set_draft_col_order(gobj, "email", json_integer(departments_order)) < 0 ||
            set_draft_col_order(gobj, "departments", json_integer(email_order)) < 0) {
        return save_fail(gobj, "TEST FAIL: the draft reorder was refused", NULL);
    }
    jn_resp = treedbs_command(gobj, "save-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 ||
            !json_integer_value(json_object_get(kw_get_dict(gobj, jn_resp, "data`topic_versions", 0, 0), "users"))) {
        result += save_fail(gobj, "TEST FAIL: a moved column was not saved", jn_resp);
    }
    JSON_DECREF(jn_resp)

    jn_resp = treedbs_command(gobj, "saved-schema", json_object());
    if(json_object_size(kw_get_dict(gobj, jn_resp, "data`draft_changed", 0, 0)) != 0) {
        result += save_fail(gobj, "TEST FAIL: a saved reorder reads as unsaved", jn_resp);
    }
    json_t *changed = kw_get_dict(gobj, jn_resp, "data`diff`changed", 0, 0);
    json_t *order_row = json_object_get(changed, "topics`users`__cols_order__");
    if(!order_row ||
            !strstr(json_string_value(json_object_get(order_row, "to"))?
                json_string_value(json_object_get(order_row, "to")) : "", "departments, email")) {
        result += save_fail(gobj, "TEST FAIL: saved-schema's diff does not show the moved column", jn_resp);
    }
    JSON_DECREF(jn_resp)

    /*
     *  Moved back: the draft is the file in use, the save is withdrawn
     */
    set_draft_col_order(gobj, "email", json_integer(email_order));
    set_draft_col_order(gobj, "departments", json_integer(departments_order));
    jn_resp = treedbs_command(gobj, "save-schema", json_object());
    if(!kw_get_bool(gobj, jn_resp, "data`withdrawn", 0, 0)) {
        result += save_fail(gobj, "TEST FAIL: the column moved back did not withdraw the save", jn_resp);
    }
    JSON_DECREF(jn_resp)

    return result;
}

/***************************************************************************
 *  A `required` container column declared WITHOUT a default stays required
 *  through save + apply (review of the second fix round, 2026-09-23: revert
 *  of L-2, b6f66cdf8).
 *
 *  The meta-schema stores `{}` for a column that declares no default (the
 *  attribute is a blob), and it cannot tell that `{}` from a `default: {}`
 *  the author wrote. b6f66cdf8 kept `{}` on every required column, so a
 *  required dict/list/array/blob column with NO default came out of save +
 *  apply with `default: {}` -- and a default fills the field, so `required`
 *  never refused a record again. The placeholder is dropped again, on every
 *  column. The trade-off, pinned here too: a required column that really
 *  declared `default: {}` loses it through save + apply.
 ***************************************************************************/
PRIVATE json_t *create_draft_col(hgobj gobj, json_t *kw_col) // owned
{
    char users_topic_id[NAME_MAX];
    system_topic_id(gobj, "users", users_topic_id, sizeof(users_topic_id));
    char users_fkey[NAME_MAX + sizeof("topics^^cols")];
    snprintf(users_fkey, sizeof(users_fkey), "topics^%s^cols", users_topic_id);
    json_object_set_new(kw_col, "topics", json_string(users_fkey));

    return gobj_update_node(
        gobj_find_service(SYSTEM_TREEDB, FALSE),
        "cols",
        kw_col,
        json_pack("{s:b, s:b, s:b}", "create", 1, "autolink", 1, "refs", 1),
        gobj
    );
}

PRIVATE json_t *saved_users_col(hgobj gobj, json_t *jn_resp, const char *col_name)
{
    json_t *col = NULL;
    int idx; json_t *topic;
    json_array_foreach(kw_get_list(gobj, jn_resp, "data`schema`topics", 0, 0), idx, topic) {
        if(strcmp(kw_get_str(gobj, topic, "id", "", 0), "users")==0) {
            col = json_object_get(json_object_get(topic, "cols"), col_name);
        }
    }
    return col;
}

PRIVATE int check_required_default_placeholder_dropped(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    json_t *jn_resp;

    char saved_dir[PATH_MAX];
    build_path(saved_dir, sizeof(saved_dir), priv->path_database, "__system__", "saved_schemas", NULL);
    file_remove(saved_dir, TREEDB_NAME ".treedb_schema.json");

    /*
     *  Two draft columns of `users`: one declares NO default, the other
     *  declares `default: {}` -- stored the same way in __system__
     */
    json_t *no_default = create_draft_col(gobj, json_pack("{s:s, s:s, s:s, s:[s,s]}",
        "value", "required_list", "header", "Required list", "type", "list",
        "flag", "persistent", "required"));
    json_t *empty_default = create_draft_col(gobj, json_pack("{s:s, s:s, s:s, s:[s,s], s:{}}",
        "value", "required_dict", "header", "Required dict", "type", "dict",
        "flag", "persistent", "required", "default"));
    if(!no_default || !empty_default) {
        JSON_DECREF(no_default)
        JSON_DECREF(empty_default)
        return save_fail(gobj, "TEST FAIL: the required draft columns were refused", NULL);
    }
    char no_default_id[NAME_MAX];
    char empty_default_id[NAME_MAX];
    snprintf(no_default_id, sizeof(no_default_id), "%s", kw_get_str(gobj, no_default, "id", "", 0));
    snprintf(empty_default_id, sizeof(empty_default_id), "%s", kw_get_str(gobj, empty_default, "id", "", 0));
    JSON_DECREF(no_default)
    JSON_DECREF(empty_default)

    /*
     *  Neither carries a default in the schema a save writes
     */
    jn_resp = treedbs_command(gobj, "save-schema", json_object());
    json_t *col_list = saved_users_col(gobj, jn_resp, "required_list");
    json_t *col_dict = saved_users_col(gobj, jn_resp, "required_dict");
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 || !col_list || !col_dict ||
            json_object_get(col_list, "default") || json_object_get(col_dict, "default")) {
        result += save_fail(gobj, "TEST FAIL: a required column carries the placeholder default in the saved schema",
            jn_resp);
    }
    JSON_DECREF(jn_resp)

    /*
     *  ...nor reads as "default added" in what the save changes
     */
    jn_resp = treedbs_command(gobj, "saved-schema", json_object());
    {
        const char *id_k; json_t *v_k;
        json_object_foreach(kw_get_dict(gobj, jn_resp, "data`diff`added", 0, 0), id_k, v_k) {
            size_t len = strlen(id_k);
            if(len >= 8 && strcmp(id_k + len - 8, "`default")==0) {
                result += save_fail(gobj, "TEST FAIL: saved-schema reads a placeholder default as added", jn_resp);
                break;
            }
        }
    }
    JSON_DECREF(jn_resp)

    /*
     *  Applied and opened, the column with no default is REQUIRED: a
     *  record without it is refused
     */
    jn_resp = treedbs_command(gobj, "apply-schema", json_object());
    if(!kw_get_bool(gobj, jn_resp, "data`applied", 0, 0)) {
        result += save_fail(gobj, "TEST FAIL: the required columns were not applied", jn_resp);
    }
    JSON_DECREF(jn_resp)
    if(reopen_test_treedb(gobj, FALSE) < 0) {
        return result - 1;
    }
    json_t *node = gobj_create_node(
        gobj_find_service(TREEDB_NAME, FALSE),
        "users",
        json_pack("{s:s, s:s, s:{}}", "id", "no_required_list", "username", "nobody", "required_dict"),
        0,
        gobj
    );
    if(node) {
        result += save_fail(gobj, "TEST FAIL: a required column with no default accepted a record without it", node);
    }
    JSON_DECREF(node)

    /*
     *  Back to the schema of the other checks: the draft columns go, and
     *  that is published and applied
     */
    const char *ids[] = {no_default_id, empty_default_id, NULL};
    for(int i = 0; ids[i]; i++) {
        if(gobj_delete_node(
                gobj_find_service(SYSTEM_TREEDB, FALSE), "cols", json_pack("{s:s}", "id", ids[i]),
                json_pack("{s:b}", "force", 1), gobj) < 0) {
            result += save_fail(gobj, "TEST FAIL: a required draft column could not be removed", NULL);
        }
    }
    jn_resp = treedbs_command(gobj, "save-schema", json_object());
    JSON_DECREF(jn_resp)
    jn_resp = treedbs_command(gobj, "apply-schema", json_object());
    JSON_DECREF(jn_resp)
    if(reopen_test_treedb(gobj, FALSE) < 0) {
        return result - 1;
    }
    return result;
}

/***************************************************************************
 *  A literal with the version of the file in use and the SAME schema,
 *  written with its cols as a LIST, is not "another content" (L-6 of the
 *  2026-09-23 independent review). The file apply-schema wrote keys them
 *  by name; compared as they were written, every open of such a literal
 *  said "another content: NOT applied".
 ***************************************************************************/
PRIVATE int check_same_schema_other_form_is_quiet(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    char in_use_dir[PATH_MAX];
    build_path(in_use_dir, sizeof(in_use_dir), priv->path_database, TREEDB_NAME, NULL);
    json_t *literal = load_json_from_file(gobj, in_use_dir, TREEDB_NAME ".treedb_schema.json", 0);
    if(!literal) {
        return save_fail(gobj, "TEST FAIL: no file in use to write as a literal", NULL);
    }
    int idx; json_t *topic;
    json_array_foreach(json_object_get(literal, "topics"), idx, topic) {
        json_t *as_list = json_array();
        const char *col_name; json_t *col;
        json_object_foreach(json_object_get(topic, "cols"), col_name, col) {
            json_array_append(as_list, col);
        }
        json_object_set_new(topic, "cols", as_list);
    }
    /*
     *  The last projection came from ANOTHER literal (what apply-schema
     *  leaves: __system__ at the saved version, `c_schema_version` at the
     *  literal it was projected from)
     */
    json_int_t in_use_v = kw_get_int(gobj, literal, "schema_version", 0, KW_WILD_NUMBER);
    json_t *edited = gobj_update_node(
        gobj_find_service(SYSTEM_TREEDB, FALSE),
        "treedbs",
        json_pack("{s:s, s:I}", "id", TREEDB_NAME, "c_schema_version", in_use_v - 1),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    JSON_DECREF(edited)

    json_t *jn_resp = treedbs_command(gobj, "close-treedb", json_pack("{s:b}", "force", 1));
    JSON_DECREF(jn_resp)
    if(open_test_treedb(gobj, literal) < 0) {   // literal owned
        result += -1;
    }
    return result;
}

/***************************************************************************
 *  A literal that TAKES OVER the file in use re-projects only the topics
 *  it raised past that file (M-B of the 2026-09-23 independent review).
 *
 *  A save not applied raises __system__ past the file in use; a literal
 *  newer than the file (but not than __system__) then takes over, and
 *  runs. It was projected with the rule of `impose`: every topic that
 *  differed was re-written, so an operator's edit of a topic the developer
 *  never raised was overwritten in __system__ -- while that topic, not
 *  raised, goes on running from the file (tranger2 installs a topic only
 *  when its topic_version is higher) and a literal N+1 arriving the
 *  ordinary way leaves it alone.
 ***************************************************************************/
PRIVATE json_int_t in_use_topic_version(hgobj gobj, const char *topic_name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), priv->path_database, TREEDB_NAME, NULL);
    json_t *jn = load_json_from_file(gobj, dir, TREEDB_NAME ".treedb_schema.json", 0);
    json_int_t v = -1;
    int idx; json_t *topic;
    json_array_foreach(json_object_get(jn, "topics"), idx, topic) {
        if(strcmp(kw_get_str(gobj, topic, "id", "", 0), topic_name)==0) {
            v = kw_get_int(gobj, topic, "topic_version", 0, KW_WILD_NUMBER);
        }
    }
    JSON_DECREF(jn)
    return v;
}

PRIVATE int check_takeover_projects_raised_topics_only(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    json_t *jn_resp;

    if(reopen_test_treedb(gobj, FALSE) < 0) {
        return -1;  // Error already logged
    }
    char saved_dir[PATH_MAX];
    build_path(saved_dir, sizeof(saved_dir), priv->path_database, "__system__", "saved_schemas", NULL);
    file_remove(saved_dir, TREEDB_NAME ".treedb_schema.json");

    json_int_t in_use_v = disk_schema_version(gobj);
    json_int_t users_in_use_tv = in_use_topic_version(gobj, "users");
    json_int_t departments_in_use_tv = in_use_topic_version(gobj, "departments");

    /*
     *  The operator edits `departments` and saves, never applies:
     *  __system__ goes to the file's version + 1
     */
    {
        json_t *ids = system_topic_cols(gobj, "departments");
        const char *name_id = json_string_value(json_object_get(ids, "name"));
        json_t *edited = gobj_update_node(
            gobj_find_service(SYSTEM_TREEDB, FALSE),
            "cols",
            json_pack("{s:s, s:s}", "id", name_id?name_id:"", "header", "Operator name"),
            json_pack("{s:b}", "refs", 1),
            gobj
        );
        if(!edited) {
            result += save_fail(gobj, "TEST FAIL: the operator edit was refused", NULL);
        }
        JSON_DECREF(edited)
        JSON_DECREF(ids)
    }
    jn_resp = treedbs_command(gobj, "save-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
        result += save_fail(gobj, "TEST FAIL: the operator save failed", jn_resp);
    }
    JSON_DECREF(jn_resp)

    /*
     *  The developer's literal: the file's version + 1 (so it takes over
     *  the file, while it is not newer than __system__), `users` raised
     *  with a change, `departments` NOT raised
     */
    json_t *literal = legalstring2json(schema_test2, TRUE);
    json_object_set_new(literal, "schema_version", json_integer(in_use_v + 1));
    int idx; json_t *topic;
    json_array_foreach(json_object_get(literal, "topics"), idx, topic) {
        const char *topic_name = kw_get_str(gobj, topic, "id", "", 0);
        if(strcmp(topic_name, "users")==0) {
            json_object_set_new(topic, "topic_version", json_integer(users_in_use_tv + 1));
            json_object_set_new(
                json_object_get(json_object_get(topic, "cols"), "email"),
                "header",
                json_string("Mail from C")
            );
        } else if(strcmp(topic_name, "departments")==0) {
            json_object_set_new(topic, "topic_version", json_integer(departments_in_use_tv));
        }
    }
    jn_resp = treedbs_command(gobj, "close-treedb", json_pack("{s:b}", "force", 1));
    JSON_DECREF(jn_resp)
    if(open_test_treedb(gobj, literal) < 0) {   // literal owned
        return result - 1;
    }

    if(file_exists(saved_dir, TREEDB_NAME ".treedb_schema.json")) {
        result += save_fail(gobj, "TEST FAIL: a literal that took over the file left the save made against it", NULL);
    }

    json_t *users = system_topic_cols(gobj, "users");
    json_t *departments = system_topic_cols(gobj, "departments");
    const char *email_header = json_string_value(json_object_get(users, "email__header"));
    const char *name_header = json_string_value(json_object_get(departments, "name__header"));
    if(!email_header || strcmp(email_header, "Mail from C")!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: the topic the literal raised was not projected",
            "email_header", "%s", email_header?email_header:"",
            NULL
        );
        result += -1;
    }
    if(!name_header || strcmp(name_header, "Operator name")!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a topic the literal did not raise lost the operator's edit",
            "name_header",  "%s", name_header?name_header:"",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(users)
    JSON_DECREF(departments)

    char header[NAME_MAX];
    client_col_header(gobj, "users", "email", header, sizeof(header));
    if(strcmp(header, "Mail from C")!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: the literal that takes over does not run",
            "header",       "%s", header,
            NULL
        );
        result += -1;
    }

    /*
     *  With NO file in use the literal takes over too, and the warning says
     *  that, not "a draft saved over it" (L-6 of the same review): the
     *  expected log list pins the message.
     */
    jn_resp = treedbs_command(gobj, "save-schema", json_object());   /*  __system__ ahead again  */
    json_int_t saved_v = kw_get_int(gobj, jn_resp, "data`schema_version", 0, KW_WILD_NUMBER);
    JSON_DECREF(jn_resp)
    json_t *literal2 = legalstring2json(schema_test2, TRUE);
    json_object_set_new(literal2, "schema_version", json_integer(in_use_v + 1));
    json_array_foreach(json_object_get(literal2, "topics"), idx, topic) {
        const char *topic_name = kw_get_str(gobj, topic, "id", "", 0);
        if(strcmp(topic_name, "users")==0) {
            json_object_set_new(topic, "topic_version", json_integer(users_in_use_tv + 1));
            json_object_set_new(
                json_object_get(json_object_get(topic, "cols"), "email"),
                "header",
                json_string("Mail from C")
            );
        } else if(strcmp(topic_name, "departments")==0) {
            json_object_set_new(topic, "topic_version", json_integer(departments_in_use_tv));
        }
    }
    jn_resp = treedbs_command(gobj, "close-treedb", json_pack("{s:b}", "force", 1));
    JSON_DECREF(jn_resp)
    {
        char in_use_dir[PATH_MAX];
        build_path(in_use_dir, sizeof(in_use_dir), priv->path_database, TREEDB_NAME, NULL);
        file_remove(in_use_dir, TREEDB_NAME ".treedb_schema.json");
    }
    if(open_test_treedb(gobj, literal2) < 0) {   // literal2 owned
        result += -1;
    }

    /*
     *  The literal (in_use_v + 1) is older than the save (saved_v): taking
     *  the missing file over, it wrote ITS number into __system__, which
     *  never goes down (review of the second fix round, 2026-09-23)
     */
    /*  ...and the save, newer than the literal, was made against the file
     *  that is gone: withdrawn, never applicable over the literal  */
    jn_resp = treedbs_command(gobj, "saved-schema", json_object());
    if(file_exists(saved_dir, TREEDB_NAME ".treedb_schema.json") ||
            kw_get_bool(gobj, jn_resp, "data`can_apply", 1, 0)) {
        result += save_fail(gobj, "TEST FAIL: a save older than the missing file is still applicable over the literal", jn_resp);
    }
    JSON_DECREF(jn_resp)

    json_int_t system_v = system_schema_version(gobj, "schema_version");
    if(saved_v <= in_use_v + 1 || system_v < saved_v) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "TEST FAIL: a take-over with no file in use lowered __system__'s schema_version",
            "saved_version",    "%d", (int)saved_v,
            "literal_version",  "%d", (int)(in_use_v + 1),
            "system_version",   "%d", (int)system_v,
            NULL
        );
        result += -1;
    }

    return result;
}

/***************************************************************************
 *  A literal arriving the ORDINARY way (newer than __system__) follows the
 *  rule of the take-over: a topic it raises past the FILE IN USE runs from
 *  the literal, so __system__ says the literal (review of the second fix
 *  round, 2026-09-23: C_TREEDB medium).
 *
 *  The operator saves an edit of `users` (never applied): `users` goes to
 *  the file's topic_version + 1 in __system__. The developer ships a literal
 *  two schema versions ahead that raises `users` to the same number with
 *  another change. treedb_open_db() installs it (newer than the file) and
 *  tranger2 installs `users` (newer than its topic_cols.json): the treedb
 *  runs the developer's header. __system__ used to keep the operator's
 *  draft and log "differs from the one in use ... not applied" -- a false
 *  log, and a next save that would have reverted the developer's change
 *  with no word. Taken over (literal one version ahead), the same edit was
 *  replaced: one rule for both now.
 ***************************************************************************/
PRIVATE int check_ordinary_literal_follows_the_file(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    json_t *jn_resp;

    if(reopen_test_treedb(gobj, FALSE) < 0) {
        return -1;  // Error already logged
    }
    char saved_dir[PATH_MAX];
    build_path(saved_dir, sizeof(saved_dir), priv->path_database, "__system__", "saved_schemas", NULL);
    file_remove(saved_dir, TREEDB_NAME ".treedb_schema.json");

    json_int_t in_use_v = disk_schema_version(gobj);
    json_int_t users_in_use_tv = in_use_topic_version(gobj, "users");

    {
        json_t *ids = system_topic_cols(gobj, "users");
        const char *email_id = json_string_value(json_object_get(ids, "email"));
        json_t *edited = gobj_update_node(
            gobj_find_service(SYSTEM_TREEDB, FALSE),
            "cols",
            json_pack("{s:s, s:s}", "id", email_id?email_id:"", "header", "Operator mail"),
            json_pack("{s:b}", "refs", 1),
            gobj
        );
        if(!edited) {
            result += save_fail(gobj, "TEST FAIL: the operator edit was refused", NULL);
        }
        JSON_DECREF(edited)
        JSON_DECREF(ids)
    }
    jn_resp = treedbs_command(gobj, "save-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 ||
            kw_get_int(gobj, jn_resp, "data`topic_versions`users", 0, KW_WILD_NUMBER) != users_in_use_tv + 1) {
        result += save_fail(gobj, "TEST FAIL: the operator save did not raise users past the file", jn_resp);
    }
    JSON_DECREF(jn_resp)

    /*
     *  The developer's literal: newer than __system__ (the ordinary path),
     *  `users` raised past the file to the number the save gave the draft
     */
    json_t *literal = legalstring2json(schema_test2, TRUE);
    json_object_set_new(literal, "schema_version", json_integer(in_use_v + 2));
    int idx; json_t *topic;
    json_array_foreach(json_object_get(literal, "topics"), idx, topic) {
        if(strcmp(kw_get_str(gobj, topic, "id", "", 0), "users")==0) {
            json_object_set_new(topic, "topic_version", json_integer(users_in_use_tv + 1));
            json_object_set_new(
                json_object_get(json_object_get(topic, "cols"), "email"),
                "header",
                json_string("Mail from C")
            );
        }
    }
    jn_resp = treedbs_command(gobj, "close-treedb", json_pack("{s:b}", "force", 1));
    JSON_DECREF(jn_resp)
    if(open_test_treedb(gobj, literal) < 0) {   // literal owned
        return result - 1;
    }

    /*
     *  The file the operator's save was published against is gone: the save
     *  is withdrawn. The drafts of the topics the literal did not raise are
     *  still in __system__, and the next save publishes them against the new
     *  file (review of the second fix round, 2026-09-23)
     */
    if(file_exists(saved_dir, TREEDB_NAME ".treedb_schema.json")) {
        result += save_fail(gobj, "TEST FAIL: a literal installed over the file left the save made against it", NULL);
    }

    /*
     *  ...and the API says so, not only the log: the saved schema it
     *  withdrew and the topic whose saved draft it replaced (L1 of the third
     *  independent review, 2026-09-23)
     */
    {
        json_t *jn_saved = treedbs_command(gobj, "saved-schema", json_object());
        json_t *w = kw_get_dict(gobj, jn_saved, "data`withdrawn_at_open", 0, 0);
        if(kw_get_int(gobj, w, "saved_schema_version", 0, 0) <= in_use_v ||
                strcmp(kw_get_str(gobj, w, "topics`users", "", 0), "saved")!=0) {
            result += save_fail(gobj, "TEST FAIL: saved-schema does not say what the open withdrew", jn_saved);
        }
        JSON_DECREF(jn_saved)
    }

    char running[NAME_MAX];
    client_col_header(gobj, "users", "email", running, sizeof(running));
    json_t *users = system_topic_cols(gobj, "users");
    const char *in_system = json_string_value(json_object_get(users, "email__header"));
    if(strcmp(running, "Mail from C")!=0 || !in_system || strcmp(in_system, running)!=0 ||
            system_topic_version(gobj, "users") != users_in_use_tv + 1) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", "TEST FAIL: __system__ does not say the topic the treedb runs from the literal",
            "running",          "%s", running,
            "in_system",        "%s", in_system?in_system:"",
            "users_version",    "%d", (int)system_topic_version(gobj, "users"),
            NULL
        );
        result += -1;
    }
    JSON_DECREF(users)

    return result;
}

/***************************************************************************
 *  Helpers of the third independent review (2026-09-23): an edit of one
 *  column of the draft, a literal that raises one topic, and what the FILE
 *  in use says of a column.
 ***************************************************************************/
PRIVATE int set_draft_col_header(hgobj gobj, const char *topic_name, const char *col_name,
    const char *header)
{
    json_t *ids = system_topic_cols(gobj, topic_name);
    const char *col_id = json_string_value(json_object_get(ids, col_name));
    json_t *edited = gobj_update_node(
        gobj_find_service(SYSTEM_TREEDB, FALSE),
        "cols",
        json_pack("{s:s, s:s}", "id", col_id?col_id:"", "header", header),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    int ret = edited? 0 : -1;
    JSON_DECREF(edited)
    JSON_DECREF(ids)
    return ret;
}

PRIVATE json_t *literal_raising(hgobj gobj, json_int_t schema_version,
    const char *topic_name, json_int_t topic_version, const char *col_name, const char *header)
{
    json_t *literal = legalstring2json(schema_test2, TRUE);
    json_object_set_new(literal, "schema_version", json_integer(schema_version));
    int idx; json_t *topic;
    json_array_foreach(json_object_get(literal, "topics"), idx, topic) {
        if(strcmp(kw_get_str(gobj, topic, "id", "", 0), topic_name)!=0) {
            continue;
        }
        json_object_set_new(topic, "topic_version", json_integer(topic_version));
        if(col_name) {
            json_object_set_new(
                json_object_get(json_object_get(topic, "cols"), col_name),
                "header",
                json_string(header)
            );
        }
    }
    return literal;
}

PRIVATE void set_literal_topic_version(hgobj gobj, json_t *literal, const char *topic_name,
    json_int_t topic_version)
{
    int idx; json_t *topic;
    json_array_foreach(json_object_get(literal, "topics"), idx, topic) {
        if(strcmp(kw_get_str(gobj, topic, "id", "", 0), topic_name)==0) {
            json_object_set_new(topic, "topic_version", json_integer(topic_version));
        }
    }
}

PRIVATE void file_col_header(hgobj gobj, const char *topic_name, const char *col_name,
    char *bf, size_t bfsize)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    bf[0] = 0;
    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), priv->path_database, TREEDB_NAME, NULL);
    json_t *jn = load_json_from_file(gobj, dir, TREEDB_NAME ".treedb_schema.json", 0);
    int idx; json_t *topic;
    json_array_foreach(json_object_get(jn, "topics"), idx, topic) {
        if(strcmp(kw_get_str(gobj, topic, "id", "", 0), topic_name)!=0) {
            continue;
        }
        json_t *cols = json_object_get(topic, "cols");
        json_t *col = NULL;
        if(json_is_object(cols)) {
            col = json_object_get(cols, col_name);
        } else {
            int idx2; json_t *c;
            json_array_foreach(cols, idx2, c) {
                if(strcmp(kw_get_str(gobj, c, "id", "", 0), col_name)==0) {
                    col = c;
                }
            }
        }
        snprintf(bf, bfsize, "%s", kw_get_str(gobj, col, "header", "", 0));
    }
    JSON_DECREF(jn)
}

/***************************************************************************
 *  The three homes of one column agree: what the treedb RUNS, what the
 *  schema FILE in use says, and the draft in __system__. Logs what differs.
 ***************************************************************************/
PRIVATE int check_col_agrees(hgobj gobj, const char *label, const char *topic_name,
    const char *col_name, const char *expected)
{
    char running[NAME_MAX];
    char in_file[NAME_MAX];
    client_col_header(gobj, topic_name, col_name, running, sizeof(running));
    file_col_header(gobj, topic_name, col_name, in_file, sizeof(in_file));
    json_t *cols = system_topic_cols(gobj, topic_name);
    char key[NAME_MAX];
    snprintf(key, sizeof(key), "%s__header", col_name);
    const char *in_system = json_string_value(json_object_get(cols, key));

    int result = 0;
    if(strcmp(running, expected)!=0 || strcmp(in_file, expected)!=0 ||
            !in_system || strcmp(in_system, expected)!=0 ||
            in_use_topic_version(gobj, topic_name) != disk_topic_version(gobj, topic_name)) {
        gobj_log_error(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_INTERNAL,
            "msg",              "%s", label,
            "topic",            "%s", topic_name,
            "col",              "%s", col_name,
            "expected",         "%s", expected,
            "running",          "%s", running,
            "in_file",          "%s", in_file,
            "in_system",        "%s", in_system?in_system:"",
            "file_version",     "%d", (int)in_use_topic_version(gobj, topic_name),
            "running_version",  "%d", (int)disk_topic_version(gobj, topic_name),
            NULL
        );
        result = -1;
    }
    JSON_DECREF(cols)
    return result;
}

/***************************************************************************
 *  An APPLIED schema that has not been opened yet survives the literal of
 *  the next open (M-1 of the third independent review, 2026-09-23).
 *
 *  apply-schema writes the file in use; the treedb reads it at its next
 *  open. When that open brings a literal newer than the file (upgrade-yunos
 *  after an apply), treedb_open_db() wrote the literal over the WHOLE file:
 *  the apply was gone with no word, __system__ kept the operator's value,
 *  and a topic the literal did not raise went on running a third version.
 *
 *  Now a literal that takes over the file takes over only the topics it
 *  raises past the one in use; the file keeps its own topic otherwise, so
 *  an applied topic the literal does not raise runs at this open, as the
 *  operator applied it:
 *
 *    C  the literal gives `users` the number the apply gave it (a tie):
 *       the file wins, as ties always do, and the apply runs.
 *    D  the literal raises only `departments`: `users` runs as applied,
 *       `departments` as the literal says.
 ***************************************************************************/
PRIVATE int check_unopened_apply_survives_a_literal(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    json_t *jn_resp;

    if(reopen_test_treedb(gobj, FALSE) < 0) {
        return -1;  // Error already logged
    }
    char saved_dir[PATH_MAX];
    build_path(saved_dir, sizeof(saved_dir), priv->path_database, "__system__", "saved_schemas", NULL);
    file_remove(saved_dir, TREEDB_NAME ".treedb_schema.json");

    /*
     *  C: save + apply `users`, and restart with a literal two versions
     *  ahead that raises `users` to the number the apply gave it
     */
    json_int_t file_v = disk_schema_version(gobj);
    json_int_t users_tv = in_use_topic_version(gobj, "users");
    if(set_draft_col_header(gobj, "users", "email", "Operator applied mail") < 0) {
        return save_fail(gobj, "TEST FAIL: the operator edit was refused", NULL);
    }
    jn_resp = treedbs_command(gobj, "save-schema", json_object());
    JSON_DECREF(jn_resp)
    jn_resp = treedbs_command(gobj, "apply-schema", json_object());
    if(!kw_get_bool(gobj, jn_resp, "data`applied", 0, 0) ||
            in_use_topic_version(gobj, "users") != users_tv + 1) {
        result += save_fail(gobj, "TEST FAIL: the operator's edit was not applied", jn_resp);
    }
    JSON_DECREF(jn_resp)

    jn_resp = treedbs_command(gobj, "close-treedb", json_pack("{s:b}", "force", 1));
    JSON_DECREF(jn_resp)
    if(open_test_treedb(gobj,
            literal_raising(gobj, file_v + 2, "users", users_tv + 1, "email", "Mail from C")) < 0) {
        return result - 1;
    }
    result += check_col_agrees(gobj,
        "TEST FAIL: an applied schema not opened yet was thrown away by a literal of the same topic_version",
        "users", "email", "Operator applied mail");

    /*
     *  D: save + apply `users` again, and restart with a literal that
     *  raises only `departments` (its `users` is the version that runs)
     */
    file_v = disk_schema_version(gobj);
    users_tv = disk_topic_version(gobj, "users");
    json_int_t departments_tv = in_use_topic_version(gobj, "departments");
    if(set_draft_col_header(gobj, "users", "email", "Operator applied again") < 0) {
        return result + save_fail(gobj, "TEST FAIL: the operator edit was refused", NULL);
    }
    jn_resp = treedbs_command(gobj, "save-schema", json_object());
    JSON_DECREF(jn_resp)
    jn_resp = treedbs_command(gobj, "apply-schema", json_object());
    if(!kw_get_bool(gobj, jn_resp, "data`applied", 0, 0)) {
        result += save_fail(gobj, "TEST FAIL: the second operator's edit was not applied", jn_resp);
    }
    JSON_DECREF(jn_resp)

    jn_resp = treedbs_command(gobj, "close-treedb", json_pack("{s:b}", "force", 1));
    JSON_DECREF(jn_resp)
    json_t *literal = literal_raising(gobj, file_v + 2,
        "departments", departments_tv + 1, "name", "Name from C");
    set_literal_topic_version(gobj, literal, "users", users_tv);
    if(open_test_treedb(gobj, literal) < 0) {   // literal owned
        return result - 1;
    }
    result += check_col_agrees(gobj,
        "TEST FAIL: an applied topic the literal did not raise does not run as applied",
        "users", "email", "Operator applied again");
    result += check_col_agrees(gobj,
        "TEST FAIL: the topic the literal raised does not run from the literal",
        "departments", "name", "Name from C");

    return result;
}

/***************************************************************************
 *  What an open withdrew, as the API says it: `withdrawn_at_open` of
 *  saved-schema (and of the `treedbs` row). Return is YOURS, {} when the
 *  answer carries none.
 ***************************************************************************/
PRIVATE json_t *withdrawn_at_open(hgobj gobj)
{
    json_t *jn_resp = treedbs_command(gobj, "saved-schema", json_object());
    json_t *w = kw_get_dict(gobj, jn_resp, "data`withdrawn_at_open", 0, 0);
    json_t *ret = w? json_incref(w) : json_object();
    JSON_DECREF(jn_resp)
    return ret;
}

PRIVATE json_t *treedbs_row_withdrawn_at_open(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *jn_resp = gobj_command(priv->gobj_treedbs, "treedbs", json_object(), gobj);
    json_t *ret = NULL;
    int idx; json_t *row;
    json_array_foreach(kw_get_list(gobj, jn_resp, "data", 0, 0), idx, row) {
        if(strcmp(kw_get_str(gobj, row, "treedb_name", "", 0), TREEDB_NAME)==0) {
            json_t *w = json_object_get(row, "withdrawn_at_open");
            ret = w? json_incref(w) : NULL;
        }
    }
    JSON_DECREF(jn_resp)
    return ret? ret : json_object();
}

/***************************************************************************
 *  A draft a literal replaces at open is SAID, and through the API, not
 *  only in the log (L1-L3 of the third independent review, 2026-09-23):
 *
 *    unsaved   an edit never saved, of a topic the literal raises: it was
 *              dropped with no word at all;
 *    withdrawn a save taken back (the draft is the file again): nothing is
 *              replaced, and nothing is said -- it was told "replaces its
 *              saved draft";
 *    applied   an apply not opened yet, of a topic the literal raises past
 *              it: the literal runs, and the apply is said to be replaced.
 *
 *  `withdrawn_at_open` of saved-schema and of the `treedbs` row carries
 *  `topics: {<topic>: "unsaved" | "saved" | "applied"}` and the
 *  `saved_schema_version` the open withdrew (0 when none).
 ***************************************************************************/
PRIVATE int check_replaced_drafts_are_said(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    json_t *jn_resp;

    if(reopen_test_treedb(gobj, FALSE) < 0) {
        return -1;  // Error already logged
    }
    char saved_dir[PATH_MAX];
    build_path(saved_dir, sizeof(saved_dir), priv->path_database, "__system__", "saved_schemas", NULL);
    file_remove(saved_dir, TREEDB_NAME ".treedb_schema.json");

    /*
     *  Unsaved: an edit of `users`, and a literal newer than everything
     *  that raises `users`
     */
    json_int_t sv = system_schema_version(gobj, "schema_version");
    if(disk_schema_version(gobj) > sv) {
        sv = disk_schema_version(gobj);
    }
    json_int_t users_tv = disk_topic_version(gobj, "users");
    if(set_draft_col_header(gobj, "users", "username", "Unsaved login") < 0) {
        return save_fail(gobj, "TEST FAIL: the operator edit was refused", NULL);
    }
    jn_resp = treedbs_command(gobj, "close-treedb", json_pack("{s:b}", "force", 1));
    JSON_DECREF(jn_resp)
    if(open_test_treedb(gobj,
            literal_raising(gobj, sv + 1, "users", users_tv + 1, "email", "Mail A")) < 0) {
        return result - 1;
    }
    {
        json_t *w = withdrawn_at_open(gobj);
        json_t *row = treedbs_row_withdrawn_at_open(gobj);
        if(strcmp(kw_get_str(gobj, w, "topics`users", "", 0), "unsaved")!=0 ||
                kw_get_int(gobj, w, "saved_schema_version", -1, 0) != 0 ||
                !json_equal(w, row)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: an unsaved draft the literal replaced is not said",
                "saved_schema", "%j", w,
                "treedbs",      "%j", row,
                NULL
            );
            result += -1;
        }
        JSON_DECREF(w)
        JSON_DECREF(row)
    }

    /*
     *  Withdrawn: saved, taken back, saved again (withdrawn); a literal that
     *  takes over the file raising `users`. Nothing is replaced.
     */
    char original[NAME_MAX];
    client_col_header(gobj, "users", "email", original, sizeof(original));
    set_draft_col_header(gobj, "users", "email", "Taken back");
    jn_resp = treedbs_command(gobj, "save-schema", json_object());
    JSON_DECREF(jn_resp)
    set_draft_col_header(gobj, "users", "email", original);
    jn_resp = treedbs_command(gobj, "save-schema", json_object());
    if(!kw_get_bool(gobj, jn_resp, "data`withdrawn", 0, 0)) {
        result += save_fail(gobj, "TEST FAIL: the save taken back was not withdrawn", jn_resp);
    }
    JSON_DECREF(jn_resp)
    users_tv = disk_topic_version(gobj, "users");
    json_int_t file_v = disk_schema_version(gobj);
    jn_resp = treedbs_command(gobj, "close-treedb", json_pack("{s:b}", "force", 1));
    JSON_DECREF(jn_resp)
    if(open_test_treedb(gobj,
            literal_raising(gobj, file_v + 1, "users", users_tv + 1, "email", "Mail B")) < 0) {
        return result - 1;
    }
    {
        json_t *w = withdrawn_at_open(gobj);
        if(json_object_size(kw_get_dict(gobj, w, "topics", 0, 0)) != 0 ||
                kw_get_int(gobj, w, "saved_schema_version", 0, 0) != 0) {
            result += save_fail(gobj, "TEST FAIL: a draft withdrawn by a save reads as replaced at open", w);
        }
        JSON_DECREF(w)
    }

    /*
     *  Applied, never opened, and the literal raises `users` past the apply
     */
    set_draft_col_header(gobj, "users", "email", "Applied, never ran");
    jn_resp = treedbs_command(gobj, "save-schema", json_object());
    JSON_DECREF(jn_resp)
    jn_resp = treedbs_command(gobj, "apply-schema", json_object());
    if(!kw_get_bool(gobj, jn_resp, "data`applied", 0, 0)) {
        result += save_fail(gobj, "TEST FAIL: the edit was not applied", jn_resp);
    }
    JSON_DECREF(jn_resp)
    json_int_t applied_tv = in_use_topic_version(gobj, "users");
    json_int_t applied_v = disk_schema_version(gobj);
    jn_resp = treedbs_command(gobj, "close-treedb", json_pack("{s:b}", "force", 1));
    JSON_DECREF(jn_resp)
    if(open_test_treedb(gobj,
            literal_raising(gobj, applied_v + 1, "users", applied_tv + 1, "email", "Mail C")) < 0) {
        return result - 1;
    }
    result += check_col_agrees(gobj,
        "TEST FAIL: the literal raised past an apply does not run",
        "users", "email", "Mail C");
    {
        json_t *w = withdrawn_at_open(gobj);
        if(strcmp(kw_get_str(gobj, w, "topics`users", "", 0), "applied")!=0) {
            result += save_fail(gobj, "TEST FAIL: an applied topic the literal replaced before it ran is not said", w);
        }
        JSON_DECREF(w)
    }

    return result;
}

/***************************************************************************
 *  __system__'s tranger LOST its lock (another process took its store
 *  while it was stopped): timeranger2 leaves it a replica, `master` false
 *  in its json. C_TREEDB must use and report THAT, not its `master`
 *  attribute (M3 of the 2026-09-23 independent review): delete-treedb
 *  asked the attribute and went on writing __system__. The json state a
 *  lost lock leaves is set here by hand.
 ***************************************************************************/
PRIVATE int check_lost_system_lock(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    hgobj gobj_tranger_system = gobj_find_service("tranger_system_schema", FALSE);
    json_t *tranger = gobj_tranger_system?
        gobj_read_pointer_attr(gobj_tranger_system, "tranger") : NULL;
    if(!tranger) {
        return save_fail(gobj, "TEST FAIL: no __system__ tranger", NULL);
    }
    json_object_set_new(tranger, "master", json_false());
    json_object_set_new(tranger, "master_lost", json_true());

    if(gobj_read_bool_attr(gobj_tranger_system, "master")) {
        result += save_fail(gobj, "TEST FAIL: the __system__ C_TRANGER still says master", NULL);
    }

    json_t *jn_resp = gobj_command(priv->gobj_treedbs, "treedbs", json_object(), gobj);
    int idx; json_t *row;
    BOOL system_seen = FALSE, client_seen = FALSE;
    json_array_foreach(kw_get_list(gobj, jn_resp, "data", 0, 0), idx, row) {
        const char *name = kw_get_str(gobj, row, "treedb_name", "", 0);
        json_t *master = json_object_get(row, "master");
        if(strcmp(name, "treedb_system_schema")==0) {
            system_seen = json_is_false(master);
        } else if(strcmp(name, TREEDB_NAME)==0) {
            client_seen = json_is_true(master);
        }
    }
    if(!system_seen || !client_seen) {
        result += save_fail(gobj, "TEST FAIL: treedbs does not report what each tranger IS", jn_resp);
    }
    JSON_DECREF(jn_resp)

    jn_resp = gobj_command(priv->gobj_treedbs, "delete-treedb",
        json_pack("{s:s, s:b}", "treedb_name", "treedb_never_opened", "force", 1), gobj);
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) != -1 ||
            !strstr(kw_get_str(gobj, jn_resp, "comment", "", 0), "READ-ONLY")) {
        result += save_fail(gobj, "TEST FAIL: delete-treedb wrote __system__ after its lock was lost", jn_resp);
    }
    JSON_DECREF(jn_resp)

    json_object_set_new(tranger, "master", json_true());
    json_object_del(tranger, "master_lost");
    return result;
}

/***************************************************************************
 *  Between a STOP of C_TREEDB and its next start, the __system__ tranger is
 *  stopped: tranger2_stop() gave its lock back, and its `master` still says
 *  what it was until something revives it (the first write or topic open,
 *  which is the start). The prechecks read that stale TRUE (review of the
 *  second fix round, 2026-09-23): save-schema and delete-treedb went on to
 *  a __system__ whose treedb is closed, and `treedbs` reported it master.
 *  What they read now is what the tranger holds: nothing, it is stopped.
 ***************************************************************************/
PRIVATE int check_stopped_system_is_not_master(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    gobj_stop(priv->gobj_treedbs);

    json_t *jn_resp = gobj_command(priv->gobj_treedbs, "treedbs", json_object(), gobj);
    BOOL system_seen = FALSE;
    int idx; json_t *row;
    json_array_foreach(kw_get_list(gobj, jn_resp, "data", 0, 0), idx, row) {
        if(strcmp(kw_get_str(gobj, row, "treedb_name", "", 0), "treedb_system_schema")==0) {
            system_seen = json_is_false(json_object_get(row, "master")) &&
                json_is_true(json_object_get(row, "stopped"));
        }
    }
    if(!system_seen) {
        result += save_fail(gobj, "TEST FAIL: treedbs reports a stopped __system__ as master", jn_resp);
    }
    JSON_DECREF(jn_resp)

    jn_resp = treedbs_command(gobj, "save-schema", json_object());
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) != -1 ||
            !strstr(kw_get_str(gobj, jn_resp, "comment", "", 0), "STOPPED")) {
        result += save_fail(gobj, "TEST FAIL: save-schema went on with __system__ stopped", jn_resp);
    }
    JSON_DECREF(jn_resp)

    jn_resp = gobj_command(priv->gobj_treedbs, "delete-treedb",
        json_pack("{s:s, s:b}", "treedb_name", "treedb_never_opened", "force", 1), gobj);
    if(kw_get_int(gobj, jn_resp, "result", 0, 0) != -1 ||
            !strstr(kw_get_str(gobj, jn_resp, "comment", "", 0), "STOPPED")) {
        result += save_fail(gobj, "TEST FAIL: delete-treedb went on with __system__ stopped", jn_resp);
    }
    JSON_DECREF(jn_resp)

    gobj_start(priv->gobj_treedbs);

    /*
     *  Started again, it is the master again
     */
    hgobj gobj_tranger_system = gobj_find_service("tranger_system_schema", FALSE);
    if(!gobj_read_bool_attr(gobj_tranger_system, "master")) {
        result += save_fail(gobj, "TEST FAIL: __system__ is not the master after its restart", NULL);
    }
    return result;
}

/***************************************************************************
 *  A second treedb for the apply-schema of all of them
 ***************************************************************************/
PRIVATE char schema_test_b[] = "\
{                                                                   \n\
    'id': '"B_TREEDB_NAME"',                                        \n\
    'schema_version': 1,                                            \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'things',                                         \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': 1,                                     \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Name',                               \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

PRIVATE json_int_t b_in_use_version(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), priv->path_database, B_TREEDB_NAME, NULL);
    json_t *jn = load_json_from_file(gobj, dir, B_TREEDB_NAME ".treedb_schema.json", 0);
    json_int_t v = kw_get_int(gobj, jn, "schema_version", -1, KW_WILD_NUMBER);
    JSON_DECREF(jn)
    return v;
}

/***************************************************************************
 *  apply-schema of every treedb is ALL OR NONE (M2 of the 2026-09-23
 *  review). It applied them one by one: A replaced, B refused, answer -1,
 *  and a console that read the -1 as "nothing applied" did not restart --
 *  A went in use silently at some later start. And each row says whether
 *  ITS file was replaced (`data.applied`), which is what a console restarts
 *  a yuno for.
 ***************************************************************************/
PRIVATE int check_apply_all_or_none(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;
    json_t *jn_resp;

    /*
     *  A: the test treedb, impose off, with a saved schema newer than the
     *  one in use
     */
    if(reopen_test_treedb(gobj, FALSE) < 0) {
        return -1;  // Error already logged
    }
    json_t *ids = system_topic_cols(gobj, "users");
    const char *username_id = json_string_value(json_object_get(ids, "username"));
    json_t *edited = gobj_update_node(
        gobj_find_service(SYSTEM_TREEDB, FALSE),
        "cols",
        json_pack("{s:s, s:s}", "id", username_id?username_id:"", "header", "All or none"),
        json_pack("{s:b}", "refs", 1),
        gobj
    );
    JSON_DECREF(edited)
    JSON_DECREF(ids)
    jn_resp = treedbs_command(gobj, "save-schema", json_object());
    json_int_t a_saved = kw_get_int(gobj, jn_resp, "data`schema_version", 0, KW_WILD_NUMBER);
    JSON_DECREF(jn_resp)
    json_int_t a_in_use = disk_schema_version(gobj);
    if(a_saved <= a_in_use) {
        return save_fail(gobj, "TEST FAIL: no saved schema newer than the one in use to apply", NULL);
    }

    /*
     *  B: another treedb, impose off, whose saved schema does not parse
     */
    helper_quote2doublequote(schema_test_b);
    jn_resp = gobj_command(priv->gobj_treedbs, "open-treedb",
        json_pack("{s:s, s:i, s:s, s:o}",
            "filename_mask", "%Y",
            "exit_on_error", 0,
            "treedb_name", B_TREEDB_NAME,
            "treedb_schema", legalstring2json(schema_test_b, TRUE)
        ),
        gobj
    );
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
        result += save_fail(gobj, "TEST FAIL: cannot open the second treedb", jn_resp);
    }
    JSON_DECREF(jn_resp)

    char saved_dir[PATH_MAX];
    build_path(saved_dir, sizeof(saved_dir), priv->path_database, "__system__", "saved_schemas", NULL);
    save_json_to_file(gobj, saved_dir, B_TREEDB_NAME ".treedb_schema.json",
        02770, 0660, 0, TRUE, FALSE,
        json_pack("{s:s, s:i, s:[{s:s, s:s, s:s}]}",
            "id", B_TREEDB_NAME,
            "schema_version", 50,
            "topics",
                "id", "things",
                "pkey", "id",
                "cols", "not columns"
        )
    );

    /*
     *  B cannot be applied: NOTHING is written, A neither, and no row says
     *  applied
     */
    jn_resp = gobj_command(priv->gobj_treedbs, "apply-schema", json_object(), gobj);
    {
        int rows_applied = 0;
        int rows = 0;
        int idx; json_t *row;
        json_array_foreach(kw_get_list(gobj, jn_resp, "data", 0, 0), idx, row) {
            rows++;
            if(kw_get_bool(gobj, row, "data`applied", 1, 0)) {
                rows_applied++;
            }
        }
        if(kw_get_int(gobj, jn_resp, "result", 0, 0) >= 0 || rows != 2 || rows_applied != 0 ||
                disk_schema_version(gobj) != a_in_use || b_in_use_version(gobj) != 1) {
            result += save_fail(gobj, "TEST FAIL: an apply of all wrote something although one could not be applied", jn_resp);
        }
    }
    JSON_DECREF(jn_resp)

    /*
     *  B has nothing to apply: A is applied, and says so
     */
    file_remove(saved_dir, B_TREEDB_NAME ".treedb_schema.json");
    jn_resp = gobj_command(priv->gobj_treedbs, "apply-schema", json_object(), gobj);
    {
        json_t *rows = kw_get_list(gobj, jn_resp, "data", 0, 0);
        json_t *row = json_array_get(rows, 0);
        if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 || json_array_size(rows) != 1 ||
                strcmp(kw_get_str(gobj, row, "treedb_name", "", 0), TREEDB_NAME)!=0 ||
                !kw_get_bool(gobj, row, "data`applied", 0, 0) ||
                disk_schema_version(gobj) != a_saved) {
            result += save_fail(gobj, "TEST FAIL: an apply of all did not apply the one that could be", jn_resp);
        }
    }
    JSON_DECREF(jn_resp)

    /*
     *  Nothing left to apply: 0 and no row, so nobody restarts
     */
    jn_resp = gobj_command(priv->gobj_treedbs, "apply-schema", json_object(), gobj);
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0 ||
            json_array_size(kw_get_list(gobj, jn_resp, "data", 0, 0)) != 0) {
        result += save_fail(gobj, "TEST FAIL: an apply with nothing to apply answered a row", jn_resp);
    }
    JSON_DECREF(jn_resp)


    jn_resp = gobj_command(priv->gobj_treedbs, "close-treedb",
        json_pack("{s:s, s:b}", "treedb_name", B_TREEDB_NAME, "force", 1), gobj);
    JSON_DECREF(jn_resp)

    return result;
}

/***************************************************************************
 *  EVERY command of C_TREEDB refuses a user with no permission. Walked
 *  from the table itself, like tests/c/c_node_authz does for C_NODE: a
 *  hand-written list is what let save-schema and apply-schema ask the
 *  wrong permission for a release (M41/M42 and F-2 of the reviews).
 ***************************************************************************/
PRIVATE int check_every_command_refuses_nobody(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    const char *asks_nothing[] = {
        "help",     /*  the command list, and the parameters of each  */
        "authzs",   /*  the permissions the service checks  */
        NULL
    };
    const sdata_desc_t *cmds = gclass_command_desc(gclass_find_by_name(C_TREEDB), NULL, TRUE);
    for(const sdata_desc_t *it = cmds; it && it->name; it++) {
        BOOL exempt = FALSE;
        for(int i = 0; asks_nothing[i]; i++) {
            if(strcmp(it->name, asks_nothing[i])==0) {
                exempt = TRUE;
            }
        }
        if(exempt) {
            continue;
        }
        json_t *jn_resp = gobj_command(priv->gobj_treedbs, it->name,
            json_pack("{s:s, s:s, s:s, s:b, s:b, s:s}",
                "__username__", "denied@test",
                "treedb_name", TREEDB_NAME,
                "topic_name", "users",
                "force", 1,
                "dry_run", 1,
                "filename_mask", "%Y"
            ),
            gobj
        );
        int ret = (int)kw_get_int(gobj, jn_resp, "result", 0, 0);
        if(ret != -403) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "TEST FAIL: a C_TREEDB command served a user with no permission",
                "command",      "%s", it->name,
                "result",       "%d", ret,
                NULL
            );
            result += -1;
        }
        JSON_DECREF(jn_resp)
    }

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
    result += check_no_saved_schema(gobj);

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
     *  Test 4: __system__ is raised past the literal
     *  (10, what a save does to a draft that is never
     *  applied), while the file in use stays at 2. A
     *  literal 3 is newer than the FILE, so the treedb
     *  runs it -- and __system__ must say so: it is
     *  projected, over the draft, and a warning says
     *  the draft went. Judged by __system__'s 10 it was
     *  "behind", never projected, and the next save put
     *  the draft back over it (the second medium of M36,
     *  2026-09-23 review). Its topics land, and the
     *  treedb's schema_version stays 10: a number of
     *  __system__ never goes down. Then 11 lands as
     *  usual, under its own number. Nobody invents one.
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
        BOOL must_land = TRUE;

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

        /*  A number of __system__ never goes down: the literal 3 lands
         *  (c_schema_version 3, its header) under the 10 already there  */
        json_int_t expected_from_c = must_land? literal_version: 2;
        json_int_t expected_version = (literal_version > 10)? literal_version: 10;
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
     *  Test 6: an edit of a schema is a draft
     *-----------------------------------------------*/
    result += check_edits_are_drafts(gobj, ids_after);
    result += check_create_and_delete_are_drafts(gobj);

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
     *  Test 9b: dynamic_schema_treedbs names a treedb
     *  that opens from its file with impose_c_schema
     *  on, and `treedbs` says who decided.
     *-----------------------------------------------*/
    result += check_dynamic_schema_treedbs(gobj);

    /*-----------------------------------------------*
     *  Test 10: the yuno's code imposes the schema
     *  from C over the attribute: a value set by
     *  command cannot undo what the binary decides.
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
    /*-----------------------------------------------*
     *  Test 13b: an edit is a draft, save-schema
     *  publishes it beside the file in use, and
     *  apply-schema puts it in place when C does
     *  not impose the schema.
     *-----------------------------------------------*/
    result += check_save_and_apply(gobj);

    /*-----------------------------------------------*
     *  Test 13b2: a saved draft taken back in the
     *  editor is withdrawn by the next save
     *-----------------------------------------------*/
    result += check_reverted_draft_withdraws_the_save(gobj);

    /*-----------------------------------------------*
     *  Test 13b2b: a required column with no default
     *  stays required through save + apply; the same
     *  schema in another form is not "another content"
     *-----------------------------------------------*/
    result += check_reorder_reads_as_a_change(gobj);
    result += check_required_default_placeholder_dropped(gobj);
    result += check_same_schema_other_form_is_quiet(gobj);

    /*-----------------------------------------------*
     *  Test 13b3: a literal that takes over the file
     *  in use re-projects only the topics it raised,
     *  and a literal arriving the ordinary way
     *  follows the same rule
     *-----------------------------------------------*/
    result += check_ordinary_literal_follows_the_file(gobj);
    result += check_takeover_projects_raised_topics_only(gobj);

    /*-----------------------------------------------*
     *  Test 13b4: an applied schema not opened yet
     *  survives the literal of the next open, and a
     *  draft an open replaces is said, in the API
     *-----------------------------------------------*/
    result += check_unopened_apply_survives_a_literal(gobj);
    result += check_replaced_drafts_are_said(gobj);

    /*-----------------------------------------------*
     *  Test 13c: apply-schema of every treedb is all
     *  or none, and each row says `applied`
     *-----------------------------------------------*/
    result += check_apply_all_or_none(gobj);

    /*-----------------------------------------------*
     *  Test 13d: every command of C_TREEDB refuses a
     *  user with no permission
     *-----------------------------------------------*/
    result += check_every_command_refuses_nobody(gobj);

    /*-----------------------------------------------*
     *  Test 13e: __system__ lost its lock: C_TREEDB
     *  uses and reports what the tranger IS
     *-----------------------------------------------*/
    result += check_lost_system_lock(gobj);

    /*-----------------------------------------------*
     *  Test 13f: between a stop and the next start,
     *  __system__ is not the master
     *-----------------------------------------------*/
    result += check_stopped_system_is_not_master(gobj);

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
