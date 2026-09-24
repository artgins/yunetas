/***********************************************************************
 *          C_TEST_FIND_NEW_YUNOS.C
 *
 *          GClass to test the rows of the agent's find-new-yunos
 *
 *          find-new-yunos lists a create-yuno command for every yuno whose
 *          binary or configuration has a newer version than the release it
 *          runs. The old release stays the PRIMARY row until deactivate-snap
 *          promotes the new one, so after a `find-new-yunos create=1` that
 *          was never promoted (a resumed upgrade) the old row still matches:
 *          up to 7.25.4 the preview listed its instance as "would be
 *          created", and create=1 failed on it with "Yuno already exists".
 *
 *          Verified here, on the agent's own treedb schema and the agent's
 *          own source (find_new_yunos.c):
 *
 *              - a yuno whose new release is already registered comes back
 *                with `registered` TRUE;
 *              - a yuno with a newer binary and nothing registered comes
 *                back with `registered` FALSE, as a create-yuno to run;
 *              - a yuno with nothing newer does not come back at all;
 *              - a yuno_multiple row is judged by its own id: another
 *                instance of the same role and name registered at the new
 *                release does not answer for it.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include "find_new_yunos.h"
#include "treedb_schema_yuneta_agent.c"
#include "c_test_find_new_yunos.h"

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

#define TREEDB_NAME "treedb_yuneta_agent"

/***************************************************************************
 *              Framework Method Create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  A database of its own, wiped on every run.
     */
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), path_root, "c_agent_find_new_yunos", NULL);
    rmrdir(path_database);

    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path_root,
        "database", "c_agent_find_new_yunos",
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK
    );
    priv->tranger = tranger2_startup(0, jn_tranger, 0);

    helper_quote2doublequote(treedb_schema_yuneta_agent);
    json_t *jn_schema = legalstring2json(treedb_schema_yuneta_agent, TRUE);

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

    /*
     *  Fire a one-shot timer to run tests inside the event loop
     */
    set_timeout(priv->timer, 100);

    return 0;
}

/***************************************************************************
 *  Create a node, logging a failure. Returns the node (yours) or NULL.
 ***************************************************************************/
PRIVATE json_t *seed_node(hgobj gobj, const char *topic_name, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *node = gobj_create_node(
        priv->gobj_node,
        topic_name,
        kw,
        json_pack("{s:b, s:b}", "only_id", 1, "with_metadata", 1),
        gobj
    );
    if(!node) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: cannot create the fixture node",
            "topic_name", "%s", topic_name,
            NULL
        );
    }
    return node;
}

/***************************************************************************
 *  A yuno row as create-yuno writes it: the node, then its realm, binary
 *  and configuration links. Returns 0 or -1 (logged).
 ***************************************************************************/
PRIVATE int seed_yuno(
    hgobj gobj,
    json_t *realm,          // not owned
    const char *id,
    const char *yuno_role,
    const char *role_version,
    const char *yuno_name,
    const char *name_version,
    BOOL multiple
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char yuno_release[NAME_MAX];
    if(build_yuno_release_name(
            gobj, yuno_release, sizeof(yuno_release), role_version, name_version)<0) {
        // Error already logged
        return -1;
    }

    json_t *yuno = seed_node(gobj, "yunos", json_pack(
        "{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:b}",
        "id", id,
        "realm_id", kw_get_str(gobj, realm, "id", "", KW_REQUIRED),
        "yuno_role", yuno_role,
        "yuno_name", yuno_name,
        "yuno_release", yuno_release,
        "role_version", role_version,
        "name_version", name_version,
        "date", "2026/09/24 00:00:00",
        "yuno_multiple", multiple
    ));
    if(!yuno) {
        // Error already logged
        return -1;
    }

    int ret = gobj_link_nodes(
        priv->gobj_node,
        "yunos",
        "realms",
        json_incref(realm),
        "yunos",
        yuno,   // owned
        gobj
    );
    if(ret < 0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: cannot link the yuno to its realm",
            "id", "%s", id,
            NULL
        );
    }
    return ret;
}

/***************************************************************************
 *  The row of `id` in `rows`, or NULL.
 ***************************************************************************/
PRIVATE json_t *find_row(json_t *rows, const char *id)
{
    char prefix[NAME_MAX];
    snprintf(prefix, sizeof(prefix), "create-yuno id=%s ", id);

    int idx; json_t *row;
    json_array_foreach(rows, idx, row) {
        const char *command = json_string_value(json_object_get(row, "command"));
        if(command && strncmp(command, prefix, strlen(prefix))==0) {
            return row;
        }
    }
    return NULL;
}

/***************************************************************************
 *  Check that `id` came back with `registered`, or did not come back at
 *  all when `expected` is -1. Returns 0 or -1 (logged).
 ***************************************************************************/
PRIVATE int check_row(hgobj gobj, json_t *rows, const char *id, int expected, const char *what)
{
    json_t *row = find_row(rows, id);
    if(expected < 0) {
        if(row) {
            gobj_log_error(gobj, 0,
                "function", "%s", __FUNCTION__,
                "msgset", "%s", MSGSET_INTERNAL,
                "msg", "%s", "TEST FAIL: a yuno with nothing newer came back",
                "id", "%s", id,
                "what", "%s", what,
                NULL
            );
            return -1;
        }
        return 0;
    }
    if(!row) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: a yuno with a newer release did not come back",
            "id", "%s", id,
            "what", "%s", what,
            NULL
        );
        return -1;
    }
    BOOL registered = json_is_true(json_object_get(row, "registered"));
    if(registered != (expected?TRUE:FALSE)) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: wrong `registered`",
            "id", "%s", id,
            "what", "%s", what,
            "expected", "%d", expected,
            "command", "%s", json_string_value(json_object_get(row, "command")),
            NULL
        );
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  Run all tests — called from the timer callback inside the event loop
 ***************************************************************************/
PRIVATE int run_tests(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int result = 0;

    /*-----------------------------------------------*
     *  Fixture: one realm, the binaries, the configs
     *-----------------------------------------------*/
    json_t *realm = seed_node(gobj, "realms", json_pack("{s:s, s:s, s:s, s:s, s:s}",
        "id", "test.realm",
        "realm_owner", "test",
        "realm_role", "test",
        "realm_name", "realm",
        "realm_env", "test"
    ));
    if(!realm) {
        return -1;
    }

    struct {
        const char *id;
        const char *version;
    } binaries[] = {
        {"role_a", "1.0.0"}, {"role_a", "1.1.0"},
        {"role_b", "2.0.0"}, {"role_b", "2.1.0"},
        {"role_c", "3.0.0"},
        {"role_m", "4.0.0"}, {"role_m", "4.1.0"},
    };
    for(size_t i=0; i<ARRAY_SIZE(binaries); i++) {
        json_t *node = seed_node(gobj, "binaries", json_pack("{s:s, s:s, s:s, s:s}",
            "id", binaries[i].id,
            "version", binaries[i].version,
            "date", "2026/09/24 00:00:00",
            "binary", "/bin/true"
        ));
        if(!node) {
            result += -1;
        }
        JSON_DECREF(node)
    }

    const char *configs[] = {"role_a.name_a", "role_b.name_b", "role_c.name_c", "role_m.name_m"};
    for(size_t i=0; i<ARRAY_SIZE(configs); i++) {
        json_t *node = seed_node(gobj, "configurations", json_pack("{s:s, s:s, s:s, s:s}",
            "id", configs[i],
            "version", "1",
            "date", "2026/09/24 00:00:00",
            "zcontent", "{}"
        ));
        if(!node) {
            result += -1;
        }
        JSON_DECREF(node)
    }

    /*-----------------------------------------------*
     *  Fixture: the yunos
     *
     *  yuno_a  1.0.0-1, and its 1.1.0-1 already registered by a
     *          create=1 that nothing promoted: the old row is still
     *          the primary.
     *  yuno_b  2.0.0-1, and nothing registered at 2.1.0-1.
     *  yuno_c  3.0.0-1, the only binary: nothing newer.
     *  yuno_m1, yuno_m2   multiple, same role and name, both at
     *          4.0.0-1; only yuno_m1 registered at 4.1.0-1.
     *-----------------------------------------------*/
    result += seed_yuno(gobj, realm, "yuno_a", "role_a", "1.0.0", "name_a", "1", FALSE);
    result += seed_yuno(gobj, realm, "yuno_a", "role_a", "1.1.0", "name_a", "1", FALSE);
    result += seed_yuno(gobj, realm, "yuno_b", "role_b", "2.0.0", "name_b", "1", FALSE);
    result += seed_yuno(gobj, realm, "yuno_c", "role_c", "3.0.0", "name_c", "1", FALSE);
    result += seed_yuno(gobj, realm, "yuno_m1", "role_m", "4.0.0", "name_m", "1", TRUE);
    result += seed_yuno(gobj, realm, "yuno_m2", "role_m", "4.0.0", "name_m", "1", TRUE);
    result += seed_yuno(gobj, realm, "yuno_m1", "role_m", "4.1.0", "name_m", "1", TRUE);
    JSON_DECREF(realm)

    /*
     *  The fixture reproduces what it claims: the old release of yuno_a is
     *  still the primary one, or there would be nothing to find.
     */
    json_t *primary = gobj_get_node(
        priv->gobj_node,
        "yunos",
        json_pack("{s:s}", "id", "yuno_a"),
        json_pack("{s:b}", "only_id", 1),
        gobj
    );
    const char *primary_release = kw_get_str(gobj, primary, "yuno_release", "", 0);
    if(strcmp(primary_release, "1.0.0-1")!=0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: the fixture does not keep the old release primary",
            "primary_release", "%s", primary_release,
            NULL
        );
        result += -1;
    }
    JSON_DECREF(primary)

    /*-----------------------------------------------*
     *  The rows
     *-----------------------------------------------*/
    json_t *rows = find_new_yunos(gobj, priv->gobj_node, json_object(), FALSE, gobj);

    result += check_row(gobj, rows, "yuno_a", 1, "new release already registered");
    result += check_row(gobj, rows, "yuno_b", 0, "new release not registered");
    result += check_row(gobj, rows, "yuno_c", -1, "nothing newer");
    result += check_row(gobj, rows, "yuno_m1", 1, "multiple, registered");
    result += check_row(gobj, rows, "yuno_m2", 0, "multiple, only its sibling registered");

    json_t *row_b = find_row(rows, "yuno_b");
    const char *command_b = json_string_value(json_object_get(row_b, "command"));
    const char *expected_b =
        "create-yuno id=yuno_b realm_id=test.realm yuno_role=role_b role_version=2.1.0 "
        "yuno_name=name_b name_version=1 ";
    if(!command_b || strncmp(command_b, expected_b, strlen(expected_b))!=0) {
        gobj_log_error(gobj, 0,
            "function", "%s", __FUNCTION__,
            "msgset", "%s", MSGSET_INTERNAL,
            "msg", "%s", "TEST FAIL: wrong create-yuno command",
            "command", "%s", command_b?command_b:"(none)",
            "expected", "%s", expected_b,
            NULL
        );
        result += -1;
    }
    JSON_DECREF(rows)

    if(result == 0) {
        gobj_log_info(gobj, 0,
            "msgset", "%s", MSGSET_INFO,
            "msg", "%s", "All find-new-yunos tests PASSED",
            NULL
        );
    }

    return result;
}

/***************************************************************************
 *  The treedb wrote a node. Nothing to do here, but the parent of a
 *  C_NODE has to accept them.
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

GOBJ_DEFINE_GCLASS(C_TEST_FIND_NEW_YUNOS);

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
         *  fixture is written. A CHILD's published event must be declared
         *  by its parent. */
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
PUBLIC int register_c_test_find_new_yunos(void)
{
    return create_gclass(C_TEST_FIND_NEW_YUNOS);
}
