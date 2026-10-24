/****************************************************************************
 *          test_authz_scoped_permissions.c
 *
 *          The scope of a permission: a role's `parameters` (C_AUTHZ).
 *
 *          A role grants a permission only when the kw of the check carries
 *          what its `parameters` say: each key with its value, or one of its
 *          list of values. An entry of `permissions` may carry its own
 *          `parameters`, which replace the role's for that entry. A role
 *          with no `parameters` grants as it always did.
 *            1. viewer   read-scenarios for s1 only: s1 yes, s2 no, a kw
 *                        with no scenario_id no, another permission no
 *            2. multi    read for s1 and s2 (its entry's list), run for s2
 *                        (the role's scope, the entry has none)
 *            3. reader   read-scenarios with no scope: any scenario, and a
 *                        kw with none
 *            4. denier   run-scenarios for all, denied for s1 by a scoped
 *                        entry: s1 no, s2 yes; a kw with no scenario_id
 *                        no: a scoped deny fails CLOSED, it applies unless
 *                        the kw says a value out of its scope
 *            5. the value is compared as json: the scope "s1" does not
 *               match the integer 1, nor "S1"
 *            6. a scope that is not a dict grants nothing and logs an
 *               error once per check
 *
 *          A real C_AUTHZ service over a temp tranger store, its roles and
 *          users seeded by initial_load. The checks ask the permissions of
 *          this driver (its authz table) as the service `checked`, from a
 *          timer action inside the running loop (see test_command_delete_user.c
 *          for why).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP             "test_authz_scoped_permissions"
#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DOC         "scoped permissions of C_AUTHZ"
#define APP_DATETIME    ""
#define STORE           "/tmp/test_authz_scoped_permissions_store"

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int s_result = 0;   /* accumulated check result, read after entry_point */
PRIVATE int s_errors = 0;   /* logs of priority ERROR and up, see count_errors() */

GOBJ_DEFINE_GCLASS(C_TEST_SCOPED);

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
        'yuno_role': 'test_authz_scoped_permissions',               \n\
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
            'name': 'checked',                                      \n\
            'gclass': 'C_TEST_SCOPED',                              \n\
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
                            'id': 'viewer',                         \n\
                            'description': 'reads s1',              \n\
                            'realm_id': '*',                        \n\
                            'service': 'checked',                   \n\
                            'permission': 'read-scenarios',         \n\
                            'parameters': {'scenario_id': ['s1']}   \n\
                        },                                          \n\
                        {                                           \n\
                            'id': 'multi',                          \n\
                            'description': 'reads s1+s2, runs s2',  \n\
                            'realm_id': '*',                        \n\
                            'service': 'checked',                   \n\
                            'parameters': {'scenario_id': 's2'},    \n\
                            'permissions': [                        \n\
                                {                                   \n\
                                    'permission': 'read-scenarios', \n\
                                    'parameters': {                 \n\
                                        'scenario_id': ['s1', 's2'] \n\
                                    }                               \n\
                                },                                  \n\
                                {'permission': 'run-scenarios'}     \n\
                            ]                                       \n\
                        },                                          \n\
                        {                                           \n\
                            'id': 'reader',                         \n\
                            'description': 'reads all',             \n\
                            'realm_id': '*',                        \n\
                            'service': 'checked',                   \n\
                            'permission': 'read-scenarios'          \n\
                        },                                          \n\
                        {                                           \n\
                            'id': 'denier',                         \n\
                            'description': 'runs all but s1',       \n\
                            'realm_id': '*',                        \n\
                            'service': 'checked',                   \n\
                            'permission': 'run-scenarios',          \n\
                            'permissions': [                        \n\
                                {                                   \n\
                                    'permission': 'run-scenarios',  \n\
                                    'deny': true,                   \n\
                                    'parameters': {                 \n\
                                        'scenario_id': ['s1']       \n\
                                    }                               \n\
                                }                                   \n\
                            ]                                       \n\
                        },                                          \n\
                        {                                           \n\
                            'id': 'broken',                         \n\
                            'description': 'a scope not a dict',    \n\
                            'realm_id': '*',                        \n\
                            'service': 'checked',                   \n\
                            'permissions': [                        \n\
                                {                                   \n\
                                    'permission': 'read-scenarios', \n\
                                    'parameters': ['s1']            \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ],                                              \n\
                    'users': [                                      \n\
                        {'id': 'viewer', 'roles': ['roles^viewer^users']}, \n\
                        {'id': 'multi',  'roles': ['roles^multi^users']},  \n\
                        {'id': 'reader', 'roles': ['roles^reader^users']}, \n\
                        {'id': 'denier', 'roles': ['roles^denier^users']}, \n\
                        {'id': 'broken', 'roles': ['roles^broken^users']}  \n\
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
        printf("FAIL %-52s got %d expected %d\n", name, got, expected);
        s_result += -1;
    } else {
        printf("ok   %-52s (%d)\n", name, got);
    }
}

/*
 *  A log handler that only counts: a check reads the difference around one
 *  call, so what the rest of the test logs does not count.
 */
PRIVATE int count_errors(void *h, int priority, const char *bf, size_t len)
{
    s_errors++;
    return 0;
}

/*
 *  Has `username` the permission on this scenario? An empty scenario_id
 *  leaves it out of the kw.
 */
PRIVATE void check_may(
    hgobj gobj,
    const char *username,
    const char *permission,
    const char *scenario_id,
    int expected
)
{
    json_t *kw = json_pack("{s:s}", "__username__", username);
    if(!empty_string(scenario_id)) {
        json_object_set_new(kw, "scenario_id", json_string(scenario_id));
    }
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "%s %s %s", username, permission,
        empty_string(scenario_id)? "(none)" : scenario_id);
    check_int(name, gobj_user_has_authz(gobj, permission, kw, gobj)? 1 : 0, expected);
}

PRIVATE void run_checks(hgobj gobj)
{
    /*  1  */
    check_may(gobj, "viewer", "read-scenarios", "s1", 1);
    check_may(gobj, "viewer", "read-scenarios", "s2", 0);
    check_may(gobj, "viewer", "read-scenarios", "", 0);
    check_may(gobj, "viewer", "run-scenarios", "s1", 0);
    check_may(gobj, "viewer", "command-agent", "s1", 0);

    /*  2  */
    check_may(gobj, "multi", "read-scenarios", "s1", 1);
    check_may(gobj, "multi", "read-scenarios", "s2", 1);
    check_may(gobj, "multi", "read-scenarios", "s3", 0);
    check_may(gobj, "multi", "run-scenarios", "s2", 1);
    check_may(gobj, "multi", "run-scenarios", "s1", 0);

    /*  3  */
    check_may(gobj, "reader", "read-scenarios", "s3", 1);
    check_may(gobj, "reader", "read-scenarios", "", 1);
    check_may(gobj, "reader", "run-scenarios", "s3", 0);

    /*  4  */
    check_may(gobj, "denier", "run-scenarios", "s1", 0);
    check_may(gobj, "denier", "run-scenarios", "s2", 1);
    check_may(gobj, "denier", "run-scenarios", "", 0);   // a deny fails closed

    /*  5  */
    json_t *kw = json_pack("{s:s, s:i}", "__username__", "viewer", "scenario_id", 1);
    check_int("viewer read-scenarios (integer 1)",
        gobj_user_has_authz(gobj, "read-scenarios", kw, gobj)? 1 : 0, 0);
    check_may(gobj, "viewer", "read-scenarios", "S1", 0);

    /*  6  */
    int errors0 = s_errors;
    check_may(gobj, "broken", "read-scenarios", "s1", 0);
    check_int("a scope not a dict logs one error", s_errors - errors0, 1);
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
 *  First fire: run the checks, then arm the death timer. Second fire:
 *  set_yuno_must_die(), from inside the running loop.
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

PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATAAUTHZ (DTP_SCHEMA, "read-scenarios",   0,      0,      0,      "Read scenarios"),
SDATAAUTHZ (DTP_SCHEMA, "run-scenarios",    0,      0,      0,      "Run scenarios"),
SDATAAUTHZ (DTP_SCHEMA, "command-agent",    0,      0,      0,      "Command an agent"),
SDATA_END()
};

PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_start  = mt_start,
    .mt_stop   = mt_stop,
    .mt_play   = mt_play,
    .mt_pause  = mt_pause,
};

PRIVATE int register_c_test_scoped(void)
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
        C_TEST_SCOPED,
        event_types,
        states,
        &gmt,
        0,                          // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        0,                          // command_table
        0,                          // trace_level
        0                           // gclass_flag
    );
    return gc ? 0 : -1;
}

PRIVATE int register_yuno_and_more(void)
{
    return register_c_test_scoped();
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    glog_init();
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("count_errors", 0, count_errors, 0);
    gobj_log_add_handler("count_errors", "count_errors", LOG_OPT_UP_ERROR, 0);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);

    /*  The store must exist before C_AUTHZ autostarts (it checks is_directory).  */
    rmrdir(STORE);
    mkrdir(STORE, 02770);

    yuneta_setup(
        NULL,                   // persistent_attrs
        command_parser,         // command_parser
        NULL,                   // stats_parser
        NULL,                   // authz_checker: C_AUTHZ's
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
