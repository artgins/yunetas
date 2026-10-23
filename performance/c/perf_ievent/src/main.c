/****************************************************************************
 *          MAIN.C
 *
 *          perf_ievent: inter-yuno round trips (C_IEVENT_CLI -> C_WEBSOCKET
 *          -> C_IEVENT_SRV -> a service -> its publication back), in one yuno
 *
 *          - `__input_side__`: a C_IEVENT_SRV gate on ws://127.0.0.1:7798
 *          - `echo` (C_PERF_IEV_ECHO): answers EV_PERF_PING with EV_PERF_PONG
 *          - `cli` (C_IEVENT_CLI): the client of `echo`
 *          - `tester` (C_PERF_IEVENT): 100 000 round trips, timed
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include "c_perf_ievent.h"
#include "c_perf_iev_echo.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "perf_ievent"
#define APP_DOC         "Inter-yuno round trips through C_IEVENT_CLI and C_IEVENT_SRV"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

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
        'trace_levels': {                                           \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'tester',                                       \n\
            'gclass': 'C_PERF_IEVENT',                              \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'echo',                                         \n\
            'gclass': 'C_PERF_IEV_ECHO',                            \n\
            'autostart': true,                                      \n\
            'autoplay': false                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__input_side__',                               \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'server_port',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'ws://127.0.0.1:7798',               \n\
                        'child_tree_filter': {                      \n\
                            'kw': {                                 \n\
                                '__gclass_name__': 'C_CHANNEL',     \n\
                                '__disabled__': false,              \n\
                                'connected': false                  \n\
                            }                                       \n\
                        }                                           \n\
                    }                                               \n\
                }                                                   \n\
            ],                                                      \n\
            '[^^children^^]': {                                     \n\
                '__range__': [1,1],                                 \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': 'input-(^^__range__^^)',                \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'input-(^^__range__^^)',        \n\
                            'gclass': 'C_IEVENT_SRV',               \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'input-(^^__range__^^)', \n\
                                    'gclass': 'C_WEBSOCKET',        \n\
                                    'kw': {                         \n\
                                        'iamServer': true           \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                    'name': 'input-(^^__range__^^)', \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'cli',                                          \n\
            'gclass': 'C_IEVENT_CLI',                               \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
                'jwt': 'perf',                                      \n\
                'remote_yuno_name': '',                             \n\
                'remote_yuno_role': '"APP_NAME"',                   \n\
                'remote_yuno_service': 'echo'                       \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'cli_gate',                             \n\
                    'gclass': 'C_IOGATE',                           \n\
                    'as_service': true,                             \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'cli',                          \n\
                            'gclass': 'C_CHANNEL',                  \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'cli',                  \n\
                                    'gclass': 'C_WEBSOCKET',        \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'cli',          \n\
                                            'gclass': 'C_TCP',      \n\
                                            'kw': {                 \n\
                                    'url': 'ws://127.0.0.1:7798'    \n\
                                            }                       \n\
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
 *  Authentication without a C_AUTHZ: the user is the `jwt` of the identity
 *  card, and it may reach `echo`. What C_AUTHZ leaves on the gate is
 *  repeated: `__username__` on the C_IEVENT_SRV (src).
 ***************************************************************************/
static json_t *test_authentication_parser(hgobj gobj_service, json_t *kw, hgobj src)
{
    const char *username = kw_get_str(gobj_service, kw, "jwt", "", 0);
    gobj_write_str_attr(src, "__username__", username);

    json_t *jn_resp = json_pack("{s:i, s:s, s:s, s:{s:[]}}",
        "result", 0,
        "comment", "test authentication",
        "username", username,
        "services_roles",
            "echo"
    );
    KW_DECREF(kw)
    return jn_resp;
}

/***************************************************************************
 *  Every permission is granted
 ***************************************************************************/
static BOOL test_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return TRUE;
}

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
    result += register_c_perf_ievent();
    result += register_c_perf_iev_echo();

    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER), "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    // gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "ievents2", TRUE);
    // gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "ievents2", TRUE);
    // gobj_set_gclass_trace(gclass_find_by_name(C_WEBSOCKET), "traffic", TRUE);
    // gobj_set_gobj_trace(0, "machine", TRUE, 0);

    /*
     *  No warning nor error is expected: one fails the benchmark
     */
    set_expected_results(
        APP_NAME,
        json_array(),
        NULL,
        NULL,
        TRUE
    );

    return result;
}

/***************************************************************************
 *  HACK This function is executed on yunetas environment
 *  AFTER the yuno has stopped
 ***************************************************************************/
static void cleaning(void)
{
    result += test_json(NULL);
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*------------------------------*
     *  Capture the log
     *------------------------------*/
    glog_init();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_UP_WARNING, 0);

    gobj_log_register_handler(
        "testing",
        0,
        capture_log_write,
        0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_WARNING, 0);

    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    set_auto_kill_time(120);    // a hang must not hang the suite

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        NULL,       // persistent_attrs
        NULL,       // command_parser
        NULL,       // stats_parser
        test_authz_checker,
        test_authentication_parser,
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
