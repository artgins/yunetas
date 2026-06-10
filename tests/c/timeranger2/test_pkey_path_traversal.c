/****************************************************************************
 *          test_pkey_path_traversal.c
 *
 *  Regression coverage for primary-key / id path-traversal hardening
 *  (security branch: validate pkey/id before it becomes a keys/<key>/ path
 *  component). Two boundaries are exercised:
 *
 *      - do_test_append_rejects_traversal: tranger2_append_record() rejects a
 *        sf_string_key pkey containing '/' or beginning with '.', and still
 *        accepts a legitimate key (positive control). Covers the write/create
 *        sink (mkrdir/newfile under <topic_dir>/keys/<key>/).
 *
 *      - do_test_delete_rejects_traversal: tranger2_delete_key() rejects a key
 *        containing '/' or beginning with '.' BEFORE the rmrdir() sink, and
 *        still deletes a legitimate existing key (positive control).
 *
 *  Both negative cases FAIL before the hardening (the traversal key would be
 *  accepted / acted on) and PASS after it.
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

#define APP "test_pkey_path_traversal"

#define DATABASE    "tr_pkey_path_traversal"
#define TOPIC_NAME  "topic_pkey_path_traversal"
#define GOOD_KEY    "good_key_0001"
#define BASE_T      946684800   // 2000-01-01T00:00:00+0000

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/*
 *  Keys that must never be accepted as a single keys/<key>/ directory
 *  component: embedded separators, relative-path elements, dot-leading.
 *  (Empty key is excluded here: append reports it as "no pkey", a
 *  different message; the delete guard would map it to the same error,
 *  but we keep one shared list and one expected message per phase.)
 */
PRIVATE const char *bad_keys[] = {
    "../../../../tmp/evil",
    "..",
    ".",
    ".hidden",
    "a/b",
    "/etc/passwd"
};

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
        "id",
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

PRIVATE int append_str(json_t *tranger, const char *id)
{
    json_t *jn_record = json_pack("{s:s, s:I, s:s}",
        "id", id,
        "tm", (json_int_t)BASE_T,
        "content", "payload"
    );
    md2_record_ex_t md = {0};
    return tranger2_append_record(tranger, TOPIC_NAME, BASE_T, 0, &md, jn_record);
}

/***************************************************************************
 *  do_test_append_rejects_traversal
 *  tranger2_append_record() rejects traversal pkeys, accepts a valid one.
 ***************************************************************************/
PRIVATE int do_test_append_rejects_traversal(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);

    set_expected_results(
        "append_traversal: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, TRUE
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
     *  Positive control: a legitimate key
     *  appends cleanly, no error logged.
     *-------------------------------------*/
    set_expected_results(
        "append_traversal: valid key accepted",
        NULL, NULL, NULL, TRUE
    );
    if(append_str(tranger, GOOD_KEY) < 0) {
        printf("%sERROR%s --> valid key '%s' was rejected\n",
            On_Red BWhite, Color_Off, GOOD_KEY);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Negatives: every traversal pkey is
     *  rejected with the guard error.
     *-------------------------------------*/
    for(size_t i = 0; i < sizeof(bad_keys)/sizeof(bad_keys[0]); i++) {
        set_expected_results(
            "append_traversal: reject metachar pkey",
            json_pack("[{s:s}]",
                "msg", "Cannot append record, invalid pkey (path traversal)"
            ),
            NULL, NULL, TRUE
        );
        if(append_str(tranger, bad_keys[i]) >= 0) {
            printf("%sERROR%s --> traversal pkey '%s' was NOT rejected\n",
                On_Red BWhite, Color_Off, bad_keys[i]);
            result += -1;
        }
        result += test_json(NULL);
    }

    set_expected_results("append_traversal: shutdown", NULL, NULL, NULL, TRUE);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  do_test_delete_rejects_traversal
 *  tranger2_delete_key() rejects traversal keys before rmrdir, deletes a
 *  legitimate existing key.
 ***************************************************************************/
PRIVATE int do_test_delete_rejects_traversal(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);

    set_expected_results(
        "delete_traversal: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, TRUE
    );

    json_t *tranger = startup_master(path_root);
    if(!tranger || create_topic(tranger) < 0) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    if(append_str(tranger, GOOD_KEY) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Positive control: deleting a real
     *  existing key succeeds, no error.
     *-------------------------------------*/
    set_expected_results(
        "delete_traversal: valid key deleted",
        NULL, NULL, NULL, TRUE
    );
    if(tranger2_delete_key(tranger, TOPIC_NAME, GOOD_KEY) < 0) {
        printf("%sERROR%s --> valid key '%s' delete was rejected\n",
            On_Red BWhite, Color_Off, GOOD_KEY);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Negatives: every traversal key is
     *  rejected before any filesystem op.
     *-------------------------------------*/
    for(size_t i = 0; i < sizeof(bad_keys)/sizeof(bad_keys[0]); i++) {
        set_expected_results(
            "delete_traversal: reject metachar key",
            json_pack("[{s:s}]",
                "msg", "Invalid key (path metacharacters not allowed)"
            ),
            NULL, NULL, TRUE
        );
        if(tranger2_delete_key(tranger, TOPIC_NAME, bad_keys[i]) >= 0) {
            printf("%sERROR%s --> traversal key '%s' delete was NOT rejected\n",
                On_Red BWhite, Color_Off, bad_keys[i]);
            result += -1;
        }
        result += test_json(NULL);
    }

    set_expected_results("delete_traversal: shutdown", NULL, NULL, NULL, TRUE);
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

    int result = 0;
    result += do_test_append_rejects_traversal();
    result += do_test_delete_rejects_traversal();

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
    }
    return result < 0? -1 : 0;
}
