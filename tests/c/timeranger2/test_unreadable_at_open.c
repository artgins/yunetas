/****************************************************************************
 *          test_unreadable_at_open.c
 *
 *  A store damaged BEFORE the tranger starts: the topic's cache is built
 *  from what is on disk, and a key whose files cannot be read must say so
 *  in every load of it, exactly as it does when the damage happens behind
 *  a running tranger's back (test_open_list_history.c).
 *
 *  It did not (independent review of the third fix round, repro
 *  indep3_B/restart): the cache build dropped an unreadable md2 with a
 *  `continue` after a critical. The key read as a shorter key, nothing
 *  failed, `load_failed` and `load_failed_keys` stayed empty, and treedb's
 *  guards never fired after a restart. And a record whose CONTENT cannot be
 *  read was handed to the callback as NULL: treedb made a blank node of it,
 *  with id "".
 *
 *  A md2 cut to 0 bytes with its content not empty is NOT here: it is the
 *  shape of an append that was never acknowledged, not damage, and it is
 *  ignored with a warning (test_uncommitted_append.c).
 *
 *  Key A has three daily files (rows 1, 2, 3), key B one. The damage is in
 *  A's second file. A load walks the files in its direction and stops where
 *  the damage is, the same place a running tranger stops:
 *      forward:  A@1 (the files before the damage), then B
 *      backward: A@3 (the files after it), then B
 *
 *      2. the md2 cannot be read (mode 000). A md2 whose size is not a
 *         whole number of rows is NOT here: its last row is torn, an
 *         append that was never acknowledged, and a master cuts it back
 *         (test_torn_md2_tail.c).
 *      3. the content file cut to 0 bytes, the md2 whole.
 *      4. an EMPTY md2 with an empty content file: nothing is lost, the
 *         key loads whole.
 *      5. the key deleted: the list is whole again.
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

#define APP         "test_unreadable_at_open"
#define DATABASE    "tr_unreadable_at_open"
#define TOPIC_NAME  "topic_unreadable"
#define DAY1        946684800   // 2000-01-01
#define DAY         86400

#define MSG_FLAG    "md2 file of the key unreadable when its cache was built: every load of the key says load_failed"
#define MSG_ITER    "The history of the key is not whole: a md2 file of it could not be read when its cache was built"
#define MSG_LIST    "Cannot load the whole history of a key of the list: the records read before the failure were handed, the list goes on with the next key"
#define MSG_ONE     "Cannot load the history of the list's key"

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

PRIVATE void append(json_t *tranger, const char *key, int day, int v)
{
    md2_record_ex_t md = {0};
    tranger2_append_record(tranger, TOPIC_NAME, (uint64_t)(DAY1 + day*DAY), 0, &md,
        json_pack("{s:s, s:I, s:i}", "id", key, "tm", (json_int_t)(DAY1 + day*DAY), "v", v)
    );
}

/*
 *  A store with A: rows 1, 2, 3 in three daily files, and B: row 1
 */
PRIVATE int build_store(void)
{
    rmrdir(path_database);
    set_expected_results("unreadable at open: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    if(!tranger || !create_topic(tranger)) {
        printf("%sERROR%s --> cannot create the store\n", On_Red BWhite, Color_Off);
        return -1;
    }
    append(tranger, "A", 0, 1);
    append(tranger, "A", 1, 2);
    append(tranger, "A", 2, 3);
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
 *  The keyless list, both directions, and what it says it lacks
 */
PRIVATE int check_keyless(json_t *tranger, const char *case_name,
    const char *expected_fwd, const char *expected_bwd, BOOL expect_failed)
{
    int result = 0;
    char what[128];
    for(int b = 0; b < 2; b++) {
        json_t *list = open_list(tranger, NULL, b? TRUE: FALSE);
        snprintf(what, sizeof(what), "%s: keyless list %s", case_name, b? "backward": "forward");
        if(!list) {
            printf("%sERROR%s --> %s REFUSED\n", On_Red BWhite, Color_Off, what);
            result += -1;
            continue;
        }
        result += expect(what, got, b? expected_bwd: expected_fwd);

        BOOL failed = json_is_true(json_object_get(list, "load_failed"));
        json_t *keys = json_object_get(list, "load_failed_keys");
        char *s = json2uglystr(keys? keys: json_null());
        char expected_keys[32];
        snprintf(expected_keys, sizeof(expected_keys), "%s", expect_failed? "[\"A\"]": "null");
        snprintf(what, sizeof(what), "%s: load_failed_keys %s", case_name, b? "backward": "forward");
        result += expect(what, s? s: "", expected_keys);
        GBMEM_FREE(s)
        if(failed != expect_failed) {
            printf("%sERROR%s --> %s: load_failed %d, expected %d\n",
                On_Red BWhite, Color_Off, case_name, failed, expect_failed);
            result += -1;
        }
        tranger2_close_list(tranger, list);
    }
    return result;
}

/*
 *  One damage, a restart, and every way of loading the key
 */
PRIVATE int test_damage(const char *case_name, const char *damage)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }

    char path[PATH_MAX];
    if(strcmp(damage, "unreadable") == 0) {
        if(geteuid() == 0) {
            printf("%s: SKIPPED, running as root: a md2 of mode 000 is still read\n", case_name);
            return 0;
        }
        file_of_a(path, sizeof(path), "2000-01-02", "md2");
        if(chmod(path, 0) < 0) {
            result += -1;
        }
    } else if(strcmp(damage, "json") == 0) {
        file_of_a(path, sizeof(path), "2000-01-02", "json");
        if(truncate(path, 0) < 0) {
            result += -1;
        }
    }
    if(result < 0) {
        printf("%sERROR%s --> %s: cannot damage %s\n", On_Red BWhite, Color_Off, case_name, path);
        return result;
    }

    /*-------------------------------------*
     *  The restart: the cache is built
     *  from the damaged store
     *-------------------------------------*/
    char test[128];
    snprintf(test, sizeof(test), "%s: open", case_name);
    if(strcmp(damage, "unreadable") == 0) {
        set_expected_results(test, json_pack("[{s:s},{s:s}]",
            "msg", "Cannot open md2 file",
            "msg", MSG_FLAG
        ), NULL, NULL, 1);
    } else {
        set_expected_results(test, NULL, NULL, NULL, 1);
    }
    json_t *tranger = startup();
    if(!tranger || !tranger2_open_topic(tranger, TOPIC_NAME, FALSE)) {
        printf("%sERROR%s --> %s: cannot open the store\n", On_Red BWhite, Color_Off, case_name);
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  The keyless list
     *-------------------------------------*/
    snprintf(test, sizeof(test), "%s: keyless lists", case_name);
    if(strcmp(damage, "json") == 0) {
        set_expected_results(test, json_pack("[{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Bad on-disk record: __offset__/__size__ out of range",
            "msg", MSG_LIST,
            "msg", "Bad on-disk record: __offset__/__size__ out of range",
            "msg", MSG_LIST
        ), NULL, NULL, 1);
    } else {
        set_expected_results(test, json_pack("[{s:s},{s:s},{s:s},{s:s}]",
            "msg", MSG_ITER,
            "msg", MSG_LIST,
            "msg", MSG_ITER,
            "msg", MSG_LIST
        ), NULL, NULL, 1);
    }
    result += check_keyless(tranger, case_name, "A@1 B@1", "A@3 B@1", TRUE);
    result += test_json(NULL);

    /*-------------------------------------*
     *  The list of the key alone is
     *  refused, and an iterator says so
     *-------------------------------------*/
    snprintf(test, sizeof(test), "%s: the key's own list and iterator", case_name);
    if(strcmp(damage, "json") == 0) {
        set_expected_results(test, json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "Bad on-disk record: __offset__/__size__ out of range",
            "msg", MSG_ONE,
            "msg", "Bad on-disk record: __offset__/__size__ out of range"
        ), NULL, NULL, 1);
    } else {
        set_expected_results(test, json_pack("[{s:s},{s:s},{s:s}]",
            "msg", MSG_ITER,
            "msg", MSG_ONE,
            "msg", MSG_ITER
        ), NULL, NULL, 1);
    }
    json_t *list = open_list(tranger, "A", FALSE);
    if(list) {
        printf("%sERROR%s --> %s: the list of A alone was handed over\n",
            On_Red BWhite, Color_Off, case_name);
        tranger2_close_list(tranger, list);
        result += -1;
    }
    got[0] = 0;
    json_t *it = tranger2_open_iterator(
        tranger, TOPIC_NAME, "A", NULL, on_record, "it-a", "test", NULL, NULL
    );
    if(!it || !json_is_true(json_object_get(it, "load_failed"))) {
        printf("%sERROR%s --> %s: an iterator of A does not say load_failed\n",
            On_Red BWhite, Color_Off, case_name);
        result += -1;
    }
    result += expect("iterator of A", got, "A@1");
    if(it) {
        tranger2_close_iterator(tranger, it);
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  5. The key deleted: whole again
     *-------------------------------------*/
    snprintf(test, sizeof(test), "%s: the key deleted", case_name);
    set_expected_results(test, NULL, NULL, NULL, 1);
    tranger2_delete_key(tranger, TOPIC_NAME, "A");
    result += check_keyless(tranger, case_name, "B@1", "B@1", FALSE);
    result += test_json(NULL);

    set_expected_results("unreadable at open: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/*
 *  4. An empty md2 with an empty content file loses nothing
 */
PRIVATE int test_empty_files(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }
    char path[PATH_MAX];
    file_of_a(path, sizeof(path), "2000-01-05", "md2");
    int fd1 = open(path, O_WRONLY|O_CREAT, 0660);
    file_of_a(path, sizeof(path), "2000-01-05", "json");
    int fd2 = open(path, O_WRONLY|O_CREAT, 0660);
    if(fd1 < 0 || fd2 < 0) {
        printf("%sERROR%s --> cannot create the empty files\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(fd1 >= 0) {
        close(fd1);
    }
    if(fd2 >= 0) {
        close(fd2);
    }

    set_expected_results("4. empty md2 and content: the key loads whole", NULL, NULL, NULL, 1);
    json_t *tranger = startup();
    tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    result += check_keyless(tranger, "4. empty files", "A@1 A@2 A@3 B@1", "A@3 A@2 A@1 B@1", FALSE);
    json_t *range = tranger2_topic_key_range(tranger, TOPIC_NAME, "A");
    char bf[64];
    snprintf(bf, sizeof(bf), "%d %d",
        (int)kw_get_int(0, range, "rows", 0, 0),
        (int)(kw_get_int(0, range, "fr_t", 0, 0) - DAY1));
    result += expect("4. the range of A ignores the empty file", bf, "3 0");
    JSON_DECREF(range)
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

    result += test_damage("2. md2 that cannot be read", "unreadable");
    result += test_damage("3. content cut to 0 bytes", "json");
    result += test_empty_files();

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
