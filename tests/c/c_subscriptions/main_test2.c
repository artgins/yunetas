/****************************************************************************
 *          MAIN.C
 *
 *          Test: subscribe and unsusbcribe
 *
 *          Tasks

 *          Copyright (c) 2025 by ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_test2.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_subs_" "test2"
#define APP_DOC         "Test Subscriptions"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   false
#define MEM_MIN_BLOCK           0       // use default
#define MEM_MAX_BLOCK           0       // use default
#define MEM_SUPERBLOCK          0       // use default
#define MEM_MAX_SYSTEM_MEMORY   0       // use default

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': '"APP_NAME"',                                  \n\
        'tags': ['test', 'yunetas']                                 \n\
    }                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'console_log_handlers': {                                   \n\
        },                                                          \n\
        'daemon_log_handlers': {                                    \n\
        }                                                           \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'autoplay': true,                                           \n\
        'required_services': [],                                    \n\
        'public_services': [],                                      \n\
        'service_descriptor': {                                     \n\
        },                                                          \n\
        'i18n_dirname': '/yuneta/share/locale/',                    \n\
        'i18n_domain': 'test_timer',                                \n\
        'trace_levels': {                                           \n\
            'C_TEST2': []       \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'c_test2',                                      \n\
            'gclass': 'C_TEST2',                                    \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'children': [                                            \n\
            ]                                                       \n\
        }                                                          \n\
    ]                                                               \n\
}                                                                   \n\
";

time_measure_t time_measure;

/***************************************************************************
 *  HACK This function is executed on yunetas environment (mem, log, paths)
 *  BEFORE creating the yuno
 ***************************************************************************/
int result = 0;

static int register_yuno_and_more(void)
{
    int result = 0;

    /*--------------------*
     *  Register gclass
     *--------------------*/
    result += register_c_test2();

    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    // Avoid timer trace, too much information
    gobj_set_gclass_no_trace(gclass_find_by_name(C_YUNO), "machine", true);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", true);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER), "machine", true);
    gobj_set_global_no_trace("timer_periodic", true);

    // Samples of traces
    // gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "identity-card", true);
    // gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "identity-card", true);

    //gobj_set_gclass_trace(gclass_find_by_name(C_TEST2), "messages", true);
    //gobj_set_gclass_trace(gclass_find_by_name(C_TEST2), "machine", true);

    // gobj_set_gclass_trace(gclass_find_by_name(C_PEPON), "messages", true);
    // gobj_set_gclass_trace(gclass_find_by_name(C_TESTON), "messages", true);
    // gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "ievents2", true);
    // gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "ievents2", true);
    // gobj_set_gclass_trace(gclass_find_by_name(C_TCP), "traffic", true);

    // Samples of global traces
    // gobj_set_gobj_trace(0, "create_delete", true, 0);
    // gobj_set_gobj_trace(0, "create_delete2", true, 0);
    // gobj_set_gobj_trace(0, "start_stop", true, 0);
    // gobj_set_gobj_trace(0, "subscriptions", true, 0);
    // gobj_set_gobj_trace(0, "machine", true, 0);
    // gobj_set_gobj_trace(0, "ev_kw", true, 0);
    // gobj_set_gobj_trace(0, "liburing", true, 0);
    // gobj_set_gobj_trace(0, "liburing_timer", true, 0);

    /*------------------------------*
     *  Start test
     *------------------------------*/
    json_t *errors_list = json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
        "msg", "Starting yuno",
        "msg", "Playing yuno",
        "msg", "key ignored in subscription",
        "msg", "on_message",
        "msg", "No subscription found",
        "msg", "key ignored in subscription",
        "msg", "subscription(s) REPEATED, will be deleted and override",
        "msg", "on_message",
        "msg", "No subscription found",
        "msg", "key ignored in subscription",
        "msg", "subscription(s) REPEATED, will be deleted and override",
        "msg", "on_message",
        "msg", "No subscription found",
        "msg", "key ignored in subscription",
        "msg", "subscription(s) REPEATED, will be deleted and override",
        "msg", "on_message",
        "msg", "No subscription found",
        "msg", "key ignored in subscription",
        "msg", "subscription(s) REPEATED, will be deleted and override",
        "msg", "Exit to die",
        "msg", "Pausing yuno",
        "msg", "Yuno stopped, gobj end"
    );

    set_expected_results( // Check that no logs happen
        APP_NAME, // test name
        errors_list, // errors_list,
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        true    // verbose
    );

    MT_START_TIME(time_measure)

    return result;
}

/***************************************************************************
 *  HACK This function is executed on yunetas environment (mem, log, paths)
 *  BEFORE creating the yuno
 ***************************************************************************/
static void cleaning(void)
{
    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, APP_NAME)

    result += test_json(NULL);  // NULL: we want to check only the logs
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*------------------------------*
     *  Captura salida logger
     *------------------------------*/
    glog_init();

    /*
     *  Add all handlers very early
     */
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);


    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

    /*------------------------------------------------*
     *      To check
     *------------------------------------------------*/
    // gobj_set_deep_tracing(1);
    // set_auto_kill_time(6);
    // Avoid timer trace, too much information
    gobj_set_global_no_trace("timer_periodic", true);

    gobj_set_gobj_trace(0, "create_delete", true, 0);
    gobj_set_gobj_trace(0, "create_delete2", true, 0);
    gobj_set_gobj_trace(0, "start_stop", true, 0);
    gobj_set_gobj_trace(0, "machine", true, 0);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        NULL,       // persistent_attrs, default internal dbsimple
        NULL,       // command_parser, default internal command_parser
        NULL,       // stats_parser, default internal stats_parser
        NULL,       // authz_checker, default Monoclass C_AUTHZ
        NULL,       // authenticate_parser, default Monoclass C_AUTHZ
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_SUPERBLOCK
    );

    result += yuneta_entry_point(
        argc, argv,
        APP_NAME, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more,
        cleaning
    );

    if(get_cur_system_memory()!=0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP_NAME);
    }
    return result<0?-1:0;
}
