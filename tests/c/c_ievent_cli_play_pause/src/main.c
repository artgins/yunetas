/****************************************************************************
 *          MAIN.C
 *
 *          Main of test_c_ievent_cli_play_pause
 *          Tests who may play and pause a yuno through a C_IEVENT_CLI: only
 *          the link configured to reach the yuno's agent (a yuneta_agent).
 *          Any other link refuses: one warning per session, the ack -1.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include "c_test_play_pause.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_ievent_cli_play_pause"
#define APP_DOC         "Test who may play and pause a yuno through a C_IEVENT_CLI"
#define TEST_PORT       "7897"

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

/*
 *  cli_other: a link configured to reach a central (a gate's, or the
 *  agent's to its control center). cli_agent: the link the agent gives
 *  every yuno it launches, configured to reach a yuneta_agent.
 */
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
            'gclass': 'C_TEST_PLAY_PAUSE',                          \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__input_side__',                               \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'server_port',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'ws://127.0.0.1:" TEST_PORT "',      \n\
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
                '__range__': [1,2],                                 \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': 'raw-(^^__range__^^)',                  \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'raw-(^^__range__^^)',          \n\
                            'gclass': 'C_WEBSOCKET',                \n\
                            'kw': {                                 \n\
                                'iamServer': true                   \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'raw-(^^__range__^^)',  \n\
                                    'gclass': 'C_TCP'               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'cli_other',                                       \n\
            'gclass': 'C_IEVENT_CLI',                               \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
                'jwt': 'tester',                                    \n\
                'remote_yuno_name': '',                             \n\
                'remote_yuno_role': 'central',                       \n\
                'remote_yuno_service': 'agent'                      \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'cli_other',                               \n\
                    'gclass': 'C_IOGATE',                           \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'cli_other',                       \n\
                            'gclass': 'C_CHANNEL',                  \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'cli_other',               \n\
                                    'gclass': 'C_WEBSOCKET',        \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'cli_other',       \n\
                                            'gclass': 'C_TCP',      \n\
                                            'kw': {                 \n\
                                    'url': 'ws://127.0.0.1:" TEST_PORT "', \n\
                                    'timeout_between_connections': 100 \n\
                                            }                       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        },\n\
        {                                                           \n\
            'name': 'cli_agent',                                       \n\
            'gclass': 'C_IEVENT_CLI',                               \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
                'jwt': 'tester',                                    \n\
                'remote_yuno_name': '',                             \n\
                'remote_yuno_role': 'yuneta_agent',                       \n\
                'remote_yuno_service': 'agent'                      \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'cli_agent',                               \n\
                    'gclass': 'C_IOGATE',                           \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'cli_agent',                       \n\
                            'gclass': 'C_CHANNEL',                  \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'cli_agent',               \n\
                                    'gclass': 'C_WEBSOCKET',        \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'cli_agent',       \n\
                                            'gclass': 'C_TCP',      \n\
                                            'kw': {                 \n\
                                    'url': 'ws://127.0.0.1:" TEST_PORT "', \n\
                                    'timeout_between_connections': 100 \n\
                                            }                       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        }\n\
    ]                                                               \n\
}                                                                   \n\
";

time_measure_t time_measure;

/***************************************************************************
 *  The refusal: ONE line, a WARNING, for the three refused events
 ***************************************************************************/
#define REFUSAL_MARK "play/pause of the yuno refused"
int refusal_lines = 0;
int refusal_priority = -1;

static int refusal_log_write(void *v, int priority, const char *bf, size_t len)
{
    if(memmem(bf, len, REFUSAL_MARK, strlen(REFUSAL_MARK))) {
        refusal_lines++;
        refusal_priority = priority;
    }
    return 0;
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
    result += register_c_test_play_pause();

    /*------------------------------*
     *  Start test
     *------------------------------*/
    set_expected_results(
        APP_NAME,
        /*  Strict FIFO of the warnings and errors: one refusal for the session  */
        json_pack("[{s:s}]",
            "msg", "play/pause of the yuno refused: only the link to its agent plays or pauses it"
        ),
        NULL,   // expected
        NULL,   // ignore_keys
        1       // verbose
    );

    MT_START_TIME(time_measure)

    return result;
}

/***************************************************************************
 *  HACK This function is executed on yunetas environment
 *  AFTER the yuno has stopped
 ***************************************************************************/
static void cleaning(void)
{
    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, APP_NAME)

    result += test_json(NULL);

    if(refusal_lines != 1 || refusal_priority != LOG_WARNING) {
        printf("%sERROR --> %s: lines %d (expected 1), priority %d (expected %d)%s\n",
            On_Red BWhite,
            "the refusal is not ONE warning",
            refusal_lines,
            refusal_priority,
            LOG_WARNING,
            Color_Off
        );
        result += -1;
    }
    if(test_play_pause_failed) {
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
    glog_init();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    gobj_log_register_handler(
        "testing",
        0,
        capture_log_write,
        0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_WARNING, 0);

    gobj_log_register_handler(
        "refusal_log",
        0,
        refusal_log_write,
        0
    );
    gobj_log_add_handler("refusal_log", "refusal_log", LOG_OPT_ALL, 0);

    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    set_auto_kill_time(20);    // a crash-free hang must not hang the suite

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        NULL,       // persistent_attrs
        NULL,       // command_parser
        NULL,       // stats_parser
        NULL,       // authz_checker
        NULL,       // authentication_parser
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
