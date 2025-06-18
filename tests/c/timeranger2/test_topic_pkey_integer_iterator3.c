/****************************************************************************
 *          test_topic_pkey_integer_iterator3.c
 *
 *  - Open as master, open iterator to search with callback and data
 *  - Do ABSOLUTE searches
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>

#include <gobj.h>
#include <timeranger2.h>
#include <kwid.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP "test_topic_pkey_integer_iterator3"

#define DATABASE    "tr_topic_pkey_integer"
#define TOPIC_NAME  "topic_pkey_integer"
#define MAX_KEYS    2
#define MAX_RECORDS 90000 // 1 day and 1 hour

PRIVATE int pinta_md = 1;
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
PRIVATE uint64_t leidos = 0;
PRIVATE json_int_t counter_rowid = 0;
PRIVATE json_t *callback_data = 0;

PRIVATE int load_rango_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record      // must be owned
)
{
    leidos++;
    counter_rowid++;

    if(pinta_md) {
        char temp[1024];
        tranger2_print_md1_record(
            temp,
            sizeof(temp),
            key,
            rowid,
            md_record,
            FALSE
        );
        printf("%s\n", temp);
    }
    if(pinta_records) {
        print_json2("record", record);
    }

    json_t *md = json_object();
    json_object_set_new(md, "rowid", json_integer(rowid));
    json_object_set_new(md, "t", json_integer((json_int_t)md_record->__t__));
    json_array_append_new(callback_data, md);

    JSON_DECREF(record)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int search_data(
    json_t *tranger,
    const char *key,
    const char *TEST_NAME,
    BOOL BACKWARD,
    json_int_t FROM_ROWID,
    json_int_t TO_ROWID,
    json_int_t ROWS_EXPECTED
)
{
    int result = 0;

    const char *test_name = TEST_NAME;

    set_expected_results( // Check that no logs happen
        test_name, // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        TRUE    // verbose
    );

    json_int_t from_rowid = FROM_ROWID;
    json_int_t to_rowid = TO_ROWID;

    time_measure_t time_measure;
    MT_START_TIME(time_measure)

    JSON_DECREF(callback_data)
    callback_data = json_array();

    leidos = 0;
    counter_rowid = from_rowid;
    json_t *match_cond = json_pack("{s:b, s:I, s:I}",
        "backward", BACKWARD,
        "from_rowid", (json_int_t)from_rowid,
        "to_rowid", (json_int_t)to_rowid
    );
    json_t *data = json_array();

    json_t *iterator = tranger2_open_iterator(
        tranger,
        TOPIC_NAME,
        key,                    // key
        match_cond,             // match_cond, owned
        load_rango_callback,    // load_record_callback
        NULL,                   // rt_id
        NULL,                   // creator
        data,                   // data
        NULL                    // options
    );

    MT_INCREMENT_COUNT(time_measure, ROWS_EXPECTED)
    MT_PRINT_TIME(time_measure, test_name)

//    // TRICK adjust the expected data
//    if(from_rowid == 0) {
//        // Asking from 0 must be equal to asking by 1
//        from_rowid = 1;
//    }
//    if(to_rowid > MAX_RECORDS) {
//        // Asking to > MAX_RECORDS must be equal to asking by MAX_RECORDS
//        to_rowid = MAX_RECORDS;
//    }

    // TRICK adjust the expected data
    // WARNING adjust REPEATED
    if(from_rowid == 0) {
        from_rowid = 1;
    } else if(from_rowid > 0) {
        // positive offset
    } else {
        // negative offset
        if(from_rowid < -MAX_RECORDS) {
            // out of range, begin at 0
            from_rowid = 1;
        } else {
            from_rowid = MAX_RECORDS + from_rowid + 1;
        }
    }

    // WARNING adjust REPEATED
    if(to_rowid == 0) {
        to_rowid = MAX_RECORDS;
    } else if(to_rowid > 0) {
        // positive offset
        if(to_rowid > MAX_RECORDS) {
            // out of range, begin at 0
            to_rowid = MAX_RECORDS;
        }
    } else {
        // negative offset
        if(to_rowid < -MAX_RECORDS) {
            // not exist
            return -1;
        } else {
            to_rowid = MAX_RECORDS + to_rowid + 1;
        }
    }

    /*
     *  Test data
     */
    json_t *matches = json_array();

    if(!BACKWARD) {
        json_int_t t1 = 946684800 + from_rowid - 1; // 2000-01-01T00:00:00+0000
        for(int i=0; i<ROWS_EXPECTED; i++){
            json_t *match = json_pack("{s:I}",
                "tm", t1 + i
            );
            json_array_append_new(matches, match);
        }
    } else {
        json_int_t t1 = 946684800 + to_rowid - 1; // 2000-01-01T00:00:00+0000
        for(int i=0; i<ROWS_EXPECTED; i++) {
            json_t *match = json_pack("{s:I}",
                "tm", t1 - i
            );
            json_array_append_new(matches, match);
        }
    }

    result += test_list(data, matches, "%s - %s", TEST_NAME, "data");
    JSON_DECREF(matches)
    JSON_DECREF(data)

    /*
     *  Test callback data
     */
    matches = json_array();

    if(!BACKWARD) {
        json_int_t t1 = 946684800 + from_rowid - 1; // 2000-01-01T00:00:00+0000
        for(int i=0; i<ROWS_EXPECTED; i++) {
            json_t *match = json_pack("{s:I, s:I}",
                "rowid", from_rowid + i,
                "t", t1 + i
            );
            json_array_append_new(matches, match);
        }
    } else {
        json_int_t t1 = 946684800 + to_rowid - 1; // 2000-01-01T00:00:00+0000
        for(int i=0; i<ROWS_EXPECTED; i++){
            json_t *match = json_pack("{s:I, s:I}",
                "rowid", to_rowid - i,
                "t", t1 - i
            );
            json_array_append_new(matches, match);
        }
    }

    result += test_list(callback_data, matches, "%s - %s", TEST_NAME, "callback_data");

    JSON_DECREF(matches)
    JSON_DECREF(callback_data)

    result += tranger2_close_iterator(tranger, iterator);
    result += test_json(NULL);  // NULL: we want to check only the logs

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
    json_t *tranger = tranger2_startup(0, jn_tranger, 0);
    result += test_json(NULL);  // NULL: we want to check only the logs

    /*-------------------------------------*
     *  Search absolute range, forward
     *-------------------------------------*/
    BOOL test_forward = 1;

    if(test_forward) {
        const char *TEST_NAME = "Search absolute range 1-10, FORWARD";
        BOOL BACKWARD               = 0;
        json_int_t FROM_ROWID       = 1;
        json_int_t TO_ROWID         = 10;
        json_int_t ROWS_EXPECTED    = 10;
        const char *KEY             = "0000000000000000001";

        result += search_data(
            tranger,
            KEY,
            TEST_NAME,
            BACKWARD,
            FROM_ROWID,
            TO_ROWID,
            ROWS_EXPECTED
        );
    }

    if(test_forward) {
        const char *TEST_NAME = "Search absolute range 0-10, FORWARD";
        BOOL BACKWARD               = 0;
        json_int_t FROM_ROWID       = 0;
        json_int_t TO_ROWID         = 10;
        json_int_t ROWS_EXPECTED    = 10;
        const char *KEY             = "0000000000000000001";

        result += search_data(
            tranger,
            KEY,
            TEST_NAME,
            BACKWARD,
            FROM_ROWID,
            TO_ROWID,
            ROWS_EXPECTED
        );
    }

    if(test_forward) {
        const char *TEST_NAME = "Search absolute range 89991-90000, FORWARD";
        BOOL BACKWARD               = 0;
        json_int_t FROM_ROWID       = 89991;
        json_int_t TO_ROWID         = 90000;
        const char *KEY             = "0000000000000000001";
        json_int_t ROWS_EXPECTED    = 10;

        result += search_data(
            tranger,
            KEY,
            TEST_NAME,
            BACKWARD,
            FROM_ROWID,
            TO_ROWID,
            ROWS_EXPECTED
        );
    }

    if(test_forward) {
        const char *TEST_NAME = "Search absolute range 89991-90001, FORWARD";
        BOOL BACKWARD               = 0;
        json_int_t FROM_ROWID       = 89991;
        json_int_t TO_ROWID         = 90001;
        const char *KEY             = "0000000000000000001";
        json_int_t ROWS_EXPECTED    = 10;

        result += search_data(
            tranger,
            KEY,
            TEST_NAME,
            BACKWARD,
            FROM_ROWID,
            TO_ROWID,
            ROWS_EXPECTED
        );
    }

    if(test_forward) {
        const char *TEST_NAME = "Search absolute range 10-10, FORWARD";
        BOOL BACKWARD               = 0;
        json_int_t FROM_ROWID       = 10;
        json_int_t TO_ROWID         = 10;
        const char *KEY             = "0000000000000000001";
        json_int_t ROWS_EXPECTED    = 1;

        result += search_data(
            tranger,
            KEY,
            TEST_NAME,
            BACKWARD,
            FROM_ROWID,
            TO_ROWID,
            ROWS_EXPECTED
        );
    }

    if(test_forward) {
        const char *TEST_NAME = "Search absolute BAD range 10-9, FORWARD";
        BOOL BACKWARD               = 0;
        json_int_t FROM_ROWID       = 10;
        json_int_t TO_ROWID         = 9;
        const char *KEY             = "0000000000000000001";
        json_int_t ROWS_EXPECTED    = 0;

        result += search_data(
            tranger,
            KEY,
            TEST_NAME,
            BACKWARD,
            FROM_ROWID,
            TO_ROWID,
            ROWS_EXPECTED
        );
    }

    if(test_forward) {
        const char *TEST_NAME = "Search absolute range MIDDLE-5 - MIDDEL+5, FORWARD";
        BOOL BACKWARD               = 0;
        // TRICK last row of the first segment: 86400
        json_int_t FROM_ROWID       = 86400 - 5;
        json_int_t TO_ROWID         = 86400 + 5 - 1;
        const char *KEY             = "0000000000000000001";
        json_int_t ROWS_EXPECTED    = 10;

        result += search_data(
            tranger,
            KEY,
            TEST_NAME,
            BACKWARD,
            FROM_ROWID,
            TO_ROWID,
            ROWS_EXPECTED
        );
    }

    /*-------------------------------------*
     *  Search Absolute range, backward
     *-------------------------------------*/
    BOOL test_backward = 1;

    if(test_backward) {
        const char *TEST_NAME = "Search absolute range 1-10, BACKWARD";
        BOOL BACKWARD               = 1;
        json_int_t FROM_ROWID       = 1;
        json_int_t TO_ROWID         = 10;
        json_int_t ROWS_EXPECTED    = 10;
        const char *KEY             = "0000000000000000001";

        result += search_data(
            tranger,
            KEY,
            TEST_NAME,
            BACKWARD,
            FROM_ROWID,
            TO_ROWID,
            ROWS_EXPECTED
        );
    }

    if(test_backward) {
        const char *TEST_NAME = "Search absolute range 0-10, BACKWARD";
        BOOL BACKWARD               = 1;
        json_int_t FROM_ROWID       = 0;
        json_int_t TO_ROWID         = 10;
        json_int_t ROWS_EXPECTED    = 10;
        const char *KEY             = "0000000000000000001";

        result += search_data(
            tranger,
            KEY,
            TEST_NAME,
            BACKWARD,
            FROM_ROWID,
            TO_ROWID,
            ROWS_EXPECTED
        );
    }

    if(test_backward) {
        const char *TEST_NAME = "Search absolute range 89991-90000, BACKWARD";
        BOOL BACKWARD               = 1;
        json_int_t FROM_ROWID       = 89991;
        json_int_t TO_ROWID         = 90000;
        const char *KEY             = "0000000000000000001";
        json_int_t ROWS_EXPECTED    = 10;

        result += search_data(
            tranger,
            KEY,
            TEST_NAME,
            BACKWARD,
            FROM_ROWID,
            TO_ROWID,
            ROWS_EXPECTED
        );
    }

    if(test_backward) {
        const char *TEST_NAME = "Search absolute range 89991-90001, BACKWARD";
        BOOL BACKWARD               = 1;
        json_int_t FROM_ROWID       = 89991;
        json_int_t TO_ROWID         = 90001;
        const char *KEY             = "0000000000000000001";
        json_int_t ROWS_EXPECTED    = 10;

        result += search_data(
            tranger,
            KEY,
            TEST_NAME,
            BACKWARD,
            FROM_ROWID,
            TO_ROWID,
            ROWS_EXPECTED
        );
    }

    if(test_backward) {
        const char *TEST_NAME = "Search absolute range 10-10, BACKWARD";
        BOOL BACKWARD               = 1;
        json_int_t FROM_ROWID       = 10;
        json_int_t TO_ROWID         = 10;
        const char *KEY             = "0000000000000000001";
        json_int_t ROWS_EXPECTED    = 1;

        result += search_data(
            tranger,
            KEY,
            TEST_NAME,
            BACKWARD,
            FROM_ROWID,
            TO_ROWID,
            ROWS_EXPECTED
        );
    }

    if(test_backward) {
        const char *TEST_NAME = "Search absolute BAD range 10-9, BACKWARD";
        BOOL BACKWARD               = 1;
        json_int_t FROM_ROWID       = 10;
        json_int_t TO_ROWID         = 9;
        const char *KEY             = "0000000000000000001";
        json_int_t ROWS_EXPECTED    = 0;

        result += search_data(
            tranger,
            KEY,
            TEST_NAME,
            BACKWARD,
            FROM_ROWID,
            TO_ROWID,
            ROWS_EXPECTED
        );
    }

    if(test_backward) {
        const char *TEST_NAME = "Search absolute range MIDDLE-5 - MIDDEL+5, BACKWARD";
        BOOL BACKWARD               = 1;
        // TRICK last row of the first segment: 86400
        json_int_t FROM_ROWID       = 86400 - 5;
        json_int_t TO_ROWID         = 86400 + 5 - 1;
        const char *KEY             = "0000000000000000001";
        json_int_t ROWS_EXPECTED    = 10;

        result += search_data(
            tranger,
            KEY,
            TEST_NAME,
            BACKWARD,
            FROM_ROWID,
            TO_ROWID,
            ROWS_EXPECTED
        );
    }

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
//    gobj_set_global_trace(0, true);     // TODO TEST

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
        NULL    // global_authenticate_parser
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
