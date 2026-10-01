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
 *      - do_test_signal_dir_seen:     the master signals a delete to a feed
 *        without the key's directory by creating it and removing it; a
 *        follower that reads the IN_CREATE in between finds the directory
 *        there. That is not the key alive again: the feed's debt of the
 *        delete stays (the test holds the rmdir() of the master's signal).
 *        Nor when the master writes the key AGAIN before the follower reads
 *        that IN_CREATE: the directory then holds the new key's link, read
 *        at the signal's place; the debt stays, the live key stays in the
 *        follower's cache, and nobody is left owing.
 *      - do_test_reborn_before_read: the master deletes a key and writes it
 *        again before the follower reads the signal (one feed, two feeds,
 *        one new record or five): the feed hears the delete, then every
 *        record of the key born again, R1..Rn, and its cache keeps the
 *        key. Up to 7.25.20 the scan at the signal's IN_CREATE read the new
 *        key's file against the OLD key's cell: one new record was never
 *        handed, five were handed as R4 R5 (the next append handed R1..R6),
 *        and the delete heard after took the live key out of the cache.
 *      - do_test_stale_debt_reborn: a debt a feed will never pay (a delete
 *        signalled across its opening) is forgotten by the first record of
 *        the key born again, found by the scan of its new directory (its
 *        link made before the directory was watched).
 *      - do_test_master_rt_disk_reborn: a master's own rt_disk feed hears
 *        the delete after the master wrote the key again: the live key
 *        stays in the master's cache.
 *      - do_test_rkey_filter:         a feed opened with an `rkey` (a
 *        regular expression over the keys) is told the deletes of the keys
 *        it matches and of no other: an rt_mem feed of the master and an
 *        rt_disk feed of a follower. Up to 7.25.20 the delete looked at
 *        the exact `key` only, and an rkey feed heard every key deleted.
 *      - do_test_rmrdir_fails:        a key whose directory cannot be removed
 *        is NOT announced deleted: the notices go out after the rmrdir.
 *      - do_test_rmrdir_fails_filtered: a delete that fails with no file
 *        removed leaves a FILTERED paging iterator of the key its rows.
 *        Up to 7.25.4 the failure emptied its index (a filtered index
 *        is built only at the open): total_rows 0 for a key still on disk.
 *      - do_test_key_dir_unstatable: a key whose directory cannot be
 *        stat'ed (EIO, EACCES on keys/) is not "not found": the delete
 *        answers -1, announces nothing, and the key keeps its records.
 *        Up to 7.25.4 it was taken as not found: 0, the key dropped from
 *        the cache, the delete announced, and the files left on disk.
 *      - do_test_mirror_fails:       the delete cannot list disks/ (its
 *        opendir() or its readdir() fails): the feeds of the replicas are
 *        not told, and that is logged. Up to 7.25.4 it was silent.
 *
 *  The failures of stat(), opendir() and readdir() are made by the
 *  __wrap_*() below (the test links with --wrap=stat,opendir,readdir), for
 *  the one path each is told to fail; __wrap_rmdir() holds the master's
 *  delete signals for do_test_signal_dir_seen.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>

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
 *              A stat(), opendir(), readdir() that fail
 ***************************************************************/
int __real_stat(const char *path, struct stat *st);
int __wrap_stat(const char *path, struct stat *st);
DIR *__real_opendir(const char *name);
DIR *__wrap_opendir(const char *name);
struct dirent *__real_readdir(DIR *dirp);
struct dirent *__wrap_readdir(DIR *dirp);

PRIVATE char failing_stat[PATH_MAX] = "";      // the path whose stat() fails (EIO)
PRIVATE char failing_opendir[PATH_MAX] = "";   // the directory whose opendir() fails (EMFILE)
PRIVATE char failing_readdir[PATH_MAX] = "";   // the directory whose readdir() fails (EIO)
PRIVATE DIR *failing_dirp = NULL;
PRIVATE int wrapped_failures = 0;

int __real_rmdir(const char *path);
int __wrap_rmdir(const char *path);

PRIVATE yev_loop_h yev_loop;
PRIVATE char held_signals[PATH_MAX] = "";      // the disks/ whose delete signals of KEY_A are held
PRIVATE int held_rmdirs = 0;
PRIVATE int count_x = 0, count_y = 0;           // key_deleted of the feeds rtX and rtY
PRIVATE json_t *feed_x = NULL, *feed_y = NULL;
PRIVATE int debt_kept_in_signal = -1;           // the second feed, its IN_CREATE read: still owes?
PRIVATE json_t *held_master = NULL;             // set: the key is written again in the hold
PRIVATE int append_to(json_t *tranger, json_int_t id, int n);

int __wrap_rmdir(const char *path)
{
    size_t lp = strlen(held_signals);
    size_t lk = strlen(KEY_A);
    size_t l = strlen(path);
    if(lp && strncmp(path, held_signals, lp)==0 && path[lp]=='/' &&
            l > lk && strcmp(path + l - lk, KEY_A)==0) {
        held_rmdirs++;
        if(held_rmdirs == 1) {
            /*
             *  The first feed signalled: it hears the delete, the other
             *  one now owes it
             */
            int ret = __real_rmdir(path);
            for(int i = 0; i < 200 && count_x + count_y == 0; i++) {
                yev_loop_run_once(yev_loop);
            }
            return ret;
        }
        if(held_rmdirs == 2) {
            /*
             *  The second: its directory made and not removed yet, as when
             *  the master is preempted between the two. Or removed, and the
             *  key written again (its new directory and link) before the
             *  follower reads the signal.
             */
            int ret = 0;
            if(held_master) {
                ret = __real_rmdir(path);
                if(append_to(held_master, 1, 1) < 0) {
                    printf("%sERROR%s --> cannot write the key again\n", On_Red BWhite, Color_Off);
                }
            }
            for(int i = 0; i < 20; i++) {
                yev_loop_run_once(yev_loop);
            }
            json_t *feed = strstr(path, "/rtX/")? feed_x : feed_y;
            debt_kept_in_signal = json_object_get(
                json_object_get(feed, "deletes_unheard"), KEY_A
            )? 1 : 0;
            if(held_master) {
                return ret;
            }
        }
    }
    return __real_rmdir(path);
}

int __wrap_stat(const char *path, struct stat *st)
{
    if(failing_stat[0] && strcmp(path, failing_stat) == 0) {
        wrapped_failures++;
        errno = EIO;
        return -1;
    }
    return __real_stat(path, st);
}

DIR *__wrap_opendir(const char *name)
{
    if(failing_opendir[0] && strcmp(name, failing_opendir) == 0) {
        wrapped_failures++;
        errno = EMFILE;
        return NULL;
    }
    DIR *dirp = __real_opendir(name);
    if(dirp && failing_readdir[0] && strcmp(name, failing_readdir) == 0) {
        failing_dirp = dirp;
    }
    return dirp;
}

struct dirent *__wrap_readdir(DIR *dirp)
{
    if(dirp && dirp == failing_dirp) {
        failing_dirp = NULL;
        wrapped_failures++;
        errno = EIO;
        return NULL;
    }
    return __real_readdir(dirp);
}

/***************************************************************
 *              Data
 ***************************************************************/
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
 *  do_test_rmrdir_fails
 *  The key directory holds a directory that cannot be emptied: the delete
 *  fails, and nobody hears it as done. Once it can be removed, the delete
 *  goes through and is heard once.
 ***************************************************************************/
PRIVATE int do_test_rmrdir_fails(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);
    reset_callback_state();

    set_expected_results(
        "rmrdir_fails: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = startup_master(path_root, FALSE);
    if(!tranger || create_topic(tranger) < 0 || append_to(tranger, 1, 3) < 0) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    json_t *rt = tranger2_open_rt_mem(
        tranger, TOPIC_NAME, KEY_A, NULL, my_record_callback, "rmrdir", "", NULL
    );
    tranger2_set_rt_key_deleted_callback(rt, my_key_deleted_callback, NULL);
    result += test_json(NULL);

    set_expected_results_unordered(
        "rmrdir_fails: a delete that cannot remove the directory is not announced",
        json_pack("[{s:s},{s:s}]",
            "msg", "remove() FAILED",
            "msg", "Cannot delete subdir key. rmrdir() FAILED"
        ),
        NULL, NULL, 1
    );
    char blocker[PATH_MAX];
    char blocked[PATH_MAX];
    build_path(blocker, sizeof(blocker), path_topic, "keys", KEY_A, "blocker", NULL);
    build_path(blocked, sizeof(blocked), blocker, "file", NULL);
    mkdir(blocker, 0700);
    FILE *f = fopen(blocked, "w");
    if(f) {
        fclose(f);
    }
    chmod(blocker, 0500);
    if(tranger2_delete_key(tranger, TOPIC_NAME, KEY_A) == 0) {
        printf("%sERROR%s --> rmrdir_fails: the delete answered done\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(deleted_callback_count != 0) {
        printf("%sERROR%s --> rmrdir_fails: a failed delete was announced %zu time(s)\n",
            On_Red BWhite, Color_Off, deleted_callback_count);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("rmrdir_fails: the delete once it can be done", NULL, NULL, NULL, 1);
    chmod(blocker, 0700);
    if(tranger2_delete_key(tranger, TOPIC_NAME, KEY_A) < 0) {
        printf("%sERROR%s --> rmrdir_fails: the second delete failed\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(deleted_callback_count != 1) {
        printf("%sERROR%s --> rmrdir_fails: expected 1 fire, got %zu\n",
            On_Red BWhite, Color_Off, deleted_callback_count);
        result += -1;
    }
    tranger2_close_rt_mem(tranger, rt);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  do_test_rmrdir_fails_filtered
 *  The key directory is read-only: rmrdir() cannot remove any file, and the
 *  delete fails. A FILTERED paging iterator opened before it keeps its
 *  rows: its index is built again from the key's cache, read again from
 *  the disk. Skipped as root, who removes the files anyway.
 ***************************************************************************/
PRIVATE int do_test_rmrdir_fails_filtered(void)
{
    int result = 0;
    if(geteuid() == 0) {
        printf("  rmrdir_fails_filtered: skipped as root\n");
        return 0;
    }
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);
    reset_callback_state();

    set_expected_results(
        "rmrdir_fails_filtered: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = startup_master(path_root, FALSE);
    if(!tranger || create_topic(tranger) < 0 || append_to(tranger, 1, 3) < 0) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    json_t *iterator = tranger2_open_iterator(
        tranger, TOPIC_NAME, KEY_A,
        json_pack("{s:I}", "from_t", (json_int_t)BASE_T),   // filtered: it builds an index
        NULL, "filtered", "rmrdir", NULL, NULL
    );
    json_t *page = tranger2_iterator_get_page(tranger, iterator, 1, 10, FALSE);
    if(kw_get_int(0, page, "total_rows", -1, 0) != 3) {
        printf("%sERROR%s --> rmrdir_fails_filtered: %d rows before the delete, expected 3\n",
            On_Red BWhite, Color_Off, (int)kw_get_int(0, page, "total_rows", -1, 0));
        result += -1;
    }
    JSON_DECREF(page)
    result += test_json(NULL);

    set_expected_results(
        "rmrdir_fails_filtered: a delete that removes nothing fails",
        json_pack("[{s:s},{s:s}]",
            "msg", "remove() FAILED",
            "msg", "Cannot delete subdir key. rmrdir() FAILED"
        ),
        NULL, NULL, 1
    );
    char key_dir[PATH_MAX];
    build_path(key_dir, sizeof(key_dir), path_topic, "keys", KEY_A, NULL);
    chmod(key_dir, 0550);   // its files cannot be unlinked
    if(tranger2_delete_key(tranger, TOPIC_NAME, KEY_A) == 0) {
        printf("%sERROR%s --> rmrdir_fails_filtered: the delete answered done\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    chmod(key_dir, 02700);
    result += test_json(NULL);

    set_expected_results("rmrdir_fails_filtered: the filtered iterator keeps its rows", NULL, NULL, NULL, 1);
    page = tranger2_iterator_get_page(tranger, iterator, 1, 10, FALSE);
    json_int_t total_rows = kw_get_int(0, page, "total_rows", -1, 0);
    size_t data = json_array_size(kw_get_list(0, page, "data", 0, 0));
    if(total_rows != 3 || data != 3) {
        printf("%sERROR%s --> rmrdir_fails_filtered: the filtered iterator after the failed delete: "
            "total_rows %d, %d rows, expected 3 and 3\n",
            On_Red BWhite, Color_Off, (int)total_rows, (int)data);
        result += -1;
    }
    JSON_DECREF(page)
    tranger2_close_iterator(tranger, iterator);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    return result;
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
PRIVATE int count_a = 0, count_b = 0, count_all = 0, count_close = 0, count_rkey = 0;
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
    } else if(id && strcmp(id, "rtRKEY")==0) {
        count_rkey++;
    } else if(id && strcmp(id, "rtX")==0) {
        count_x++;
    } else if(id && strcmp(id, "rtY")==0) {
        count_y++;
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
 *  do_test_signal_dir_seen
 ***************************************************************************/
PRIVATE int do_test_signal_dir_seen(BOOL key_back)
{
    int result = 0;
    const char *label = key_back? "signal dir seen, key back" : "signal dir seen";
    char title[128];
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);
    count_x = count_y = 0;
    held_rmdirs = 0;
    debt_kept_in_signal = -1;

    snprintf(title, sizeof(title), "%s: setup", label);
    set_expected_results(
        title,
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
    if(append_to(tm, 1, 1) < 0) {  // KEY_A, before the feeds: neither has its directory
        result += -1;
    }
    drain(5);
    json_t *tf = startup_tranger(path_root, FALSE, TRUE);
    if(!tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        if(tf) {
            tranger2_shutdown(tf);
        }
        tranger2_shutdown(tm);
        return -1;
    }
    feed_x = tranger2_open_rt_disk(tf, TOPIC_NAME, "", NULL, my_record_callback, "rtX", "", NULL);
    feed_y = tranger2_open_rt_disk(tf, TOPIC_NAME, "", NULL, my_record_callback, "rtY", "", NULL);
    if(!feed_x || !feed_y) {
        result += -1;
    }
    tranger2_set_rt_key_deleted_callback(feed_x, follower_key_deleted_callback, NULL);
    tranger2_set_rt_key_deleted_callback(feed_y, follower_key_deleted_callback, NULL);
    drain(10);
    result += test_json(NULL);

    snprintf(title, sizeof(title), "%s: the debt stays", label);
    set_expected_results(title, NULL, NULL, NULL, 1);
    build_path(held_signals, sizeof(held_signals), path_topic, "disks", NULL);
    held_master = key_back? tm : NULL;
    if(tranger2_delete_key(tm, TOPIC_NAME, KEY_A) < 0) {
        result += -1;
    }
    held_signals[0] = 0;
    held_master = NULL;
    drain(30);
    if(key_back && !json_object_get(json_object_get(tranger2_topic(tf, TOPIC_NAME), "cache"), KEY_A)) {
        printf("%sERROR%s --> %s: the key written again is not in the follower's cache\n",
            On_Red BWhite, Color_Off, label);
        result += -1;
    }

    if(held_rmdirs != 2) {
        printf("%sERROR%s --> %s: %d signals held, expected 2: the test did not test\n",
            On_Red BWhite, Color_Off, label, held_rmdirs);
        result += -1;
    }
    if(!key_back && debt_kept_in_signal != 1) {     // with the key back the signal is read whole
        printf("%sERROR%s --> %s: the directory of the signal, seen made, dropped the debt of the delete\n",
            On_Red BWhite, Color_Off, label);
        result += -1;
    }
    if(count_x != 1 || count_y != 1) {
        printf("%sERROR%s --> %s: rtX heard %d, rtY %d, expected 1/1\n",
            On_Red BWhite, Color_Off, label, count_x, count_y);
        result += -1;
    }
    if(json_object_size(json_object_get(feed_x, "deletes_unheard")) != 0 ||
       json_object_size(json_object_get(feed_y, "deletes_unheard")) != 0) {
        printf("%sERROR%s --> %s: a feed owes deletes it will never hear\n",
            On_Red BWhite, Color_Off, label);
        result += -1;
    }
    tranger2_close_rt_disk(tf, feed_x);
    tranger2_close_rt_disk(tf, feed_y);
    feed_x = feed_y = NULL;
    drain(20);
    result += test_json(NULL);

    snprintf(title, sizeof(title), "%s: shutdown", label);
    set_expected_results(title, NULL, NULL, NULL, 1);
    tranger2_shutdown(tf);
    tranger2_shutdown(tm);
    drain(10);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  do_test_reborn_before_read
 ***************************************************************************/
PRIVATE char seq_x[1024];   // what rtX was told, in order
PRIVATE int seq_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record
)
{
    char b[32];
    snprintf(b, sizeof(b), "R%lld ", (long long)rowid);
    strncat(seq_x, b, sizeof(seq_x) - strlen(seq_x) - 1);
    JSON_DECREF(record)
    return 0;
}

PRIVATE int seq_key_deleted_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    void *user_data
)
{
    strncat(seq_x, "DEL ", sizeof(seq_x) - strlen(seq_x) - 1);
    return 0;
}

PRIVATE int append_one_at(json_t *tranger, json_int_t id, uint64_t t)
{
    json_t *jn_record = json_pack("{s:I, s:I, s:s}",
        "id", id,
        "tm", (json_int_t)t,
        "content", "payload"
    );
    md2_record_ex_t md = {0};
    return tranger2_append_record(tranger, TOPIC_NAME, t, 0, &md, jn_record);
}

PRIVATE int do_test_reborn_before_read(int new_rows, BOOL two_feeds)
{
    int result = 0;
    char label[64];
    snprintf(label, sizeof(label), "reborn before read, %d new, %s",
        new_rows, two_feeds? "two feeds" : "one feed");
    char title[128];
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);
    seq_x[0] = 0;
    count_x = count_y = 0;

    snprintf(title, sizeof(title), "%s: setup", label);
    set_expected_results(
        title,
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
    if(append_to(tm, 1, 3) < 0) {
        result += -1;
    }
    drain(5);
    json_t *tf = startup_tranger(path_root, FALSE, TRUE);
    if(!tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        if(tf) {
            tranger2_shutdown(tf);
        }
        tranger2_shutdown(tm);
        return -1;
    }
    json_t *fx = tranger2_open_rt_disk(tf, TOPIC_NAME, "", NULL, seq_record_callback, "rtX", "", NULL);
    json_t *fy = NULL;
    if(two_feeds) {
        fy = tranger2_open_rt_disk(tf, TOPIC_NAME, "", NULL, my_record_callback, "rtY", "", NULL);
        tranger2_set_rt_key_deleted_callback(fy, follower_key_deleted_callback, NULL);
    }
    if(!fx || (two_feeds && !fy)) {
        result += -1;
    }
    tranger2_set_rt_key_deleted_callback(fx, seq_key_deleted_callback, NULL);
    drain(10);
    result += test_json(NULL);

    /*
     *  With the loop stopped: the key deleted, then written again
     */
    snprintf(title, sizeof(title), "%s: the delete, then the new key", label);
    set_expected_results(title, NULL, NULL, NULL, 1);
    if(tranger2_delete_key(tm, TOPIC_NAME, KEY_A) < 0) {
        result += -1;
    }
    for(int i = 0; i < new_rows; i++) {
        if(append_one_at(tm, 1, BASE_T + 10 + i) < 0) {
            result += -1;
        }
    }
    drain(30);
    char expected[256] = "DEL ";
    for(int i = 1; i <= new_rows; i++) {
        char b[16];
        snprintf(b, sizeof(b), "R%d ", i);
        strncat(expected, b, sizeof(expected) - strlen(expected) - 1);
    }
    if(strcmp(seq_x, expected) != 0) {
        printf("%sERROR%s --> %s: rtX was told [%s], expected [%s]\n",
            On_Red BWhite, Color_Off, label, seq_x, expected);
        result += -1;
    }
    json_t *cache = json_object_get(tranger2_topic(tf, TOPIC_NAME), "cache");
    if(!json_object_get(cache, KEY_A)) {
        printf("%sERROR%s --> %s: the key written again is not in the follower's cache\n",
            On_Red BWhite, Color_Off, label);
        result += -1;
    }
    if(json_object_size(json_object_get(fx, "deletes_unheard")) != 0 ||
       (fy && json_object_size(json_object_get(fy, "deletes_unheard")) != 0)) {
        printf("%sERROR%s --> %s: a feed owes deletes it will never hear\n",
            On_Red BWhite, Color_Off, label);
        result += -1;
    }

    /*
     *  The next record of the key: once, with the next rowid
     */
    seq_x[0] = 0;
    if(append_one_at(tm, 1, BASE_T + 100) < 0) {
        result += -1;
    }
    drain(30);
    snprintf(expected, sizeof(expected), "R%d ", new_rows + 1);
    if(strcmp(seq_x, expected) != 0) {
        printf("%sERROR%s --> %s: the next record: rtX was told [%s], expected [%s]\n",
            On_Red BWhite, Color_Off, label, seq_x, expected);
        result += -1;
    }
    tranger2_close_rt_disk(tf, fx);
    if(fy) {
        tranger2_close_rt_disk(tf, fy);
    }
    drain(10);
    result += test_json(NULL);

    snprintf(title, sizeof(title), "%s: shutdown", label);
    set_expected_results(title, NULL, NULL, NULL, 1);
    tranger2_shutdown(tf);
    tranger2_shutdown(tm);
    drain(10);
    result += test_json(NULL);
    count_x = count_y = 0;
    return result;
}

/***************************************************************************
 *  do_test_stale_debt_reborn
 ***************************************************************************/
PRIVATE int do_test_stale_debt_reborn(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);
    seq_x[0] = 0;
    count_x = count_y = 0;

    set_expected_results(
        "stale debt reborn: setup",
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
    if(append_to(tm, 1, 1) < 0) {
        result += -1;
    }
    drain(5);
    json_t *tf = startup_tranger(path_root, FALSE, TRUE);
    if(!tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        if(tf) {
            tranger2_shutdown(tf);
        }
        tranger2_shutdown(tm);
        return -1;
    }
    json_t *fx = tranger2_open_rt_disk(tf, TOPIC_NAME, "", NULL, seq_record_callback, "rtX", "", NULL);
    json_t *fy = tranger2_open_rt_disk(tf, TOPIC_NAME, "", NULL, my_record_callback, "rtY", "", NULL);
    if(!fx || !fy) {
        result += -1;
    }
    tranger2_set_rt_key_deleted_callback(fx, seq_key_deleted_callback, NULL);
    tranger2_set_rt_key_deleted_callback(fy, follower_key_deleted_callback, NULL);
    drain(10);
    result += test_json(NULL);

    set_expected_results("stale debt reborn: the first record forgets it", NULL, NULL, NULL, 1);
    /*
     *  A debt of the window of doubt (a delete signalled across rtX's
     *  opening), put there by hand; then a record of the key: rtX's
     *  directory of it is new, its first link found by the scan
     */
    json_object_set_new(fx, "deletes_unheard", json_pack("{s:[i]}", KEY_A, 0));
    if(append_one_at(tm, 1, BASE_T + 10) < 0) {
        result += -1;
    }
    drain(30);
    if(json_object_size(json_object_get(fx, "deletes_unheard")) != 0) {
        printf("%sERROR%s --> stale debt reborn: rtX still owes after the record of the key born again\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(tranger2_delete_key(tm, TOPIC_NAME, KEY_A) < 0) {
        result += -1;
    }
    drain(30);
    if(json_object_size(json_object_get(fx, "deletes_unheard")) != 0 ||
       json_object_size(json_object_get(fy, "deletes_unheard")) != 0) {
        printf("%sERROR%s --> stale debt reborn: a feed owes deletes it will never hear\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    tranger2_close_rt_disk(tf, fx);
    tranger2_close_rt_disk(tf, fy);
    drain(10);
    result += test_json(NULL);

    set_expected_results("stale debt reborn: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tf);
    tranger2_shutdown(tm);
    drain(10);
    result += test_json(NULL);
    count_x = count_y = 0;
    return result;
}

/***************************************************************************
 *  do_test_master_rt_disk_reborn
 *  A master's own rt_disk feed hears a delete after the master wrote the
 *  key again: the live key stays in the master's cache.
 ***************************************************************************/
PRIVATE int do_test_master_rt_disk_reborn(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);
    reset_callback_state();

    set_expected_results(
        "master rt_disk reborn: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = startup_master(path_root, TRUE);
    if(!tranger || create_topic(tranger) < 0) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    result += test_json(NULL);

    set_expected_results("master rt_disk reborn: the live key stays", NULL, NULL, NULL, 1);
    json_t *rt = tranger2_open_rt_disk(
        tranger, TOPIC_NAME, "", NULL, my_record_callback, "disk-own", "", NULL
    );
    if(!rt) {
        result += -1;
    }
    tranger2_set_rt_key_deleted_callback(rt, my_key_deleted_callback, NULL);
    if(append_to(tranger, 1, 3) < 0) {
        result += -1;
    }
    drain(10);

    /*
     *  With the loop stopped: the key deleted and written again, then the
     *  feed's watcher hears the delete
     */
    if(tranger2_delete_key(tranger, TOPIC_NAME, KEY_A) < 0 || append_to(tranger, 1, 1) < 0) {
        result += -1;
    }
    drain(20);
    json_t *cache = json_object_get(tranger2_topic(tranger, TOPIC_NAME), "cache");
    if(!json_object_get(cache, KEY_A)) {
        printf("%sERROR%s --> master rt_disk reborn: the key written again is not in the master's cache\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(deleted_callback_count != 1) {
        printf("%sERROR%s --> master rt_disk reborn: %zu fires, expected 1\n",
            On_Red BWhite, Color_Off, deleted_callback_count);
        result += -1;
    }
    tranger2_close_rt_disk(tranger, rt);
    drain(10);
    result += test_json(NULL);

    set_expected_results("master rt_disk reborn: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tranger);
    drain(10);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  do_test_rkey_filter
 *  Feeds whose rkey matches KEY_A only: KEY_B deleted, they hear nothing;
 *  KEY_A deleted, each hears it once.
 ***************************************************************************/
PRIVATE int do_test_rkey_filter(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);
    reset_callback_state();
    count_a = count_b = count_all = count_rkey = 0;

    set_expected_results(
        "rkey: setup",
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
    if(append_to(tm, 1, 1) < 0 || append_to(tm, 2, 1) < 0) {
        result += -1;
    }
    drain(5);
    result += test_json(NULL);

    set_expected_results("rkey: a feed hears the deletes of the keys it matches", NULL, NULL, NULL, 1);

    json_t *rt_mem = tranger2_open_rt_mem(
        tm, TOPIC_NAME, "",
        json_pack("{s:s}", "rkey", "1$"),
        my_record_callback,
        "rkey_mem",
        "", NULL
    );
    tranger2_set_rt_key_deleted_callback(rt_mem, my_key_deleted_callback, NULL);

    json_t *tf = startup_tranger(path_root, FALSE, TRUE);
    if(!rt_mem || !tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        printf("%sERROR%s --> rkey: cannot open the feeds\n", On_Red BWhite, Color_Off);
        if(tf) {
            tranger2_shutdown(tf);
        }
        tranger2_shutdown(tm);
        return -1;
    }
    json_t *rt_rkey = tranger2_open_rt_disk(
        tf, TOPIC_NAME, "", json_pack("{s:s}", "rkey", "1$"), my_record_callback, "rtRKEY", "", NULL
    );
    json_t *rt_all = tranger2_open_rt_disk(tf, TOPIC_NAME, "", NULL, my_record_callback, "rtALL", "", NULL);
    if(!rt_rkey || !rt_all) {
        result += -1;
    }
    tranger2_set_rt_key_deleted_callback(rt_rkey, follower_key_deleted_callback, NULL);
    tranger2_set_rt_key_deleted_callback(rt_all, follower_key_deleted_callback, NULL);
    drain(10);

    if(tranger2_delete_key(tm, TOPIC_NAME, KEY_B) < 0) {
        result += -1;
    }
    drain(30);
    if(deleted_callback_count != 0 || count_rkey != 0 || count_all != 1) {
        printf("%sERROR%s --> rkey, delete of KEY_B: rt_mem %zu, rt_disk rkey %d, rt_disk all %d, expected 0/0/1\n",
            On_Red BWhite, Color_Off, deleted_callback_count, count_rkey, count_all);
        result += -1;
    }
    reset_callback_state();
    count_rkey = count_all = 0;

    if(tranger2_delete_key(tm, TOPIC_NAME, KEY_A) < 0) {
        result += -1;
    }
    drain(30);
    if(deleted_callback_count != 1 || count_rkey != 1 || count_all != 1) {
        printf("%sERROR%s --> rkey, delete of KEY_A: rt_mem %zu, rt_disk rkey %d, rt_disk all %d, expected 1/1/1\n",
            On_Red BWhite, Color_Off, deleted_callback_count, count_rkey, count_all);
        result += -1;
    }
    if(json_object_size(json_object_get(rt_rkey, "deletes_unheard")) != 0 ||
       json_object_size(json_object_get(rt_all, "deletes_unheard")) != 0) {
        printf("%sERROR%s --> rkey: a feed owes deletes it will never hear\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    count_a = count_b = count_all = count_rkey = 0;

    tranger2_close_rt_mem(tm, rt_mem);
    tranger2_close_rt_disk(tf, rt_rkey);
    tranger2_close_rt_disk(tf, rt_all);
    drain(20);
    result += test_json(NULL);

    set_expected_results("rkey: shutdown", NULL, NULL, NULL, 1);
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
 *  do_test_key_dir_unstatable
 *  The stat() of keys/<key> fails with EIO: the delete does not know if
 *  the key is there, so it deletes nothing and announces nothing.
 ***************************************************************************/
PRIVATE int do_test_key_dir_unstatable(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);
    reset_callback_state();

    set_expected_results(
        "unstatable: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = startup_master(path_root, FALSE);
    if(!tranger || create_topic(tranger) < 0 || append_to(tranger, 1, 3) < 0) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    json_t *rt = tranger2_open_rt_mem(
        tranger, TOPIC_NAME, "", NULL, my_record_callback, "unstatable", "", NULL
    );
    tranger2_set_rt_key_deleted_callback(rt, my_key_deleted_callback, NULL);
    result += test_json(NULL);

    set_expected_results(
        "unstatable: a key whose directory cannot be stat'ed is not deleted",
        json_pack("[{s:s}]",
            "msg", "Cannot delete key, stat() of its directory FAILED"
        ),
        NULL, NULL, 1
    );
    char key_dir[PATH_MAX];
    build_path(key_dir, sizeof(key_dir), path_topic, "keys", KEY_A, NULL);
    snprintf(failing_stat, sizeof(failing_stat), "%s", key_dir);
    wrapped_failures = 0;
    int ret = tranger2_delete_key(tranger, TOPIC_NAME, KEY_A);
    failing_stat[0] = 0;
    if(wrapped_failures == 0) {
        printf("%sERROR%s --> unstatable: no stat() failed, the test proves nothing\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(ret != -1) {
        printf("%sERROR%s --> unstatable: the delete answered %d, expected -1\n",
            On_Red BWhite, Color_Off, ret);
        result += -1;
    }
    if(deleted_callback_count != 0) {
        printf("%sERROR%s --> unstatable: a delete that did not happen was announced %zu time(s)\n",
            On_Red BWhite, Color_Off, deleted_callback_count);
        result += -1;
    }
    if(tranger2_topic_key_size(tranger, TOPIC_NAME, KEY_A) != 3) {
        printf("%sERROR%s --> unstatable: the key lost its records in memory: %d\n",
            On_Red BWhite, Color_Off, (int)tranger2_topic_key_size(tranger, TOPIC_NAME, KEY_A));
        result += -1;
    }
    if(!is_directory(key_dir)) {
        printf("%sERROR%s --> unstatable: the key directory is gone\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("unstatable: the delete once it can be done", NULL, NULL, NULL, 1);
    if(tranger2_delete_key(tranger, TOPIC_NAME, KEY_A) < 0) {
        printf("%sERROR%s --> unstatable: the second delete failed\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(deleted_callback_count != 1) {
        printf("%sERROR%s --> unstatable: expected 1 fire, got %zu\n",
            On_Red BWhite, Color_Off, deleted_callback_count);
        result += -1;
    }
    tranger2_close_rt_mem(tranger, rt);
    tranger2_shutdown(tranger);
    result += test_json(NULL);
    return result;
}

/***************************************************************************
 *  do_test_mirror_fails
 *  The delete is done, and disks/ cannot be listed to tell the feeds of
 *  the replicas: first its opendir() fails, then its readdir(). Each is
 *  logged; the in-process subscribers are told anyway.
 ***************************************************************************/
PRIVATE int do_test_mirror_fails(void)
{
    int result = 0;
    char path_root[PATH_MAX], path_database[PATH_MAX], path_topic[PATH_MAX];
    build_paths(path_root, sizeof(path_root),
                path_database, sizeof(path_database),
                path_topic, sizeof(path_topic));
    rmrdir(path_database);
    reset_callback_state();

    set_expected_results(
        "mirror_fails: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tranger = startup_master(path_root, FALSE);
    if(!tranger || create_topic(tranger) < 0 ||
            append_to(tranger, 1, 2) < 0 || append_to(tranger, 2, 2) < 0) {
        if(tranger) {
            tranger2_shutdown(tranger);
        }
        return -1;
    }
    json_t *rt = tranger2_open_rt_mem(
        tranger, TOPIC_NAME, "", NULL, my_record_callback, "mirror", "", NULL
    );
    tranger2_set_rt_key_deleted_callback(rt, my_key_deleted_callback, NULL);
    result += test_json(NULL);

    char disks_dir[PATH_MAX];
    build_path(disks_dir, sizeof(disks_dir), path_topic, "disks", NULL);

    set_expected_results(
        "mirror_fails: disks/ cannot be opened",
        json_pack("[{s:s}]",
            "msg", "Cannot tell the rt_disk feeds that a key was deleted, opendir() of disks/ FAILED"
        ),
        NULL, NULL, 1
    );
    snprintf(failing_opendir, sizeof(failing_opendir), "%s", disks_dir);
    wrapped_failures = 0;
    int ret = tranger2_delete_key(tranger, TOPIC_NAME, KEY_A);
    failing_opendir[0] = 0;
    if(wrapped_failures != 1) {
        printf("%sERROR%s --> mirror_fails: no opendir() failed, the test proves nothing\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(ret != 0 || deleted_callback_count != 1) {
        printf("%sERROR%s --> mirror_fails: the delete is done: ret %d, fired %zu\n",
            On_Red BWhite, Color_Off, ret, deleted_callback_count);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results(
        "mirror_fails: disks/ cannot be read",
        json_pack("[{s:s}]",
            "msg", "Cannot tell every rt_disk feed that a key was deleted, readdir() of disks/ FAILED"
        ),
        NULL, NULL, 1
    );
    snprintf(failing_readdir, sizeof(failing_readdir), "%s", disks_dir);
    wrapped_failures = 0;
    ret = tranger2_delete_key(tranger, TOPIC_NAME, KEY_B);
    failing_readdir[0] = 0;
    failing_dirp = NULL;
    if(wrapped_failures != 1) {
        printf("%sERROR%s --> mirror_fails: no readdir() failed, the test proves nothing\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    if(ret != 0 || deleted_callback_count != 2) {
        printf("%sERROR%s --> mirror_fails: the delete is done: ret %d, fired %zu\n",
            On_Red BWhite, Color_Off, ret, deleted_callback_count);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("mirror_fails: shutdown", NULL, NULL, NULL, 1);
    tranger2_close_rt_mem(tranger, rt);
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
    result += do_test_rkey_filter();
    result += do_test_signal_dir_seen(FALSE);
    result += do_test_signal_dir_seen(TRUE);
    result += do_test_master_rt_disk_reborn();
    result += do_test_reborn_before_read(1, FALSE);
    result += do_test_reborn_before_read(5, FALSE);
    result += do_test_reborn_before_read(1, TRUE);
    result += do_test_stale_debt_reborn();
    result += do_test_rmrdir_fails();
    result += do_test_rmrdir_fails_filtered();
    result += do_test_key_dir_unstatable();
    result += do_test_mirror_fails();

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
