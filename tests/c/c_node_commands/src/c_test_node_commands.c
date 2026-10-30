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
/***************************************************************************
 *  The answer is a refusal (`result` -1) whose comment holds `needle`
 ***************************************************************************/
PRIVATE int expect_refused(hgobj gobj, const char *what, json_t *resp, const char *needle)
{
    const char *comment = kw_get_str(gobj, resp, "comment", "", 0);
    if(kw_get_int(gobj, resp, "result", 0, 0) != -1 || !strstr(comment, needle)) {
        return fail(gobj, what, resp);
    }
    return 0;
}

#define CHECK(check, what, command, kw, ...) {              \
    json_t *resp_ = ask(gobj, command, kw);                 \
    result += check(gobj, what, resp_, __VA_ARGS__);        \
    JSON_DECREF(resp_)                                      \
}

/***************************************************************************
 *  import-db of what export-db wrote, in each mode of if-resource-exists:
 *  exported with dev named "Dev 2", dev renamed "Changed", then
 *    - skip: nothing overwritten, dev stays "Changed", every node ignored;
 *    - overwrite: dev is "Dev 2" again, every node overwritten;
 *    - abort (the default): -1, ABORTED, at the first node that exists.
 ***************************************************************************/
PRIVATE int export_and_import(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    char root[PATH_MAX];
    build_path(root, sizeof(root), getenv("HOME"), "tests_yuneta", NULL);
    char realm[PATH_MAX];
    build_path(realm, sizeof(realm), root, "c_node_commands_realm", NULL);
    rmrdir(realm);
    register_yuneta_environment(root, "c_node_commands_realm", 02770, 0660);

    json_t *resp = ask(gobj, "export-db", json_pack("{s:s}", "filename", "roundtrip"));
    result += expect_str(gobj, "export-db: its file", resp, "filename", "roundtrip.trdb.json");
    JSON_DECREF(resp)

    char path[PATH_MAX];
    yuneta_realm_file(path, sizeof(path), "temp", "roundtrip.trdb.json", FALSE);
    gbuffer_t *gbuf64 = gbuffer_file2base64(path);
    if(!gbuf64) {
        result += fail(gobj, "export-db: the file cannot be read", NULL);
    } else {
        char *content64 = gbuffer_cur_rd_pointer(gbuf64);
        size_t len64 = gbuffer_leftbytes(gbuf64);

        json_t *dev = treedb_get_node(priv->tranger, TREEDB, "departments", "dev");
        json_object_set_new(dev, "name", json_string("Changed"));
        treedb_save_node(priv->tranger, dev);

        const char *modes[] = {"skip", "overwrite", "", NULL};
        for(int i = 0; modes[i]; i++) {
            json_t *kw = json_pack("{s:s#}", "content64", content64, (int)len64);
            if(*modes[i]) {
                json_object_set_new(kw, "if-resource-exists", json_string(modes[i]));
            }
            json_t *r = ask(gobj, "import-db", kw);
            json_t *data = kw_get_dict(gobj, r, "data", 0, 0);
            json_int_t ret = kw_get_int(gobj, r, "result", 0, 0);
            const char *comment = kw_get_str(gobj, r, "comment", "", 0);
            if(i == 0 && (ret != 0 || kw_get_int(gobj, data, "overwrite", -1, 0) != 0 ||
                    kw_get_int(gobj, data, "ignored", 0, 0) <= 0)) {
                result += fail(gobj, "import-db skip: all ignored, none overwritten", r);
            }
            if(i == 1 && (ret != 0 || kw_get_int(gobj, data, "overwrite", 0, 0) <= 0 ||
                    kw_get_int(gobj, data, "added", -1, 0) != 0)) {
                result += fail(gobj, "import-db overwrite: overwritten, none added", r);
            }
            if(i == 2 && (ret != -1 || !strstr(comment, "ABORTED") ||
                    kw_get_int(gobj, data, "abort", 0, 0) != 1)) {
                result += fail(gobj, "import-db abort: -1, ABORTED", r);
            }
            JSON_DECREF(r)

            json_t *node = ask(gobj, "node",
                json_pack("{s:s, s:s}", "topic_name", "departments", "node_id", "dev"));
            const char *expected = (i == 0)? "Changed" : "Dev 2";
            result += expect_str(gobj, modes[i][0]? modes[i] : "abort", node, "name", expected);
            JSON_DECREF(node)
        }
        GBUFFER_DECREF(gbuf64)
    }

    register_yuneta_environment(NULL, NULL, 02775, 0664);
    rmrdir(realm);
    return result;
}

/***************************************************************************
 *  What an rt list of `departments` sees of the peer's
 *  window. It is fed inside the treedb's append, and its callback is a
 *  subscriber like those of the publication.
 ***************************************************************************/
PRIVATE int rt_probe_window = -1;   // -1: the callback did not run

PRIVATE int rt_probe_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record_ex,
    json_t *jn_record   // owned
)
{
    rt_probe_window = treedb_peer_data()? 1 : 0;
    JSON_DECREF(jn_record)
    return 0;
}

/***************************************************************************
 *  Run every check
 ***************************************************************************/
PRIVATE int run_tests(hgobj gobj)
{
    int result = create_data(gobj);

    /*
     *  What a command names is the PEER's data: a name that is unknown or
     *  breaks the name rule is a WARNING and a refusal, the same for every
     *  command. main.c lists the warnings, in order.
     */
    CHECK(expect_refused, "refused: node of an unknown topic", "node",
        json_pack("{s:s, s:s}", "topic_name", "nosuch", "node_id", "x"), "Topic not found")
    CHECK(expect_refused, "refused: a node_id that is not a string", "node",
        json_pack("{s:s, s:i, s:s}", "topic_name", "departments", "node_id", 5,
            "__username__", "peer@example.com"),    // a peer's command: a WARNING
        "parameter 'node_id' must be a string, not an integer")
    CHECK(expect_refused, "refused: pkey2s of a topic with the delimiter", "pkey2s",
        json_pack("{s:s}", "topic_name", "a`b"), "Topic not found")
    {
        json_t *resp = ask(gobj, "parents", json_pack("{s:s, s:s, s:s}",
            "topic_name", "departments", "node_id", "dev", "link", "nolink"));
        if(kw_get_int(gobj, resp, "result", 0, 0) != -1 ||
                !strstr(kw_get_str(gobj, resp, "comment", "", 0), "cannot list the parents")) {
            result += fail(gobj, "refused: parents by an unknown link", resp);
        }
        JSON_DECREF(resp)
    }
    CHECK(expect_refused, "refused: children by a hook with the delimiter", "children",
        json_pack("{s:s, s:s, s:s}", "topic_name", "departments", "node_id", "top", "hook", "a`b"),
        "cannot list the children")
    CHECK(expect_refused, "refused: a parent ref of two parts", "link-nodes",
        json_pack("{s:s, s:s}", "parent_ref", "departments^top", "child_ref", "departments^dev"),
        "Wrong parent ref")
    CHECK(expect_refused, "refused: a child ref of an unknown topic", "link-nodes",
        json_pack("{s:s, s:s}", "parent_ref", "departments^top^departments", "child_ref", "us`ers^ana"),
        "Topic not found")
    CHECK(expect_refused, "refused: link by a column that is not a hook", "link-nodes",
        json_pack("{s:s, s:s}", "parent_ref", "departments^top^name", "child_ref", "departments^dev"),
        "Hook not found")

    /*
     *  A hook that does not take the child's topic, and
     *  an update of a node that does not exist, are the peer's data too:
     *  they were an ERROR of the treedb, the second with a stack and the
     *  whole kw dumped.
     */
    CHECK(expect_refused, "refused: link by a hook that does not link the child's topic", "link-nodes",
        json_pack("{s:s, s:s}", "parent_ref", "departments^top^users", "child_ref", "departments^dev"),
        "does not link topic")
    CHECK(expect_refused, "refused: unlink by a hook that does not link the child's topic", "unlink-nodes",
        json_pack("{s:s, s:s}", "parent_ref", "departments^top^users", "child_ref", "departments^dev"),
        "does not link topic")
    CHECK(expect_refused, "refused: update of a node that does not exist, no create", "update-node",
        json_pack("{s:s, s:{s:s, s:s}}", "topic_name", "departments",
            "record", "id", "nosuch", "name", "Nobody"),
        "Node not found: 'nosuch'")

    /*
     *  The rest of what a peer can get wrong is a
     *  WARNING too -- refused by C_NODE before the treedb is asked, or said
     *  by the treedb as the peer's (treedb_set_peer_data()). Each one was an
     *  ERROR, most with a stack.
     */
    CHECK(expect_refused, "refused: a record id that is not a string (update)", "update-node",
        json_pack("{s:s, s:{s:i}}", "topic_name", "departments", "record", "id", 5),
        "Wrong record")
    CHECK(expect_refused, "refused: a record id that is not a string (delete)", "delete-node",
        json_pack("{s:s, s:{s:i}}", "topic_name", "departments", "record", "id", 5),
        "Wrong record")
    CHECK(expect_refused, "refused: a content64 that is not an object", "update-node",
        json_pack("{s:s, s:s}", "topic_name", "departments", "content64", "WzFd"),   // [1]
        "Wrong record")
    CHECK(expect_refused, "refused: a content64 that is not json", "update-node",
        json_pack("{s:s, s:s}", "topic_name", "departments",
            "content64", "eyJwYXNzd29yZCI6InM0Y3JldCIs"),    // {"password":"s4cret",
        "Can't decode json content64")
    CHECK(expect_refused, "refused: a content64 that is not json (create)", "create-node",
        json_pack("{s:s, s:s}", "topic_name", "departments",
            "content64", "eyJwYXNzd29yZCI6InM0Y3JldCIs"),
        "Can't decode json content64")

    /*
     *  A NUL in a string of the record (a frame is
     *  decoded with JSON_ALLOW_NUL). C read the id "dx\u0000evil" as "dx",
     *  and the key "dx" was taken; an update of "top" with the id
     *  "top\u0000x" saved that id under "top". Refused, at any depth.
     */
    CHECK(expect_refused, "refused: an id with a NUL (create)", "create-node",
        json_pack("{s:s, s:{s:s%, s:s}}", "topic_name", "departments",
            "record", "id", "dx\0evil", (size_t)7, "name", "Evil"),
        "Wrong record: a string with a NUL in it")
    CHECK(expect_refused, "refused: an id with a NUL (update)", "update-node",
        json_pack("{s:s, s:{s:s%, s:s}}", "topic_name", "departments",
            "record", "id", "top\0x", (size_t)5, "name", "Top"),
        "Wrong record: a string with a NUL in it")
    CHECK(expect_refused, "refused: a NUL down the record", "update-node",
        json_pack("{s:s, s:{s:s, s:{s:[s%]}}}", "topic_name", "departments",
            "record", "id", "top", "extra", "list", "To\0p", (size_t)4),
        "Wrong record: a string with a NUL in it")
    CHECK(expect_refused, "refused: create_only of a node that exists", "update-node",
        json_pack("{s:s, s:{s:s, s:s}, s:{s:b}}", "topic_name", "departments",
            "record", "id", "top", "name", "Top", "options", "create_only", 1),
        "Node already exists: 'top'")
    CHECK(expect_refused, "refused: a link that would close a cycle", "link-nodes",
        json_pack("{s:s, s:s}", "parent_ref", "departments^dev^departments", "child_ref", "departments^top"),
        "cannot link")
    CHECK(expect_refused, "refused: a node linked to itself", "link-nodes",
        json_pack("{s:s, s:s}", "parent_ref", "departments^top^departments", "child_ref", "departments^top"),
        "cannot link")
    CHECK(expect_refused, "refused: an unlink of a child that does not hang there", "unlink-nodes",
        json_pack("{s:s, s:s}", "parent_ref", "departments^dev^departments", "child_ref", "departments^ops"),
        "cannot unlink")
    CHECK(expect_refused, "refused: an id with a path metacharacter", "create-node",
        json_pack("{s:s, s:{s:s, s:s}}", "topic_name", "departments", "record", "id", "a/b", "name", "AB"),
        "cannot create the node")
    {
        /*  Import-db refuses what the upload gets wrong, counted  */
        const char *db = "{\"departments\": [1, \"x\", {\"id\": 5}], \"nosuch\": [{\"id\": \"q\"}]}";
        gbuffer_t *gbuf = gbuffer_create(256, 256);
        gbuffer_append_string(gbuf, db);
        gbuffer_t *gbuf64 = gbuffer_binary_to_base64(gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf));
        json_t *resp = ask(gobj, "import-db", json_pack("{s:s, s:s}",
            "content64", gbuffer_cur_rd_pointer(gbuf64), "if-resource-exists", "skip"));
        json_t *data = kw_get_dict(gobj, resp, "data", 0, 0);
        if(kw_get_int(gobj, data, "failure", -1, 0) != 4 || kw_get_int(gobj, data, "added", -1, 0) != 0) {
            result += fail(gobj, "import-db: 3 wrong records and an unknown topic, 4 failures", resp);
        }
        JSON_DECREF(resp)
        GBUFFER_DECREF(gbuf64)
        GBUFFER_DECREF(gbuf)
    }
    {
        /*  A valid record with no fkey imports with nothing logged  */
        const char *db = "{\"departments\": [{\"id\": \"plain\", \"name\": \"Plain\"}]}";
        gbuffer_t *gbuf = gbuffer_create(256, 256);
        gbuffer_append_string(gbuf, db);
        gbuffer_t *gbuf64 = gbuffer_binary_to_base64(gbuffer_cur_rd_pointer(gbuf), gbuffer_leftbytes(gbuf));
        json_t *resp = ask(gobj, "import-db", json_pack("{s:s, s:s}",
            "content64", gbuffer_cur_rd_pointer(gbuf64), "if-resource-exists", "skip"));
        json_t *data = kw_get_dict(gobj, resp, "data", 0, 0);
        if(kw_get_int(gobj, resp, "result", -1, 0) != 0 ||
                kw_get_int(gobj, data, "added", -1, 0) != 1 ||
                kw_get_int(gobj, data, "failure", -1, 0) != 0) {
            result += fail(gobj, "import-db: a record with no fkey, added", resp);
        }
        JSON_DECREF(resp)
        GBUFFER_DECREF(gbuf64)
        GBUFFER_DECREF(gbuf)
    }
    {
        /*  No node_id and no filter, all the instances of the topic  */
        json_t *resp = ask(gobj, "instances", json_pack("{s:s}", "topic_name", "items"));
        if(kw_get_int(gobj, resp, "result", -1, 0) != 0 ||
                json_array_size(kw_get_list(gobj, resp, "data", 0, 0)) != 2) {
            result += fail(gobj, "instances with no node_id and no filter", resp);
        }
        JSON_DECREF(resp)
    }
    CHECK(expect_refused, "refused: instances by the primary index", "instances",
        json_pack("{s:s, s:s}", "topic_name", "items", "pkey2", "id"),
        "Not a pkey2 of topic 'items'")
    CHECK(expect_refused, "refused: instances by an unknown pkey2", "instances",
        json_pack("{s:s, s:s, s:s}", "topic_name", "items", "node_id", "item1", "pkey2", "nosuchpk"),
        "Not a pkey2 of topic 'items'")
    CHECK(expect_refused, "refused: a create without a required field", "create-node",
        json_pack("{s:s, s:{s:s}}", "topic_name", "departments", "record", "id", "noname"),
        "cannot create the node")
    {
        json_t *resp = ask(gobj, "create-node", json_pack("{s:s, s:{s:s, s:s}}",
            "topic_name", "departments", "record", "id", "probe", "name", "Probe"));
        if(kw_get_int(gobj, resp, "result", -1, 0) != 0 || treedb_peer_data()) {
            result += fail(gobj, "create-node of probe, and the window closed after it", resp);
        }
        JSON_DECREF(resp)
    }
    {
        /*  An rt list is fed outside the window too (update = append)  */
        PRIVATE_DATA *priv = gobj_priv_data(gobj);
        json_t *rt = tranger2_open_rt_mem(priv->tranger, "departments", "probe", NULL,
            rt_probe_callback, "rt_probe", "c_node_commands", NULL);
        json_t *resp = ask(gobj, "update-node", json_pack("{s:s, s:{s:s, s:s}}",
            "topic_name", "departments", "record", "id", "probe", "name", "Probe"));
        if(kw_get_int(gobj, resp, "result", -1, 0) != 0 || rt_probe_window != 0) {
            result += fail(gobj, "an rt list of the topic is fed outside the peer's window", resp);
        }
        JSON_DECREF(resp)
        tranger2_close_rt_mem(priv->tranger, rt);
    }
    {
        /*  The options of children and parent_refs may be NULL  */
        PRIVATE_DATA *priv = gobj_priv_data(gobj);
        json_t *top = treedb_get_node(priv->tranger, TREEDB, "departments", "top");
        json_t *dev = treedb_get_node(priv->tranger, TREEDB, "departments", "dev");
        json_t *children = treedb_node_children(priv->tranger, "departments", top, NULL, NULL);
        json_t *parents = treedb_parent_refs(priv->tranger, "department_id", dev, NULL);
        if(json_array_size(children) == 0 || json_array_size(parents) != 1) {
            result += fail(gobj, "children and parent_refs with no options", NULL);
        }
        JSON_DECREF(children)
        JSON_DECREF(parents)
    }

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
     *  jtree without rename_hook: the children whole in the hook itself,
     *  each once. Up to 7.25.22 the hook kept its refs and the children were
     *  appended after them: "dev ops dev ops".
     */
    {
        json_t *resp = ask(gobj, "jtree", json_pack("{s:s, s:s, s:s}",
            "topic_name", "departments", "node_id", "top", "hook", "departments"));
        result += expect_ids(gobj, "jtree: the children of top, once", resp, "departments", "dev ops");
        json_t *children = kw_get_list(gobj, kw_get_dict_value(gobj, resp, "data", 0, 0), "departments", 0, 0);
        if(strcmp(kw_get_str(gobj, json_array_get(children, 0), "__path__", "", 0), "top`dev") != 0) {
            result += fail(gobj, "jtree: a child whole, in the hook", resp);
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

    result += export_and_import(gobj);

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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  A subscriber runs OUTSIDE the peer's window. The
     *  create of `probe` comes from a command; what this subscriber writes
     *  in its handler is its own C, and its fault is an ERROR, not the
     *  WARNING of the peer's data (main.c asks for the priority).
     */
    if(event == EV_TREEDB_NODE_CREATED &&
            strcmp(kw_get_str(gobj, kw, "node`id", "", 0), "probe")==0) {
        if(treedb_peer_data()) {
            fail(gobj, "a subscriber runs inside the peer's window", kw);
        }
        json_t *node = treedb_create_node(priv->tranger, TREEDB, "departments",
            json_pack("{s:s}", "id", "probe_child"));   // no `name`: required
        if(node) {
            fail(gobj, "a create without a required field was made", NULL);
        }
    }
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
