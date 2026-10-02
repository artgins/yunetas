/****************************************************************************
 *          MAIN.C
 *
 *          Main of test_c_qiogate_stats
 *          Tests the queue gauges of C_QIOGATE through a C_MQIOGATE
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include "c_test_qiogate_stats.h"

#define QUEUES_PATH     "/tmp/test_c_qiogate_stats"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_qiogate_stats"
#define APP_DOC         "Test the queue gauges of C_QIOGATE"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define DEBUG_MEMORY            false
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
            'C_TCP': ['connections'],                               \n\
            'C_TCP_S': ['listen', 'not-accepted']                   \n\
        }                                                           \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'c_test_qiogate_stats',                         \n\
            'gclass': 'C_TEST_QIOGATE_STATS',                       \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__output_side__',                              \n\
            'gclass': 'C_MQIOGATE',                                 \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
                'method': 'broadcast'                               \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'output-q1',                            \n\
                    'gclass': 'C_QIOGATE',                          \n\
                    'as_service': true,                             \n\
                    'kw': {                                         \n\
                        'tranger_path': '" QUEUES_PATH "',          \n\
                        'tranger_database': 'q1',                   \n\
                        'topic_name': 'msgs',                       \n\
                        'tkey': 'tm',                               \n\
                        'system_flag': 'sf_string_key'              \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'output-iogate-q1',             \n\
                            'gclass': 'C_IOGATE',                   \n\
                            'as_service': true,                     \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'output-q1',            \n\
                                    'gclass': 'C_CHANNEL',          \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'output-q1',    \n\
                                            'gclass': 'C_PROT_TCP4H', \n\
                                            'children': [           \n\
                                                {                   \n\
                                                    'name': 'output-q1', \n\
                                                    'gclass': 'C_TCP', \n\
                                                    'kw': {         \n\
                                                        'url': 'tcp://127.0.0.1:7746' \n\
                                                    }               \n\
                                                }                   \n\
                                            ]                       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                },                                                  \n\
                {                                                   \n\
                    'name': 'output-q2',                            \n\
                    'gclass': 'C_QIOGATE',                          \n\
                    'as_service': true,                             \n\
                    'kw': {                                         \n\
                        'tranger_path': '" QUEUES_PATH "',          \n\
                        'tranger_database': 'q2',                   \n\
                        'topic_name': 'msgs',                       \n\
                        'tkey': 'tm',                               \n\
                        'system_flag': 'sf_string_key'              \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'output-iogate-q2',             \n\
                            'gclass': 'C_IOGATE',                   \n\
                            'as_service': true,                     \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'output-q2',            \n\
                                    'gclass': 'C_CHANNEL',          \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'output-q2',    \n\
                                            'gclass': 'C_PROT_TCP4H', \n\
                                            'children': [           \n\
                                                {                   \n\
                                                    'name': 'output-q2', \n\
                                                    'gclass': 'C_TCP', \n\
                                                    'kw': {         \n\
                                                        'url': 'tcp://127.0.0.1:7746' \n\
                                                    }               \n\
                                                }                   \n\
                                            ]                       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        }                                                           \n\
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
    result += register_c_test_qiogate_stats();

    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    // Avoid timer trace, too much information
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER), "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    //gobj_set_gclass_trace(gclass_find_by_name(C_TEST_QIOGATE_STATS), "machine", TRUE);
    // gobj_set_gclass_trace(gclass_find_by_name(C_TCP), "traffic", TRUE);

    // Samples of global traces
    // gobj_set_gobj_trace(0, "create_delete", TRUE, 0);
    // gobj_set_gobj_trace(0, "start_stop", TRUE, 0);
    // gobj_set_gobj_trace(0, "subscriptions", TRUE, 0);
    // gobj_set_gobj_trace(0, "machine", TRUE, 0);
    // gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
    // gobj_set_gobj_trace(0, "liburing", TRUE, 0);

    /*------------------------------*
     *  Start test
     *------------------------------*/
    /*
     *  The warnings and errors, in order: none
     */
    json_t *errors_list = json_array();

    set_expected_results( // Check that no logs happen
        APP_NAME, // test name
        errors_list, // errors_list,
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
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
    if(test_qiogate_stats_failed) {
        result += -1;
    }
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*------------------------------*
     *  Captura salida logger
     *------------------------------*/
    rmrdir(QUEUES_PATH);    // the queues of a run before

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
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_WARNING, 0);

    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

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
        NULL,       // authentication_parser, default Monoclass C_AUTHZ
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

    rmrdir(QUEUES_PATH);

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
