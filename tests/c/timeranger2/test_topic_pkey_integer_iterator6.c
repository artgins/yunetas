/****************************************************************************
 *          test_topic_pkey_integer_iterator6.c
 *
 *  - Open as master, open iterator to search with callback and data
 *  - Add records in realtime
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
#define MAX_KEYS    1 // TODO 2
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

PRIVATE uint64_t leidos_by_mem = 0;
PRIVATE uint64_t leidos_by_disk = 0;
PRIVATE json_int_t counter_rowid_by_mem = 0;
PRIVATE json_int_t counter_rowid_by_disk = 0;
PRIVATE json_int_t last_rowid_by_mem = 0;
PRIVATE json_int_t last_rowid_by_disk = 0;
PRIVATE uint64_t last_t_by_mem = 0;
PRIVATE uint64_t last_t_by_disk = 0;
PRIVATE json_t *callback_data_by_mem = 0;
PRIVATE json_t *callback_data_by_disk = 0;

PRIVATE int rt_mem_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *rt_id,
    json_int_t relative_rowid,
    md2_record_t *md_record,
    json_t *record      // must be owned
)
{
    leidos_by_mem++;
    counter_rowid_by_mem++;
    last_rowid_by_mem = relative_rowid;
    last_t_by_mem = md_record->__t__;

    if(pinta_md) {
        char temp[1024];
        tranger2_print_md1_record(
            tranger,
            topic,
            md_record,
            key,
            relative_rowid,
            temp,
            sizeof(temp)
        );
        printf("BY MEM: %s\n", temp);
    }
    if(pinta_records) {
        print_json2("MEM record", record);
    }

    json_t *md = json_object();
    json_object_set_new(md, "rowid", json_integer(relative_rowid));
    json_object_set_new(md, "t", json_integer((json_int_t)md_record->__t__));
    json_array_append_new(callback_data_by_mem, md);

    JSON_DECREF(record)
    return 0;
}

PRIVATE int rt_disk_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    const char *rt_id,
    json_int_t rowid,
    md2_record_t *md_record,
    json_t *record      // must be owned
)
{
    leidos_by_disk++;
    counter_rowid_by_disk++;
    last_rowid_by_disk = rowid;
    last_t_by_disk = md_record->__t__;

    if(pinta_md) {
        char temp[1024];
        tranger2_print_md1_record(
            tranger,
            topic,
            md_record,
            key,
            rowid,
            temp,
            sizeof(temp)
        );
        printf("BY DISK: %s\n", temp);
    }
    if(pinta_records) {
        print_json2("DISK record", record);
    }

    json_t *md = json_object();
    json_object_set_new(md, "rowid", json_integer(rowid));
    json_object_set_new(md, "t", json_integer((json_int_t)md_record->__t__));
    json_array_append_new(callback_data_by_disk, md);

    JSON_DECREF(record)
    return 0;
}

/***************************************************************************
 *              Test
 *  Open as master, open iterator (realtime by disk) with callback
 *  HACK: return -1 to fail, 0 to ok
 ***************************************************************************/
PRIVATE int do_test(json_t *tranger)
{
    int result = 0;

    /*-------------------------------------*
     *  Monitoring a key, last 10 records
     *-------------------------------------*/
    if(1) {
        const char *KEY = "0000000000000000001";
        json_t *topic = tranger2_topic(tranger, TOPIC_NAME);
        time_measure_t time_measure;
        json_t *match_cond;

        BOOL by_mem = 0;
        BOOL by_disk = 1;

        /*-----------------------*
         *      By mem
         *-----------------------*/
        if(by_mem) {
            set_expected_results( // Check that no logs happen
                "tranger2_open_iterator by mem", // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );
            MT_START_TIME(time_measure)

            match_cond = json_pack("{s:b, s:b, s:i}",
                "backward", 0,
                "rt_by_mem", 1,
                "from_rowid", -10
            );
            tranger2_open_iterator(
                tranger,
                topic,
                KEY,                    // key
                match_cond,             // match_cond, owned
                rt_mem_record_callback, // load_record_callback
                "it_by_mem",            // id
                NULL                    // data
            );

            MT_INCREMENT_COUNT(time_measure, MAX_RECORDS)
            MT_PRINT_TIME(time_measure, "tranger2_open_iterator by mem")
            result += test_json(NULL, result);  // NULL: we want to check only the logs
        }

        /*-----------------------*
         *      By disk
         *-----------------------*/
        if(by_disk) {
            set_expected_results( // Check that no logs happen
                "tranger2_open_iterator by disk", // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );
            MT_START_TIME(time_measure)

            match_cond = json_pack("{s:b, s:b, s:i}",
                "backward", 0,
                "rt_by_mem", 0,
                "from_rowid", -10
            );
            tranger2_open_iterator(
                tranger,
                topic,
                KEY,                    // key
                match_cond,             // match_cond, owned
                rt_disk_record_callback, // load_record_callback
                "it_by_disk",           // id
                NULL                    // data
            );

            MT_INCREMENT_COUNT(time_measure, MAX_RECORDS)
            MT_PRINT_TIME(time_measure, "tranger2_open_iterator by disk")
            result += test_json(NULL, result);  // NULL: we want to check only the logs
        }

        yev_loop_run_once(yev_loop);

        /*-------------------------------------*
         *      Add records
         *-------------------------------------*/
        set_expected_results( // Check that no logs happen
            "append records", // test name
            NULL,   // error's list, It must not be any log error
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );

        MT_START_TIME(time_measure)

        uint64_t t1 = 946774800; // 2000-01-01T00:00:00+0000 2000-01-02T01:00:00+0000
        for(json_int_t i=0; i<MAX_KEYS; i++) {
            uint64_t tm = t1;
            for(json_int_t j=0; j<MAX_RECORDS; j++) {
                json_t *jn_record1 = json_pack("{s:I, s:I, s:s}",
                   "id", i + 1,
                   "tm", tm+j,
                   "content",
                   "Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el."
                   "Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el."
                   "Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el alfa.Pepe el.x"
                );
                md2_record_t md_record;
                tranger2_append_record(tranger, TOPIC_NAME, tm+j, 0, &md_record, jn_record1);
            }
        }

        MT_INCREMENT_COUNT(time_measure, MAX_RECORDS)
        MT_PRINT_TIME(time_measure, "append records")

        if(by_mem) {
            if(last_t_by_mem != 946864799) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "bad time by mem");
            }
        }

        if(by_disk) {
            if(last_t_by_disk != 946864799) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "bad time by mem");
            }
        }

        result += test_json(NULL, result);  // NULL: we want to check only the logs
    }

    /*-------------------------------*
     *  tranger_backup_topic
     *-------------------------------*/
    // TODO old test in test_timeranger2.c

    return result;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *open_all(void)
{
    global_result = 0;
    int result = 0;

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
    json_t *tranger = tranger2_startup(0, jn_tranger, yev_loop);
    global_result += test_json(NULL, result);  // NULL: we want to check only the logs

    return tranger;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int close_all(json_t *tranger)
{
    int result = 0;
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
    if(tranger2_get_iterator_by_id(tranger,"it_by_mem")) {
        result += tranger2_close_iterator(
            tranger,
            tranger2_get_iterator_by_id(tranger,"it_by_mem")
        );
    }
    if(tranger2_get_iterator_by_id(tranger,"it_by_disk")) {
        result += tranger2_close_iterator(
            tranger,
            tranger2_get_iterator_by_id(tranger,"it_by_disk")
        );
    }
    result += test_json(NULL, result);  // NULL: we want to check only the logs

    yev_loop_run_once(yev_loop);

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

    return result;
}

/***************************************************************************
 *  Finish by timeout if test don't end in time
 ***************************************************************************/
PRIVATE int yev_timer_do_finish_callback(yev_event_t *yev_event)
{
    global_result += -1;
    printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "finish by timeout");
    yev_loop->running = 0;
    return 0;
}
/***************************************************************************
 *  Here do all test
 ***************************************************************************/
PRIVATE int yev_timer_do_test_callback(yev_event_t *yev_event)
{
    json_t *tranger = yev_event->gobj;
    yev_stop_event(yev_event);
    yev_destroy_event(yev_event);
    global_result += do_test(tranger);
    return 0;
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
    json_t *tranger = open_all();

    yev_event_t *yev_timer_test = yev_create_timer_event(
        yev_loop, yev_timer_do_test_callback, tranger
    );
    yev_start_timer_event(yev_timer_test, 100, FALSE);

    yev_event_t *yev_timer_finish = yev_create_timer_event(
        yev_loop, yev_timer_do_finish_callback, tranger
    );
    yev_start_timer_event(yev_timer_finish, 10*1000, FALSE);

    yev_loop_run(yev_loop);

    int result = close_all(tranger);

    /*--------------------------------*
     *  Stop the event loop
     *--------------------------------*/
    yev_stop_event(yev_timer_finish);
    yev_destroy_event(yev_timer_finish);

    yev_loop_run_once(yev_loop);

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory()!=0) {
        printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "system memory not free");
        result += -1;
    }
    result += global_result;

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
