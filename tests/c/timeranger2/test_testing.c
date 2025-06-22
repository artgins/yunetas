/****************************************************************************
 *          test_testing.c
 *
 *          Test some functions of testing
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>

#include <gobj.h>
#include <timeranger2.h>
#include <yev_loop.h>
#include <testing.h>
#include <helpers.h>

#define APP "test_testing"

/***************************************************************
 *              Constants
 ***************************************************************/
#define DATABASE    "tr_testing"
#define TOPIC_NAME  "topic_sample"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_h yev_loop;
yev_event_h yev_event_once;
yev_event_h yev_event_periodic;
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

    /*------------------------------------------*
     *  Check test_json_1
     *------------------------------------------*/
    if(1) {
        char expected[32*1024];
        snprintf(expected, sizeof(expected), "\
        { \
            'topics': { \
                'XXX': { \
                    'done': {\
                        '0001': { \
                            '2000': 0, \
                            '2001': 0 \
                        }, \
                        '0002': { \
                            '2000': 0, \
                            '2001': 0 \
                        } \
                    }, \
                    'done1': [ \
                        { \
                            'crea': 0, \
                            'topi': 0, \
                            'clas': {}, \
                            'list': 0 \
                        }, \
                        { \
                            'crea': 0, \
                            'topi': 0, \
                            'clas': {}, \
                            'list': 0 \
                        } \
                    ], \
                    'done3': {}, \
                    'done4': {} \
                } \
            } \
        } \
        ");

        const char *ignore_keys[]= {
            NULL
        };
        json_t *expected_ = string2json(helper_quote2doublequote(expected), TRUE);
        if(!expected_) {
            result += -1;
        }
        json_t *found = string2json(helper_quote2doublequote(expected), TRUE);
        if(!found) {
            result += -1;
        }
        set_expected_results(
            "test_json_1",      // test name
            NULL,
            expected_,
            ignore_keys,
            TRUE
        );
        result += test_json(found);
    }

    /*------------------------------------------*
     *  Check test_json_2
     *------------------------------------------*/
    if(0) {
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
            'trace_level': 1, \
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
                    'cols': { \
                        'id': '', \
                        'tm': 0, \
                        'content': '' \
                    }, \
                    'directory': '%s', \
                    'wr_fd_files': {\
                        '0000000000000000001': { \
                            '2000-01-02.json': 99999, \
                            '2000-01-02.md2': 99999 \
                        }, \
                        '0000000000000000002': { \
                            '2000-01-02.json': 99999, \
                            '2000-01-02.md2': 99999 \
                        } \
                    }, \
                    'rd_fd_files': {}, \
                    'cache': { \
                        '0000000000000000001': { \
                            'files': [ \
                                { \
                                    'id': '2000-01-01', \
                                    'fr_t': 946684800, \
                                    'to_t': 946771199, \
                                    'fr_tm': 70369690862464, \
                                    'to_tm': 70369690948863, \
                                    'rows': 86400 \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 70369690948864, \
                                    'to_tm': 70369690952463, \
                                    'rows': 3600 \
                                } \
                            ], \
                            'total': { \
                                'fr_t': 946684800, \
                                'to_t': 946774799, \
                                'fr_tm': 70369690862464, \
                                'to_tm': 70369690952463, \
                                'rows': 90000 \
                            } \
                        }, \
                        '0000000000000000002': { \
                            'files': [ \
                                { \
                                    'id': '2000-01-01', \
                                    'fr_t': 946684800, \
                                    'to_t': 946771199, \
                                    'fr_tm': 70369690862464, \
                                    'to_tm': 70369690948863, \
                                    'rows': 86400 \
                                }, \
                                { \
                                    'id': '2000-01-02', \
                                    'fr_t': 946771200, \
                                    'to_t': 946774799, \
                                    'fr_tm': 70369690948864, \
                                    'to_tm': 70369690952463, \
                                    'rows': 3600 \
                                } \
                            ], \
                            'total': { \
                                'fr_t': 946684800, \
                                'to_t': 946774799, \
                                'fr_tm': 70369690862464, \
                                'to_tm': 70369690952463, \
                                'rows': 90000 \
                            } \
                        } \
                    }, \
                    'lists': [ \
                        { \
                            'id': 'list1', \
                            'creator': '', \
                            'topic_name': '%s', \
                            'key': '', \
                            'match_cond': {}, \
                            'load_record_callback': 99999, \
                            'list_type': 'rt_mem'\
                        }, \
                        { \
                            'id': 'list2', \
                            'creator': '', \
                            'topic_name': '%s', \
                            'key': '0000000000000000001', \
                            'match_cond': {}, \
                            'load_record_callback': 99999, \
                            'list_type': 'rt_mem' \
                        } \
                    ], \
                    'disks': [], \
                    'iterators': [] \
                } \
            } \
        } \
        ", path_root, DATABASE, path_database, TOPIC_NAME, TOPIC_NAME, path_topic, TOPIC_NAME, TOPIC_NAME);

        const char *ignore_keys[]= {
            NULL
        };
        json_t *expected_ = string2json(helper_quote2doublequote(expected), TRUE);
        if(!expected_) {
            result += -1;
        }
        json_t *found = string2json(helper_quote2doublequote(expected), TRUE);
        if(!found) {
            result += -1;
        }
        set_expected_results(
            "test_json_2",      // test name
            NULL,
            expected_,
            ignore_keys,
            TRUE
        );
        result += test_json(found);
    }

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

    unsigned long memory_check_list[] = {1377, 0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL   // global_authenticate_parser
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
        10,
        NULL,
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
        print_track_mem();
        result += -1;
    }

    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    }
    return result<0?-1:0;
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int xtimes_once = 0;
    xtimes_once++;
    yev_loop_reset_running(yev_loop);
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
