/****************************************************************************
 *          perf_yev_timer
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <gobj.h>
#include <timeranger2.h>
#include <stacktrace_with_bfd.h>
#include <yunetas_ev_loop.h>
#include <testing.h>

#define TEST_NAME   "create_topic"
#define TOPIC_NAME "topic_sample"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void yuno_catch_signals(void);

/***************************************************************
 *              Data
 ***************************************************************/
yev_loop_t *yev_loop;
yev_event_t *yev_event_once;
yev_event_t *yev_event_periodic;
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
    char file[PATH_MAX];

    /*
     *  Write the tests in ~/tests_yuneta/
     */
    const char *home = getenv("HOME");
    char path[PATH_MAX];
    build_path(path, sizeof(path), home, "tests_yuneta", NULL);
    rmrdir(path);
    mkrdir(path, 02770);

    /*
     *  Startup/create the timeranger db
     */
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b}",
        "path", path,
        "database", "tr_"TEST_NAME,
        "master", 1
    );
    json_t *tranger = tranger2_startup(0, jn_tranger);

    /*
     *  CREATE CONDITION
     */
    tranger2_create_topic(
        tranger,
        TOPIC_NAME,  // topic name
        "id",           // pkey
        "",             // tkey
        0,              // system_latch
        json_pack("{s:s, s:s}", // jn_cols, owned
            "id", "",
            "address", ""
        ),
        0
    );
    tranger2_close_topic(tranger, TOPIC_NAME);

    /*
     *  TEST CONDITION
     */
    build_path(file, sizeof(file), path, "tr_"TEST_NAME,"__timeranger2__.json", NULL);
    {
        char expected[]= "\
        {                                                                    \n\
          'filename_mask': '%Y-%m-%d',                                       \n\
          'rpermission': 432,                                                \n\
          'xpermission': 1528                                                \n\
        }                                                                    \n\
        ";
        set_expected_results(
            "tr_"TEST_NAME"_",      // test name
            json_pack("[]"          // error's list
            ),
            string2json(helper_quote2doublequote(expected), TRUE),
            TRUE
        );
    }
    result += test_file(file);

//    {}
//    {
//        "id": "",
//        "address": ""
//    }{
//        "topic_name": "topic_sample",
//            "pkey": "id",
//            "tkey": "",
//            "system_flag": 4
//    }{
//        "filename_mask": "%Y-%m-%d",
//            "rpermission": 432,
//            "xpermission": 1528
//    }

    /*
     *  Shutdown timeranger
     */
    tranger2_shutdown(tranger);

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
        64*1024L,    // max_block, largest memory block
        256*1024L   // max_system_memory, maximum system memory
    );

    yuno_catch_signals();

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

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
