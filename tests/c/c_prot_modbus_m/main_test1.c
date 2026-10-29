/****************************************************************************
 *          MAIN.C
 *
 *          Test: a C_PROT_MODBUS_M master over TCP against a raw slave
 *
 *          Tasks
 *          - Play a raw server (C_PROT_RAW) that answers Modbus TCP by hand
 *          - Create the master (C_PROT_MODBUS_M) with two slaves
 *          - Cycle 1: a block and a whole slave answer exceptions: only
 *            what was read is published, at once; a write is queued
 *          - Cycle 2: the write was sent once, the exceptions go on (not
 *            logged again); a block answered with a write echo fails
 *          - Between them, the requests: answers of another function, a late
 *            echo of a write hidden in the traffic trace, half a frame after
 *            an answer, and an MBAP length of 65535 that drops the
 *            connection
 *          - Cycle 3: every block answers: everything is published, and
 *            each block that was failing is logged as answered again:
 *            shutdown
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include <c_prot_modbus_m.h>
#include "c_test1.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_prot_modbus_m_" "test1"
#define APP_DOC         "Test C_PROT_MODBUS_M: what a cycle publishes, exceptions, writes"

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
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
        '__input_side__.__json_config_variables__': {               \n\
            '__input_url__': 'tcp://0.0.0.0:7759',                  \n\
            '__input_host__': '0.0.0.0',                            \n\
            '__input_port__': '7759'                                \n\
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
                '__range__': [1,1],                                  \n\
                '__vars__': {                                        \n\
                },                                                  \n\
                '__content__': {                                     \n\
                    'name': '__input_port__-(^^__range__^^)',        \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                    \n\
                        {                                           \n\
                            'name': '__input_port__-(^^__range__^^)', \n\
                            'gclass': 'C_PROT_RAW',                 \n\
                            'kw': {                                 \n\
                            },                                      \n\
                            'children': [                            \n\
                                {                                   \n\
                                    'name': '__input_port__-(^^__range__^^)', \n\
                                    'gclass': 'C_TCP'               \n\
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
 *  Every log line, traces included: a write value must not show in it.
 *  The values are written to be found as their hex dump: 0x7E5F (function
 *  06) and 0x5A3C 0x6B4D (function 16). The one exception is the dump of
 *  what the transport RECEIVES (" ⏪ "): the echo of an 06 carries its value,
 *  and received bytes nobody can mark as secret.
 ***************************************************************************/
PRIVATE int secrets_shown = 0;

PRIVATE int secret_scan_write(void *h, int priority, const char *bf, size_t len)
{
    char line[8*1024];
    size_t ln = len < sizeof(line) - 1? len : sizeof(line) - 1;
    memcpy(line, bf, ln);
    line[ln] = 0;

    if(strstr(line, "5A 3C 6B")) {
        secrets_shown++;
        printf("%sERROR --> the value of a write (16) in a log line%s\n", On_Red BWhite, Color_Off);
    }
    if(strstr(line, "7E 5F") && !strstr(line, " ⏪ ")) {
        secrets_shown++;
        printf("%sERROR --> the value of a write (06) in a log line%s\n", On_Red BWhite, Color_Off);
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
    result += register_c_prot_modbus_m();
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
    // gobj_set_gclass_trace(gclass_find_by_name(C_PROT_MODBUS_M), "polling", TRUE);
    // gobj_set_gclass_trace(gclass_find_by_name(C_PROT_MODBUS_M), "traffic", TRUE);

    /*------------------------------*
     *  Start test
     *------------------------------*/
    json_t *errors_list = json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
        "msg", "Starting yuno",
        "msg", "Playing yuno",
        "msg", "Allocating Modbus Array of 20 B (20) bytes, 2 slaves",
        "msg", "Data filled",
        "msg", "Modbus request refused: not connected", // the request sent before connecting
        "msg", "modbus exception",               // slave 1, block 1: once, not every cycle
        "msg", "modbus exception",               // slave 2: once
        "msg", "Modbus request refused",         // address "abc"
        "msg", "Modbus request refused",         // address null
        "msg", "Modbus request refused",         // address "010"
        "msg", "Modbus request refused",         // address 20.9
        "msg", "Modbus request refused",         // id "x"
        "msg", "Modbus request refused",         // value "12ab"
        "msg", "Modbus request refused",         // a value that does not fit
        "msg", "Modbus request refused",         // a read past 65535
        "msg", "Modbus request refused",         // a write past 65535
        "msg", "modbus exception",               // the write the slave refuses
        "msg", "Modbus Timeout",                 // the read answered late
        "msg", "Modbus frame dropped: not the answer awaited", // its late answer
        "msg", "Modbus answer of a request does not match it", // the MBAP of length 3
        "msg", "Modbus answer of another function", // the exception 0x84 to a 03
        "msg", "Modbus Timeout",                 // the write answered late
        "msg", "Modbus frame dropped: not the answer awaited", // its late echo
        "msg", "Modbus frame dropped: not the answer awaited", // the half frame after an answer
        "msg", "Modbus TCP: bad MBAP length, the connection is dropped", // the MBAP length 65535
        "msg", "Modbus request NOT confirmed, the connection dropped before its response", // the read after it
        "msg", "Modbus answer of another function", // cycle 2: block 1 answered with 06
        "msg", "Modbus block answered again",    // slave 1, block 1
        "msg", "Modbus block answered again",    // slave 2
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

    if(secrets_shown) {
        printf("%sERROR --> %d log lines show the value of a write%s\n",
            On_Red BWhite, secrets_shown, Color_Off
        );
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

    gobj_log_register_handler(
        "secret_scan",      // handler_name
        0,                  // close_fn
        secret_scan_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("secret_scan", "secret_scan", LOG_OPT_ALL, 0);


    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

    /*
     *  A poll that never resumes must fail the test, not hang it
     */
    set_auto_kill_time(20);

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
