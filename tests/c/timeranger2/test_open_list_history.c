/****************************************************************************
 *          test_open_list_history.c
 *
 *  A list IS the history of its keys: tranger2_open_list() loads it with
 *  one-shot iterators before it hands the list over.
 *
 *      - A keyless list (every key of the topic) closed each key's
 *        iterator with its `load_failed` unread: a key whose history could
 *        not be loaded gave the caller a list, and the caller took what it
 *        got for the whole topic. treedb's snapshot guard of the assets
 *        read "held by nothing" from such a list (M2 of the 2026-09-23
 *        independent review of 7.25.4). It is refused now, like the list
 *        of one key already was.
 *      - The one-shot iterators took the caller's default identity (the
 *        key as the id, creator ""): an iterator the caller kept open on
 *        the key made the load's one "already exist", and the list of that
 *        key was refused. They have a creator of their own now.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP         "test_open_list_history"
#define DATABASE    "tr_open_list_history"
#define TOPIC_NAME  "topic_history"
#define DAY1        946684800   // 2000-01-01

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE int loaded = 0;

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE int on_record(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record
)
{
    loaded++;
    JSON_DECREF(record)
    return 0;
}

PRIVATE json_t *open_history(json_t *tranger, const char *key)
{
    json_t *match_cond = json_pack("{s:I, s:I}",
        "to_rowid", (json_int_t)1000000,   // no realtime
        "load_record_callback", (json_int_t)(uintptr_t)on_record
    );
    if(key) {
        json_object_set_new(match_cond, "key", json_string(key));
    }
    return tranger2_open_list(tranger, TOPIC_NAME, match_cond, json_object(), "", FALSE, "");
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    set_expected_results(
        "open_list history: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i, s:s}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y-%m-%d"
    ), 0);
    if(!tranger || !tranger2_create_topic(
        tranger, TOPIC_NAME, "id", "tm", NULL, sf_string_key,
        json_pack("{s:s, s:I, s:s}", "id", "", "tm", (json_int_t)0, "content", ""),
        0
    )) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    const char *keys[] = {"A", "B", NULL};
    for(int k = 0; keys[k]; k++) {
        for(int i = 0; i < 3; i++) {
            md2_record_ex_t md = {0};
            tranger2_append_record(tranger, TOPIC_NAME, DAY1 + (uint64_t)i, 0, &md,
                json_pack("{s:s, s:I, s:s}", "id", keys[k], "tm", (json_int_t)(DAY1 + i), "content", "x")
            );
        }
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  A list of a key the caller already
     *  iterates with the default identity
     *-------------------------------------*/
    set_expected_results("open_list history: a key the caller iterates", NULL, NULL, NULL, 1);
    json_t *mine = tranger2_open_iterator(
        tranger, TOPIC_NAME, "A", NULL, NULL, "", "", NULL, NULL     // id: the key, creator ""
    );
    loaded = 0;
    json_t *list = open_history(tranger, "A");
    if(!list || loaded != 3) {
        printf("%sERROR%s --> the list of a key the caller iterates: %s, %d records, expected 3\n",
            On_Red BWhite, Color_Off, list? "opened": "REFUSED", loaded);
        result += -1;
    }
    if(list) {
        tranger2_close_list(tranger, list);
    }
    tranger2_close_iterator(tranger, mine);
    result += test_json(NULL);

    /*-------------------------------------*
     *  A keyless list with a key whose
     *  history cannot be read: the md2 of
     *  B is cut behind the tranger's back
     *-------------------------------------*/
    set_expected_results(
        "open_list history: a keyless list with a key that cannot be loaded",
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot read record metadata, read FAILED",
            "msg", "Cannot load the history of a key of the list"
        ),
        NULL, NULL, 1
    );
    char path_md2[PATH_MAX];
    build_path(path_md2, sizeof(path_md2), path_database, TOPIC_NAME, "keys", "B", "2000-01-01.md2", NULL);
    if(truncate(path_md2, 2 * 32) < 0) {
        printf("%sERROR%s --> cannot cut %s\n", On_Red BWhite, Color_Off, path_md2);
        result += -1;
    }
    loaded = 0;
    list = open_history(tranger, NULL);
    if(list) {
        printf("%sERROR%s --> a keyless list was handed over with %d records of 6\n",
            On_Red BWhite, Color_Off, loaded);
        result += -1;
        tranger2_close_list(tranger, list);
    }
    result += test_json(NULL);

    set_expected_results("open_list history: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
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
