/****************************************************************************
 *          test_topic_pkey_integer_iterator5.c
 *
 *  - Open as master, open iterator to search with callback and data
 *  - Do search of pages
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#define APP "test_topic_pkey_integer_iterator5"

#include <string.h>
#include <signal.h>
#include <limits.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define DATABASE    "tr_topic_pkey_integer"
#define TOPIC_NAME  "topic_pkey_integer"
#define MAX_KEYS    2
#define MAX_RECORDS 90000 // 1 day and 1 hour

PRIVATE int pinta_tiempos_intermedios = 0;
PRIVATE int pinta_records = 0;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE int global_result = 0;

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int search_page(
    json_t *tranger,
    json_t *iterator,
    json_int_t from_rowid,
    size_t limit,
    size_t rows_expected,
    size_t total_rows,
    size_t pages,
    BOOL BACKWARD
)
{
    int result = 0;

    json_t *page = tranger2_iterator_get_page(
        tranger,
        iterator,
        from_rowid,
        limit,
        BACKWARD
    );

    if(pinta_records) {
        print_json2("page", page);
    }
    uint64_t rows_found = json_array_size(json_object_get(page, "data"));
    if(rows_found != rows_expected) {
        printf("%sERROR%s --> rows expected %d, found %d\n", On_Red BWhite, Color_Off,
            (int)rows_expected,
            (int)rows_found
        );
        result += -1;
    }

    if(total_rows != json_integer_value(json_object_get(page, "total_rows"))) {
        printf("%sERROR%s --> total_rows expected %d, found %d\n", On_Red BWhite, Color_Off,
            (int)total_rows,
            (int)json_integer_value(json_object_get(page, "total_rows"))
        );
        result += -1;
    }

    if(pages != json_integer_value(json_object_get(page, "pages"))) {
        printf("%sERROR%s --> pages expected %d, found %d\n", On_Red BWhite, Color_Off,
            (int)pages,
            (int)json_integer_value(json_object_get(page, "pages"))
        );
        result += -1;
    }

    json_t *data = json_object_get(page, "data");

    uint64_t t1, rowid;
    if(!BACKWARD) {
        t1 = 946684800 + from_rowid -1;
        rowid = from_rowid;

    } else {
        t1 = 946684800 + from_rowid + rows_found - 2;
        rowid = from_rowid + rows_found - 1;
    }
    int idx; json_t *row;
    json_array_foreach(data, idx, row) {
        uint64_t tm = json_integer_value(json_object_get(row, "tm"));
        if(tm != t1) {
            result += -1;
            printf("%sERROR%s --> tm expected %d, found %d\n", On_Red BWhite, Color_Off,
                (int)t1,
                (int)tm
            );
        }
        if(tm != t1) {
            result += -1;
            printf("%sERROR%s --> tm expected %d, found %d\n", On_Red BWhite, Color_Off,
                (int)t1,
                (int)tm
            );
        }

        uint64_t rowid_ = json_integer_value(
            json_object_get(json_object_get(row, "__md_tranger__"), "g_rowid")
        );
        if(rowid != rowid_) {
            result += -1;
            printf("%sERROR%s --> rowid expected %d, found %d\n", On_Red BWhite, Color_Off,
                (int)rowid,
                (int)rowid_
            );
        }

        if(!BACKWARD) {
            t1++;
            rowid++;
        } else {
            t1--;
            rowid--;
        }
    }

    JSON_DECREF(page)
    return result;
}

/***************************************************************************
 *              Test
 *  - Open as master, open iterator to search with callback and data
 *  - Do search of pages
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
    result += test_json(NULL);  // NULL: we want to check only the logs

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

        json_t *match_cond = json_pack("{s:b}",
            "backward", 0
        );
        json_t *iterator = tranger2_open_iterator(
            tranger,
            TOPIC_NAME,
            KEY,                    // key
            match_cond,             // match_cond, owned
            NULL,                   // load_record_callback
            NULL,                   // rt_id
            NULL,                   // creator
            NULL,                   // data
            NULL                    // options
        );
        MT_INCREMENT_COUNT(time_measure, MAX_RECORDS)
        MT_PRINT_TIME(time_measure, "tranger2_open_iterator")
        result += test_json(NULL);  // NULL: we want to check only the logs

        /*-------------------------*
         *
         *-------------------------*/
        const char *TEST_NAME = "Search page FORWARD";
        set_expected_results( // Check that no logs happen
            TEST_NAME, // test name
            NULL,   // error's list, It must not be any log error
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        json_int_t page_size = 41;
        size_t total_rows = tranger2_iterator_size(iterator);

        size_t pages = total_rows / page_size;
        size_t total_pages = pages + ((total_rows % page_size)?1:0);

        MT_START_TIME(time_measure)

        int page;
        json_int_t from_rowid;
        for(from_rowid=1, page=0; page<pages; page++, from_rowid += page_size) {
            time_measure_t time_measure2;
            MT_START_TIME(time_measure2)
            result += search_page(
                tranger,
                iterator,
                from_rowid,
                page_size,
                page_size,
                MAX_RECORDS,
                total_pages,
                0
            );
            MT_INCREMENT_COUNT(time_measure2, page_size)
            if(pinta_tiempos_intermedios) {
                MT_PRINT_TIME(time_measure2, "search_page")
            }
            if(result < 0) {
                break;
            }
        }
        if(from_rowid <= total_rows) { // if((total_rows%page_size)!=0) {
            time_measure_t time_measure2;
            MT_START_TIME(time_measure2)
            result += search_page(
                tranger,
                iterator,
                from_rowid,
                page_size,
                total_rows % page_size,
                MAX_RECORDS,
                total_pages,
                0
            );
            MT_INCREMENT_COUNT(time_measure2, total_rows % page_size)
            if(pinta_tiempos_intermedios) {
                MT_PRINT_TIME(time_measure2, "search_page")
            }
        }

        MT_INCREMENT_COUNT(time_measure, MAX_RECORDS)
        MT_PRINT_TIME(time_measure, TEST_NAME)

        result += test_json(NULL);  // NULL: we want to check only the logs

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
        result += test_json(NULL);  // NULL: we want to check only the logs
    }

    /*-------------------------------------*
     *  Search pages, backward
     *-------------------------------------*/
    if(test_backward) {
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

        json_t *match_cond = json_pack("{s:b}",
            "backward", 1
        );
        json_t *iterator = tranger2_open_iterator(
            tranger,
            TOPIC_NAME,
            KEY,                    // key
            match_cond,             // match_cond, owned
            NULL,                   // load_record_callback
            NULL,                   // rt_id
            NULL,                   // creator
            NULL,                   // data
            NULL                    // options
        );
        MT_INCREMENT_COUNT(time_measure, MAX_RECORDS)
        MT_PRINT_TIME(time_measure, "tranger2_open_iterator")
        result += test_json(NULL);  // NULL: we want to check only the logs

        /*-------------------------*
         *
         *-------------------------*/
        const char *TEST_NAME = "Search page BACKWARD";
        set_expected_results( // Check that no logs happen
            TEST_NAME, // test name
            NULL,   // error's list, It must not be any log error
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        json_int_t page_size = 41;
        size_t total_rows = tranger2_iterator_size(iterator);

        size_t pages = total_rows / page_size;
        size_t total_pages = pages + ((total_rows % page_size)?1:0);

        MT_START_TIME(time_measure)

        int page;
        json_int_t from_rowid;
        for(from_rowid=1, page=0; page<pages; page++, from_rowid += page_size) {
            time_measure_t time_measure2;
            MT_START_TIME(time_measure2)
            result += search_page(
                tranger,
                iterator,
                from_rowid,
                page_size,
                page_size,
                MAX_RECORDS,
                total_pages,
                1
            );
            MT_INCREMENT_COUNT(time_measure2, page_size)
            if(pinta_tiempos_intermedios) {
                MT_PRINT_TIME(time_measure2, "search_page")
            }
            if(result < 0) {
                break;
            }
        }
        if(from_rowid <= total_rows) { // if((total_rows%page_size)!=0) {
            time_measure_t time_measure2;
            MT_START_TIME(time_measure2)
            result += search_page(
                tranger,
                iterator,
                from_rowid,
                page_size,
                total_rows % page_size,
                MAX_RECORDS,
                total_pages,
                1
            );
            MT_INCREMENT_COUNT(time_measure2, total_rows % page_size)
            if(pinta_tiempos_intermedios) {
                MT_PRINT_TIME(time_measure2, "search_page")
            }
        }

        MT_INCREMENT_COUNT(time_measure, MAX_RECORDS)
        MT_PRINT_TIME(time_measure, TEST_NAME)

        result += test_json(NULL);  // NULL: we want to check only the logs

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
        result += test_json(NULL);  // NULL: we want to check only the logs
    }

    /*-------------------------------*
     *  tranger_backup_topic
     *-------------------------------*/
    // TODO old test in test_timeranger2.c

    /*-------------------------------*
     *      Shutdown timeranger
     *-------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger2_shutdown", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    result += debug_json("tranger", tranger, FALSE);

    tranger2_shutdown(tranger);
    result += test_json(NULL);  // NULL: we want to check only the logs

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
