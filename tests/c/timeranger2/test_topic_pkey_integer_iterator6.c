/****************************************************************************
 *          test_topic_pkey_integer_iterator6.c
 *
 *  - Open as master, open a iterator by MEM if master, by DISK if non-master,
 *      to search with callback and data
 *  - Add records in realtime
 *
 *  To run this test, you must:
 *      1) Firstly run ./tests/c/timeranger2/test_topic_pkey_integer to reset the bbdd
 *      2) Run this test as client:
 *          ./tests/c/timeranger2/test_topic_pkey_integer_iterator6 -c
 *      3) Run this test as master
 *          ./tests/c/timeranger2/test_topic_pkey_integer_iterator6
 *
 *      If you want to test several clients, run each client with different name in option -n :
 *          ./tests/c/timeranger2/test_topic_pkey_integer_iterator6 -c --name=pepe0
 *          ./tests/c/timeranger2/test_topic_pkey_integer_iterator6 -c --name=pepe1
 *          ./tests/c/timeranger2/test_topic_pkey_integer_iterator6 -c --name=pepe2
 *
 *  Performance, in my machine, 17-Nov-2024:
 *
 *      With one client:
 *          MASTER (count: 180000): 2.617252 seconds, 68774 op/sec
 *          CLIENT (count: 180000): 2.616992 seconds, 68781 op/sec
 *
 *      With two client:
 *          MASTER  (count: 180000): 3.937710 seconds, 45711 op/sec
 *          CLIENT1 (count: 180000): 3.937640 seconds, 45712 op/sec
 *          CLIENT2 (count: 180000): 3.937661 seconds, 45712 op/sec
 *
 *      With three client:
 *          MASTER  (count: 180000): 4.628994 seconds, 38885 op/sec
 *          CLIENT1 (count: 180000): 4.629067 seconds, 38884 op/sec
 *          CLIENT2 (count: 180000): 4.629221 seconds, 38883 op/sec
 *          CLIENT3 (count: 180000): 4.628958 seconds, 38885 op/sec
 *
 *      With four client:
 *          MASTER  (count: 180000): 4.771080 seconds, 37727 op/sec
 *          CLIENT1 (count: 180000): 4.771036 seconds, 37727 op/sec
 *          CLIENT2 (count: 180000): 4.771012 seconds, 37727 op/sec
 *          CLIENT3 (count: 180000): 4.771083 seconds, 37727 op/sec
 *          CLIENT4 (count: 180000): 4.771046 seconds, 37727 op/sec
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>

#include <gobj.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <kwid.h>
#include <testing.h>

#define APP     "test_topic_pkey_integer_iterator6"
#define DOC     "test_topic_pkey_integer_iterator6"
#define SUPPORT "<niyamaka at yuneta.io>"
#define VERSION "1.0"

#define DATABASE    "tr_topic_pkey_integer"
#define TOPIC_NAME  "topic_pkey_integer"
#define MAX_KEYS    2
#define MAX_RECORDS 90000 // 1 day and 1 hour

PRIVATE int pinta_md = 1;
PRIVATE int pinta_records = 0;

/***************************************************************************
 *              Arguments
 ***************************************************************************/
#include <argp-standalone.h>

#define MIN_ARGS 0
#define MAX_ARGS 0
struct arguments {
    char *args[MAX_ARGS+1];     /* positional args */

    int client;
    char *name;
};

const char *argp_program_version = APP " " VERSION;
const char *argp_program_bug_address = SUPPORT;

/* Program documentation. */
static char doc[] = DOC;

/* A description of the arguments we accept. */
static char args_doc[] = "";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
/*-name---------key-----arg---------flags---doc---------------------------------group */
{"client",      'c',    0,          0,      "Run as client, default is master", 0},
{"name",        'n',    "NAME",     0,      "Run as client opening list with this name", 0},
{0}
};

static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
    /*
     *  Get the input argument from argp_parse,
     *  which we know is a pointer to our arguments structure.
     */
    struct arguments *arguments_ = state->input;

    switch (key) {
    case 'c':
        arguments_->client = 1;
        break;
    case 'n':
        arguments_->name= arg;
        break;
    case ARGP_KEY_ARG:
        if (state->arg_num >= MAX_ARGS) {
            /* Too many arguments_. */
            argp_usage (state);
        }
        arguments_->args[state->arg_num] = arg;
        break;

    case ARGP_KEY_END:
        if (state->arg_num < MIN_ARGS) {
            /* Not enough arguments_. */
            argp_usage (state);
        }
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/* Our argp parser. */
static struct argp argp = {
    options,
    parse_opt,
    args_doc,
    doc,
    0,
    0,
    0
};
struct arguments arguments;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int global_result = 0;

PRIVATE time_measure_t time_measure;
PRIVATE uint64_t rows_appending = 0;

PRIVATE uint64_t first_t_by_disk = 0;
PRIVATE uint64_t last_t_by_disk = 0;
PRIVATE uint64_t last_tm_by_disk = 0;

PRIVATE int rt_disk_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record      // must be owned
)
{
    last_t_by_disk = md_record->__t__;
    last_tm_by_disk = md_record->__tm__;

    system_flag2_t system_flag = md_record->system_flag;
    if(system_flag & sf_loading_from_disk) {
        first_t_by_disk = md_record->__t__;
    } else {
        rows_appending++;
        if(rows_appending == 1) {
            MT_START_TIME(time_measure) /* HACK the time starts when receiving the first record, set in callback */
        }
        if(rows_appending >= MAX_KEYS * MAX_RECORDS) {
            if(arguments.client) {
                yev_loop_reset_running(yev_loop);
            }
        }
    }

    if(pinta_md) {
        char temp[1024];
        tranger2_print_md1_record(
            temp,
            sizeof(temp),
            key,
            md_record,
            FALSE
        );
        //printf("BY DISK: %s\n", temp);
    }
    if(pinta_records) {
        print_json2("DISK record", record);
    }

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
        json_t *match_cond;

        /*-----------------------*
         *      Open list
         *-----------------------*/
        char *test_name = "tranger2_open_iterator by MEM";
        if(arguments.client) {
            test_name = "tranger2_open_iterator by DISK";
        }
        set_expected_results( // Check that no logs happen
            test_name, // test name
            NULL,   // error's list, It must not be any log error
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        MT_START_TIME(time_measure)

        match_cond = json_pack("{s:b, s:i, s:I}",
            "backward", 0,
            "from_rowid", -10,
            "load_record_callback", (json_int_t)(size_t)rt_disk_record_callback
        );
        json_t *list = tranger2_open_list(
            tranger,
            TOPIC_NAME,
            match_cond,             // match_cond, owned
            NULL,                   // extra
            arguments.client?arguments.name:NULL, // rt_id
            arguments.client,       // rt_by_disk
            NULL                    // creator
        );

        MT_INCREMENT_COUNT(time_measure, MAX_RECORDS)
        MT_PRINT_TIME(time_measure, test_name)
        result += test_json(NULL);  // NULL: we want to check only the logs

        yev_loop_run_once(yev_loop);

        /*-------------------------------------*
         *      Add records
         *-------------------------------------*/
        test_name = "MASTER append records";
        char test_name_[80];
        if(arguments.client) {
            snprintf(test_name_, sizeof(test_name_),
                "CLIENT '%s' waiting append records",
                arguments.name
            );
            test_name = test_name_;
        }

        set_expected_results( // Check that no logs happen
            test_name, // test name
            NULL,   // error's list, It must not be any log error
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );

        MT_START_TIME(time_measure) /* HACK the time starts when receiving the first record, set in callback */

        uint64_t t1 = first_t_by_disk; // coge el ultimo t1 de disco
        if(last_t_by_disk != 946774799 || last_tm_by_disk != 946774799 ) {
            result += -1;
            printf("%sERROR%s --> bad first t, expected %lu, found %lu\n", On_Red BWhite, Color_Off,
                (unsigned long)946774799,
                (unsigned long)last_t_by_disk
            );
        }
        t1++;   // begin by the next

        if(!arguments.client) {
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
                    md2_record_ex_t md_record;
                    tranger2_append_record(tranger, TOPIC_NAME, tm+j, 0, &md_record, jn_record1);
                    if(i % 10 == 0) {
                        yev_loop_run_once(yev_loop);
                    }
                }
            }
            yev_loop_run_once(yev_loop);
        } else {
            yev_loop_run(yev_loop, -1);
        }

        MT_INCREMENT_COUNT(time_measure, MAX_KEYS*MAX_RECORDS)
        MT_PRINT_TIME(time_measure, test_name)

        if(last_t_by_disk != 946864799 || last_tm_by_disk != 946864799 ) {
            result += -1;
            printf("%sERROR%s --> bad last t, expected %lu, found %lu\n", On_Red BWhite, Color_Off,
                (unsigned long)946864799,
                (unsigned long)last_t_by_disk
            );
        } else {
            printf("%sOK%s --> %s\n", On_Green BWhite, Color_Off, "times by disk");
        }

        result += test_json(NULL);  // NULL: we want to check only the logs

        /*-------------------------------------*
         *      Close list
         *-------------------------------------*/
        set_expected_results( // Check that no logs happen
            "close list", // test name
            NULL,   // error's list, It must not be any log error
            NULL,   // expected, NULL: we want to check only the logs
            NULL,   // ignore_keys
            TRUE    // verbose
        );
        tranger2_close_list(tranger, list);
        result += test_json(NULL);  // NULL: we want to check only the logs
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
PRIVATE json_t *open_tranger(void)
{
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
        "master", !arguments.client,
        "on_critical_error", 0
    );
    json_t *tranger = tranger2_startup(0, jn_tranger, yev_loop);
    global_result += test_json(NULL);  // NULL: we want to check only the logs

    return tranger;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int close_tranger(json_t *tranger)
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
    if(tranger2_get_iterator_by_id(tranger, TOPIC_NAME, "it_by_mem", "")) {
        result += tranger2_close_iterator(
            tranger,
            tranger2_get_iterator_by_id(tranger, TOPIC_NAME, "it_by_mem", "")
        );
        yev_loop_run_once(yev_loop);
    }
    if(tranger2_get_iterator_by_id(tranger, TOPIC_NAME, "it_by_disk", "")) {
        result += tranger2_close_iterator(
            tranger,
            tranger2_get_iterator_by_id(tranger, TOPIC_NAME, "it_by_disk", "")
        );
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);  // NULL: we want to check only the logs

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

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*---------------------------------*
     *      Arguments
     *---------------------------------*/
    memset(&arguments, 0, sizeof(arguments));
    /*
     *  Default values
     */
    arguments.client = 0;
    arguments.name = "it_by_disk";

    /*
     *  Parse arguments
     */
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

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

    /*-------------------------------------*
     *      Check memory loss
     *-------------------------------------*/
    unsigned long memory_check_list[] = {0}; // WARNING: the list ended with 0
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    /*-------------------------------------*
     *  Captura salida logger very early
     *-------------------------------------*/
    glog_init();
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    //gobj_set_deep_tracing(2);
    //gobj_set_global_trace(0, TRUE);

    // gobj_set_gobj_trace(0, "liburing", TRUE, 0);
    // gobj_set_gobj_trace(0, "fs", TRUE, 0);

    /*--------------------------------*
     *      Startup gobj
     *--------------------------------*/
    gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL,   // global_authenticate_parser
        0,      // max_block, largest memory block
        0,      // max_system_memory, maximum system memory
        FALSE,
        0,
        0
    );

    yuno_catch_signals();

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
    json_t *tranger = open_tranger();

    global_result += do_test(tranger);

    yev_loop_run_once(yev_loop);

    int result = close_tranger(tranger);

    yev_loop_run_once(yev_loop);

    /*--------------------------------*
     *  Stop the event loop
     *--------------------------------*/
    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory()!=0) {
        printf("%sERROR%s --> %s\n", On_Red BWhite, Color_Off, "system memory not free");
        print_track_mem();
        result += -1;
    }
    result += global_result;

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
