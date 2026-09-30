/****************************************************************************
 *          MAIN.C
 *
 *          Test: a TLS server (child_tree_filter) stopped and started
 *          again while a connection of it lives, and its certificates
 *          reloaded: the old connection and a new one must both work.
 *          Freed memory is poisoned, so a use of the ytls the restart
 *          freed fails every time.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include <c_pepon.h>
#include "c_test6.h"
#include "../emailsender/poison_alloc.h"

/*
 *  The backend, and the port, of this binary: test6 runs on the backend
 *  TLS_LIBRARY_NAME prefers, and test6_mbedtls (CMakeLists.txt, when both
 *  are compiled in) on mbedTLS, at once
 */
#ifndef TEST6_TLS_LIBRARY
    #define TEST6_TLS_LIBRARY   TLS_LIBRARY_NAME
#endif
#ifndef TEST6_PORT
    #define TEST6_PORT          "7788"
#endif

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_tcps_" "test6"
#define APP_DOC         "Test C_TCP_S TLS restart with a live connection"

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
            'C_TCP_S': ['listen', 'not-accepted']       \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
        '__input_side__.__json_config_variables__': {               \n\
            '__input_url__': 'tcps://0.0.0.0:" TEST6_PORT "',                 \n\
            '__input_host__': '0.0.0.0',                            \n\
            '__input_port__': '" TEST6_PORT "'                                \n\
        }                                                           \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'c_test6',                                      \n\
            'gclass': 'C_TEST6',                                    \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'children': [                                            \n\
            ]                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__input_side__',                               \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'children': [                                            \n\
                {                                                   \n\
                    'name': 'server_port',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'crypto': {                                 \n\
                            'library': '" TEST6_TLS_LIBRARY "',                   \n\
'ssl_certificate': '/yuneta/agent/certs/localhost.crt',             \n\
'ssl_certificate_key': '/yuneta/agent/certs/localhost.key',         \n\
                            'trace': false                          \n\
                        },                                          \n\
                        'url': '(^^__input_url__^^)',               \n\
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
                    'name': 'input', \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'input',                        \n\
                            'gclass': 'C_PROT_TCP4H',               \n\
                            'kw': {                                 \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'input',                \n\
                                    'gclass': 'C_TCP'               \n\
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
            'children': [                                            \n\
                {                                                   \n\
                    'name': 'output',                               \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                    \n\
                        {                                           \n\
                            'name': 'output',                       \n\
                            'gclass': 'C_PROT_TCP4H',               \n\
                            'kw': {                                 \n\
                            },                                      \n\
                            'children': [                            \n\
                                {                                   \n\
                                    'name': 'output',               \n\
                                    'gclass': 'C_TCP',              \n\
                                    'kw': {                         \n\
                                        'url':'tcps://127.0.0.1:" TEST6_PORT "', \n\
                                        'crypto': {                 \n\
                                            'library': '" TEST6_TLS_LIBRARY "',   \n\
                                            'ssl_allow_insecure_client': true,   \n\
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
            'name': '__output_side2__',                              \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                            \n\
                {                                                   \n\
                    'name': 'output2',                               \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                    \n\
                        {                                           \n\
                            'name': 'output2',                       \n\
                            'gclass': 'C_PROT_TCP4H',               \n\
                            'kw': {                                 \n\
                            },                                      \n\
                            'children': [                            \n\
                                {                                   \n\
                                    'name': 'output2',               \n\
                                    'gclass': 'C_TCP',              \n\
                                    'kw': {                         \n\
                                        'url':'tcps://127.0.0.1:" TEST6_PORT "', \n\
                                        'crypto': {                 \n\
                                            'library': '" TEST6_TLS_LIBRARY "',   \n\
                                            'ssl_allow_insecure_client': true,   \n\
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
    result += register_c_pepon();
    result += register_c_test6();

    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    // Avoid timer trace, too much information
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER), "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    // Samples of traces
    gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "identity-card", TRUE);
    gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "identity-card", TRUE);

    //gobj_set_gclass_trace(gclass_find_by_name(C_TEST6), "messages", TRUE);
    //gobj_set_gclass_trace(gclass_find_by_name(C_TEST6), "machine", TRUE);

    // gobj_set_gclass_trace(gclass_find_by_name(C_PEPON), "messages", TRUE);
    // gobj_set_gclass_trace(gclass_find_by_name(C_TESTON), "messages", TRUE);
    // gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_CLI), "ievents2", TRUE);
    // gobj_set_gclass_trace(gclass_find_by_name(C_IEVENT_SRV), "ievents2", TRUE);
    // gobj_set_gclass_trace(gclass_find_by_name(C_TCP), "traffic", TRUE);

    // Samples of global traces
    // gobj_set_gobj_trace(0, "create_delete", TRUE, 0);
    // gobj_set_gobj_trace(0, "create_delete2", TRUE, 0);
    // gobj_set_gobj_trace(0, "start_stop", TRUE, 0);
    // gobj_set_gobj_trace(0, "subscriptions", TRUE, 0);
    // gobj_set_gobj_trace(0, "machine", TRUE, 0);
    // gobj_set_gobj_trace(0, "ev_kw", TRUE, 0);
    // gobj_set_gobj_trace(0, "liburing", TRUE, 0);
    // gobj_set_gobj_trace(0, "liburing_timer", TRUE, 0);

    /*------------------------------*
     *  Start test
     *------------------------------*/
    /*
     *  The warnings and errors, in order: those of the TLS clients of the
     *  test (no validation of the test certificate), none of the restart
     */
    json_t *errors_list = json_pack("[{s:s}, {s:s}, {s:s}, {s:s}]",
        "msg", "TLS client WITHOUT server-certificate validation (MITM surface): set ssl_trusted_certificate or ssl_use_system_ca",
        "msg", "TLS peer certificate did NOT verify (accepted under VERIFY_OPTIONAL; set ssl_verify_mode=required to reject)",
        "msg", "TLS client WITHOUT server-certificate validation (MITM surface): set ssl_trusted_certificate or ssl_use_system_ca",
        "msg", "TLS peer certificate did NOT verify (accepted under VERIFY_OPTIONAL; set ssl_verify_mode=required to reject)"
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

    release_quarantine();   // before the memory check of the entry point
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*------------------------------*
     *  Captura salida logger
     *------------------------------*/
    /*
     *  Freed memory is poisoned (poison_alloc.c): a use of the ytls a
     *  restart freed reads poison, every run, instead of the block as it
     *  was
     */
    install_poison_allocators();

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
     *      To check
     *------------------------------------------------*/
    // gobj_set_deep_tracing(1);
    // set_auto_kill_time(6);

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

    release_quarantine();   // an entry point that ended before cleaning()

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
