/****************************************************************************
 *          test_key_reborn_pages.c
 *
 *  An iterator that stays open while its key is deleted and written again.
 *
 *  An unfiltered iterator keeps the segments it pages over, and takes them
 *  again only when the key's cache moved: a stamp of three numbers, {rows,
 *  files, last_file}. A key deleted and written again with the SAME three
 *  numbers but its rows spread another way over its files kept the stamp,
 *  and the page read the new files with the old segments (7.25.4: "Cannot
 *  read record metadata", a CRITICAL, and a page with one record of
 *  three).
 *
 *  The delete of a key now forgets the segments of every iterator of that
 *  key, and a filtered iterator's index with them: the rows it indexed are
 *  gone.
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

#define APP         "test_key_reborn_pages"
#define DATABASE    "tr_key_reborn_pages"
#define TOPIC_NAME  "topic_reborn"
#define KEY         "K"
#define DAY1        946684800   // 2000-01-01
#define DAY2        (DAY1 + 86400)

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE void append_one(json_t *tranger, json_int_t t, const char *content)
{
    md2_record_ex_t md = {0};
    tranger2_append_record(tranger, TOPIC_NAME, (uint64_t)t, 0, &md,
        json_pack("{s:s, s:I, s:s}", "id", KEY, "tm", t, "content", content)
    );
}

/*
 *  A page of the iterator as "total: <content> ..."
 */
PRIVATE void page_of(json_t *tranger, json_t *it, char *bf, size_t bfsize)
{
    json_t *page = tranger2_iterator_get_page(tranger, it, 1, 100, FALSE);
    snprintf(bf, bfsize, "%d:", (int)kw_get_int(0, page, "total_rows", -1, 0));
    int idx; json_t *record;
    json_array_foreach(json_object_get(page, "data"), idx, record) {
        char item[64];
        snprintf(item, sizeof(item), " %s",
            record? kw_get_str(0, record, "content", "?", 0): "NULL"
        );
        strncat(bf, item, bfsize - strlen(bf) - 1);
    }
    JSON_DECREF(page)
}

PRIVATE int expect(const char *what, const char *got, const char *expected)
{
    if(strcmp(got, expected) != 0) {
        printf("%sERROR%s --> %s: '%s', expected '%s'\n",
            On_Red BWhite, Color_Off, what, got, expected);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    char bf[256];
    char path_root[PATH_MAX];
    char path_database[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);
    rmrdir(path_database);

    set_expected_results(
        "key reborn: setup",
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
    /*  day 1 two rows, day 2 one  */
    append_one(tranger, DAY1 + 1, "A1");
    append_one(tranger, DAY1 + 2, "A2");
    append_one(tranger, DAY2 + 1, "B1");
    result += test_json(NULL);

    set_expected_results("key reborn: the pages of an iterator kept open", NULL, NULL, NULL, 1);
    json_t *it = tranger2_open_iterator(
        tranger, TOPIC_NAME, KEY, json_object(), NULL, "pager", "", NULL, NULL
    );
    json_t *it_filtered = tranger2_open_iterator(
        tranger, TOPIC_NAME, KEY, json_pack("{s:I}", "to_t", (json_int_t)(DAY2 + 100)),
        NULL, "filtered", "", NULL, NULL
    );
    page_of(tranger, it, bf, sizeof(bf));
    result += expect("before the delete", bf, "3: A1 A2 B1");
    page_of(tranger, it_filtered, bf, sizeof(bf));
    result += expect("filtered, before the delete", bf, "3: A1 A2 B1");

    /*  The same three numbers: 3 rows, 2 files, the last one day 2  */
    tranger2_delete_key(tranger, TOPIC_NAME, KEY);
    page_of(tranger, it_filtered, bf, sizeof(bf));
    result += expect("filtered, right after the delete: its rows are gone", bf, "0:");

    append_one(tranger, DAY1 + 5, "C1");
    append_one(tranger, DAY2 + 5, "D1");
    append_one(tranger, DAY2 + 6, "D2");
    page_of(tranger, it, bf, sizeof(bf));
    result += expect("after the key is written again", bf, "3: C1 D1 D2");

    json_t *it2 = tranger2_open_iterator(
        tranger, TOPIC_NAME, KEY, json_object(), NULL, "fresh", "", NULL, NULL
    );
    page_of(tranger, it2, bf, sizeof(bf));
    result += expect("a fresh iterator", bf, "3: C1 D1 D2");

    tranger2_close_iterator(tranger, it2);
    tranger2_close_iterator(tranger, it_filtered);
    tranger2_close_iterator(tranger, it);
    result += test_json(NULL);

    set_expected_results("key reborn: shutdown", NULL, NULL, NULL, 1);
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
