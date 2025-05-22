/****************************************************************************
 *          test_kw1.c
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <locale.h>
#include <signal.h>
#include <time.h>
#include <yunetas.h>

#define APP "test_kw1"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void yuno_catch_signals(void);

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE int global_result = 0;

/***************************************************************************
 *  Test matching true
 ***************************************************************************/
static int test1_true(void)
{
    int result = 0;
    json_t *jn_filter = 0;

    json_t *kw = json_pack("{s:s, s:b, s:i, s:f}",
        "string", "s",
        "bool",  true,
        "integer", 123,
        "real", 1.5
    );
    BOOL matched = kw_match_simple(kw, NULL);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:s}",
        "string", "s"
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:b}",
        "bool", true
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:i}",
        "integer", 123
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:f}",
        "real", 1.5
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:s, s:b, s:i, s:f}",
        "string", "s",
        "bool",  true,
        "integer", 123,
        "real", 1.5
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("[{s:s, s:b}, {s:i, s:f}]",
        "string", "s",
        "bool",  true,
        "integer", 123,
        "real", 1.5
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("[{}, {s:i, s:f}]",
        "integer", 0,
        "real", 0.0
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    KW_DECREF(kw)
    return result;
}

/***************************************************************************
 *  Test matching false
 ***************************************************************************/
static int test1_false(void)
{
    int result = 0;
    json_t *jn_filter = 0;
    BOOL matched = false;

    json_t *kw = json_pack("{s:s, s:b, s:i, s:f}",
        "string", "s",
        "bool",  true,
        "integer", 123,
        "real", 1.5
    );

    jn_filter = json_pack("{s:s}",
        "string", "sa"
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:b}",
        "bool", false
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:i}",
        "integer", 1234
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:f}",
        "real", 1.55
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:s, s:b, s:i, s:f}",
        "string", "ss",
        "bool",  true,
        "integer", 123,
        "real", 1.5
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("[{s:s, s:b}, {s:i, s:f}]",
        "string", "sa",
        "bool",  true,
        "integer", 123,
        "real", 1.55
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("[{s:i, s:f}]",
        "integer", 0,
        "real", 0.0
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    KW_DECREF(kw)
    return result;
}

/***************************************************************************
 *  Test complex
 ***************************************************************************/
static int test2_true(void)
{
    int result = 0;
    json_t *jn_filter = 0;

    json_t *kw = json_pack("{s:{s:s, s:b, s:i, s:f}, s:[{s:s, s:b}, {s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  true,
            "integer", 123,
            "real", 1.5,
        "list",
            "string2", "string2",
            "false",  false,
            "integer", 456,
            "real", 2.6
    );

    BOOL matched = kw_match_simple(kw, NULL);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{s:s, s:b}]}",
        "object",
            "string1", "string1",
            "true",  true,
        "list",
            "string2", "string2",
            "false",  false
);
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{}, {s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  true,
        "list",
            "integer", 1235,
            "real", 1.55
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{s:s, s:b}, {s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  true,
        "list",
            "string2", "string2",
            "false",  false,
            "integer", 4563,
            "real", 2.6
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    KW_DECREF(kw)
    return result;
}

/***************************************************************************
 *  Test complex
 ***************************************************************************/
static int test2_false(void)
{
    int result = 0;
    json_t *jn_filter = 0;

    json_t *kw = json_pack("{s:{s:s, s:b, s:i, s:f}, s:[{s:s, s:b, s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  true,
            "integer", 123,
            "real", 1.5,
        "list",
            "string2", "string2",
            "false",  false,
            "integer", 456,
            "real", 2.6
    );

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  true,
        "list",
            "integer", 456,
            "real", 2.6
    );
    BOOL matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{s:i, s:f}]}",
        "object",
            "string1", "string2",
            "true",  true,
        "list",
            "integer", 456,
            "real", 2.6
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  false,
        "list",
            "integer", 456,
            "real", 2.6
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  true,
        "list",
            "integer", 4566,
            "real", 2.6
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  true,
        "list",
            "integer", 456,
            "real", 2.66
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }


    KW_DECREF(kw)
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

    result += test1_true();
    result += test1_false();
    result += test2_true();
    result += test2_false();

    /*-------------------------------*
     *      Shutdown timeranger
     *-------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger2_shutdown", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        true    // verbose
    );
    result += test_json(NULL);  // NULL: we want to check only the logs

    return result;
}

/***************************************************************************
 *                      Main
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
