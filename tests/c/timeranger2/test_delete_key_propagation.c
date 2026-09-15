/****************************************************************************
 *          test_delete_key_propagation.c
 *
 *  Regression coverage for tranger2_delete_key() → subscriber propagation:
 *      - do_test_rt_mem_specific_key: rt_mem listening for a specific key
 *        receives a key_deleted callback when that key is deleted.
 *      - do_test_rt_mem_all_keys:     rt_mem listening for "" (any) is
 *        notified on every delete with the right deleted key.
 *      - do_test_rt_mem_filter_skip:  rt_mem listening for key A is NOT
 *        notified when key B is deleted.
 *      - do_test_rt_disk_in_process:  rt_disk on the master's own tranger
 *        receives the callback through the inotify FS_SUBDIR_DELETED branch.
 *      - do_test_follower:            a real follower: each feed hears a
 *        delete exactly once, with and without records since it opened, its
 *        cache loses the key, and a feed may close itself from the callback.
 *      - do_test_cache_cleared:       topic.cache rollup loses the entry.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP "test_delete_key_propagation"

#define DATABASE    "tr_delete_key_propagation"
#define TOPIC_NAME  "topic_delete_key_propagation"
#define KEY_A       "0000000000000000001"
#define KEY_B       "0000000000000000002"
#define BASE_T      946684800   // 2000-01-01T00:00:00+0000

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

PRIVATE size_t deleted_callback_count = 0;
PRIVATE char deleted_callback_last_key[64];
PRIVATE void *deleted_callback_last_user_data = NULL;

PRIVATE int record_loaded_count = 0;

PRIVATE int my_key_deleted_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    void *user_data
)
{
    deleted_callback_count++;
    snprintf(deleted_callback_last_key,
        sizeof(deleted_callback_last_key), "%s", key? key : "");
    deleted_callback_last_user_data = user_data;
    return 0;
}

PRIVATE int my_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record
)
{
    record_loaded_count++;
    JSON_DECREF(record)
    return 0;
}

PRIVATE void reset_callback_state(void)
{
    deleted_callback_count = 0;
    deleted_callback_last_key[0] = '\0';
    deleted_callback_last_user_data = NULL;
    record_loaded_count = 0;
}

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE void build_paths(
    char *path_root, size_t root_sz,
    char *path_database, size_t db_sz,
    char *path_topic, size_t topic_sz
)
{
    const char *home = getenv("HOME");
    build_path(path_root, root_sz, home, "tests_yuneta", NULL);
    mkrdir(path_root, 02770);
    build_path(path_database, db_sz, path_root, DATABASE, NULL);
    build_path(path_topic, topic_sz, path_database, TOPIC_NAME, NULL);
}

/*
 *  The loop is the PARAMETER of tranger2_startup(): a "yev_loop" key in the
 *  config is overwritten by it. This test used to put it in the config, so
 *  the rt_disk case never armed inotify and passed through the rt_mem path.
 */
PRIVATE json_t *startup_tranger(const char *path_root, BOOL master, BOOL with_loop)
{
    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i}",
        "path", path_root,
        "database", DATABASE,
        "master", master?1:0,
        "on_critical_error", LOG_OPT_TRACE_STACK,
        "filename_mask", "%Y",
        "xpermission" , 02770,
        "rpermission", 0600
    );
    return tranger2_startup(0, jn_tranger, with_loop? yev_loop: 0);
}

PRIVATE json_t *startup_master(const char *path_root, BOOL with_loop)
{
    return startup_tranger(path_root, TRUE, with_loop);
}

/*
 *  The watcher stops queued by a shutdown complete asynchronously (their
 *  io_uring CQEs free each fs_event): give the loop the turns to run them.
 */
PRIVATE void drain(int turns)
{
    for(int i = 0; i < turns; i++) {
        yev_loop_run_once(yev_loop);
    }
}

PRIVATE int create_topic(json_t *tranger)
{
    json_t *topic = tranger2_create_topic(
        tranger,
        TOPIC_NAME,
        "id",
        "tm",
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
    return topic? 0 : -1;
}

PRIVATE int append_to(json_t *tranger, json_int_t id, int n)
{
    for(int j = 0; j < n; j++) {
        json_t *jn_record = json_pack("{s:I, s:I, s:s}",
            "id", id,
            "tm", (json_int_t)(BASE_T + j),
            "content", "payload"
        );
        md2_record_ex_t md = {0};
        if(tranger2_append_record(tranger, TOPIC_NAME,
                BASE_T + j, 0, &md, jn_record) < 0) {
            return -1;
        }
    }
    return 0;
}

/***************************************************************************
 *  do_test_rt_mem_specific_key
 *  rt_mem listens for KEY_A; delete_key(KEY_A) fires the callback once.
 ***************************************************************************/
PRIVATE int do_test_rt_mem_specific_key(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);
    reset_callback_state();

    set_expected_results(
        "rt_mem_specific: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );

    json_t *tranger = startup_master(path_root, FALSE);
    if(!tranger || create_topic(tranger) < 0) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    if(append_to(tranger, 1, 3) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Subscribe with key=KEY_A
     *-------------------------------------*/
    set_expected_results(
        "rt_mem_specific: open + delete fires once",
        NULL, NULL, NULL, 1
    );

    int sentinel = 0;
    json_t *rt = tranger2_open_rt_mem(
        tranger, TOPIC_NAME, KEY_A,
        NULL,
        my_record_callback,
        "specific",
        "", NULL
    );
    if(!rt) {
        result += -1;
    }
    tranger2_set_rt_key_deleted_callback(rt, my_key_deleted_callback, &sentinel);

    if(tranger2_delete_key(tranger, TOPIC_NAME, KEY_A) < 0) {
        result += -1;
    }

    if(deleted_callback_count != 1) {
        printf("%sERROR%s --> specific: expected 1 fire, got %zu\n",
            On_Red BWhite, Color_Off, deleted_callback_count);
        result += -1;
    }
    if(strcmp(deleted_callback_last_key, KEY_A) != 0) {
        printf("%sERROR%s --> specific: expected key=%s, got %s\n",
            On_Red BWhite, Color_Off, KEY_A, deleted_callback_last_key);
        result += -1;
    }
    if(deleted_callback_last_user_data != &sentinel) {
        printf("%sERROR%s --> specific: user_data not echoed back\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    tranger2_close_rt_mem(tranger, rt);
    result += test_json(NULL);

    set_expected_results("rt_mem_specific: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  do_test_rt_mem_all_keys
 *  rt_mem listens with key=""; delete_key(KEY_B) fires once with KEY_B.
 ***************************************************************************/
PRIVATE int do_test_rt_mem_all_keys(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);
    reset_callback_state();

    set_expected_results(
        "rt_mem_all: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );

    json_t *tranger = startup_master(path_root, FALSE);
    if(!tranger || create_topic(tranger) < 0) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    if(append_to(tranger, 2, 2) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }
    result += test_json(NULL);

    set_expected_results(
        "rt_mem_all: any-key listener sees delete",
        NULL, NULL, NULL, 1
    );

    json_t *rt = tranger2_open_rt_mem(
        tranger, TOPIC_NAME, "",
        NULL,
        my_record_callback,
        "any",
        "", NULL
    );
    if(!rt) {
        result += -1;
    }
    tranger2_set_rt_key_deleted_callback(rt, my_key_deleted_callback, NULL);

    if(tranger2_delete_key(tranger, TOPIC_NAME, KEY_B) < 0) {
        result += -1;
    }

    if(deleted_callback_count != 1) {
        printf("%sERROR%s --> all: expected 1 fire, got %zu\n",
            On_Red BWhite, Color_Off, deleted_callback_count);
        result += -1;
    }
    if(strcmp(deleted_callback_last_key, KEY_B) != 0) {
        printf("%sERROR%s --> all: expected key=%s, got %s\n",
            On_Red BWhite, Color_Off, KEY_B, deleted_callback_last_key);
        result += -1;
    }

    tranger2_close_rt_mem(tranger, rt);
    result += test_json(NULL);

    set_expected_results("rt_mem_all: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  do_test_rt_mem_filter_skip
 *  rt_mem listens for KEY_A; delete_key(KEY_B) does NOT fire.
 ***************************************************************************/
PRIVATE int do_test_rt_mem_filter_skip(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);
    reset_callback_state();

    set_expected_results(
        "rt_mem_filter: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );

    json_t *tranger = startup_master(path_root, FALSE);
    if(!tranger || create_topic(tranger) < 0) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    if(append_to(tranger, 1, 2) < 0 || append_to(tranger, 2, 2) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }
    result += test_json(NULL);

    set_expected_results(
        "rt_mem_filter: KEY_A listener ignores delete of KEY_B",
        NULL, NULL, NULL, 1
    );

    json_t *rt = tranger2_open_rt_mem(
        tranger, TOPIC_NAME, KEY_A,
        NULL,
        my_record_callback,
        "filter",
        "", NULL
    );
    if(!rt) {
        result += -1;
    }
    tranger2_set_rt_key_deleted_callback(rt, my_key_deleted_callback, NULL);

    if(tranger2_delete_key(tranger, TOPIC_NAME, KEY_B) < 0) {
        result += -1;
    }

    if(deleted_callback_count != 0) {
        printf("%sERROR%s --> filter: expected 0 fires, got %zu\n",
            On_Red BWhite, Color_Off, deleted_callback_count);
        result += -1;
    }

    tranger2_close_rt_mem(tranger, rt);
    result += test_json(NULL);

    set_expected_results("rt_mem_filter: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  do_test_rt_disk_in_process
 *  Master + its own rt_disk subscriber. Inotify carries the
 *  FS_SUBDIR_DELETED_TYPE on the master's own writes; the follower
 *  branch fires the key_deleted callback exactly once.
 ***************************************************************************/
PRIVATE int do_test_rt_disk_in_process(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);
    reset_callback_state();

    set_expected_results(
        "rt_disk: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );

    /*
     *  Pass yev_loop so the rt_disk watcher actually starts.
     */
    json_t *tranger = startup_master(path_root, TRUE);
    if(!tranger || create_topic(tranger) < 0) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    result += test_json(NULL);

    set_expected_results(
        "rt_disk: open + append + drain + delete",
        NULL, NULL, NULL, 1
    );

    json_t *rt = tranger2_open_rt_disk(
        tranger, TOPIC_NAME, KEY_A,
        NULL,
        my_record_callback,
        "disk-sub",
        "", NULL
    );
    if(!rt) {
        result += -1;
    }
    tranger2_set_rt_key_deleted_callback(rt, my_key_deleted_callback, NULL);

    if(append_to(tranger, 1, 3) < 0) {
        result += -1;
    }

    /*
     *  Drain inotify. The number of records drained doesn't matter
     *  for this test — the key_deleted assertion below is what we
     *  care about. mirror_key_delete_to_disks rmrdirs the subdir
     *  whether empty or with pending hardlinks; inotify reports
     *  SUBDIR_DELETED either way.
     */
    for(int i = 0; i < 8; i++) {
        yev_loop_run_once(yev_loop);
    }

    if(tranger2_delete_key(tranger, TOPIC_NAME, KEY_A) < 0) {
        result += -1;
    }

    /*
     *  Drain the SUBDIR_DELETED event.
     */
    for(int i = 0; i < 12 && deleted_callback_count == 0; i++) {
        yev_loop_run_once(yev_loop);
    }

    if(deleted_callback_count != 1) {
        printf("%sERROR%s --> rt_disk: expected 1 fire, got %zu\n",
            On_Red BWhite, Color_Off, deleted_callback_count);
        result += -1;
    }
    if(strcmp(deleted_callback_last_key, KEY_A) != 0) {
        printf("%sERROR%s --> rt_disk: expected key=%s, got %s\n",
            On_Red BWhite, Color_Off, KEY_A, deleted_callback_last_key);
        result += -1;
    }

    /*  and exactly once: the tail of the batch must not fire it again  */
    drain(10);
    if(deleted_callback_count != 1) {
        printf("%sERROR%s --> rt_disk: %zu fires after draining, expected 1\n",
            On_Red BWhite, Color_Off, deleted_callback_count);
        result += -1;
    }

    tranger2_close_rt_disk(tranger, rt);
    drain(10);
    result += test_json(NULL);

    set_expected_results("rt_disk: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    drain(10);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  do_test_follower
 *  A REAL follower (a non-master tranger, inotify armed) with three feeds:
 *  keyed on A, keyed on B, keyless. Each feed must hear a delete of a key
 *  it wants EXACTLY once, and its topic cache must lose the key:
 *
 *    - A had records after the feeds opened. It used to fire 6 times per
 *      feed: the master mirrors the key directory of EVERY feed, each
 *      removal arrived twice (IN_DELETE_SELF + the parent's IN_DELETE),
 *      and each arrival fired every feed of the topic.
 *    - B had none, so no feed had its directory: it fired 0 times, and the
 *      follower's cache kept the dead key.
 *
 *  Then a keyless feed whose key_deleted callback closes the feed ITSELF,
 *  with two deletes in the same inotify batch: the watcher is stopped from
 *  inside its own callback, and the rest of the batch must not run on it.
 ***************************************************************************/
PRIVATE int count_a = 0, count_b = 0, count_all = 0, count_close = 0;
PRIVATE json_t *closing_tranger = NULL;

PRIVATE int follower_key_deleted_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    void *user_data
)
{
    const char *id = json_string_value(json_object_get(list, "id"));
    if(id && strcmp(id, "rtA")==0) {
        count_a++;
    } else if(id && strcmp(id, "rtB")==0) {
        count_b++;
    } else if(id && strcmp(id, "rtALL")==0) {
        count_all++;
    } else if(id && strcmp(id, "rtCLOSE")==0) {
        count_close++;
        tranger2_close_rt_disk(closing_tranger, list);  // the feed closes itself
    }
    return 0;
}

PRIVATE int check_counts(const char *what, int a, int b, int all)
{
    int result = 0;
    if(count_a != a || count_b != b || count_all != all) {
        printf("%sERROR%s --> follower, %s: rtA=%d rtB=%d rtALL=%d, expected %d/%d/%d\n",
            On_Red BWhite, Color_Off, what, count_a, count_b, count_all, a, b, all);
        result = -1;
    }
    count_a = count_b = count_all = 0;
    return result;
}

PRIVATE int do_test_follower(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);

    set_expected_results(
        "follower: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_master(path_root, TRUE);
    if(!tm || create_topic(tm) < 0) {
        if(tm) {
            tranger2_shutdown(tm);
        }
        return -1;
    }
    if(append_to(tm, 2, 1) < 0) {   /*  KEY_B exists before the feeds  */
        result += -1;
    }
    drain(5);
    result += test_json(NULL);

    set_expected_results("follower: deletes reach each feed once", NULL, NULL, NULL, 1);

    json_t *tf = startup_tranger(path_root, FALSE, TRUE);
    if(!tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> follower: cannot open the follower\n", On_Red BWhite, Color_Off);
        if(tf) {
            tranger2_shutdown(tf);
        }
        tranger2_shutdown(tm);
        return -1;
    }
    json_t *rt_a = tranger2_open_rt_disk(tf, TOPIC_NAME, KEY_A, NULL, my_record_callback, "rtA", "", NULL);
    json_t *rt_b = tranger2_open_rt_disk(tf, TOPIC_NAME, KEY_B, NULL, my_record_callback, "rtB", "", NULL);
    json_t *rt_all = tranger2_open_rt_disk(tf, TOPIC_NAME, "", NULL, my_record_callback, "rtALL", "", NULL);
    if(!rt_a || !rt_b || !rt_all) {
        result += -1;
    }
    tranger2_set_rt_key_deleted_callback(rt_a, follower_key_deleted_callback, NULL);
    tranger2_set_rt_key_deleted_callback(rt_b, follower_key_deleted_callback, NULL);
    tranger2_set_rt_key_deleted_callback(rt_all, follower_key_deleted_callback, NULL);
    drain(10);

    json_t *cache = json_object_get(tranger2_topic(tf, TOPIC_NAME), "cache");

    /*  A: records after the feeds opened  */
    if(append_to(tm, 1, 2) < 0) {
        result += -1;
    }
    drain(20);
    if(tranger2_delete_key(tm, TOPIC_NAME, KEY_A) < 0) {
        result += -1;
    }
    drain(30);
    result += check_counts("delete of a key with records", 1, 0, 1);
    if(json_object_get(cache, KEY_A)) {
        printf("%sERROR%s --> follower: KEY_A still in the cache\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  B: no record since the feeds opened  */
    if(!json_object_get(cache, KEY_B)) {
        printf("%sERROR%s --> follower: KEY_B not in the cache before its delete\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(tranger2_delete_key(tm, TOPIC_NAME, KEY_B) < 0) {
        result += -1;
    }
    drain(30);
    result += check_counts("delete of a key with no records since the feeds opened", 0, 1, 1);
    if(json_object_get(cache, KEY_B)) {
        printf("%sERROR%s --> follower: KEY_B still in the cache\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    /*  Closing the feeds fires nothing  */
    tranger2_close_rt_disk(tf, rt_a);
    tranger2_close_rt_disk(tf, rt_b);
    tranger2_close_rt_disk(tf, rt_all);
    drain(20);
    result += check_counts("closing the feeds", 0, 0, 0);
    result += test_json(NULL);

    /*
     *  A feed that closes itself from its key_deleted callback, with two
     *  deletes queued in the same batch of its watcher.
     */
    set_expected_results("follower: a feed closed from its own callback", NULL, NULL, NULL, 1);
    if(append_to(tm, 1, 1) < 0 || append_to(tm, 2, 1) < 0) {
        result += -1;
    }
    drain(10);
    closing_tranger = tf;
    count_close = 0;
    json_t *rt_close = tranger2_open_rt_disk(
        tf, TOPIC_NAME, "", NULL, my_record_callback, "rtCLOSE", "", NULL
    );
    tranger2_set_rt_key_deleted_callback(rt_close, follower_key_deleted_callback, NULL);
    drain(10);
    if(tranger2_delete_key(tm, TOPIC_NAME, KEY_A) < 0 ||
       tranger2_delete_key(tm, TOPIC_NAME, KEY_B) < 0) {
        result += -1;
    }
    drain(30);
    if(count_close != 1) {
        printf("%sERROR%s --> follower: the self-closing feed fired %d times, expected 1\n",
            On_Red BWhite, Color_Off, count_close);
        result += -1;
    }
    if(tranger2_get_rt_disk_by_id(tf, TOPIC_NAME, "rtCLOSE", "")) {
        printf("%sERROR%s --> follower: the self-closing feed is still open\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("follower: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tf);
    tranger2_shutdown(tm);
    drain(10);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  do_test_cache_cleared
 *  After delete_key, topic.cache no longer has the deleted entry.
 *  (Already verified inside delete_key but worth a direct introspect.)
 ***************************************************************************/
PRIVATE int do_test_cache_cleared(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);
    reset_callback_state();

    set_expected_results(
        "cache: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );

    json_t *tranger = startup_master(path_root, FALSE);
    if(!tranger || create_topic(tranger) < 0) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    if(append_to(tranger, 1, 2) < 0 || append_to(tranger, 2, 2) < 0) {
        tranger2_shutdown(tranger);
        return -1;
    }
    result += test_json(NULL);

    set_expected_results(
        "cache: delete_key removes the entry from rollup cache",
        NULL, NULL, NULL, 1
    );

    json_t *topic = tranger2_topic(tranger, TOPIC_NAME);
    json_t *cache = json_object_get(topic, "cache");
    if(!json_object_get(cache, KEY_A) || !json_object_get(cache, KEY_B)) {
        printf("%sERROR%s --> cache: KEY_A/KEY_B missing before delete\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    if(tranger2_delete_key(tranger, TOPIC_NAME, KEY_A) < 0) {
        result += -1;
    }

    if(json_object_get(cache, KEY_A)) {
        printf("%sERROR%s --> cache: KEY_A still present after delete\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(!json_object_get(cache, KEY_B)) {
        printf("%sERROR%s --> cache: KEY_B vanished but should still be there\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }

    result += test_json(NULL);

    set_expected_results("cache: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
PRIVATE void quit_sighandler(int sig)
{
    static int xtimes_once = 0;
    xtimes_once++;
    yev_loop_reset_running(yev_loop);
    if(xtimes_once > 1) {
        exit(-1);
    }
}

PRIVATE void yuno_catch_signals(void)
{
    struct sigaction sigIntHandler;
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    memset(&sigIntHandler, 0, sizeof(sigIntHandler));
    sigIntHandler.sa_handler = quit_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = SA_NODEFER|SA_RESTART;
    sigaction(SIGALRM, &sigIntHandler, NULL);
    sigaction(SIGQUIT, &sigIntHandler, NULL);
    sigaction(SIGINT, &sigIntHandler, NULL);
}

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
    yuno_catch_signals();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("testing", 0, capture_log_write, 0);
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    yev_loop_create(0, 2024, 10, NULL, &yev_loop);

    int result = 0;
    result += do_test_rt_mem_specific_key();
    result += do_test_rt_mem_all_keys();
    result += do_test_rt_mem_filter_skip();
    result += do_test_rt_disk_in_process();
    result += do_test_follower();
    result += do_test_cache_cleared();

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
    }
    return result < 0? -1 : 0;
}
