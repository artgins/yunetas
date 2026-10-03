/***********************************************************************
 *          C_TEST_NODE_COMMANDS.C
 *
 *          GClass to test what the read commands of C_NODE answer
 *
 *          Their permissions are tested in test_c_node_authz; this test
 *          asks what each one ANSWERS, on a small treedb: departments in a
 *          tree (a self hook), users under departments, and items with a
 *          pkey2 ("version"). The commands: treedb-info, node, instances,
 *          pkey2s, jtree, parents, children, hooks, links, print-tranger,
 *          and what the snap commands do to the data (shoot-snap,
 *          activate-snap, deactivate-snap).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_node_commands.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define DATABASE    "c_node_commands"
#define TREEDB      "treedb_cmds_test"

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
    hgobj gobj_node;
    hgobj timer;
    json_t *tranger;
} PRIVATE_DATA;

/***************************************************************************
 *  Schema: departments in a tree, users under departments, items with a
 *  pkey2
 ***************************************************************************/
PRIVATE char schema_cmds_test[] = "\
{                                                                   \n\
    'schema_version': 1,                                            \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'topic_name': 'departments',                            \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Name',                               \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'department_id': {                                  \n\
                    'header': 'Top Department',                     \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'departments': {                                    \n\
                    'header': 'Departments',                        \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'departments': 'department_id'              \n\
                    }                                               \n\
                },                                                  \n\
                'users': {                                          \n\
                    'header': 'Users',                              \n\
                    'type': 'array',                                \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'users': 'departments'                      \n\
                    }                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'topic_name': 'users',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'departments': {                                    \n\
                    'header': 'Departments',                        \n\
                    'type': 'array',                                \n\
                    'flag': ['fkey']                                \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'topic_name': 'items',                                  \n\
            'pkey': 'id',                                           \n\
            'pkey2s': 'version',                                    \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'version': {                                        \n\
                    'header': 'Version',                            \n\
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




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    priv->tranger = tranger2_startup(0, jn_tranger, 0);

    helper_quote2doublequote(schema_cmds_test);
    json_t *jn_schema = legalstring2json(schema_cmds_test, TRUE);

    json_t *kw_resource = json_pack("{s:I, s:s, s:o, s:i}",
        "tranger", (json_int_t)(uintptr_t)priv->tranger,
        "treedb_name", TREEDB,
        "treedb_schema", jn_schema,
        "exit_on_error", LOG_OPT_TRACE_STACK
    );
    priv->gobj_node = gobj_create_pure_child("test_node", C_NODE, kw_resource, gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->gobj_node);
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
    gobj_stop(priv->gobj_node);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 100);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->tranger) {
        tranger2_shutdown(priv->tranger);   // the treedb was closed by C_NODE
        priv->tranger = NULL;
    }
}




                    /***************************
                     *      Commands
                     ***************************/




/***************************************************************************
 *              Command: help
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




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Ask C_NODE a command; the answer is YOURS
 ***************************************************************************/
PRIVATE json_t *ask(hgobj gobj, const char *command, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *resp = gobj_command(priv->gobj_node, command, kw, gobj);
    if(getenv("C_NODE_COMMANDS_PROBE")) {
        char *s = json_dumps(resp, JSON_INDENT(2));
        printf("=== %s\n%s\n", command, s? s : "");
        gbmem_free(s);
    }
    return resp;
}

/***************************************************************************
 *  Log a failed check (the test counts it)
 ***************************************************************************/
PRIVATE int fail(hgobj gobj, const char *what, json_t *resp)
{
    char *s = json_dumps(resp, JSON_COMPACT);
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "TEST FAIL",
        "what",         "%s", what,
        "answer",       "%s", s? s : "",
        NULL
    );
    gbmem_free(s);
    return -1;
}

/***************************************************************************
 *  Close the treedb and open it again: C_NODE stopped and started
 ***************************************************************************/
PRIVATE void reopen_treedb(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gobj_stop(priv->gobj_node);
    gobj_start(priv->gobj_node);
}

/***************************************************************************
 *  Create the data the commands are asked about
 ***************************************************************************/
PRIVATE int create_data(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *tranger = priv->tranger;
    int result = 0;

    json_t *top = treedb_create_node(tranger, TREEDB, "departments",
        json_pack("{s:s, s:s}", "id", "top", "name", "Top"));
    json_t *dev = treedb_create_node(tranger, TREEDB, "departments",
        json_pack("{s:s, s:s}", "id", "dev", "name", "Development"));
    json_t *ops = treedb_create_node(tranger, TREEDB, "departments",
        json_pack("{s:s, s:s}", "id", "ops", "name", "Operations"));
    json_t *ana = treedb_create_node(tranger, TREEDB, "users",
        json_pack("{s:s}", "id", "ana"));
    if(!top || !dev || !ops || !ana) {
        return fail(gobj, "create the nodes", NULL);
    }
    if(treedb_link_nodes(tranger, "departments", top, dev) < 0 ||
       treedb_link_nodes(tranger, "departments", top, ops) < 0 ||
       treedb_link_nodes(tranger, "users", dev, ana) < 0) {
        result += fail(gobj, "link the nodes", NULL);
    }
    json_t *v1 = treedb_create_node(tranger, TREEDB, "items",
        json_pack("{s:s, s:s, s:s}", "id", "item1", "version", "v1", "name", "first"));
    json_t *v2 = treedb_create_node(tranger, TREEDB, "items",
        json_pack("{s:s, s:s, s:s}", "id", "item1", "version", "v2", "name", "second"));
    if(!v1 || !v2) {
        result += fail(gobj, "create the instances", NULL);
    }
    return result;
}

/***************************************************************************
 *  The answer has `result` 0 and a string `expected` at `path` of `data`
 *  (an empty path: data itself)
 ***************************************************************************/
PRIVATE int expect_str(hgobj gobj, const char *what, json_t *resp, const char *path, const char *expected)
{
    json_t *data = kw_get_dict_value(gobj, resp, "data", 0, 0);
    const char *value = empty_string(path)? json_string_value(data) : kw_get_str(gobj, data, path, 0, 0);
    if(kw_get_int(gobj, resp, "result", -1, 0) < 0 || !value || strcmp(value, expected) != 0) {
        return fail(gobj, what, resp);
    }
    return 0;
}

/***************************************************************************
 *  `data` is a list of strings equal to `expected` (a json list)
 ***************************************************************************/
PRIVATE int expect_list(hgobj gobj, const char *what, json_t *resp, const char *expected)
{
    json_t *jn_expected = legalstring2json(expected, TRUE);
    json_t *data = kw_get_dict_value(gobj, resp, "data", 0, 0);
    int ret = 0;
    if(kw_get_int(gobj, resp, "result", -1, 0) < 0 || !json_equal(data, jn_expected)) {
        ret = fail(gobj, what, resp);
    }
    JSON_DECREF(jn_expected)
    return ret;
}

/***************************************************************************
 *  The ids of the dicts of `list`, joined by a space
 ***************************************************************************/
PRIVATE void ids_of(json_t *list, char *bf, size_t bfsize)
{
    bf[0] = 0;
    int idx; json_t *item;
    json_array_foreach(list, idx, item) {
        const char *id = json_is_string(item)? json_string_value(item) :
            json_string_value(json_object_get(item, "id"));
        size_t l = strlen(bf);
        snprintf(bf + l, bfsize - l, "%s%s", l? " " : "", id? id : "?");
    }
}

/***************************************************************************
 *  The ids of the list at `path` of `data` are `expected`
 ***************************************************************************/
PRIVATE int expect_ids(hgobj gobj, const char *what, json_t *resp, const char *path, const char *expected)
{
    json_t *data = kw_get_dict_value(gobj, resp, "data", 0, 0);
    json_t *list = empty_string(path)? data : kw_get_list(gobj, data, path, 0, 0);
    char ids[256];
    ids_of(list, ids, sizeof(ids));
    if(kw_get_int(gobj, resp, "result", -1, 0) < 0 || !json_is_array(list) || strcmp(ids, expected) != 0) {
        return fail(gobj, what, resp);
    }
    return 0;
}

/***************************************************************************
 *  A command that asks and checks, the answer dropped
 ***************************************************************************/
#define CHECK(check, what, command, kw, ...) {              \
    json_t *resp_ = ask(gobj, command, kw);                 \
    result += check(gobj, what, resp_, __VA_ARGS__);        \
    JSON_DECREF(resp_)                                      \
}

/***************************************************************************
 *  Run every check
 ***************************************************************************/
PRIVATE int run_tests(hgobj gobj)
{
    int result = create_data(gobj);

    /*
     *  treedb-info: its name, master, the schema version and the topics
     */
    {
        json_t *resp = ask(gobj, "treedb-info", json_object());
        result += expect_str(gobj, "treedb-info: the name", resp, "treedb_name", TREEDB);
        json_t *data = kw_get_dict_value(gobj, resp, "data", 0, 0);
        if(!kw_get_bool(gobj, data, "master", 0, 0) ||
                kw_get_int(gobj, data, "schema_version", 0, 0) != 1) {
            result += fail(gobj, "treedb-info: master and schema_version", resp);
        }
        char topics[256];
        ids_of(kw_get_list(gobj, data, "topics", 0, 0), topics, sizeof(topics));
        if(!strstr(topics, "departments users items")) {
            result += fail(gobj, "treedb-info: the topics", resp);
        }
        JSON_DECREF(resp)
    }

    /*
     *  node: a node with its links (refs, list_dict by default); and one
     *  that is not there
     */
    CHECK(expect_str, "node: its name", "node",
        json_pack("{s:s, s:s}", "topic_name", "departments", "node_id", "dev"), "name", "Development")
    CHECK(expect_ids, "node: its users hook", "node",
        json_pack("{s:s, s:s}", "topic_name", "departments", "node_id", "dev"), "users", "ana")
    CHECK(expect_ids, "node: its parent", "node",
        json_pack("{s:s, s:s}", "topic_name", "departments", "node_id", "dev"), "department_id", "top")
    {
        json_t *resp = ask(gobj, "node", json_pack("{s:s, s:s}", "topic_name", "departments", "node_id", "nobody"));
        const char *comment = kw_get_str(gobj, resp, "comment", "", 0);
        if(kw_get_int(gobj, resp, "result", 0, 0) != -1 || !strstr(comment, "Node not found") ||
                !json_is_null(kw_get_dict_value(gobj, resp, "data", 0, 0))) {
            result += fail(gobj, "node: one that is not there", resp);
        }
        JSON_DECREF(resp)
    }

    /*
     *  instances and pkey2s: the two versions of item1, by its pkey2
     */
    CHECK(expect_ids, "instances: both versions", "instances",
        json_pack("{s:s, s:s}", "topic_name", "items", "node_id", "item1"), "", "item1 item1")
    {
        json_t *resp = ask(gobj, "instances", json_pack("{s:s, s:s}", "topic_name", "items", "node_id", "item1"));
        json_t *data = kw_get_dict_value(gobj, resp, "data", 0, 0);
        if(json_array_size(data) != 2 ||
                strcmp(kw_get_str(gobj, json_array_get(data, 0), "version", "", 0), "v1") != 0 ||
                strcmp(kw_get_str(gobj, json_array_get(data, 1), "version", "", 0), "v2") != 0) {
            result += fail(gobj, "instances: v1 and v2", resp);
        }
        JSON_DECREF(resp)
    }
    CHECK(expect_list, "pkey2s", "pkey2s", json_pack("{s:s}", "topic_name", "items"), "[\"version\"]")

    /*
     *  jtree: the tree of top by the self hook, in `data` (rename_hook):
     *  each child whole, with its __path__
     */
    {
        json_t *resp = ask(gobj, "jtree", json_pack("{s:s, s:s, s:s, s:s}",
            "topic_name", "departments", "node_id", "top", "hook", "departments", "rename_hook", "data"));
        result += expect_ids(gobj, "jtree: the children of top", resp, "data", "dev ops");
        result += expect_str(gobj, "jtree: the root's path", resp, "__path__", "top");
        json_t *children = kw_get_list(gobj, kw_get_dict_value(gobj, resp, "data", 0, 0), "data", 0, 0);
        json_t *dev = json_array_get(children, 0);
        if(strcmp(kw_get_str(gobj, dev, "__path__", "", 0), "top`dev") != 0 ||
                strcmp(kw_get_str(gobj, dev, "name", "", 0), "Development") != 0 ||
                kw_get_dict_value(gobj, kw_get_dict_value(gobj, resp, "data", 0, 0), "departments", 0, 0)) {
            result += fail(gobj, "jtree: a child whole, the hook renamed", resp);
        }
        JSON_DECREF(resp)
    }

    /*
     *  parents and children, with no options (list_dict) and with them
     */
    CHECK(expect_ids, "parents: of dev", "parents",
        json_pack("{s:s, s:s}", "topic_name", "departments", "node_id", "dev"), "", "top")
    CHECK(expect_list, "parents: only_id", "parents",
        json_pack("{s:s, s:s, s:{s:b}}", "topic_name", "departments", "node_id", "dev", "options", "only_id", 1),
        "[\"top\"]")
    CHECK(expect_list, "parents: of the root", "parents",
        json_pack("{s:s, s:s}", "topic_name", "departments", "node_id", "top"), "[]")
    CHECK(expect_ids, "children: of top", "children",
        json_pack("{s:s, s:s, s:s}", "topic_name", "departments", "node_id", "top", "hook", "departments"),
        "", "dev ops")
    CHECK(expect_ids, "children: users of dev", "children",
        json_pack("{s:s, s:s, s:s}", "topic_name", "departments", "node_id", "dev", "hook", "users"),
        "", "ana")
    CHECK(expect_ids, "children: users of top, not recursive", "children",
        json_pack("{s:s, s:s, s:s}", "topic_name", "departments", "node_id", "top", "hook", "users"),
        "", "")

    /*
     *  hooks and links of a topic
     */
    CHECK(expect_list, "hooks: of departments", "hooks",
        json_pack("{s:s}", "topic_name", "departments"), "[\"departments\",\"users\"]")
    CHECK(expect_list, "links: of users", "links",
        json_pack("{s:s}", "topic_name", "users"), "[\"departments\"]")
    CHECK(expect_list, "links: of departments", "links",
        json_pack("{s:s}", "topic_name", "departments"), "[\"department_id\"]")

    /*
     *  print-tranger: a subtree of the tranger, by its path
     */
    CHECK(expect_str, "print-tranger: topics", "print-tranger",
        json_pack("{s:s}", "path", "topics"), "departments`topic_name", "departments")

    /*
     *  A kw that carries a gbuffer: the commands that keep a reference to
     *  their kw take it with kw_incref(), which takes one of the gbuffer
     *  too. With json_incref() (up to 7.25.4) the kw's two decrefs each
     *  dropped the gbuffer: "BAD gbuf_decref()".
     */
    {
        const char *with_gbuffer[] = {"treedbs", "links", "hooks", "node", NULL};
        for(int i = 0; with_gbuffer[i]; i++) {
            gbuffer_t *gbuf = gbuffer_create(32, 32);
            gbuffer_append_string(gbuf, "carried along");
            json_t *kw = json_pack("{s:s, s:s, s:I}",
                "topic_name", "departments",
                "node_id", "dev",
                "gbuffer", (json_int_t)(uintptr_t)gbuf
            );
            json_t *resp = ask(gobj, with_gbuffer[i], kw);
            if(kw_get_int(gobj, resp, "result", -1, 0) < 0) {
                result += fail(gobj, with_gbuffer[i], resp);
            }
            JSON_DECREF(resp)
        }
    }

    /*
     *  The snaps: shot, the node changed; activated (a mark on disk), the
     *  treedb opened again shows the node as it was; deactivated and opened
     *  again, the node as it is. The activation takes effect at the next
     *  open, as the agent does it (restart_nodes()).
     */
    CHECK(expect_ids, "shoot-snap", "shoot-snap", json_pack("{s:s}", "name", "s1"), "", "1")
    {
        PRIVATE_DATA *priv = gobj_priv_data(gobj);
        json_t *dev = treedb_get_node(priv->tranger, TREEDB, "departments", "dev");
        json_object_set_new(dev, "name", json_string("Dev 2"));
        if(treedb_save_node(priv->tranger, dev) < 0) {
            result += fail(gobj, "save the node after the snap", NULL);
        }
    }
    CHECK(expect_str, "after the snap: the node as it is", "node",
        json_pack("{s:s, s:s}", "topic_name", "departments", "node_id", "dev"), "name", "Dev 2")
    {
        json_t *resp = ask(gobj, "activate-snap", json_pack("{s:s}", "name", "s1"));
        if(kw_get_int(gobj, resp, "result", -1, 0) < 0) {
            result += fail(gobj, "activate-snap", resp);
        }
        JSON_DECREF(resp)
    }
    reopen_treedb(gobj);
    CHECK(expect_str, "snap active: the node as it was", "node",
        json_pack("{s:s, s:s}", "topic_name", "departments", "node_id", "dev"), "name", "Development")
    {
        json_t *resp = ask(gobj, "snaps", json_object());
        json_t *snap = json_array_get(kw_get_dict_value(gobj, resp, "data", 0, 0), 0);
        if(!kw_get_bool(gobj, snap, "active", 0, 0)) {
            result += fail(gobj, "snaps: s1 active", resp);
        }
        JSON_DECREF(resp)
    }
    {
        json_t *resp = ask(gobj, "deactivate-snap", json_object());
        if(kw_get_int(gobj, resp, "result", -1, 0) < 0) {
            result += fail(gobj, "deactivate-snap", resp);
        }
        JSON_DECREF(resp)
    }
    reopen_treedb(gobj);
    CHECK(expect_str, "snap deactivated: the node as it is", "node",
        json_pack("{s:s, s:s}", "topic_name", "departments", "node_id", "dev"), "name", "Dev 2")

    if(result == 0) {
        gobj_log_info(gobj, 0,
            "msgset", "%s", MSGSET_INFO,
            "msg", "%s", "All c_node command tests PASSED",
            NULL
        );
    }
    return result;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The treedb wrote a node: the parent of a C_NODE has to accept it
 ***************************************************************************/
PRIVATE int ac_treedb_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_TIMEOUT -- runs the checks inside the event loop, then exits
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    run_tests(gobj);

    gobj_log_info(gobj, 0,
        "msgset", "%s", MSGSET_INFO,
        "msg", "%s", "Exit to die",
        NULL
    );
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_destroy = mt_destroy,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST_NODE_COMMANDS);

/***************************************************************************
 *          Create the GClass
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

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,                ac_timeout,         0},
        {EV_TREEDB_NODE_CREATED,    ac_treedb_event,    0},
        {EV_TREEDB_NODE_UPDATED,    ac_treedb_event,    0},
        {EV_TREEDB_NODE_DELETED,    ac_treedb_event,    0},
        {EV_TREEDB_NODE_LINKED,     ac_treedb_event,    0},
        {EV_TREEDB_NODE_UNLINKED,   ac_treedb_event,    0},
        {EV_STOPPED,                ac_stopped,         0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_TREEDB_NODE_CREATED,    EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
        {EV_TREEDB_NODE_UPDATED,    EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
        {EV_TREEDB_NODE_DELETED,    EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
        {EV_TREEDB_NODE_LINKED,     EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
        {EV_TREEDB_NODE_UNLINKED,   EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
        {EV_STOPPED,                0},
        {0, 0}
    };

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        command_table,
        s_user_trace_level,
        0   // gcflag_t
    );
    return __gclass__ ? 0 : -1;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_test_node_commands(void)
{
    return create_gclass(C_TEST_NODE_COMMANDS);
}
