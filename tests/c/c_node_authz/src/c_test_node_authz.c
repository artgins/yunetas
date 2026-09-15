/***********************************************************************
 *          C_TEST_NODE_AUTHZ.C
 *
 *          GClass to test the permission every C_NODE command asks for
 *
 *          C_NODE checks a permission inside each command handler, with or
 *          without the global `enable_command_authz` gate. Only `nodes` and
 *          the four node writes did: the other reads (`node`, `instances`,
 *          `parents`, `print-tranger`, `export-db`, ...) answered anyone,
 *          and so did four WRITES -- `link-nodes`, `unlink-nodes`,
 *          `import-db` and the snaps. And `update-node` with `create=1`,
 *          the way the SPAs create, created under `update` alone.
 *
 *          Verified here, with a checker that grants by user name:
 *
 *              - every read command refuses `nobody` and serves `reader`;
 *              - link/unlink and activate/deactivate-snap ask `update`,
 *                shoot-snap `create`, import-db `create` AND `update`;
 *              - update-node with create=1 asks `create` only for a node
 *                that does not exist: an `editor` saves an existing node
 *                through it, and cannot create one.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_node_authz.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TREEDB_NAME "treedb_authz_test"

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
 *  Schema for the test treedb: one topic that hangs from itself, so there
 *  is something to link
 ***************************************************************************/
PRIVATE char schema_authz_test[] = "\
{                                                                   \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'topic_name': 'items',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
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
                    'flag': ['persistent','writable']               \n\
                },                                                  \n\
                'children': {                                       \n\
                    'header': 'Children',                           \n\
                    'fillspace': 10,                                \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'items': 'parent_id'                        \n\
                    }                                               \n\
                },                                                  \n\
                'parent_id': {                                      \n\
                    'header': 'Parent',                             \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************************
 *              Framework Method Create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), path_root, "c_node_authz", NULL);
    rmrdir(path_database);

    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", "c_node_authz",
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    priv->tranger = tranger2_startup(0, jn_tranger, 0);

    helper_quote2doublequote(schema_authz_test);
    json_t *jn_schema = legalstring2json(schema_authz_test, TRUE);

    json_t *kw_resource = json_pack("{s:I, s:s, s:o, s:i}",
        "tranger", (json_int_t)(uintptr_t)priv->tranger,
        "treedb_name", TREEDB_NAME,
        "treedb_schema", jn_schema,
        "exit_on_error", LOG_OPT_TRACE_STACK
    );

    priv->gobj_node = gobj_create_pure_child(
        "test_node",
        C_NODE,
        kw_resource,
        gobj
    );

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

/***************************************************************************
 *              Framework Method Start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->gobj_node);
    gobj_start(priv->timer);

    return 0;
}

/***************************************************************************
 *              Framework Method Stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    gobj_stop(priv->gobj_node);

    return 0;
}

/***************************************************************************
 *              Framework Method Destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->tranger) {
        // NOTE: treedb is already closed by C_NODE's mt_destroy
        tranger2_shutdown(priv->tranger);
        priv->tranger = NULL;
    }
}

/***************************************************************************
 *              Framework Method Play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    set_timeout(priv->timer, 100);

    return 0;
}

/***************************************************************************
 *  Run `command` on the C_NODE as `user`, and return its result code.
 *  `kw` is owned.
 ***************************************************************************/
PRIVATE int ask(hgobj gobj, const char *user, const char *command, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_object_set_new(kw, "__username__", json_string(user));
    json_t *resp = gobj_command(priv->gobj_node, command, kw, gobj);
    int ret = (int)kw_get_int(gobj, resp, "result", -1, 0);
    JSON_DECREF(resp)
    return ret;
}

/***************************************************************************
 *  `user` must be refused `command` with -403 (refused) or must get past
 *  the permission (whatever the command then answers).
 ***************************************************************************/
PRIVATE int expect(
    hgobj gobj,
    const char *user,
    const char *command,
    json_t *kw,     // owned
    BOOL refused
)
{
    int ret = ask(gobj, user, command, kw);
    if(refused? (ret != -403): (ret == -403)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", refused?
                "TEST FAIL: a user WITHOUT the permission got past it":
                "TEST FAIL: a user WITH the permission was refused",
            "user",         "%s", user,
            "command",      "%s", command,
            "result",       "%d", ret,
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Run all tests -- called from the timer callback inside the event loop
 ***************************************************************************/
PRIVATE int run_tests(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int result = 0;

    /*-----------------------------------------------*
     *  Fixture: two nodes, straight to the treedb
     *-----------------------------------------------*/
    treedb_create_node(priv->tranger, TREEDB_NAME, "items",
        json_pack("{s:s, s:s}", "id", "item00", "name", "Item 00"));
    treedb_create_node(priv->tranger, TREEDB_NAME, "items",
        json_pack("{s:s, s:s}", "id", "item01", "name", "Item 01"));

    /*-----------------------------------------------*
     *  Every read: nobody is refused, reader is served
     *-----------------------------------------------*/
    struct {
        const char *command;
        const char *kw;
    } reads[] = {
        {"nodes",           "{'topic_name':'items'}"},
        {"node",            "{'topic_name':'items', 'node_id':'item00'}"},
        {"instances",       "{'topic_name':'items', 'node_id':'item00'}"},
        {"pkey2s",          "{'topic_name':'items'}"},
        {"parents",         "{'topic_name':'items', 'node_id':'item01'}"},
        {"children",        "{'topic_name':'items', 'node_id':'item00', 'hook':'children', 'options':{}}"},
        {"jtree",           "{'topic_name':'items', 'hook':'children', 'options':{}}"},
        {"hooks",           "{'topic_name':'items'}"},
        {"links",           "{'topic_name':'items'}"},
        {"snaps",           "{}"},
        {"print-tranger",   "{'path':'treedbs'}"},
        {"treedbs",         "{}"},
        {"treedb-info",     "{}"},
        {"topics",          "{}"},
        {"desc",            "{'topic_name':'items'}"},
        {"descs",           "{}"},
        {0, 0}
    };
    for(int i=0; reads[i].command; i++) {
        char kw_[256];
        snprintf(kw_, sizeof(kw_), "%s", reads[i].kw);
        helper_quote2doublequote(kw_);
        result += expect(gobj, "nobody", reads[i].command, legalstring2json(kw_, TRUE), TRUE);
        result += expect(gobj, "reader", reads[i].command, legalstring2json(kw_, TRUE), FALSE);
    }

    /*
     *  export-db writes a file: only its refusal is asked here
     */
    result += expect(gobj, "nobody", "export-db", json_object(), TRUE);

    /*-----------------------------------------------*
     *  link / unlink: an update of the child's fkey
     *-----------------------------------------------*/
    json_t *kw_link = json_pack("{s:s, s:s}",
        "parent_ref", "items^item00^children",
        "child_ref", "items^item01"
    );
    result += expect(gobj, "reader", "link-nodes", json_deep_copy(kw_link), TRUE);
    result += expect(gobj, "editor", "link-nodes", json_deep_copy(kw_link), FALSE);
    result += expect(gobj, "reader", "unlink-nodes", json_deep_copy(kw_link), TRUE);
    result += expect(gobj, "editor", "unlink-nodes", json_deep_copy(kw_link), FALSE);
    JSON_DECREF(kw_link)

    /*-----------------------------------------------*
     *  Snaps: shoot creates, activate/deactivate update
     *-----------------------------------------------*/
    result += expect(gobj, "editor", "shoot-snap", json_pack("{s:s}", "name", "s1"), TRUE);
    result += expect(gobj, "creator", "shoot-snap", json_pack("{s:s}", "name", "s1"), FALSE);
    result += expect(gobj, "nobody", "snap-content", json_pack("{s:s}", "name", "s1"), TRUE);
    result += expect(gobj, "reader", "snap-content", json_pack("{s:s}", "name", "s1"), FALSE);
    result += expect(gobj, "reader", "activate-snap", json_pack("{s:s}", "name", "s1"), TRUE);
    result += expect(gobj, "editor", "activate-snap", json_pack("{s:s}", "name", "s1"), FALSE);
    result += expect(gobj, "reader", "deactivate-snap", json_object(), TRUE);
    result += expect(gobj, "editor", "deactivate-snap", json_object(), FALSE);

    /*-----------------------------------------------*
     *  import-db creates AND overwrites
     *-----------------------------------------------*/
    result += expect(gobj, "editor", "import-db", json_object(), TRUE);

    /*-----------------------------------------------*
     *  update-node with create=1: `create` only for
     *  a node that does not exist
     *-----------------------------------------------*/
    result += expect(gobj, "editor", "update-node",
        json_pack("{s:s, s:{s:s, s:s}, s:{s:b}}",
            "topic_name", "items",
            "record", "id", "item00", "name", "renamed by an editor",
            "options", "create", 1
        ),
        FALSE
    );
    result += expect(gobj, "editor", "update-node",
        json_pack("{s:s, s:{s:s, s:s}, s:{s:b}}",
            "topic_name", "items",
            "record", "id", "item-new", "name", "created by an editor",
            "options", "create", 1
        ),
        TRUE
    );
    if(treedb_get_node(priv->tranger, TREEDB_NAME, "items", "item-new")) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: an editor created a node through update-node",
            NULL
        );
        result += -1;
    }
    result += expect(gobj, "creator", "update-node",
        json_pack("{s:s, s:{s:s, s:s}, s:{s:b}}",
            "topic_name", "items",
            "record", "id", "item-new", "name", "created by a creator",
            "options", "create", 1
        ),
        FALSE
    );
    if(!treedb_get_node(priv->tranger, TREEDB_NAME, "items", "item-new")) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "TEST FAIL: a creator could not create through update-node",
            NULL
        );
        result += -1;
    }

    if(result == 0) {
        gobj_log_info(gobj, 0,
            "msgset", "%s", MSGSET_INFO,
            "msg", "%s", "All c_node authz tests PASSED",
            NULL
        );
    }

    return result;
}

/***************************************************************************
 *  The treedb wrote a node: the parent of a C_NODE has to accept it
 ***************************************************************************/
PRIVATE int ac_node_written(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_TIMEOUT -- runs test logic inside the event loop, then exits
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
 *  EV_STOPPED
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

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

/***************************************************************************
 *              GClass
 ***************************************************************************/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_destroy = mt_destroy,
};

GOBJ_DEFINE_GCLASS(C_TEST_NODE_AUTHZ);

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
        /*  The C_NODE below us publishes these to its parent whenever the
         *  treedb is written. */
        {EV_TREEDB_NODE_CREATED,    ac_node_written,    0},
        {EV_TREEDB_NODE_UPDATED,    ac_node_written,    0},
        {EV_TREEDB_NODE_DELETED,    ac_node_written,    0},
        {EV_TREEDB_NODE_LINKED,     ac_node_written,    0},
        {EV_TREEDB_NODE_UNLINKED,   ac_node_written,    0},
        {EV_STOPPED,                ac_stopped,         0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_TREEDB_NODE_CREATED,    0},
        {EV_TREEDB_NODE_UPDATED,    0},
        {EV_TREEDB_NODE_DELETED,    0},
        {EV_TREEDB_NODE_LINKED,     0},
        {EV_TREEDB_NODE_UNLINKED,   0},
        {EV_STOPPED,                0},
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
        0   // gcflag_t
    );
    return __gclass__ ? 0 : -1;
}

/***************************************************************************
 *              Registration
 ***************************************************************************/
PUBLIC int register_c_test_node_authz(void)
{
    return create_gclass(C_TEST_NODE_AUTHZ);
}
