/****************************************************************************
 *          test_delete_key_files.c
 *
 *  Retention: tranger2_delete_key_files() deletes the OLD files of a key
 *  and keeps every g_rowid where it was (DESIGN-retention.md). A g_rowid is
 *  a position computed from the files of a key, so without the base of the
 *  key (keys/<key>/.purged) deleting its first files renumbered the rest.
 *
 *  What is checked, on the master (the followers: see
 *  test_delete_key_files_follower):
 *
 *      - the records left keep their g_rowid, the next append gets the
 *        next one, and a reopen says the same;
 *      - the rowid bounds of a match_cond and the positions of a page
 *        count from the base;
 *      - the iterators opened before a purge, filtered or not, page what
 *        is left;
 *      - an append into a purged period is refused;
 *      - the guards: a cut in the future, a mask with no year;
 *      - the hard link a dead feed left is removed with its file; one that
 *        cannot be removed keeps the files of the period, and the next
 *        open of the master removes both;
 *      - a key purged whole keeps its base and goes on numbering;
 *      - a purge cut by a crash (the base written, the files still there)
 *        is finished by the open of the master, and skipped by a replica;
 *      - a `.purged.new` left behind is a purge that did not happen;
 *      - a `.purged` that cannot be read flags the key, it is no base of 0;
 *      - the positions of the pages of an iterator, filtered or not, stay
 *        where they were across a purge: the ones of the rows purged come
 *        back empty, and no live row is skipped;
 *      - a mask whose first conversion is not the year (%d-%m-%Y), and a
 *        topic of a treedb, are refused;
 *      - a delete of a key purged whole that fails keeps the key and its
 *        base: its next record goes on numbering, not from 1;
 *      - tranger2_delete_key() takes the base with the key;
 *      - a filtered iterator opened with a NEGATIVE rowid bound keeps the
 *        rows it indexed when the key grows and then its old files are
 *        purged: the bound is resolved once, at the open.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP "test_delete_key_files"

#define DATABASE    "tr_delete_key_files"
#define TOPIC_NAME  "topic_delete_key_files"
#define TOPIC_QUEUE "topic_delete_key_files_queue"
#define TOPIC_NOYEAR "topic_delete_key_files_noyear"
#define KEY         "0000000000000000001"
#define KEY2        "0000000000000000002"   // a purge cut by a crash
#define KEY3        "0000000000000000003"   // a .purged.new left, a .purged that cannot be read
#define KEY4        "0000000000000000004"   // pages across a purge
#define KEY5        "0000000000000000005"   // a negative rowid bound across appends and a purge
#define KEY6        "0000000000000000006"   // a link of a dead feed that cannot be removed
#define TOPIC_DMY   "topic_delete_key_files_dmy"
#define DAY1        946684800   // 2000-01-01
#define DAY2        (DAY1 + 86400)
#define DAY3        (DAY2 + 86400)
#define DAY4        (DAY3 + 86400)

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char path_database[PATH_MAX];

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int expect(const char *what, const char *got, const char *expected);
PRIVATE int expect_int(const char *what, json_int_t got, json_int_t expected);

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
        "filename_mask", "%Y",
        "xpermission" , 02770,
        "rpermission", 0600
    );
    return tranger2_startup(0, jn_tranger, yev_loop);
}

PRIVATE json_t *create_topic(json_t *tranger, const char *topic_name, const char *mask)
{
    return tranger2_create_topic(
        tranger, topic_name, "id", "tm",
        json_pack("{s:i, s:s, s:i, s:i}",
            "on_critical_error", 4,
            "filename_mask", mask,
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
    );
}

/*
 *  Append one record of key `id`, return its g_rowid (-1 refused)
 */
PRIVATE json_int_t append_one(
    json_t *tranger,
    const char *topic_name,
    json_int_t id,
    uint64_t t,
    const char *content
)
{
    json_t *record = json_pack("{s:I, s:I, s:s}",
        "id", id,
        "tm", (json_int_t)t,
        "content", content
    );
    md2_record_ex_t md = {0};
    if(tranger2_append_record(tranger, topic_name, t, 0, &md, record) < 0) {
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

/*
 *  "<content>:<g_rowid> ..." of a list of records
 */
PRIVATE void records_str(json_t *data, char *bf, size_t bfsize)
{
    bf[0] = 0;
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
}

/*
 *  What an iterator of `key` loads with `cond` (owned)
 */
PRIVATE void served(json_t *tranger, const char *key, json_t *cond, char *bf, size_t bfsize)
{
    json_t *data = json_array();
    json_t *it = tranger2_open_iterator(
        tranger, TOPIC_NAME, key, cond, NULL, "served", "", data, NULL
    );
    records_str(data, bf, bfsize);
    if(it) {
        tranger2_close_iterator(tranger, it);
    }
    JSON_DECREF(data)
}

/*
 *  A page of an iterator opened for paging
 */
PRIVATE void paged(json_t *tranger, json_t *it, json_int_t from, size_t limit, char *bf, size_t bfsize)
{
    json_t *page = tranger2_iterator_get_page(tranger, it, from, limit, FALSE);
    records_str(json_object_get(page, "data"), bf, bfsize);
    JSON_DECREF(page)
}

PRIVATE BOOL key_file_exists(const char *key, const char *filename)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, TOPIC_NAME, "keys", key, filename, NULL);
    return access(path, F_OK) == 0;
}

PRIVATE int write_key_file(const char *key, const char *filename, const char *content)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, TOPIC_NAME, "keys", key, filename, NULL);
    int fd = open(path, O_CREAT|O_TRUNC|O_WRONLY|O_CLOEXEC, 0600);
    if(fd < 0) {
        printf("%sERROR%s --> cannot write %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }
    ssize_t len = (ssize_t)strlen(content);
    int ret = (write(fd, content, (size_t)len) == len)? 0 : -1;
    close(fd);
    return ret;
}

PRIVATE json_int_t key_range_int(json_t *tranger, const char *key, const char *field)
{
    json_t *range = tranger2_topic_key_range(tranger, TOPIC_NAME, key);
    json_int_t v = kw_get_int(0, range, field, -1, 0);
    JSON_DECREF(range)
    return v;
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
        printf("%sERROR%s --> %s: %ld, expected %ld\n",
            On_Red BWhite, Color_Off, what, (long)got, (long)expected);
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
     *  Three days of a key
     *-------------------------------------*/
    set_expected_results(
        "delete key files: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_tranger(TRUE);
    if(!tm || !create_topic(tm, TOPIC_NAME, "%Y-%m-%d")) {
        if(tm) {
            tranger2_shutdown(tm);
        }
        return -1;
    }
    const char *days[3][3] = {
        {"A1", "A2", "A3"},
        {"B1", "B2", "B3"},
        {"C1", "C2", "C3"}
    };
    for(int d = 0; d < 3; d++) {
        for(int r = 0; r < 3; r++) {
            append_one(tm, TOPIC_NAME, 1, DAY1 + (uint64_t)d*86400 + (uint64_t)r, days[d][r]);
        }
    }
    for(int d = 0; d < 3; d++) {
        char content[8];
        for(int r = 0; r < 2; r++) {
            snprintf(content, sizeof(content), "%c%d", 'P' + d, r + 1);
            append_one(tm, TOPIC_NAME, 2, DAY1 + (uint64_t)d*86400 + (uint64_t)r, content);
            snprintf(content, sizeof(content), "%c%d", 'X' + d, r + 1);
            append_one(tm, TOPIC_NAME, 3, DAY1 + (uint64_t)d*86400 + (uint64_t)r, content);
        }
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Purge the first day: the rest keep
     *  their g_rowid, the iterators opened
     *  before page what is left
     *-------------------------------------*/
    set_expected_results("delete key files: purge and numbering", NULL, NULL, NULL, 1);
    json_t *it_plain = tranger2_open_iterator(
        tm, TOPIC_NAME, KEY, json_object(), NULL, "plain", "", NULL, NULL
    );
    json_t *it_filtered = tranger2_open_iterator(
        tm, TOPIC_NAME, KEY,
        json_pack("{s:I}", "from_t", (json_int_t)(DAY1 + 1)),
        NULL, "filtered", "", NULL, NULL
    );
    result += expect_int("rows deleted", tranger2_delete_key_files(tm, TOPIC_NAME, KEY, DAY2), 3);
    result += expect_int("first day's md2 gone", key_file_exists(KEY, "2000-01-01.md2"), 0);
    result += expect_int("first day's content gone", key_file_exists(KEY, "2000-01-01.json"), 0);
    result += expect_int("second day's md2 kept", key_file_exists(KEY, "2000-01-02.md2"), 1);
    result += expect_int("base written", key_file_exists(KEY, ".purged"), 1);

    served(tm, KEY, json_object(), bf, sizeof(bf));
    result += expect("served after purge", bf, "B1:4 B2:5 B3:6 C1:7 C2:8 C3:9");
    result += expect_int("key size: the live rows", (json_int_t)tranger2_topic_key_size(tm, TOPIC_NAME, KEY), 6);
    result += expect_int("range: rows", key_range_int(tm, KEY, "rows"), 6);
    result += expect_int("range: purged", key_range_int(tm, KEY, "purged"), 3);

    paged(tm, it_plain, 1, 100, bf, sizeof(bf));
    result += expect("unfiltered iterator, page 1", bf, "B1:4 B2:5 B3:6 C1:7 C2:8 C3:9");
    paged(tm, it_plain, 2, 2, bf, sizeof(bf));
    result += expect("unfiltered iterator, positions 2-3: purged since its open", bf, "");
    paged(tm, it_plain, 5, 2, bf, sizeof(bf));
    result += expect("unfiltered iterator, positions 5-6: where they were", bf, "B2:5 B3:6");
    paged(tm, it_filtered, 1, 100, bf, sizeof(bf));
    result += expect("filtered iterator, page 1", bf, "B1:4 B2:5 B3:6 C1:7 C2:8 C3:9");
    tranger2_close_iterator(tm, it_plain);
    tranger2_close_iterator(tm, it_filtered);

    result += expect_int("next append", append_one(tm, TOPIC_NAME, 1, DAY3 + 100, "D1"), 10);
    result += test_json(NULL);

    /*-------------------------------------*
     *  The rowid bounds count from the base
     *-------------------------------------*/
    set_expected_results("delete key files: rowid bounds", NULL, NULL, NULL, 1);
    struct {
        const char *name;
        json_int_t from_rowid;
        json_int_t to_rowid;
        const char *expected;
    } bounds[] = {
        {"from 0",          0,      0,      "B1:4 B2:5 B3:6 C1:7 C2:8 C3:9 D1:10"},
        {"from 1 (purged)", 1,      0,      "B1:4 B2:5 B3:6 C1:7 C2:8 C3:9 D1:10"},
        {"from the base",   3,      0,      "B1:4 B2:5 B3:6 C1:7 C2:8 C3:9 D1:10"},
        {"from base+1",     4,      0,      "B1:4 B2:5 B3:6 C1:7 C2:8 C3:9 D1:10"},
        {"from 5",          5,      0,      "B2:5 B3:6 C1:7 C2:8 C3:9 D1:10"},
        {"from -1",         -1,     0,      "D1:10"},
        {"from -2",         -2,     0,      "C3:9 D1:10"},
        {"from -(rows+5)",  -12,    0,      "B1:4 B2:5 B3:6 C1:7 C2:8 C3:9 D1:10"},
        {"from past end",   11,     0,      ""},
        {"to past end",     0,      100,    "B1:4 B2:5 B3:6 C1:7 C2:8 C3:9 D1:10"},
        {"to the base",     0,      3,      ""},
        {"to 5",            0,      5,      "B1:4 B2:5"},
        {"to -1",           0,      -1,     "B1:4 B2:5 B3:6 C1:7 C2:8 C3:9 D1:10"},
        {"to -6",           0,      -6,     "B1:4 B2:5"},
        {"to -(rows+5)",    0,      -12,    ""},
        {0}
    };
    for(int i = 0; bounds[i].name; i++) {
        json_t *cond = json_object();
        if(bounds[i].from_rowid) {
            json_object_set_new(cond, "from_rowid", json_integer(bounds[i].from_rowid));
        }
        if(bounds[i].to_rowid) {
            json_object_set_new(cond, "to_rowid", json_integer(bounds[i].to_rowid));
        }
        served(tm, KEY, cond, bf, sizeof(bf));
        result += expect(bounds[i].name, bf, bounds[i].expected);
    }
    served(tm, KEY, json_pack("{s:b}", "backward", 1), bf, sizeof(bf));
    result += expect("backward", bf, "D1:10 C3:9 C2:8 C1:7 B3:6 B2:5 B1:4");
    {
        /*
         *  The size of a loading iterator with bounds and no index: the rows
         *  of the files it reads (the last one, 4 rows), as in 7.27.4 -- not
         *  the positions of the whole key (7)
         */
        json_t *data = json_array();
        json_t *it = tranger2_open_iterator(
            tm, TOPIC_NAME, KEY, json_pack("{s:I}", "from_rowid", (json_int_t)-2),
            NULL, "bounded", "", data, NULL
        );
        result += expect_int("a bounded loading iterator: its rows", (json_int_t)json_array_size(data), 2);
        result += expect_int("and its size", (json_int_t)tranger2_iterator_size(it), 4);
        tranger2_close_iterator(tm, it);
        JSON_DECREF(data)
    }
    served(tm, KEY, json_pack("{s:b, s:I}", "backward", 1, "to_rowid", (json_int_t)5), bf, sizeof(bf));
    result += expect("backward to 5", bf, "B2:5 B1:4");
    result += test_json(NULL);

    /*-------------------------------------*
     *  A reopen says the same
     *-------------------------------------*/
    set_expected_results("delete key files: a reopen says the same", NULL, NULL, NULL, 1);
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
    served(tm, KEY, json_object(), bf, sizeof(bf));
    result += expect("reopened", bf, "B1:4 B2:5 B3:6 C1:7 C2:8 C3:9 D1:10");
    result += expect_int("reopened: purged", key_range_int(tm, KEY, "purged"), 3);
    result += expect_int("reopened: next append", append_one(tm, TOPIC_NAME, 1, DAY3 + 200, "E1"), 11);
    result += test_json(NULL);

    /*-------------------------------------*
     *  An append into a purged period
     *-------------------------------------*/
    set_expected_results(
        "delete key files: an append into a purged period",
        json_pack("[{s:s}]",
            "msg", "Cannot append record, it falls in a purged period of its key"
        ),
        NULL, NULL, 1
    );
    result += expect_int("late append refused", append_one(tm, TOPIC_NAME, 1, DAY1 + 500, "L1"), -1);
    result += expect_int("late append wrote nothing", key_file_exists(KEY, "2000-01-01.md2"), 0);
    result += test_json(NULL);

    /*-------------------------------------*
     *  The guards
     *-------------------------------------*/
    set_expected_results(
        "delete key files: the guards",
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Cannot delete the old files of a key, the cut is in the future: the file being written would go",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Cannot delete the old files of a key, the file ids of the file name mask of its topic do not sort as time",
            "msg", "Cannot delete the old files of a key, the file ids of the file name mask of its topic do not sort as time"
        ),
        NULL, NULL, 1
    );
    uint64_t future = (uint64_t)time_in_seconds() + 3*86400;
    result += expect_int("cut in the future", tranger2_delete_key_files(tm, TOPIC_NAME, KEY, future), -1);
    served(tm, KEY, json_object(), bf, sizeof(bf));
    result += expect("nothing deleted by the future cut", bf, "B1:4 B2:5 B3:6 C1:7 C2:8 C3:9 D1:10 E1:11");
    if(!create_topic(tm, TOPIC_QUEUE, "queue") || !create_topic(tm, TOPIC_NOYEAR, "%m-%d")) {
        result += -1;
    }
    append_one(tm, TOPIC_QUEUE, 1, DAY1, "Q1");
    append_one(tm, TOPIC_NOYEAR, 1, DAY1, "N1");
    result += expect_int("a queue",
        tranger2_delete_key_files(tm, TOPIC_QUEUE, KEY, DAY4), -1);
    result += expect_int("a mask with no year",
        tranger2_delete_key_files(tm, TOPIC_NOYEAR, KEY, DAY4), -1);
    result += test_json(NULL);

    /*-------------------------------------*
     *  The hard link a dead feed left
     *-------------------------------------*/
    set_expected_results("delete key files: the link of a dead feed", NULL, NULL, NULL, 1);
    char dead_dir[PATH_MAX];
    char dead_link[PATH_MAX];
    char md2_day2[PATH_MAX];
    build_path(dead_dir, sizeof(dead_dir), path_database, TOPIC_NAME, "disks", "rt_dead", KEY, NULL);
    build_path(dead_link, sizeof(dead_link), dead_dir, "2000-01-02.md2", NULL);
    build_path(md2_day2, sizeof(md2_day2), path_database, TOPIC_NAME, "keys", KEY, "2000-01-02.md2", NULL);
    mkrdir(dead_dir, 02770);
    if(link(md2_day2, dead_link) < 0) {
        printf("%sERROR%s --> cannot link %s\n", On_Red BWhite, Color_Off, dead_link);
        result += -1;
    }
    result += expect_int("rows deleted", tranger2_delete_key_files(tm, TOPIC_NAME, KEY, DAY3), 3);
    result += expect_int("the dead feed's link removed", access(dead_link, F_OK) == 0, 0);
    served(tm, KEY, json_object(), bf, sizeof(bf));
    result += expect("served", bf, "C1:7 C2:8 C3:9 D1:10 E1:11");
    result += expect_int("purged", key_range_int(tm, KEY, "purged"), 6);
    result += test_json(NULL);

    /*-------------------------------------*
     *  A link of a dead feed that cannot be
     *  removed: the files of the period stay,
     *  and the next open removes them all
     *-------------------------------------*/
    if(geteuid() != 0) {
        set_expected_results(
            "delete key files: a link that cannot be removed",
            json_pack("[{s:s}]",
                "msg", "Cannot remove a feed's link of a purged period, unlink() FAILED: the files of the period stay, removed at the next open"
            ),
            NULL, NULL, 1
        );
        append_one(tm, TOPIC_NAME, 6, DAY1, "A1");
        append_one(tm, TOPIC_NAME, 6, DAY1 + 1, "A2");
        append_one(tm, TOPIC_NAME, 6, DAY2, "B1");
        char dead6_dir[PATH_MAX];
        char dead6_link[PATH_MAX];
        char md2_6[PATH_MAX];
        build_path(dead6_dir, sizeof(dead6_dir), path_database, TOPIC_NAME, "disks", "rt_dead6", KEY6, NULL);
        build_path(dead6_link, sizeof(dead6_link), dead6_dir, "2000-01-01.md2", NULL);
        build_path(md2_6, sizeof(md2_6), path_database, TOPIC_NAME, "keys", KEY6, "2000-01-01.md2", NULL);
        mkrdir(dead6_dir, 02770);
        if(link(md2_6, dead6_link) < 0) {
            printf("%sERROR%s --> cannot link %s\n", On_Red BWhite, Color_Off, dead6_link);
            result += -1;
        }
        chmod(dead6_dir, 0550);
        result += expect_int("rows deleted", tranger2_delete_key_files(tm, TOPIC_NAME, KEY6, DAY2), 2);
        chmod(dead6_dir, 02770);
        result += expect_int("the link stays", access(dead6_link, F_OK) == 0, 1);
        result += expect_int("and so does the md2 of the period", key_file_exists(KEY6, "2000-01-01.md2"), 1);
        served(tm, KEY6, json_object(), bf, sizeof(bf));
        result += expect("served: the base counts the period", bf, "B1:3");
        result += test_json(NULL);

        set_expected_results(
            "delete key files: the next open removes the link",
            json_pack("[{s:s}]",
                "msg", "Files of a purged period still on disk, a purge cut by a crash: removed now"
            ),
            NULL, NULL, 1
        );
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
        result += expect_int("the link removed", access(dead6_link, F_OK) == 0, 0);
        result += expect_int("the md2 removed", key_file_exists(KEY6, "2000-01-01.md2"), 0);
        result += expect_int("the content removed", key_file_exists(KEY6, "2000-01-01.json"), 0);
        served(tm, KEY6, json_object(), bf, sizeof(bf));
        result += expect("served after the open", bf, "B1:3");
        result += test_json(NULL);
    }

    /*-------------------------------------*
     *  A key purged whole
     *-------------------------------------*/
    set_expected_results("delete key files: a key purged whole", NULL, NULL, NULL, 1);
    result += expect_int("rows deleted", tranger2_delete_key_files(tm, TOPIC_NAME, KEY, DAY4), 5);
    result += expect_int("key size", (json_int_t)tranger2_topic_key_size(tm, TOPIC_NAME, KEY), 0);
    result += expect_int("purged", key_range_int(tm, KEY, "purged"), 11);
    result += expect_int("no file, no span: fr_t", key_range_int(tm, KEY, "fr_t"), 0);
    result += expect_int("no file, no span: to_t", key_range_int(tm, KEY, "to_t"), 0);
    result += expect_int("no file, no span: fr_tm", key_range_int(tm, KEY, "fr_tm"), 0);
    result += expect_int("no file, no span: to_tm", key_range_int(tm, KEY, "to_tm"), 0);
    served(tm, KEY, json_object(), bf, sizeof(bf));
    result += expect("served", bf, "");
    result += expect_int("nothing more to delete", tranger2_delete_key_files(tm, TOPIC_NAME, KEY, DAY4), 0);
    result += expect_int("next append", append_one(tm, TOPIC_NAME, 1, DAY4 + 10, "F1"), 12);
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
    served(tm, KEY, json_object(), bf, sizeof(bf));
    result += expect("reopened", bf, "F1:12");
    result += test_json(NULL);

    /*-------------------------------------*
     *  A purge cut by a crash: the base is
     *  written, the files are still there.
     *  A replica skips them, the master
     *  removes them.
     *-------------------------------------*/
    set_expected_results(
        "delete key files: a purge cut by a crash",
        json_pack("[{s:s}]",
            "msg", "Files of a purged period still on disk, a purge cut by a crash: removed now"
        ),
        NULL, NULL, 1
    );
    tranger2_shutdown(tm);
    drain(10);
    result += write_key_file(KEY2, ".purged", "{\"upto\": \"2000-01-01\", \"rows\": 2, \"t\": 0}");
    json_t *tr = startup_tranger(FALSE);
    if(!tr || !tranger2_open_topic(tr, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> cannot open the replica\n", On_Red BWhite, Color_Off);
        if(tr) {
            tranger2_shutdown(tr);
        }
        return -1;
    }
    served(tr, KEY2, json_object(), bf, sizeof(bf));
    result += expect("replica", bf, "Q1:3 Q2:4 R1:5 R2:6");
    result += expect_int("the replica removes nothing", key_file_exists(KEY2, "2000-01-01.md2"), 1);
    tranger2_shutdown(tr);
    drain(10);
    tm = startup_tranger(TRUE);
    if(!tm || !tranger2_open_topic(tm, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> cannot reopen the master\n", On_Red BWhite, Color_Off);
        if(tm) {
            tranger2_shutdown(tm);
        }
        return -1;
    }
    served(tm, KEY2, json_object(), bf, sizeof(bf));
    result += expect("master", bf, "Q1:3 Q2:4 R1:5 R2:6");
    result += expect_int("the master removed the md2", key_file_exists(KEY2, "2000-01-01.md2"), 0);
    result += expect_int("the master removed the content", key_file_exists(KEY2, "2000-01-01.json"), 0);
    result += expect_int("next append", append_one(tm, TOPIC_NAME, 2, DAY4, "S1"), 7);
    result += test_json(NULL);

    /*-------------------------------------*
     *  A .purged.new left behind: a purge
     *  that did not happen
     *-------------------------------------*/
    set_expected_results(
        "delete key files: a .purged.new left behind",
        json_pack("[{s:s}]",
            "msg", "Base of a purge that did not happen removed: the key is as it was before it"
        ),
        NULL, NULL, 1
    );
    tranger2_shutdown(tm);
    drain(10);
    result += write_key_file(KEY3, ".purged.new", "{\"upto\": \"2000-01-02\", \"rows\": 4");
    tm = startup_tranger(TRUE);
    if(!tm || !tranger2_open_topic(tm, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> cannot reopen the master\n", On_Red BWhite, Color_Off);
        if(tm) {
            tranger2_shutdown(tm);
        }
        return -1;
    }
    result += expect_int(".purged.new removed", key_file_exists(KEY3, ".purged.new"), 0);
    served(tm, KEY3, json_object(), bf, sizeof(bf));
    result += expect("the key as it was", bf, "X1:1 X2:2 Y1:3 Y2:4 Z1:5 Z2:6");
    result += test_json(NULL);

    /*-------------------------------------*
     *  A .purged that cannot be read is no
     *  base of 0: the key is flagged
     *-------------------------------------*/
    set_expected_results(
        "delete key files: a .purged that cannot be read",
        json_pack("[{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Cannot read the base of a purged key",
            "msg", "The base of a purged key cannot be read: the key is flagged unlisted, every load of it says load_failed",
            "msg", "The history of the key is not whole: its directory could not be listed when its cache was built",
            "msg", "Cannot delete the old files of a key flagged unlisted: its files are not counted",
            "msg", "Cannot read the base of a purged key",
            "msg", "The base of a purged key cannot be read: the key is flagged unlisted, every load of it says load_failed",
            "msg", "Cannot append record, its key cannot be listed: its row would follow rows no cell counts",
            "msg", "key directory listed again: its files are counted, and the key is not flagged"
        ),
        NULL, NULL, 1
    );
    tranger2_shutdown(tm);
    drain(10);
    result += write_key_file(KEY3, ".purged", "not json");
    tm = startup_tranger(TRUE);
    if(!tm || !tranger2_open_topic(tm, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> cannot reopen the master\n", On_Red BWhite, Color_Off);
        if(tm) {
            tranger2_shutdown(tm);
        }
        return -1;
    }
    {
        json_t *data = json_array();
        json_t *it = tranger2_open_iterator(
            tm, TOPIC_NAME, KEY3, json_object(), NULL, "flagged", "", data, NULL
        );
        result += expect_int("load_failed", kw_get_bool(0, it, "load_failed", 0, 0), 1);
        result += expect_int("nothing loaded", (json_int_t)json_array_size(data), 0);
        if(it) {
            tranger2_close_iterator(tm, it);
        }
        JSON_DECREF(data)
    }
    result += expect_int("no purge of a flagged key", tranger2_delete_key_files(tm, TOPIC_NAME, KEY3, DAY2), -1);
    result += expect_int("append refused", append_one(tm, TOPIC_NAME, 3, DAY4, "W1"), -1);
    /*
     *  The base put right (a key never purged has none): the next append
     *  lists the key again
     */
    char purged_path[PATH_MAX];
    build_path(purged_path, sizeof(purged_path), path_database, TOPIC_NAME, "keys", KEY3, ".purged", NULL);
    unlink(purged_path);
    result += expect_int("append after the base is put right", append_one(tm, TOPIC_NAME, 3, DAY4, "W1"), 7);
    result += test_json(NULL);

    /*-------------------------------------*
     *  The pages of an iterator across a
     *  purge: their positions stay
     *-------------------------------------*/
    set_expected_results("delete key files: pages across a purge", NULL, NULL, NULL, 1);
    {
        const char *contents4[3][2] = {{"A1", "A2"}, {"B1", "B2"}, {"C1", "C2"}};
        for(int d = 0; d < 3; d++) {
            for(int r = 0; r < 2; r++) {
                append_one(tm, TOPIC_NAME, 4, DAY1 + (uint64_t)d*86400 + (uint64_t)r, contents4[d][r]);
            }
        }
        json_t *it_p = tranger2_open_iterator(
            tm, TOPIC_NAME, KEY4, json_object(), NULL, "pages_plain", "", NULL, NULL
        );
        json_t *it_f = tranger2_open_iterator(
            tm, TOPIC_NAME, KEY4, json_pack("{s:I}", "from_t", (json_int_t)DAY1),
            NULL, "pages_filtered", "", NULL, NULL
        );
        paged(tm, it_p, 1, 2, bf, sizeof(bf));
        result += expect("unfiltered, page 1 before", bf, "A1:1 A2:2");
        paged(tm, it_f, 1, 2, bf, sizeof(bf));
        result += expect("filtered, page 1 before", bf, "A1:1 A2:2");

        result += expect_int("purged", tranger2_delete_key_files(tm, TOPIC_NAME, KEY4, DAY2), 2);
        result += expect_int("unfiltered, its size before any page", (json_int_t)tranger2_iterator_size(it_p), 6);

        paged(tm, it_p, 3, 2, bf, sizeof(bf));
        result += expect("unfiltered, page 2 after: no row skipped", bf, "B1:3 B2:4");
        paged(tm, it_p, 1, 2, bf, sizeof(bf));
        result += expect("unfiltered, the positions purged", bf, "");
        result += expect_int("unfiltered, its size", (json_int_t)tranger2_iterator_size(it_p), 6);
        paged(tm, it_f, 3, 2, bf, sizeof(bf));
        result += expect("filtered, page 2 after: no row skipped", bf, "B1:3 B2:4");
        paged(tm, it_f, 1, 2, bf, sizeof(bf));
        result += expect("filtered, the positions purged", bf, "");
        result += expect_int("filtered, its size", (json_int_t)tranger2_iterator_size(it_f), 6);
        tranger2_close_iterator(tm, it_p);
        tranger2_close_iterator(tm, it_f);

        json_t *it_new = tranger2_open_iterator(
            tm, TOPIC_NAME, KEY4, json_object(), NULL, "pages_new", "", NULL, NULL
        );
        paged(tm, it_new, 1, 2, bf, sizeof(bf));
        result += expect("an iterator opened after, page 1", bf, "B1:3 B2:4");
        result += expect_int("its size", (json_int_t)tranger2_iterator_size(it_new), 4);
        result += expect_int("purged whole", tranger2_delete_key_files(tm, TOPIC_NAME, KEY4, DAY4), 4);
        result += expect_int("its size: its 4 positions, empty", (json_int_t)tranger2_iterator_size(it_new), 4);
        paged(tm, it_new, 1, 4, bf, sizeof(bf));
        result += expect("its page", bf, "");
        tranger2_close_iterator(tm, it_new);
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  A negative rowid bound, then rows
     *  appended, then a purge: the bound
     *  is the one the open resolved
     *-------------------------------------*/
    set_expected_results("delete key files: a negative rowid bound across a purge", NULL, NULL, NULL, 1);
    {
        char content[8];
        for(int d = 0; d < 3; d++) {
            for(int r = 0; r < 3; r++) {
                snprintf(content, sizeof(content), "%c%d", 'A' + d, r + 1);
                append_one(tm, TOPIC_NAME, 5, DAY1 + (uint64_t)d*86400 + (uint64_t)r, content);
            }
        }
        json_t *it_from = tranger2_open_iterator(
            tm, TOPIC_NAME, KEY5, json_pack("{s:I}", "from_rowid", (json_int_t)-4),
            NULL, "neg_from", "", NULL, NULL
        );
        json_t *it_both = tranger2_open_iterator(
            tm, TOPIC_NAME, KEY5,
            json_pack("{s:I, s:I}", "from_rowid", (json_int_t)-5, "to_rowid", (json_int_t)-2),
            NULL, "neg_both", "", NULL, NULL
        );
        paged(tm, it_from, 1, 10, bf, sizeof(bf));
        result += expect("from -4, at the open", bf, "B3:6 C1:7 C2:8 C3:9");
        paged(tm, it_both, 1, 10, bf, sizeof(bf));
        result += expect("from -5 to -2, at the open", bf, "B2:5 B3:6 C1:7 C2:8");
        for(int r = 0; r < 4; r++) {
            snprintf(content, sizeof(content), "D%d", r + 1);
            append_one(tm, TOPIC_NAME, 5, DAY4 + (uint64_t)r, content);
        }
        result += expect_int("purged", tranger2_delete_key_files(tm, TOPIC_NAME, KEY5, DAY2), 3);
        paged(tm, it_from, 1, 10, bf, sizeof(bf));
        result += expect("from -4, after the appends and the purge", bf, "B3:6 C1:7 C2:8 C3:9");
        result += expect_int("from -4, its size", (json_int_t)tranger2_iterator_size(it_from), 4);
        {
            json_t *page = tranger2_iterator_get_page(tm, it_from, 1, 10, TRUE);
            records_str(json_object_get(page, "data"), bf, sizeof(bf));
            JSON_DECREF(page)
        }
        result += expect("from -4, backward", bf, "C3:9 C2:8 C1:7 B3:6");
        paged(tm, it_both, 1, 10, bf, sizeof(bf));
        result += expect("from -5 to -2, after the appends and the purge", bf, "B2:5 B3:6 C1:7 C2:8");
        tranger2_close_iterator(tm, it_from);
        tranger2_close_iterator(tm, it_both);

        /*  A new open resolves against the key as it is now  */
        json_t *it_new = tranger2_open_iterator(
            tm, TOPIC_NAME, KEY5, json_pack("{s:I}", "from_rowid", (json_int_t)-4),
            NULL, "neg_new", "", NULL, NULL
        );
        paged(tm, it_new, 1, 10, bf, sizeof(bf));
        result += expect("from -4, opened after", bf, "D1:10 D2:11 D3:12 D4:13");
        tranger2_close_iterator(tm, it_new);
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  More guards: the year must come
     *  first, and a treedb topic is its
     *  treedb's
     *-------------------------------------*/
    set_expected_results(
        "delete key files: the year first, a treedb topic",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "Creating topic",
            "msg", "Cannot delete the old files of a key, the file ids of the file name mask of its topic do not sort as time",
            "msg", "Cannot delete the old files of a key of a topic of a treedb or a msg2db: its keys are its nodes, written through it"
        ),
        NULL, NULL, 1
    );
    {
        struct {
            const char *mask;
            BOOL sorts;
        } masks[] = {
            {"tracks-%Y-%m-%d", TRUE}, {"series-1d-%Y", TRUE}, {"%Y", TRUE}, {"%Y-%m", TRUE},
            {"%FT%T", TRUE}, {"%G-W%V", TRUE}, {"%Y%m%d%H%M%S", TRUE}, {"%Y-%j", TRUE},
            {"%G-W%V-%uT%H", TRUE}, {"%Y-%m-%dT%H", TRUE},
            {"%EY-%Om", FALSE}, {"%OY-%m-%d", FALSE}, {"%Y-%Em-%d", FALSE},
            {"%s", FALSE}, {"%Y-%d", FALSE}, {"%Y%H", FALSE}, {"%Y-%m-%dT%M", FALSE},
            {"%G-%j", FALSE}, {"%G-%d", FALSE}, {"%G-W%V-%d", FALSE}, {"%G-W%V%H", FALSE},
            {"%G", FALSE}, {"%%", FALSE}, {"%Y-%m-%", FALSE},
            {"%Y-%m-%d_%R", TRUE}, {"%%%Y", TRUE},
            {"queue", FALSE}, {"%d-%m-%Y", FALSE}, {"%Y-%d-%m", FALSE}, {"%Y-%b", FALSE},
            {"%Y-%a", FALSE}, {"%m-%Y", FALSE}, {"%Y-%e", FALSE}, {"%G-%m", FALSE},
            {"%Y-%V", FALSE}, {"%s-%Y", FALSE}, {"%F-%m", FALSE}, {"", FALSE},
            {0, 0}
        };
        for(int i = 0; masks[i].mask; i++) {
            char what[64];
            snprintf(what, sizeof(what), "mask '%s'", masks[i].mask);
            result += expect_int(what, tranger2_mask_sorts_as_time(masks[i].mask), masks[i].sorts);
        }
    }
    if(!create_topic(tm, TOPIC_DMY, "%d-%m-%Y")) {
        result += -1;
    }
    append_one(tm, TOPIC_DMY, 1, DAY1, "M1");
    result += expect_int("a mask with the year last",
        tranger2_delete_key_files(tm, TOPIC_DMY, KEY, DAY4), -1);
    {
        /*  The topic as a treedb on the tranger names it  */
        json_t *treedbs = json_object_get(tm, "treedbs");
        BOOL made = FALSE;
        if(!treedbs) {
            treedbs = json_object();
            json_object_set_new(tm, "treedbs", treedbs);
            made = TRUE;
        }
        json_object_set_new(treedbs, "treedb_fake", json_pack("{s:{}}", TOPIC_NAME));
        result += expect_int("a topic of a treedb",
            tranger2_delete_key_files(tm, TOPIC_NAME, KEY4, DAY4), -1);
        json_object_del(treedbs, "treedb_fake");
        if(made) {
            json_object_del(tm, "treedbs");
        }
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  A delete of a key purged whole that
     *  fails keeps the key, and its base
     *-------------------------------------*/
    if(geteuid() != 0) {
        set_expected_results(
            "delete key files: a failed delete of a key purged whole",
            json_pack("[{s:s},{s:s}]",
                "msg", "Cannot delete key, rename() of its directory out of keys/ FAILED: the key stays as it was",
                "msg", "Cannot delete subdir key: its directory cannot be moved out of keys/, the key stays as it was"
            ),
            NULL, NULL, 1
        );
        result += expect_int("purged whole again", tranger2_delete_key_files(tm, TOPIC_NAME, KEY, DAY4 + 86400), 1);
        char key_dir[PATH_MAX];
        build_path(key_dir, sizeof(key_dir), path_database, TOPIC_NAME, "keys", KEY, NULL);
        chmod(key_dir, 0550);
        result += expect_int("the delete fails", tranger2_delete_key(tm, TOPIC_NAME, KEY), -1);
        chmod(key_dir, 0770);
        result += expect_int("the key goes on numbering", append_one(tm, TOPIC_NAME, 1, DAY4 + 86400 + 10, "H1"), 13);
        result += test_json(NULL);
    }

    /*-------------------------------------*
     *  A delete of a key with a base and
     *  files that cannot move it out of
     *  keys/ leaves it whole
     *-------------------------------------*/
    if(geteuid() != 0) {
        set_expected_results(
            "delete key files: a failed delete of a key with a base",
            json_pack("[{s:s},{s:s}]",
                "msg", "Cannot delete key, rename() of its directory out of keys/ FAILED: the key stays as it was",
                "msg", "Cannot delete subdir key: its directory cannot be moved out of keys/, the key stays as it was"
            ),
            NULL, NULL, 1
        );
        char keys_dir[PATH_MAX];
        build_path(keys_dir, sizeof(keys_dir), path_database, TOPIC_NAME, "keys", NULL);
        struct stat st;
        stat(keys_dir, &st);
        json_int_t rows_before = (json_int_t)tranger2_topic_key_size(tm, TOPIC_NAME, KEY2);
        chmod(keys_dir, 0550);
        result += expect_int("the delete fails", tranger2_delete_key(tm, TOPIC_NAME, KEY2), -1);
        chmod(keys_dir, st.st_mode & 07777);
        result += expect_int("every file of it stays",
            (json_int_t)tranger2_topic_key_size(tm, TOPIC_NAME, KEY2), rows_before);
        result += expect_int("with its base", key_file_exists(KEY2, ".purged"), 1);
        result += expect_int("and goes on numbering", append_one(tm, TOPIC_NAME, 2, DAY4 + 20, "T1"), 8);
        result += test_json(NULL);
    }

    /*-------------------------------------*
     *  tranger2_delete_key() takes the
     *  base with the key
     *-------------------------------------*/
    set_expected_results("delete key files: delete the key", NULL, NULL, NULL, 1);
    result += expect_int("delete key", tranger2_delete_key(tm, TOPIC_NAME, KEY), 0);
    result += expect_int("a key written again starts at 1", append_one(tm, TOPIC_NAME, 1, DAY4, "G1"), 1);
    result += expect_int("with no base", key_file_exists(KEY, ".purged"), 0);
    {
        /*  The directory a delete of a key with a base moves aside is gone  */
        char topic_dir[PATH_MAX];
        build_path(topic_dir, sizeof(topic_dir), path_database, TOPIC_NAME, NULL);
        DIR *dir = opendir(topic_dir);
        int left = 0;
        struct dirent *entry;
        while(dir && (entry = readdir(dir)) != NULL) {
            if(strncmp(entry->d_name, ".deleted_key.", 13) == 0) {
                left++;
            }
        }
        if(dir) {
            closedir(dir);
        }
        result += expect_int("no directory left aside", left, 0);
    }
    result += test_json(NULL);

    set_expected_results("delete key files: shutdown", NULL, NULL, NULL, 1);
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
