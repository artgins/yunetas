/****************************************************************************
 *          test_append_md_contract.c
 *
 *  What tranger2_append_record() does to the record it is handed, as the
 *  header and the api page state it. The function OWNS the record and
 *  changes it in place, so a caller that keeps a reference of its own
 *  (json_incref before the call) sees each change:
 *
 *      - a `__md_tranger__` the record carries in (from an earlier append or
 *        a load) is metadata of THAT record: it is dropped before the content
 *        is written, and it is never stored;
 *      - no `__md_tranger__` is added for the caller: with no realtime list
 *        on the key, or only `only_md` feeds (which get NULL, not a record),
 *        the record leaves the append without one;
 *      - a realtime list that takes the record gets it WITH a fresh
 *        `__md_tranger__`, attached to that same object.
 *
 *  The metadata a caller reads is the `md_record_ex` out-param.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP         "test_append_md_contract"
#define DATABASE    "tr_append_md_contract"
#define TOPIC_NAME  "topic_append_md_contract"
#define KEY         "key1"
#define OTHER_KEY   "key2"
#define BASE_T      946684800   // 2000-01-01T00:00:00+0000

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE int got_records = 0;        // a list got a record
PRIVATE int got_md_in_record = 0;   // ... and it carried __md_tranger__
PRIVATE int got_null = 0;           // an only_md feed got NULL

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE int list_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record_ex,
    json_t *record  // owned
)
{
    if(record) {
        got_records++;
        if(json_object_get(record, "__md_tranger__")) {
            got_md_in_record++;
        }
    } else {
        got_null++;
    }
    JSON_DECREF(record)
    return 0;
}

PRIVATE json_t *startup_master(const char *path_root)
{
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y-%m-%d",
        "xpermission" , 02770,
        "rpermission", 0600
    );
    return tranger2_startup(0, jn_tranger, 0);
}

/*
 *  Append a record the caller keeps a reference of, and say whether it
 *  leaves the append carrying a __md_tranger__.
 */
PRIVATE BOOL append_kept(json_t *tranger, const char *key, json_int_t t, json_t *carried_md)
{
    json_t *record = json_pack("{s:s, s:I, s:s}",
        "id", key,
        "tm", t,
        "content", "payload"
    );
    if(carried_md) {
        json_object_set_new(record, "__md_tranger__", carried_md);
    }
    json_incref(record);    // the caller keeps one
    md2_record_ex_t md = {0};
    tranger2_append_record(tranger, TOPIC_NAME, (uint64_t)t, 0, &md, record);
    BOOL has_md = json_object_get(record, "__md_tranger__")? TRUE: FALSE;
    JSON_DECREF(record)
    return has_md;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX];

    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    set_expected_results(
        "setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = startup_master(path_root);
    if(!tranger || !tranger2_create_topic(
        tranger,
        TOPIC_NAME,
        "id",
        "tm",
        0,
        sf_string_key,
        json_pack("{s:s, s:I, s:s}", "id", "", "tm", (json_int_t)0, "content", ""),
        0
    )) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  A carried __md_tranger__ is dropped,
     *  and never stored
     *-------------------------------------*/
    set_expected_results("a carried __md_tranger__ is dropped and not stored", NULL, NULL, NULL, 1);
    if(append_kept(tranger, KEY, BASE_T, json_pack("{s:I, s:s}",
            "g_rowid", (json_int_t)999999, "stale", "stale-metadata-marker"))) {
        printf("%sERROR%s --> the record left the append with a __md_tranger__ (no list open)\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    char path_data[PATH_MAX];
    build_path(path_data, sizeof(path_data),
        path_database, TOPIC_NAME, "keys", KEY, "2000-01-01.json", NULL);
    int fd = open(path_data, O_RDONLY|O_CLOEXEC);
    char content[4096] = {0};
    if(fd < 0 || read(fd, content, sizeof(content) - 1) <= 0) {
        printf("%sERROR%s --> cannot read %s\n", On_Red BWhite, Color_Off, path_data);
        result += -1;
    }
    if(fd >= 0) {
        close(fd);
    }
    if(strstr(content, "__md_tranger__") || strstr(content, "stale-metadata-marker")) {
        printf("%sERROR%s --> a carried __md_tranger__ was stored: %s\n",
            On_Red BWhite, Color_Off, content);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Only an only_md feed: no dict, and
     *  the feed gets NULL
     *-------------------------------------*/
    set_expected_results("an only_md feed builds no __md_tranger__", NULL, NULL, NULL, 1);
    json_t *rt_md = tranger2_open_rt_mem(
        tranger, TOPIC_NAME, KEY, json_pack("{s:b}", "only_md", 1),
        list_callback, "rt_md", "", NULL
    );
    got_records = got_md_in_record = got_null = 0;
    if(append_kept(tranger, KEY, BASE_T + 1, NULL)) {
        printf("%sERROR%s --> an only_md feed made the append attach a __md_tranger__\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(got_null != 1 || got_records != 0) {
        printf("%sERROR%s --> the only_md feed got %d records and %d NULLs, expected 0 and 1\n",
            On_Red BWhite, Color_Off, got_records, got_null);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  A list on another key: nothing
     *-------------------------------------*/
    set_expected_results("a list on another key builds no __md_tranger__", NULL, NULL, NULL, 1);
    json_t *rt_other = tranger2_open_rt_mem(
        tranger, TOPIC_NAME, OTHER_KEY, NULL, list_callback, "rt_other", "", NULL
    );
    got_records = got_md_in_record = got_null = 0;
    if(append_kept(tranger, KEY, BASE_T + 2, NULL)) {
        printf("%sERROR%s --> a list on another key made the append attach a __md_tranger__\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  A list that takes the record: it
     *  gets the dict, on the same object
     *-------------------------------------*/
    set_expected_results("a list that takes the record gets __md_tranger__", NULL, NULL, NULL, 1);
    json_t *rt_full = tranger2_open_rt_mem(
        tranger, TOPIC_NAME, KEY, NULL, list_callback, "rt_full", "", NULL
    );
    got_records = got_md_in_record = got_null = 0;
    if(!append_kept(tranger, KEY, BASE_T + 3, NULL)) {
        printf("%sERROR%s --> the record handed to a list has no __md_tranger__ on the caller's object\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(got_records != 1 || got_md_in_record != 1) {
        printf("%sERROR%s --> the list got %d records, %d with __md_tranger__, expected 1 and 1\n",
            On_Red BWhite, Color_Off, got_records, got_md_in_record);
        result += -1;
    }
    tranger2_close_rt_mem(tranger, rt_md);
    tranger2_close_rt_mem(tranger, rt_other);
    tranger2_close_rt_mem(tranger, rt_full);
    result += test_json(NULL);

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
