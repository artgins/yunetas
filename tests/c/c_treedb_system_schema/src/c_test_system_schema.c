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
 *                 column pkey2 — `id` is a rowid),
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
 *  Client schema: two topics, one of them marked system_topic, so the
 *  round-trip of that flag is covered too.
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

    json_t *kw_treedbs = json_pack("{s:s, s:s, s:b, s:i, s:i, s:i}",
        "path", priv->path_database,
        "filename_mask", "%Y",
        "master", 1,
        "xpermission", 02770,
        "rpermission", 0660,
        "exit_on_error", LOG_OPT_TRACE_STACK
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
 *  Open the test treedb, letting the __system__ treedb be the schema
 *  source (use_internal_schema=0).
 ***************************************************************************/
PRIVATE int open_test_treedb(hgobj gobj, json_t *jn_schema) // owned
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw_treedb = json_pack("{s:s, s:i, s:s, s:b}",
        "filename_mask", "%Y",
        "exit_on_error", 0,
        "treedb_name", TREEDB_NAME,
        "use_internal_schema", 0
    );
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
    if(json_array_size(topics) != 2) {
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
            const char *id = kw_get_str(gobj, topic, "id", "", 0);
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
    if(json_array_size(cols) != 6) {
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

    json_t *cols_before = client_topic_cols(gobj, "users");

    /*-----------------------------------------------*
     *  Test 2: the projection alone rebuilds the
     *  schema. Close the treedb and remove its schema
     *  file, so that on re-open neither the file nor
     *  an input schema can supply it: what comes back
     *  must come from __system__.
     *-----------------------------------------------*/
    json_t *jn_resp = gobj_command(
        priv->gobj_treedbs,
        "close-treedb",
        json_pack("{s:s}", "treedb_name", TREEDB_NAME),
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
