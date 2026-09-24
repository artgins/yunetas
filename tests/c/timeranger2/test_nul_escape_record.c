/****************************************************************************
 *          test_nul_escape_record.c
 *
 *  A record with a string that holds NUL characters. An append writes it
 *  with json_dumps(), which writes each NUL of a string as the escape
 *  "\u0000": the content file has no NUL byte inside the record, only the
 *  one after it. Jansson reads "\u0000" only with JSON_ALLOW_NUL. In
 *  7.25.4 the read of a record did not give that flag: the append took
 *  such a record, and every read of it failed (a CRITICAL "Bad data,
 *  anystring2json() FAILED.") and handed the record to the callback as
 *  NULL. The check of a torn md2 tail, new after 7.25.4, gives the flag
 *  too: without it a torn row after such a record would be flagged, not
 *  cut.
 *
 *  What an append takes, a read reads back:
 *      1. a master appends records with NULs in a string: the list hands
 *         each one back, with the same string, and logs nothing. Also
 *         after a restart.
 *      2. the md2 torn after such records: the master cuts it back with
 *         the one warning of a torn row, and the key loads.
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

#define APP         "test_nul_escape_record"
#define DATABASE    "tr_nul_escape_record"
#define TOPIC_NAME  "topic_nul"
#define DAY1        946684800   // 2000-01-01
#define ROW         32          // sizeof(md2_record_t)
#define TORN        13          // the bytes of the torn row

#define MSG_CUT     "md2 file of the key ends in a part of a row: an append that was never acknowledged was cut back"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char path_root[PATH_MAX];
PRIVATE char path_database[PATH_MAX];
PRIVATE char got[512];
PRIVATE int not_equal = 0;  // records read back that are not what was appended

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE json_t *record_with_nul(int v)
{
    json_t *record = json_pack("{s:s, s:I, s:i}",
        "id", "A",
        "tm", (json_int_t)(DAY1 + v),
        "v", v
    );
    json_object_set_new(record, "bin", json_stringn("a\0b\0\0c", 6));
    return record;
}

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
        return 0;
    }
    int v = (int)kw_get_int(0, record, "v", 0, 0);
    snprintf(got + ln, sizeof(got) - ln, "%s%s@%d", ln? " ": "", key, v);
    json_t *expected = record_with_nul(v);
    if(!json_equal(json_object_get(record, "bin"), json_object_get(expected, "bin")) ||
            !json_equal(json_object_get(record, "tm"), json_object_get(expected, "tm"))) {
        not_equal++;
    }
    JSON_DECREF(expected)
    JSON_DECREF(record)
    return 0;
}

PRIVATE json_t *startup(void)
{
    return tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i, s:s}",
        "path", path_root,
        "database", DATABASE,
        "master", 1,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y-%m-%d"
    ), 0);
}

PRIVATE int append(json_t *tranger, int v)
{
    md2_record_ex_t md = {0};
    return tranger2_append_record(tranger, TOPIC_NAME, (uint64_t)(DAY1 + v), 0, &md,
        record_with_nul(v)
    );
}

PRIVATE off_t md2_size(void)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, TOPIC_NAME, "keys", "A", "2000-01-01.md2", NULL);
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

/*
 *  The keyless list forward: what it hands, that it is whole, and that
 *  each record is what was appended
 */
PRIVATE int check_list(json_t *tranger, const char *what, const char *expected)
{
    int result = 0;
    got[0] = 0;
    not_equal = 0;
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
    if(not_equal) {
        printf("%sERROR%s --> %s: %d records are not what was appended\n",
            On_Red BWhite, Color_Off, what, not_equal);
        result += -1;
    }
    tranger2_close_list(tranger, list);
    return result;
}

/*
 *  A store with 4 records of key A, each one with NULs in a string
 */
PRIVATE json_t *build_store(void)
{
    rmrdir(path_database);
    json_t *tranger = startup();
    if(!tranger || !tranger2_create_topic(
            tranger, TOPIC_NAME, "id", "tm", NULL, sf_string_key,
            json_pack("{s:s, s:I, s:I}", "id", "", "tm", (json_int_t)0, "v", (json_int_t)0),
            0)) {
        printf("%sERROR%s --> cannot create the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return NULL;
    }
    for(int v = 1; v <= 4; v++) {
        if(append(tranger, v) < 0) {
            printf("%sERROR%s --> the append of v %d was refused\n", On_Red BWhite, Color_Off, v);
        }
    }
    return tranger;
}

/***************************************************************************
 *  1. Records with NULs in a string are read back
 ***************************************************************************/
PRIVATE int test_read_back(void)
{
    int result = 0;

    set_expected_results("1. setup", NULL, NULL, NULL, 0);
    json_t *tranger = build_store();
    test_json(NULL);    // the setup logs are not what is tested
    if(!tranger) {
        return -1;
    }

    set_expected_results("1. the master that appended reads them", NULL, NULL, NULL, 1);
    result += check_list(tranger, "1. the list", "A@1 A@2 A@3 A@4");
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    set_expected_results("1. the restart: every record is read", NULL, NULL, NULL, 1);
    tranger = startup();
    tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    result += check_list(tranger, "1. the list after the restart", "A@1 A@2 A@3 A@4");
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  2. A torn md2 after records with NULs: the master cuts it
 ***************************************************************************/
PRIVATE int test_torn_after_nul(void)
{
    int result = 0;

    set_expected_results("2. setup", NULL, NULL, NULL, 0);
    json_t *tranger = build_store();
    if(tranger) {
        tranger2_shutdown(tranger);
    }
    test_json(NULL);    // the setup logs are not what is tested
    if(!tranger) {
        return -1;
    }

    /*
     *  The row of v 4 torn: its content is in the content file, 13 bytes
     *  of its row are in the md2
     */
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, TOPIC_NAME, "keys", "A", "2000-01-01.md2", NULL);
    if(truncate(path, 3*ROW + TORN) < 0) {
        printf("%sERROR%s --> cannot tear %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }

    set_expected_results("2. the master cuts the torn row back",
        json_pack("[{s:s, s:s, s:s, s:i, s:i}]",
            "msg", MSG_CUT,
            "key", "A",
            "file_id", "2000-01-01",
            "old_size", 3*ROW + TORN,
            "new_size", 3*ROW
        ), NULL, NULL, 1
    );
    tranger = startup();
    tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    result += test_json(NULL);

    set_expected_results("2. the key loads", NULL, NULL, NULL, 1);
    if(md2_size() != 3*ROW) {
        printf("%sERROR%s --> 2: md2 size %ld, expected %d\n",
            On_Red BWhite, Color_Off, (long)md2_size(), 3*ROW);
        result += -1;
    }
    result += check_list(tranger, "2. the list", "A@1 A@2 A@3");
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

    result += test_read_back();
    result += test_torn_after_nul();

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
