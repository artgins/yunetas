/****************************************************************************
 *          TEST_MAIN.C
 *
 *          Shared yuno entry point for the c_task_authenticate tests.
 *          See the companion .h for the contract.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdio.h>

#include <yunetas.h>

#include "c_mock_idp.h"
#include "c_test_driver.h"
#include "test_main.h"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#ifndef IDP_PORT
#error "build with -DIDP_PORT=\"<port>\": one port per test binary (see CMakeLists.txt)"
#endif

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config_template[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': '%s',                                          \n\
        'tags': ['test', 'yunetas']                                 \n\
    }                                                               \n\
}                                                                   \n\
";

/*
 *  variable_config_template is a printf format with three %s slots:
 *    1. expected_result for the test driver
 *    2. task-authenticate kw snippet (single-quoted JSON object)
 *    3. C_MOCK_IDP kw snippet (single-quoted JSON object; "{}" for the
 *       default mock that synthesises a normal discovery + token reply)
 *
 *  The task-authenticate service is autostart=false because the driver
 *  needs to subscribe to its EV_ON_TOKEN before it runs.
 */
PRIVATE char variable_config_template[]= "\
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
    'global': {                                                     \n\
        'Authz.allow_anonymous_in_localhost': true                  \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'test-driver',                                  \n\
            'gclass': 'C_TEST_DRIVER',                              \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'expected_result': %s%s                             \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'task-authenticate',                            \n\
            'gclass': 'C_TASK_AUTHENTICATE',                        \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': %s                                                \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__idp_side__',                                 \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'persistent_channels': false                        \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'idp_server',                           \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://0.0.0.0:" IDP_PORT "',        \n\
                        'child_tree_filter': {                      \n\
                            'kw': {                                 \n\
                                '__gclass_name__': 'C_CHANNEL',     \n\
                                '__disabled__': false,              \n\
                                'connected': false                  \n\
                            }                                       \n\
                        }                                           \n\
                    }                                               \n\
                },                                                  \n\
                {                                                   \n\
                    'name': 'idp-1',                                \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {},                                       \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'idp-1',                        \n\
                            'gclass': 'C_MOCK_IDP',                 \n\
                            'kw': %s,                               \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'idp-1',                \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {},                       \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'idp-1',        \n\
                                            'gclass': 'C_TCP'       \n\
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

/***************************************************************************
 *  Authz checker: allow everything in the self-contained test
 ***************************************************************************/
PRIVATE BOOL test_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return TRUE;
}

PRIVATE time_measure_t s_time_measure;
PRIVATE char s_driver_kw_extra[128] = "";
PRIVATE int s_extra_result = 0;
PRIVATE const char *s_app_name = "";
PRIVATE const char *s_expected_log_msg = NULL;

/***************************************************************************
 *  HACK: runs on yunetas environment BEFORE creating the yuno
 *  (called from yuneta_entry_point AFTER json_set_alloc_funcs has
 *  installed the gbmem allocators in jansson)
 ***************************************************************************/
PRIVATE int register_yuno_and_more(void)
{
    int res = 0;

    res += register_c_mock_idp();
    res += register_c_test_driver();

    /* Suppress noisy traces */
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER),  "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    /* Safety: kill the yuno after 10 s if stuck */
    set_auto_kill_time(10);

    /*
     *  Build expected_log_messages here (now that jansson uses gbmem) and
     *  hand ownership to set_expected_results.  test_json(NULL) in
     *  cleaning() will JSON_DECREF it.
     */
    json_t *expected_log_messages;
    if(!s_expected_log_msg) {
        expected_log_messages = json_array();
    } else if(s_expected_log_msg[0] == '[') {
        // A json list of the msg of each log, in order
        json_t *jn_msgs = anystring2json(s_expected_log_msg, strlen(s_expected_log_msg), TRUE);
        expected_log_messages = json_array();
        size_t idx;
        json_t *jn_msg;
        json_array_foreach(jn_msgs, idx, jn_msg) {
            json_array_append_new(expected_log_messages,
                json_pack("{s:s}", "msg", json_string_value(jn_msg)));
        }
        JSON_DECREF(jn_msgs)
    } else {
        expected_log_messages = json_pack("[{s:s}]", "msg", s_expected_log_msg);
    }

    set_expected_results(
        s_app_name,
        expected_log_messages,
        NULL,           // no JSON comparison
        NULL,           // no ignore_keys
        TRUE            // verbose
    );

    MT_START_TIME(s_time_measure)

    return res;
}

/***************************************************************************
 *  HACK: runs on yunetas environment BEFORE destroying the yuno
 ***************************************************************************/
PRIVATE void cleaning(void)
{
    MT_INCREMENT_COUNT(s_time_measure, 1)
    MT_PRINT_TIME(s_time_measure, s_app_name)

    s_extra_result += test_json(NULL);
}

/***************************************************************************
 *  Before run_task_authenticate_test(): the task is run `runs` times, and
 *  stopped inside its EV_ON_TOKEN when `stop_in_on_token`
 ***************************************************************************/
PUBLIC void set_task_runs(int runs, BOOL stop_in_on_token)
{
    snprintf(s_driver_kw_extra, sizeof(s_driver_kw_extra),
        ", 'runs': %d, 'stop_in_on_token': %s", runs, stop_in_on_token? "true" : "false");
}

/***************************************************************************
 *  Before run_task_authenticate_test(): the test goes on `ms` after
 *  EV_ON_TOKEN
 ***************************************************************************/
PUBLIC void set_task_end_delay(int ms)
{
    snprintf(s_driver_kw_extra, sizeof(s_driver_kw_extra), ", 'end_delay_ms': %d", ms);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int run_task_authenticate_test(
    int argc, char *argv[],
    const char *app_name,
    const char *app_doc,
    const char *task_kw_snippet,
    const char *mock_idp_kw_snippet,
    int expected_result,
    const char *expected_log_msg,
    BOOL capture_warnings
)
{
    s_app_name = app_name;
    s_expected_log_msg = expected_log_msg;

    /*------------------------------*
     *  Init logger
     *------------------------------*/
    glog_init();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    gobj_log_register_handler(
        "testing",
        0,
        capture_log_write,
        0
    );
    gobj_log_add_handler(
        "test_capture",
        "testing",
        capture_warnings ? LOG_OPT_UP_WARNING : LOG_OPT_UP_ERROR,
        0
    );

    /* Memory leak check */
    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    /*------------------------------------------------*
     *          Render fixed + variable config
     *------------------------------------------------*/
    char fixed_config[1024];
    snprintf(fixed_config, sizeof(fixed_config), fixed_config_template, app_name);

    char expected_result_str[16];
    snprintf(expected_result_str, sizeof(expected_result_str), "%d", expected_result);

    /*
     *  Static, not on the heap: a block allocated before yuneta_entry_point()
     *  is still alive when the yuno ends, and the end reports it as a leak
     */
    static char variable_config[16*1024];
    int cfg_len = snprintf(variable_config, sizeof(variable_config), variable_config_template,
        expected_result_str,
        s_driver_kw_extra,
        task_kw_snippet,
        mock_idp_kw_snippet ? mock_idp_kw_snippet : "{}");
    if(cfg_len < 0 || (size_t)cfg_len >= sizeof(variable_config)) {
        printf("ERROR: variable_config does not fit in %d bytes\n", (int)sizeof(variable_config));
        return -1;
    }

    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    yuneta_setup(
        NULL,       // persistent_attrs
        NULL,       // command_parser
        NULL,       // stats_parser
        test_authz_checker,
        NULL,       // authentication_parser
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_SUPERBLOCK
    );

    char app_datetime[64];
    snprintf(app_datetime, sizeof(app_datetime), "%s %s", __DATE__, __TIME__);

    int result = yuneta_entry_point(
        argc, argv,
        app_name, APP_VERSION, APP_SUPPORT, app_doc, app_datetime,
        fixed_config,
        variable_config,
        register_yuno_and_more,
        cleaning
    );

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    return result + s_extra_result;
}
