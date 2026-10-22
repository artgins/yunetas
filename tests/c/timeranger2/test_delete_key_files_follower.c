/****************************************************************************
 *          test_delete_key_files_follower.c
 *
 *  Retention, the followers: tranger2_delete_key_files() deletes the old
 *  files of a key on the master, and a follower in another process holds
 *  cells of those files. It must drop them and take the base of the key
 *  (keys/<key>/.purged), or it serves rows that are gone and numbers the
 *  next records differently from the master (DESIGN-retention.md §4.5).
 *
 *      - a follower with an rt_disk feed hears the signal the master
 *        leaves in disks/<rt_id>/ (`.p.<key>`, `.q.<sha256>` for a key too
 *        long for the name) and applies the base at once;
 *      - its feed goes on: a record appended after the purge reaches it
 *        with the master's g_rowid;
 *      - a follower that heard nothing (it has no feed: no watcher of
 *        disks/) finds the purge before its next read, and reads what the
 *        master reads, with no error;
 *      - a file the follower flagged unreadable and the master purged is
 *        not a flag for ever: every load of the key failed; the purge
 *        renumbers the key, and says so with a WARNING;
 *      - a signal left in a feed directory (a master that died between its
 *        mkdir() and its rmdir()) does not stop the next ones;
 *      - a feed callback that reads its key while a purge waits does not
 *        apply it under the cell the new records are counted in;
 *      - a filtered pager of the follower opened with a NEGATIVE rowid
 *        bound keeps what it indexed when the key grows and the master
 *        then purges it.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP "test_delete_key_files_follower"

#define DATABASE    "tr_delete_key_files_follower"
#define TOPIC_NAME  "topic_delete_key_files_follower"
#define TOPIC_LONG  "topic_delete_key_files_follower_long"
#define KEY         "0000000000000000001"   // a follower with a feed
#define KEY2        "0000000000000000002"   // read by a follower with no feed
#define KEY3        "0000000000000000003"   // a file flagged by a follower, then purged
#define KEY5        "0000000000000000005"   // a feed callback that reads its key
#define KEY6        "0000000000000000006"   // a base the follower fails to read once
#define KEY7        "0000000000000000007"   // a negative rowid bound across appends and a purge
#define DAY1        946684800   // 2000-01-01
#define DAY2        (DAY1 + 86400)
#define DAY3        (DAY2 + 86400)

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char got[512] = "";
PRIVATE char long_key[241];     // past what the name of a signal holds
PRIVATE char got5[512] = "";

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int expect(const char *what, const char *got, const char *expected);
PRIVATE int expect_int(const char *what, json_int_t got, json_int_t expected);

/***************************************************************************
 *  A file of day 1 back on disk (a purge whose files stayed: a copy of day
 *  3), and its link in a feed directory, as a late row's
 ***************************************************************************/
PRIVATE int link_back_purged_file(const char *path_database, const char *key, const char *rt_name)
{
    int ret = 0;
    char key_dir[PATH_MAX];
    char src[PATH_MAX];
    char dst[PATH_MAX];
    char lnk[PATH_MAX];
    build_path(key_dir, sizeof(key_dir), path_database, TOPIC_NAME, "keys", key, NULL);
    const char *exts[] = {".json", ".md2", NULL};
    for(int i = 0; exts[i]; i++) {
        char name[NAME_MAX];
        snprintf(name, sizeof(name), "2000-01-03%s", exts[i]);
        build_path(src, sizeof(src), key_dir, name, NULL);
        snprintf(name, sizeof(name), "2000-01-01%s", exts[i]);
        build_path(dst, sizeof(dst), key_dir, name, NULL);
        if(link(src, dst) < 0) {
            printf("%sERROR%s --> cannot link %s\n", On_Red BWhite, Color_Off, dst);
            ret += -1;
        }
    }
    build_path(lnk, sizeof(lnk), path_database, TOPIC_NAME, "disks", rt_name, key, "2000-01-01.md2", NULL);
    mkrdir(lnk, 02770);
    rmdir(lnk);
    if(link(dst, lnk) < 0) {
        printf("%sERROR%s --> cannot link %s\n", On_Red BWhite, Color_Off, lnk);
        ret += -1;
    }
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void unlink_purged_file(const char *path_database, const char *key, const char *rt_name)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, TOPIC_NAME, "keys", key, "2000-01-01.json", NULL);
    unlink(path);
    build_path(path, sizeof(path), path_database, TOPIC_NAME, "keys", key, "2000-01-01.md2", NULL);
    unlink(path);
    build_path(path, sizeof(path), path_database, TOPIC_NAME, "disks", rt_name, key, "2000-01-01.md2", NULL);
    unlink(path);
}

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
    snprintf(item, sizeof(item), "%s%s:%ld",
        got[0]? " ": "",
        record? kw_get_str(0, record, "content", "?", 0): "?",
        (long)rowid
    );
    strncat(got, item, sizeof(got) - strlen(got) - 1);
    JSON_DECREF(record)
    return 0;
}

/*
 *  A feed of KEY5 whose callback reads the key, as a consumer may
 */
PRIVATE int on_feed_reentrant(
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
    snprintf(item, sizeof(item), "%s%s:%ld",
        got5[0]? " ": "",
        record? kw_get_str(0, record, "content", "?", 0): "?",
        (long)rowid
    );
    strncat(got5, item, sizeof(got5) - strlen(got5) - 1);
    JSON_DECREF(record)

    json_t *it = tranger2_open_iterator(
        tranger, tranger2_topic_name(topic), key,
        json_pack("{s:b}", "only_md", 1), NULL, "reentrant", "", NULL, NULL
    );
    if(it) {
        tranger2_close_iterator(tranger, it);
    }
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

PRIVATE json_t *create_topic(json_t *tranger, const char *topic_name, system_flag2_t key_type)
{
    return tranger2_create_topic(
        tranger, topic_name, "id", "tm",
        json_pack("{s:i, s:s, s:i, s:i}",
            "on_critical_error", 4,
            "filename_mask", "%Y-%m-%d",
            "xpermission" , 02700,
            "rpermission", 0600
        ),
        key_type,
        json_pack("{s:s, s:I, s:s}",
            "id", "",
            "tm", (json_int_t)0,
            "content", ""
        ),
        0
    );
}

PRIVATE json_int_t append_one(json_t *tranger, json_int_t id, uint64_t t, const char *content)
{
    json_t *record = json_pack("{s:I, s:I, s:s}",
        "id", id,
        "tm", (json_int_t)t,
        "content", content
    );
    md2_record_ex_t md = {0};
    if(tranger2_append_record(tranger, TOPIC_NAME, t, 0, &md, record) < 0) {
        return -1;
    }
    return (json_int_t)md.g_rowid;
}

PRIVATE json_int_t append_long(json_t *tranger, uint64_t t, const char *content)
{
    json_t *record = json_pack("{s:s, s:I, s:s}",
        "id", long_key,
        "tm", (json_int_t)t,
        "content", content
    );
    md2_record_ex_t md = {0};
    if(tranger2_append_record(tranger, TOPIC_LONG, t, 0, &md, record) < 0) {
        return -1;
    }
    return (json_int_t)md.g_rowid;
}

PRIVATE void drain(int turns)
{
    for(int i = 0; i < turns; i++) {
        yev_loop_run_once(yev_loop);
    }
}

PRIVATE void served(
    json_t *tranger,
    const char *topic_name,
    const char *key,
    char *bf,
    size_t bfsize
)
{
    bf[0] = 0;
    json_t *data = json_array();
    json_t *it = tranger2_open_iterator(
        tranger, topic_name, key, json_object(), NULL, "served", "", data, NULL
    );
    int idx; json_t *record;
    json_array_foreach(data, idx, record) {
        char item[64];
        snprintf(item, sizeof(item), "%s%s:%ld",
            bf[0]? " ": "",
            kw_get_str(0, record, "content", "?", 0),
            (long)kw_get_int(0, record, "__md_tranger__`g_rowid", -1, 0)
        );
        strncat(bf, item, bfsize - strlen(bf) - 1);
    }
    if(it) {
        tranger2_close_iterator(tranger, it);
    }
    JSON_DECREF(data)
}

/*
 *  The cells the follower holds of a key, as "<file id> ..."
 */
PRIVATE void cells_of(json_t *tranger, const char *topic_name, const char *key, char *bf, size_t bfsize)
{
    bf[0] = 0;
    json_t *topic = tranger2_topic(tranger, topic_name);
    json_t *cells = json_object_get(
        json_object_get(json_object_get(topic, "cache"), key), "files"
    );
    int idx; json_t *cell;
    json_array_foreach(cells, idx, cell) {
        if(bf[0]) {
            strncat(bf, " ", bfsize - strlen(bf) - 1);
        }
        strncat(bf, json_string_value(json_object_get(cell, "id")), bfsize - strlen(bf) - 1);
    }
}

PRIVATE int expect(const char *what, const char *got_, const char *expected)
{
    if(strcmp(got_, expected) != 0) {
        printf("%sERROR%s --> %s: '%s', expected '%s'\n",
            On_Red BWhite, Color_Off, what, got_, expected);
        return -1;
    }
    return 0;
}

PRIVATE int expect_int(const char *what, json_int_t got_, json_int_t expected)
{
    if(got_ != expected) {
        printf("%sERROR%s --> %s: %ld, expected %ld\n",
            On_Red BWhite, Color_Off, what, (long)got_, (long)expected);
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
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database),
        getenv("HOME"), "tests_yuneta", DATABASE, NULL);
    rmrdir(path_database);

    memset(long_key, 'k', sizeof(long_key) - 1);
    long_key[sizeof(long_key) - 1] = 0;

    /*-------------------------------------*
     *  A master with three days of two
     *  keys, a follower with a feed of
     *  one of them
     *-------------------------------------*/
    set_expected_results(
        "purge follower: setup",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_tranger(TRUE);
    if(!tm || !create_topic(tm, TOPIC_NAME, sf_int_key) ||
            !create_topic(tm, TOPIC_LONG, sf_string_key)) {
        if(tm) {
            tranger2_shutdown(tm);
        }
        return -1;
    }
    const char *contents[3] = {"A", "B", "C"};
    for(int d = 0; d < 3; d++) {
        for(int r = 0; r < 2; r++) {
            char content[8];
            snprintf(content, sizeof(content), "%s%d", contents[d], r + 1);
            append_one(tm, 1, DAY1 + (uint64_t)d*86400 + (uint64_t)r, content);
            append_one(tm, 2, DAY1 + (uint64_t)d*86400 + (uint64_t)r, content);
            append_one(tm, 3, DAY1 + (uint64_t)d*86400 + (uint64_t)r, content);
            append_one(tm, 5, DAY1 + (uint64_t)d*86400 + (uint64_t)r, content);
            append_one(tm, 6, DAY1 + (uint64_t)d*86400 + (uint64_t)r, content);
            append_one(tm, 7, DAY1 + (uint64_t)d*86400 + (uint64_t)r, content);
            append_long(tm, DAY1 + (uint64_t)d*86400 + (uint64_t)r, content);
        }
    }

    json_t *tf = startup_tranger(FALSE);
    if(!tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE) || !tranger2_open_topic(tf, TOPIC_LONG, TRUE)) {
        printf("%sERROR%s --> cannot open the follower\n", On_Red BWhite, Color_Off);
        if(tf) {
            tranger2_shutdown(tf);
        }
        tranger2_shutdown(tm);
        return -1;
    }
    json_t *tf_feed = tf;
    json_t *rt = tranger2_open_rt_disk(
        tf, TOPIC_NAME, KEY, NULL, on_feed_record, "rt_follower", "", NULL
    );
    json_t *rt_long = tranger2_open_rt_disk(
        tf, TOPIC_LONG, long_key, NULL, on_feed_record, "rt_follower_long", "", NULL
    );
    if(!rt || !rt_long) {
        result += -1;
    }
    drain(10);
    served(tf, TOPIC_NAME, KEY, bf, sizeof(bf));
    result += expect("follower before", bf, "A1:1 A2:2 B1:3 B2:4 C1:5 C2:6");
    result += test_json(NULL);

    /*-------------------------------------*
     *  The follower with a feed hears the
     *  purge, and applies it at once
     *-------------------------------------*/
    set_expected_results("purge follower: the signal", NULL, NULL, NULL, 1);
    result += expect_int("purged", tranger2_delete_key_files(tm, TOPIC_NAME, KEY, DAY2), 2);
    drain(20);
    cells_of(tf, TOPIC_NAME, KEY, bf, sizeof(bf));
    result += expect("follower cells", bf, "2000-01-02 2000-01-03");
    json_t *range = tranger2_topic_key_range(tf, TOPIC_NAME, KEY);
    result += expect_int("follower base", kw_get_int(0, range, "purged", -1, 0), 2);
    JSON_DECREF(range)
    served(tf, TOPIC_NAME, KEY, bf, sizeof(bf));
    result += expect("follower after", bf, "B1:3 B2:4 C1:5 C2:6");
    result += test_json(NULL);

    /*-------------------------------------*
     *  Its feed goes on with the
     *  master's g_rowid
     *-------------------------------------*/
    set_expected_results("purge follower: the feed goes on", NULL, NULL, NULL, 1);
    got[0] = 0;
    result += expect_int("master append", append_one(tm, 1, DAY3 + 100, "D1"), 7);
    drain(30);
    result += expect("feed got", got, "D1:7");
    served(tf, TOPIC_NAME, KEY, bf, sizeof(bf));
    result += expect("follower after the append", bf, "B1:3 B2:4 C1:5 C2:6 D1:7");
    result += test_json(NULL);

    /*-------------------------------------*
     *  A pager of the follower with a
     *  negative rowid bound, the key grows,
     *  the master purges it
     *-------------------------------------*/
    set_expected_results("purge follower: a negative rowid bound", NULL, NULL, NULL, 1);
    {
        json_t *rt7 = tranger2_open_rt_disk(
            tf, TOPIC_NAME, KEY7, NULL, on_feed_record, "rt_key7", "", NULL
        );
        drain(10);
        json_t *it7 = tranger2_open_iterator(
            tf, TOPIC_NAME, KEY7, json_pack("{s:I}", "from_rowid", (json_int_t)-3),
            NULL, "neg_from", "", NULL, NULL
        );
        json_t *page = tranger2_iterator_get_page(tf, it7, 1, 10, FALSE);
        char got7[128];
        int i7; json_t *rec;
        got7[0] = 0;
        json_array_foreach(json_object_get(page, "data"), i7, rec) {
            char item[32];
            snprintf(item, sizeof(item), "%s%s:%ld", got7[0]? " ": "",
                kw_get_str(0, rec, "content", "?", 0),
                (long)kw_get_int(0, rec, "__md_tranger__`g_rowid", -1, 0));
            strncat(got7, item, sizeof(got7) - strlen(got7) - 1);
        }
        JSON_DECREF(page)
        result += expect("from -3, at the open", got7, "B2:4 C1:5 C2:6");

        got[0] = 0;
        append_one(tm, 7, DAY3 + 86400, "D1");
        append_one(tm, 7, DAY3 + 86400 + 1, "D2");
        append_one(tm, 7, DAY3 + 86400 + 2, "D3");
        drain(30);
        result += expect("the feed got the new rows", got, "D1:7 D2:8 D3:9");
        result += expect_int("purged", tranger2_delete_key_files(tm, TOPIC_NAME, KEY7, DAY2), 2);
        drain(20);

        page = tranger2_iterator_get_page(tf, it7, 1, 10, FALSE);
        got7[0] = 0;
        json_array_foreach(json_object_get(page, "data"), i7, rec) {
            char item[32];
            snprintf(item, sizeof(item), "%s%s:%ld", got7[0]? " ": "",
                kw_get_str(0, rec, "content", "?", 0),
                (long)kw_get_int(0, rec, "__md_tranger__`g_rowid", -1, 0));
            strncat(got7, item, sizeof(got7) - strlen(got7) - 1);
        }
        JSON_DECREF(page)
        result += expect("from -3, after the appends and the purge", got7, "B2:4 C1:5 C2:6");
        tranger2_close_iterator(tf, it7);
        tranger2_close_rt_disk(tf, rt7);
        drain(10);
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  A key too long for the name of the
     *  signal: `.q.<sha256>`
     *-------------------------------------*/
    set_expected_results("purge follower: a long key", NULL, NULL, NULL, 1);
    result += expect_int("purged", tranger2_delete_key_files(tm, TOPIC_LONG, long_key, DAY3), 4);
    drain(20);
    cells_of(tf, TOPIC_LONG, long_key, bf, sizeof(bf));
    result += expect("follower cells", bf, "2000-01-03");
    served(tf, TOPIC_LONG, long_key, bf, sizeof(bf));
    result += expect("follower after", bf, "C1:5 C2:6");
    result += test_json(NULL);

    /*-------------------------------------*
     *  A follower with no feed hears
     *  nothing, and finds the purge before
     *  its next read
     *-------------------------------------*/
    set_expected_results("purge follower: no signal heard", NULL, NULL, NULL, 1);
    json_t *tf2 = startup_tranger(FALSE);
    if(!tf2 || !tranger2_open_topic(tf2, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> cannot open the second follower\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_int("purged", tranger2_delete_key_files(tm, TOPIC_NAME, KEY2, DAY3), 4);
    drain(20);
    cells_of(tf, TOPIC_NAME, KEY2, bf, sizeof(bf));
    result += expect("the follower with a feed heard it", bf, "2000-01-03");
    tf = tf2;   // the rest of the step reads with the deaf one
    cells_of(tf, TOPIC_NAME, KEY2, bf, sizeof(bf));
    result += expect("follower cells, nothing heard", bf, "2000-01-01 2000-01-02 2000-01-03");
    served(tf, TOPIC_NAME, KEY2, bf, sizeof(bf));
    result += expect("follower read", bf, "C1:5 C2:6");
    cells_of(tf, TOPIC_NAME, KEY2, bf, sizeof(bf));
    result += expect("follower cells after the read", bf, "2000-01-03");
    {
        /*  A page of an iterator opened before the purge was found  */
        json_t *it = tranger2_open_iterator(
            tf, TOPIC_NAME, KEY2, json_object(), NULL, "paged", "", NULL, NULL
        );
        result += expect_int("master purges again", tranger2_delete_key_files(tm, TOPIC_NAME, KEY2, DAY3 + 86400), 2);
        drain(20);
        json_t *page = tranger2_iterator_get_page(tf, it, 1, 10, FALSE);
        result += expect_int("the page of a key purged whole", (json_int_t)json_array_size(json_object_get(page, "data")), 0);
        result += expect_int("its total: the positions it had, empty", kw_get_int(0, page, "total_rows", -1, 0), 2);
        JSON_DECREF(page)
        tranger2_close_iterator(tf, it);
    }
    tf = tf_feed;
    tranger2_shutdown(tf2);
    result += test_json(NULL);

    /*-------------------------------------*
     *  A file the follower flagged
     *  unreadable, then purged by the
     *  master: not a flag for ever
     *-------------------------------------*/
    if(geteuid() != 0) {
        char md2_day1[PATH_MAX];
        build_path(md2_day1, sizeof(md2_day1), path_database, TOPIC_NAME, "keys", KEY3, "2000-01-01.md2", NULL);
        chmod(md2_day1, 0);
        set_expected_results(
            "purge follower: a flagged file purged",
            json_pack("[{s:s},{s:s},{s:s}]",
                "msg", "Cannot open md2 file",
                "msg", "md2 file of the key unreadable when its cache was built: every load of the key says load_failed",
                "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built"
            ),
            NULL, NULL, 1
        );
        json_t *tf3 = startup_tranger(FALSE);
        if(!tf3 || !tranger2_open_topic(tf3, TOPIC_NAME, TRUE)) {
            result += -1;
        }
        /*  A filtered pager of the key, opened over the flag  */
        json_t *it3f = tranger2_open_iterator(
            tf3, TOPIC_NAME, KEY3, json_pack("{s:I}", "from_t", (json_int_t)DAY1),
            NULL, "pager_flagged", "", NULL, NULL
        );
        result += test_json(NULL);

        set_expected_results(
            "purge follower: the page before the purge",
            json_pack("[{s:s}]",
                "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built"
            ),
            NULL, NULL, 1
        );
        json_t *it3u = tranger2_open_iterator(
            tf3, TOPIC_NAME, KEY3, json_object(), NULL, "pager_flagged_plain", "", NULL, NULL
        );
        json_t *page1u = tranger2_iterator_get_page(tf3, it3u, 1, 2, FALSE);
        result += expect_int("unfiltered page 1 before the purge",
            (json_int_t)json_array_size(json_object_get(page1u, "data")), 2);
        JSON_DECREF(page1u)
        json_t *page1 = tranger2_iterator_get_page(tf3, it3f, 1, 2, FALSE);
        result += expect_int("page 1 before the purge: the 2 rows the follower counted",
            (json_int_t)json_array_size(json_object_get(page1, "data")), 2);
        JSON_DECREF(page1)
        result += test_json(NULL);

        set_expected_results(
            "purge follower: a flagged file purged, read",
            json_pack("[{s:s}]",
                "msg", "The numbering of the key moved to the master's: the records handed before had g_rowids below the master's"
            ),
            NULL, NULL, 1
        );
        /*
         *  The flagged file and one the follower counted: its g_rowids move
         *  by the rows of the flagged one
         */
        result += expect_int("purged", tranger2_delete_key_files(tm, TOPIC_NAME, KEY3, DAY3), 4);
        drain(20);
        served(tf3, TOPIC_NAME, KEY3, bf, sizeof(bf));
        result += expect("the flagged follower reads", bf, "C1:5 C2:6");
        {
            json_t *data = json_array();
            json_t *it = tranger2_open_iterator(
                tf3, TOPIC_NAME, KEY3, json_object(), NULL, "flagged", "", data, NULL
            );
            result += expect_int("and its loads do not fail", kw_get_bool(0, it, "load_failed", 0, 0), 0);
            tranger2_close_iterator(tf3, it);
            JSON_DECREF(data)
        }
        {
            /*  Its numbering moved to the master's: its index taken again  */
            json_t *page = tranger2_iterator_get_page(tf3, it3f, 1, 10, FALSE);
            char got3[128] = "";
            int i3; json_t *rec;
            json_array_foreach(json_object_get(page, "data"), i3, rec) {
                char item[32];
                snprintf(item, sizeof(item), "%s%s:%ld", got3[0]? " ": "",
                    kw_get_str(0, rec, "content", "?", 0),
                    (long)kw_get_int(0, rec, "__md_tranger__`g_rowid", -1, 0));
                strncat(got3, item, sizeof(got3) - strlen(got3) - 1);
            }
            result += expect("the filtered pager opened over the flag", got3, "C1:5 C2:6");
            JSON_DECREF(page)

            /*  The page after the one read before the purge: no row skipped  */
            page = tranger2_iterator_get_page(tf3, it3f, 3, 2, FALSE);
            got3[0] = 0;
            json_array_foreach(json_object_get(page, "data"), i3, rec) {
                char item[32];
                snprintf(item, sizeof(item), "%s%s:%ld", got3[0]? " ": "",
                    kw_get_str(0, rec, "content", "?", 0),
                    (long)kw_get_int(0, rec, "__md_tranger__`g_rowid", -1, 0));
                strncat(got3, item, sizeof(got3) - strlen(got3) - 1);
            }
            result += expect("page 2 after the purge", got3, "C1:5 C2:6");
            JSON_DECREF(page)

            /*  The unfiltered pager: its positions stay too  */
            page = tranger2_iterator_get_page(tf3, it3u, 3, 2, FALSE);
            got3[0] = 0;
            json_array_foreach(json_object_get(page, "data"), i3, rec) {
                char item[32];
                snprintf(item, sizeof(item), "%s%s:%ld", got3[0]? " ": "",
                    kw_get_str(0, rec, "content", "?", 0),
                    (long)kw_get_int(0, rec, "__md_tranger__`g_rowid", -1, 0));
                strncat(got3, item, sizeof(got3) - strlen(got3) - 1);
            }
            result += expect("unfiltered page 2 after the purge", got3, "C1:5 C2:6");
            JSON_DECREF(page)
            tranger2_close_iterator(tf3, it3u);
            json_t *range3 = tranger2_topic_key_range(tf3, TOPIC_NAME, KEY3);
            result += expect_int("renumbered: the rows of the flagged file",
                kw_get_int(0, range3, "renumbered", -1, 0), 2);
            JSON_DECREF(range3)
            tranger2_close_iterator(tf3, it3f);
        }
        if(tf3) {
            tranger2_shutdown(tf3);
        }
        result += test_json(NULL);
    }

    /*-------------------------------------*
     *  A base the follower cannot read for
     *  a while: the key keeps its cells and
     *  its numbering, and its new records;
     *  the purge is applied when it reads
     *-------------------------------------*/
    if(geteuid() != 0) {
        set_expected_results(
            "purge follower: a base that cannot be read",
            json_pack("[{s:s},{s:s}]",
                "msg", "Cannot read the base of a purged key",
                "msg", "The base of a purged key cannot be read: the purge waits, the key keeps its cells and its numbering"
            ),
            NULL, NULL, 1
        );
        json_t *rt6 = tranger2_open_rt_disk(
            tf, TOPIC_NAME, KEY6, NULL, on_feed_record, "rt_key6", "", NULL
        );
        drain(10);
        /*  A late row of day 1, heard by the feed: it has a mark of that file  */
        result += expect_int("a late row of day 1", append_one(tm, 6, DAY1 + 500, "A3"), 3);
        drain(30);
        result += expect_int("the feed has a mark of day 1",
            json_object_get(json_object_get(json_object_get(rt6, "published"), KEY6), "2000-01-01")? 1 : 0, 1);
        char purged6[PATH_MAX];
        build_path(purged6, sizeof(purged6), path_database, TOPIC_NAME, "keys", KEY6, ".purged", NULL);
        result += expect_int("purged", tranger2_delete_key_files(tm, TOPIC_NAME, KEY6, DAY2), 3);
        chmod(purged6, 0);
        drain(20);
        /*  A new record while the base cannot be read: the master's g_rowid  */
        got[0] = 0;
        result += expect_int("master append", append_one(tm, 6, DAY3 + 100, "C3"), 8);
        drain(30);
        result += expect("the feed got it", got, "C3:8");
        /*  Read while it waits: the live rows, the files gone left out  */
        served(tf, TOPIC_NAME, KEY6, bf, sizeof(bf));
        result += expect("read while the purge waits", bf, "B1:4 B2:5 C1:6 C2:7 C3:8");
        result += expect_int("the feed's mark of the file gone goes with it",
            json_object_get(json_object_get(json_object_get(rt6, "published"), KEY6), "2000-01-01")? 1 : 0, 0);
        result += test_json(NULL);

        set_expected_results(
            "purge follower: a base read at last",
            json_pack("[{s:s}]",
                "msg", "The base of a purged key read at last: the purge is applied"
            ),
            NULL, NULL, 1
        );
        chmod(purged6, 0600);
        served(tf, TOPIC_NAME, KEY6, bf, sizeof(bf));
        result += expect("the follower reads the master's numbering", bf, "B1:4 B2:5 C1:6 C2:7 C3:8");
        json_t *range6 = tranger2_topic_key_range(tf, TOPIC_NAME, KEY6);
        result += expect_int("its base", kw_get_int(0, range6, "purged", -1, 0), 3);
        JSON_DECREF(range6)
        result += test_json(NULL);

        /*  The base it waited for guards the purged period too  */
        set_expected_results(
            "purge follower: a link of a purged file, after the wait",
            json_pack("[{s:s}]",
                "msg", "New records of a file of a purged period not read: the purge took them, they are in the base of the key"
            ),
            NULL, NULL, 1
        );
        got[0] = 0;
        result += link_back_purged_file(path_database, KEY6, "rt_key6");
        drain(30);
        result += expect("the feed got nothing", got, "");
        served(tf, TOPIC_NAME, KEY6, bf, sizeof(bf));
        result += expect("the numbering stays the master's", bf, "B1:4 B2:5 C1:6 C2:7 C3:8");
        unlink_purged_file(path_database, KEY6, "rt_key6");
        tranger2_close_rt_disk(tf, rt6);
        drain(10);
        result += test_json(NULL);
    }

    /*-------------------------------------*
     *  A signal left in a feed directory
     *-------------------------------------*/
    set_expected_results("purge follower: a signal left", NULL, NULL, NULL, 1);
    {
        char left[PATH_MAX];
        build_path(left, sizeof(left), path_database, TOPIC_NAME, "disks", "rt_follower", ".p." KEY, NULL);
        mkdir(left, 02770);
        drain(10);
        result += expect_int("purged", tranger2_delete_key_files(tm, TOPIC_NAME, KEY, DAY3), 2);
        drain(20);
        result += expect_int("the signal left is gone", access(left, F_OK) == 0, 0);
        cells_of(tf, TOPIC_NAME, KEY, bf, sizeof(bf));
        result += expect("the follower heard it", bf, "2000-01-03");
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  A file of a purged period still on
     *  disk, whose link a follower hears:
     *  its rows are in the base
     *-------------------------------------*/
    set_expected_results(
        "purge follower: a link of a purged file",
        json_pack("[{s:s}]",
            "msg", "New records of a file of a purged period not read: the purge took them, they are in the base of the key"
        ),
        NULL, NULL, 1
    );
    {
        got[0] = 0;
        result += link_back_purged_file(path_database, KEY, "rt_follower");
        drain(30);
        result += expect("the feed got nothing", got, "");
        cells_of(tf, TOPIC_NAME, KEY, bf, sizeof(bf));
        result += expect("the follower's cells", bf, "2000-01-03");
        json_t *rd_fds = json_object_get(
            json_object_get(tranger2_topic(tf, TOPIC_NAME), "rd_fd_files"), KEY
        );
        result += expect_int("no descriptor left open on the file refused",
            json_object_get(rd_fds, "2000-01-01.md2") || json_object_get(rd_fds, "2000-01-01.json"), 0);
        unlink_purged_file(path_database, KEY, "rt_follower");
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  A feed callback reading its key
     *  while a purge waits, unheard
     *-------------------------------------*/
    set_expected_results("purge follower: a read from a feed callback", NULL, NULL, NULL, 1);
    {
        json_t *rt5 = tranger2_open_rt_disk(
            tf, TOPIC_NAME, KEY5, NULL, on_feed_reentrant, "rt_reentrant", "", NULL
        );
        drain(10);
        /*
         *  A purge the follower is not told of: its base and its files, by
         *  hand (the master's own cache does not know, and numbers as before)
         */
        char key5_dir[PATH_MAX];
        char path[PATH_MAX];
        build_path(key5_dir, sizeof(key5_dir), path_database, TOPIC_NAME, "keys", KEY5, NULL);
        build_path(path, sizeof(path), key5_dir, ".purged", NULL);
        FILE *fp = fopen(path, "w");
        if(fp) {
            fputs("{\"upto\": \"2000-01-01\", \"rows\": 2, \"t\": 0}", fp);
            fclose(fp);
        }
        build_path(path, sizeof(path), key5_dir, "2000-01-01.md2", NULL);
        unlink(path);
        build_path(path, sizeof(path), key5_dir, "2000-01-01.json", NULL);
        unlink(path);

        got5[0] = 0;
        result += expect_int("master append", append_one(tm, 5, DAY3 + 86400 + 100, "D1"), 7);
        drain(30);
        result += expect("the feed got", got5, "D1:7");
        cells_of(tf, TOPIC_NAME, KEY5, bf, sizeof(bf));
        result += expect("the new file has its cell", bf, "2000-01-01 2000-01-02 2000-01-03 2000-01-04");
        served(tf, TOPIC_NAME, KEY5, bf, sizeof(bf));
        result += expect("the next read applies the purge", bf, "B1:3 B2:4 C1:5 C2:6 D1:7");
        cells_of(tf, TOPIC_NAME, KEY5, bf, sizeof(bf));
        result += expect("cells after", bf, "2000-01-02 2000-01-03 2000-01-04");
        tranger2_close_rt_disk(tf, rt5);
        drain(10);
    }
    result += test_json(NULL);

    set_expected_results("purge follower: shutdown", NULL, NULL, NULL, 1);
    tranger2_close_rt_disk(tf, rt);
    tranger2_close_rt_disk(tf, rt_long);
    drain(10);
    tranger2_shutdown(tf);
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
