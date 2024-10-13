/****************************************************************************
 *          test_topic_pkey_integer_iterator.c
 *
 *  Open as master, open iterator without callback
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

#define DATABASE    "tr_topic_pkey_integer"
#define TOPIC_NAME  "topic_pkey_integer"
#define MAX_KEYS    2
#define MAX_RECORDS 90000 // 1 day and 1 hour

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_t *yev_loop;
PRIVATE int global_result = 0;

/***************************************************************************
 *              Test
 *  Open as master, open iterator (realtime by disk) without callback
 *  HACK: return -1 to fail, 0 to ok
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    global_result = 0;

    /*
     *  Write the tests in ~/tests_yuneta/
     */
    const char *home = getenv("HOME");
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    char path_topic[PATH_MAX];

    build_path(path_root, sizeof(path_root), home, "tests_yuneta", NULL);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    build_path(path_topic, sizeof(path_topic), path_database, TOPIC_NAME, NULL);

    /*-------------------------------------------------*
     *      Startup the timeranger db
     *-------------------------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger2_startup", // test name
        NULL,   // error's list, It must not be any log error
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
    result += test_json(NULL, result);  // NULL: we want to check only the logs

    /*-------------------------------------------------*
     *      Open the topic
     *-------------------------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger2_open_topic", // test name
        NULL,   // error's list
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    json_t *topic = tranger2_open_topic(
        tranger,
        TOPIC_NAME,     // topic name
        TRUE
    );
    result += test_json(NULL, result);  // NULL: we want to check only the logs
    if(!topic) {
        tranger2_shutdown(tranger);
        return -1;
    }

    /*--------------------------------------------------------------------*
     *  Open an iterator, no callback, match_cond NULL (use defaults)
     *--------------------------------------------------------------------*/
    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    set_expected_results( // Check that no logs happen
        "open iterator without callback neither data", // test name
        NULL,   // error's list
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    json_t *iterator = tranger2_open_iterator(
        tranger,
        TOPIC_NAME,
        "0000000000000000001",     // key,
        NULL,   // match_cond, owned
        NULL,   // load_record_callback
        NULL,   // iterator id
        NULL,   // data
        NULL    // options
    );

    MT_INCREMENT_COUNT(time_measure, MAX_KEYS*MAX_RECORDS)
    MT_PRINT_TIME(time_measure, "open iterator without callback neither data")

    result += test_json(NULL, result);  // NULL: we want to check only the logs
    if(!iterator) {
        tranger2_shutdown(tranger);
        return -1;
    }

    /*---------------------------------------------*
     *  Check tranger memory with iterator opened
     *---------------------------------------------*/
    if(1) {
        char expected[32*1024];
        snprintf(expected, sizeof(expected), "\
        { \
            'path': '%s', \
            'database': '%s', \
            'filename_mask': '%%Y', \
            'xpermission': 1528, \
            'rpermission': 384, \
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
                    'tkey': 'tm', \
                    'system_flag': 4, \
                    'filename_mask': '%%Y-%%m-%%d', \
                    'xpermission': 1472, \
                    'rpermission': 384, \
                    'topic_version': 1, \
                    'cols': { \
                        'id': '', \
                        'tm': 0, \
                        'content': '', \
                        'content2': '' \
                    }, \
                    'directory': '%s', \
                    'wr_fd_files': {}, \
                    'rd_fd_files': {}, \
                    'cache': { \
                        '0000000000000000001': { \
                            'files': [ \
                                { \
                                    'id': '2000-01-01', \
                                    'fr_t': 946684800, \
                                    'to_t': 946771199, \
                                    'fr_tm': 946684800, \
                                    'to_tm': 946771199, \
                                    'rows': 86400, \
                                    'wr_time': 9999 \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 946771200, \
                                    'to_tm': 946774799, \
                                    'rows': 3600, \
                                    'wr_time': 9999 \
                                } \
                            ], \
                            'total': { \
                                'fr_t': 946684800, \
                                'to_t': 946774799, \
                                'fr_tm': 946684800, \
                                'to_tm': 946774799, \
                                'rows': 90000 \
                            } \
                        }, \
                        '0000000000000000002': { \
                            'files': [ \
                                { \
                                    'id': '2000-01-01', \
                                    'fr_t': 946684800, \
                                    'to_t': 946771199, \
                                    'fr_tm': 946684800, \
                                    'to_tm': 946771199, \
                                    'rows': 86400, \
                                    'wr_time': 9999 \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 946771200, \
                                    'to_tm': 946774799, \
                                    'rows': 3600, \
                                    'wr_time': 9999 \
                                } \
                            ], \
                            'total': { \
                                'fr_t': 946684800, \
                                'to_t': 946774799, \
                                'fr_tm': 946684800, \
                                'to_tm': 946774799, \
                                'rows': 90000 \
                            } \
                        } \
                    }, \
                    'lists': [], \
                    'disks': [], \
                    'iterators': [\
                        { \
                            'id': '0000000000000000001', \
                            'key': '0000000000000000001', \
                            'topic_name': '%s', \
                            'match_cond': {}, \
                            'segments': [ \
                                { \
                                    'id': '2000-01-01', \
                                    'fr_t': 946684800, \
                                    'to_t': 946771199, \
                                    'fr_tm': 946684800, \
                                    'to_tm': 946771199, \
                                    'rows': 86400, \
                                    'wr_time': 99999, \
                                    'first_row': 1, \
                                    'last_row': 86400, \
                                    'key': '0000000000000000001' \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 946771200, \
                                    'to_tm': 946774799, \
                                    'rows': 3600, \
                                    'wr_time': 99999, \
                                    'first_row': 86401, \
                                    'last_row': 90000, \
                                    'key': '0000000000000000001' \
                                } \
                            ], \
                            'cur_segment': 0, \
                            'cur_rowid': 0, \
                            'realtime': false, \
                            'list_type': 'iterator',\
                            'load_record_callback': 0 \
                        } \
                    ] \
                } \
            } \
        } \
        ", path_root, DATABASE, path_database, TOPIC_NAME, TOPIC_NAME, path_topic, TOPIC_NAME);

        const char *ignore_keys[]= {
            "__timeranger2__.json",
            "wr_time",
            NULL
        };
        json_t *expected_ = string2json(helper_quote2doublequote(expected), TRUE);
        if(!expected_) {
            result += -1;
        }
        set_expected_results(
            "check tranger mem, with iterator open",      // test name
            NULL,
            expected_,
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger), result);
    }

    /*-------------------------------*
     *      Close iterator
     *-------------------------------*/
    set_expected_results( // Check that no logs happen
        "close iterator", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    result += tranger2_close_iterator(tranger, iterator);
    result += test_json(NULL, result);  // NULL: we want to check only the logs

    /*-------------------------------*
     *      Shutdown timeranger
     *-------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger_shutdown", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    json_check_refcounts(tranger, 1000, &result);

    tranger2_shutdown(tranger);
    result += test_json(NULL, result);  // NULL: we want to check only the logs

    result += global_result;

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

    unsigned long memory_check_list[] = {0}; // WARNING: list ended with 0
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
        256*1024L,    // max_block, largest memory block
        1*1024*1024L   // max_system_memory, maximum system memory
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

    if(get_cur_system_memory()!=0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
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
