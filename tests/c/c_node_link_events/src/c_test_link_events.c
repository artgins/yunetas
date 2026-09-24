/***********************************************************************
 *          C_TEST_LINK_EVENTS.C
 *
 *          GClass to test EV_TREEDB_NODE_LINKED/UNLINKED at c_node level
 *
 *          Verifies that when C_NODE's `with_link_events` attribute is set,
 *          link/unlink operations publish EV_TREEDB_NODE_LINKED and
 *          EV_TREEDB_NODE_UNLINKED events through the GObj event system,
 *          and that an update-node with autolink moves only the links
 *          that change and saves the record even when a link fails.
 *
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_link_events.h"

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

    int linked_count;
    int unlinked_count;
    int created_count;
    int updated_count;
    int deleted_count;

    char last_linked_hook[64];
    char last_linked_parent_topic[64];
    char last_linked_child_topic[64];
    char last_linked_parent_id[64];
    char last_linked_child_id[64];

    char last_unlinked_hook[64];
    char last_unlinked_parent_id[64];
    char last_unlinked_child_id[64];
} PRIVATE_DATA;

/***************************************************************************
 *  Schema for test treedb — minimal: departments + users
 ***************************************************************************/
PRIVATE char schema_link_test[] = "\
{                                                                   \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'topic_name': 'users',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
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
            'topic_name': 'departments',                            \n\
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
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'department_id': {                                  \n\
                    'header': 'Top Department',                     \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'departments': {                                    \n\
                    'header': 'Departments',                        \n\
                    'fillspace': 20,                                \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'departments': 'department_id'              \n\
                    }                                               \n\
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

/***************************************************************************
 *              Framework Methods
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

    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), path_root, "c_node_link_events", NULL);
    rmrdir(path_database);

    /*
     *  Start tranger
     */
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", "c_node_link_events",
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    priv->tranger = tranger2_startup(0, jn_tranger, 0);

    /*
     *  Create C_NODE child with with_link_events=true
     */
    helper_quote2doublequote(schema_link_test);
    json_t *jn_schema = legalstring2json(schema_link_test, TRUE);

    json_t *kw_resource = json_pack("{s:I, s:s, s:o, s:i, s:b}",
        "tranger", (json_int_t)(uintptr_t)priv->tranger,
        "treedb_name", "treedb_link_test",
        "treedb_schema", jn_schema,
        "exit_on_error", LOG_OPT_TRACE_STACK,
        "with_link_events", 1
    );

    priv->gobj_node = gobj_create_pure_child(
        "test_node",
        C_NODE,
        kw_resource,
        gobj
    );

    /*
     *  Create a timer to trigger tests from within the event loop
     */
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->gobj_node);
    gobj_start(priv->timer);

    return 0;
}

PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->gobj_node);

    return 0;
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
 *  Reset the event counters
 ***************************************************************************/
PRIVATE void reset_counters(PRIVATE_DATA *priv)
{
    priv->linked_count = 0;
    priv->unlinked_count = 0;
    priv->created_count = 0;
    priv->updated_count = 0;
    priv->deleted_count = 0;
}

/***************************************************************************
 *  Log a failure when an event counter is not what the test expects
 ***************************************************************************/
PRIVATE int expect_count(
    hgobj gobj,
    const char *test,
    const char *counter,
    int expected,
    int got
)
{
    if(got == expected) {
        return 0;
    }
    gobj_log_error(gobj, 0,
        "function", "%s", __FUNCTION__,
        "msgset", "%s", MSGSET_INTERNAL,
        "msg", "%s", "TEST FAIL: wrong event count",
        "test", "%s", test,
        "counter", "%s", counter,
        "expected", "%d", expected,
        "got", "%d", got,
        NULL
    );
    return -1;
}

/***************************************************************************
 *  update-node of alice with autolink, the way a client sends it
 ***************************************************************************/
PRIVATE json_t *update_alice_with_autolink( // Return is YOURS
    hgobj gobj,
    const char *username,
    json_t *departments     // owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return gobj_update_node(
        priv->gobj_node,
        "users",
        json_pack("{s:s, s:s, s:o}",
            "id", "alice",
            "username", username,
            "departments", departments
        ),
        json_pack("{s:b}", "autolink", 1),
        gobj
    );
}

/***************************************************************************
 *  The record is saved ONCE and the link that did not change is still there
 ***************************************************************************/
PRIVATE int expect_alice_saved(
    hgobj gobj,
    const char *test,
    json_t *alice,          // NOT owned, pure node
    const char *username,
    const char *kept_ref,
    json_int_t g_rowid
)
{
    int ret = 0;

    if(strcmp(kw_get_str(gobj, alice, "username", "", 0), username) != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: the record was not updated",
            "test", "%s", test,
            "expected", "%s", username,
            "got", "%s", kw_get_str(gobj, alice, "username", "", 0),
            NULL
        );
        ret += -1;
    }
    if(kw_get_int(gobj, alice, "__md_treedb__`g_rowid", 0, 0) != g_rowid) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: the record was not saved once",
            "test", "%s", test,
            "expected", "%d", (int)g_rowid,
            "got", "%d", (int)kw_get_int(gobj, alice, "__md_treedb__`g_rowid", 0, 0),
            NULL
        );
        ret += -1;
    }
    if(!json_str_in_list(gobj, kw_get_dict_value(gobj, alice, "departments", 0, 0), kept_ref, FALSE)) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: a link that did not change was lost",
            "test", "%s", test,
            "ref", "%s", kept_ref,
            NULL
        );
        ret += -1;
    }

    return ret;
}

/***************************************************************************
 *  Run all tests — called from timer callback inside the event loop
 ***************************************************************************/
PRIVATE int run_tests(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int result = 0;
    const char *treedb_name = "treedb_link_test";

    /*
     *  C_NODE is started, treedb is open, callback is set with LINK_EVENTS.
     *  Events are published via gobj_publish_event → arrive at parent (us).
     */

    /*-----------------------------------------------*
     *  Test 1: Create nodes (no link events yet)
     *-----------------------------------------------*/
    json_t *dept1 = treedb_create_node(
        priv->tranger, treedb_name, "departments",
        json_pack("{s:s, s:s}", "id", "engineering", "name", "Engineering")
    );
    json_t *dept2 = treedb_create_node(
        priv->tranger, treedb_name, "departments",
        json_pack("{s:s, s:s}", "id", "research", "name", "Research")
    );
    json_t *user1 = treedb_create_node(
        priv->tranger, treedb_name, "users",
        json_pack("{s:s, s:s}", "id", "alice", "username", "alice_w")
    );

    /*
     *  Reset counters after creates (creates fire EV_TREEDB_NODE_CREATED)
     */
    priv->linked_count = 0;
    priv->unlinked_count = 0;
    priv->created_count = 0;
    priv->updated_count = 0;
    priv->deleted_count = 0;

    /*-----------------------------------------------*
     *  Test 2: Link dept1 -> departments -> dept2
     *  Should fire EV_TREEDB_NODE_LINKED
     *-----------------------------------------------*/
    treedb_link_nodes(priv->tranger, "departments", dept1, dept2);

    if(priv->linked_count != 1) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: link dept->dept expected 1 linked event",
            "got", "%d", priv->linked_count,
            NULL
        );
        result += -1;
    }
    if(strcmp(priv->last_linked_hook, "departments") != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: wrong hook_name in linked event",
            "expected", "%s", "departments",
            "got", "%s", priv->last_linked_hook,
            NULL
        );
        result += -1;
    }
    if(strcmp(priv->last_linked_parent_id, "engineering") != 0 ||
       strcmp(priv->last_linked_child_id, "research") != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: wrong parent_id/child_id in linked event",
            "parent_id", "%s", priv->last_linked_parent_id,
            "child_id", "%s", priv->last_linked_child_id,
            NULL
        );
        result += -1;
    }

    /*-----------------------------------------------*
     *  Test 3: Unlink dept1 -/-> dept2
     *  Should fire EV_TREEDB_NODE_UNLINKED
     *-----------------------------------------------*/
    priv->linked_count = 0;
    priv->unlinked_count = 0;

    treedb_unlink_nodes(priv->tranger, "departments", dept1, dept2);

    if(priv->unlinked_count != 1) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: unlink expected 1 unlinked event",
            "got", "%d", priv->unlinked_count,
            NULL
        );
        result += -1;
    }
    if(strcmp(priv->last_unlinked_parent_id, "engineering") != 0 ||
       strcmp(priv->last_unlinked_child_id, "research") != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: wrong ids in unlinked event",
            "parent_id", "%s", priv->last_unlinked_parent_id,
            "child_id", "%s", priv->last_unlinked_child_id,
            NULL
        );
        result += -1;
    }

    /*-----------------------------------------------*
     *  Test 4: Cross-topic link (dept -> users -> user)
     *-----------------------------------------------*/
    priv->linked_count = 0;
    priv->unlinked_count = 0;

    treedb_link_nodes(priv->tranger, "users", dept1, user1);

    if(priv->linked_count != 1) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: cross-topic link expected 1 linked event",
            "got", "%d", priv->linked_count,
            NULL
        );
        result += -1;
    }
    if(strcmp(priv->last_linked_parent_topic, "departments") != 0 ||
       strcmp(priv->last_linked_child_topic, "users") != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: wrong topic names in cross-topic link event",
            "parent_topic", "%s", priv->last_linked_parent_topic,
            "child_topic", "%s", priv->last_linked_child_topic,
            NULL
        );
        result += -1;
    }

    /*-----------------------------------------------*
     *  Test 5: Link events replace UPDATED for the
     *  _link_nodes callback, but treedb_save_node
     *  still fires UPDATED for the saved child node.
     *  Verify that LINKED events are used instead of
     *  UPDATED for the link operation itself.
     *-----------------------------------------------*/
    /* updated_count > 0 is expected from save_node callbacks */

    /*-----------------------------------------------*
     *  Test 6: update-node with autolink repeating
     *  the links the node has: no link event at all,
     *  only the UPDATED of the one save
     *-----------------------------------------------*/
    json_int_t g_rowid = kw_get_int(gobj, user1, "__md_treedb__`g_rowid", 0, 0);
    reset_counters(priv);

    json_t *jn_node = update_alice_with_autolink(gobj, "alice_b",
        json_pack("[s]", "departments^engineering^users")
    );
    if(!jn_node) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: update-node with the same links answered NULL",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(jn_node)

    result += expect_count(gobj, "same links", "linked", 0, priv->linked_count);
    result += expect_count(gobj, "same links", "unlinked", 0, priv->unlinked_count);
    result += expect_count(gobj, "same links", "updated", 1, priv->updated_count);
    result += expect_alice_saved(gobj, "same links", user1,
        "alice_b", "departments^engineering^users", g_rowid + 1
    );

    /*-----------------------------------------------*
     *  Test 7: update-node with autolink moving the
     *  link: ONE unlink and ONE link
     *-----------------------------------------------*/
    g_rowid = kw_get_int(gobj, user1, "__md_treedb__`g_rowid", 0, 0);
    reset_counters(priv);

    jn_node = update_alice_with_autolink(gobj, "alice_c",
        json_pack("[s]", "departments^research^users")
    );
    JSON_DECREF(jn_node)

    result += expect_count(gobj, "moved link", "linked", 1, priv->linked_count);
    result += expect_count(gobj, "moved link", "unlinked", 1, priv->unlinked_count);
    result += expect_count(gobj, "moved link", "updated", 1, priv->updated_count);
    if(strcmp(priv->last_unlinked_parent_id, "engineering") != 0 ||
       strcmp(priv->last_linked_parent_id, "research") != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: moved link, wrong parents in the events",
            "unlinked", "%s", priv->last_unlinked_parent_id,
            "linked", "%s", priv->last_linked_parent_id,
            NULL
        );
        result += -1;
    }
    result += expect_alice_saved(gobj, "moved link", user1,
        "alice_c", "departments^research^users", g_rowid + 1
    );

    /*-----------------------------------------------*
     *  Test 8: a link that cannot be made (its parent
     *  does not exist) does not cost the record its
     *  save, nor the links that did not change
     *-----------------------------------------------*/
    g_rowid = kw_get_int(gobj, user1, "__md_treedb__`g_rowid", 0, 0);
    reset_counters(priv);

    jn_node = update_alice_with_autolink(gobj, "alice_d",
        json_pack("[s, s]", "departments^research^users", "departments^ghost^users")
    );
    if(!jn_node) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: update-node with a missing parent answered NULL",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(jn_node)

    result += expect_count(gobj, "missing parent", "linked", 0, priv->linked_count);
    result += expect_count(gobj, "missing parent", "unlinked", 0, priv->unlinked_count);
    result += expect_alice_saved(gobj, "missing parent", user1,
        "alice_d", "departments^research^users", g_rowid + 1
    );

    /*-----------------------------------------------*
     *  Test 9: a ref whose hook links into ANOTHER
     *  column is refused, and the record is saved
     *-----------------------------------------------*/
    g_rowid = kw_get_int(gobj, user1, "__md_treedb__`g_rowid", 0, 0);
    reset_counters(priv);

    jn_node = update_alice_with_autolink(gobj, "alice_e",
        json_pack("[s, s]", "departments^research^users", "departments^engineering^departments")
    );
    JSON_DECREF(jn_node)

    result += expect_count(gobj, "wrong column", "linked", 0, priv->linked_count);
    result += expect_count(gobj, "wrong column", "unlinked", 0, priv->unlinked_count);
    result += expect_alice_saved(gobj, "wrong column", user1,
        "alice_e", "departments^research^users", g_rowid + 1
    );

    /*-----------------------------------------------*
     *  Test 8b: the ONLY parent replaced by one that
     *  cannot be linked (M1 of the 2026-09-21 review).
     *  The old link was undone first and the new one
     *  failed after: alice ended orphaned on disk,
     *  UNLINKED published, no LINKED. A column is now
     *  replaced whole or not at all.
     *-----------------------------------------------*/
    g_rowid = kw_get_int(gobj, user1, "__md_treedb__`g_rowid", 0, 0);
    reset_counters(priv);

    jn_node = update_alice_with_autolink(gobj, "alice_f",
        json_pack("[s]", "departments^ghost^users")
    );
    JSON_DECREF(jn_node)

    result += expect_count(gobj, "refused parent", "linked", 0, priv->linked_count);
    result += expect_count(gobj, "refused parent", "unlinked", 0, priv->unlinked_count);
    result += expect_alice_saved(gobj, "refused parent", user1,
        "alice_f", "departments^research^users", g_rowid + 1
    );

    /*-----------------------------------------------*
     *  Test 8c: the same through the COMMAND, which
     *  answered "Node update!" -- the record is saved,
     *  the links are not what was asked: -1
     *-----------------------------------------------*/
    {
        json_t *jn_answer = gobj_command(priv->gobj_node, "update-node",
            json_pack("{s:s, s:{s:s, s:s, s:[s]}, s:{s:b}}",
                "topic_name", "users",
                "record",
                    "id", "alice",
                    "username", "alice_g",
                    "departments", "departments^ghost^users",
                "options",
                    "autolink", 1
            ),
            gobj
        );
        if(kw_get_int(gobj, jn_answer, "result", 0, 0) >= 0) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset", "%s", MSGSET_INTERNAL,
                "msg", "%s", "TEST FAIL: update-node with a refused link answered success",
                NULL
            );
            result += -1;
        }
        JSON_DECREF(jn_answer)
    }

    /*-----------------------------------------------*
     *  Test 8d: `create_only` refuses an id that
     *  exists (M28 of the 2026-09-21 review). +New
     *  with a taken id was an update: the record
     *  overwritten and, with autolink, unlinked.
     *-----------------------------------------------*/
    {
        g_rowid = kw_get_int(gobj, user1, "__md_treedb__`g_rowid", 0, 0);
        reset_counters(priv);
        json_t *jn_answer = gobj_command(priv->gobj_node, "update-node",
            json_pack("{s:s, s:{s:s, s:s, s:[]}, s:{s:b, s:b}}",
                "topic_name", "users",
                "record",
                    "id", "alice",
                    "username", "someone_else",
                    "departments",
                "options",
                    "create_only", 1,
                    "autolink", 1
            ),
            gobj
        );
        if(kw_get_int(gobj, jn_answer, "result", 0, 0) >= 0) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset", "%s", MSGSET_INTERNAL,
                "msg", "%s", "TEST FAIL: create_only on an existing id answered success",
                NULL
            );
            result += -1;
        }
        JSON_DECREF(jn_answer)
        result += expect_count(gobj, "create_only", "unlinked", 0, priv->unlinked_count);
        result += expect_alice_saved(gobj, "create_only", user1,
            "alice_g", "departments^research^users", g_rowid
        );
    }

    /*-----------------------------------------------*
     *  Test 10: set-link-events switches the events
     *  of a link at run time: off, a link publishes
     *  the parent's UPDATED; on again, the unlink
     *  publishes UNLINKED
     *-----------------------------------------------*/
    json_t *jn_resp = gobj_command(priv->gobj_node, "set-link-events",
        json_pack("{s:s}", "set", "0"),
        gobj
    );
    if(kw_get_bool(gobj, jn_resp, "data`with_link_events", 1, 0)) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: set-link-events set=0 did not answer off",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(jn_resp)
    reset_counters(priv);

    treedb_link_nodes(priv->tranger, "users", dept1, user1);

    result += expect_count(gobj, "link events off", "linked", 0, priv->linked_count);
    result += expect_count(gobj, "link events off", "updated", 2, priv->updated_count);

    jn_resp = gobj_command(priv->gobj_node, "set-link-events",
        json_pack("{s:s}", "set", "1"),
        gobj
    );
    if(!kw_get_bool(gobj, jn_resp, "data`with_link_events", 0, 0)) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: set-link-events set=1 did not answer on",
            NULL
        );
        result += -1;
    }
    JSON_DECREF(jn_resp)
    reset_counters(priv);

    treedb_unlink_nodes(priv->tranger, "users", dept1, user1);

    result += expect_count(gobj, "link events on again", "unlinked", 1, priv->unlinked_count);
    result += expect_count(gobj, "link events on again", "updated", 1, priv->updated_count);

    /*-----------------------------------------------*
     *  Test 11: `links` / `hooks` with no topic answer
     *  one key per topic. The loop keyed every topic
     *  by the EMPTY topic_name it was asked for, so
     *  the answer was {"": <the last topic's>}.
     *-----------------------------------------------*/
    {
        const char *what[] = {"links", "hooks"};
        for(int i = 0; i < 2; i++) {
            json_t *jn_all = (i == 0)?
                gobj_topic_links(priv->gobj_node, treedb_name, "", 0, gobj):
                gobj_topic_hooks(priv->gobj_node, treedb_name, "", 0, gobj);
            if(!json_object_get(jn_all, "users") ||
                    !json_object_get(jn_all, "departments") ||
                    json_object_get(jn_all, "")) {
                gobj_log_error(gobj, 0,
                    "function", "%s", __FUNCTION__,
                    "msgset", "%s", MSGSET_INTERNAL,
                    "msg", "%s", "TEST FAIL: all-topics answer not keyed by topic",
                    "what", "%s", what[i],
                    "got", "%j", jn_all,
                    NULL
                );
                result += -1;
            }
            JSON_DECREF(jn_all)
        }
    }

    /*-----------------------------------------------*
     *  Test 12: delete_node on a topic that does not
     *  exist is a refusal (-1), like every other one.
     *  It answered 0, which callers read as "deleted".
     *-----------------------------------------------*/
    if(gobj_delete_node(
            priv->gobj_node,
            "no_such_topic",
            json_pack("{s:s}", "id", "x"),
            0,
            gobj) >= 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: delete_node on a missing topic answered success",
            NULL
        );
        result += -1;
    }

    /*-----------------------------------------------*
     *  Test 13: the update-node COMMAND without
     *  `options`, the form the docs use with ycommand
     *  (M14 of the 2026-09-21 review). It asked the
     *  NULL options for `create` and logged "kw must be
     *  list or dict" with a stack on every call; the
     *  expected log list of this test has no room for it.
     *-----------------------------------------------*/
    jn_resp = gobj_command(priv->gobj_node, "update-node",
        json_pack("{s:s, s:{s:s, s:s}}",
            "topic_name", "users",
            "record",
                "id", "alice",
                "username", "alice_w"
        ),
        gobj
    );
    if(kw_get_int(gobj, jn_resp, "result", -1, 0) < 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: update-node without options was refused",
            "comment", "%s", kw_get_str(gobj, jn_resp, "comment", "", 0),
            NULL
        );
        result += -1;
    }
    JSON_DECREF(jn_resp)

    /*-----------------------------------------------*
     *  Test 14: import-db counts its errors by their
     *  CAUSE. It keyed them on gobj_log_last_message(),
     *  the process-global buffer of the last error, so
     *  each refused create got its own key (the message
     *  names the id), and a failure logged below ERROR
     *  was counted under an older, unrelated message.
     *  Two nodes that exist, one new, `skip`.
     *-----------------------------------------------*/
    {
        json_t *jn_db = json_pack("{s:[{s:s}], s:[{s:s, s:s}, {s:s, s:s, s:[]}]}",
            "departments",
                "id", "engineering",
            "users",
                "id", "alice", "username", "alice_imported",
                "id", "zoe_imported", "username", "zoe", "departments"
        );
        char *s_db = json_dumps(jn_db, JSON_COMPACT);
        gbuffer_t *gbuf_b64 = gbuffer_binary_to_base64(s_db, strlen(s_db));
        jn_resp = gobj_command(priv->gobj_node, "import-db",
            json_pack("{s:s, s:s}",
                "content64", gbuffer_cur_rd_pointer(gbuf_b64),
                "if-resource-exists", "skip"
            ),
            gobj
        );
        GBUFFER_DECREF(gbuf_b64)
        gbmem_free(s_db);
        JSON_DECREF(jn_db)

        json_t *errores = kw_get_dict(gobj, jn_resp, "data`errores", 0, 0);
        json_t *expected = json_pack("{s:i}", "node exists", 2);
        if(!json_equal(errores, expected) ||
                kw_get_int(gobj, jn_resp, "data`added", -1, 0) != 1 ||
                kw_get_int(gobj, jn_resp, "data`ignored", -1, 0) != 2 ||
                kw_get_int(gobj, jn_resp, "data`failure", -1, 0) != 0 ||
                !treedb_get_node(priv->tranger, treedb_name, "users", "zoe_imported")) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset", "%s", MSGSET_INTERNAL,
                "msg", "%s", "TEST FAIL: import-db does not count its errors by cause",
                "expected", "%j", expected,
                "got", "%j", jn_resp,
                NULL
            );
            result += -1;
        }
        JSON_DECREF(expected)
        JSON_DECREF(jn_resp)
    }

    if(result == 0) {
        gobj_log_info(gobj, 0,
            "msgset", "%s", MSGSET_INFO,
            "msg", "%s", "All c_node link event tests PASSED",
            NULL
        );
    }

    return result;
}

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
 *              Action callbacks
 ***************************************************************************/

/***************************************************************************
 *  EV_TREEDB_NODE_LINKED
 ***************************************************************************/
PRIVATE int ac_node_linked(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->linked_count++;

    const char *hook = kw_get_str(gobj, kw, "hook_name", "", 0);
    const char *pt = kw_get_str(gobj, kw, "parent_topic_name", "", 0);
    const char *ct = kw_get_str(gobj, kw, "child_topic_name", "", 0);
    const char *pid = kw_get_str(gobj, kw, "parent_id", "", 0);
    const char *cid = kw_get_str(gobj, kw, "child_id", "", 0);

    snprintf(priv->last_linked_hook, sizeof(priv->last_linked_hook), "%s", hook);
    snprintf(priv->last_linked_parent_topic, sizeof(priv->last_linked_parent_topic), "%s", pt);
    snprintf(priv->last_linked_child_topic, sizeof(priv->last_linked_child_topic), "%s", ct);
    snprintf(priv->last_linked_parent_id, sizeof(priv->last_linked_parent_id), "%s", pid);
    snprintf(priv->last_linked_child_id, sizeof(priv->last_linked_child_id), "%s", cid);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_TREEDB_NODE_UNLINKED
 ***************************************************************************/
PRIVATE int ac_node_unlinked(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->unlinked_count++;

    const char *hook = kw_get_str(gobj, kw, "hook_name", "", 0);
    const char *pid = kw_get_str(gobj, kw, "parent_id", "", 0);
    const char *cid = kw_get_str(gobj, kw, "child_id", "", 0);

    snprintf(priv->last_unlinked_hook, sizeof(priv->last_unlinked_hook), "%s", hook);
    snprintf(priv->last_unlinked_parent_id, sizeof(priv->last_unlinked_parent_id), "%s", pid);
    snprintf(priv->last_unlinked_child_id, sizeof(priv->last_unlinked_child_id), "%s", cid);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_TREEDB_NODE_CREATED
 ***************************************************************************/
PRIVATE int ac_node_created(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->created_count++;
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_TREEDB_NODE_UPDATED
 ***************************************************************************/
PRIVATE int ac_node_updated(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->updated_count++;
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_TREEDB_NODE_DELETED
 ***************************************************************************/
PRIVATE int ac_node_deleted(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->deleted_count++;
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

GOBJ_DEFINE_GCLASS(C_TEST_LINK_EVENTS);

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
        {EV_TREEDB_NODE_LINKED,     ac_node_linked,     0},
        {EV_TREEDB_NODE_UNLINKED,   ac_node_unlinked,   0},
        {EV_TREEDB_NODE_CREATED,    ac_node_created,    0},
        {EV_TREEDB_NODE_UPDATED,    ac_node_updated,    0},
        {EV_TREEDB_NODE_DELETED,    ac_node_deleted,    0},
        {EV_STOPPED,                ac_stopped,         0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,                0},
        {EV_TREEDB_NODE_LINKED,     EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
        {EV_TREEDB_NODE_UNLINKED,   EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
        {EV_TREEDB_NODE_CREATED,    EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
        {EV_TREEDB_NODE_UPDATED,    EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
        {EV_TREEDB_NODE_DELETED,    EVF_PUBLIC_EVENT|EVF_NO_WARN_SUBS},
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
PUBLIC int register_c_test_link_events(void)
{
    return create_gclass(C_TEST_LINK_EVENTS);
}
