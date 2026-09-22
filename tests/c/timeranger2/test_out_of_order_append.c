/****************************************************************************
 *          test_out_of_order_append.c
 *
 *  An append whose __t__ belongs to an EARLIER file of the key.
 *
 *  A key's cache has one cell per md2 file, in the order the load gives
 *  them (by file name), and a global rowid is a position in that order. The
 *  append path looked only at the LAST cell, so a record for an earlier file
 *  got a second cell of that file at the end:
 *
 *      - the master served A, B, A -- the new record was unreadable and the
 *        FIRST record of the old file was served twice;
 *      - a follower counted the old file twice (4 rows for 3), re-published
 *        the whole file to its feed, and served A, B, A, C;
 *      - only a reload gave A, C, B.
 *
 *  Now the memory says what a reload says, at once: the record goes to its
 *  file's cell, a new file's cell goes to its place in the order, and the
 *  record's global rowid is its place (the records of the later files move
 *  one up, as a reload always moved them).
 *
 *  Triggers in real life: `append-record __t__=`, tr2migrate, tr2q_mqtt, or
 *  a clock stepping back across the filename_mask.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP "test_out_of_order_append"

#define DATABASE    "tr_out_of_order_append"
#define TOPIC_NAME  "topic_out_of_order"
#define KEY         "0000000000000000001"
#define DAY0        946598400   // 1999-12-31
#define DAY1        946684800   // 2000-01-01
#define DAY2        (DAY1 + 86400)

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char feed_got[256] = "";

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE int on_feed_record(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record
)
{
    char item[64];
    snprintf(item, sizeof(item), "%s%s@%d",
        feed_got[0]? " ": "",
        record? kw_get_str(0, record, "content", "?", 0): "?",
        (int)rowid
    );
    strncat(feed_got, item, sizeof(feed_got) - strlen(feed_got) - 1);
    JSON_DECREF(record)
    return 0;
}

PRIVATE json_t *startup_tranger(BOOL master)
{
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", master?1:0,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y",
        "xpermission" , 02770,
        "rpermission", 0600
    );
    return tranger2_startup(0, jn_tranger, yev_loop);
}

PRIVATE json_int_t append_one(json_t *tranger, uint64_t t, const char *content)
{
    json_t *record = json_pack("{s:I, s:I, s:s}",
        "id", (json_int_t)1,
        "tm", (json_int_t)t,
        "content", content
    );
    md2_record_ex_t md = {0};
    json_int_t g_rowid = -1;
    if(tranger2_append_record(tranger, TOPIC_NAME, t, 0, &md, record) == 0) {
        g_rowid = (json_int_t)md.g_rowid;
    }
    return g_rowid;
}

PRIVATE void drain(int turns)
{
    for(int i = 0; i < turns; i++) {
        yev_loop_run_once(yev_loop);
    }
}

/*
 *  The key's cells as "<file id>:<rows> ..."
 */
PRIVATE void cells_of(json_t *tranger, char *bf, size_t bfsize)
{
    bf[0] = 0;
    json_t *topic = tranger2_topic(tranger, TOPIC_NAME);
    json_t *files = kw_get_list(0, topic, "cache`" KEY "`files", 0, 0);
    int idx; json_t *cell;
    json_array_foreach(files, idx, cell) {
        char item[NAME_MAX];
        snprintf(item, sizeof(item), "%s%s:%d",
            bf[0]? " ": "",
            kw_get_str(0, cell, "id", "", 0),
            (int)kw_get_int(0, cell, "rows", 0, 0)
        );
        strncat(bf, item, bfsize - strlen(bf) - 1);
    }
}

/*
 *  What an iterator over the key serves, as "<content>@<g_rowid> ..."
 */
PRIVATE void served_by(json_t *tranger, char *bf, size_t bfsize)
{
    bf[0] = 0;
    json_t *data = json_array();
    json_t *it = tranger2_open_iterator(
        tranger, TOPIC_NAME, KEY, NULL, NULL, "check", "", data, NULL
    );
    int idx; json_t *record;
    json_array_foreach(data, idx, record) {
        char item[64];
        snprintf(item, sizeof(item), "%s%s@%d",
            bf[0]? " ": "",
            kw_get_str(0, record, "content", "?", 0),
            (int)kw_get_int(0, record, "__md_tranger__`g_rowid", 0, 0)
        );
        strncat(bf, item, bfsize - strlen(bf) - 1);
    }
    if(it) {
        tranger2_close_iterator(tranger, it);
    }
    JSON_DECREF(data)
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

PRIVATE int expect_int(const char *what, json_int_t got, json_int_t expected)
{
    if(got != expected) {
        printf("%sERROR%s --> %s: %d, expected %d\n",
            On_Red BWhite, Color_Off, what, (int)got, (int)expected);
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
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database),
        getenv("HOME"), "tests_yuneta", DATABASE, NULL);
    rmrdir(path_database);

    /*-------------------------------------*
     *  Master, a follower with a keyless feed
     *-------------------------------------*/
    set_expected_results(
        "out_of_order: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_tranger(TRUE);
    if(!tm || !tranger2_create_topic(
        tm, TOPIC_NAME, "id", "tm",
        json_pack("{s:i, s:s, s:i, s:i}",
            "on_critical_error", 4,
            "filename_mask", "%Y-%m-%d",
            "xpermission" , 02700,
            "rpermission", 0600
        ),
        sf_int_key,
        json_pack("{s:s, s:I, s:s}",
            "id", "",
            "tm", (json_int_t)0,
            "content", ""
        ),
        0
    )) {
        if(tm) {
            tranger2_shutdown(tm);
        }
        return -1;
    }
    json_t *tf = startup_tranger(FALSE);
    if(!tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> cannot open the follower\n", On_Red BWhite, Color_Off);
        if(tf) {
            tranger2_shutdown(tf);
        }
        tranger2_shutdown(tm);
        return -1;
    }
    json_t *rt = tranger2_open_rt_disk(
        tf, TOPIC_NAME, "", NULL, on_feed_record, "rtALL", "", NULL
    );
    if(!rt) {
        result += -1;
    }
    drain(10);
    result += test_json(NULL);

    /*-------------------------------------*
     *  A (day 1), B (day 2), then C (day 1)
     *-------------------------------------*/
    set_expected_results("out_of_order: a record for an earlier file", NULL, NULL, NULL, 1);

    result += expect_int("g_rowid of A", append_one(tm, DAY1, "A"), 1);
    drain(20);
    result += expect_int("g_rowid of B", append_one(tm, DAY2, "B"), 2);
    drain(20);

    feed_got[0] = 0;
    result += expect_int("g_rowid of C (day 1, after B)", append_one(tm, DAY1 + 10, "C"), 2);
    drain(30);
    result += expect("the follower's feed got", feed_got, "C@2");

    cells_of(tm, bf, sizeof(bf));
    result += expect("master cells", bf, "2000-01-01:2 2000-01-02:1");
    served_by(tm, bf, sizeof(bf));
    result += expect("master serves", bf, "A@1 C@2 B@3");

    cells_of(tf, bf, sizeof(bf));
    result += expect("follower cells", bf, "2000-01-01:2 2000-01-02:1");
    served_by(tf, bf, sizeof(bf));
    result += expect("follower serves", bf, "A@1 C@2 B@3");

    /*-------------------------------------*
     *  D: a NEW file, before all the others
     *-------------------------------------*/
    feed_got[0] = 0;
    result += expect_int("g_rowid of D (a new file before the others)",
        append_one(tm, DAY0, "D"), 1);
    drain(30);
    result += expect("the follower's feed got", feed_got, "D@1");

    cells_of(tm, bf, sizeof(bf));
    result += expect("master cells", bf, "1999-12-31:1 2000-01-01:2 2000-01-02:1");
    served_by(tm, bf, sizeof(bf));
    result += expect("master serves", bf, "D@1 A@2 C@3 B@4");

    cells_of(tf, bf, sizeof(bf));
    result += expect("follower cells", bf, "1999-12-31:1 2000-01-01:2 2000-01-02:1");
    served_by(tf, bf, sizeof(bf));
    result += expect("follower serves", bf, "D@1 A@2 C@3 B@4");
    result += test_json(NULL);

    /*-------------------------------------*
     *  A reload says the same
     *-------------------------------------*/
    set_expected_results("out_of_order: a reload says the same", NULL, NULL, NULL, 1);
    tranger2_close_rt_disk(tf, rt);
    drain(10);
    tranger2_shutdown(tf);
    tranger2_shutdown(tm);
    drain(10);

    tm = startup_tranger(TRUE);
    if(!tm || !tranger2_open_topic(tm, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> cannot reopen the master\n", On_Red BWhite, Color_Off);
        if(tm) {
            tranger2_shutdown(tm);
        }
        return -1;
    }
    cells_of(tm, bf, sizeof(bf));
    result += expect("cells after reload", bf, "1999-12-31:1 2000-01-01:2 2000-01-02:1");
    served_by(tm, bf, sizeof(bf));
    result += expect("served after reload", bf, "D@1 A@2 C@3 B@4");
    result += test_json(NULL);

    set_expected_results("out_of_order: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tm);
    drain(10);
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
    }
    return result < 0? -1 : 0;
}
