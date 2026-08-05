/****************************************************************************
 *          test_command_shutdown.c
 *
 *          Regression test for the `shutdown` command (C_YUNO).
 *
 *          Invariant under test: the command ANSWERS FIRST and DIES AFTER.
 *          The answer travels over the same event loop the shutdown is about
 *          to stop, so a handler that called set_yuno_must_die() itself would
 *          take the socket down with the response still in it. The handler
 *          therefore only arms a timer, and the yuno's periodic action does
 *          the dying -- the same shape `timeout_restart` has always used.
 *
 *          So the test asserts three things, in order:
 *            1. the command answers result=0 with a comment
 *            2. the yuno is STILL RUNNING when the command returns
 *            3. the yuno dies on its own shortly after, with no further help
 *
 *          The third one is what a watchdog turns into a real check: if the
 *          periodic never kills us, the watchdog fires first and the test
 *          fails with a message instead of hanging until ctest times out.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP             "test_command_shutdown"
#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DOC         "shutdown command regression test"
#define APP_DATETIME    ""

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

/*
 *  The yuno's periodic runs at `timeout_periodic` (1000 ms by default) and it
 *  is what serves the pending shutdown, so the watchdog has to be comfortably
 *  above one period.
 */
#define WATCHDOG_MS             5000

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int s_result = 0;   /* accumulated check result, read after entry_point */

GOBJ_DEFINE_GCLASS(C_TEST_SHUTDOWN);

typedef struct {
    hgobj   timer;
    BOOL    asked;      /* the shutdown command has been issued */
} PRIVATE_DATA;

/***************************************************************
 *              Config (single quotes -> double at runtime)
 ***************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': 'test_command_shutdown',                       \n\
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
            'name': 'shutdown-driver',                              \n\
            'gclass': 'C_TEST_SHUTDOWN',                            \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true                                        \n\
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

PRIVATE void check_true(const char *name, BOOL got)
{
    check_int(name, got?1:0, 1);
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

    /*  Ask from inside the running loop, which is where a command arrives. */
    set_timeout(priv->timer, 100);
    return 0;
}

PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    return 0;
}

/***************************************************************
 *              Actions
 ***************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->asked) {
        priv->asked = TRUE;

        /*
         *  src is ourselves and the kw carries no __username__, so the
         *  SDF_AUTHZ_X gate does not fire: what is under test is the
         *  handler, not the authz plumbing.
         */
        json_t *resp = gobj_command(gobj_yuno(), "shutdown", json_object(), gobj);

        check_int("shutdown answers result=0",
            (int)kw_get_int(gobj, resp, "result", -999, 0), 0
        );
        check_true("shutdown answers a comment",
            !empty_string(kw_get_str(gobj, resp, "comment", "", 0))
        );
        JSON_DECREF(resp)

        /*
         *  THE point of the whole test: the answer came back and we are
         *  still here. A handler that died on the spot would never reach
         *  this line -- and neither would its response reach the caller.
         */
        check_true("the yuno is still running after answering",
            gobj_is_running(gobj_yuno())
        );

        /*  Nobody helps it die from here on: the periodic must do it. */
        set_timeout(priv->timer, WATCHDOG_MS);

        KW_DECREF(kw)
        return 0;
    }

    /*
     *  Reaching here means the periodic never served the pending shutdown.
     */
    check_true("the yuno died on its own after the command", FALSE);
    set_yuno_must_die();

    KW_DECREF(kw)
    return 0;
}

/***************************************************************
 *              GClass
 ***************************************************************/
PRIVATE sdata_desc_t attrs_table[] = {
    SDATA_END()
};

PRIVATE int register_c_test_shutdown(void)
{
    /*  Not static: the event and state names are runtime pointers
     *  (GOBJ_DEFINE_EVENT), so they cannot initialize a static table.  */
    static const GMETHODS gmt = {
        .mt_create = mt_create,
        .mt_start = mt_start,
        .mt_stop = mt_stop,
    };

    event_type_t event_types[] = {
        {EV_TIMEOUT,    0},
        {0, 0}
    };

    ev_action_t st_idle[] = {
        {EV_TIMEOUT,    ac_timeout,     0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,   st_idle},
        {0, 0}
    };

    hgclass gc = gclass_create(
        C_TEST_SHUTDOWN,
        event_types,
        states,
        &gmt,
        0,                          // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,                          // authz_table
        0,                          // command_table
        0,                          // s_user_trace_level
        0                           // gclass_flag
    );
    return gc ? 0 : -1;
}

PRIVATE int register_yuno_and_more(void)
{
    /*  yuneta_entry_point already calls yunetas_register_c_core(); only our
     *  own driver gclass needs registering here.  */
    return register_c_test_shutdown();
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

    size_t leaked = get_cur_system_memory();
    check_int("no memory leak", (int)leaked, 0);

    printf("\n%s: %s\n", APP, (s_result == 0 && result == 0) ? "PASS" : "FAIL");
    return s_result + result;
}
