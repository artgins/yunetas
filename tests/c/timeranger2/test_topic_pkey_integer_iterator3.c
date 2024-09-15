/****************************************************************************
 *          test_topic_pkey_integer_iterator3.c
 *
 *  - Open as master, open iterator to search with callback and data
 *  - Do all types of searches
 *
 *          Copyright (c) 2024 Niyamaka.
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

PRIVATE int pinta_md = 1;
PRIVATE int pinta_records = 0;

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
PRIVATE uint64_t leidos = 0;
PRIVATE json_int_t counter_rowid = 0;
PRIVATE json_t *callback_data = 0;

PRIVATE int load_rango_callback(
    json_t *tranger,
    json_t *topic,
    json_t *match_cond,     // not yours, don't own
    md2_record_t *md_record,
    json_t *record,      // must be owned
    const char *key,
    json_int_t rowid
)
{
    leidos++;
    counter_rowid++;

    if(pinta_md) {
        char temp[1024];
        print_md1_record(
            tranger,
            topic,
            md_record,
            key,
            rowid,
            temp,
            sizeof(temp)
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
    json_t *topic = tranger2_topic(tranger, TOPIC_NAME);
    json_t *match_cond = json_pack("{s:b, s:I, s:I}",
        "backward", BACKWARD,
        "from_rowid", (json_int_t)from_rowid,
        "to_rowid", (json_int_t)to_rowid
    );
    json_t *data = json_array();
    json_t *iterator = tranger2_open_iterator(
        tranger,
        topic,
        key,                    // key
        match_cond,             // match_cond, owned
        load_rango_callback,    // load_record_callback
        NULL,                   // id
        data                    // data
    );

    MT_INCREMENT_COUNT(time_measure, ROWS_EXPECTED)
    MT_PRINT_TIME(time_measure, test_name)

    // TRICK adjust the expected data
    if(from_rowid == 0) {
        // Asking from 0 must be equal to asking by 1
        from_rowid = 1;
    }
    if(to_rowid > MAX_RECORDS) {
        // Asking to > MAX_RECORDS must be equal to asking by MAX_RECORDS
        to_rowid = MAX_RECORDS;
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
    result += test_json(NULL, result);  // NULL: we want to check only the logs

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

    /*-------------------------------------*
     *  Search absolute range, forward
     *-------------------------------------*/
    BOOL test_forward = 0;

    if(test_forward) {
        const char *TEST_NAME = "Search absolute range 1-10, FORWARD (old 60.000 op/sec)";
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
        const char *TEST_NAME = "Search absolute range 1-10, BACKWARD (old 60.000 op/sec)";
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

    /*-------------------------------------*
     *  Search Relative range, forward
     *-------------------------------------*/
//    json_int_t from_rowid = -10;

    /*-------------------------------------*
     *  Search Relative range, backward
     *-------------------------------------*/
//    json_int_t from_rowid = -10;

    /*-------------------------------------*
     *  Search Relative range, forward
     *-------------------------------------*/
//    json_int_t from_rowid = -20;
//    json_int_t to_rowid = -10;

    /*-------------------------------------*
     *  Search Relative range, backward
     *-------------------------------------*/
//    json_int_t from_rowid = -20;
//    json_int_t to_rowid = -10;

    /*-------------------------------------*
     *  Search BAD Relative range, forward
     *-------------------------------------*/
//    json_int_t from_rowid = -10;
//    json_int_t to_rowid = -20;

    /*-------------------------------------*
     *      Search by rowid
     *-------------------------------------*/
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
