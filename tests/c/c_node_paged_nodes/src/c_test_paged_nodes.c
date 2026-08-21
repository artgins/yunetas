/***********************************************************************
 *          C_TEST_PAGED_NODES.C
 *
 *          GClass to test the paging of C_NODE's `nodes` command
 *
 *          A treedb lives in memory, so walking it is not what costs:
 *          serializing every node and pushing it through a websocket is.
 *          `nodes` therefore cuts the answer on the way out, with the same
 *          contract `list-keys` of C_TRANGER uses — no `limit` gives the
 *          plain list it always gave, and a `limit` gives the envelope
 *          {total_rows, pages, data}.
 *
 *          What is verified here: the plain list is unchanged, a page holds
 *          the right slice, `from` is 1-based, the last page is short, a
 *          page past the end is empty without lying about the total, and
 *          both parameters survive arriving as STRINGS — which is how they
 *          arrive through the agent's command-yuno forwarding, which does
 *          not coerce.
 *
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_paged_nodes.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

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
 *  Schema for the test treedb: one topic, nothing else needed
 ***************************************************************************/
PRIVATE char schema_paged_test[] = "\
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
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

#define TOTAL_ITEMS 10

/***************************************************************************
 *              Framework Method Create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Prepare paths: a database of its own, wiped on every run, so the test
     *  starts from the same ten nodes it asserts about.
     */
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), path_root, "c_node_paged_nodes", NULL);
    rmrdir(path_database);

    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", "c_node_paged_nodes",
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    priv->tranger = tranger2_startup(0, jn_tranger, 0);

    helper_quote2doublequote(schema_paged_test);
    json_t *jn_schema = legalstring2json(schema_paged_test, TRUE);

    json_t *kw_resource = json_pack("{s:I, s:s, s:o, s:i}",
        "tranger", (json_int_t)(uintptr_t)priv->tranger,
        "treedb_name", "treedb_paged_test",
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

PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Fire a one-shot timer to run tests inside the event loop
     */
    set_timeout(priv->timer, 100);

    return 0;
}

/***************************************************************************
 *  Ask the C_NODE for a page of `items`.
 *  `from` / `limit` are given as STRINGS when `as_string` is set: that is
 *  how they arrive through the agent's command-yuno forwarding, which does
 *  not coerce, and KW_WILD_NUMBER is what has to survive it.
 ***************************************************************************/
PRIVATE json_t *ask_nodes(hgobj gobj, int from, int limit, BOOL as_string)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw = json_pack("{s:s}", "topic_name", "items");
    if(limit > 0 || from > 0) {
        if(as_string) {
            char s_from[32];
            char s_limit[32];
            snprintf(s_from, sizeof(s_from), "%d", from);
            snprintf(s_limit, sizeof(s_limit), "%d", limit);
            json_object_set_new(kw, "from", json_string(s_from));
            json_object_set_new(kw, "limit", json_string(s_limit));
        } else {
            json_object_set_new(kw, "from", json_integer(from));
            json_object_set_new(kw, "limit", json_integer(limit));
        }
    }

    return gobj_command(priv->gobj_node, "nodes", kw, gobj);
}

/***************************************************************************
 *  The `data` of a command response, whatever shape it came in.
 ***************************************************************************/
PRIVATE json_t *resp_data(json_t *resp)
{
    return json_object_get(resp, "data");
}

/***************************************************************************
 *  Run all tests — called from the timer callback inside the event loop
 ***************************************************************************/
PRIVATE int run_tests(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int result = 0;
    const char *treedb_name = "treedb_paged_test";

    /*-----------------------------------------------*
     *  Fixture: TOTAL_ITEMS nodes
     *-----------------------------------------------*/
    for(int i = 0; i < TOTAL_ITEMS; i++) {
        char id[32];
        char name[32];
        snprintf(id, sizeof(id), "item%02d", i);
        snprintf(name, sizeof(name), "Item %02d", i);
        json_t *node = treedb_create_node(
            priv->tranger, treedb_name, "items",
            json_pack("{s:s, s:s}", "id", id, "name", name)
        );
        if(!node) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset", "%s", MSGSET_INTERNAL,
                "msg", "%s", "TEST FAIL: cannot create the fixture node",
                "id", "%s", id,
                NULL
            );
            result += -1;
        }
    }

    /*-----------------------------------------------*
     *  Test 1: no limit -> the plain list, unchanged
     *-----------------------------------------------*/
    json_t *resp = ask_nodes(gobj, 0, 0, FALSE);
    json_t *data = resp_data(resp);
    if(!json_is_array(data) || json_array_size(data) != TOTAL_ITEMS) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: no limit must answer the plain list",
            "size", "%d", (int)json_array_size(data),
            NULL
        );
        result += -1;
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  Test 2: a limit -> the envelope, and the FIRST page
     *-----------------------------------------------*/
    resp = ask_nodes(gobj, 1, 3, FALSE);
    data = resp_data(resp);
    if(!json_is_object(data)) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: a limit must answer the envelope",
            NULL
        );
        result += -1;
    } else {
        json_int_t total = json_integer_value(json_object_get(data, "total_rows"));
        json_int_t pages = json_integer_value(json_object_get(data, "pages"));
        json_t *rows = json_object_get(data, "data");
        const char *first = json_string_value(
            json_object_get(json_array_get(rows, 0), "id")
        );
        if(total != TOTAL_ITEMS || pages != 4 ||
            json_array_size(rows) != 3 || !first || strcmp(first, "item00") != 0
        ) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset", "%s", MSGSET_INTERNAL,
                "msg", "%s", "TEST FAIL: wrong first page",
                "total", "%d", (int)total,
                "pages", "%d", (int)pages,
                "rows", "%d", (int)json_array_size(rows),
                "first", "%s", first?first:"(none)",
                NULL
            );
            result += -1;
        }
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  Test 3: `from` is 1-BASED
     *-----------------------------------------------*/
    resp = ask_nodes(gobj, 4, 3, FALSE);
    data = resp_data(resp);
    {
        json_t *rows = json_object_get(data, "data");
        const char *first = json_string_value(
            json_object_get(json_array_get(rows, 0), "id")
        );
        if(json_array_size(rows) != 3 || !first || strcmp(first, "item03") != 0) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset", "%s", MSGSET_INTERNAL,
                "msg", "%s", "TEST FAIL: from is not 1-based",
                "rows", "%d", (int)json_array_size(rows),
                "first", "%s", first?first:"(none)",
                NULL
            );
            result += -1;
        }
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  Test 4: the LAST page is short, and a page past
     *  the end is empty without lying about the total
     *-----------------------------------------------*/
    resp = ask_nodes(gobj, 10, 3, FALSE);
    data = resp_data(resp);
    if(json_array_size(json_object_get(data, "data")) != 1) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: the last page must hold what is left",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(resp)

    resp = ask_nodes(gobj, 100, 3, FALSE);
    data = resp_data(resp);
    if(json_array_size(json_object_get(data, "data")) != 0 ||
        json_integer_value(json_object_get(data, "total_rows")) != TOTAL_ITEMS
    ) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: a page past the end must be empty, total intact",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(resp)

    /*-----------------------------------------------*
     *  Test 5: from/limit arriving as STRINGS
     *-----------------------------------------------*/
    resp = ask_nodes(gobj, 4, 3, TRUE);
    data = resp_data(resp);
    {
        json_t *rows = json_object_get(data, "data");
        const char *first = json_string_value(
            json_object_get(json_array_get(rows, 0), "id")
        );
        if(json_array_size(rows) != 3 || !first || strcmp(first, "item03") != 0) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset", "%s", MSGSET_INTERNAL,
                "msg", "%s", "TEST FAIL: from/limit must survive arriving as strings",
                "rows", "%d", (int)json_array_size(rows),
                "first", "%s", first?first:"(none)",
                NULL
            );
            result += -1;
        }
    }
    JSON_DECREF(resp)

    if(result == 0) {
        gobj_log_info(gobj, 0,
            "msgset", "%s", MSGSET_INFO,
            "msg", "%s", "All c_node paged nodes tests PASSED",
            NULL
        );
    }

    return result;
}

/***************************************************************************
 *  The treedb wrote a node. Nothing to do here — this test asks about the
 *  SHAPE of the `nodes` answer, not about the write notifications — but the
 *  parent of a C_NODE has to accept them.
 ***************************************************************************/
PRIVATE int ac_node_written(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_TIMEOUT — runs test logic inside the event loop, then exits
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

GOBJ_DEFINE_GCLASS(C_TEST_PAGED_NODES);

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
         *  fixture is written. The test has nothing to do with them, but a
         *  CHILD's published event must be declared by its parent or the
         *  framework logs "Event NOT DEFINED in state" — correctly. */
        {EV_TREEDB_NODE_CREATED,    ac_node_written,    0},
        {EV_TREEDB_NODE_UPDATED,    ac_node_written,    0},
        {EV_TREEDB_NODE_DELETED,    ac_node_written,    0},
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
PUBLIC int register_c_test_paged_nodes(void)
{
    return create_gclass(C_TEST_PAGED_NODES);
}
