/****************************************************************************
 *          test_command_delete_user.c
 *
 *          Regression test for cmd_delete_user (C_AUTHZ).
 *
 *          Invariant under test: the ONLY "do not delete" boundary for an
 *          authz user is immutability, NOT whether it holds roles.
 *            1. plain local user (no roles)      -> delete-user deletes it
 *            2. role-holding user, no force      -> refused (result -1), kept
 *            3. role-holding user, force=1       -> deleted (roles unlinked)
 *            4. immutable seed user, even force  -> refused (result -1), kept
 *
 *          A real C_AUTHZ service is instantiated over a temp tranger store;
 *          a role and an immutable user are seeded via initial_load, and the
 *          mutable users are created with create-user (which can attach roles
 *          to a local user too -- that is why "has roles" is not a boundary).
 *
 *          The whole test runs under yuneta_entry_point (the canonical yuno
 *          lifecycle): the driver does its work from a timer action, INSIDE
 *          the running event loop, then set_yuno_must_die(). That matters for
 *          leak checking -- the tranger master registers io_uring fs_watchers
 *          per topic whose teardown is asynchronous, so the loop must be
 *          running to reap the cancellations. A synchronous test (no loop)
 *          would leak the fs_watchers and the yev_loop.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP             "test_command_delete_user"
#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DOC         "delete-user regression test"
#define APP_DATETIME    ""
#define STORE           "/tmp/test_command_delete_user_store"

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int s_result = 0;   /* accumulated check result, read after entry_point */

GOBJ_DEFINE_GCLASS(C_TEST_DELUSER);

typedef struct {
    hgobj   timer;
    BOOL    dying;
} PRIVATE_DATA;

/***************************************************************
 *              Config (single quotes -> double at runtime)
 ***************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': 'test_command_delete_user',                    \n\
        'tags': ['test', 'yunetas']                                 \n\
    }                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'work_dir': '/tmp',                                         \n\
        'console_log_handlers': {},                                 \n\
        'daemon_log_handlers': {}                                   \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'autoplay': true,                                           \n\
        'required_services': [],                                    \n\
        'public_services': [],                                      \n\
        'service_descriptor': {},                                   \n\
        'realm_owner': 'test',                                      \n\
        'realm_id':    'test',                                      \n\
        'trace_levels': {}                                          \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'deluser-driver',                               \n\
            'gclass': 'C_TEST_DELUSER',                             \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true                                        \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'authz',                                        \n\
            'gclass': 'C_AUTHZ',                                    \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'tranger_path': '" STORE "',                        \n\
                'master': true,                                     \n\
                'initial_load': {                                   \n\
                    'roles': [                                      \n\
                        {                                           \n\
                            'id': 'testrole',                       \n\
                            'disabled': false,                      \n\
                            'description': 'test role',             \n\
                            'realm_id': '*',                        \n\
                            'parent_role_id': '',                   \n\
                            'service': '*',                         \n\
                            'permission': '*'                       \n\
                        }                                           \n\
                    ],                                              \n\
                    'users': [                                      \n\
                        {                                           \n\
                            'id': 'seed_immutable',                 \n\
                            'roles': ['roles^testrole^users']       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE void check_int(const char *name, int got, int expected)
{
    if(got != expected) {
        printf("FAIL %-44s got %d expected %d\n", name, got, expected);
        s_result += -1;
    } else {
        printf("ok   %-44s (%d)\n", name, got);
    }
}

/*
 *  Drive a C_AUTHZ command internally: src == the authz service, and no
 *  __username__ in the kw, so the SDF_AUTHZ_X command gate never fires and we
 *  test cmd_delete_user's own logic in isolation.
 */
PRIVATE int cmd_result(hgobj authz, const char *command, json_t *kw)
{
    json_t *r = gobj_command(authz, command, kw, authz);
    int result = (int)kw_get_int(0, r, "result", -999, 0);
    JSON_DECREF(r)
    return result;
}

PRIVATE BOOL user_exists(const char *username)
{
    hgobj treedb = gobj_find_service("treedb_authzs", FALSE);
    if(!treedb) {
        return FALSE;
    }
    json_t *node = gobj_get_node(
        treedb,
        "users",
        json_pack("{s:s}", "id", username),
        0,
        treedb
    );
    BOOL exists = (node != NULL);
    JSON_DECREF(node)
    return exists;
}

/***************************************************************************
 *              The actual checks (run inside the loop, from the timer)
 ***************************************************************************/
PRIVATE void run_checks(hgobj gobj)
{
    /*
     *  C_AUTHZ is a config-defined service (already started+seeded by the yuno,
     *  so the yuno's own shutdown tears it down cleanly inside the loop). We
     *  only drive it here.
     */
    hgobj authz = gobj_find_service("authz", FALSE);
    if(!authz) {
        printf("FAIL %-44s\n", "C_AUTHZ service not found");
        s_result += -1;
        return;
    }

    /*  Case 1: plain local user (no roles) deletes without force  */
    check_int("create local_noroles",
        cmd_result(authz, "create-user", json_pack("{s:s}", "username", "local_noroles")),
        0);
    check_int("local_noroles exists after create", user_exists("local_noroles"), 1);
    check_int("delete local_noroles (no force)",
        cmd_result(authz, "delete-user", json_pack("{s:s}", "username", "local_noroles")),
        0);
    check_int("local_noroles gone", user_exists("local_noroles"), 0);

    /*  Case 2 & 3: role-holding user needs force  */
    check_int("create local_withroles",
        cmd_result(authz, "create-user",
            json_pack("{s:s, s:s}", "username", "local_withroles", "role", "roles^testrole^users")),
        0);
    check_int("local_withroles exists after create", user_exists("local_withroles"), 1);

    check_int("delete local_withroles (no force) refused",
        cmd_result(authz, "delete-user", json_pack("{s:s}", "username", "local_withroles")),
        -1);
    check_int("local_withroles kept after refusal", user_exists("local_withroles"), 1);

    check_int("delete local_withroles (force=1) ok",
        cmd_result(authz, "delete-user",
            json_pack("{s:s, s:b}", "username", "local_withroles", "force", 1)),
        0);
    check_int("local_withroles gone after force", user_exists("local_withroles"), 0);

    /*  Case 4: immutable seed user is never deletable, even with force  */
    check_int("delete seed_immutable (no force) refused",
        cmd_result(authz, "delete-user", json_pack("{s:s}", "username", "seed_immutable")),
        -1);
    check_int("delete seed_immutable (force=1) refused",
        cmd_result(authz, "delete-user",
            json_pack("{s:s, s:b}", "username", "seed_immutable", "force", 1)),
        -1);
    check_int("seed_immutable kept", user_exists("seed_immutable"), 1);
}

/***************************************************************
 *              Framework Methods
 ***************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);
}

PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gobj_start(priv->timer);
    return 0;
}

PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gobj_stop(priv->timer);
    return 0;
}

PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    set_timeout(priv->timer, 10);   // fire once, inside the loop
    return 0;
}

PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    clear_timeout(priv->timer);
    return 0;
}

/***************************************************************
 *              Actions
 ***************************************************************/
/*
 *  First fire (dying==FALSE): run the checks, then arm the death timer.
 *  Second fire (dying==TRUE): set_yuno_must_die() -- from inside the running
 *  loop, so the yuno teardown (and the tranger's async fs_watcher
 *  cancellations) are reaped before the loop exits.
 */
PRIVATE int ac_timer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->dying) {
        JSON_DECREF(kw)
        set_yuno_must_die();
        return 0;
    }

    run_checks(gobj);

    priv->dying = TRUE;
    set_timeout(priv->timer, 10);
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************
 *              GClass registration
 ***************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
    SDATA_END()
};

PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start  = mt_start,
    .mt_stop   = mt_stop,
    .mt_play   = mt_play,
    .mt_pause  = mt_pause,
};

PRIVATE int register_c_test_deluser(void)
{
    ev_action_t st_idle[] = {
        {EV_TIMEOUT, ac_timer, 0},
        {0, 0, 0}
    };
    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };
    event_type_t event_types[] = {
        {EV_TIMEOUT, 0},
        {0, 0}
    };
    hgclass gc = gclass_create(
        C_TEST_DELUSER,
        event_types,
        states,
        &gmt,
        0,                          // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,                          // authz_table
        0,                          // command_table
        0,                          // trace_level
        0                           // gclass_flag
    );
    return gc ? 0 : -1;
}

PRIVATE int register_yuno_and_more(void)
{
    /*  yuneta_entry_point already calls yunetas_register_c_core(); only our
     *  own driver gclass needs registering here.  */
    return register_c_test_deluser();
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    glog_init();
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);

    /*  The store must exist before C_AUTHZ autostarts (it checks is_directory).  */
    rmrdir(STORE);
    mkrdir(STORE, 02770);

    yuneta_setup(
        NULL,                   // persistent_attrs
        command_parser,         // command_parser (needed for gobj_command)
        NULL,                   // stats_parser
        NULL,                   // authz_checker
        NULL,                   // authentication_parser
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_SUPERBLOCK
    );

    int result = yuneta_entry_point(
        argc, argv,
        APP, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more,
        NULL                    // cleaning
    );

    rmrdir(STORE);

    size_t leaked = get_cur_system_memory();
    check_int("no memory leak", (int)leaked, 0);

    printf("\n%s: %s\n", APP, (s_result == 0 && result == 0) ? "PASS" : "FAIL");
    return s_result + result;
}
