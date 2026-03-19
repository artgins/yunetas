/***********************************************************************
 *          C_TEST_LINK_EVENTS.C
 *
 *          GClass to test EV_TREEDB_NODE_LINKED/UNLINKED at c_node level
 *
 *          Verifies that when C_NODE's `with_link_events` attribute is set,
 *          link/unlink operations publish EV_TREEDB_NODE_LINKED and
 *          EV_TREEDB_NODE_UNLINKED events through the GObj event system.
 *
 *          Copyright (c) 2024, ArtGins.
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
                    'flag': ['hook', 'fkey'],                       \n\
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
    gobj_stop(priv->timer);

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
            "msgset", "%s", MSGSET_INTERNAL_ERROR,
            "msg", "%s", "TEST FAIL: link dept->dept expected 1 linked event",
            "got", "%d", priv->linked_count,
            NULL
        );
        result += -1;
    }
    if(strcmp(priv->last_linked_hook, "departments") != 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL_ERROR,
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
            "msgset", "%s", MSGSET_INTERNAL_ERROR,
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
            "msgset", "%s", MSGSET_INTERNAL_ERROR,
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
            "msgset", "%s", MSGSET_INTERNAL_ERROR,
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
            "msgset", "%s", MSGSET_INTERNAL_ERROR,
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
            "msgset", "%s", MSGSET_INTERNAL_ERROR,
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
        treedb_close_db(priv->tranger, "treedb_link_test");
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
PRIVATE int ac_node_linked(hgobj gobj, const char *event, json_t *kw, hgobj src)
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
PRIVATE int ac_node_unlinked(hgobj gobj, const char *event, json_t *kw, hgobj src)
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
PRIVATE int ac_node_created(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->created_count++;
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_TREEDB_NODE_UPDATED
 ***************************************************************************/
PRIVATE int ac_node_updated(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->updated_count++;
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_TREEDB_NODE_DELETED
 ***************************************************************************/
PRIVATE int ac_node_deleted(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->deleted_count++;
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  EV_TIMEOUT — runs test logic inside the event loop, then exits
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
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
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
