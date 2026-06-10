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
        "bool",  TRUE,
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
        "bool", TRUE
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
        "bool",  TRUE,
        "integer", 123,
        "real", 1.5
    );
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("[{s:s, s:b}, {s:i, s:f}]",
        "string", "s",
        "bool",  TRUE,
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
    BOOL matched = FALSE;

    json_t *kw = json_pack("{s:s, s:b, s:i, s:f}",
        "string", "s",
        "bool",  TRUE,
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
        "bool", FALSE
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
        "bool",  TRUE,
        "integer", 123,
        "real", 1.5
    );
    matched = kw_match_simple(kw, jn_filter);
    if(matched) {
        result += -1;
    }

    jn_filter = json_pack("[{s:s, s:b}, {s:i, s:f}]",
        "string", "sa",
        "bool",  TRUE,
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
            "true",  TRUE,
            "integer", 123,
            "real", 1.5,
        "list",
            "string2", "string2",
            "false",  FALSE,
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
            "true",  TRUE,
        "list",
            "string2", "string2",
            "false",  FALSE
);
    matched = kw_match_simple(kw, jn_filter);
    if(!matched) {
        result += -1;
    }

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{}, {s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  TRUE,
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
            "true",  TRUE,
        "list",
            "string2", "string2",
            "false",  FALSE,
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
            "true",  TRUE,
            "integer", 123,
            "real", 1.5,
        "list",
            "string2", "string2",
            "false",  FALSE,
            "integer", 456,
            "real", 2.6
    );

    jn_filter = json_pack("{s:{s:s, s:b}, s:[{s:i, s:f}]}",
        "object",
            "string1", "string1",
            "true",  TRUE,
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
            "true",  TRUE,
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
            "true",  FALSE,
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
            "true",  TRUE,
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
            "true",  TRUE,
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
 *  Regression f001: gbuffer_deserialize() must reject a serialized blob whose
 *  "data" field is missing/non-string instead of crashing on strlen(NULL).
 ***************************************************************************/
static int test_reg_f001_gbuffer_deserialize_null_data(void)
{
    int result = 0;
    set_expected_results(
        "reg_f001_gbuffer_deserialize_null_data",
        json_pack("[{s:s}]",    // exactly this error must be logged, nothing else
            "msg", "gbuffer_deserialize: 'data' field missing or not a string"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    json_t *jn = json_pack("{s:s, s:i}",    // a serialized blob with NO "data" field
        "label", "x",
        "mark", 0
    );
    gbuffer_t *gbuf = gbuffer_deserialize(0, jn);
    if(gbuf != NULL) {
        result += -1;   // must reject, not return a buffer
        gbuffer_decref(gbuf);
    }
    json_decref(jn);

    result += test_json(NULL);  // NULL: we want to check only the logs
    return result;
}

/***************************************************************************
 *  Regression f001 (variant): gbuffer_deserialize() must also reject a
 *  serialized blob whose "data" field is PRESENT but is not valid base64.
 *  gbuffer_base64_to_binary() returns NULL on a decode failure; the old code
 *  fed that NULL straight into gbuffer_setmark() (gbuf->mark = mark), a NULL
 *  deref reachable pre-auth via ac_on_message -> kw_deserialize.
 ***************************************************************************/
static int test_reg_f001_gbuffer_deserialize_bad_base64(void)
{
    int result = 0;
    set_expected_results(
        "reg_f001_gbuffer_deserialize_bad_base64",
        json_pack("[{s:s},{s:s}]",  // b64_decode fails, then deserialize rejects
            "msg", "b64_decode() FAILED",
            "msg", "gbuffer_deserialize: base64 decode of 'data' field FAILED"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    /* "!!!!" is the right length to attempt a decode but is not valid base64 */
    json_t *jn = json_pack("{s:s, s:i, s:s}",
        "label", "x",
        "mark", 0,
        "data", "!!!!"
    );
    gbuffer_t *gbuf = gbuffer_deserialize(0, jn);
    if(gbuf != NULL) {
        result += -1;   // must reject, not return a buffer (and must not crash)
        gbuffer_decref(gbuf);
    }
    json_decref(jn);

    result += test_json(NULL);  // NULL: we want to check only the logs
    return result;
}

/***************************************************************************
 *  Regression f003: kw_find_path() must bound recursion depth so a deeply
 *  nested kw plus a long backtick-delimited path cannot exhaust the stack.
 ***************************************************************************/
static int test_reg_f003_kw_find_path_depth(void)
{
    int result = 0;
    int N = 100;    // > KW_MAX_PATH_DEPTH (64)

    /* kw nested N deep: {"a":{"a":{...}}} */
    json_t *root = json_object();
    json_t *cur = root;
    for(int i=0; i<N; i++) {
        json_t *child = json_object();
        json_object_set_new(cur, "a", child);
        cur = child;
    }

    /* path "a`a`...`a" with N segments */
    char path[256];
    int off = 0;
    for(int i=0; i<N; i++) {
        if(i) {
            path[off++] = '`';
        }
        path[off++] = 'a';
    }
    path[off] = 0;

    set_expected_results(
        "reg_f003_kw_find_path_depth",
        json_pack("[{s:s}]",
            "msg", "kw_find_path: max nesting depth exceeded"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    json_t *found = kw_find_path(0, root, path, FALSE);
    if(found != NULL) {
        result += -1;   // must abort to not-found, not keep recursing
    }
    json_decref(root);

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  Regression f004: the kwid comparators must bound recursion depth so
 *  deeply nested JSON cannot exhaust the stack or amplify via deep-copy.
 ***************************************************************************/
static int test_reg_f004_kwid_compare_records_depth(void)
{
    int result = 0;
    int N = 100;    // > KWID_MAX_COMPARE_DEPTH (64)

    /* two identical records nested N deep: {"a":{"a":{...}}} */
    json_t *rec = json_object();
    json_t *cur = rec;
    for(int i=0; i<N; i++) {
        json_t *child = json_object();
        json_object_set_new(cur, "a", child);
        cur = child;
    }
    json_t *expected = json_deep_copy(rec);

    set_expected_results(
        "reg_f004_kwid_compare_records_depth",
        json_pack("[{s:s}]",
            "msg", "compare: max nesting depth exceeded"
        ),
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
    );

    BOOL eq = kwid_compare_records(0, rec, expected, NULL, FALSE, FALSE, FALSE);
    if(eq != FALSE) {
        result += -1;   // must bail to FALSE at the depth cap, not recurse off the stack
    }
    json_decref(rec);
    json_decref(expected);

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  Regression f002: kwid_find_record_in_list must return -1 (not 0) when an
 *  id is genuinely absent. The match-by-id branch of the list comparator
 *  tests `idx2 < 0`; a bogus 0 made it compare/remove the WRONG record
 *  (expected[0]) and report a false match.
 ***************************************************************************/
static int test_reg_f002_find_record_not_found(void)
{
    int result = 0;

    /*
     *  list has a record whose id ("X") is NOT present in expected.
     *  expected[0] has a different id ("A") but the same payload.
     *  Ignoring "id", a buggy not-found return of 0 makes the comparator
     *  match list[0] against the wrong expected[0] and report equal (TRUE).
     *  With the -1 fix the absent id is detected and the lists differ (FALSE).
     */
    json_t *list = json_pack("[{s:s, s:i}]",
        "id", "X",
        "v", 1
    );
    json_t *expected = json_pack("[{s:s, s:i}]",
        "id", "A",
        "v", 1
    );
    const char *ignore_keys[] = {"id", NULL};

    set_expected_results(
        "reg_f002_find_record_not_found",
        NULL,   // error's list: no error log expected on this path
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        0       // verbose off: assert on the BOOL, not on trace logs
    );

    BOOL eq = kwid_compare_lists(0, list, expected, ignore_keys, FALSE, FALSE, FALSE);
    if(eq != FALSE) {
        result += -1;   // absent id must surface as a mismatch, not a wrong-record match
    }
    JSON_DECREF(list);
    JSON_DECREF(expected);

    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *              Test
 *  Open as master, check main files, add records, open rt lists
 *  HACK: return -1 to fail, 0 to ok
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;

    result += test1_true();
    result += test1_false();
    result += test2_true();
    result += test2_false();

    result += test_reg_f001_gbuffer_deserialize_null_data();
    result += test_reg_f001_gbuffer_deserialize_bad_base64();
    result += test_reg_f002_find_record_not_found();
    result += test_reg_f003_kw_find_path_depth();
    result += test_reg_f004_kwid_compare_records_depth();

    /*-------------------------------*
     *      Shutdown timeranger
     *-------------------------------*/
    set_expected_results( // Check that no logs happen
        "tranger2_shutdown", // test name
        NULL,   // error's list, It must not be any log error
        NULL,   // expected, NULL: we want to check only the logs
        NULL,   // ignore_keys
        1       // verbose
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

    gbmem_get_allocators(
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
        NULL    // global_authentication_parser
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
