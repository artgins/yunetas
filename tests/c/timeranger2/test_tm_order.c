/****************************************************************************
 *          test_tm_order.c
 *
 *  The __tm__ of a record is written by its producer, and nothing makes it
 *  grow with __t__, the time the md2 files are cut by. Four things went
 *  wrong with it (the 2026-09-23 independent review of 7.25.4):
 *
 *      - M1: a file whose tm range does not meet the condition is left out
 *        of the segments, so the segments of a key can have HOLES. The scan
 *        stepped from one segment to the next only when their rowids were
 *        consecutive: it logged "next rowids not consecutive" (a false
 *        internal error) and ended, and every row after the hole was lost.
 *        A `from_rowid` / `to_rowid` that falls in the hole was read in a
 *        segment it does not belong to.
 *      - M2: a reload (and a replica) took a file's tm range from its first
 *        and last rows only: with the tm out of order inside the file, a tm
 *        query skipped a file holding matching rows.
 *      - L4: no tm condition ended a scan, not even in a file whose rows are
 *        in tm order: a query for the first seconds of a big file read it
 *        whole.
 *      - A topic written before files were marked cannot tell which of its
 *        files are in tm order: it must not trust any file's tm range.
 *
 *  The master marks a file whose tm goes back (`<file>.tm_unordered`), like
 *  it marks a late __t__ (`<file>.unordered`); a topic that marks says so in
 *  its topic_desc.json (`marks_tm_unordered`).
 *
 *  A marker that cannot be written (independent review of the second fix
 *  round, M-A): the cell of the file was not flagged in memory either, so
 *  the master itself took the file for one in tm order and its early end
 *  hid the rows 7.25.4 served; and nothing wrote the marker later, so a
 *  reload hid them too. The cell is flagged whatever the disk says, the
 *  marker is written BEFORE the md2 row, and the next append to the file
 *  writes a marker that is still missing.
 *
 *  A legacy topic (every topic created by 7.25.4 or earlier) trusts no tm
 *  range, so a tm query reads every file of the key: ~30x slower than
 *  7.25.4 on 30 files (M-C). tranger2_mark_tm_order() is the migration an
 *  operator asks for: it reads every md2 file once, writes the markers,
 *  and makes the topic one that marks. Run again, it re-marks a topic whose
 *  markers were lost (a crash, a rollback binary that appends without
 *  them).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP "test_tm_order"

extern void jsonp_free(void *ptr);

#define DATABASE    "tr_tm_order"
#define TOPIC_NAME  "topic_tm_order"
#define DAY1        946684800   // 2000-01-01
#define DAY         86400

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char got_list[512] = "";
PRIVATE char path_database[PATH_MAX];

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int expect(const char *what, const char *got, const char *expected);

/***************************************************************
 *              Helpers
 ***************************************************************/
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
        "filename_mask", "%Y-%m-%d",
        "xpermission" , 02770,
        "rpermission", 0600
    );
    return tranger2_startup(0, jn_tranger, yev_loop);
}

PRIVATE int append_tm(json_t *tranger, const char *key, uint64_t t, uint64_t tm, const char *content)
{
    json_t *record = json_pack("{s:s, s:I, s:s}",
        "id", key,
        "tm", (json_int_t)tm,
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

PRIVATE void add_content(char *bf, size_t bfsize, json_t *record)
{
    char item[64];
    snprintf(item, sizeof(item), "%s%s",
        bf[0]? " ": "",
        record? kw_get_str(0, record, "content", "?", 0): "NULL"
    );
    strncat(bf, item, bfsize - strlen(bf) - 1);
}

PRIVATE int on_list_record(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record
)
{
    add_content(got_list, sizeof(got_list), record);
    JSON_DECREF(record)
    return 0;
}

/*
 *  What `key` serves with `cond` (owned), as "<content> ...", by one of the
 *  three roads a history is read:
 *      0   an iterator filling `data` (the loading walk)
 *      1   an iterator paged by the client (the index)
 *      2   a list with no realtime (the load callback)
 */
PRIVATE void served_cond(
    json_t *tranger,
    const char *key,
    json_t *cond,
    int road,
    char *bf,
    size_t bfsize
)
{
    bf[0] = 0;
    BOOL backward = json_is_true(json_object_get(cond, "backward"));

    if(road == 2) {
        got_list[0] = 0;
        json_object_set_new(cond, "key", json_string(key));
        json_object_set_new(cond, "load_record_callback",
            json_integer((json_int_t)(uintptr_t)on_list_record));
        if(!json_object_get(cond, "to_rowid")) {
            json_object_set_new(cond, "to_rowid", json_integer(1000000));   // no realtime
        }
        json_t *list = tranger2_open_list(
            tranger, TOPIC_NAME, cond, json_object(), "", FALSE, ""
        );
        if(list) {
            tranger2_close_list(tranger, list);
        }
        snprintf(bf, bfsize, "%s", got_list);
        return;
    }

    json_t *data = road == 1? NULL: json_array();
    json_t *it = tranger2_open_iterator(
        tranger, TOPIC_NAME, key, cond, NULL, "cond", "", data, NULL
    );
    json_t *page = NULL;
    if(road == 1 && it) {
        page = tranger2_iterator_get_page(tranger, it, 1, 100, backward);
        data = json_incref(json_object_get(page, "data"));
    }
    int idx; json_t *record;
    json_array_foreach(data, idx, record) {
        add_content(bf, bfsize, record);
    }
    if(it) {
        tranger2_close_iterator(tranger, it);
    }
    JSON_DECREF(page)
    JSON_DECREF(data)
}

/*
 *  The three roads, both directions, of one condition
 */
PRIVATE int expect_cond(
    json_t *tranger,
    const char *who,
    const char *key,
    json_t *cond,   // owned
    const char *forward,
    const char *backward
)
{
    int result = 0;
    char bf[512];
    char what[256];
    const char *roads[] = {"iterator", "paged", "list"};
    for(int road = 0; road < 3; road++) {
        for(int bwd = 0; bwd < 2; bwd++) {
            json_t *c = json_deep_copy(cond);
            json_object_set_new(c, "backward", json_boolean(bwd));
            served_cond(tranger, key, c, road, bf, sizeof(bf));
            char *s = json_dumps(cond, JSON_COMPACT);
            snprintf(what, sizeof(what), "%s key %s %s %s %s",
                who, key, roads[road], bwd? "backward": "forward", s? s: ""
            );
            jsonp_free(s);
            result += expect(what, bf, bwd? backward: forward);
        }
    }
    JSON_DECREF(cond)
    return result;
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

PRIVATE int expect_file(const char *what, const char *key, const char *name, BOOL exists)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, TOPIC_NAME, "keys", key, name, NULL);
    if(is_regular_file(path) != exists) {
        printf("%sERROR%s --> %s: %s %s\n",
            On_Red BWhite, Color_Off, what, path, exists? "missing": "should not exist");
        return -1;
    }
    return 0;
}

/*
 *  The conditions every store answers the same, whatever it knows of the
 *  order of its files
 */
PRIVATE int expect_the_answers(json_t *tranger, const char *who)
{
    int result = 0;

    /*  M1: the middle file is out of the tm range  */
    result += expect_cond(tranger, who, "gap",
        json_pack("{s:I}", "to_tm", (json_int_t)300),
        "D1 D3", "D3 D1"
    );
    /*  M1: a rowid bound that falls in the hole  */
    result += expect_cond(tranger, who, "gap",
        json_pack("{s:I, s:I}", "to_tm", (json_int_t)300, "from_rowid", (json_int_t)2),
        "D3", "D3"
    );
    result += expect_cond(tranger, who, "gap",
        json_pack("{s:I, s:I}", "to_tm", (json_int_t)300, "to_rowid", (json_int_t)2),
        "D1", "D1"
    );
    /*  M2: tm out of order inside one file  */
    result += expect_cond(tranger, who, "infile",
        json_pack("{s:I, s:I}", "from_tm", (json_int_t)50, "to_tm", (json_int_t)200),
        "T2", "T2"
    );
    /*  ... and a file out of tm order is never ended by tm  */
    result += expect_cond(tranger, who, "infile",
        json_pack("{s:I}", "to_tm", (json_int_t)400),
        "T2 T3", "T3 T2"
    );
    result += expect_cond(tranger, who, "infile",
        json_pack("{s:I}", "from_tm", (json_int_t)250),
        "T1 T3", "T3 T1"
    );
    return result;
}

/*
 *  Rewrite the topic_desc.json of the topic without `marks_tm_unordered`
 *  and remove the tm markers: what a topic written before them looks like.
 */
PRIVATE int make_it_legacy(void)
{
    char directory[PATH_MAX];
    build_path(directory, sizeof(directory), path_database, TOPIC_NAME, NULL);
    char path[PATH_MAX];
    build_path(path, sizeof(path), directory, "topic_desc.json", NULL);

    json_t *desc = json_load_file(path, 0, 0);
    if(!desc || !json_is_true(json_object_get(desc, "marks_tm_unordered"))) {
        printf("%sERROR%s --> topic_desc.json does not say the topic marks tm\n",
            On_Red BWhite, Color_Off);
        JSON_DECREF(desc)
        return -1;
    }
    json_object_del(desc, "marks_tm_unordered");
    chmod(path, 0600);
    if(json_dump_file(desc, path, JSON_INDENT(4)) < 0) {
        printf("%sERROR%s --> cannot rewrite %s\n", On_Red BWhite, Color_Off, path);
        JSON_DECREF(desc)
        return -1;
    }
    JSON_DECREF(desc)

    char key_dir[PATH_MAX];
    build_path(key_dir, sizeof(key_dir), directory, "keys", "infile", NULL);
    dir_array_t da;
    get_ordered_filename_array(0, key_dir, ".*\\.tm_unordered", WD_MATCH_REGULAR_FILE, &da);
    int removed = da.count;
    for(int i = 0; i < da.count; i++) {
        unlink(da.items[i]);
    }
    dir_array_free(&da);
    if(removed == 0) {
        printf("%sERROR%s --> no tm marker to remove\n", On_Red BWhite, Color_Off);
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
    char bf[512];
    build_path(path_database, sizeof(path_database),
        getenv("HOME"), "tests_yuneta", DATABASE, NULL);
    rmrdir(path_database);

    /*-------------------------------------*
     *  The master, and the data
     *-------------------------------------*/
    set_expected_results(
        "tm order: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_tranger(TRUE);
    if(!tm || !tranger2_create_topic(
        tm, TOPIC_NAME, "id", "tm", NULL, sf_string_key,
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

    /*  three day files, the middle one with a tm out of every range below  */
    append_tm(tm, "gap", DAY1 + 10, 100, "D1");
    append_tm(tm, "gap", DAY1 + DAY + 10, 5000, "D2");
    append_tm(tm, "gap", DAY1 + 2*DAY + 10, 150, "D3");

    /*  t in order (no .unordered), tm not  */
    append_tm(tm, "infile", DAY1 + 1000, 500, "T1");
    append_tm(tm, "infile", DAY1 + 1001, 100, "T2");
    append_tm(tm, "infile", DAY1 + 1002, 300, "T3");

    result += expect_file("the file whose tm goes back is marked",
        "infile", "2000-01-01.tm_unordered", TRUE);
    result += expect_file("a file whose tm goes back is not a late t",
        "infile", "2000-01-01.unordered", FALSE);
    result += expect_file("a file in tm order is not marked",
        "gap", "2000-01-02.tm_unordered", FALSE);
    result += test_json(NULL);

    /*-------------------------------------*
     *  In memory, reloaded, a replica
     *-------------------------------------*/
    set_expected_results("tm order: the master in memory", NULL, NULL, NULL, 1);
    result += expect_the_answers(tm, "master");
    result += test_json(NULL);

    /*-------------------------------------*
     *  A marker that cannot be written:
     *  the key directory made read-only
     *  when the tm goes back
     *-------------------------------------*/
    set_expected_results(
        "tm order: a marker that cannot be written",
        json_pack("[{s:s}]",
            "msg", "Cannot mark md2 file, a reload will misread its time range"
        ),
        NULL, NULL, 1
    );
    append_tm(tm, "nomark", DAY1 + 1, 500, "N1");
    char nomark_dir[PATH_MAX];
    build_path(nomark_dir, sizeof(nomark_dir), path_database, TOPIC_NAME, "keys", "nomark", NULL);
    chmod(nomark_dir, 0500);
    append_tm(tm, "nomark", DAY1 + 2, 100, "N2");
    chmod(nomark_dir, 02770);
    result += expect_file("the marker could not be written",
        "nomark", "2000-01-01.tm_unordered", FALSE);
    result += expect_cond(tm, "master (marker not written)", "nomark",
        json_pack("{s:I}", "to_tm", (json_int_t)200),
        "N2", "N2"
    );
    result += test_json(NULL);

    set_expected_results(
        "tm order: the next append writes the missing marker",
        json_pack("[{s:s}]",
            "msg", "md2 file marked, the marker missed earlier is written"
        ),
        NULL, NULL, 1
    );
    append_tm(tm, "nomark", DAY1 + 3, 600, "N3");
    result += expect_file("the next append to the file writes the marker",
        "nomark", "2000-01-01.tm_unordered", TRUE);
    result += test_json(NULL);

    set_expected_results("tm order: a replica", NULL, NULL, NULL, 1);
    json_t *tf = startup_tranger(FALSE);
    if(!tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> cannot open the replica\n", On_Red BWhite, Color_Off);
        result += -1;
    } else {
        result += expect_the_answers(tf, "replica");
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  A replica that follows the key
     *  while its tm goes back
     *-------------------------------------*/
    set_expected_results("tm order: a replica that follows", NULL, NULL, NULL, 1);
    json_t *rt = tf? tranger2_open_rt_disk(
        tf, TOPIC_NAME, "live", NULL, on_list_record, "rt_live", "", NULL
    ) : NULL;
    drain(10);
    append_tm(tm, "live", DAY1 + 10, 100, "L1");
    drain(20);
    append_tm(tm, "live", DAY1 + 11, 900, "L2");
    drain(20);
    append_tm(tm, "live", DAY1 + 12, 150, "L3");
    drain(20);
    append_tm(tm, "live", DAY1 + 13, 950, "L4");
    drain(30);
    if(tf) {
        result += expect_cond(tf, "follower", "live",
            json_pack("{s:I}", "to_tm", (json_int_t)200),
            "L1 L3", "L3 L1"
        );
    }
    result += expect_cond(tm, "master", "live",
        json_pack("{s:I}", "to_tm", (json_int_t)200),
        "L1 L3", "L3 L1"
    );
    if(rt) {
        tranger2_close_rt_disk(tf, rt);
    }
    drain(10);
    if(tf) {
        tranger2_shutdown(tf);
    }
    result += test_json(NULL);

    set_expected_results("tm order: the master reloaded", NULL, NULL, NULL, 1);
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
    result += expect_the_answers(tm, "reloaded");
    result += expect_cond(tm, "reloaded (marker written late)", "nomark",
        json_pack("{s:I}", "to_tm", (json_int_t)200),
        "N2", "N2"
    );
    result += test_json(NULL);

    /*-------------------------------------*
     *  L4: a file in tm order ends a tm
     *  scan. To SEE it, the md2 is cut
     *  behind the master's back after the
     *  rows the query needs: a scan that
     *  goes on reads a row that is not
     *  there, and says so.
     *-------------------------------------*/
    set_expected_results("tm order: a tm scan ends in a file in tm order", NULL, NULL, NULL, 1);
    for(int i = 0; i < 40; i++) {
        char content[16];
        snprintf(content, sizeof(content), "E%d", i);
        append_tm(tm, "early", DAY1 + (uint64_t)i, DAY1 + (uint64_t)i, content);
    }
    char path_md2[PATH_MAX];
    build_path(path_md2, sizeof(path_md2),
        path_database, TOPIC_NAME, "keys", "early", "2000-01-01.md2", NULL);
    if(truncate(path_md2, 20 * 32) < 0) {
        printf("%sERROR%s --> cannot cut %s\n", On_Red BWhite, Color_Off, path_md2);
        result += -1;
    }
    for(int road = 0; road < 3; road++) {
        served_cond(tm, "early",
            json_pack("{s:I}", "to_tm", (json_int_t)(DAY1 + 3)),
            road, bf, sizeof(bf)
        );
        result += expect("early end forward", bf, "E0 E1 E2 E3");
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  A topic written before the marks:
     *  no file's tm range is trusted
     *-------------------------------------*/
    set_expected_results("tm order: a topic written before the marks", NULL, NULL, NULL, 1);
    tranger2_shutdown(tm);
    drain(10);
    result += make_it_legacy();
    tm = startup_tranger(TRUE);
    if(!tm || !tranger2_open_topic(tm, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> cannot reopen the legacy master\n", On_Red BWhite, Color_Off);
        if(tm) {
            tranger2_shutdown(tm);
        }
        return -1;
    }
    if(json_is_true(json_object_get(tranger2_topic(tm, TOPIC_NAME), "marks_tm_unordered"))) {
        printf("%sERROR%s --> a legacy topic says it marks tm\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_the_answers(tm, "legacy");
    result += test_json(NULL);

    /*-------------------------------------*
     *  The migration: the legacy topic is
     *  marked, and answers the same
     *-------------------------------------*/
    set_expected_results(
        "tm order: a legacy topic marked",
        json_pack("[{s:s}]",
            "msg", "Topic marked: its md2 files out of order have their markers"
        ),
        NULL, NULL, 1
    );
    json_t *report = tranger2_mark_tm_order(tm, TOPIC_NAME);
    char *s_report = json_dumps(report, JSON_COMPACT|JSON_SORT_KEYS);
    printf("  mark_tm_order: %s\n", s_report? s_report: "NULL");
    jsonp_free(s_report);
    result += expect("the migration answers",
        report && json_is_true(json_object_get(report, "marks_tm_unordered"))? "marked": "refused",
        "marked"
    );
    /*  make_it_legacy() removed the one tm marker, of "infile"  */
    result += expect("the migration writes the tm marker that was missing",
        json_integer_value(json_object_get(report, "tm_unordered_marked")) == 1? "1": "other", "1"
    );
    JSON_DECREF(report)
    result += expect_file("the legacy file whose tm goes back is marked",
        "infile", "2000-01-01.tm_unordered", TRUE);
    result += expect("the topic in memory marks",
        json_is_true(json_object_get(tranger2_topic(tm, TOPIC_NAME), "marks_tm_unordered"))?
            "marks": "legacy", "marks");
    char path_desc[PATH_MAX];
    build_path(path_desc, sizeof(path_desc), path_database, TOPIC_NAME, "topic_desc.json", NULL);
    json_t *desc = json_load_file(path_desc, 0, 0);
    result += expect("topic_desc.json says it marks",
        json_is_true(json_object_get(desc, "marks_tm_unordered"))? "marks": "legacy", "marks");
    JSON_DECREF(desc)
    result += expect_the_answers(tm, "migrated");
    result += expect_cond(tm, "migrated", "nomark",
        json_pack("{s:I}", "to_tm", (json_int_t)200),
        "N2", "N2"
    );
    result += expect_cond(tm, "migrated", "live",
        json_pack("{s:I}", "to_tm", (json_int_t)200),
        "L1 L3", "L3 L1"
    );
    result += test_json(NULL);

    set_expected_results("tm order: a marked topic reloaded", NULL, NULL, NULL, 1);
    tranger2_shutdown(tm);
    drain(10);
    tm = startup_tranger(TRUE);
    if(!tm || !tranger2_open_topic(tm, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> cannot reopen the marked master\n", On_Red BWhite, Color_Off);
        if(tm) {
            tranger2_shutdown(tm);
        }
        return -1;
    }
    result += expect_the_answers(tm, "migrated, reloaded");
    result += expect_cond(tm, "migrated, reloaded", "live",
        json_pack("{s:I}", "to_tm", (json_int_t)200),
        "L1 L3", "L3 L1"
    );
    result += test_json(NULL);

    /*-------------------------------------*
     *  Markers lost (a rollback binary, a
     *  crash): marked again
     *-------------------------------------*/
    set_expected_results(
        "tm order: markers lost, marked again",
        json_pack("[{s:s}]",
            "msg", "Topic marked: its md2 files out of order have their markers"
        ),
        NULL, NULL, 1
    );
    tranger2_shutdown(tm);
    drain(10);
    char key_dir[PATH_MAX];
    build_path(key_dir, sizeof(key_dir), path_database, TOPIC_NAME, "keys", "live", NULL);
    dir_array_t da;
    get_ordered_filename_array(0, key_dir, ".*\\.tm_unordered", WD_MATCH_REGULAR_FILE, &da);
    int lost = da.count;
    for(int i = 0; i < da.count; i++) {
        unlink(da.items[i]);
    }
    dir_array_free(&da);
    result += expect("a marker of \"live\" to lose", lost == 1? "1": "other", "1");
    tm = startup_tranger(TRUE);
    tranger2_open_topic(tm, TOPIC_NAME, TRUE);
    report = tranger2_mark_tm_order(tm, TOPIC_NAME);
    result += expect("the lost marker is written again",
        json_integer_value(json_object_get(report, "tm_unordered_marked")) == 1? "1": "other", "1"
    );
    JSON_DECREF(report)
    result += expect_cond(tm, "re-marked", "live",
        json_pack("{s:I}", "to_tm", (json_int_t)200),
        "L1 L3", "L3 L1"
    );
    result += test_json(NULL);

    set_expected_results(
        "tm order: marking twice writes nothing",
        json_pack("[{s:s}]",
            "msg", "Topic marked: its md2 files out of order have their markers"
        ),
        NULL, NULL, 1
    );
    report = tranger2_mark_tm_order(tm, TOPIC_NAME);
    result += expect("a second migration writes no marker",
        json_integer_value(json_object_get(report, "tm_unordered_marked")) == 0 &&
        json_integer_value(json_object_get(report, "t_unordered_marked")) == 0? "0": "other", "0"
    );
    JSON_DECREF(report)
    result += test_json(NULL);

    set_expected_results(
        "tm order: a replica cannot mark",
        json_pack("[{s:s}]",
            "msg", "Only master can write"
        ),
        NULL, NULL, 1
    );
    json_t *replica = startup_tranger(FALSE);
    if(replica) {
        tranger2_open_topic(replica, TOPIC_NAME, TRUE);
        report = tranger2_mark_tm_order(replica, TOPIC_NAME);
        result += expect("a replica cannot mark", report? "marked": "refused", "refused");
        JSON_DECREF(report)
        tranger2_shutdown(replica);
    }
    result += test_json(NULL);

    set_expected_results("tm order: shutdown", NULL, NULL, NULL, 1);
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
