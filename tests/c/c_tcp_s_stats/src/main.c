/****************************************************************************
 *          MAIN.C
 *
 *          Main of test_c_tcp_s_stats
 *          Tests the connection stats of C_TCP_S (connxs, tconnxs)
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include "c_test_tcp_s_stats.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_tcp_s_stats"
#define APP_DOC         "Test the connection stats of C_TCP_S"

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
            'name': 'test_tcp_s_stats',                             \n\
            'gclass': 'C_TEST_TCP_S_STATS',                         \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__legacy_side__',                              \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'legacy_port',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://127.0.0.1:7814',              \n\
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
                '__range__': [1,3],                                 \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': 'legacy-(^^__range__^^)',                \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'legacy-(^^__range__^^)',        \n\
                            'gclass': 'C_PROT_TCP4H',               \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'gclass': 'C_TCP'               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__new_side__',                                 \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'new_port',                             \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://127.0.0.1:7815'               \n\
                    }                                               \n\
                }                                                   \n\
            ],                                                      \n\
            '[^^children^^]': {                                     \n\
                '__range__': [1,3],                                 \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': 'new-(^^__range__^^)',                \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'new-(^^__range__^^)',        \n\
                            'gclass': 'C_PROT_TCP4H',               \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'gclass': 'C_TCP'               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__shared_side__',                              \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'shared_a',                             \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://127.0.0.1:7816',              \n\
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
                    'name': 'shared_b',                             \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://127.0.0.2:7816',              \n\
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
                '__range__': [1,4],                                 \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': 'shared-(^^__range__^^)',               \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'shared-(^^__range__^^)',       \n\
                            'gclass': 'C_PROT_TCP4H',               \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'gclass': 'C_TCP'               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__names_side__',                               \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'names_port',                           \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://127.0.0.1:7817'               \n\
                    }                                               \n\
                }                                                   \n\
            ],                                                      \n\
            '[^^children^^]': {                                     \n\
                '__range__': [1,2],                                 \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': 'names-(^^__range__^^)',                \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'names-(^^__range__^^)',        \n\
                            'gclass': 'C_PROT_TCP4H'                \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__lone_side__',                                \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'lone_port',                            \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://127.0.0.1:7818'               \n\
                    }                                               \n\
                },                                                  \n\
                {                                                   \n\
                    'name': 'lone-1',                               \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'lone-1',                       \n\
                            'gclass': 'C_PROT_TCP4H'                \n\
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
    result += register_c_test_tcp_s_stats();

    /*------------------------------*
     *  Start test
     *------------------------------*/
    set_expected_results(
        APP_NAME,
        /*  No warning, no error: a wrong count is an error  */
        json_array(),
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
