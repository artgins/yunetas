/****************************************************************************
 *          MAIN.C
 *
 *          Test: archive_sender ships the batches of a timeranger2 archive
 *
 *          Tasks
 *          - Two stores, each archived into its own archive
 *          - native: archive_sender -> archive_keeper over
 *            tcp://127.0.0.1:7755: the batches found by its scan, one told
 *            by its mark, one the keeper already has
 *          - rsync: archive_sender_rsync -> a local directory, with room
 *            for one batch of two: the oldest dropped
 *          - A batch of the same name planted in the backup first: the
 *            keeper refuses, keeps it, and the sender keeps its own
 *          - Mutual TLS (tcps): a sender with the client certificate of
 *            another node, and one without a certificate, naming
 *            test_node: refused, the node is bound to the certificate
 *          - Everything shipped is whole at the other end, and gone here
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_test1.h"
#include "test_pki.h"
#include "c_archive_sender.h"
#include "c_archive_keeper.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_archive_shipping"
#define APP_DOC         "Test archive_sender and archive_keeper"

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
        'trace_levels': {                                           \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'c_test1',                                      \n\
            'gclass': 'C_TEST1',                                    \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'archive_keeper',                               \n\
            'gclass': 'C_ARCHIVE_KEEPER',                           \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
                'allowed_nodes': ['test_node']                      \n\
            }                                                       \n\
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
                        'url': 'tcps://127.0.0.1:7755',             \n\
                        'crypto': {                                 \n\
                            'ssl_certificate': '" PKI_SERVER "',    \n\
                            'ssl_certificate_key': '" PKI_SERVER_KEY "',\n\
                            'ssl_trusted_certificate': '" PKI_CA "',\n\
                            'ssl_verify_mode': 'optional'           \n\
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
                '__range__': [1,4],                                 \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': 'input-(^^__range__^^)',                \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'input-(^^__range__^^)',        \n\
                            'gclass': 'C_PROT_TCP4H',               \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'input-(^^__range__^^)',\n\
                                    'gclass': 'C_TCP'               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'archive_sender',                               \n\
            'gclass': 'C_ARCHIVE_SENDER',                           \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
                'transport': 'native',                              \n\
                'node_name': 'test_node',                           \n\
                'chunk_size': 100,                                  \n\
                'window': 2                                         \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__output_side__',                              \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'output',                               \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'output',                       \n\
                            'gclass': 'C_PROT_TCP4H',               \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'output',               \n\
                                    'gclass': 'C_TCP',              \n\
                                    'kw': {                         \n\
                                        'url': 'tcps://127.0.0.1:7755',\n\
                                        'crypto': {                 \n\
                                            'ssl_trusted_certificate': '" PKI_CA "',\n\
                                            'ssl_certificate': '" PKI_NODE "',\n\
                                            'ssl_certificate_key': '" PKI_NODE_KEY "',\n\
                                            'ssl_server_name': 'localhost'\n\
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
            'name': 'archive_sender_rsync',                         \n\
            'gclass': 'C_ARCHIVE_SENDER',                           \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
                'transport': 'rsync',                               \n\
                'node_name': 'test_node',                           \n\
                'rsync_timeout': 30                                 \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'archive_sender_other',                         \n\
            'gclass': 'C_ARCHIVE_SENDER',                           \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
                'transport': 'native',                              \n\
                'node_name': 'test_node',                           \n\
                'output_service': '__output_other__',               \n\
                'retry_period': 3600                                \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__output_other__',                             \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'output',                               \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'output',                       \n\
                            'gclass': 'C_PROT_TCP4H',               \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'output',               \n\
                                    'gclass': 'C_TCP',              \n\
                                    'kw': {                         \n\
                                        'url': 'tcps://127.0.0.1:7755',\n\
                                        'crypto': {                 \n\
                                            'ssl_trusted_certificate': '" PKI_CA "',\n\
                                            'ssl_certificate': '" PKI_OTHER "',\n\
                                            'ssl_certificate_key': '" PKI_OTHER_KEY "',\n\
                                            'ssl_server_name': 'localhost'\n\
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
            'name': 'archive_sender_nocert',                        \n\
            'gclass': 'C_ARCHIVE_SENDER',                           \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
                'transport': 'native',                              \n\
                'node_name': 'test_node',                           \n\
                'output_service': '__output_nocert__',              \n\
                'retry_period': 3600                                \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__output_nocert__',                            \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'output',                               \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'output',                       \n\
                            'gclass': 'C_PROT_TCP4H',               \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'output',               \n\
                                    'gclass': 'C_TCP',              \n\
                                    'kw': {                         \n\
                                        'url': 'tcps://127.0.0.1:7755',\n\
                                        'crypto': {                 \n\
                                            'ssl_trusted_certificate': '" PKI_CA "',\n\
                                            'ssl_server_name': 'localhost'\n\
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
    result += register_c_test1();
    result += register_c_archive_sender();
    result += register_c_archive_keeper();

    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    // Avoid timer trace, too much information
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER), "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

        
    // Samples of global traces
    gobj_set_gobj_trace(0, "create_delete", TRUE, 0);
    gobj_set_gobj_trace(0, "start_stop", TRUE, 0);
    // gobj_set_gobj_trace(0, "machine", TRUE, 0);
    // gobj_set_gobj_trace(0, "liburing", TRUE, 0);

    /*------------------------------*
     *  Start test
     *------------------------------*/
    json_t *errors_list = json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
        "msg", "Starting yuno",
        "msg", "Playing yuno",
        "msg", "Creating __timeranger2__.json",
        "msg", "Creating topic",
        "msg", "Creating __timeranger2__.json",
        "msg", "Creating topic",
        "msg", "Batch removed WITHOUT being shipped: the archive is over max_archive_bytes",
        "msg", "A batch of this name is here with other content: kept as it is, nothing overwritten",
        "msg", "Batch refused",
        "msg", "Batch NOT shipped: tried again later, after the others",
        "msg", "A sender names a node that is not the name of its client certificate",
        "msg", "Batch refused",
        "msg", "Batch NOT shipped: tried again later, after the others",
        "msg", "Batch refused",
        "msg", "Batch NOT shipped: tried again later, after the others",
        "msg", "Exit to die",
        "msg", "Pausing yuno",
        "msg", "Yuno stopped, gobj end"
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
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);


    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

    /*
     *  A batch that never ends must fail the test, not hang it
     */
    set_auto_kill_time(30);

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
