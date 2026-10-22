/****************************************************************************
 *          test_rt_disk_emfile.c
 *
 *  A follower's rt_disk feed that runs out of descriptors (EMFILE) while it
 *  reads the key directories the master has just made: the master appends
 *  10 records of a new day to each of 60 keys, and the follower handles
 *  their events with two descriptors free. The directories it cannot open
 *  are left with their links, and the master links a file only once, so
 *  the key's next records in that file bring no event.
 *
 *  The reads that failed are OWED, and done at the end of the feed's next
 *  batch of events: once descriptors are free again, one more record of
 *  each key (no event: the links are there) and a record of a NEW key (an
 *  event) bring every key level with the master. Up to 7.27.6 the 60 keys
 *  stayed at their 2 records of the first day for good (13 on the master),
 *  until a new file of each key or a new feed.
 *
 *  The descriptors run out for real: the soft limit of open files is taken
 *  down to 1024 (if it is above) and filled with /dev/null, two left free.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/resource.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>

#define APP "test_rt_disk_emfile"

#define DATABASE    "tr_rt_disk_emfile"
#define TOPIC_NAME  "topic_rt_disk_emfile"
#define NKEYS       60
#define DAY1        946684800   // 2000-01-01T00:00:00+0000
#define DAY2        (DAY1 + 86400)
#define FREE_FDS    2
#define FILL_LIMIT  1024
#define MAX_FILL    4096

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE int feed_count[NKEYS + 1];
PRIVATE int fill_fds[MAX_FILL];
PRIVATE int n_fill = 0;
PRIVATE struct rlimit saved_limit;

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
    int k = atoi(key);
    if(k >= 0 && k <= NKEYS) {
        feed_count[k]++;
    }
    JSON_DECREF(record)
    return 0;
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
        "on_critical_error", 0,
        "filename_mask", "%Y",
        "xpermission" , 02770,
        "rpermission", 0600
    );
    return tranger2_startup(0, jn_tranger, yev_loop);
}

PRIVATE int append_one(json_t *tranger, json_int_t id, uint64_t t)
{
    json_t *jn_record = json_pack("{s:I, s:I, s:s}",
        "id", id,
        "tm", (json_int_t)t,
        "content", "payload"
    );
    md2_record_ex_t md = {0};
    return tranger2_append_record(tranger, TOPIC_NAME, t, 0, &md, jn_record);
}

PRIVATE void drain(int turns)
{
    for(int i = 0; i < turns; i++) {
        yev_loop_run_once(yev_loop);
    }
}

PRIVATE json_int_t served(json_t *tranger, int k)
{
    char key[32];
    snprintf(key, sizeof(key), "%019d", k);
    json_t *data = json_array();
    json_t *it = tranger2_open_iterator(
        tranger, TOPIC_NAME, key, json_object(), NULL, "served", "", data, NULL
    );
    json_int_t n = (json_int_t)json_array_size(data);
    if(it) {
        tranger2_close_iterator(tranger, it);
    }
    JSON_DECREF(data)
    return n;
}

/*
 *  The soft limit down to FILL_LIMIT (if it is above), filled up to
 *  FREE_FDS descriptors
 */
PRIVATE int exhaust_fds(void)
{
    if(getrlimit(RLIMIT_NOFILE, &saved_limit) < 0) {
        return -1;
    }
    struct rlimit low = saved_limit;
    if(low.rlim_cur > FILL_LIMIT) {
        low.rlim_cur = FILL_LIMIT;
        if(setrlimit(RLIMIT_NOFILE, &low) < 0) {
            return -1;
        }
    }
    while(n_fill < MAX_FILL) {
        int fd = open("/dev/null", O_RDONLY|O_CLOEXEC);
        if(fd < 0) {
            break;
        }
        fill_fds[n_fill++] = fd;
    }
    if(n_fill >= MAX_FILL) {
        return -1;
    }
    for(int i = 0; i < FREE_FDS && n_fill > 0; i++) {
        close(fill_fds[--n_fill]);
    }
    return 0;
}

PRIVATE void release_fds(void)
{
    while(n_fill > 0) {
        close(fill_fds[--n_fill]);
    }
    setrlimit(RLIMIT_NOFILE, &saved_limit);
}

/*
 *  Every key the follower serves and the feed handed, against the master
 */
PRIVATE int check_level(json_t *tm, json_t *tf, int nkeys, const int *expected_feed, const char *when)
{
    int bad = 0;
    for(int k = 0; k < nkeys; k++) {
        json_int_t ms = served(tm, k);
        json_int_t fs = served(tf, k);
        if(ms != fs || feed_count[k] != expected_feed[k]) {
            if(bad < 5) {
                printf("%sERROR%s --> %s: key %d served %d by the master, %d by the follower, %d handed by the feed (expected %d)\n",
                    On_Red BWhite, Color_Off, when, k, (int)ms, (int)fs, feed_count[k], expected_feed[k]);
            }
            bad++;
        }
    }
    if(bad) {
        printf("%sERROR%s --> %s: %d keys of %d not level with the master\n",
            On_Red BWhite, Color_Off, when, bad, nkeys);
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
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database),
        getenv("HOME"), "tests_yuneta", DATABASE, NULL);
    rmrdir(path_database);

    /*-------------------------------------*
     *  Master with 60 keys of 2 records,
     *  follower with a feed of every key
     *-------------------------------------*/
    set_expected_results(
        "emfile: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_tranger(TRUE);
    if(!tm) {
        return -1;
    }
    json_t *topic = tranger2_create_topic(
        tm, TOPIC_NAME, "id", "tm",
        json_pack("{s:s}",
            "filename_mask", "%Y-%m-%d"
        ),
        sf_int_key,
        json_pack("{s:s, s:I, s:s}",
            "id", "",
            "tm", (json_int_t)0,
            "content", ""
        ),
        0
    );
    for(int k = 0; topic && k < NKEYS; k++) {
        append_one(tm, k, DAY1);
        append_one(tm, k, DAY1 + 1);
    }
    json_t *tf = topic? startup_tranger(FALSE) : NULL;
    if(!tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        if(tf) {
            tranger2_shutdown(tf);
        }
        tranger2_shutdown(tm);
        return -1;
    }
    json_t *rt_all = tranger2_open_rt_disk(
        tf, TOPIC_NAME, "", NULL, my_record_callback, "rtALL", "", NULL
    );
    if(!rt_all) {
        result += -1;
    }
    drain(20);
    int expected_feed[NKEYS + 1] = {0};
    result += check_level(tm, tf, NKEYS, expected_feed, "before");
    result += test_json(NULL);

    /*-------------------------------------*
     *  10 records of a new day to each
     *  key, read with 2 descriptors free
     *-------------------------------------*/
    set_expected_results_unordered(
        "emfile: the reads that fail are owed",
        json_pack("[{s:s},{s:s},{s:s},{s:s}]",
            "msg", "Cannot open a directory to watch it through its descriptor: watched by its path (and the next ones that fail, counted)",
            "msg", "Cannot open a key directory to list its links",
            "msg", "Cannot open a key directory to read its links",
            "msg", "rt_disk feed: a key directory could not be read for a passing cause, its read is owed to the end of the next batch of events of the feed (the next ones not said)"
        ),
        NULL, NULL, 1
    );
    for(int j = 0; j < 10; j++) {
        for(int k = 0; k < NKEYS; k++) {
            append_one(tm, k, DAY2 + j);
        }
    }
    if(exhaust_fds() < 0) {
        printf("%sERROR%s --> emfile: the descriptors could not be exhausted\n",
            On_Red BWhite, Color_Off);
        release_fds();
        result += -1;
    }
    drain(60);
    release_fds();
    drain(20);
    /*
     *  Unordered: how many fail each way varies with what took the last
     *  descriptors. The first two directories take them for their watch
     *  and cannot be listed; the rest are watched by their path and cannot
     *  be opened. Each failure is logged, the debt said once.
     */
    result += test_json(NULL);

    /*-------------------------------------*
     *  Descriptors free: one record more
     *  of each key (its link is there: no
     *  event), then one of a NEW key, a
     *  batch of the feed
     *-------------------------------------*/
    set_expected_results(
        "emfile: the next batch reads what was owed",
        json_pack("[{s:s},{s:s}]",
            "msg", "Directories watched through their descriptor again",
            "msg", "rt_disk feed: no read of a key directory owed any more"
        ),
        NULL, NULL, 1
    );
    for(int k = 0; k < NKEYS; k++) {
        append_one(tm, k, DAY2 + 100);
    }
    drain(20);
    append_one(tm, NKEYS, DAY2 + 200);
    drain(60);
    for(int k = 0; k < NKEYS; k++) {
        expected_feed[k] = 11;
    }
    expected_feed[NKEYS] = 1;
    result += check_level(tm, tf, NKEYS + 1, expected_feed, "after");
    result += test_json(NULL);

    /*-------------------------------------*
     *  Shutdown
     *-------------------------------------*/
    set_expected_results("emfile: shutdown", NULL, NULL, NULL, 1);
    tranger2_close_rt_disk(tf, rt_all);
    tranger2_shutdown(tf);
    tranger2_shutdown(tm);
    drain(10);
    result += test_json(NULL);
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
    }
    return result < 0? -1 : 0;
}
