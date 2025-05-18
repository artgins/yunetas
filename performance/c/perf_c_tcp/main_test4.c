/****************************************************************************
 *          MAIN.C
 *
 *          Test: Use pepon as server and test interchange of messages
 *
  *          Tasks
 *          - Play pepon as server with echo
 *          - Open __out_side__ (teston)
 *          - On open (pure cli connected to pepon), send a Hola message
 *          - On receiving the message re-send again
 *          - On 180000 received messages shutdown
 *
 *  Performance 18-Nov-2024 in my machine
 *      # TIME C_TEST4^c_test4 (count: 180000): 5.033485 seconds, 35760 op/sec
 *
 *          Copyright (c) 2024 by ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "common/c_pepon.h"
#include "c_test4.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "perf_tcp_" "test4"
#define APP_DOC         "Test C_TCP"

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
            'C_TCP': ['connections'],                               \n\
            'C_TCP_S': ['listen', 'not-accepted', 'accepted']       \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
        '__input_side__.__json_config_variables__': {               \n\
            '__input_url__': 'tcp://0.0.0.0:7778',                  \n\
            '__input_host__': '0.0.0.0',                            \n\
            '__input_port__': '7778'                                \n\
        }                                                           \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'c_test4',                                      \n\
            'gclass': 'C_TEST4',                                    \n\
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
                        'url': '(^^__input_url__^^)',               \n\
                        'child_tree_filter': {                      \n\
                            'op': 'find',                           \n\
                            'kw': {                                 \n\
                                '__prefix_gobj_name__': '(^^__input_port__^^)-', \n\
                                '__gclass_name__': 'C_CHANNEL',     \n\
                                '__disabled__': false,              \n\
                                'connected': false                  \n\
                            }                                       \n\
                        }                                           \n\
                    }                                               \n\
                }                                                   \n\
            ],                                                      \n\
            '[^^children^^]': {                                      \n\
                '__range__': [[1,1]],                               \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': '(^^__input_port__^^)-(^^__range__^^)', \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                    \n\
                        {                                           \n\
                            'name': '(^^__input_port__^^)-(^^__range__^^)', \n\
                            'gclass': 'C_PROT_TCP4H',               \n\
                            'kw': {                                 \n\
                            }                                       \n\
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
                                        'url':'tcp://127.0.0.1:7778' \n\
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
    result += register_c_test4();

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

    // gobj_set_gclass_trace(gclass_find_by_name(C_TEST4), "messages", true);
    // gobj_set_gclass_trace(gclass_find_by_name(C_TEST4), "machine", true);

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

    return result;
}

/***************************************************************************
 *  HACK This function is executed on yunetas environment (mem, log, paths)
 *  BEFORE creating the yuno
 ***************************************************************************/
static void cleaning(void)
{
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
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_UP_WARNING, 0);

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
    // set_auto_kill_time(4);

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
