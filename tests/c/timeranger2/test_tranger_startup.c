/****************************************************************************
 *          test_create_topic.c
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <gobj.h>
#include <timeranger2.h>
#include <stacktrace_with_bfd.h>
#include <yunetas_ev_loop.h>
#include <testing.h>

/***************************************************************
 *              Constants
 ***************************************************************/
#define DATABASE    "tr_tranger_startup"
#define TOPIC_NAME  "topic_sample"

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
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    char path_topic[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    build_path(path_topic, sizeof(path_topic), path_database, TOPIC_NAME, NULL);

    /*-------------------------------------------------*
     *      Startup the timeranger db
     *-------------------------------------------------*/
    set_expected_results(
        "tr__check_tranger_startup", // test name
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
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", 0
    );
    json_t *tranger = tranger2_startup(0, jn_tranger, 0);

    /*------------------------------------*
     *  Check __timeranger2__.json file
     *------------------------------------*/
    build_path(file, sizeof(file), path_database, "__timeranger2__.json", NULL);
    if(1) {
        char expected[]= "\
        { \
          'filename_mask': '%Y-%m-%d', \
          'rpermission': 432, \
          'xpermission': 1528 \
        } \
        ";
        set_expected_results(
            "check__timeranger2__.json",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            NULL,
            TRUE
        );
        result += test_json_file(file, result);
    }

    /*-------------------------------------------------*
     *      Create a topic
     *-------------------------------------------------*/
    json_t *topic = tranger2_create_topic(
        tranger,
        TOPIC_NAME,  // topic name
        "id",           // pkey
        "",             // tkey
        NULL,           // jn_topic_desc
        0,              // system_flag
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
    result += test_json(NULL, result);  // NULL: we want to check only the logs

    /*------------------------------------*
     *  Check "topic_desc.json" file
     *------------------------------------*/
    build_path(file, sizeof(file), path_topic, "topic_desc.json", NULL);
    if(1) {
        char expected[16*1024];
        snprintf(expected, sizeof(expected), "\
        { \
          'topic_name': '%s', \
          'pkey': 'id', \
          'tkey': '', \
          'system_flag': 1 \
        } \
        ", TOPIC_NAME);

        set_expected_results(
            "check_topic_desc.json",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            NULL,
            TRUE
        );
        result += test_json_file(file, result);
    }

    /*------------------------------------*
     *  Check "topic_cols.json" file
     *------------------------------------*/
    build_path(file, sizeof(file), path_topic, "topic_cols.json", NULL);
    if(1) {
        char expected[]= "\
        { \
          'id': '', \
          'address': '' \
        } \
        ";

        set_expected_results(
            "check_topic_cols.json",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            NULL,
            TRUE
        );
        result += test_json_file(file, result);
    }

    /*------------------------------------*
     *  Check "topic_var.json" file
     *------------------------------------*/
    build_path(file, sizeof(file), path_topic, "topic_var.json", NULL);
    if(1) {
        char expected[]= "\
        { \
        } \
        ";

        set_expected_results(
            "check_topic_var.json",      // test name
            NULL,
            string2json(helper_quote2doublequote(expected), TRUE),
            NULL,
            TRUE
        );
        result += test_json_file(file, result);
    }

    /*------------------------------------------*
     *  Check tranger memory with topic opened
     *------------------------------------------*/
    if(1) {
        char expected[16*1024];
        snprintf(expected, sizeof(expected), "\
        { \
            'path': '%s', \
            'database': '%s', \
            'filename_mask': '%%Y-%%m-%%d', \
            'xpermission': 1528, \
            'rpermission': 432, \
            'on_critical_error': 0, \
            'master': true, \
            'gobj': 0, \
            'trace_level': 0, \
            'directory': '%s', \
            'fd_opened_files': { \
                '__timeranger2__.json': 9999 \
            }, \
            'yev_loop': 0, \
            'topics': { \
                '%s': { \
                    'topic_name': '%s', \
                        'pkey': 'id', \
                        'tkey': '', \
                        'system_flag': 1, \
                        'cols': { \
                        'id': '', \
                            'address': '' \
                    }, \
                    'directory': '%s', \
                    'wr_fd_files': {}, \
                    'rd_fd_files': {}, \
                    'lists': [], \
                    'disks': [], \
                    'iterators': [] \
                } \
            } \
        } \
        ", path_root, DATABASE, path_database, TOPIC_NAME, TOPIC_NAME, path_topic);

        const char *ignore_keys[]= {
            "__timeranger2__.json",
            NULL
        };
        json_t *expected_ = string2json(helper_quote2doublequote(expected), TRUE);
        if(!expected_) {
            result += -1;
        }
        set_expected_results(
            "check_tranger_mem1",      // test name
            NULL,
            expected_,
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger), result);
    }

    list_open_files();

    /*------------------------*
     *      Close topic
     *------------------------*/
    set_expected_results(
        "check_close_topic", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    tranger2_close_topic(tranger, TOPIC_NAME);

    result += test_json(NULL, result);  // NULL: we want to check only the logs

    /*------------------------------------------*
     *  Check tranger memory with topic closed
     *------------------------------------------*/
    if(1) {
        char expected[16*1024];
        snprintf(expected, sizeof(expected), "\
        { \
            'path': '%s', \
            'database': 'tr_tranger_startup', \
            'filename_mask': '%%Y-%%m-%%d', \
            'xpermission': 1528, \
            'rpermission': 432, \
            'on_critical_error': 0, \
            'master': true, \
            'gobj': 0, \
            'trace_level': 0, \
            'directory': '%s', \
            'fd_opened_files': { \
                '__timeranger2__.json': 9999 \
            }, \
            'yev_loop': 0, \
            'topics': {} \
        } \
        ", path_root, path_database);

        const char *ignore_keys[]= {
            "__timeranger2__.json",
            NULL
        };
        json_t *expected_ = string2json(helper_quote2doublequote(expected), TRUE);
        if(!expected_) {
            result += -1;
        }
        set_expected_results(
            "check_tranger_mem2",      // test name
            NULL,
            expected_,
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger), result);
    }

    list_open_files();

    /*------------------------------------------*
     *  Check re-open tranger as master
     *------------------------------------------*/
    if(1) {
        char expected[16*1024];
        snprintf(expected, sizeof(expected), "\
        { \
            'path': '%s', \
            'database': 'tr_tranger_startup', \
            'filename_mask': '%%Y-%%m-%%d', \
            'xpermission': 1528, \
            'rpermission': 432, \
            'on_critical_error': 0, \
            'master': false, \
            'gobj': 0, \
            'trace_level': 0, \
            'directory': '%s', \
            'fd_opened_files': { \
                '__timeranger2__.json': -1 \
            }, \
            'yev_loop': 0, \
            'topics': {} \
        } \
        ", path_root, path_database);
        const char *ignore_keys[]= {
            NULL
        };

        json_t *expected_ = string2json(helper_quote2doublequote(expected), TRUE);
        if(!expected_) {
            result += -1;
        }
        set_expected_results(
            "check_tranger_reopen_as_master",      // test name
            json_pack("[{s:s},{s:s}]", // error's list
                "msg", "Cannot open json file",
                "msg", "Open as not master, __timeranger2__.json locked"
            ),
            expected_,
            ignore_keys,
            TRUE
        );

        json_t *jn_tr = json_pack("{s:s, s:s, s:b, s:i}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", 0
        );
        json_t *tr = tranger2_startup(0, jn_tr, 0);

        result += test_json(json_incref(tr), result);

        set_expected_results(
            "tranger_shutdown", // test name
            NULL,   // error's list, It must not be any log error
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        tranger2_shutdown(tr);
        result += test_json(NULL, result);  // NULL: we want to check only the logs
    }

    /*
     *  Shutdown timeranger
     */
    set_expected_results(
        "tranger_shutdown", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    tranger2_shutdown(tranger);
    result += test_json(NULL, result);  // NULL: we want to check only the logs

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

    if(get_cur_system_memory()!=0) {
        printf("system memory not free\n");
        result += -1;
    }
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
