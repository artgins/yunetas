/****************************************************************************
 *          test.c
 *
 *          Copyright (c) 2018 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <inttypes.h>
#include <locale.h>
#include <time.h>
#include <sys/resource.h>

#include <yunetas.h>

#define APP "test_tr_msg2"

/***************************************************************************
 *      Constants
 ***************************************************************************/
#define DATABASE    "tr_msg"
#define TOPIC_NAME  "gpss2"
#define DEVICES 1000
#define TRAZAS  100

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE int global_result = 0;
PRIVATE json_t *hrc2_topic_iter1 = 0;
PRIVATE json_t *hrc2_topic_iter2 = 0;

/***************************************************************************
 *
 ***************************************************************************/
static int test(json_t *tranger, int caso, const char *desc)
{
    int result = 0;
    uint64_t cnt = 1;

    /*-------------------------------------*
     *  Loop
     *-------------------------------------*/
    switch(caso) {
    case 1:
        {
            const char *test_name = desc;

            json_t *error_list = json_array();
            for(int i=0; i<100; i++) {
                json_t *err = json_pack("{s:s}",
                    "msg", "TOO MANY OPEN FILES 2"
                );
                json_array_append_new(error_list, err);
            }

            set_expected_results( // Check that no logs happen
                test_name, // test name
                error_list,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            for(long trace=1; trace<=TRAZAS; trace++) { // traces de 1 hora, a 1/segundo
                for(int imei=1; imei<=DEVICES; imei++) {
                    const char *event = "CycleOff";
                    char simei[32];
                    snprintf(simei, sizeof(simei), "%016d", imei);
                    trmsg_add_instance(
                        tranger,
                        TOPIC_NAME,     // topic
                        json_pack("{s:s, s:s, s:I, s:f, s:f, s:i, s:i, s:b, s:b}",
                            "imei", simei,
                            "event", event,
                            "gps_date", (json_int_t)trace,
                            "latitude", 0.0,
                            "longitude", 0.0,
                            "altitude", 0,
                            "heading", 0,
                            "on", 0,
                            "idle", 0
                        ),
                        0
                    );
                }
            }

            cnt = DEVICES * TRAZAS;
            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL);  // NULL: we want to check only the logs
        }
        break;

    case 2:
        {
            const char *test_name = desc;
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            hrc2_topic_iter1 = trmsg_open_list(
                tranger,
                TOPIC_NAME,    // topic
                json_pack("{s:s}",  // match_cond
                    "rkey", ""
                ),
                NULL,
                "",     // rt_id
                FALSE,  // rt_by_disk
                ""      // creator
            );

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL);  // NULL: we want to check only the logs
        }
        break;

    case 3:
        {
            const char *test_name = desc;
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            hrc2_topic_iter2 = trmsg_open_list(
                tranger,
                TOPIC_NAME,    // topic
                json_pack("{s:s, s:b, s:b}",  // match_cond
                    "rkey", "",
                    "backward", 1,
                    "order_by_tm", 1
                ),
                NULL,
                "",     // rt_id
                FALSE,  // rt_by_disk
                ""      // creator
            );

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL);  // NULL: we want to check only the logs
        }
        break;

    case 4:
        {
            const char *test_name = desc;
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            cnt = 1;
            const char *key = "0000000000000001";

            json_t *msg = trmsg_get_active_message(
                hrc2_topic_iter1,
                key
            );
            if(!msg) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "No message found");
            }
            const char *key_ = kw_get_str(0, msg, "imei", "", KW_REQUIRED);
            if(strcmp(key, key_)!=0) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD id");
            }
            json_int_t gps_date = kw_get_int(0, msg, "gps_date", -1, KW_REQUIRED);
            if(gps_date != 100) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD gps_date");
            }

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL);  // NULL: we want to check only the logs
        }
        break;

    case 5:
        {
            const char *test_name = desc;
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            cnt = 1;
            const char *key = "0000000000000500";
            json_t *msg = trmsg_get_active_message(
                hrc2_topic_iter1,
                key
            );
            if(!msg) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "No message found");
            }
            const char *key_ = kw_get_str(0, msg, "imei", "", KW_REQUIRED);
            if(strcmp(key, key_)!=0) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD id");
            }
            json_int_t gps_date = kw_get_int(0, msg, "gps_date", -1, KW_REQUIRED);
            if(gps_date != 100) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD gps_date");
            }

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL);  // NULL: we want to check only the logs
        }
        break;

    case 6:
        {
            const char *test_name = desc;
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            cnt = 1;
            const char *key = "0000000000000999";
            json_t *msg = trmsg_get_active_message(
                hrc2_topic_iter1,
                key
            );
            if(!msg) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "No message found");
            }
            const char *key_ = kw_get_str(0, msg, "imei", "", KW_REQUIRED);
            if(strcmp(key, key_)!=0) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD id");
            }
            json_int_t gps_date = kw_get_int(0, msg, "gps_date", -1, KW_REQUIRED);
            if(gps_date != 100) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD gps_date");
            }

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL);  // NULL: we want to check only the logs
        }
        break;

    case 7:
        {
            const char *test_name = desc;
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            cnt = 1;
            const char *key = "0000000000000001";
            json_t *msg = trmsg_get_active_message(
                hrc2_topic_iter2,
                key
            );
            if(!msg) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "No message found");
            }
            const char *key_ = kw_get_str(0, msg, "imei", "", KW_REQUIRED);
            if(strcmp(key, key_)!=0) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD id");
            }
            json_int_t gps_date = kw_get_int(0, msg, "gps_date", -1, KW_REQUIRED);
            if(gps_date != 100) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD gps_date");
            }

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL);  // NULL: we want to check only the logs
        }
        break;

    case 8:
        {
            const char *test_name = desc;
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            cnt = 1;
            const char *key = "0000000000000500";
            json_t *msg = trmsg_get_active_message(
                hrc2_topic_iter2,
                key
            );
            if(!msg) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "No message found");
            }
            const char *key_ = kw_get_str(0, msg, "imei", "", KW_REQUIRED);
            if(strcmp(key, key_)!=0) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD id");
            }
            json_int_t gps_date = kw_get_int(0, msg, "gps_date", -1, KW_REQUIRED);
            if(gps_date != 100) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD gps_date");
            }

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL);  // NULL: we want to check only the logs
        }
        break;

    case 9:
        {
            const char *test_name = desc;
            set_expected_results( // Check that no logs happen
                test_name, // test name
                NULL,   // error's list, It must not be any log error
                NULL,   // expected, NULL: we want to check only the logs
                NULL,   // ignore_keys
                TRUE    // verbose
            );

            time_measure_t time_measure;
            MT_START_TIME(time_measure)

            cnt = 1;
            const char *key = "0000000000000999";
            json_t *msg = trmsg_get_active_message(
                hrc2_topic_iter2,
                key
            );
            if(!msg) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "No message found");
            }
            const char *key_ = kw_get_str(0, msg, "imei", "", KW_REQUIRED);
            if(strcmp(key, key_)!=0) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD id");
            }
            json_int_t gps_date = kw_get_int(0, msg, "gps_date", -1, KW_REQUIRED);
            if(gps_date != 100) {
                result += -1;
                printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "BAD gps_date");
            }

            MT_INCREMENT_COUNT(time_measure, cnt)
            MT_PRINT_TIME(time_measure, test_name)

            result += test_json(NULL);  // NULL: we want to check only the logs
        }
        break;

    default:
        printf("MIERDA\n");
        result += -1;
    }

    return result;
}

/***************************************************************************
 *              Test
 *  Open as master, check main files, add records, open rt lists
 *  HACK: return -1 to fail, 0 to ok
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

    /*-------------------------------------------------*
     *      Startup the timeranger db
     *-------------------------------------------------*/
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", 0,
        "filename_mask", "%Y",
        "xpermission" , 02770,
        "rpermission", 0600,
        "trace_level", 1
    );
    json_t *tranger = tranger2_startup(0, jn_tranger, 0);

    /*------------------------------*
     *  Crea la bbdd
     *------------------------------*/
    static const json_desc_t traces_json_desc[] = {
        // Name             Type        Default
        {"imei",            "str",      "",     0},
        {"event",           "str",      "",     0},
        {"gps_date",        "int",      "",     0},
        {"latitude",        "int",      "",     0},
        {"longitude",       "int",      "",     0},
        {"altitude",        "int",      "",     0},
        {"heading",         "int",      "",     0},
        {"on",              "bool",     "false",0},
        {"idle",            "bool",     "TRUE", 0},
        {0}
    };

    static topic_desc_t db_test_desc[] = {
    // Topic Name,  Pkey    Key Type            Tkey            Cols                Topic ext
    {TOPIC_NAME,    "imei", sf_string_key,      "gps_date",     traces_json_desc,   NULL},
    {0}
    };

    result += trmsg_open_topics(tranger, db_test_desc);

    /*------------------------------*
     *  Ejecuta los tests
     *------------------------------*/
    struct rlimit rl;

    // Get current limit
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        printf("Current limit: soft = %ld, hard = %ld\n", (long)rl.rlim_cur, (long)rl.rlim_max);
    } else {
        result += -1;
        printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "Error getrlimit()");
    }

    // Set new limit
    rl.rlim_cur = 2000;  // Set soft limit
    rl.rlim_max = 2000;  // Set hard limit
    if (setrlimit(RLIMIT_NOFILE, &rl) == 0) {
        printf("New limit set: soft = %ld, hard = %ld\n", (long)rl.rlim_cur, (long)rl.rlim_max);
    } else {
        result += -1;
        printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "Error setrlimit()");
    }

    result += test(tranger, 2, "LOAD FORWARD");
    result += test(tranger, 3, "LOAD BACKWARD/TM");
    result += test(tranger, 1, "ADD RECORDS");
    result += test(tranger, 4, "FIND fore first");
    result += test(tranger, 5, "FIND fore medium");
    result += test(tranger, 6, "FIND fore last");
    result += test(tranger, 7, "FIND back first");
    result += test(tranger, 8, "FIND back medium");
    result += test(tranger, 9, "FIND back last");

    trmsg_close_list(tranger, hrc2_topic_iter1);
    trmsg_close_list(tranger, hrc2_topic_iter2);

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
    tranger2_shutdown(tranger);
    result += test_json(NULL);  // NULL: we want to check only the logs

    return result;
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

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

    unsigned long memory_check_list[] = {0, 0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_setup_memory(
        256*1024L,          // max_block, largest memory block
        1024*1024*1024L,    // max_system_memory, maximum system memory
        FALSE,
        0,
        0
    );
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
    result += global_result;

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
