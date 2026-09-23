/****************************************************************************
 *          test_md2_short_write.c
 *
 *  The md2 row of an append written in part, and the cut back of that
 *  part failing. The md2 row is the commit point of an append: an append
 *  whose row is not written whole is refused, its content is cut back, and
 *  its part of a row is cut back too. When that cut fails, the md2 ends in
 *  a part of a row (test_torn_md2_tail.c, case 4, makes the same shape by
 *  hand).
 *
 *  No file mode makes a write short or an ftruncate() fail on a file that
 *  is open, so the test links with `-Wl,--wrap=write,--wrap=ftruncate`
 *  (see CMakeLists.txt): __wrap_write() writes only TORN bytes of the next
 *  md2 row, and __wrap_ftruncate() fails the next cuts of a md2 file. Every
 *  other call passes through. It works as root too.
 *
 *  Key A has 1 row in 2000-01-01 and 3 rows in 2000-01-02, key B one row.
 *
 *      1. an append into A's 2000-01-02 writes 13 bytes of its md2 row, and
 *         the cut of them fails: an ERROR says the md2 is not a whole
 *         number of rows, a CRITICAL says the write was short, the append
 *         is refused, and its content is cut back. The next append cuts
 *         the 13 bytes back first (the warning of test_torn_md2_tail.c) and
 *         writes its row on the row boundary: every row is read, also after
 *         a restart.
 *      2. the same, and the cut of the next append fails too: that append
 *         is refused with a CRITICAL, its content is cut back, and the md2
 *         keeps its size. The append after it cuts back and goes in.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
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

#define APP         "test_md2_short_write"
#define DATABASE    "tr_md2_short_write"
#define TOPIC_NAME  "topic_short_write"
#define DAY1        946684800   // 2000-01-01
#define DAY         86400
#define ROW         32          // sizeof(md2_record_t)
#define TORN        13          // the bytes of the short write

#define MSG_CUT_FAILED  "Cannot cut back the md2 of an append whose row was not written whole: its size is not a whole number of rows"
#define MSG_SHORT       "Cannot save record metadata, short write: the file size limit or the disk is full"
#define MSG_CUT         "md2 file of the key ends in a part of a row: an append that was never acknowledged was cut back"
#define MSG_NOT_CUT     "Cannot append record, its md2 file ends in a part of a row that cannot be cut back: the append is refused"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char path_root[PATH_MAX];
PRIVATE char path_database[PATH_MAX];
PRIVATE char got[512];
PRIVATE int short_md2_writes = 0;       // the next md2 writes write TORN bytes only
PRIVATE int failed_md2_ftruncates = 0;  // the next ftruncate() calls of a md2 fail

/***************************************************************
 *              The write and the ftruncate that fail
 ***************************************************************/
ssize_t __real_write(int fd, const void *buf, size_t count);
ssize_t __wrap_write(int fd, const void *buf, size_t count);
int __real_ftruncate(int fd, off_t length);
int __wrap_ftruncate(int fd, off_t length);

PRIVATE BOOL is_md2_file(int fd)
{
    char link[64];
    char target[PATH_MAX];
    snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    ssize_t ln = readlink(link, target, sizeof(target) - 1);
    if(ln <= 0) {
        return FALSE;
    }
    target[ln] = 0;
    return ln > 4 && strcmp(target + ln - 4, ".md2") == 0;
}

ssize_t __wrap_write(int fd, const void *buf, size_t count)
{
    if(short_md2_writes > 0 && count == ROW && is_md2_file(fd)) {
        short_md2_writes--;
        return __real_write(fd, buf, TORN);
    }
    return __real_write(fd, buf, count);
}

int __wrap_ftruncate(int fd, off_t length)
{
    if(failed_md2_ftruncates > 0 && is_md2_file(fd)) {
        failed_md2_ftruncates--;
        errno = EIO;
        return -1;
    }
    return __real_ftruncate(fd, length);
}

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

PRIVATE int append(json_t *tranger, const char *key, int day, int v)
{
    md2_record_ex_t md = {0};
    return tranger2_append_record(tranger, TOPIC_NAME, (uint64_t)(DAY1 + day*DAY + v), 0, &md,
        json_pack("{s:s, s:I, s:i}", "id", key, "tm", (json_int_t)(DAY1 + day*DAY + v), "v", v)
    );
}

PRIVATE int build_store(void)
{
    rmrdir(path_database);
    set_expected_results("md2 short write: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    if(!tranger || !tranger2_create_topic(
            tranger, TOPIC_NAME, "id", "tm", NULL, sf_string_key,
            json_pack("{s:s, s:I, s:I}", "id", "", "tm", (json_int_t)0, "v", (json_int_t)0),
            0)) {
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

PRIVATE off_t size_of_a(const char *ext)
{
    char name[NAME_MAX];
    char path[PATH_MAX];
    snprintf(name, sizeof(name), "2000-01-02.%s", ext);
    build_path(path, sizeof(path), path_database, TOPIC_NAME, "keys", "A", name, NULL);
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

PRIVATE int expect_size(const char *what, const char *ext, off_t expected)
{
    off_t found = size_of_a(ext);
    if(found != expected) {
        printf("%sERROR%s --> %s: %s size %ld, expected %ld\n",
            On_Red BWhite, Color_Off, what, ext, (long)found, (long)expected);
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

/*
 *  The append of v 5 into A's 2000-01-02: 13 bytes of its md2 row are
 *  written, and their cut back fails
 */
PRIVATE int short_write_not_cut(json_t *tranger, const char *case_name)
{
    int result = 0;
    char test[128];
    off_t content_size = size_of_a("json");

    snprintf(test, sizeof(test), "%s: a short md2 write whose cut back fails", case_name);
    set_expected_results(test,
        json_pack("[{s:s, s:s, s:s, s:i},{s:s, s:i, s:i}]",
            "msg", MSG_CUT_FAILED,
            "key", "A",
            "file_id", "2000-01-02",
            "errno", EIO,
            "msg", MSG_SHORT,
            "written", TORN,
            "expected", ROW
        ), NULL, NULL, 1
    );
    short_md2_writes = 1;
    failed_md2_ftruncates = 1;
    if(append(tranger, "A", 1, 5) == 0) {
        printf("%sERROR%s --> %s: the append was taken\n", On_Red BWhite, Color_Off, case_name);
        result += -1;
    }
    short_md2_writes = 0;
    failed_md2_ftruncates = 0;
    result += test_json(NULL);

    snprintf(test, sizeof(test), "%s: after the short write", case_name);
    result += expect_size(test, "md2", 3*ROW + TORN);
    result += expect_size(test, "json", content_size);
    return result;
}

/***************************************************************************
 *  1. A short md2 write whose cut back fails: the next append cuts
 ***************************************************************************/
PRIVATE int test_short_write(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }
    set_expected_results("1. a master", NULL, NULL, NULL, 1);
    json_t *tranger = startup();
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> 1: cannot open the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);

    result += short_write_not_cut(tranger, "1");

    set_expected_results("1. the next append cuts back first",
        json_pack("[{s:s, s:s, s:s, s:i, s:i}]",
            "msg", MSG_CUT,
            "key", "A",
            "file_id", "2000-01-02",
            "old_size", 3*ROW + TORN,
            "new_size", 3*ROW
        ), NULL, NULL, 1
    );
    if(append(tranger, "A", 1, 6) < 0) {
        printf("%sERROR%s --> 1: the next append was refused\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("1. every row is read", NULL, NULL, NULL, 1);
    result += expect_size("1. after the next append", "md2", 4*ROW);
    result += check_list(tranger, "1. the list", "A@1 A@2 A@3 A@4 A@6 B@1");
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    set_expected_results("1. the restart: nothing to say", NULL, NULL, NULL, 1);
    tranger = startup();
    tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    result += expect_size("1. after the restart", "md2", 4*ROW);
    result += check_list(tranger, "1. the list after the restart", "A@1 A@2 A@3 A@4 A@6 B@1");
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  2. The cut of the next append fails too: that append is refused
 ***************************************************************************/
PRIVATE int test_append_cut_fails(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }
    set_expected_results("2. a master", NULL, NULL, NULL, 1);
    json_t *tranger = startup();
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> 2: cannot open the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);

    result += short_write_not_cut(tranger, "2");
    off_t content_size = size_of_a("json");

    set_expected_results("2. the cut of the next append fails: it is refused",
        json_pack("[{s:s, s:s, s:s, s:i, s:i, s:i}]",
            "msg", MSG_NOT_CUT,
            "key", "A",
            "file_id", "2000-01-02",
            "old_size", 3*ROW + TORN,
            "new_size", 3*ROW,
            "errno", EIO
        ), NULL, NULL, 1
    );
    failed_md2_ftruncates = 1;
    if(append(tranger, "A", 1, 6) == 0) {
        printf("%sERROR%s --> 2: the append was taken\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    failed_md2_ftruncates = 0;
    result += test_json(NULL);
    result += expect_size("2. after the refused append", "md2", 3*ROW + TORN);
    result += expect_size("2. after the refused append", "json", content_size);

    set_expected_results("2. the append after it cuts back",
        json_pack("[{s:s, s:i, s:i}]",
            "msg", MSG_CUT,
            "old_size", 3*ROW + TORN,
            "new_size", 3*ROW
        ), NULL, NULL, 1
    );
    if(append(tranger, "A", 1, 7) < 0) {
        printf("%sERROR%s --> 2: the append after it was refused\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("2. every row is read", NULL, NULL, NULL, 1);
    result += expect_size("2. at the end", "md2", 4*ROW);
    result += check_list(tranger, "2. the list", "A@1 A@2 A@3 A@4 A@7 B@1");
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

    result += test_short_write();
    result += test_append_cut_fails();

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
