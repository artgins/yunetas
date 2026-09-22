/****************************************************************************
 *          test_late_record.c
 *
 *  A LATE record: one whose __t__ is below the times already in its md2
 *  file. Two things went wrong with it (block 7 of the 2026-09-21 review):
 *
 *      - M16: a cell's time range was rebuilt from the FIRST and LAST md2
 *        rows only, and the late record is the last row with a lower `t`.
 *        The follower (at once) and the master (after a reload) put the
 *        cell's `to_t` below its real maximum, and a time-range query
 *        skipped the file: records inside the range were not served.
 *      - M18: a follower with TWO disk feeds on one key kept ONE watermark
 *        per (feed, key). A batch that touched two files of the key reseeded
 *        the second feed's mark on the second file before it had read the
 *        first, and that feed lost the records of BOTH files for ever.
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

#define APP "test_late_record"

#define DATABASE    "tr_late_record"
#define TOPIC_NAME  "topic_late_record"
#define KEY         "0000000000000000001"
#define DAY1        946684800   // 2000-01-01
#define DAY2        (DAY1 + 86400)

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char got_a[256] = "";
PRIVATE char got_b[256] = "";

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
    const char *id = kw_get_str(0, list, "id", "", 0);
    char *bf = (strcmp(id, "rtA") == 0)? got_a : got_b;
    char item[64];
    snprintf(item, sizeof(item), "%s%s",
        bf[0]? " ": "",
        record? kw_get_str(0, record, "content", "?", 0): "?"
    );
    strncat(bf, item, 256 - strlen(bf) - 1);
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

PRIVATE int append_one(json_t *tranger, uint64_t t, const char *content)
{
    json_t *record = json_pack("{s:I, s:I, s:s}",
        "id", (json_int_t)1,
        "tm", (json_int_t)t,
        "content", content
    );
    md2_record_ex_t md = {0};
    return tranger2_append_record(tranger, TOPIC_NAME, t, 0, &md, record);
}

PRIVATE void drain(int turns)
{
    for(int i = 0; i < turns; i++) {
        yev_loop_run_once(yev_loop);
    }
}

/*
 *  What an iterator of the key serves inside [from_t, to_t], as "<content> ..."
 */
PRIVATE void served_in(json_t *tranger, uint64_t from_t, uint64_t to_t, char *bf, size_t bfsize)
{
    bf[0] = 0;
    json_t *data = json_array();
    json_t *it = tranger2_open_iterator(
        tranger, TOPIC_NAME, KEY,
        json_pack("{s:I, s:I}", "from_t", (json_int_t)from_t, "to_t", (json_int_t)to_t),
        NULL, "range", "", data, NULL
    );
    int idx; json_t *record;
    json_array_foreach(data, idx, record) {
        char item[64];
        snprintf(item, sizeof(item), "%s%s",
            bf[0]? " ": "",
            kw_get_str(0, record, "content", "?", 0)
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
     *  Master, and a follower with TWO
     *  disk feeds on the same key
     *-------------------------------------*/
    set_expected_results(
        "late record: setup",
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
    json_t *rt_a = tranger2_open_rt_disk(
        tf, TOPIC_NAME, KEY, NULL, on_feed_record, "rtA", "", NULL
    );
    json_t *rt_b = tranger2_open_rt_disk(
        tf, TOPIC_NAME, KEY, NULL, on_feed_record, "rtB", "", NULL
    );
    if(!rt_a || !rt_b) {
        result += -1;
    }
    drain(10);
    result += test_json(NULL);

    /*-------------------------------------*
     *  M16: day 1 gets +100 and +50000,
     *  then a LATE +200
     *-------------------------------------*/
    set_expected_results("late record: the time range of its file", NULL, NULL, NULL, 1);
    append_one(tm, DAY1 + 100, "E1");
    drain(20);
    append_one(tm, DAY1 + 50000, "E2");
    drain(20);
    append_one(tm, DAY1 + 200, "E3");
    drain(30);

    served_in(tm, DAY1 + 40000, DAY1 + 60000, bf, sizeof(bf));
    result += expect("master serves [+40000, +60000]", bf, "E2");
    served_in(tf, DAY1 + 40000, DAY1 + 60000, bf, sizeof(bf));
    result += expect("follower serves [+40000, +60000]", bf, "E2");
    result += test_json(NULL);

    /*-------------------------------------*
     *  The marked file goes on growing: the
     *  follower reads only the rows after
     *  the ones its cell counted (N12), and
     *  the range it keeps is the union.
     *-------------------------------------*/
    set_expected_results("late record: a marked file keeps growing", NULL, NULL, NULL, 1);
    append_one(tm, DAY1 + 70000, "E4");
    drain(30);
    served_in(tf, DAY1 + 60000, DAY1 + 80000, bf, sizeof(bf));
    result += expect("follower serves [+60000, +80000]", bf, "E4");
    served_in(tf, DAY1 + 40000, DAY1 + 60000, bf, sizeof(bf));
    result += expect("follower still serves [+40000, +60000]", bf, "E2");
    /*  Not asserted: [+150, +250] -> E3. The late row is behind E2 in the
     *  file and the forward scan stops at the first row past to_t
     *  (tranger2_match_metadata), so a marked file serves a late row only
     *  in a range that also reaches the rows before it. Open (TODO.md).  */
    result += test_json(NULL);

    /*-------------------------------------*
     *  M18: ONE batch touches two files
     *  of the key: a late day-1 record and
     *  a day-2 one. Both feeds get both.
     *-------------------------------------*/
    set_expected_results("late record: two files in one batch, two feeds", NULL, NULL, NULL, 1);
    got_a[0] = 0;
    got_b[0] = 0;
    append_one(tm, DAY1 + 300, "F1");
    append_one(tm, DAY2 + 100, "F2");
    drain(60);
    result += expect("feed A got", got_a, "F1 F2");
    result += expect("feed B got", got_b, "F1 F2");
    result += test_json(NULL);

    /*-------------------------------------*
     *  A reload says the same (M16)
     *-------------------------------------*/
    set_expected_results("late record: a reload says the same", NULL, NULL, NULL, 1);
    tranger2_close_rt_disk(tf, rt_a);
    tranger2_close_rt_disk(tf, rt_b);
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
    served_in(tm, DAY1 + 40000, DAY1 + 60000, bf, sizeof(bf));
    result += expect("reloaded master serves [+40000, +60000]", bf, "E2");
    result += test_json(NULL);

    set_expected_results("late record: shutdown", NULL, NULL, NULL, 1);
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
    } else {
        printf("<-- %sTEST OK%s: %s\n", On_Green BWhite, Color_Off, APP);
    }
    return result < 0? -1 : 0;
}
