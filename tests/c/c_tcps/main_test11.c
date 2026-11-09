/****************************************************************************
 *          MAIN.C
 *
 *          Test: a TLS handshake that does not end is dropped at
 *          timeout_tls_handshake (see c_test11.c). On OpenSSL here, on
 *          mbedTLS as test_tcps_test11_mbedtls.
 *
 *          Tasks
 *          - Play a wss server of 2 channels, timeout_tls_handshake 1 s
 *          - Two raw clients (no TLS) connect: one sends nothing, one the
 *            byte 0x16. The server drops both at the deadline
 *          - A raw client sends a line that is not TLS to a TLS server
 *            of the new method (no child_tree_filter): dropped at once,
 *            and nothing fires after it
 *          - A wss client opens a session (the channels are free again)
 *            and keeps it past the deadline
 *          - A wss client (timeout_tls_handshake 1 s) connects to a plain
 *            server that says nothing: it drops at the deadline, and
 *            connects again: the end
 *          - The three deadlines come in one window of 10 s: ONE warning
 *          - Shutdown
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_test11.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#ifndef TEST11_TLS_LIBRARY
    #define TEST11_TLS_LIBRARY  TLS_LIBRARY_NAME
#endif
#ifndef TEST11_PORT_TLS
    #define TEST11_PORT_TLS     "7806"
#endif
#ifndef TEST11_PORT_MUTE
    #define TEST11_PORT_MUTE    "7807"
#endif
#ifndef TEST11_PORT_NEW
    #define TEST11_PORT_NEW     "7810"
#endif
#ifndef TEST11_APP_NAME
    #define TEST11_APP_NAME     "test_tcps_test11"  // must be the executable's name
#endif

#define APP_NAME        TEST11_APP_NAME
#define APP_DOC         "Test C_TCP drops a TLS handshake that does not end"

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
            'name': 'c_test11',                                     \n\
            'gclass': 'C_TEST11',                                   \n\
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
                        'url': 'wss://127.0.0.1:" TEST11_PORT_TLS "', \n\
                        'crypto': {                                 \n\
                            'library': '" TEST11_TLS_LIBRARY "',    \n\
'ssl_certificate': '/yuneta/agent/certs/localhost.crt',             \n\
'ssl_certificate_key': '/yuneta/agent/certs/localhost.key',         \n\
                            'trace': false                          \n\
                        },                                          \n\
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
                    'name': 'input-(^^__range__^^)',                \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'input-(^^__range__^^)',        \n\
                            'gclass': 'C_WEBSOCKET',                \n\
                            'kw': {                                 \n\
                                'iamServer': true,                  \n\
                                'timeout_handshake': 1000,          \n\
                                'timeout_close': 500                \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'input-(^^__range__^^)', \n\
                                    'gclass': 'C_TCP',              \n\
                                    'kw': {                         \n\
                                        'timeout_tls_handshake': 1000 \n\
                                    }                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__output_side__',                              \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
            ],                                                      \n\
            '[^^children^^]': {                                     \n\
                '__range__': [0,1],                                 \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': 'case-(^^__range__^^)',                 \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'case-(^^__range__^^)',         \n\
                            'gclass': 'C_PROT_RAW',                 \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'case-(^^__range__^^)', \n\
                                    'gclass': 'C_TCP',              \n\
                                    'kw': {                         \n\
                                        'url':'tcp://127.0.0.1:" TEST11_PORT_TLS "', \n\
                                        'timeout_between_connections': 60000 \n\
                                    }                               \n\
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
                        'url': 'tcps://127.0.0.1:" TEST11_PORT_NEW "', \n\
                        'crypto': {                                 \n\
                            'library': '" TEST11_TLS_LIBRARY "',    \n\
'ssl_certificate': '/yuneta/agent/certs/localhost.crt',             \n\
'ssl_certificate_key': '/yuneta/agent/certs/localhost.key',         \n\
                            'trace': false                          \n\
                        }                                           \n\
                    }                                               \n\
                },                                                  \n\
                {                                                   \n\
                    'name': 'new-1',                                \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'new-1',                        \n\
                            'gclass': 'C_PROT_RAW',                 \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'new-1',                \n\
                                    'gclass': 'C_TCP',              \n\
                                    'kw': {                         \n\
                                        'timeout_tls_handshake': 1000 \n\
                                    }                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__bad_side__',                                 \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'bad_client',                           \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'bad_client',                   \n\
                            'gclass': 'C_PROT_RAW',                 \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'bad_client',           \n\
                                    'gclass': 'C_TCP',              \n\
                                    'kw': {                         \n\
                                        'url':'tcp://127.0.0.1:" TEST11_PORT_NEW "', \n\
                                        'timeout_between_connections': 60000 \n\
                                    }                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__output_side2__',                             \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'wss_client',                           \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'wss_client',                   \n\
                            'gclass': 'C_WEBSOCKET',                \n\
                            'kw': {                                 \n\
                                'timeout_close': 500                \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'wss_client',           \n\
                                    'gclass': 'C_TCP',              \n\
                                    'kw': {                         \n\
                                        'url':'wss://127.0.0.1:" TEST11_PORT_TLS "', \n\
                                        'timeout_between_connections': 60000, \n\
                                        'crypto': {                 \n\
                                            'library': '" TEST11_TLS_LIBRARY "', \n\
'ssl_trusted_certificate': '/yuneta/agent/certs/localhost.crt',     \n\
                                            'ssl_server_name': 'localhost', \n\
                                            'trace': false          \n\
                                        }                           \n\
                                    }                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__mute_side__',                                \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'mute_port',                            \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://127.0.0.1:" TEST11_PORT_MUTE "', \n\
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
                    'name': 'mute-(^^__range__^^)',                 \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'mute-(^^__range__^^)',         \n\
                            'gclass': 'C_PROT_RAW',                 \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'mute-(^^__range__^^)', \n\
                                    'gclass': 'C_TCP'               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__output_side3__',                             \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'tls_client',                           \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'tls_client',                   \n\
                            'gclass': 'C_WEBSOCKET',                \n\
                            'kw': {                                 \n\
                                'timeout_close': 500                \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'tls_client',           \n\
                                    'gclass': 'C_TCP',              \n\
                                    'kw': {                         \n\
                                        'url':'wss://127.0.0.1:" TEST11_PORT_MUTE "', \n\
                                        'timeout_tls_handshake': 1000, \n\
                                        'timeout_between_connections': 300, \n\
                                        'crypto': {                 \n\
                                            'library': '" TEST11_TLS_LIBRARY "', \n\
'ssl_trusted_certificate': '/yuneta/agent/certs/localhost.crt',     \n\
                                            'ssl_server_name': 'localhost', \n\
                                            'trace': false          \n\
                                        }                           \n\
                                    }                               \n\
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
    result += register_c_test11();

    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    // Avoid timer trace, too much information
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER), "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    // gobj_set_gclass_trace(gclass_find_by_name(C_TCP), "tls", TRUE);
    // gobj_set_gobj_trace(0, "machine", TRUE, 0);

    /*------------------------------*
     *  Start test
     *------------------------------*/
    json_t *errors_list = json_pack("[{s:s, s:i}, {s:s}, {s:s, s:i}]",
        "msg", "TLS handshake not ended in time, connection dropped",   // the raw clients, then the wss client: one window
        "suppressed", 0,
        "msg", "TLS handshake rejected",                                // the raw client of a line
        "msg", "TLS handshakes not ended in time, connections dropped, not said in their window",
        "suppressed", 2                                                 // the two others of the window, said at the end
    );

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
