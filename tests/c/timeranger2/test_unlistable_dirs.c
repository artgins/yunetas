/****************************************************************************
 *          test_unlistable_dirs.c
 *
 *  A directory of the store that cannot be LISTED (opendir() fails: EMFILE,
 *  EACCES, ENOMEM) is a failure, never an empty directory. Up to 7.25.4 it
 *  was read as empty, and nothing said so:
 *
 *      1. tranger2_mark_tm_order() with a key directory that cannot be
 *         listed: the key counted as 0 files, the topic was MARKED, and a
 *         tm query then lost the rows of the file nobody scanned (a legacy
 *         file whose tm goes back: 100, 300, 200 -- its cell says
 *         [100,200], and a query of [250,350] skipped it). Now the
 *         migration fails and the topic is not marked.
 *
 *      2. A key directory that cannot be listed at the open: the key loaded
 *         EMPTY with load_failed false, so treedb's create guard did not
 *         fire. Now the key is flagged `unlisted`: every load of it says
 *         load_failed, a keyless list names it in load_failed_keys, and an
 *         append into it lists the key again first -- refused while it
 *         cannot be listed, accepted with the key's files counted once it
 *         can.
 *
 *      3. `keys/` that cannot be listed at the open: the topic opened with
 *         no keys. Now the topic is not opened; the next open tries again.
 *
 *      4. A REPLICA that opened a key it could not list, while a feed of
 *         it is open: once the directory can be listed again, the next
 *         notification of the master's append lists the key again, hands
 *         the feed the new row (with its rowid in the whole key), and the
 *         replica reads the whole key. Up to this fix the key stayed
 *         flagged until the topic was opened again (and before the flag,
 *         the notified file alone was counted as the whole key).
 *
 *      5. A load tries the flags again: a replica with a key it could not
 *         list and a md2 it could not read (mode 0 both) reads both keys
 *         whole once the modes are put back, with no append at all. While
 *         the cause is there the load only says load_failed, and logs
 *         nothing more (the flags are not tried again at every load).
 *
 *  The directories are made unlistable with chmod 0: skipped as root.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP         "test_unlistable_dirs"
#define DATABASE    "tr_unlistable_dirs"
#define TOPIC_NAME  "topic_unlistable"
#define DAY1        946684800   // 2000-01-01
#define DAY         86400

#define MSG_OPENDIR     "Cannot open directory"
#define MSG_UNLISTED    "key directory cannot be listed when its cache was built: every load of the key says load_failed"
#define MSG_ITER        "The history of the key is not whole: its directory could not be listed when its cache was built"
#define MSG_LIST        "Cannot load the whole history of a key of the list: the records read before the failure were handed, the list goes on with the next key"
#define MSG_APPEND      "Cannot append record, its key cannot be listed: its row would follow rows no cell counts"
#define MSG_RELISTED    "key directory listed again: its files are counted, and the key is not flagged"
#define MSG_MARK        "Cannot mark the topic: it is not marked; the markers written stay, and the cells read keep their whole ranges"
#define MSG_KEYS        "Cannot list the keys of the topic"
#define MSG_NO_TOPIC    "Cannot open topic: its keys cannot be listed"

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
    snprintf(got + ln, sizeof(got) - ln, "%s%s@%d", ln? " ": "", key,
        (int)kw_get_int(0, record, "v", 0, 0));
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

PRIVATE int append(json_t *tranger, const char *key, int day, json_int_t tm, int v)
{
    md2_record_ex_t md = {0};
    return tranger2_append_record(tranger, TOPIC_NAME, (uint64_t)(DAY1 + day*DAY), 0, &md,
        json_pack("{s:s, s:I, s:i}", "id", key, "tm", tm, "v", v)
    );
}

PRIVATE void dir_of(char *bf, size_t bfsize, const char *key)
{
    build_path(bf, bfsize, path_database, TOPIC_NAME, "keys", key, NULL);
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
 *  An iterator of the key: what it loads, and whether it says load_failed
 */
PRIVATE int check_iterator(
    json_t *tranger,
    const char *what,
    const char *key,
    json_t *match_cond,     // owned
    const char *expected,
    BOOL expect_failed
)
{
    int result = 0;
    got[0] = 0;
    json_t *it = tranger2_open_iterator(
        tranger, TOPIC_NAME, key, match_cond, on_record, "it", "test", NULL, NULL
    );
    if(!it) {
        printf("%sERROR%s --> %s: no iterator\n", On_Red BWhite, Color_Off, what);
        return -1;
    }
    result += expect(what, got, expected);
    BOOL failed = json_is_true(json_object_get(it, "load_failed"));
    if(failed != expect_failed) {
        printf("%sERROR%s --> %s: load_failed %d, expected %d\n",
            On_Red BWhite, Color_Off, what, failed, expect_failed);
        result += -1;
    }
    tranger2_close_iterator(tranger, it);
    return result;
}

/*
 *  The rows of A whose tm is in [250,350]: the row of tm 300
 */
PRIVATE int check_tm_query(json_t *tranger, const char *what)
{
    return check_iterator(tranger, what, "A",
        json_pack("{s:I, s:I}", "from_tm", (json_int_t)250, "to_tm", (json_int_t)350),
        "A@2", FALSE
    );
}

/*
 *  The topic looks like one of 7.25.4 or earlier: not marking
 */
PRIVATE int unmark_topic_desc(void)
{
    char topic_dir[PATH_MAX];
    build_path(topic_dir, sizeof(topic_dir), path_database, TOPIC_NAME, NULL);
    json_t *desc = load_json_from_file(0, topic_dir, "topic_desc.json", 0);
    if(!desc) {
        return -1;
    }
    json_object_del(desc, "marks_tm_unordered");
    char path[PATH_MAX];
    build_path(path, sizeof(path), topic_dir, "topic_desc.json", NULL);
    chmod(path, 0660);
    int ret = json_dump_file(desc, path, JSON_INDENT(4));
    JSON_DECREF(desc)
    return ret;
}

PRIVATE BOOL topic_desc_marks(void)
{
    char topic_dir[PATH_MAX];
    build_path(topic_dir, sizeof(topic_dir), path_database, TOPIC_NAME, NULL);
    json_t *desc = load_json_from_file(0, topic_dir, "topic_desc.json", 0);
    BOOL marks = json_is_true(json_object_get(desc, "marks_tm_unordered"));
    JSON_DECREF(desc)
    return marks;
}

/***************************************************************************
 *  1. mark_tm_order() with a key directory that cannot be listed
 ***************************************************************************/
PRIVATE int test_mark_unlistable(void)
{
    int result = 0;
    char key_dir[PATH_MAX];
    dir_of(key_dir, sizeof(key_dir), "A");

    rmrdir(path_database);
    set_expected_results("1. setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    if(!tranger || !create_topic(tranger)) {
        printf("%sERROR%s --> cannot create the store\n", On_Red BWhite, Color_Off);
        return -1;
    }
    tranger2_shutdown(tranger);
    if(unmark_topic_desc() < 0) {
        printf("%sERROR%s --> cannot unmark the topic\n", On_Red BWhite, Color_Off);
        return -1;
    }
    tranger = startup();
    tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    append(tranger, "A", 0, 100, 1);
    append(tranger, "A", 0, 300, 2);
    append(tranger, "A", 0, 200, 3);
    tranger2_shutdown(tranger);
    test_json(NULL);    // the setup logs are not what is tested

    set_expected_results("1. the legacy topic answers the tm query", NULL, NULL, NULL, 1);
    tranger = startup();
    tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    result += check_tm_query(tranger, "1. before the migration");
    result += test_json(NULL);

    set_expected_results("1. the migration fails", json_pack("[{s:s},{s:s}]",
        "msg", MSG_OPENDIR,
        "msg", MSG_MARK
    ), NULL, NULL, 1);
    chmod(key_dir, 0);
    json_t *report = tranger2_mark_tm_order(tranger, TOPIC_NAME);
    chmod(key_dir, 02770);
    if(report) {
        printf("%sERROR%s --> 1. mark_tm_order() succeeded with a key it could not list\n",
            On_Red BWhite, Color_Off);
        JSON_DECREF(report)
        result += -1;
    }
    if(topic_desc_marks()) {
        printf("%sERROR%s --> 1. the topic was marked\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);
    tranger2_shutdown(tranger);

    set_expected_results("1. after the failed migration, the rows are all there", NULL, NULL, NULL, 1);
    tranger = startup();
    tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    result += check_tm_query(tranger, "1. after the failed migration");
    result += test_json(NULL);

    set_expected_results("1. the migration again, listable", json_pack("[{s:s}]",
        "msg", "Topic marked: its md2 files out of order have their markers"
    ), NULL, NULL, 1);
    report = tranger2_mark_tm_order(tranger, TOPIC_NAME);
    if(!report || !topic_desc_marks()) {
        printf("%sERROR%s --> 1. the migration failed with the key listable\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    JSON_DECREF(report)
    tranger2_shutdown(tranger);
    tranger = startup();
    tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    result += check_tm_query(tranger, "1. after the migration");
    tranger2_shutdown(tranger);
    result += test_json(NULL);

    return result;
}

/*
 *  A store with A: rows 1, 2, 3 in three daily files, and B: row 1
 */
PRIVATE int build_store(void)
{
    rmrdir(path_database);
    set_expected_results("setup", NULL, NULL, NULL, 0);
    json_t *tranger = startup();
    if(!tranger || !create_topic(tranger)) {
        printf("%sERROR%s --> cannot create the store\n", On_Red BWhite, Color_Off);
        return -1;
    }
    append(tranger, "A", 0, DAY1, 1);
    append(tranger, "A", 1, DAY1 + DAY, 2);
    append(tranger, "A", 2, DAY1 + 2*DAY, 3);
    append(tranger, "B", 0, DAY1, 1);
    tranger2_shutdown(tranger);
    test_json(NULL);    // the setup logs are not what is tested
    return 0;
}

/***************************************************************************
 *  2. A key directory that cannot be listed at the open
 ***************************************************************************/
PRIVATE int test_key_unlistable_at_open(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }
    char key_dir[PATH_MAX];
    dir_of(key_dir, sizeof(key_dir), "A");

    set_expected_results("2. open with the key A unlistable", json_pack("[{s:s},{s:s}]",
        "msg", MSG_OPENDIR,
        "msg", MSG_UNLISTED
    ), NULL, NULL, 1);
    chmod(key_dir, 0);
    json_t *tranger = startup();
    json_t *topic = tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    if(!topic) {
        printf("%sERROR%s --> 2. the topic did not open\n", On_Red BWhite, Color_Off);
        chmod(key_dir, 02770);
        tranger2_shutdown(tranger);
        test_json(NULL);
        return -1;
    }
    result += test_json(NULL);

    set_expected_results("2. an iterator of A says load_failed", json_pack("[{s:s}]",
        "msg", MSG_ITER
    ), NULL, NULL, 1);
    result += check_iterator(tranger, "2. iterator of A", "A", NULL, "", TRUE);
    result += test_json(NULL);

    set_expected_results("2. a keyless list names A", json_pack("[{s:s},{s:s}]",
        "msg", MSG_ITER,
        "msg", MSG_LIST
    ), NULL, NULL, 1);
    got[0] = 0;
    json_t *list = tranger2_open_list(tranger, TOPIC_NAME,
        json_pack("{s:I, s:I}",
            "to_rowid", (json_int_t)1000000,   // no realtime
            "load_record_callback", (json_int_t)(uintptr_t)on_record
        ),
        json_object(), "", FALSE, ""
    );
    if(!list) {
        printf("%sERROR%s --> 2. the keyless list was refused\n", On_Red BWhite, Color_Off);
        result += -1;
    } else {
        result += expect("2. keyless list", got, "B@1");
        char *s = json2uglystr(json_object_get(list, "load_failed_keys"));
        result += expect("2. load_failed_keys", s? s: "", "[\"A\"]");
        GBMEM_FREE(s)
        if(!json_is_true(json_object_get(list, "load_failed"))) {
            printf("%sERROR%s --> 2. the keyless list does not say load_failed\n",
                On_Red BWhite, Color_Off);
            result += -1;
        }
        tranger2_close_list(tranger, list);
    }
    result += test_json(NULL);

    set_expected_results("2. an append into A is refused while A cannot be listed",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", MSG_OPENDIR,
            "msg", MSG_UNLISTED,
            "msg", MSG_APPEND
        ), NULL, NULL, 1);
    if(append(tranger, "A", 3, DAY1 + 3*DAY, 4) >= 0) {
        printf("%sERROR%s --> 2. an append into the unlisted key was accepted\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("2. once A can be listed, the append lists it and goes on",
        json_pack("[{s:s}]",
            "msg", MSG_RELISTED
        ), NULL, NULL, 1);
    chmod(key_dir, 02770);
    if(append(tranger, "A", 3, DAY1 + 3*DAY, 4) < 0) {
        printf("%sERROR%s --> 2. the append into A, listable again, was refused\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += check_iterator(tranger, "2. iterator of A after the append", "A", NULL,
        "A@1 A@2 A@3 A@4", FALSE);
    result += test_json(NULL);

    set_expected_results("2. shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  3. keys/ that cannot be listed at the open
 ***************************************************************************/
PRIVATE int test_keys_unlistable_at_open(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }
    char keys_dir[PATH_MAX];
    build_path(keys_dir, sizeof(keys_dir), path_database, TOPIC_NAME, "keys", NULL);

    set_expected_results("3. open with keys/ unlistable", json_pack("[{s:s},{s:s}]",
        "msg", MSG_KEYS,
        "msg", MSG_NO_TOPIC
    ), NULL, NULL, 1);
    chmod(keys_dir, 0);
    json_t *tranger = startup();
    json_t *topic = tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    chmod(keys_dir, 02770);
    if(topic) {
        printf("%sERROR%s --> 3. the topic opened with keys/ unlistable\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(tranger2_topic_is_open(tranger, TOPIC_NAME)) {
        printf("%sERROR%s --> 3. the topic stays open\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("3. the next open, listable, reads every key", NULL, NULL, NULL, 1);
    topic = tranger2_open_topic(tranger, TOPIC_NAME, FALSE);
    if(!topic) {
        printf("%sERROR%s --> 3. the topic does not open again\n", On_Red BWhite, Color_Off);
        result += -1;
    } else {
        result += check_iterator(tranger, "3. iterator of A", "A", NULL, "A@1 A@2 A@3", FALSE);
        result += check_iterator(tranger, "3. iterator of B", "B", NULL, "B@1", FALSE);
    }
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    return result;
}

/*
 *  A tranger that follows the disk (its rt_disk feeds need the loop)
 */
PRIVATE json_t *startup_with_loop(BOOL master)
{
    return tranger2_startup(0, json_pack("{s:s, s:s, s:b, s:i, s:s}",
        "path", path_root,
        "database", DATABASE,
        "master", master? 1: 0,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y-%m-%d"
    ), yev_loop);
}

PRIVATE int feed_rows = 0;
PRIVATE json_int_t feed_last_rowid = 0;
PRIVATE int feed_last_v = 0;

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
    if(strcmp(key, "A") == 0) {
        feed_rows++;
        feed_last_rowid = rowid;
        feed_last_v = (int)kw_get_int(0, record, "v", 0, 0);
    }
    JSON_DECREF(record)
    return 0;
}

/***************************************************************************
 *  4. A replica: the notification of an append lists the key again
 ***************************************************************************/
PRIVATE int test_replica_notification(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }
    char key_dir[PATH_MAX];
    dir_of(key_dir, sizeof(key_dir), "A");

    set_expected_results("4. a replica opens with the key A unlistable", json_pack("[{s:s},{s:s}]",
        "msg", MSG_OPENDIR,
        "msg", MSG_UNLISTED
    ), NULL, NULL, 1);
    json_t *master = startup_with_loop(TRUE);
    tranger2_open_topic(master, TOPIC_NAME, FALSE);
    chmod(key_dir, 0);
    json_t *replica = startup_with_loop(FALSE);
    json_t *topic = tranger2_open_topic(replica, TOPIC_NAME, FALSE);
    json_t *feed = tranger2_open_rt_disk(
        replica, TOPIC_NAME, "", NULL, on_feed_record, "rtALL", "test", NULL
    );
    chmod(key_dir, 02770);
    if(!topic || !feed) {
        printf("%sERROR%s --> 4. the replica or its feed did not open\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    set_expected_results("4. the notification of an append lists the key again", json_pack("[{s:s}]",
        "msg", MSG_RELISTED
    ), NULL, NULL, 1);
    feed_rows = 0;
    append(master, "A", 3, DAY1 + 3*DAY, 4);
    for(int i = 0; i < 50 && feed_rows == 0; i++) {
        yev_loop_run_once(yev_loop);
    }
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(feed_rows != 1 || feed_last_v != 4 || feed_last_rowid != 4) {
        printf("%sERROR%s --> 4. the feed got %d row(s), the last v=%d rowid=%d; expected 1, v=4, rowid=4\n",
            On_Red BWhite, Color_Off, feed_rows, feed_last_v, (int)feed_last_rowid);
        result += -1;
    }
    result += check_iterator(replica, "4. the replica reads the whole key", "A", NULL,
        "A@1 A@2 A@3 A@4", FALSE);
    result += test_json(NULL);

    set_expected_results("4. shutdown", NULL, NULL, NULL, 1);
    tranger2_close_rt_disk(replica, feed);
    tranger2_shutdown(replica);
    tranger2_shutdown(master);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  5. A load tries the flags again once their cause is gone
 ***************************************************************************/
PRIVATE int test_load_retries_flags(void)
{
    int result = 0;
    if(build_store() < 0) {
        return -1;
    }
    char key_dir[PATH_MAX];
    dir_of(key_dir, sizeof(key_dir), "A");
    char md2_b[PATH_MAX];
    build_path(md2_b, sizeof(md2_b), path_database, TOPIC_NAME, "keys", "B", "2000-01-01.md2", NULL);
    struct stat st;
    stat(md2_b, &st);

    set_expected_results("5. a replica opens with A unlistable and B's md2 unreadable",
        json_pack("[{s:s},{s:s},{s:s},{s:s}]",
            "msg", MSG_OPENDIR,
            "msg", MSG_UNLISTED,
            "msg", "Cannot open md2 file",
            "msg", "md2 file of the key unreadable when its cache was built: every load of the key says load_failed"
        ), NULL, NULL, 1);
    chmod(key_dir, 0);
    chmod(md2_b, 0);
    json_t *replica = startup_with_loop(FALSE);
    tranger2_open_topic(replica, TOPIC_NAME, FALSE);
    result += test_json(NULL);

    set_expected_results("5. while the cause is there, a load says load_failed and nothing more",
        json_pack("[{s:s},{s:s}]",
            "msg", MSG_ITER,
            "msg", "The history of the key is not whole: a md2 file of it could not be read when its cache was built"
        ), NULL, NULL, 1);
    result += check_iterator(replica, "5. A, still unlistable", "A", NULL, "", TRUE);
    result += check_iterator(replica, "5. B, still unreadable", "B", NULL, "", TRUE);
    result += test_json(NULL);

    set_expected_results("5. the modes put back: the loads read both keys whole",
        json_pack("[{s:s},{s:s}]",
            "msg", MSG_RELISTED,
            "msg", "md2 file of the key readable again: it is counted, and the key is not flagged for it"
        ), NULL, NULL, 1);
    chmod(key_dir, 02770);
    chmod(md2_b, st.st_mode & 07777);
    result += check_iterator(replica, "5. A, listable again", "A", NULL, "A@1 A@2 A@3", FALSE);
    result += check_iterator(replica, "5. B, readable again", "B", NULL, "B@1", FALSE);
    tranger2_shutdown(replica);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    if(geteuid() == 0) {
        printf("%s: SKIPPED, running as root: a directory of mode 000 is still listed\n", APP);
        return 0;
    }
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, sizeof(path_database), path_root, DATABASE, NULL);

    result += test_mark_unlistable();
    result += test_key_unlistable_at_open();
    result += test_keys_unlistable_at_open();
    result += test_replica_notification();
    result += test_load_retries_flags();

    rmrdir(path_database);
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
