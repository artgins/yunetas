/****************************************************************************
 *          test_md2_read_error.c
 *
 *  A md2 whose first or last row cannot be read when the cache of its key
 *  is built. The size of the file says it has whole rows, so the rows
 *  exist: a key read without them is a SHORTER key. The file is flagged
 *  unreadable instead, and every load of the key says load_failed, as for
 *  a md2 that cannot be opened (test_unreadable_at_open.c).
 *
 *  No file mode makes a read fail after an open succeeds, so the test
 *  links with `-Wl,--wrap=pread` (see CMakeLists.txt): __wrap_pread()
 *  fails the read of ONE md2 file, in the way the case asks, and passes
 *  every other read through. It works as root too.
 *
 *  Key A has rows in three daily files: 1 row in 2000-01-01, 3 rows in
 *  2000-01-02, 1 row in 2000-01-03. Key B has one row. The fault is in
 *  A's 2000-01-02.
 *
 *      1. the read of the FIRST row fails (EIO): a CRITICAL names the row,
 *         the file is flagged, and the forward load says load_failed after
 *         A@1. Then the fault goes: an append into the file counts it again,
 *         unflags it, and the key loads whole.
 *      2. the read of the LAST row is short: the same.
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

#define APP         "test_md2_read_error"
#define DATABASE    "tr_md2_read_error"
#define TOPIC_NAME  "topic_read_error"
#define DAY1        946684800   // 2000-01-01
#define DAY         86400
#define ROW         32          // sizeof(md2_record_t)
#define FAULT_FILE  "/keys/A/2000-01-02.md2"

#define MSG_FLAG    "md2 file of the key unreadable when its cache was built: every load of the key says load_failed"
#define MSG_UNFLAG  "md2 file of the key readable again: it is counted, and the key is not flagged for it"
#define MSG_ITER    "The history of the key is not whole: a md2 file of it could not be read when its cache was built"
#define MSG_LIST    "Cannot load the whole history of a key of the list: the records read before the failure were handed, the list goes on with the next key"

/***************************************************************
 *              Data
 ***************************************************************/
typedef enum {
    FAULT_NONE = 0,
    FAULT_FIRST_ROW_EIO,
    FAULT_LAST_ROW_SHORT,
} fault_t;

PRIVATE yev_loop_h yev_loop;
PRIVATE char path_root[PATH_MAX];
PRIVATE char path_database[PATH_MAX];
PRIVATE char got[512];
PRIVATE fault_t fault = FAULT_NONE;

/***************************************************************
 *              The read that fails
 ***************************************************************/
ssize_t __real_pread(int fd, void *buf, size_t count, off_t offset);
ssize_t __wrap_pread(int fd, void *buf, size_t count, off_t offset);

PRIVATE BOOL is_fault_file(int fd)
{
    char link[64];
    char target[PATH_MAX];
    snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    ssize_t ln = readlink(link, target, sizeof(target) - 1);
    if(ln <= 0) {
        return FALSE;
    }
    target[ln] = 0;
    size_t suffix = strlen(FAULT_FILE);
    return (size_t)ln >= suffix && strcmp(target + ln - suffix, FAULT_FILE) == 0;
}

ssize_t __wrap_pread(int fd, void *buf, size_t count, off_t offset)
{
    if(fault != FAULT_NONE && is_fault_file(fd)) {
        if(fault == FAULT_FIRST_ROW_EIO && offset == 0) {
            errno = EIO;
            return -1;
        }
        if(fault == FAULT_LAST_ROW_SHORT && offset > 0) {
            return __real_pread(fd, buf, count/2, offset);
        }
    }
    return __real_pread(fd, buf, count, offset);
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
    set_expected_results("md2 read error: setup", NULL, NULL, NULL, 0);
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
    append(tranger, "A", 2, 5);
    append(tranger, "B", 0, 1);
    tranger2_shutdown(tranger);
    test_json(NULL);    // the setup logs are not what is tested
    return 0;
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
 *  The keyless list forward: what it hands, and whether it says load_failed
 */
PRIVATE int check_list(json_t *tranger, const char *what, const char *expected, BOOL expect_failed)
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
    BOOL failed = json_is_true(json_object_get(list, "load_failed"));
    if(failed != expect_failed) {
        printf("%sERROR%s --> %s: load_failed %d, expected %d\n",
            On_Red BWhite, Color_Off, what, failed, expect_failed);
        result += -1;
    }
    tranger2_close_list(tranger, list);
    return result;
}

/***************************************************************************
 *  One fault while the cache is built, then the fault goes
 ***************************************************************************/
PRIVATE int test_fault(const char *case_name, fault_t fault_, const char *msg, const char *row)
{
    int result = 0;
    char test[128];
    if(build_store() < 0) {
        return -1;
    }

    snprintf(test, sizeof(test), "%s: the file is flagged", case_name);
    set_expected_results(test,
        json_pack("[{s:s, s:s},{s:s, s:s, s:s}]",
            "msg", msg,
            "row", row,
            "msg", MSG_FLAG,
            "key", "A",
            "file_id", "2000-01-02"
        ), NULL, NULL, 1
    );
    fault = fault_;
    json_t *tranger = startup();
    json_t *topic = tranger? tranger2_open_topic(tranger, TOPIC_NAME, FALSE): NULL;
    fault = FAULT_NONE;
    if(!topic) {
        printf("%sERROR%s --> %s: cannot open the store\n", On_Red BWhite, Color_Off, case_name);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);

    snprintf(test, sizeof(test), "%s: the load says load_failed", case_name);
    set_expected_results(test,
        json_pack("[{s:s},{s:s}]",
            "msg", MSG_ITER,
            "msg", MSG_LIST
        ), NULL, NULL, 1
    );
    snprintf(test, sizeof(test), "%s: the list with the file flagged", case_name);
    result += check_list(tranger, test, "A@1 B@1", TRUE);
    result += test_json(NULL);

    snprintf(test, sizeof(test), "%s: the fault gone, an append counts the file again", case_name);
    set_expected_results(test,
        json_pack("[{s:s, s:s, s:s}]",
            "msg", MSG_UNFLAG,
            "key", "A",
            "file_id", "2000-01-02"
        ), NULL, NULL, 1
    );
    if(append(tranger, "A", 1, 6) < 0) {
        printf("%sERROR%s --> %s: the append was refused\n", On_Red BWhite, Color_Off, case_name);
        result += -1;
    }
    result += test_json(NULL);

    snprintf(test, sizeof(test), "%s: the key loads whole", case_name);
    set_expected_results(test, NULL, NULL, NULL, 1);
    result += check_list(tranger, test, "A@1 A@2 A@3 A@4 A@6 A@5 B@1", FALSE);
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

    result += test_fault("1. the first row cannot be read", FAULT_FIRST_ROW_EIO,
        "Cannot read a record of md2 file, read FAILED", "first");
    result += test_fault("2. the last row is read short", FAULT_LAST_ROW_SHORT,
        "Cannot read a record of md2 file, short read", "last");

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
