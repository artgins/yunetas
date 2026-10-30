/****************************************************************************
 *          MAIN.C
 *
 *          Test: a C_UART on a pty, hosted by the test
 *
 *          Tasks
 *          - A write of 64 KB, more than the line takes at once: all of it
 *            goes, in order, and EV_TX_READY once
 *          - EV_DROP: the tty closed, EV_DISCONNECTED, and opened again
 *          - A write the line does not take, and a stop: the stop ends;
 *            again, a write more than max_tx_queue, and a destroy right
 *            after the stop
 *          - A destroy, and a start, in ST_WAIT_STOPPED
 *          - A host that destroys it in the EV_DISCONNECTED of a stop
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_test5.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_prot_modbus_m_" "test5"
#define APP_DOC         "Test C_UART on a pty: its writes, its drop, its stop and destroy"

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
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'c_test5',                                      \n\
            'gclass': 'C_TEST5',                                    \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'children': [                                            \n\
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
    result += register_c_test5();

    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    // Avoid timer trace, too much information
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER), "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    // gobj_set_gobj_trace(0, "machine", TRUE, 0);
    // gobj_set_gclass_trace(gclass_find_by_name(C_UART), "connections", TRUE);
    // gobj_set_gclass_trace(gclass_find_by_name(C_UART), "traffic", TRUE);

    /*------------------------------*
     *  Start test
     *------------------------------*/
    /*
     *  The opens of the C_UARTs, the start refused in ST_WAIT_STOPPED, and
     *  the destroy of a host inside the EV_DISCONNECTED of a stop, which the
     *  kernel says is wrong: nothing more
     */
    json_t *errors_list = json_array();
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "Starting yuno"));
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "Playing yuno"));
    // uart1: open, open again after the drop, the stop, started again
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "Start Uart"));
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "tty opened"));
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "tty opened"));
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "Start Uart"));
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "tty opened"));
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "uart tx queue full, the oldest write dropped"));
    // uart2: destroyed in ST_WAIT_STOPPED
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "Start Uart"));
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "tty opened"));
    // uart3: started in ST_WAIT_STOPPED
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "Start Uart"));
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "tty opened"));
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "Initial wrong task state"));
    // uart4: destroyed by its host in the EV_DISCONNECTED of a stop
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "Start Uart"));
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "tty opened"));
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "Destroying a gobj while its tree is publishing"));
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "Exit to die"));
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "Pausing yuno"));
    json_array_append_new(errors_list, json_pack("{s:s}", "msg", "Yuno stopped, gobj end"));

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
     *  A phase that never ends must fail the test, not hang it
     */
    set_auto_kill_time(45);

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
