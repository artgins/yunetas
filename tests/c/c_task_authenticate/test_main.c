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

#define IDP_PORT    "18901"

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
                'expected_result': %s                               \n\
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
PRIVATE int s_extra_result = 0;
PRIVATE const char *s_app_name = "";

/***************************************************************************
 *  HACK: runs on yunetas environment BEFORE creating the yuno
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
 *
 ***************************************************************************/
PUBLIC int run_task_authenticate_test(
    int argc, char *argv[],
    const char *app_name,
    const char *app_doc,
    const char *task_kw_snippet,
    const char *mock_idp_kw_snippet,
    int expected_result,
    json_t *expected_log_messages,
    BOOL capture_warnings
)
{
    s_app_name = app_name;

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

    /* Capture log expectations */
    set_expected_results(
        app_name,
        expected_log_messages ? expected_log_messages : json_array(),
        NULL,           // no JSON comparison
        NULL,           // no ignore_keys
        TRUE            // verbose
    );

    /*------------------------------------------------*
     *          Render fixed + variable config
     *------------------------------------------------*/
    char fixed_config[1024];
    snprintf(fixed_config, sizeof(fixed_config), fixed_config_template, app_name);

    char expected_result_str[16];
    snprintf(expected_result_str, sizeof(expected_result_str), "%d", expected_result);

    /* The variable_config is large; allocate once on the heap. */
    size_t cfg_sz = strlen(variable_config_template)
                  + strlen(expected_result_str)
                  + strlen(task_kw_snippet)
                  + strlen(mock_idp_kw_snippet ? mock_idp_kw_snippet : "{}")
                  + 64;
    char *variable_config = gbmem_malloc(cfg_sz);
    if(!variable_config) {
        printf("ERROR: out of memory rendering variable_config\n");
        return -1;
    }
    snprintf(variable_config, cfg_sz, variable_config_template,
        expected_result_str,
        task_kw_snippet,
        mock_idp_kw_snippet ? mock_idp_kw_snippet : "{}");

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

    gbmem_free(variable_config);

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    return result + s_extra_result;
}
