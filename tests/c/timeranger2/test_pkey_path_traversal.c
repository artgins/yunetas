/****************************************************************************
 *          test_pkey_path_traversal.c
 *
 *  Regression coverage for the primary-key / id path-traversal hardening at
 *  the timeranger2 trust boundary (a string pkey/id becomes a single
 *  keys/<key>/ directory component). Exercises every guarded sink:
 *
 *      - tranger2_append_record (create/write sink): REJECTS a sf_string_key
 *        pkey containing '/' or beginning with '.' ('.', '..', dot-relative /
 *        hidden), returning -1 and creating NO directory outside keys/.
 *        ACCEPTS a normal key and a single-component key with an embedded ".."
 *        (e.g. "a..b", which cannot escape) — the guard must not over-reject
 *        what the boundary legitimately allows (same predicate the disk mirror
 *        now uses).
 *      - tranger2_delete_key (whole-key delete sink): REJECTS the same
 *        metacharacter keys before any rmrdir; still deletes a valid existing
 *        key (positive control).
 *      - tranger2_delete_instance (per-row delete sink): REJECTS them too
 *        before any keys/<key>/ mkrdir (defense-in-depth for direct callers).
 *
 *  The negative assertions FAIL against the pre-hardening library (the
 *  traversal key is accepted / acted on, and "../MARKER" actually creates
 *  <topic>/MARKER on disk) and PASS after the hardening.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP         "test_pkey_path_traversal"
#define DATABASE    "tr_pkey_path_traversal"
#define TOPIC_NAME  "topic_pkey_path_traversal"
#define GOOD_KEY    "valid_key_001"
#define BASE_T      946684800   // 2000-01-01T00:00:00+0000

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/*
 *  A pkey that, if the guard were missing, would resolve from keys/ up into
 *  the topic directory (keys/../ESCAPE_MARKER -> <topic>/ESCAPE_MARKER).
 */
#define ESCAPE_KEY      "../ESCAPE_MARKER"
#define ESCAPE_MARKER   "ESCAPE_MARKER"

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE void build_paths(
    char *path_root, size_t root_sz,
    char *path_database, size_t db_sz,
    char *path_topic, size_t topic_sz
)
{
    const char *home = getenv("HOME");
    build_path(path_root, root_sz, home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, db_sz, path_root, DATABASE, NULL);
    build_path(path_topic, topic_sz, path_database, TOPIC_NAME, NULL);
}

PRIVATE json_t *startup_master(const char *path_root)
{
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i, s:I}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y",
        "xpermission" , 02770,
        "rpermission", 0600,
        "yev_loop", (json_int_t)0
    );
    return tranger2_startup(0, jn_tranger, 0);
}

PRIVATE int create_topic(json_t *tranger)
{
    json_t *topic = tranger2_create_topic(
        tranger,
        TOPIC_NAME,
        "id",       // string pkey
        "tm",
        json_pack("{s:i, s:s, s:i, s:i}",
            "on_critical_error", 4,
            "filename_mask", "%Y-%m-%d",
            "xpermission" , 02700,
            "rpermission", 0600
        ),
        sf_string_key,
        json_pack("{s:s, s:I, s:s}",
            "id", "",
            "tm", (json_int_t)0,
            "content", ""
        ),
        0
    );
    return topic? 0 : -1;
}

/*
 *  Append one record with the given string pkey. The record is OWNED by
 *  tranger2_append_record (it decref's it on every path), so we never decref
 *  it ourselves. Returns the tranger2_append_record return code.
 */
PRIVATE int append_key(json_t *tranger, const char *key)
{
    md2_record_ex_t md = {0};
    json_t *jn_record = json_pack("{s:s, s:I, s:s}",
        "id", key,
        "tm", (json_int_t)BASE_T,
        "content", "payload"
    );
    return tranger2_append_record(tranger, TOPIC_NAME, BASE_T, 0, &md, jn_record);
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);

    /*-------------------------------------*
     *  Setup
     *-------------------------------------*/
    set_expected_results(
        "setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = startup_master(path_root);
    if(!tranger || create_topic(tranger) < 0) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Positive controls: legit keys are
     *  accepted (guard must not over-reject).
     *  "a..b" is a single component with an
     *  embedded ".." -> cannot escape -> OK.
     *-------------------------------------*/
    set_expected_results("positive: legit keys accepted", NULL, NULL, NULL, 0);
    if(append_key(tranger, GOOD_KEY) < 0) {
        printf("%sERROR%s --> legit key '%s' was rejected\n",
            On_Red BWhite, Color_Off, GOOD_KEY);
        result += -1;
    }
    if(append_key(tranger, "a..b") < 0) {
        printf("%sERROR%s --> legit single-component key 'a..b' was rejected\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Negative: append rejects traversal
     *  pkeys. Each emits exactly one error
     *  log (trace is debug -> not captured).
     *-------------------------------------*/
    set_expected_results(
        "negative: append rejects path-traversal pkeys",
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Cannot append record, invalid pkey (path traversal)",
            "msg", "Cannot append record, invalid pkey (path traversal)",
            "msg", "Cannot append record, invalid pkey (path traversal)",
            "msg", "Cannot append record, invalid pkey (path traversal)",
            "msg", "Cannot append record, invalid pkey (path traversal)"
        ),
        NULL, NULL, 0
    );
    const char *bad_append[] = {ESCAPE_KEY, "a/b", ".hidden", ".", "..", NULL};
    for(int i = 0; bad_append[i]; i++) {
        if(append_key(tranger, bad_append[i]) >= 0) {
            printf("%sERROR%s --> append accepted traversal pkey '%s'\n",
                On_Red BWhite, Color_Off, bad_append[i]);
            result += -1;
        }
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Filesystem proof: the escaping key
     *  must NOT have created a directory one
     *  level up from keys/ (<topic>/MARKER).
     *-------------------------------------*/
    set_expected_results("negative: no escaped directory created", NULL, NULL, NULL, 0);
    char escaped_path[PATH_MAX];
    build_path(escaped_path, sizeof(escaped_path), path_topic, ESCAPE_MARKER, NULL);
    if(is_directory(escaped_path)) {
        printf("%sERROR%s --> traversal created an out-of-keys directory: %s\n",
            On_Red BWhite, Color_Off, escaped_path);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Positive control: deleting a real
     *  existing key (GOOD_KEY, appended above)
     *  succeeds, no error.
     *-------------------------------------*/
    set_expected_results("positive: delete_key removes a valid key", NULL, NULL, NULL, 0);
    if(tranger2_delete_key(tranger, TOPIC_NAME, GOOD_KEY) < 0) {
        printf("%sERROR%s --> valid key '%s' delete was rejected\n",
            On_Red BWhite, Color_Off, GOOD_KEY);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Negative: delete_key rejects the same
     *  metacharacter keys (incl. empty) before
     *  any rmrdir. One error log each.
     *-------------------------------------*/
    set_expected_results(
        "negative: delete_key rejects path-traversal keys",
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Invalid key (path metacharacters not allowed)",
            "msg", "Invalid key (path metacharacters not allowed)",
            "msg", "Invalid key (path metacharacters not allowed)",
            "msg", "Invalid key (path metacharacters not allowed)",
            "msg", "Invalid key (path metacharacters not allowed)",
            "msg", "Invalid key (path metacharacters not allowed)"
        ),
        NULL, NULL, 0
    );
    const char *bad_delete[] = {ESCAPE_KEY, "a/b", ".hidden", ".", "..", "", NULL};
    for(int i = 0; bad_delete[i]; i++) {
        if(tranger2_delete_key(tranger, TOPIC_NAME, bad_delete[i]) >= 0) {
            printf("%sERROR%s --> delete_key accepted traversal key '%s'\n",
                On_Red BWhite, Color_Off, bad_delete[i]);
            result += -1;
        }
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Negative: delete_instance (per-row
     *  delete sink) rejects the same keys
     *  before any keys/<key>/ mkrdir.
     *-------------------------------------*/
    set_expected_results(
        "negative: delete_instance rejects path-traversal keys",
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Invalid key (path metacharacters not allowed)",
            "msg", "Invalid key (path metacharacters not allowed)",
            "msg", "Invalid key (path metacharacters not allowed)",
            "msg", "Invalid key (path metacharacters not allowed)",
            "msg", "Invalid key (path metacharacters not allowed)",
            "msg", "Invalid key (path metacharacters not allowed)"
        ),
        NULL, NULL, 0
    );
    for(int i = 0; bad_delete[i]; i++) {
        if(tranger2_delete_instance(
                tranger, TOPIC_NAME, bad_delete[i], BASE_T, 0, FALSE) >= 0) {
            printf("%sERROR%s --> delete_instance accepted traversal key '%s'\n",
                On_Red BWhite, Color_Off, bad_delete[i]);
            result += -1;
        }
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Shutdown
     *-------------------------------------*/
    set_expected_results("shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *              Main
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

PRIVATE void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    memset(&sigIntHandler, 0, sizeof(sigIntHandler));
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGALRM, &sigIntHandler, NULL);
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);
}

int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;
    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);
    yuno_catch_signals();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("testing", 0, capture_log_write, 0);
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    yev_loop_create(0, 2024, 10, NULL, &yev_loop);

    int result = do_test();

    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result < 0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    } else {
        printf("<-- %sTEST OK%s: %s\n", On_Green BWhite, Color_Off, APP);
    }
    return result < 0? -1 : 0;
}
