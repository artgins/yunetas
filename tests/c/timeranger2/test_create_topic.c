/****************************************************************************
 *          perf_yev_timer
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <gobj.h>
#include <timeranger2.h>
#include <stacktrace_with_bfd.h>
#include <yunetas_ev_loop.h>
#include <testing.h>

#define TEST_NAME   "create_topic"
#define TOPIC_NAME "topic_sample"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_t *yev_loop;
yev_event_t *yev_event_once;
yev_event_t *yev_event_periodic;
int wait_time = 1;
int times_once = 0;
int times_periodic = 0;

/***************************************************************************
 *              Test
 *  HACK: Use gobj_set_exit_code(-1) to set error
 ***************************************************************************/
int do_test(void)
{
    int result = 0;
    char file[PATH_MAX];

    /*
     *  Write the tests in ~/tests_yuneta/
     */
    const char *home = getenv("HOME");
    char path[PATH_MAX];
    build_path(path, sizeof(path), home, "tests_yuneta", NULL);
    rmrdir(path);
    mkrdir(path, 02770);

    /*-------------------------------------------------*
     *  Startup/create the timeranger db and a topic
     *-------------------------------------------------*/
    set_expected_results(
        "tr_"TEST_NAME"_check_tranger_startup", // test name
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s}]", // error's list
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic",
            "msg", "Creating topic_desc.json",
            "msg", "Creating topic_cols.json",
            "msg", "Creating topic_var.json"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i}",
        "path", path,
        "database", "tr_"TEST_NAME,
        "master", 1,
        "on_critical_error", 0
    );
    json_t *tranger = tranger2_startup(0, jn_tranger);

    json_t *topic = tranger2_create_topic(
        tranger,
        TOPIC_NAME,  // topic name
        "id",           // pkey
        "",             // tkey
        0,              // system_latch
        json_pack("{s:s, s:s}", // jn_cols, owned
            "id", "",
            "address", ""
        ),
        0
    );
    if(!topic) {
        tranger2_shutdown(tranger);
        return -1;
    }
    result += test_json(NULL);  // NULL: we want to check only the logs

    /*------------------------------------*
     *  Check __timeranger2__.json file
     *------------------------------------*/
    build_path(file, sizeof(file), path, "tr_"TEST_NAME, "__timeranger2__.json", NULL);
    if(1) {
        char expected[]= "\
        { \
          'filename_mask': '%Y-%m-%d', \
          'rpermission': 432, \
          'xpermission': 1528 \
        } \
        ";
        set_expected_results(
            "tr_"TEST_NAME"_check__timeranger2__.json",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            NULL,
            TRUE
        );
        result += test_file(file);
    }

    /*------------------------------------*
     *  Check "topic_desc.json" file
     *------------------------------------*/
    build_path(file, sizeof(file), path, "tr_"TEST_NAME, TOPIC_NAME, "topic_desc.json", NULL);
    if(1) {
        char expected[]= "\
        { \
          'topic_name': 'topic_sample', \
          'pkey': 'id', \
          'tkey': '', \
          'system_flag': 1 \
        } \
        ";

        set_expected_results(
            "tr_"TEST_NAME"_check_topic_desc.json",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            NULL,
            TRUE
        );
        result += test_file(file);
    }

    /*------------------------------------*
     *  Check "topic_cols.json" file
     *------------------------------------*/
    build_path(file, sizeof(file), path, "tr_"TEST_NAME, TOPIC_NAME, "topic_cols.json", NULL);
    if(1) {
        char expected[]= "\
        { \
          'id': '', \
          'address': '' \
        } \
        ";

        set_expected_results(
            "tr_"TEST_NAME"_check_topic_cols.json",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            NULL,
            TRUE
        );
        result += test_file(file);
    }

    /*------------------------------------*
     *  Check "topic_var.json" file
     *------------------------------------*/
    build_path(file, sizeof(file), path, "tr_"TEST_NAME, TOPIC_NAME, "topic_var.json", NULL);
    if(1) {
        char expected[]= "\
        { \
        } \
        ";

        set_expected_results(
            "tr_"TEST_NAME"_check_topic_var.json",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            NULL,
            TRUE
        );
        result += test_file(file);
    }

    /*------------------------------------------*
     *  Check tranger memory with topic opened
     *------------------------------------------*/
    if(1) {
        char expected[]= "\
        { \
            'path': 'tests_yuneta', \
            'database': 'tr_create_topic', \
            'filename_mask': '%Y-%m-%d', \
            'xpermission': 1528, \
            'rpermission': 432, \
            'on_critical_error': 0, \
            'master': true, \
            'gobj': 0, \
            'directory': 'tests_yuneta/tr_create_topic', \
            'fd_opened_files': { \
                '__timeranger2__.json': 99999 \
            }, \
            'topics': { \
                'topic_sample': { \
                    'topic_name': 'topic_sample', \
                        'pkey': 'id', \
                        'tkey': '', \
                        'system_flag': 1, \
                        'cols': { \
                        'id': '', \
                            'address': '' \
                    }, \
                    'directory': 'tests_yuneta/tr_create_topic/topic_sample', \
                        '__last_rowid__': 0, \
                        'topic_idx_fd': 99999, \
                        'fd_opened_files': {}, \
                    'file_opened_files': {}, \
                    'lists': [] \
                } \
            } \
        } \
        ";

        const char *ignore_keys[]= {
            "path",
            "directory",
            "__timeranger2__.json",
            "topic_idx_fd",
            NULL
        };
        set_expected_results(
            "tr_"TEST_NAME"_check_tranger_mem1",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger));
    }

    /*------------------------*
     *      Close topic
     *------------------------*/
    set_expected_results(
        "tr_"TEST_NAME"_check_close_topic", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    tranger2_close_topic(tranger, TOPIC_NAME);

    result += test_json(NULL);  // NULL: we want to check only the logs

    /*------------------------------------------*
     *  Check tranger memory with topic closed
     *------------------------------------------*/
    if(1) {
        char expected[]= "\
        { \
          'path': 'tests_yuneta', \
          'database': 'tr_create_topic', \
          'filename_mask': '%Y-%m-%d', \
          'xpermission': 1528, \
          'rpermission': 432, \
          'on_critical_error': 0, \
          'master': true, \
          'gobj': 0, \
          'directory': 'tests_yuneta/tr_create_topic', \
          'fd_opened_files': { \
            '__timeranger2__.json': 9999 \
          }, \
          'topics': {} \
        } \
        ";

        const char *ignore_keys[]= {
            "path",
            "directory",
            "__timeranger2__.json",
            "topic_idx_fd",
            NULL
        };
        set_expected_results(
            "tr_"TEST_NAME"_check_tranger_mem2",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger));
    }

    /*------------------------------------------*
     *  Check re-open tranger as master
     *------------------------------------------*/
    if(1) {
        char expected[]= "\
        { \
          'path': 'tests_yuneta', \
          'database': 'tr_create_topic', \
          'filename_mask': '%Y-%m-%d', \
          'xpermission': 1528, \
          'rpermission': 432, \
          'on_critical_error': 0, \
          'master': false, \
          'gobj': 0, \
          'directory': 'tests_yuneta/tr_create_topic', \
          'fd_opened_files': { \
            '__timeranger2__.json': 9999 \
          }, \
          'topics': {} \
        } \
        ";
        const char *ignore_keys[]= {
            "path",
            "directory",
            "__timeranger2__.json",
            "topic_idx_fd",
            NULL
        };

        set_expected_results(
            "tr_"TEST_NAME"_check_tranger_reopen_as_master",      // test name
            json_pack("[{s:s},{s:s}]", // error's list
                "msg", "Cannot open json file",
                "msg", "Open as not master, __timeranger2__.json locked"
            ),
            string2json(helper_quote2doublequote(expected), TRUE),
            ignore_keys,
            TRUE
        );

        json_t *jn_tr = json_pack("{s:s, s:s, s:b, s:i}",
            "path", path,
            "database", "tr_"TEST_NAME,
            "master", 1,
            "on_critical_error", 0
        );
        json_t *tr = tranger2_startup(0, jn_tr);

        result += test_json(json_incref(tr));

        set_expected_results(
            "tr_"TEST_NAME"_tranger_shutdown", // test name
            NULL,   // error's list, It must not be any log error
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        tranger2_shutdown(tr);
        result += test_json(NULL);  // NULL: we want to check only the logs
    }

    /*
     *  Shutdown timeranger
     */
    set_expected_results(
        "tr_"TEST_NAME"_tranger_shutdown", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    tranger2_shutdown(tranger);
    result += test_json(NULL);  // NULL: we want to check only the logs

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*----------------------------------*
     *      Startup gobj system
     *----------------------------------*/
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gobj_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

//    gobj_set_deep_tracing(2);           // TODO TEST
//    gobj_set_global_trace(0, TRUE);     // TODO TEST

    unsigned long memory_check_list[] = {1627, 1628, 1769, 0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

    init_backtrace_with_bfd(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_bfd);

    gobj_start_up(
        argc,
        argv,
        NULL, // jn_global_settings
        NULL, // startup_persistent_attrs
        NULL, // end_persistent_attrs
        0,  // load_persistent_attrs
        0,  // save_persistent_attrs
        0,  // remove_persistent_attrs
        0,  // list_persistent_attrs
        NULL, // global_command_parser
        NULL, // global_stats_parser
        NULL, // global_authz_checker
        NULL, // global_authenticate_parser
        64*1024L,    // max_block, largest memory block
        256*1024L   // max_system_memory, maximum system memory
    );

    yuno_catch_signals();

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    /*------------------------------*
     *  Captura salida logger
     *------------------------------*/
    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);
    gobj_log_add_handler(
        "test_stdout",
        "stdout",
        LOG_OPT_UP_WARNING,
        0
    );

    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        &yev_loop
    );

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    int result = do_test();

    /*--------------------------------*
     *  Stop the event loop
     *--------------------------------*/
    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    return result;
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int xtimes_once = 0;
    xtimes_once++;
    yev_loop->running = 0;
    if(xtimes_once > 1) {
        exit(-1);
    }
}

PUBLIC void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    memset(&sigIntHandler, 0, sizeof(sigIntHandler));
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGALRM, &sigIntHandler, NULL);   // to debug in kdevelop
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);    // ctrl+c
}