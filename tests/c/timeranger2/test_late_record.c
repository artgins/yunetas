/****************************************************************************
 *          test_late_record.c
 *
 *  A LATE record: one whose __t__ is below the times already in its md2
 *  file. Two things went wrong with it before 7.25.4:
 *
 *      - a cell's time range was rebuilt from the FIRST and LAST md2
 *        rows only, and the late record is the last row with a lower `t`.
 *        The follower (at once) and the master (after a reload) put the
 *        cell's `to_t` below its real maximum, and a time-range query
 *        skipped the file: records inside the range were not served.
 *      - a follower with TWO disk feeds on one key kept ONE watermark
 *        per (feed, key). A batch that touched two files of the key reseeded
 *        the second feed's mark on the second file before it had read the
 *        first, and that feed lost the records of BOTH files for ever.
 *
 *  And the scan of a file that holds one. In 7.25.4,
 *  tranger2_match_metadata() ended a forward scan at the first row past
 *  to_t and a backward one at the first row below from_t, so the rows
 *  after a late row were lost in BOTH directions, and every paged iterator
 *  with a t filter lost them too (its index is built walking forward).
 *  `tm` is never marked and needs no late record to be out of order: a tm
 *  condition skips a row, it never ends a scan, neither inside a file nor
 *  across the files (they are cut by t, not by tm).
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

#define APP "test_late_record"

#define DATABASE    "tr_late_record"
#define TOPIC_NAME  "topic_late_record"
#define KEY         "0000000000000000001"
#define DAY1        946684800   // 2000-01-01
#define DAY2        (DAY1 + 86400)
#define KEY_TM      "0000000000000000002"   // t in order, tm not
#define KEY_TM2     "0000000000000000003"   // two files, the later one with the lower tm

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char got_a[256] = "";
PRIVATE char got_b[256] = "";

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int expect(const char *what, const char *got, const char *expected);

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

PRIVATE int append_tm(json_t *tranger, json_int_t id, uint64_t t, uint64_t tm, const char *content)
{
    json_t *record = json_pack("{s:I, s:I, s:s}",
        "id", id,
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

/*
 *  What an iterator of `key` serves with `cond` (owned), as "<content> ...".
 *  `paged`: opened with no data and no callback, read with one page, the
 *  way a client pages it (the index path).
 */
PRIVATE void served_cond(
    json_t *tranger,
    const char *key,
    json_t *cond,
    BOOL paged,
    char *bf,
    size_t bfsize
)
{
    bf[0] = 0;
    BOOL backward = json_is_true(json_object_get(cond, "backward"));
    json_t *data = paged? NULL: json_array();
    json_t *it = tranger2_open_iterator(
        tranger, TOPIC_NAME, key, cond, NULL, "cond", "", data, NULL
    );
    json_t *page = NULL;
    if(paged && it) {
        page = tranger2_iterator_get_page(tranger, it, 1, 100, backward);
        data = json_incref(json_object_get(page, "data"));
    }
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
    JSON_DECREF(page)
    JSON_DECREF(data)
}

/*
 *  The iterator and the paged iterator, both directions, of a t range
 */
PRIVATE int expect_t_range(
    json_t *tranger,
    const char *who,
    uint64_t from_t,
    uint64_t to_t,
    const char *forward,
    const char *backward
)
{
    int result = 0;
    char bf[256];
    char what[128];
    for(int paged = 0; paged < 2; paged++) {
        for(int bwd = 0; bwd < 2; bwd++) {
            served_cond(tranger, KEY,
                json_pack("{s:I, s:I, s:b}",
                    "from_t", (json_int_t)from_t,
                    "to_t", (json_int_t)to_t,
                    "backward", bwd
                ),
                paged, bf, sizeof(bf)
            );
            snprintf(what, sizeof(what), "%s %s%s [+%ld, +%ld]",
                who, paged? "paged ": "", bwd? "backward": "forward",
                (long)(from_t - DAY1), (long)(to_t - DAY1)
            );
            result += expect(what, bf, bwd? backward: forward);
        }
    }
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
     *  The time range: day 1 gets +100
     *  and +50000, then a LATE +200
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
     *  The scan of the marked file,
     *  both directions, iterator and pages
     *-------------------------------------*/
    set_expected_results("late record: the scan of a marked file", NULL, NULL, NULL, 1);
    result += expect_t_range(tm, "master", DAY1 + 150, DAY1 + 250, "E3", "E3");
    result += expect_t_range(tm, "master", DAY1 + 50, DAY1 + 300, "E1 E3", "E3 E1");
    result += expect_t_range(tf, "follower", DAY1 + 150, DAY1 + 250, "E3", "E3");
    result += test_json(NULL);

    /*-------------------------------------*
     *  The marked file goes on growing: the
     *  follower reads only the rows after
     *  the ones its cell counted (N12), and
     *  the range it keeps is the union.
     *-------------------------------------*/
    set_expected_results("late record: a marked file keeps growing", NULL, NULL, NULL, 1);
    /*
     *  To SEE that the rows already counted are not read again, the tm of
     *  a MIDDLE row (E2, row 2: a load always reads the first and the last
     *  row) is lowered on disk behind the follower's back: a follower that
     *  reads the file whole takes it into its cell's fr_tm, one that reads
     *  only the new rows does not.
     */
    off_t tm_offset = (off_t)(32 + sizeof(uint64_t));   // row 2, its __tm__
    char path_md2[PATH_MAX];
    build_path(path_md2, sizeof(path_md2),
        path_database, TOPIC_NAME, "keys", KEY, "2000-01-01.md2", NULL);
    uint64_t be_tm;
    int fd = open(path_md2, O_RDWR|O_CLOEXEC);
    if(fd < 0 || pread(fd, &be_tm, sizeof(be_tm), tm_offset) != sizeof(be_tm)) {
        printf("%sERROR%s --> cannot read %s\n", On_Red BWhite, Color_Off, path_md2);
        result += -1;
    } else {
        uint64_t raw = ntohll(be_tm);
        raw = (raw & ~0x00000FFFFFFFFFFFULL) | (uint64_t)(DAY1 + 1);
        be_tm = htonll(raw);
        if(pwrite(fd, &be_tm, sizeof(be_tm), tm_offset) != sizeof(be_tm)) {
            printf("%sERROR%s --> cannot write %s\n", On_Red BWhite, Color_Off, path_md2);
            result += -1;
        }
    }
    if(fd >= 0) {
        close(fd);
    }
    drain(10);

    append_one(tm, DAY1 + 70000, "E4");
    drain(30);
    served_in(tf, DAY1 + 60000, DAY1 + 80000, bf, sizeof(bf));
    result += expect("follower serves [+60000, +80000]", bf, "E4");
    served_in(tf, DAY1 + 40000, DAY1 + 60000, bf, sizeof(bf));
    result += expect("follower still serves [+40000, +60000]", bf, "E2");
    result += expect_t_range(tf, "follower", DAY1 + 40000, DAY1 + 80000, "E2 E4", "E4 E2");
    result += expect_t_range(tf, "follower", DAY1 + 150, DAY1 + 250, "E3", "E3");

    json_int_t cell_fr_tm = -1;
    json_t *cells = kw_get_list(0, tf, "topics`" TOPIC_NAME "`cache`" KEY "`files", 0, 0);
    int idx_cell; json_t *cell;
    json_array_foreach(cells, idx_cell, cell) {
        if(strcmp(kw_get_str(0, cell, "id", "", 0), "2000-01-01")==0) {
            cell_fr_tm = kw_get_int(0, cell, "fr_tm", -1, 0);
        }
    }
    if(cell_fr_tm != DAY1 + 100) {
        printf("%sERROR%s --> the follower read the rows it had counted again: fr_tm %ld, expected %ld\n",
            On_Red BWhite, Color_Off, (long)cell_fr_tm, (long)(DAY1 + 100));
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Two feeds: ONE batch touches two
     *  files of the key, a late day-1
     *  record and a day-2 one. Both feeds
     *  get both.
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
     *  A reload says the same
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
    {
        /*  The whole read of a reload DOES see the lowered tm: the check of
         *  the follower's partial read above can tell the two apart.  */
        json_int_t fr_tm = -1;
        json_t *cells2 = kw_get_list(0, tm, "topics`" TOPIC_NAME "`cache`" KEY "`files", 0, 0);
        int idx2; json_t *cell2;
        json_array_foreach(cells2, idx2, cell2) {
            if(strcmp(kw_get_str(0, cell2, "id", "", 0), "2000-01-01")==0) {
                fr_tm = kw_get_int(0, cell2, "fr_tm", -1, 0);
            }
        }
        if(fr_tm != DAY1 + 1) {
            printf("%sERROR%s --> a reload did not read the marked file whole: fr_tm %ld\n",
                On_Red BWhite, Color_Off, (long)fr_tm);
            result += -1;
        }
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  tm is not ordered, and no tm
     *  condition ends a scan
     *-------------------------------------*/
    set_expected_results("late record: tm out of order", NULL, NULL, NULL, 1);
    append_tm(tm, 2, DAY1 + 1000, 500, "T1");
    append_tm(tm, 2, DAY1 + 1001, 100, "T2");
    append_tm(tm, 2, DAY1 + 1002, 300, "T3");
    append_tm(tm, 3, DAY1 + 1000, 1500, "U1");
    append_tm(tm, 3, DAY2 + 1000, 150, "U2");
    struct {
        const char *key;
        json_int_t from_tm;
        json_int_t to_tm;
        const char *forward;
        const char *backward;
    } tm_cases[] = {
        {KEY_TM,    250,    600,    "T1 T3",    "T3 T1"},
        {KEY_TM,    0,      400,    "T2 T3",    "T3 T2"},
        {KEY_TM2,   0,      300,    "U2",       "U2"},
        {KEY_TM2,   1000,   0,      "U1",       "U1"},
        {0}
    };
    for(int i = 0; tm_cases[i].key; i++) {
        for(int paged = 0; paged < 2; paged++) {
            for(int bwd = 0; bwd < 2; bwd++) {
                json_t *cond = json_pack("{s:b}", "backward", bwd);
                if(tm_cases[i].from_tm) {
                    json_object_set_new(cond, "from_tm", json_integer(tm_cases[i].from_tm));
                }
                if(tm_cases[i].to_tm) {
                    json_object_set_new(cond, "to_tm", json_integer(tm_cases[i].to_tm));
                }
                served_cond(tm, tm_cases[i].key, cond, paged, bf, sizeof(bf));
                char what[128];
                snprintf(what, sizeof(what), "key %s %s%s tm [%ld, %ld]",
                    tm_cases[i].key + 18, paged? "paged ": "", bwd? "backward": "forward",
                    (long)tm_cases[i].from_tm, (long)tm_cases[i].to_tm
                );
                result += expect(what, bf, bwd? tm_cases[i].backward: tm_cases[i].forward);
            }
        }
    }
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
