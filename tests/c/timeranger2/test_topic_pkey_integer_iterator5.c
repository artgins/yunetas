/****************************************************************************
 *          test_topic_pkey_integer_iterator5.c
 *
 *  - Open as master, open iterator to search with callback and data
 *  - Do search of pages
 *
 *          Copyright (c) 2024 ArtGins.
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

//PRIVATE int pinta_md = 1;
//PRIVATE int pinta_records = 0;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_t *yev_loop;

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int global_result = 0;


/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int search_page(
    json_t *tranger,
    json_t *iterator,
    uint64_t from_rowid,
    size_t limit,
    size_t rows_expected
)
{
    int result = 0;

    json_t *rows = tranger2_iterator_get_page(
        tranger,
        iterator,
        from_rowid,
        limit
    );

    uint64_t rows_found = json_array_size(rows);
    if(rows_found != rows_expected) {
        printf("%sERROR%s --> rows expected %d, found %d\n", On_Red BWhite, Color_Off,
            (int)rows_expected,
            (int)json_array_size(rows)
        );
        result += -1;
    }

    uint64_t t1 = 946684800 + from_rowid -1;
    int idx; json_t *row;
    json_array_foreach(rows, idx, row) {
        uint64_t tm = json_integer_value(json_object_get(row, "tm"));
        if(tm != t1) {
            result += -1;
            printf("%sERROR%s --> tm expected %d, found %d\n", On_Red BWhite, Color_Off,
                (int)tm,
                (int)t1
            );
        }
        t1++;
    }

    JSON_DECREF(rows)
    return result;
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
    json_t *tranger = tranger2_startup(0, jn_tranger);
    result += test_json(NULL, result);  // NULL: we want to check only the logs

    BOOL test_forward = 1;
    BOOL test_backward = 1;

    /*-------------------------------------*
     *  Search pages, forward
     *-------------------------------------*/
    if(test_forward) {
        const char *KEY = "0000000000000000001";
        set_expected_results( // Check that no logs happen
            "tranger2_open_iterator", // test name
            NULL,   // error's list, It must not be any log error
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );

        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        json_t *topic = tranger2_topic(tranger, TOPIC_NAME);
        json_t *match_cond = json_pack("{s:b}",
            "backward", 0
        );
        json_t *iterator = tranger2_open_iterator(
            tranger,
            topic,
            KEY,                    // key
            match_cond,             // match_cond, owned
            NULL,                   // load_record_callback
            NULL,                   // id
            NULL                    // data
        );
        MT_INCREMENT_COUNT(time_measure, MAX_RECORDS)
        MT_PRINT_TIME(time_measure, "tranger2_open_iterator")
        result += test_json(NULL, result);  // NULL: we want to check only the logs

        /*-------------------------*
         *
         *-------------------------*/
        const char *TEST_NAME = "Search page";
        set_expected_results( // Check that no logs happen
            TEST_NAME, // test name
            NULL,   // error's list, It must not be any log error
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        json_int_t page_size = 41;
        size_t total_rows = tranger2_iterator_size(iterator);

        MT_START_TIME(time_measure)

        size_t from_rowid;
        for(from_rowid=1; from_rowid<=total_rows/page_size; from_rowid += page_size) {
            result += search_page(
                tranger,
                iterator,
                from_rowid,
                page_size,
                page_size
            );
        }
        if(from_rowid <= total_rows) {
            result += search_page(
                tranger,
                iterator,
                from_rowid,
                page_size,
                total_rows - from_rowid + 1 // = (total_rows % page_size)
            );
        }

        MT_INCREMENT_COUNT(time_measure, MAX_RECORDS)
        MT_PRINT_TIME(time_measure, TEST_NAME)

        result += test_json(NULL, result);  // NULL: we want to check only the logs

        /*-------------------------*
         *  close
         *-------------------------*/
        set_expected_results( // Check that no logs happen
            "tranger2_close_iterator", // test name
            NULL,   // error's list, It must not be any log error
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        result += tranger2_close_iterator(tranger, iterator);
        result += test_json(NULL, result);  // NULL: we want to check only the logs

    }

    /*-------------------------------------*
     *  Search pages, backward
     *-------------------------------------*/
    if(test_backward) {
        const char *KEY             = "0000000000000000001";
        set_expected_results( // Check that no logs happen
            "tranger2_open_iterator", // test name
            NULL,   // error's list, It must not be any log error
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );

        time_measure_t time_measure;
        MT_START_TIME(time_measure)

        json_t *topic = tranger2_topic(tranger, TOPIC_NAME);
        json_t *match_cond = json_pack("{s:b}",
            "backward", 1
        );
        json_t *iterator = tranger2_open_iterator(
            tranger,
            topic,
            KEY,                    // key
            match_cond,             // match_cond, owned
            NULL,                   // load_record_callback
            NULL,                   // id
            NULL                    // data
        );
        MT_INCREMENT_COUNT(time_measure, MAX_RECORDS)
        MT_PRINT_TIME(time_measure, "tranger2_open_iterator")
        result += test_json(NULL, result);  // NULL: we want to check only the logs

        /*-------------------------*
         *
         *-------------------------*/

        /*-------------------------*
         *  close
         *-------------------------*/
        set_expected_results( // Check that no logs happen
            "tranger2_close_iterator", // test name
            NULL,   // error's list, It must not be any log error
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        result += tranger2_close_iterator(tranger, iterator);
        result += test_json(NULL, result);  // NULL: we want to check only the logs

    }

//    json_t *page = tranger2_iterator_get_page( // return must be owned
//        tranger,
//        iterator,
//        from_rowid,
//        json_int_t to_rowid
//    );

    //    json_int_t key = appends/2 + 1;

    /*-------------------------------------*
     *      Open rt list
     *-------------------------------------*/
//    set_expected_results( // Check that no logs happen
//        "open rt list 2", // test name
//        NULL,   // error's list, It must not be any log error
//        NULL,   // expected, NULL: we want to check only the logs
//        NULL,   // ignore_keys
//        TRUE    // verbose
//    );
//
//    all_leidos = 0;
//    one_leidos = 0;
//
//    json_t *tr_list = tranger2_open_rt_list(
//        tranger,
//        TOPIC_NAME,
//        "",             // key
//        all_load_record_callback,
//        ""              // list id
//    );
//
//    tranger2_open_rt_list(
//        tranger,
//        TOPIC_NAME,
//        "0000000000000000001",       // key
//        one_load_record_callback,
//        "list2"              // list id
//    );
//
//    result += test_json(NULL);  // NULL: we want to check only the logs

    /*-------------------------------------*
     *      Add records
     *-------------------------------------*/
//    set_expected_results( // Check that no logs happen
//        "append records 2", // test name
//        NULL,   // error's list, It must not be any log error
//        NULL,   // expected, NULL: we want to check only the logs
//        NULL,   // ignore_keys
//        TRUE    // verbose
//    );
//
//    uint64_t t1 = 946684800; // 2000-01-01T00:00:00+0000
//    for(json_int_t i=0; i<MAX_KEYS; i++) {
//        uint64_t tm = t1;
//        for(json_int_t j=0; j<MAX_RECORDS; j++) {
//            json_t *jn_record1 = json_pack("{s:I, s:I, s:s}",
//               "id", i + 1,
//               "tm", tm+j,
//               "content",
//               "Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el."
//               "Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el."
//               "Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el.x"
//            );
//            md2_record_t md_record;
//            tranger2_append_record(tranger, TOPIC_NAME, tm+j, 0, &md_record, jn_record1);
//        }
//    }
//    result += test_json(NULL);  // NULL: we want to check only the logs

    /*-------------------------------------*
     *      Close rt lists
     *-------------------------------------*/
//    set_expected_results( // Check that no logs happen
//        "close rt lists 2", // test name
//        NULL,   // error's list, It must not be any log error
//        NULL,   // expected, NULL: we want to check only the logs
//        NULL,   // ignore_keys
//        TRUE    // verbose
//    );
//
//    tranger2_close_rt_list(
//        tranger,
//        tr_list
//    );
//
//    json_t *list2 =tranger2_get_rt_list_by_id(
//        tranger,
//        "list2"
//    );
//    tranger2_close_rt_list(
//        tranger,
//        list2
//    );
//
//    result += test_json(NULL);  // NULL: we want to check only the logs
//
//    if(all_leidos != MAX_KEYS*MAX_RECORDS) {
//        printf("%sRecords read not match%s, leidos %d, records %d\n", On_Red BWhite,Color_Off,
//           (int)all_leidos, MAX_KEYS*MAX_RECORDS
//        );
//        result += -1;
//    }
//
//    if(one_leidos != MAX_RECORDS) {
//        printf("%sRecords read not match%s, leidos %d, records %d\n", On_Red BWhite,Color_Off,
//            (int)one_leidos, MAX_RECORDS
//        );
//        result += -1;
//    }

    /*-------------------------------*
     *  tranger_backup_topic
     *-------------------------------*/
    // TODO old test in test_timeranger2.c

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

    result += debug_json(tranger, FALSE);

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