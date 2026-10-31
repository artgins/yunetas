/****************************************************************************
 *          MAIN.C
 *
 *          Test: a C_PROT_HTTP_SR server and the peers it refuses
 *
 *          Tasks
 *          - Play a server (C_IOGATE, channels of C_PROT_HTTP_SR over C_TCP)
 *          - Requests pipelined past a drop: none delivered after it
 *          - A head too long: 431; a body too long: 413; each with ONE
 *            warning, and the connection dropped after timeout_linger
 *          - A peer that reads gets every answer of a long pipeline, in order
 *          - A refusal never overtakes the answer of the request before it
 *          - A peer that never reads its answers: the requests held at
 *            max_tx_queue, dropped after timeout_tx_stall or past max_rx_held
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_test1.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_prot_http_sr_" "test1"
#define APP_DOC         "Test C_PROT_HTTP_SR limits and drops"

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
        'i18n_domain': 'test_http_sr',                              \n\
        'trace_levels': {                                           \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
        '__input_side__.__json_config_variables__': {               \n\
            '__input_url__': 'tcp://0.0.0.0:7766',                 \n\
            '__input_host__': '0.0.0.0',                            \n\
            '__input_port__': '7766'                                \n\
        }                                                           \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'c_test1',                                      \n\
            'gclass': 'C_TEST1',                                    \n\
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
                            'kw': {                                 \n\
                                '__gclass_name__': 'C_CHANNEL',     \n\
                                '__disabled__': false,               \n\
                                'connected': false                  \n\
                            }                                       \n\
                        }                                           \n\
                    }                                               \n\
                }                                                   \n\
            ],                                                      \n\
            '[^^children^^]': {                                      \n\
                '__range__': [1,2],                                  \n\
                '__vars__': {                                        \n\
                },                                                  \n\
                '__content__': {                                     \n\
                    'name': '__input_port__-(^^__range__^^)',        \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                    \n\
                        {                                           \n\
                            'name': '__input_port__-(^^__range__^^)', \n\
                            'gclass': 'C_PROT_HTTP_SR',             \n\
                            'kw': {                                 \n\
                                'max_body_size': 1024,              \n\
                                'max_tx_queue': 4,                  \n\
                                'timeout_tx_stall': 500,            \n\
                                'max_rx_held': 65536,               \n\
                                'timeout_linger': 500               \n\
                            },                                      \n\
                            'children': [                            \n\
                                {                                   \n\
                                    'name': '__input_port__-(^^__range__^^)', \n\
                                    'gclass': 'C_TCP',              \n\
                                    'kw': {                         \n\
                                        'timeout_stop_tx': 1500     \n\
                                    }                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
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
    json_t *errors_list = json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
        "msg", "Starting yuno",
        "msg", "Playing yuno",
        "msg", "HTTP headers larger than their limit, message refused",    // step 2, ONE
        "msg", "HTTP body larger than its limit, message refused",         // step 3, ONE
        "msg", "HTTP peer does not read its answers, connection dropped",  // step 4, ONE
        "msg", "HTTP headers larger than their limit, message refused",    // step 8, ONE
        "msg", "HTTP peer does not read its answers, connection dropped",  // step 9, ONE
        "msg", "HTTP peer does not read its answers and keeps sending, connection dropped", // step 10, ONE
        "msg", "HTTP peer does not read its answers, connection dropped",  // step 13, ONE
        "msg", "tcp data sent while the connection closes, dropped",       // step 13, ONE
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
     *  A request that never arrives must fail the test, not hang it
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
