/****************************************************************************
 *          test_topic_pkey_integer_iterator2.c
 *
 *  - Open as master, open iterator for each key (two keys),
 *      empty match_cond (all records)  with callback
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
#include <kwid.h>
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
PRIVATE json_int_t tm = 0;
PRIVATE uint64_t t = 0;
PRIVATE uint64_t leidos = 0;

PRIVATE int iterator_callback1(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list, // iterator or rt_list/rt_disk id, don't own
    json_int_t rowid,
    md2_record_t *md2_record,
    json_t *record      // must be owned
)
{
    t++;
    tm++;
    leidos++;

    JSON_DECREF(record)
    return 0;
}

PRIVATE int iterator_callback2(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list, // iterator or rt_list/rt_disk id, don't own
    json_int_t rowid,
    md2_record_t *md2_record,
    json_t *record      // must be owned
)
{
    t++;
    tm++;
    leidos++;

    JSON_DECREF(record)
    return 0;
}

/***************************************************************************
 *              Test
 *  Open as master, open iterator (realtime by disk) with callback
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

    /*--------------------------------------------------------------------*
     *  Open an iterator, no callback, match_cond NULL (use defaults)
     *--------------------------------------------------------------------*/
    set_expected_results( // Check that no logs happen
        "open iterator", // test name
        NULL,   // error's list
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    leidos = 0;
    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    /*
     *  Open iterator to key1
     */
    t = tm = 946684800-1;
    tranger2_open_iterator(
        tranger,
        TOPIC_NAME,
        "0000000000000000001",     // key,
        NULL,   // match_cond, owned
        iterator_callback1,    // load_record_callback
        "it1",
        NULL,
        NULL
    );

    json_t *iterator1 = tranger2_get_iterator_by_id(tranger, TOPIC_NAME, "it1");

    if(tm != 946774799) {
        printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD count tm of message");
        //print_json2("BAD count tm of message", tranger);
        result += -1;
    }
    if(t != 946774799) {
        printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD count t of message");
        //print_json2("BAD count t of message", tranger);
        result += -1;
    }

    /*
     *  Open iterator to key2
     */
    t = tm = 946684800-1;
    json_t *iterator2 = tranger2_open_iterator(
        tranger,
        TOPIC_NAME,
        "0000000000000000002",     // key,
        NULL,   // match_cond, owned
        iterator_callback2,    // load_record_callback
        NULL,   // id
        NULL,   // data
        NULL    // options
    );

    if(tm != 946774799) {
        printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD count tm of message");
        //print_json2("BAD count tm of message", tranger);
        result += -1;
    }
    if(t != 946774799) {
        printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD count t of message");
        //print_json2("BAD count t of message", tranger);
        result += -1;
    }

    /*
     *  Check totals
     */
    if(leidos != MAX_KEYS*MAX_RECORDS) {
        printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD count leidos of message");
        //print_json2("BAD count leidos of message", tranger);
        result += -1;
    }

    MT_INCREMENT_COUNT(time_measure, MAX_KEYS*MAX_RECORDS)
    MT_PRINT_TIME(time_measure, "open two iterators (2 keys) with callback (old=98.000 op/sec)")
    result += test_json(NULL, result);  // NULL: we want to check only the logs

    /*---------------------------------------------*
     *  Repeat Open iterator to key2
     *---------------------------------------------*/
    set_expected_results( // Check that no logs happen
        "repeat open iterator", // test name
        json_pack("[{s:s}]", // error's list
            "msg", "tranger2_open_iterator(): Iterator already exists"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );
    json_t *iterator22 = tranger2_open_iterator(
        tranger,
        TOPIC_NAME,
        "0000000000000000002",     // key,
        NULL,   // match_cond, owned
        NULL,   // load_record_callback
        NULL,   // id
        NULL,   // data
        NULL    // options
    );
    if(iterator22) {
        printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "Repeat iterator must be null");
        result += -1;
    }
    result += test_json(NULL, result);  // NULL: we want to check only the logs

    /*---------------------------------------------*
     *  Check iterator mem
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
                    'rd_fd_files': { \
                        '0000000000000000001': { \
                            '2000-01-01.md2': 9999, \
                            '2000-01-01.json': 9999, \
                            '2000-01-02.md2': 9999, \
                            '2000-01-02.json': 9999 \
                        }, \
                        '0000000000000000002': { \
                            '2000-01-01.md2': 9999, \
                            '2000-01-01.json': 9999, \
                            '2000-01-02.md2': 9999, \
                            '2000-01-02.json': 9999 \
                        } \
                    }, \
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
                    'disks': [ \
                        { \
                            'id': 'it1', \
                            'topic_name': 'topic_pkey_integer', \
                            'key': '0000000000000000001', \
                            'match_cond': {}, \
                            'load_record_callback': 99999, \
                            'list_type': 'rt_disk'\
                        }, \
                        { \
                            'id': '0000000000000000002', \
                            'topic_name': 'topic_pkey_integer', \
                            'key': '0000000000000000002', \
                            'match_cond': {}, \
                            'load_record_callback': 99999, \
                            'list_type': 'rt_disk'\
                        } \
                    ], \
                    'iterators': [\
                        { \
                            'id': 'it1', \
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
                            'cur_segment': 1, \
                            'cur_rowid': 90000, \
                            'realtime': true, \
                            'list_type': 'iterator',\
                            'load_record_callback': 9999, \
                            'rt_disk': { \
                                'id': 'it1', \
                                'topic_name': 'topic_pkey_integer', \
                                'key': '0000000000000000001', \
                                'match_cond': {}, \
                                'load_record_callback': 99999, \
                                'list_type': 'rt_disk'\
                            } \
                        }, \
                        { \
                            'id': '0000000000000000002', \
                            'key': '0000000000000000002', \
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
                                    'key': '0000000000000000002' \
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
                                    'key': '0000000000000000002' \
                                } \
                            ], \
                            'cur_segment': 1, \
                            'cur_rowid': 90000, \
                            'realtime': true, \
                            'list_type': 'iterator',\
                            'load_record_callback': 9999, \
                            'rt_disk': { \
                                'id': '0000000000000000002', \
                                'topic_name': 'topic_pkey_integer', \
                                'key': '0000000000000000002', \
                                'match_cond': {}, \
                                'load_record_callback': 99999, \
                                'list_type': 'rt_disk'\
                            } \
                        } \
                    ] \
                } \
            } \
        } \
        ", path_root, DATABASE, path_database, TOPIC_NAME, TOPIC_NAME, path_topic, TOPIC_NAME, TOPIC_NAME);

        const char *ignore_keys[]= {
            "__timeranger2__.json",
            "wr_time",
            "load_record_callback",
            "2000-01-01.md2",
            "2000-01-01.json",
            "2000-01-02.md2",
            "2000-01-02.json",
            NULL
        };
        json_t *expected_ = string2json(helper_quote2doublequote(expected), TRUE);
        if(!expected_) {
            result += -1;
        }
        set_expected_results(
            "check iterator mem",      // test name
            NULL,
            expected_,
            ignore_keys,
            TRUE
        );
        result += test_json(json_incref(tranger), result);
    }

    /*-------------------------------*
     *      Close iterators
     *-------------------------------*/
    set_expected_results( // Check that no logs happen
        "close iterators", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    result += debug_json("tranger", tranger, FALSE);
    result += tranger2_close_iterator(tranger, iterator1);
    result += tranger2_close_iterator(tranger, iterator2);
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
    result += debug_json("tranger", tranger, FALSE);
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

    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        10,
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
        printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "system memory not free");
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
