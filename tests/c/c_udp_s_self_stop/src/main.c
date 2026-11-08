/****************************************************************************
 *          MAIN.C
 *
 *          Main of test_c_udp_s_self_stop: the stops C_UDP_S makes by
 *          itself end, and are said; a secure url is refused; a listen
 *          that fails is tried again, and the yuno goes on
 *
 *          The red of a listen that fails was an EXIT with code 0, which
 *          ctest would take for a pass: an atexit() handler turns an exit
 *          before the end of the test into code 1.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdlib.h>
#include <unistd.h>
#include <yunetas.h>
#include "c_test_udp_self_stop.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_udp_s_self_stop"
#define APP_DOC         "Test the self-stops of C_UDP_S and its refusal of udps://"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define DEBUG_MEMORY            FALSE
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
        'timeout_periodic': 100,                                    \n\
        'service_descriptor': {                                     \n\
        },                                                          \n\
        'trace_levels': {                                           \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'test_udp_self_stop',                                     \n\
            'gclass': 'C_TEST_UDP_SELF_STOP',                               \n\
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
    result += register_c_test_udp_self_stop();

    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    // gobj_set_gclass_trace(gclass_find_by_name(C_UDP_S), "traffic", TRUE);
    // gobj_set_gobj_trace(0, "machine", TRUE, 0);
    // gobj_set_gobj_trace(0, "liburing", TRUE, 0);

    /*------------------------------*
     *  Start test
     *------------------------------*/
    set_expected_results( // Check that no logs happen
        APP_NAME, // test name
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},"
                  "{s:s,s:s},{s:s,s:s,s:i},{s:s,s:s},{s:s},{s:s,s:s},{s:s},{s:s},"
                  "{s:s},{s:s,s:s},{s:s},{s:s},{s:s},{s:s},"
                  "{s:s},{s:s},{s:s}]", // errors_list
            "msg", "Starting yuno",
            "msg", "UDP listening ...",
            "msg", "Playing yuno",
            "msg", "Cannot start event: gbuffer WITHOUT space to read",
            "msg", "UDP: the read cannot be started again, the server stops listening",
            "msg", "A secure url (udps://) is not supported by C_UDP_S: there is no DTLS",
            "msg", "TEST: the self-stop of C_UDP_S ended, and udps:// was refused",
            "msg", "bind() FAILED", "__priority__", "WARNING",
            "msg", "Listen failed: not listening, trying again", "__priority__", "ERROR", "errno", 98,
            "msg", "bind() FAILED", "__priority__", "WARNING",   // its first try, said by nobody else
            "msg", "UDP listening ...",
            "msg", "Listening again after a failed listen", "__priority__", "INFO",
            "msg", "URL NULL",
            "msg", "TEST: a failed listen of C_UDP_S listens at its try, and an empty url is refused",
            "msg", "Cannot start event: gbuffer WITHOUT space to read",
            "msg", "Cannot start the read of the UDP server: not listening, trying again", "__priority__", "ERROR",
            "msg", "Cannot start event: gbuffer WITHOUT space to read",     // its first try, said by nobody else
            "msg", "UDP listening ...",
            "msg", "Listening again after a failed listen",
            "msg", "TEST: a C_UDP_S whose read could not be started listens at its try",
            "msg", "Exit to die",
            "msg", "Pausing yuno",
            "msg", "Yuno stopped, gobj end"
        ),
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
    result += test_json(NULL);  //  NULL: we want to check only the logs
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
PRIVATE BOOL reached_end = FALSE;

PRIVATE void check_reached_end(void)
{
    if(!reached_end) {
        printf("<-- %sTEST FAILED%s: %s, the yuno exited before the end of the test\n",
            On_Red BWhite, Color_Off, APP_NAME);
        fflush(stdout);
        _exit(1);
    }
}

int main(int argc, char *argv[])
{
    atexit(check_reached_end);

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

    /*------------------------------------------------*
     *      To check
     *------------------------------------------------*/
    // gobj_set_deep_tracing(1);
    set_auto_kill_time(10);    // a stop that waits for ever (7.25.4) must not hang the suite

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
    reached_end = TRUE;

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
