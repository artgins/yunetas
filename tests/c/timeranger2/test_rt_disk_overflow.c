/****************************************************************************
 *          test_rt_disk_overflow.c
 *
 *  An rt_disk feed of a follower whose inotify queue OVERFLOWS.
 *
 *  The master hard-links each new md2 into disks/<rt_id>/<key>/ and the
 *  follower hears it through inotify. When the kernel's queue of the
 *  watcher is full (fs.inotify.max_queued_events), further events are
 *  dropped and an IN_Q_OVERFLOW says so. Up to 7.25.8 fs_watcher then
 *  aborted the yuno to reload clean; under a sustained burst the reload met
 *  the next overflow and a follower lived in a crash loop (yunovatios'
 *  stress test of its central: five aborts in two hours). Now the watcher
 *  sets its watches again and the follower rebuilds the feed from the
 *  filesystem: what the lost events said is still there.
 *
 *  The overflow is real, not simulated: with the loop NOT running, the
 *  master appends one record to more new keys than the queue holds (the
 *  limit is read from /proc), so every key directory is one IN_CREATE and
 *  the last ones are dropped. Then:
 *
 *      - every record reaches the feed EXACTLY once (the ones heard, and the
 *        ones found by the rescan);
 *      - a key deleted while the queue was full -- its signal lost with the
 *        rest -- is heard: the key_deleted callback fires once;
 *      - a key born while the queue was full is WATCHED afterwards: its
 *        next record arrives.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>
#include <fs_watcher.h>

#define APP "test_rt_disk_overflow"

#define DATABASE    "tr_rt_disk_overflow"
#define TOPIC_NAME  "topic_rt_disk_overflow"
#define BASE_T      946684800   // 2000-01-01T00:00:00+0000
#define SEED_KEY_ID 0           // exists before the flood; deleted during it
#define EXTRA_KEYS  4096        // beyond the queue's limit
#define FDS_PER_KEY 4           // files timeranger2 keeps open per key, both trangers, with room

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;

PRIVATE int queue_limit = 0;        // fs.inotify.max_queued_events
PRIVATE int n_keys = 0;             // keys of the flood: ids 1..n_keys
PRIVATE int *received = NULL;       // records received per key id
PRIVATE int received_total = 0;
PRIVATE int received_bad_key = 0;   // a key outside 0..n_keys: BUG
PRIVATE int deleted_seed = 0;       // key_deleted callbacks for the seed key
PRIVATE int deleted_other = 0;      // key_deleted callbacks for any other key: BUG
PRIVATE json_t *rt = NULL;          // the feed of the follower

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
    long id = atol(key);
    if(id < 0 || id > n_keys) {
        received_bad_key++;
    } else {
        received[id]++;
    }
    received_total++;
    JSON_DECREF(record)
    return 0;
}

PRIVATE int my_key_deleted_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    void *user_data
)
{
    if(atol(key) == SEED_KEY_ID) {
        deleted_seed++;
    } else {
        deleted_other++;
    }
    return 0;
}

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE int max_queued_events(void)
{
    int n = 16384;  // the kernel's default
    FILE *f = fopen("/proc/sys/fs/inotify/max_queued_events", "r");
    if(f) {
        if(fscanf(f, "%d", &n) != 1) {
            n = 16384;
        }
        fclose(f);
    }
    return n;
}

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

/*
 *  Run the loop until `expected` records arrived and the loop has been
 *  quiet for a while (a late duplicate would show in the quiet turns)
 */
/*
 *  A probe: a periodic timer of PROBE_MS. The longest gap between two of its
 *  firings is the longest time the loop was deaf -- a callback that held it
 *  (the rescan after an overflow ran in one piece, and took minutes on a
 *  busy disk of a central).
 */
#define PROBE_MS    50
PRIVATE uint64_t probe_last = 0;
PRIVATE uint64_t probe_max_gap = 0;
PRIVATE int probe_callback(yev_event_h yev_event)
{
    if(yev_get_state(yev_event) != YEV_ST_IDLE) {
        return 0;   // its stop
    }
    uint64_t now = time_in_milliseconds_monotonic();
    if(now - probe_last > probe_max_gap) {
        probe_max_gap = now - probe_last;
    }
    probe_last = now;
    return 0;
}

/*
 *  Bounded by TIME, not by turns: the pass after an overflow runs a slice
 *  per turn, and a slow disk needs more turns (a cap of 200000 turns cut
 *  it short on one node, with the records still coming).
 */
#define DRAIN_MAX_MS    (10*60*1000)
PRIVATE void drain(int expected)
{
    int quiet = 0;
    int last = -1;
    uint64_t t0 = time_in_milliseconds_monotonic();
    while(quiet < 50 && time_in_milliseconds_monotonic() - t0 < DRAIN_MAX_MS) {
        yev_loop_run_once(yev_loop);
        fs_event_t *fs = rt? (fs_event_t *)(uintptr_t)json_integer_value(
            json_object_get(rt, "fs_event_client")
        ) : NULL;
        BOOL pass_running = (fs && fs->rescan_dirs)? TRUE: FALSE;
        if(received_total == last && received_total >= expected && !pass_running) {
            quiet++;
        } else {
            quiet = 0;
        }
        last = received_total;
    }
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

    queue_limit = max_queued_events();
    n_keys = queue_limit + EXTRA_KEYS;

    /*
     *  timeranger2 keeps the files of every key open, as a yuno does, and a
     *  yuno raises its limit of open files at start up: so does the test.
     *  Where even the hard limit is short, the overflow cannot be reached.
     */
    struct rlimit rl;
    if(getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        rl.rlim_cur = rl.rlim_max;
        setrlimit(RLIMIT_NOFILE, &rl);
    }
    if(getrlimit(RLIMIT_NOFILE, &rl) != 0 ||
            rl.rlim_cur < (rlim_t)n_keys * FDS_PER_KEY) {
        printf("     SKIPPED: %d keys need ~%d open files, the limit is %lu\n",
            n_keys, n_keys * FDS_PER_KEY, (unsigned long)rl.rlim_cur);
        return 0;
    }

    received = GBMEM_MALLOC(sizeof(int) * (size_t)(n_keys + 1));

    /*-------------------------------------*
     *  Master, and the seed key
     *-------------------------------------*/
    set_expected_results(
        "overflow: setup",
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
    if(!topic) {
        tranger2_shutdown(tm);
        return -1;
    }
    if(append_one(tm, SEED_KEY_ID, BASE_T)<0) {
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Follower with a feed of every key
     *-------------------------------------*/
    set_expected_results("overflow: follower", NULL, NULL, NULL, 1);

    json_t *tf = startup_tranger(FALSE);
    if(!tf) {
        tranger2_shutdown(tm);
        return -1;
    }
    if(!tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        return -1;
    }
    rt = tranger2_open_rt_disk(
        tf, TOPIC_NAME, "", NULL, my_record_callback, "rtALL", "", NULL
    );
    if(!rt) {
        result += -1;
    }
    tranger2_set_rt_key_deleted_callback(rt, my_key_deleted_callback, NULL);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);    // the master hears disks/rtALL/ and feeds it
    }

    /*
     *  The seed key goes through the feed normally: its directory in
     *  disks/rtALL/ is created and WATCHED before the flood
     */
    if(append_one(tm, SEED_KEY_ID, BASE_T + 1)<0) {
        result += -1;
    }
    drain(1);
    if(received[SEED_KEY_ID] != 1) {
        printf("%sERROR%s --> the seed key got %d records before the flood, expected 1\n",
            On_Red BWhite, Color_Off, received[SEED_KEY_ID]);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  The flood, with the loop stopped:
     *  one record on n_keys new keys (one IN_CREATE each), and the seed key
     *  deleted while the queue is full (its signal is dropped too)
     *-------------------------------------*/
    /*
     *  A whitelist, not a script: how MANY overflows there are is not ours.
     *  The rescan consumes the links, each unlink is an IN_DELETE the reader
     *  hears of its own doing, and whether those overflow the queue again
     *  depends on how fast it reads. Each message at least once, nothing else.
     */
    set_expected_results_unordered(
        "overflow: the queue overflows, the feed is rebuilt from the filesystem",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "inotify IN_Q_OVERFLOW: events lost, rescanning the watched tree",
            "msg", "keys deleted while the inotify events were lost",
            "msg", "watched tree rescanned after lost inotify events"
        ),
        NULL, NULL, 1
    );

    memset(received, 0, sizeof(int) * (size_t)(n_keys + 1));
    received_total = 0;
    uint64_t t0 = time_in_milliseconds_monotonic();
    for(int id = 1; id <= n_keys; id++) {
        if(append_one(tm, id, BASE_T + 2)<0) {
            result += -1;
            break;
        }
    }
    if(tranger2_delete_key(tm, TOPIC_NAME, "0000000000000000000")<0) {
        printf("%sERROR%s --> the seed key could not be deleted\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    uint64_t t1 = time_in_milliseconds_monotonic();
    yev_event_h yev_probe = yev_create_timer_event(yev_loop, probe_callback, 0);
    probe_last = time_in_milliseconds_monotonic();  // a loop deaf from the start counts
    probe_max_gap = 0;
    yev_start_timer_event(yev_probe, PROBE_MS, TRUE);
    drain(n_keys);
    yev_stop_event(yev_probe);
    for(int i = 0; i < 5; i++) {
        yev_loop_run_once(yev_loop);
    }
    yev_destroy_event(yev_probe);
    uint64_t t2 = time_in_milliseconds_monotonic();
    printf("     flood of %d appends: %lu ms; drain of the feed: %lu ms, the loop deaf at most %lu ms\n",
        n_keys, (unsigned long)(t1 - t0), (unsigned long)(t2 - t1), (unsigned long)probe_max_gap);

    int missing = 0;
    int duplicated = 0;
    for(int id = 1; id <= n_keys; id++) {
        if(received[id] == 0) {
            missing++;
        } else if(received[id] > 1) {
            duplicated++;
        }
    }
    printf("     %d keys (queue limit %d): %d records, %d missing, %d duplicated\n",
        n_keys, queue_limit, received_total, missing, duplicated);
    if(missing || duplicated || received_bad_key) {
        printf("%sERROR%s --> every record once: %d missing, %d duplicated, %d of unknown keys\n",
            On_Red BWhite, Color_Off, missing, duplicated, received_bad_key);
        result += -1;
    }
    if(deleted_seed != 1 || deleted_other != 0) {
        printf("%sERROR%s --> the key deleted during the overflow: heard %d times (others %d), expected 1\n",
            On_Red BWhite, Color_Off, deleted_seed, deleted_other);
        result += -1;
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  After the overflow the watches are whole: the first and the last key
     *  of the flood (the last one was born while its IN_CREATE was dropped)
     *  get a new record each, and each arrives once
     *-------------------------------------*/
    set_expected_results("overflow: the keys born in the overflow are watched", NULL, NULL, NULL, 1);

    memset(received, 0, sizeof(int) * (size_t)(n_keys + 1));
    received_total = 0;
    if(append_one(tm, 1, BASE_T + 3)<0) {
        result += -1;
    }
    if(append_one(tm, n_keys, BASE_T + 3)<0) {
        result += -1;
    }
    drain(2);
    if(received[1] != 1 || received[n_keys] != 1 || received_total != 2) {
        printf("%sERROR%s --> after the overflow: first key %d, last key %d, total %d, expected 1/1/2\n",
            On_Red BWhite, Color_Off, received[1], received[n_keys], received_total);
        result += -1;
    }
    result += test_json(NULL);

    tranger2_close_rt_disk(tf, rt);
    rt = NULL;
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }

    set_expected_results("overflow: shutdown", NULL, NULL, NULL, 1);
    tranger2_shutdown(tf);
    tranger2_shutdown(tm);
    /*
     *  The watcher stops queued by the shutdowns complete asynchronously
     *  (their io_uring CQEs free each fs_event): give the loop the turns to
     *  process them, or their memory is still allocated at the leak check.
     */
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    GBMEM_FREE(received);
    rmrdir(path_database);
    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*
     *  ~70000 keys in the cache of two trangers: above the 64 MB default
     */
    gbmem_setup(
        0,                          // mem_max_block, the default
        (size_t)2*1024*1024*1024,   // mem_max_system_memory
        FALSE,                      // use_own_system_memory
        0,                          // mem_min_block
        0                           // mem_superblock
    );

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
