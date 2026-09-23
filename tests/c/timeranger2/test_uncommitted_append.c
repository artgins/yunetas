/****************************************************************************
 *          test_uncommitted_append.c
 *
 *  An append writes the record's content first and its md2 row after. When
 *  the md2 row is never written (the md2 cannot be created or written, the
 *  process is killed, the power goes), the append was never acknowledged,
 *  and the file keeps a content that no row names.
 *
 *  The cache build of 200a1791e took "a md2 of 0 rows with a content file
 *  that is not empty" for a damaged key and flagged it (independent review
 *  of the fourth fix round, repros indep4_A/r_orphan): a forward load
 *  stopped there and hid the acknowledged rows of the later files, a treedb
 *  node with good older rows disappeared, and the flag was never cleared.
 *
 *  Key A has rows in four daily files (days 0..3), key B one.
 *
 *      1. The md2 of A's day 2 with 0 rows and its content file not empty:
 *         a warning names the file, the file is ignored, the key is not
 *         flagged, every load reads the other files (A@1 A@2 A@4).
 *      2. The same file takes a new append: its row names the new content,
 *         and after a restart the file is read and warns no more.
 *      3. An append whose md2 cannot be created: -1, and the content file
 *         is cut back to where the record began (0 bytes for a new file).
 *         The next append of the file, once its md2 can be created, is the
 *         only content of it.
 *      4. A md2 that could not be opened at the cache build (0000 mode)
 *         flags the key; once it can be read, the next append into the
 *         file counts the file again and the flag goes. An append into a
 *         file still unreadable is refused: its row would follow rows no
 *         cell counts. A filtered iterator opened while the file was
 *         flagged takes its index again at the recount: the rowids after
 *         the file moved, and it used to be emptied for good.
 *      5. The rollback of 3 with the production default on_critical_error
 *         (LOG_OPT_EXIT_ZERO), in a child process: the process exits, and
 *         the content was cut back BEFORE it did.
 *      6. A write that stops part way (RLIMIT_FSIZE, in a child process),
 *         first of the content, then of the md2 row: a short write()
 *         returns a count and does not set errno. It is logged as a short
 *         write, with the bytes written and expected; it used to be logged
 *         as "write FAILED" with a stale errno. The files are cut back.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP         "test_uncommitted_append"
#define DATABASE    "tr_uncommitted_append"
#define TOPIC_NAME  "topic_uncommitted"
#define DAY1        946684800   // 2000-01-01
#define DAY         86400

#define MSG_UNCOMMITTED "md2 file of the key with no rows and a content file that is not empty: an append that was never acknowledged, the file is ignored"
#define MSG_FLAG    "md2 file of the key unreadable when its cache was built: every load of the key says load_failed"
#define MSG_ITER    "The history of the key is not whole: a md2 file of it could not be read when its cache was built"
#define MSG_LIST    "Cannot load the whole history of a key of the list: the records read before the failure were handed, the list goes on with the next key"
#define MSG_REFUSED "Cannot append record, its file is flagged unreadable: its row would follow rows no cell counts"
#define MSG_SHORT_CONTENT "Cannot append record, short write of its content: the file size limit or the disk is full"
#define MSG_SHORT_MD2   "Cannot save record metadata, short write: the file size limit or the disk is full"
#define SHORT_TOPIC     "topic_short_write"
#define MD2_ROW_SIZE    32  // the on-disk md2 row: __t__, __tm__, __offset__, __size__ (uint64 each)
#define MSG_READABLE "md2 file of the key readable again: it is counted, and the key is not flagged for it"

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
    return tranger2_append_record(tranger, TOPIC_NAME, (uint64_t)(DAY1 + day*DAY), 0, &md,
        json_pack("{s:s, s:I, s:i}", "id", key, "tm", (json_int_t)(DAY1 + day*DAY), "v", v)
    );
}

PRIVATE void file_of_a(char *bf, size_t bfsize, const char *day, const char *ext)
{
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "%s.%s", day, ext);
    build_path(bf, bfsize, path_database, TOPIC_NAME, "keys", "A", name, NULL);
}

/*
 *  A: rows 1, 2, 3, 4 in four daily files, B: row 1
 */
PRIVATE int build_store(void)
{
    rmrdir(path_database);
    set_expected_results("uncommitted append: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    if(!tranger || !create_topic(tranger)) {
        printf("%sERROR%s --> cannot create the store\n", On_Red BWhite, Color_Off);
        return -1;
    }
    append(tranger, "A", 0, 1);
    append(tranger, "A", 1, 2);
    append(tranger, "A", 2, 3);
    append(tranger, "A", 3, 4);
    append(tranger, "B", 0, 1);
    tranger2_shutdown(tranger);
    test_json(NULL);    // the setup logs are not what is tested
    return 0;
}

PRIVATE json_t *open_list(json_t *tranger, const char *key, BOOL backward)
{
    got[0] = 0;
    json_t *match_cond = json_pack("{s:I, s:b, s:I}",
        "to_rowid", (json_int_t)1000000,   // no realtime
        "backward", backward,
        "load_record_callback", (json_int_t)(uintptr_t)on_record
    );
    if(key) {
        json_object_set_new(match_cond, "key", json_string(key));
    }
    return tranger2_open_list(tranger, TOPIC_NAME, match_cond, json_object(), "", FALSE, "");
}

/*
 *  "<total_rows>: v.. v.." of the first page of an iterator
 */
PRIVATE const char *page_of(json_t *tranger, json_t *iterator)
{
    static char bf[256];
    bf[0] = 0;
    if(!iterator) {
        snprintf(bf, sizeof(bf), "(no iterator)");
        return bf;
    }
    json_t *page = tranger2_iterator_get_page(tranger, iterator, 1, 100, FALSE);
    snprintf(bf, sizeof(bf), "%ld:", (long)kw_get_int(0, page, "total_rows", 0, 0));
    int idx; json_t *record;
    json_array_foreach(json_object_get(page, "data"), idx, record) {
        size_t ln = strlen(bf);
        snprintf(bf + ln, sizeof(bf) - ln, " v%d", (int)kw_get_int(0, record, "v", 0, 0));
    }
    JSON_DECREF(page)
    return bf;
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
 *  The list of A alone and the keyless list, both directions
 */
PRIVATE int check_loads(json_t *tranger, const char *case_name,
    const char *a_fwd, const char *a_bwd, BOOL expect_failed)
{
    int result = 0;
    char what[160];
    for(int b = 0; b < 2; b++) {
        json_t *list = open_list(tranger, "A", b? TRUE: FALSE);
        snprintf(what, sizeof(what), "%s: list of A %s", case_name, b? "backward": "forward");
        if(!list) {
            if(!expect_failed) {
                printf("%sERROR%s --> %s REFUSED\n", On_Red BWhite, Color_Off, what);
                result += -1;
            }
        } else {
            result += expect(what, got, b? a_bwd: a_fwd);
            tranger2_close_list(tranger, list);
        }

        list = open_list(tranger, NULL, b? TRUE: FALSE);
        snprintf(what, sizeof(what), "%s: keyless list %s", case_name, b? "backward": "forward");
        if(!list) {
            printf("%sERROR%s --> %s REFUSED\n", On_Red BWhite, Color_Off, what);
            result += -1;
            continue;
        }
        if(!expect_failed) {
            char expected[256];
            snprintf(expected, sizeof(expected), "%s B@1", b? a_bwd: a_fwd);
            result += expect(what, got, expected);
        }
        BOOL failed = json_is_true(json_object_get(list, "load_failed"));
        if(failed != expect_failed) {
            printf("%sERROR%s --> %s: load_failed %d, expected %d\n",
                On_Red BWhite, Color_Off, what, failed, expect_failed);
            result += -1;
        }
        tranger2_close_list(tranger, list);
    }
    return result;
}

/***************************************************************************
 *  1 and 2: a file whose only append was never acknowledged
 ***************************************************************************/
PRIVATE int test_uncommitted_file(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }

    /*
     *  The shape a first append of day 2 whose md2 row was never written
     *  leaves: an md2 of 0 rows, a content file with the record
     */
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), "2000-01-03", "md2");
    if(truncate(path, 0) < 0) {
        printf("%sERROR%s --> cannot cut %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }

    set_expected_results("1. uncommitted append: open",
        json_pack("[{s:s}]", "msg", MSG_UNCOMMITTED), NULL, NULL, 1
    );
    json_t *tranger = startup();
    if(!tranger || !create_topic(tranger)) {
        printf("%sERROR%s --> 1: cannot open the store\n", On_Red BWhite, Color_Off);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);

    set_expected_results("1. uncommitted append: every load reads the other files",
        NULL, NULL, NULL, 1
    );
    result += check_loads(tranger, "1", "A@1 A@2 A@4", "A@4 A@2 A@1", FALSE);
    json_t *range = tranger2_topic_key_range(tranger, TOPIC_NAME, "A");
    char bf[64];
    snprintf(bf, sizeof(bf), "%d", (int)kw_get_int(0, range, "rows", 0, 0));
    result += expect("1. rows of A", bf, "3");
    JSON_DECREF(range)
    result += test_json(NULL);

    /*
     *  2. The file takes an append: its first row
     */
    set_expected_results("2. the file takes an append", NULL, NULL, NULL, 1);
    if(append(tranger, "A", 2, 5) < 0) {
        printf("%sERROR%s --> 2: append into the file refused\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += check_loads(tranger, "2", "A@1 A@2 A@5 A@4", "A@4 A@5 A@2 A@1", FALSE);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    set_expected_results("2. after a restart: read, no warning", NULL, NULL, NULL, 1);
    tranger = startup();
    create_topic(tranger);
    result += check_loads(tranger, "2 restarted", "A@1 A@2 A@5 A@4", "A@4 A@5 A@2 A@1", FALSE);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  3: an append whose md2 cannot be created is rolled back
 ***************************************************************************/
PRIVATE int test_rollback(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }

    /*
     *  A directory where the md2 of day 5 goes: it cannot be created
     */
    char md2_path[PATH_MAX];
    char json_path[PATH_MAX];
    file_of_a(md2_path, sizeof(md2_path), "2000-01-06", "md2");
    file_of_a(json_path, sizeof(json_path), "2000-01-06", "json");
    if(mkdir(md2_path, 0770) < 0) {
        printf("%sERROR%s --> cannot make %s\n", On_Red BWhite, Color_Off, md2_path);
        return -1;
    }

    set_expected_results_unordered("3. an append whose md2 cannot be created",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "Cannot create json file",
            "msg", "Cannot open file to write",
            "msg", "Cannot append record, its md2 file cannot be opened: its content was cut back"
        ), NULL, NULL, 1
    );
    json_t *tranger = startup();
    create_topic(tranger);
    if(append(tranger, "A", 5, 6) == 0) {
        printf("%sERROR%s --> 3: the append was acknowledged\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    char bf[64];
    snprintf(bf, sizeof(bf), "%ld", (long)filesize(json_path));
    result += expect("3. the content of the refused append is cut back", bf, "0");

    /*
     *  The md2 can be created now: the next append is the only content
     */
    set_expected_results("3. the next append of the file", NULL, NULL, NULL, 1);
    rmdir(md2_path);
    md2_record_ex_t md = {0};
    if(tranger2_append_record(tranger, TOPIC_NAME, (uint64_t)(DAY1 + 5*DAY), 0, &md,
            json_pack("{s:s, s:I, s:i}", "id", "A", "tm", (json_int_t)(DAY1 + 5*DAY), "v", 7)) < 0) {
        printf("%sERROR%s --> 3: the next append was refused\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    char expected[64];
    snprintf(bf, sizeof(bf), "offset %ld size %ld",
        (long)md.__offset__, (long)filesize(json_path));
    snprintf(expected, sizeof(expected), "offset 0 size %ld", (long)md.__size__);
    result += expect("3. the content file holds only the acknowledged record", bf, expected);

    set_expected_results("3. after a restart", NULL, NULL, NULL, 1);
    tranger = startup();
    create_topic(tranger);
    result += check_loads(tranger, "3", "A@1 A@2 A@3 A@4 A@7", "A@7 A@4 A@3 A@2 A@1", FALSE);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  4: the flag of a file goes when the file can be read again
 ***************************************************************************/
PRIVATE int test_flag_cleared(void)
{
    int result = 0;
    if(geteuid() == 0) {
        printf("  4. SKIPPED, running as root: a md2 of mode 000 is still read\n");
        return 0;
    }
    if(build_store() < 0) {
        return -1;
    }

    char path[PATH_MAX];
    file_of_a(path, sizeof(path), "2000-01-02", "md2");
    if(chmod(path, 0) < 0) {
        printf("%sERROR%s --> cannot chmod %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }

    set_expected_results("4. an md2 that cannot be opened at the open: flagged",
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot open md2 file",
            "msg", MSG_FLAG
        ), NULL, NULL, 1
    );
    json_t *tranger = startup();
    create_topic(tranger);
    result += test_json(NULL);

    /*
     *  Still unreadable: an append into the file is refused, and writes
     *  nothing
     */
    char json_path[PATH_MAX];
    file_of_a(json_path, sizeof(json_path), "2000-01-02", "json");
    off_t json_size = filesize(json_path);
    set_expected_results("4. an append into the flagged file is refused",
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot open md2 file",
            "msg", MSG_REFUSED
        ), NULL, NULL, 1
    );
    if(append(tranger, "A", 1, 8) == 0) {
        printf("%sERROR%s --> 4: an append into an unreadable file was acknowledged\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(filesize(json_path) != json_size) {
        printf("%sERROR%s --> 4: the refused append wrote content\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  A filtered iterator of A opened while the file is flagged: its
     *  index names the rows of the other files
     */
    set_expected_results("4. a filtered iterator of the flagged key",
        json_pack("[{s:s}]", "msg", MSG_ITER), NULL, NULL, 1
    );
    json_t *pager = tranger2_open_iterator(tranger, TOPIC_NAME, "A",
        json_pack("{s:I}", "from_t", (json_int_t)DAY1),
        NULL, "pager", "test", NULL, NULL
    );
    result += test_json(NULL);
    result += expect("4. the filtered iterator while flagged", page_of(tranger, pager),
        "3: v1 v3 v4");

    /*
     *  Readable again: the next append counts the file, the flag goes
     */
    chmod(path, 0660);
    set_expected_results("4. readable again: the append counts the file",
        json_pack("[{s:s}]", "msg", MSG_READABLE), NULL, NULL, 1
    );
    if(append(tranger, "A", 1, 9) < 0) {
        printf("%sERROR%s --> 4: append into the file readable again refused\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  The recount moved the rowids after the file: the filtered iterator
     *  takes its index again, as an open would have built it just before
     *  the append (the rows of the file are in it; the append, like every
     *  append after the open of a filtered iterator, is not). A recount is
     *  not a delete -- its index was emptied for good.
     */
    set_expected_results("4. the filtered iterator after the recount", NULL, NULL, NULL, 1);
    result += expect("4. the filtered iterator after the recount", page_of(tranger, pager),
        "4: v1 v2 v3 v4");
    tranger2_close_iterator(tranger, pager);
    result += test_json(NULL);

    set_expected_results("4. the key is whole", NULL, NULL, NULL, 1);
    result += check_loads(tranger, "4", "A@1 A@2 A@9 A@3 A@4", "A@4 A@3 A@9 A@2 A@1", FALSE);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  5: the rollback of 3 with the production default on_critical_error
 *
 *  C_TRANGER and C_TREEDB run their tranger with on_critical_error=2
 *  (LOG_OPT_EXIT_ZERO): gobj_log_critical() exits inside the log call. The
 *  rollback came after that call, so with the default it never ran and the
 *  content of the refused append stayed on disk (independent review of the
 *  fifth fix round, repro indep5_A/r_exit_rollback). It runs before now.
 ***************************************************************************/
PRIVATE int test_rollback_before_exit(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }

    char md2_path[PATH_MAX];
    char json_path[PATH_MAX];
    file_of_a(md2_path, sizeof(md2_path), "2000-01-06", "md2");
    file_of_a(json_path, sizeof(json_path), "2000-01-06", "json");
    if(mkdir(md2_path, 0770) < 0) {
        printf("%sERROR%s --> cannot make %s\n", On_Red BWhite, Color_Off, md2_path);
        return -1;
    }

    fflush(stdout);
    pid_t pid = fork();
    if(pid < 0) {
        printf("%sERROR%s --> 5: fork() failed\n", On_Red BWhite, Color_Off);
        rmdir(md2_path);
        return -1;
    }
    if(pid == 0) {
        json_t *tranger = tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i, s:s}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", LOG_OPT_EXIT_ZERO,
            "filename_mask", "%Y-%m-%d"
        ), 0);
        create_topic(tranger);
        append(tranger, "A", 5, 6);
        fflush(stdout);
        _exit(3);   // the critical did not exit
    }

    int status = 0;
    waitpid(pid, &status, 0);
    rmdir(md2_path);

    char bf[64];
    snprintf(bf, sizeof(bf), "exit %d", WIFEXITED(status)? WEXITSTATUS(status): -1);
    result += expect("5. the append's critical exits the process", bf, "exit 0");

    snprintf(bf, sizeof(bf), "%ld", (long)filesize(json_path));
    result += expect("5. the content of the refused append is cut back before the exit", bf, "0");

    return result;
}

/***************************************************************************
 *  6: a write that stops part way is logged as a short write.
 *  A topic whose records are smaller than their md2 row (no tkey, only the
 *  id), so that one limit can let the content through and cut the md2 row.
 ***************************************************************************/
PRIVATE json_t *create_short_topic(json_t *tranger)
{
    return tranger2_create_topic(
        tranger, SHORT_TOPIC, "id", "", NULL, sf_string_key,
        json_pack("{s:s}", "id", ""),
        0
    );
}

PRIVATE int append_short(json_t *tranger)
{
    md2_record_ex_t md = {0};
    return tranger2_append_record(tranger, SHORT_TOPIC, (uint64_t)DAY1, 0, &md,
        json_pack("{s:s}", "id", "A")
    );
}

PRIVATE void file_of_short(char *bf, size_t bfsize, const char *ext)
{
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "2000-01-01.%s", ext);
    build_path(bf, bfsize, path_database, SHORT_TOPIC, "keys", "A", name, NULL);
}

/*
 *  In a child: the file size limit is `limit`, one append, and the log it
 *  leaves must be `expected` (owned). Exit 0 when it is. The limit applies
 *  to every file the child writes, stdout too when it is a file: run the
 *  test with its output to a pipe, as ctest does.
 */
PRIVATE int short_write_in_child(const char *what, off_t limit, json_t *expected)
{
    fflush(stdout);
    pid_t pid = fork();
    if(pid < 0) {
        printf("%sERROR%s --> %s: fork() failed\n", On_Red BWhite, Color_Off, what);
        JSON_DECREF(expected)
        return -1;
    }
    if(pid == 0) {
        signal(SIGXFSZ, SIG_IGN);
        json_t *tranger = tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i, s:s}",
            "path", path_root,
            "database", DATABASE,
            "master", 1,
            "on_critical_error", 0,
            "filename_mask", "%Y-%m-%d"
        ), 0);
        create_short_topic(tranger);
        struct rlimit rl = {(rlim_t)limit, (rlim_t)limit};
        int result = 0;
        if(setrlimit(RLIMIT_FSIZE, &rl) < 0) {
            printf("%sERROR%s --> %s: setrlimit() failed\n", On_Red BWhite, Color_Off, what);
            result = -1;
        }
        set_expected_results(what, expected, NULL, NULL, 1);
        errno = ENOENT;     // a stale errno, that a short write does not change
        if(append_short(tranger) == 0) {
            printf("%sERROR%s --> %s: the append was acknowledged\n", On_Red BWhite, Color_Off, what);
            result = -1;
        }
        result += test_json(NULL);
        fflush(stdout);
        _exit(result < 0? 1 : 0);
    }

    JSON_DECREF(expected)   // the child's copy is the one that is used
    int status = 0;
    waitpid(pid, &status, 0);
    char bf[64];
    snprintf(bf, sizeof(bf), "exit %d", WIFEXITED(status)? WEXITSTATUS(status): -1);
    return expect(what, bf, "exit 0");
}

PRIVATE int test_short_write(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }

    set_expected_results("6. setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    create_short_topic(tranger);
    for(int i = 0; i < 10; i++) {
        append_short(tranger);
    }
    tranger2_shutdown(tranger);
    test_json(NULL);    // the setup logs are not what is tested

    char md2_path[PATH_MAX];
    char json_path[PATH_MAX];
    file_of_short(md2_path, sizeof(md2_path), "md2");
    file_of_short(json_path, sizeof(json_path), "json");
    off_t md2_before = filesize(md2_path);
    off_t json_before = filesize(json_path);
    if(md2_before != 10*MD2_ROW_SIZE || json_before >= md2_before) {
        printf("%sERROR%s --> 6: unexpected files, md2 %ld json %ld\n",
            On_Red BWhite, Color_Off, (long)md2_before, (long)json_before);
        return -1;
    }
    char expected_record[32];
    snprintf(expected_record, sizeof(expected_record), "%ld", (long)(json_before / 10));

    /*
     *  The content: 5 bytes of the record are written
     */
    result += short_write_in_child("6. a short write of the content",
        json_before + 5,
        json_pack("[{s:s, s:s, s:s}]",
            "msg", MSG_SHORT_CONTENT,
            "written", "5",
            "expected", expected_record
        )
    );

    /*
     *  The md2 row: the content goes whole, 16 bytes of the row are written
     */
    char expected_row[32];
    snprintf(expected_row, sizeof(expected_row), "%d", MD2_ROW_SIZE);
    result += short_write_in_child("6. a short write of the md2 row",
        md2_before + 16,
        json_pack("[{s:s, s:s, s:s}]",
            "msg", MSG_SHORT_MD2,
            "written", "16",
            "expected", expected_row
        )
    );

    char bf[64];
    char expected[64];
    snprintf(bf, sizeof(bf), "md2 %ld json %ld", (long)filesize(md2_path), (long)filesize(json_path));
    snprintf(expected, sizeof(expected), "md2 %ld json %ld", (long)md2_before, (long)json_before);
    result += expect("6. both files are cut back", bf, expected);

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

    result += test_uncommitted_file();
    result += test_rollback();
    result += test_flag_cleared();
    result += test_rollback_before_exit();
    result += test_short_write();

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
