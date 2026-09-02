/***********************************************************************
 *          C_TEST_INITIAL_LOAD.C
 *
 *          GClass to test the `initial_load` seed of C_NODE
 *
 *          What it checks:
 *          - the seed comes up complete on the FIRST start, whatever the
 *            order of its topics (the child is declared before the parent
 *            here): records first, links second;
 *          - every seed record is immutable;
 *          - the links a seed is declared with cannot be cut: not by
 *            unlink-nodes, not by an autolink update that omits them, not
 *            by deleting the parent with force;
 *          - the links a person adds to a seed afterwards CAN be cut, and a
 *            record no seed hangs from can be deleted;
 *          - a second start creates nothing and writes no link.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "c_test_initial_load.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define TREEDB_NAME     "treedb_seed_test"
#define SEED_REF        "departments^hq^users"

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
 *  Schema: departments + users, the users hook being hook+fkey
 ***************************************************************************/
PRIVATE char schema_seed_test[] = "\
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
 *  The seed. The child (users) is declared BEFORE its parent
 *  (departments) on purpose: the order must not matter.
 ***************************************************************************/
PRIVATE char initial_load[] = "\
{                                                                   \n\
    'users': [                                                      \n\
        {                                                           \n\
            'id': 'root',                                           \n\
            'username': 'root',                                     \n\
            'departments': ['departments^hq^users']                 \n\
        }                                                           \n\
    ],                                                              \n\
    'departments': [                                                \n\
        {'id': 'hq', 'name': 'Headquarters'}                        \n\
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
    build_path(path_database, sizeof(path_database), path_root, "c_node_initial_load", NULL);
    rmrdir(path_database);

    /*
     *  Start tranger
     */
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", "c_node_initial_load",
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    priv->tranger = tranger2_startup(0, jn_tranger, 0);

    /*
     *  Create C_NODE child with the seed
     */
    helper_quote2doublequote(schema_seed_test);
    json_t *jn_schema = legalstring2json(schema_seed_test, TRUE);

    helper_quote2doublequote(initial_load);
    json_t *jn_initial_load = legalstring2json(initial_load, TRUE);

    json_t *kw_resource = json_pack("{s:I, s:s, s:o, s:i, s:o}",
        "tranger", (json_int_t)(uintptr_t)priv->tranger,
        "treedb_name", TREEDB_NAME,
        "treedb_schema", jn_schema,
        "exit_on_error", LOG_OPT_TRACE_STACK,
        "initial_load", jn_initial_load
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

PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->tranger) {
        // NOTE: treedb is already closed by C_NODE's mt_stop
        tranger2_shutdown(priv->tranger);
        priv->tranger = NULL;
    }
}

/***************************************************************************
 *              Local Methods
 ***************************************************************************/

/***************************************************************************
 *  One failed check
 ***************************************************************************/
PRIVATE int fail(hgobj gobj, const char *what)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "TEST FAIL",
        "what",         "%s", what,
        NULL
    );
    return -1;
}

/***************************************************************************
 *  Is `ref` among the fkeys of the child's `departments` column?
 ***************************************************************************/
PRIVATE BOOL user_linked_to(hgobj gobj, const char *user_id, const char *ref)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *user = treedb_get_node(priv->tranger, TREEDB_NAME, "users", user_id);
    if(!user) {
        return FALSE;
    }
    json_t *fkeys = json_object_get(user, "departments");
    int idx; json_t *v;
    json_array_foreach(fkeys, idx, v) {
        if(json_is_string(v) && strcmp(json_string_value(v), ref)==0) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL node_is_immutable(hgobj gobj, const char *topic_name, const char *id)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *node = treedb_get_node(priv->tranger, TREEDB_NAME, topic_name, id);
    if(!node) {
        return FALSE;
    }
    return kw_get_bool(gobj, node, "__md_treedb__`immutable", 0, 0);
}

/***************************************************************************
 *  The seed as it must be after any start: both records, immutable,
 *  the link written.
 ***************************************************************************/
PRIVATE int check_seed_state(hgobj gobj, const char *when)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = 0;

    json_t *hq = treedb_get_node(priv->tranger, TREEDB_NAME, "departments", "hq");
    json_t *root = treedb_get_node(priv->tranger, TREEDB_NAME, "users", "root");
    if(!hq || !root) {
        result += fail(gobj, when);
        result += fail(gobj, "seed records missing");
        return result;
    }
    if(!node_is_immutable(gobj, "departments", "hq")) {
        result += fail(gobj, when);
        result += fail(gobj, "hq is not immutable");
    }
    if(!node_is_immutable(gobj, "users", "root")) {
        result += fail(gobj, when);
        result += fail(gobj, "root is not immutable");
    }
    if(!user_linked_to(gobj, "root", SEED_REF)) {
        result += fail(gobj, when);
        result += fail(gobj, "root is not linked to hq");
    }
    if(json_array_size(json_object_get(hq, "users")) != 1) {
        result += fail(gobj, when);
        result += fail(gobj, "hq's users hook does not hold exactly root");
    }
    return result;
}

/***************************************************************************
 *  Run all tests, from the timer callback, inside the event loop
 ***************************************************************************/
PRIVATE int run_tests(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj node = priv->gobj_node;
    int result = 0;

    /*-----------------------------------------------*
     *  1. The seed is complete after the first
     *     start, child declared before parent
     *-----------------------------------------------*/
    result += check_seed_state(gobj, "after first start");

    /*-----------------------------------------------*
     *  2. unlink-nodes of the seed link is refused
     *-----------------------------------------------*/
    if(gobj_unlink_nodes(node,
        "users",
        "departments", json_pack("{s:s}", "id", "hq"),
        "users", json_pack("{s:s}", "id", "root"),
        gobj
    ) == 0) {
        result += fail(gobj, "unlink of a seed link was accepted");
    }
    if(!user_linked_to(gobj, "root", SEED_REF)) {
        result += fail(gobj, "seed link gone after refused unlink");
    }

    /*-----------------------------------------------*
     *  3. An autolink update that omits the seed
     *     link is refused, and changes nothing
     *-----------------------------------------------*/
    json_t *updated = gobj_update_node(node,
        "users",
        json_pack("{s:s, s:s, s:[]}", "id", "root", "username", "root2", "departments"),
        json_pack("{s:b}", "autolink", 1),
        gobj
    );
    if(updated) {
        result += fail(gobj, "update dropping a seed link was accepted");
        JSON_DECREF(updated)
    }
    json_t *root = treedb_get_node(priv->tranger, TREEDB_NAME, "users", "root");
    if(strcmp(kw_get_str(gobj, root, "username", "", 0), "root")!=0) {
        result += fail(gobj, "refused update changed the record");
    }
    if(!user_linked_to(gobj, "root", SEED_REF)) {
        result += fail(gobj, "seed link gone after refused update");
    }

    /*-----------------------------------------------*
     *  4. The same update, repeating the seed link,
     *     goes through
     *-----------------------------------------------*/
    updated = gobj_update_node(node,
        "users",
        json_pack("{s:s, s:s, s:[s]}", "id", "root", "username", "root2", "departments", SEED_REF),
        json_pack("{s:b}", "autolink", 1),
        gobj
    );
    if(!updated) {
        result += fail(gobj, "update keeping the seed link was refused");
    }
    JSON_DECREF(updated)
    root = treedb_get_node(priv->tranger, TREEDB_NAME, "users", "root");
    if(strcmp(kw_get_str(gobj, root, "username", "", 0), "root2")!=0) {
        result += fail(gobj, "accepted update did not change the record");
    }
    if(!user_linked_to(gobj, "root", SEED_REF)) {
        result += fail(gobj, "seed link gone after accepted update");
    }

    /*-----------------------------------------------*
     *  5. Deleting the parent with force is refused
     *-----------------------------------------------*/
    if(gobj_delete_node(node,
        "departments",
        json_pack("{s:s}", "id", "hq"),
        json_pack("{s:b}", "force", 1),
        gobj
    ) == 0) {
        result += fail(gobj, "force delete of the parent of a seed link was accepted");
    }
    if(!treedb_get_node(priv->tranger, TREEDB_NAME, "departments", "hq")) {
        result += fail(gobj, "parent of a seed link gone after refused delete");
    }

    /*-----------------------------------------------*
     *  6. Deleting the seed itself is refused
     *-----------------------------------------------*/
    if(gobj_delete_node(node,
        "users",
        json_pack("{s:s}", "id", "root"),
        json_pack("{s:b}", "force", 1),
        gobj
    ) == 0) {
        result += fail(gobj, "force delete of a seed was accepted");
    }

    /*-----------------------------------------------*
     *  7. A link a person adds to the seed is not
     *     frozen: link root to a second department,
     *     cut it, delete the department
     *-----------------------------------------------*/
    json_t *lab = gobj_update_node(node,
        "departments",
        json_pack("{s:s, s:s}", "id", "lab", "name", "Laboratory"),
        json_pack("{s:b}", "create", 1),
        gobj
    );
    if(!lab) {
        result += fail(gobj, "could not create a non-seed department");
    }
    JSON_DECREF(lab)

    if(gobj_link_nodes(node,
        "users",
        "departments", json_pack("{s:s}", "id", "lab"),
        "users", json_pack("{s:s}", "id", "root"),
        gobj
    ) < 0) {
        result += fail(gobj, "could not add a link to a seed");
    }
    if(!user_linked_to(gobj, "root", "departments^lab^users")) {
        result += fail(gobj, "added link not written");
    }
    if(gobj_unlink_nodes(node,
        "users",
        "departments", json_pack("{s:s}", "id", "lab"),
        "users", json_pack("{s:s}", "id", "root"),
        gobj
    ) < 0) {
        result += fail(gobj, "unlink of an added link was refused");
    }
    if(user_linked_to(gobj, "root", "departments^lab^users")) {
        result += fail(gobj, "added link survived its unlink");
    }
    if(!user_linked_to(gobj, "root", SEED_REF)) {
        result += fail(gobj, "seed link gone after unlinking an added one");
    }
    if(gobj_delete_node(node,
        "departments",
        json_pack("{s:s}", "id", "lab"),
        json_pack("{s:b}", "force", 1),
        gobj
    ) < 0) {
        result += fail(gobj, "delete of a department no seed hangs from was refused");
    }

    /*-----------------------------------------------*
     *  8. A non-seed user linked to hq can be
     *     unlinked and deleted
     *-----------------------------------------------*/
    json_t *alice = gobj_update_node(node,
        "users",
        json_pack("{s:s, s:s, s:[s]}", "id", "alice", "username", "alice", "departments", SEED_REF),
        json_pack("{s:b, s:b}", "create", 1, "autolink", 1),
        gobj
    );
    if(!alice) {
        result += fail(gobj, "could not create a non-seed user");
    }
    JSON_DECREF(alice)
    if(gobj_unlink_nodes(node,
        "users",
        "departments", json_pack("{s:s}", "id", "hq"),
        "users", json_pack("{s:s}", "id", "alice"),
        gobj
    ) < 0) {
        result += fail(gobj, "unlink of a non-seed user from hq was refused");
    }
    if(gobj_delete_node(node,
        "users",
        json_pack("{s:s}", "id", "alice"),
        json_pack("{s:b}", "force", 1),
        gobj
    ) < 0) {
        result += fail(gobj, "delete of a non-seed user was refused");
    }

    /*-----------------------------------------------*
     *  9. A second start: creates nothing, writes
     *     no link (the expected log says so), and
     *     the seed is still whole
     *-----------------------------------------------*/
    gobj_stop(node);
    gobj_start(node);
    result += check_seed_state(gobj, "after second start");

    if(result == 0) {
        gobj_log_info(gobj, 0,
            "msgset", "%s", MSGSET_INFO,
            "msg", "%s", "All c_node initial_load tests PASSED",
            NULL
        );
    }

    return result;
}

/***************************************************************************
 *              Action callbacks
 ***************************************************************************/

/***************************************************************************
 *  EV_TIMEOUT: runs the tests inside the event loop, then exits
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
 *  The treedb events C_NODE publishes to its parent
 ***************************************************************************/
PRIVATE int ac_treedb_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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

GOBJ_DEFINE_GCLASS(C_TEST_INITIAL_LOAD);

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
PUBLIC int register_c_test_initial_load(void)
{
    return create_gclass(C_TEST_INITIAL_LOAD);
}
