/****************************************************************************
 *          main_set_url_running.c
 *
 *          A url changed while the SMTP session runs is taken when it
 *          starts again: the service starts with credentials and a url
 *          where nobody listens, set-url-from gives it the url of the fake
 *          server, and after a pause and a play the email is delivered
 *          THERE. Up to 7.25.20 the session kept the url it was created
 *          with until the yuno was restarted.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <yunetas.h>
#include <c_smtp_session.h>
#include <c_emailsender.h>
#include "c_fake_smtp.h"
#include "c_test_emailsender.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_emailsender_set_url_running"
#define APP_DOC         "set-url-from reaches a running SMTP session at its next start"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0       // use default
#define MEM_MAX_BLOCK           0       // use default
#define MEM_SUPERBLOCK          0       // use default
#define MEM_MAX_SYSTEM_MEMORY   0       // use default

#define BASE    "/tmp/test_emailsender_set_url_running"

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
        'work_dir': '"BASE"',                                       \n\
        'domain_dir': 'realm',                                      \n\
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
            'name': 'emailsender',                                  \n\
            'gclass': 'C_EMAILSENDER',                              \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'username': 'user',                                 \n\
                'password': 'secret',                               \n\
                'url': 'tcp://127.0.0.1:7829',                      \n\
                'from': 'sender@example.com',                       \n\
                'timeout_inactivity': 30000,                        \n\
                'tranger_path': '"BASE"/store',                     \n\
                'tranger_database': 'emailsender',                  \n\
                'topic_emails_queue': 'emails_queue',               \n\
                'topic_emails_failed': 'emails_failed',             \n\
                'tkey': 'tm'                                        \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__input_side__',                               \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'c_test',                                       \n\
            'gclass': 'C_TEST_EMAILSENDER',                         \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'scenario': 'set_url',                              \n\
                'server_service': 'fake_smtp_server',               \n\
                'smtp_url': 'tcp://127.0.0.1:7824'                  \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': 'fake_smtp_server',                             \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'fake_smtp_port',                       \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://127.0.0.1:7824',              \n\
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
                    'name': 'fake_smtp_channel',                    \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'fake_smtp',                    \n\
                            'gclass': 'C_FAKE_SMTP',                \n\
                            'kw': {                                 \n\
                                'auth_replies': ['235 2.7.0 Authentication successful'], \n\
                                'die_on_delivery': true             \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'gclass': 'C_TCP'               \n\
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
int result = 0;

/*
 *  A yuno that exits by itself (LOG_OPT_EXIT_ZERO, exit(0)) never reaches
 *  cleaning(), and its exit code would read as a pass.
 */
PRIVATE BOOL test_finished = FALSE;

/*
 *  Every ERROR logged, by its msg: the list of expected logs says what was
 *  logged, not at which level, and a failure caused by the peer is a
 *  WARNING, not an ERROR (the house decoder-severity rule).
 */
PRIVATE json_t *error_msgs = NULL;
PRIVATE int errors_outside_the_test = 0;

PRIVATE int capture_error_write(void *v, int priority, const char *bf, size_t len)
{
    if(priority > LOG_ERR) {
        return 0;
    }
    if(!error_msgs) {
        errors_outside_the_test++;
        return 0;
    }
    json_t *jn_log = string2json(bf, FALSE);
    json_array_append_new(error_msgs,
        json_string(kw_get_str(0, jn_log, "msg", "?", 0))
    );
    JSON_DECREF(jn_log)
    return 0;
}

PRIVATE void exit_guard(void)
{
    if(!test_finished) {
        printf("<-- TEST FAILED: %s exited before the end of the test\n", APP_NAME);
        fflush(stdout);
        _exit(1);
    }
}

/***************************************************************************
 *  HACK This function is executed on yunetas environment (mem, log, paths)
 *  BEFORE creating the yuno
 ***************************************************************************/
static int register_yuno_and_more(void)
{
    int result = 0;

    rmrdir(BASE);

    error_msgs = json_array();

    /*--------------------*
     *  Register gclass
     *--------------------*/
    result += register_c_smtp_session();
    result += register_c_emailsender();
    result += register_c_fake_smtp();
    result += register_c_test_emailsender();

    /*------------------------------------------------*
     *          Traces
     *------------------------------------------------*/
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER), "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    // gobj_set_gclass_trace(gclass_find_by_name(C_SMTP_SESSION), "smtp", TRUE);
    // gobj_set_global_trace("machine", TRUE);

    /*------------------------------*
     *  Start test
     *------------------------------*/
    json_t *errors_list = json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s, s:s, s:s}, {s:s}, {s:s}, {s:s}]",
        "msg", "Starting yuno",
        "msg", "Playing yuno",
        "msg", "Creating __timeranger2__.json",
        "msg", "Creating topic",
        "msg", "Creating topic",
        "msg", "set-url-from says the url waits for a play",
        "msg", "Fake smtp: AUTH answered",
        "msg", "Fake smtp: message delivered",
        "msg", "email sent", "to", "reader@example.com", "cc", "copy@example.com",
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
    test_finished = TRUE;

    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, APP_NAME)

    result += test_json(NULL);  // NULL: we want to check only the logs

    json_t *expected_errors = json_pack("[]");
    if(!json_equal(error_msgs, expected_errors) || errors_outside_the_test) {
        char *s_got = json2uglystr(error_msgs);
        char *s_expected = json2uglystr(expected_errors);
        printf("<-- %sERRORS NOT AS EXPECTED%s: %s\n      logged as ERROR: %s\n      expected:        %s\n",
            On_Red BWhite, Color_Off, APP_NAME, s_got, s_expected
        );
        GBMEM_FREE(s_got)
        GBMEM_FREE(s_expected)
        result += -1;
    }
    JSON_DECREF(expected_errors)
    JSON_DECREF(error_msgs)
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    atexit(exit_guard);

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
        "errors",           // handler_name
        0,                  // close_fn
        capture_error_write,// write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_errors", "errors", LOG_OPT_UP_ERROR, 0);

    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

    /*------------------------------------------------*
     *      To check
     *------------------------------------------------*/
    set_auto_kill_time(15);

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
