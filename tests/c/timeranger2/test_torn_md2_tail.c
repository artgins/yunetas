/****************************************************************************
 *          test_torn_md2_tail.c
 *
 *  A md2 file whose size is not a whole number of rows: its last row is
 *  torn. A power cut during the 32-byte write of a md2 row leaves this
 *  shape. The md2 row is the commit point of an append (the content is
 *  written first, the row after), so a torn last row is an append that was
 *  never acknowledged. It is not damage.
 *
 *  Since 7.25.4 (unreleased work) it was taken for damage: the key was
 *  flagged, every load of it said load_failed, and every append into the
 *  file was refused until an operator cut the md2 by hand or the period
 *  changed. With "filename_mask": "%Y" that is months.
 *
 *  The cases:
 *      1. a MASTER opens a store whose last md2 of a key has 3 whole
 *         rows and 13 bytes more: the md2 is cut back to the 3 rows, with
 *         ONE warning. The key loads whole, the next append goes into the
 *         same file and is read back, and the md2 size is a whole number
 *         of rows again. A restart finds nothing to say.
 *      2. a REPLICA opens the same damaged store: it writes nothing (the
 *         md2 keeps its 13 bytes), it reads the 3 whole rows, and it logs
 *         nothing. A replica can see a row of a live master half written.
 *         Then a master opens the store and cuts the md2 back (case 1).
 *      3. a md2 of 13 bytes only (its first row is torn): the master cuts
 *         it to 0 bytes, and a md2 of 0 rows beside a content file that is
 *         not empty is ignored with its own warning (test_uncommitted_
 *         append.c). The next append into the file is its row 1.
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

#define APP         "test_torn_md2_tail"
#define DATABASE    "tr_torn_md2_tail"
#define TOPIC_NAME  "topic_torn"
#define DAY1        946684800   // 2000-01-01
#define DAY         86400
#define ROW         32          // sizeof(md2_record_t)
#define TORN        13          // the bytes of the torn row

#define MSG_CUT         "md2 file of the key ends in a part of a row: an append that was never acknowledged was cut back"
#define MSG_NO_ROWS     "md2 file of the key with no rows and a content file that is not empty: an append that was never acknowledged, the file is ignored"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char path_root[PATH_MAX];
PRIVATE char path_database[PATH_MAX];
PRIVATE char got[512];

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
    size_t ln = strlen(got);
    if(!record) {
        snprintf(got + ln, sizeof(got) - ln, "%s%s@NULL", ln? " ": "", key);
    } else {
        snprintf(got + ln, sizeof(got) - ln, "%s%s@%d", ln? " ": "", key,
            (int)kw_get_int(0, record, "v", 0, 0));
    }
    JSON_DECREF(record)
    return 0;
}

PRIVATE json_t *startup(BOOL master)
{
    return tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i, s:s}",
        "path", path_root,
        "database", DATABASE,
        "master", master,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y-%m-%d"
    ), 0);
}

PRIVATE json_t *create_topic(json_t *tranger)
{
    return tranger2_create_topic(
        tranger, TOPIC_NAME, "id", "tm", NULL, sf_string_key,
        json_pack("{s:s, s:I, s:I}", "id", "", "tm", (json_int_t)0, "v", (json_int_t)0),
        0
    );
}

PRIVATE int append(json_t *tranger, const char *key, int day, int v)
{
    md2_record_ex_t md = {0};
    return tranger2_append_record(tranger, TOPIC_NAME, (uint64_t)(DAY1 + day*DAY + v), 0, &md,
        json_pack("{s:s, s:I, s:i}", "id", key, "tm", (json_int_t)(DAY1 + day*DAY + v), "v", v)
    );
}

/*
 *  A store with A: row 1 in the file of day 0 and rows 2, 3, 4 in the
 *  file of day 1, and B: row 1
 */
PRIVATE int build_store(void)
{
    rmrdir(path_database);
    set_expected_results("torn md2 tail: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup(TRUE);
    if(!tranger || !create_topic(tranger)) {
        printf("%sERROR%s --> cannot create the store\n", On_Red BWhite, Color_Off);
        return -1;
    }
    append(tranger, "A", 0, 1);
    append(tranger, "A", 1, 2);
    append(tranger, "A", 1, 3);
    append(tranger, "A", 1, 4);
    append(tranger, "B", 0, 1);
    tranger2_shutdown(tranger);
    test_json(NULL);    // the setup logs are not what is tested
    return 0;
}

PRIVATE void file_of_a(char *bf, size_t bfsize, const char *day, const char *ext)
{
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "%s.%s", day, ext);
    build_path(bf, bfsize, path_database, TOPIC_NAME, "keys", "A", name, NULL);
}

/*
 *  The md2 of A's file of `day` with `size` bytes, as a power cut leaves
 *  it, with no tranger running
 */
PRIVATE int tear_md2(const char *day, off_t size)
{
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), day, "md2");
    if(truncate(path, size) < 0) {
        printf("%sERROR%s --> cannot tear %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }
    return 0;
}

PRIVATE off_t md2_size(const char *day)
{
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), day, "md2");
    return filesize(path);
}

PRIVATE int expect(const char *what, const char *found, const char *expected)
{
    if(strcmp(found, expected) != 0) {
        printf("%sERROR%s --> %s: [%s], expected [%s]\n",
            On_Red BWhite, Color_Off, what, found, expected);
        return -1;
    }
    return 0;
}

PRIVATE int expect_size(const char *what, off_t found, off_t expected)
{
    if(found != expected) {
        printf("%sERROR%s --> %s: md2 size %ld, expected %ld\n",
            On_Red BWhite, Color_Off, what, (long)found, (long)expected);
        return -1;
    }
    return 0;
}

/*
 *  The keyless list forward: what it hands, and that it is whole
 */
PRIVATE int check_list(json_t *tranger, const char *what, const char *expected)
{
    int result = 0;
    got[0] = 0;
    json_t *match_cond = json_pack("{s:I, s:b, s:I}",
        "to_rowid", (json_int_t)1000000,   // no realtime
        "backward", 0,
        "load_record_callback", (json_int_t)(uintptr_t)on_record
    );
    json_t *list = tranger2_open_list(tranger, TOPIC_NAME, match_cond, json_object(), "", FALSE, "");
    if(!list) {
        printf("%sERROR%s --> %s: list REFUSED\n", On_Red BWhite, Color_Off, what);
        return -1;
    }
    result += expect(what, got, expected);
    if(json_is_true(json_object_get(list, "load_failed"))) {
        printf("%sERROR%s --> %s: load_failed\n", On_Red BWhite, Color_Off, what);
        result += -1;
    }
    tranger2_close_list(tranger, list);
    return result;
}

PRIVATE int check_rows(json_t *tranger, const char *what, int expected)
{
    json_t *range = tranger2_topic_key_range(tranger, TOPIC_NAME, "A");
    int rows = (int)kw_get_int(0, range, "rows", 0, 0);
    JSON_DECREF(range)
    if(rows != expected) {
        printf("%sERROR%s --> %s: A has %d rows, expected %d\n",
            On_Red BWhite, Color_Off, what, rows, expected);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  1. A master cuts a torn tail back
 ***************************************************************************/
PRIVATE int test_master_cuts(void)
{
    int result = 0;
    if(build_store() < 0 || tear_md2("2000-01-02", 3*ROW + TORN) < 0) {
        return -1;
    }

    set_expected_results("1. a master opens a torn md2: one warning",
        json_pack("[{s:s, s:s, s:s, s:i, s:i}]",
            "msg", MSG_CUT,
            "key", "A",
            "file_id", "2000-01-02",
            "old_size", 3*ROW + TORN,
            "new_size", 3*ROW
        ), NULL, NULL, 1
    );
    json_t *tranger = startup(TRUE);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> 1: cannot open the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);
    result += expect_size("1. after the open", md2_size("2000-01-02"), 3*ROW);

    set_expected_results("1. the key loads whole, the next append goes in", NULL, NULL, NULL, 1);
    result += check_list(tranger, "1. the list after the cut", "A@1 A@2 A@3 A@4 B@1");
    result += check_rows(tranger, "1. the range after the cut", 4);
    if(append(tranger, "A", 1, 5) < 0) {
        printf("%sERROR%s --> 1: the append into the cut file was refused\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_size("1. after the append", md2_size("2000-01-02"), 4*ROW);
    result += check_list(tranger, "1. the list after the append", "A@1 A@2 A@3 A@4 A@5 B@1");
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    set_expected_results("1. the restart: nothing to say", NULL, NULL, NULL, 1);
    tranger = startup(TRUE);
    tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    result += check_list(tranger, "1. the list after the restart", "A@1 A@2 A@3 A@4 A@5 B@1");
    result += check_rows(tranger, "1. the range after the restart", 5);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  2. A replica reads the whole rows and writes nothing
 ***************************************************************************/
PRIVATE int test_replica_reads(void)
{
    int result = 0;
    if(build_store() < 0 || tear_md2("2000-01-02", 3*ROW + TORN) < 0) {
        return -1;
    }

    set_expected_results("2. a replica opens a torn md2: nothing to say", NULL, NULL, NULL, 1);
    json_t *tranger = startup(FALSE);
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> 2: cannot open the store as a replica\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += expect_size("2. the replica writes nothing", md2_size("2000-01-02"), 3*ROW + TORN);
    result += check_list(tranger, "2. the replica's list", "A@1 A@2 A@3 A@4 B@1");
    result += check_rows(tranger, "2. the replica's range", 4);
    tranger2_shutdown(tranger);
    result += expect_size("2. after the replica", md2_size("2000-01-02"), 3*ROW + TORN);
    result += test_json(NULL);

    set_expected_results("2. then a master cuts it back",
        json_pack("[{s:s}]", "msg", MSG_CUT), NULL, NULL, 1
    );
    tranger = startup(TRUE);
    tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    result += expect_size("2. after the master", md2_size("2000-01-02"), 3*ROW);
    result += check_list(tranger, "2. the master's list", "A@1 A@2 A@3 A@4 B@1");
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  3. The first row torn: the md2 is cut to 0 bytes, and ignored
 ***************************************************************************/
PRIVATE int test_first_row_torn(void)
{
    int result = 0;
    if(build_store() < 0 || tear_md2("2000-01-02", TORN) < 0) {
        return -1;
    }

    set_expected_results("3. the only row torn: cut to 0 bytes, then ignored",
        json_pack("[{s:s, s:s, s:i, s:i},{s:s, s:s}]",
            "msg", MSG_CUT,
            "file_id", "2000-01-02",
            "old_size", TORN,
            "new_size", 0,
            "msg", MSG_NO_ROWS,
            "file_id", "2000-01-02"
        ), NULL, NULL, 1
    );
    json_t *tranger = startup(TRUE);
    tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    result += test_json(NULL);
    result += expect_size("3. after the open", md2_size("2000-01-02"), 0);

    set_expected_results("3. the next append is the file's row 1", NULL, NULL, NULL, 1);
    result += check_list(tranger, "3. the list after the cut", "A@1 B@1");
    if(append(tranger, "A", 1, 6) < 0) {
        printf("%sERROR%s --> 3: the append into the cut file was refused\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_size("3. after the append", md2_size("2000-01-02"), ROW);
    result += check_list(tranger, "3. the list after the append", "A@1 A@6 B@1");
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);

    result += test_master_cuts();
    result += test_replica_reads();
    result += test_first_row_torn();

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
