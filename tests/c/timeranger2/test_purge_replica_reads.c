/****************************************************************************
 *          test_purge_replica_reads.c
 *
 *  Retention, a replica that READS a purged key at a bad moment
 *  (DESIGN-retention.md §4.4, §4.5):
 *
 *      - its cache build cannot read the base of the key (`.purged`) for a
 *        cause that goes away and changes nothing on disk (EMFILE): the
 *        key is flagged, and the next load reads the base again. Up to
 *        7.27.6 only a change of the directory or of the base was a retry,
 *        so the key stayed flagged, every load of it empty;
 *      - a cause that stays (EACCES, the same file) is NOT read again at
 *        every load: that is still the rule;
 *      - the master purges the key between the listing of its directory and
 *        the open of a md2 file: the base, read again, covers the file, and
 *        the key is numbered as the master numbers it. Up to 7.27.6 the file
 *        was flagged unreadable and the key numbered below the master.
 *
 *  `-Wl,--wrap=open,--wrap=fopen` (see CMakeLists.txt): __wrap_fopen() fails
 *  the read of one `.purged` with a chosen errno, __wrap_open() purges the
 *  key on the master just before the replica opens one md2 file.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP "test_purge_replica_reads"

#define DATABASE    "tr_purge_replica_reads"
#define TOPIC_NAME  "topic_purge_replica_reads"
#define KEY1        "0000000000000000001"   // a base read with EMFILE
#define KEY2        "0000000000000000002"   // a base read with EACCES
#define KEY3        "0000000000000000003"   // purged between the listing and the open
#define DAY1        946684800   // 2000-01-01
#define DAY2        (DAY1 + 86400)
#define DAY3        (DAY2 + 86400)

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE json_t *master_tranger = NULL;

/*
 *  The fault of __wrap_fopen(): the next read of `fail_purged_of` fails
 *  with `fail_errno`. `purged_reads` counts the reads of that file
 */
PRIVATE char fail_purged_of[PATH_MAX] = "";
PRIVATE int fail_errno = 0;
PRIVATE int purged_reads = 0;
PRIVATE char count_purged_of[PATH_MAX] = "";

/*
 *  The fault of __wrap_open(): the master purges KEY3 up to day 2 just
 *  before `purge_at` is opened
 */
PRIVATE char purge_at[PATH_MAX] = "";
PRIVATE BOOL in_purge = FALSE;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PRIVATE int expect(const char *what, const char *got, const char *expected);
PRIVATE int expect_int(const char *what, json_int_t got, json_int_t expected);

/***************************************************************
 *              Wraps
 ***************************************************************/
FILE *__real_fopen(const char *path, const char *mode);
FILE *__wrap_fopen(const char *path, const char *mode);

FILE *__wrap_fopen(const char *path, const char *mode)
{
    if(count_purged_of[0] && strcmp(path, count_purged_of) == 0) {
        purged_reads++;
    }
    if(fail_purged_of[0] && strcmp(path, fail_purged_of) == 0) {
        fail_purged_of[0] = 0;
        errno = fail_errno;
        return NULL;
    }
    return __real_fopen(path, mode);
}

int __real_open(const char *path, int flags, ...);
int __wrap_open(const char *path, int flags, ...);

int __wrap_open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    if(flags & (O_CREAT|O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    if(!in_purge && purge_at[0] && strcmp(path, purge_at) == 0) {
        purge_at[0] = 0;
        in_purge = TRUE;
        tranger2_delete_key_files(master_tranger, TOPIC_NAME, KEY3, DAY3);
        in_purge = FALSE;
    }
    return __real_open(path, flags, mode);
}

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

PRIVATE json_t *create_topic(json_t *tranger)
{
    return tranger2_create_topic(
        tranger, TOPIC_NAME, "id", "tm",
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

PRIVATE void drain(int turns)
{
    for(int i = 0; i < turns; i++) {
        yev_loop_run_once(yev_loop);
    }
}

/*
 *  What a load of `key` serves, "<content>:<g_rowid> ...", and its load_failed
 */
PRIVATE BOOL served(json_t *tranger, const char *key, char *bf, size_t bfsize)
{
    bf[0] = 0;
    json_t *data = json_array();
    json_t *it = tranger2_open_iterator(
        tranger, TOPIC_NAME, key, json_object(), NULL, "served", "", data, NULL
    );
    BOOL load_failed = kw_get_bool(0, it, "load_failed", 0, 0);
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
    return load_failed;
}

PRIVATE void key_path(char *bf, size_t bfsize, const char *key, const char *filename)
{
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), getenv("HOME"), "tests_yuneta", DATABASE, NULL);
    build_path(bf, bfsize, path_database, TOPIC_NAME, "keys", key, filename, NULL);
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

    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database), getenv("HOME"), "tests_yuneta", DATABASE, NULL);
    rmrdir(path_database);

    /*-------------------------------------*
     *  A master: KEY1 and KEY2 with day 1
     *  purged, KEY3 with three days
     *-------------------------------------*/
    set_expected_results(
        "purge replica reads: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_tranger(TRUE);
    if(!tm || !create_topic(tm)) {
        if(tm) {
            tranger2_shutdown(tm);
        }
        return -1;
    }
    master_tranger = tm;
    const char *days[3] = {"A", "B", "C"};
    for(int d = 0; d < 3; d++) {
        for(int r = 0; r < 2; r++) {
            char content[8];
            snprintf(content, sizeof(content), "%s%d", days[d], r + 1);
            uint64_t t = DAY1 + (uint64_t)d*86400 + (uint64_t)r;
            if(d < 2) {
                append_one(tm, 1, t, content);
                append_one(tm, 2, t, content);
            }
            append_one(tm, 3, t, content);
        }
    }
    result += expect_int("KEY1 purged", tranger2_delete_key_files(tm, TOPIC_NAME, KEY1, DAY2), 2);
    result += expect_int("KEY2 purged", tranger2_delete_key_files(tm, TOPIC_NAME, KEY2, DAY2), 2);
    result += test_json(NULL);

    /*-------------------------------------*
     *  A replica that cannot read a base
     *  once (EMFILE): the next load reads
     *  it again
     *-------------------------------------*/
    set_expected_results(
        "purge replica reads: a base read with EMFILE",
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot read the base of a purged key",
            "msg", "The base of a purged key cannot be read: the key is flagged unlisted, every load of it says load_failed"
        ),
        NULL, NULL, 1
    );
    key_path(fail_purged_of, sizeof(fail_purged_of), KEY1, ".purged");
    fail_errno = EMFILE;
    json_t *tr = startup_tranger(FALSE);
    if(!tr || !tranger2_open_topic(tr, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> cannot open the replica\n", On_Red BWhite, Color_Off);
        if(tr) {
            tranger2_shutdown(tr);
        }
        tranger2_shutdown(tm);
        return -1;
    }
    result += expect("the fault was used", fail_purged_of, "");
    result += test_json(NULL);

    set_expected_results(
        "purge replica reads: the next load after EMFILE",
        json_pack("[{s:s}]",
            "msg", "key directory listed again: its files are counted, and the key is not flagged"
        ),
        NULL, NULL, 1
    );
    BOOL load_failed = served(tr, KEY1, bf, sizeof(bf));
    result += expect("served", bf, "B1:3 B2:4");
    result += expect_int("load_failed", load_failed, 0);
    result += test_json(NULL);

    /*-------------------------------------*
     *  A base read with EACCES: a cause
     *  that stays is not read again at
     *  every load
     *-------------------------------------*/
    set_expected_results(
        "purge replica reads: a base read with EACCES",
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot read the base of a purged key",
            "msg", "The base of a purged key cannot be read: the key is flagged unlisted, every load of it says load_failed"
        ),
        NULL, NULL, 1
    );
    tranger2_shutdown(tr);
    drain(10);
    key_path(fail_purged_of, sizeof(fail_purged_of), KEY2, ".purged");
    fail_errno = EACCES;
    tr = startup_tranger(FALSE);
    if(!tr || !tranger2_open_topic(tr, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> cannot open the replica\n", On_Red BWhite, Color_Off);
        if(tr) {
            tranger2_shutdown(tr);
        }
        tranger2_shutdown(tm);
        return -1;
    }
    result += test_json(NULL);

    set_expected_results(
        "purge replica reads: the next load after EACCES",
        json_pack("[{s:s}]",
            "msg", "The history of the key is not whole: its directory could not be listed when its cache was built"
        ),
        NULL, NULL, 1
    );
    key_path(count_purged_of, sizeof(count_purged_of), KEY2, ".purged");
    purged_reads = 0;
    load_failed = served(tr, KEY2, bf, sizeof(bf));
    result += expect("served", bf, "");
    result += expect_int("load_failed", load_failed, 1);
    result += expect_int("the base was not read again", purged_reads, 0);
    count_purged_of[0] = 0;
    result += test_json(NULL);
    tranger2_shutdown(tr);
    drain(10);

    /*-------------------------------------*
     *  The master purges KEY3 between the
     *  listing of the replica and its open
     *  of the first md2 file
     *-------------------------------------*/
    set_expected_results(
        "purge replica reads: purged between the listing and the open",
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot open md2 file",
            "msg", "md2 file of the key purged by the master while its cache was built: its rows are in the base, read again"
        ),
        NULL, NULL, 1
    );
    key_path(purge_at, sizeof(purge_at), KEY3, "2000-01-01.md2");
    tr = startup_tranger(FALSE);
    if(!tr || !tranger2_open_topic(tr, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> cannot open the replica\n", On_Red BWhite, Color_Off);
        if(tr) {
            tranger2_shutdown(tr);
        }
        tranger2_shutdown(tm);
        return -1;
    }
    result += expect("the master purged", purge_at, "");
    json_t *range = tranger2_topic_key_range(tr, TOPIC_NAME, KEY3);
    result += expect_int("the replica's base", kw_get_int(0, range, "purged", -1, 0), 4);
    result += expect_int("its rows", kw_get_int(0, range, "rows", -1, 0), 2);
    JSON_DECREF(range)
    load_failed = served(tr, KEY3, bf, sizeof(bf));
    result += expect("served: the master's numbering", bf, "C1:5 C2:6");
    result += expect_int("load_failed", load_failed, 0);
    result += test_json(NULL);

    set_expected_results("purge replica reads: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tr);
    tranger2_shutdown(tm);
    master_tranger = NULL;
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
