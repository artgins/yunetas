/****************************************************************************
 *          test_mark_tm_order.c
 *
 *  tranger2_mark_tm_order(), the migration of a topic written before the
 *  markers, where it does not go all the way:
 *
 *      1. A failure half way (a md2 file that cannot be read). The topic is
 *         not marked, the markers written stay, and the cells the call had
 *         already widened in memory keep their whole ranges -- which are
 *         true of the disk. The key's TOTALS follow the cells: the key
 *         never says a narrower range than its own cells.
 *      2. A file whose name leaves no room for a marker. It does not abort
 *         the WHOLE topic: the file cannot have a marker, and every load
 *         reads it whole already. It is skipped and logged, and the topic
 *         is marked.
 *
 *  Its cost (linear in the files of a key) is measured outside the suite,
 *  not asserted here: a timing is not a test.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP         "test_mark_tm_order"
#define DATABASE    "tr_mark_tm_order"
#define TOPIC_NAME  "topic_legacy"
#define DAY1        946684800   // 2000-01-01
#define DAY         86400

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char path_root[PATH_MAX];
PRIVATE char path_database[PATH_MAX];

/***************************************************************
 *              Helpers
 ***************************************************************/
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

PRIVATE void append(json_t *tranger, const char *topic_name, const char *key, int day, int sec, int tm)
{
    md2_record_ex_t md = {0};
    tranger2_append_record(tranger, topic_name, (uint64_t)(DAY1 + day*DAY + sec), 0, &md,
        json_pack("{s:s, s:I}", "id", key, "tm", (json_int_t)tm)
    );
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
 *  What a topic created by 7.25.4 or earlier looks like: no
 *  "marks_tm_unordered" in its topic_desc.json, no tm marker
 */
PRIVATE int make_it_legacy(const char *topic_name)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, topic_name, "topic_desc.json", NULL);
    json_t *desc = json_load_file(path, 0, 0);
    if(!desc) {
        printf("%sERROR%s --> cannot read %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }
    json_object_del(desc, "marks_tm_unordered");
    chmod(path, 0600);
    int ret = json_dump_file(desc, path, JSON_INDENT(4));
    JSON_DECREF(desc)
    if(ret < 0) {
        printf("%sERROR%s --> cannot rewrite %s\n", On_Red BWhite, Color_Off, path);
        return -1;
    }

    const char *keys[] = {"a", "b", NULL};
    for(int k = 0; keys[k]; k++) {
        char key_dir[PATH_MAX];
        build_path(key_dir, sizeof(key_dir), path_database, topic_name, "keys", keys[k], NULL);
        dir_array_t da;
        get_ordered_filename_array(0, key_dir, ".*\\.tm_unordered", WD_MATCH_REGULAR_FILE, &da);
        for(int i = 0; i < da.count; i++) {
            unlink(da.items[i]);
        }
        dir_array_free(&da);
    }
    return 0;
}

PRIVATE BOOL marker_exists(const char *topic_name, const char *key, const char *name)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, topic_name, "keys", key, name, NULL);
    return is_regular_file(path);
}

PRIVATE BOOL desc_marks(const char *topic_name)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), path_database, topic_name, "topic_desc.json", NULL);
    json_t *desc = json_load_file(path, 0, 0);
    BOOL marks = json_is_true(json_object_get(desc, "marks_tm_unordered"));
    JSON_DECREF(desc)
    return marks;
}

/***************************************************************************
 *  1. A failure half way
 ***************************************************************************/
PRIVATE int test_failure_half_way(void)
{
    int result = 0;
    rmrdir(path_database);

    /*
     *  a: day 1 with a tm that goes back (100, 50, 120), day 2 (200, 210)
     *  b: day 1 (10, 20)
     */
    set_expected_results("mark: setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    tranger2_create_topic(tranger, TOPIC_NAME, "id", "tm", NULL, sf_string_key,
        json_pack("{s:s, s:I}", "id", "", "tm", (json_int_t)0), 0);
    append(tranger, TOPIC_NAME, "a", 0, 1, 100);
    append(tranger, TOPIC_NAME, "a", 0, 2, 50);
    append(tranger, TOPIC_NAME, "a", 0, 3, 120);
    append(tranger, TOPIC_NAME, "a", 1, 1, 200);
    append(tranger, TOPIC_NAME, "a", 1, 2, 210);
    append(tranger, TOPIC_NAME, "b", 0, 1, 10);
    append(tranger, TOPIC_NAME, "b", 0, 2, 20);
    tranger2_shutdown(tranger);
    result += make_it_legacy(TOPIC_NAME);
    tranger = startup();
    tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    test_json(NULL);    // the setup logs are not what is tested

    if(geteuid() == 0) {
        printf("  SKIPPED, running as root: a md2 of mode 000 is still read\n");
        tranger2_shutdown(tranger);
        return result;
    }

    /*-------------------------------------*
     *  The second file of `a` cannot be
     *  read: the call fails half way
     *-------------------------------------*/
    set_expected_results("1. mark: a failure half way",
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot open md2 file to read its order",
            "msg", "Cannot mark the topic: it is not marked; the markers written stay, and the cells read keep their whole ranges"
        ),
        NULL, NULL, 1
    );
    char day2[PATH_MAX];
    build_path(day2, sizeof(day2), path_database, TOPIC_NAME, "keys", "a", "2000-01-02.md2", NULL);
    chmod(day2, 0);
    json_t *report = tranger2_mark_tm_order(tranger, TOPIC_NAME);
    chmod(day2, 0660);
    if(report) {
        printf("%sERROR%s --> the mark answered a report with a file it could not read\n",
            On_Red BWhite, Color_Off);
        JSON_DECREF(report)
        result += -1;
    }
    result += expect("the topic in memory is not marked",
        json_is_true(json_object_get(tranger2_topic(tranger, TOPIC_NAME), "marks_tm_unordered"))?
            "marked": "not marked", "not marked");
    result += expect("topic_desc.json is not marked",
        desc_marks(TOPIC_NAME)? "marked": "not marked", "not marked");
    result += expect("the marker written before the failure stays",
        marker_exists(TOPIC_NAME, "a", "2000-01-01.tm_unordered")? "yes": "no", "yes");

    /*
     *  The first file of `a` was read whole: its cell says tm 50..120, and
     *  the key's totals must say it too
     */
    json_t *range = tranger2_topic_key_range(tranger, TOPIC_NAME, "a");
    char bf[64];
    snprintf(bf, sizeof(bf), "%d..%d",
        (int)kw_get_int(0, range, "fr_tm", 0, 0), (int)kw_get_int(0, range, "to_tm", 0, 0));
    result += expect("the totals of `a` follow its widened cell", bf, "50..210");
    JSON_DECREF(range)
    result += test_json(NULL);

    /*-------------------------------------*
     *  Readable again: the call goes all
     *  the way
     *-------------------------------------*/
    set_expected_results("1. mark: again, with every file readable",
        json_pack("[{s:s}]",
            "msg", "Topic marked: its md2 files out of order have their markers"
        ),
        NULL, NULL, 1
    );
    report = tranger2_mark_tm_order(tranger, TOPIC_NAME);
    result += expect("the second call answers", report? "report": "NULL", "report");
    result += expect("topic_desc.json is marked",
        desc_marks(TOPIC_NAME)? "marked": "not marked", "marked");
    JSON_DECREF(report)
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *  2. A file whose name leaves no room for a marker
 ***************************************************************************/
PRIVATE int test_long_names(void)
{
    int result = 0;
    const char *topic_name = "topic_long_names";
    rmrdir(path_database);

    set_expected_results("mark: long names, setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    char long_mask[256];
    snprintf(long_mask, sizeof(long_mask), "%%Y-%%m-%%d");
    size_t ln = strlen(long_mask);
    memset(long_mask + ln, 'x', 244 - 10);
    long_mask[ln + 244 - 10] = 0;       // file ids of 244 characters
    tranger2_create_topic(tranger, topic_name, "id", "tm",
        json_pack("{s:s}", "filename_mask", long_mask),
        sf_string_key,
        json_pack("{s:s, s:I}", "id", "", "tm", (json_int_t)0),
        0
    );
    append(tranger, topic_name, "a", 0, 1, 150);
    append(tranger, topic_name, "a", 0, 2, 900);
    append(tranger, topic_name, "a", 0, 3, 120);    // tm goes back: a marker it cannot have
    append(tranger, topic_name, "b", 0, 1, 10);
    test_json(NULL);    // the append logs its marker it cannot write: test_tm_order's

    set_expected_results("2. mark: a file name too long for its marker is skipped",
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot mark a md2 file, its name leaves no room for a marker: skipped, every load reads it whole",
            "msg", "Topic marked: its md2 files out of order have their markers"
        ),
        NULL, NULL, 1
    );
    json_t *report = tranger2_mark_tm_order(tranger, topic_name);
    char *s = report? json2uglystr(report): NULL;
    printf("  mark_tm_order: %s\n", s? s: "NULL");
    GBMEM_FREE(s)
    result += expect("the topic is marked, the file skipped", report? "report": "NULL", "report");
    snprintf(long_mask, sizeof(long_mask), "%d files",
        (int)kw_get_int(0, report, "files", 0, 0));
    result += expect("every file is counted", long_mask, "2 files");
    JSON_DECREF(report)
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

    result += test_failure_half_way();
    result += test_long_names();

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
