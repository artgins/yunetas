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
 *  Before that, TWO feeds on one topic (do_test_two_feeds, cheap: it needs
 *  no open file per event). The whole-topic feed overflows -- its queue
 *  filled by directories created and removed in one of its key directories
 *  -- and a key is deleted then; a second feed, keyed on that key, hears
 *  the delete as it comes, before the first one reads down to its overflow.
 *  The feeds share the topic's cache, and up to 7.25.20 the feed that heard
 *  took the key out of it: the overflowed one compared that cache with
 *  keys/, found nothing gone, and never heard of the delete. Each feed must
 *  hear it once.
 *
 *  Then the signal of a delete queued BEHIND the overflow
 *  (do_test_signal_behind_overflow, with the keyed feed and without it).
 *  The kernel queues its overflow at the end of a full queue, and as the
 *  watcher reads down to it room is made behind it: here a few batches are
 *  read, then the key is deleted. The overflow tells the key deleted (from
 *  the cache, or from what the keyed feed heard first), and the signal
 *  comes after it: the feed must not be told twice, and the keyed feed must
 *  not be left owing a delete it already heard -- the NEXT delete of the
 *  key would pay it, and a feed that overflowed then would miss that one.
 *  The same with the key written again before the follower reads anything:
 *  the overflowed feed owes the delete and finds the key on disk at its
 *  overflow; its signal, behind it, is that delete (told once, owed by
 *  nobody after), not a new one.
 *
 *  And a feed opened while a delete was in flight
 *  (do_test_feed_opened_in_flight): the key is deleted with the loop
 *  stopped, then a second whole-topic feed is opened. The master mirrored
 *  the delete before that feed's directory existed, so it never hears it,
 *  but the first feed hears it and counts it as owed by the second. The key
 *  is born again (both feeds get its record), then the first feed
 *  overflows and the key is deleted again: the second feed hears it first,
 *  and its old debt must not pay for this new delete -- the overflowed feed
 *  must be told it. Better still, the second feed owes nothing at all: the
 *  first feed's signal was queued before the second feed was watched.
 *
 *  And an OLD delete heard after the key came back
 *  (do_test_old_delete_after_reborn): a whole-topic feed hears a delete and
 *  the record of the key born again, and loads it into the cache the feeds
 *  share; a feed of another key, slow (its queue long with events of its
 *  own), hears the delete later. It must leave the live key alone, and owe
 *  nothing new: up to 7.25.20 every feed that heard a delete took the key
 *  out of the cache and dropped its watermark; counting the deletes owed,
 *  the slow feed took the delete as new and left the first feed owing it.
 *
 *  And a MASTER's own feed that overflows (do_test_master_feed_overflow):
 *  the master forgets a key at once when it deletes it, so its cache cannot
 *  say a delete was lost. The feeds watched when it deleted owe it, and the
 *  overflowed one is told it. Up to 7.25.20 it never heard it.
 *
 *  And a key deleted and written again behind an overflow
 *  (do_test_reborn_behind_overflow): the pass after the overflow finds
 *  the key's directory with the NEW key's record, and the delete is still
 *  queued behind the overflow. The feed must hear the delete, then the new
 *  record, and keep the key in its cache. Up to 7.25.20 the pass read the
 *  new key's file against the OLD key's cell (the record never handed),
 *  and the delete heard after took the live key out of the cache. The
 *  same with a key the follower never saw, written, deleted and written
 *  again there.
 *
 *  And in a master, the echo of a delete queued behind an overflow that
 *  found the key written again (do_test_master_echo_behind_overflow): the
 *  overflow cleared the feed's debt (the key is on disk), and the echo,
 *  finding the key in the cache, must not be taken as a delete nobody
 *  heard -- in a master the debts are made by tranger2_delete_key() -- or
 *  the other feeds are left owing it.
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
#include <sys/stat.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <yev_loop.h>
#include <testing.h>
#include <fs_watcher.h>

#define APP "test_rt_disk_overflow"

#define DATABASE    "tr_rt_disk_overflow"
#define DATABASE2   "tr_rt_disk_overflow_feeds"
#define DATABASE3   "tr_rt_disk_overflow_behind"
#define DATABASE4   "tr_rt_disk_overflow_late"
#define DATABASE5   "tr_rt_disk_overflow_reborn"
#define DATABASE6   "tr_rt_disk_overflow_master"
#define DATABASE7   "tr_rt_disk_overflow_master_echo"
#define DATABASE8   "tr_rt_disk_overflow_reborn_behind"
#define SEED_KEY    "0000000000000000000"
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
PRIVATE int seed_received = 0;      // records of the feed keyed on the seed key
PRIVATE int seed_deleted = 0;       // its key_deleted callbacks
PRIVATE int late_received = 0;      // records of the feed opened while a delete was in flight
PRIVATE int late_deleted = 0;       // its key_deleted callbacks of the seed key

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

PRIVATE int seed_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record
)
{
    seed_received++;
    JSON_DECREF(record)
    return 0;
}

PRIVATE int seed_key_deleted_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    void *user_data
)
{
    seed_deleted++;
    return 0;
}

PRIVATE int late_record_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *record
)
{
    late_received++;
    JSON_DECREF(record)
    return 0;
}

PRIVATE int late_key_deleted_callback(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    void *user_data
)
{
    if(atol(key) == SEED_KEY_ID) {
        late_deleted++;
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

PRIVATE json_t *startup_tranger(const char *database, BOOL master)
{
    char path_root[PATH_MAX];
    build_path(path_root, sizeof(path_root), getenv("HOME"), "tests_yuneta", NULL);
    mkrdir(path_root, 02770);

    json_t *jn_tranger = json_pack("{s:s, s:s, s:b, s:i, s:s, s:i, s:i}",
        "path", path_root,
        "database", database,
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
 *  it short on one node, with the records still coming). The flood drains
 *  in ~3.5 s here; two minutes is far beyond any node's. A drain that ends
 *  by time FAILS: a wait for records that never come once ran 10 minutes
 *  and passed.
 */
#define DRAIN_MAX_MS    (2*60*1000)
PRIVATE int drain(int expected)
{
    int quiet = 0;
    int last = -1;
    uint64_t t0 = time_in_milliseconds_monotonic();
    while(quiet < 50) {
        if(time_in_milliseconds_monotonic() - t0 >= DRAIN_MAX_MS) {
            fs_event_t *fs = rt? (fs_event_t *)(uintptr_t)json_integer_value(
                json_object_get(rt, "fs_event_client")
            ) : NULL;
            printf("%sERROR%s --> drain: %d ms and not done: %d records of %d expected, the pass %s\n",
                On_Red BWhite, Color_Off, DRAIN_MAX_MS, received_total, expected,
                (fs && fs->rescan_dirs)? "still running" : "ended");
            return -1;
        }
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
    return 0;
}

/***************************************************************************
 *  Two feeds on one topic, the whole-topic one overflowed
 ***************************************************************************/
PRIVATE json_t *create_topic(json_t *tm)
{
    return tranger2_create_topic(
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
}

PRIVATE int do_test_two_feeds(void)
{
    int result = 0;
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database),
        getenv("HOME"), "tests_yuneta", DATABASE2, NULL);
    rmrdir(path_database);

    queue_limit = max_queued_events();
    n_keys = 1;
    received = GBMEM_MALLOC(sizeof(int) * (size_t)(n_keys + 1));
    received_total = 0;
    deleted_seed = 0;
    deleted_other = 0;

    set_expected_results(
        "two feeds: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_tranger(DATABASE2, TRUE);
    if(!tm) {
        GBMEM_FREE(received);
        return -1;
    }
    if(!create_topic(tm)) {
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    if(append_one(tm, SEED_KEY_ID, BASE_T)<0) {
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("two feeds: follower", NULL, NULL, NULL, 1);
    json_t *tf = startup_tranger(DATABASE2, FALSE);
    if(!tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    rt = tranger2_open_rt_disk(
        tf, TOPIC_NAME, "", NULL, my_record_callback, "rtALL", "", NULL
    );
    json_t *rt_seed = tranger2_open_rt_disk(
        tf, TOPIC_NAME, "0000000000000000000", NULL, seed_record_callback, "rtSEED", "", NULL
    );
    if(!rt || !rt_seed) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    tranger2_set_rt_key_deleted_callback(rt, my_key_deleted_callback, NULL);
    tranger2_set_rt_key_deleted_callback(rt_seed, seed_key_deleted_callback, NULL);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }

    /*
     *  A record of each key: disks/rtALL/<key>/ of both keys are made and
     *  watched, and disks/rtSEED/ gets the seed key's
     */
    if(append_one(tm, SEED_KEY_ID, BASE_T + 1)<0 || append_one(tm, 1, BASE_T + 1)<0) {
        result += -1;
    }
    result += drain(2);
    if(received[SEED_KEY_ID] != 1 || received[1] != 1 || seed_received != 1) {
        printf("%sERROR%s --> before the overflow: whole-topic feed %d/%d, seed feed %d, expected 1/1/1\n",
            On_Red BWhite, Color_Off, received[SEED_KEY_ID], received[1], seed_received);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  With the loop stopped: the queue of rtALL filled past its limit by a
     *  directory created and removed in its key directory of key 1 (nothing
     *  a feed hears as a record or a delete), then the seed key deleted --
     *  rtALL's signal is dropped, rtSEED's (one event) comes first
     */
    set_expected_results_unordered(
        "two feeds: the key deleted while one feed was overflowed reaches both",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "inotify IN_Q_OVERFLOW: events lost, rescanning the watched tree",
            "msg", "keys deleted while the inotify events were lost",
            "msg", "watched tree rescanned after lost inotify events"
        ),
        NULL, NULL, 1
    );
    char churn[PATH_MAX];
    build_path(churn, sizeof(churn), path_database, TOPIC_NAME, "disks", "rtALL",
        "0000000000000000001", "c", NULL);
    for(int i = 0; i < queue_limit/2 + 1024; i++) {
        if(mkdir(churn, 0700)<0 || rmdir(churn)<0) {
            printf("%sERROR%s --> cannot churn %s: %s\n",
                On_Red BWhite, Color_Off, churn, strerror(errno));
            result += -1;
            break;
        }
    }
    if(tranger2_delete_key(tm, TOPIC_NAME, "0000000000000000000")<0) {
        printf("%sERROR%s --> the seed key could not be deleted\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += drain(0);

    if(deleted_seed != 1 || deleted_other != 0) {
        printf("%sERROR%s --> the overflowed feed heard the delete %d times (others %d), expected 1\n",
            On_Red BWhite, Color_Off, deleted_seed, deleted_other);
        result += -1;
    }
    if(seed_deleted != 1) {
        printf("%sERROR%s --> the keyed feed heard the delete %d times, expected 1\n",
            On_Red BWhite, Color_Off, seed_deleted);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("two feeds: shutdown", NULL, NULL, NULL, 1);
    tranger2_close_rt_disk(tf, rt);
    rt = NULL;
    tranger2_close_rt_disk(tf, rt_seed);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    tranger2_shutdown(tf);
    tranger2_shutdown(tm);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    GBMEM_FREE(received);
    received_total = 0;
    received_bad_key = 0;
    deleted_seed = 0;
    deleted_other = 0;
    rmrdir(path_database);
    return result;
}

/*
 *  The deletes a feed owes (its `deletes_unheard`): none once every feed
 *  has heard what it will ever hear
 */
PRIVATE int expect_no_debts(const char *feed_name, json_t *feed)
{
    json_t *unheard = json_object_get(feed, "deletes_unheard");
    if(json_object_size(unheard) > 0) {
        char dump[1024] = {0};
        json_dumpb(unheard, dump, sizeof(dump)-1, JSON_COMPACT);
        printf("%sERROR%s --> the feed %s owes deletes it will never hear: %s\n",
            On_Red BWhite, Color_Off, feed_name, dump);
        return -1;
    }
    return 0;
}

/*
 *  With the loop stopped: the queue of rtALL filled past its limit by a
 *  directory created and removed in its key directory of key 1 (nothing a
 *  feed hears as a record or a delete)
 */
PRIVATE int overflow_whole_topic_feed(const char *path_database)
{
    char churn[PATH_MAX];
    build_path(churn, sizeof(churn), path_database, TOPIC_NAME, "disks", "rtALL",
        "0000000000000000001", "c", NULL);
    for(int i = 0; i < queue_limit/2 + 1024; i++) {
        if(mkdir(churn, 0700)<0 || rmdir(churn)<0) {
            printf("%sERROR%s --> cannot churn %s: %s\n",
                On_Red BWhite, Color_Off, churn, strerror(errno));
            return -1;
        }
    }
    return 0;
}

/***************************************************************************
 *  The signal of a delete queued behind the overflow
 ***************************************************************************/
PRIVATE int do_test_signal_behind_overflow(BOOL with_keyed_feed, BOOL written_again)
{
    int result = 0;
    const char *label = written_again? "behind the overflow, written again" :
        with_keyed_feed? "behind the overflow, two feeds" : "behind the overflow, one feed";
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database),
        getenv("HOME"), "tests_yuneta", DATABASE3, NULL);
    rmrdir(path_database);

    queue_limit = max_queued_events();
    n_keys = 1;
    received = GBMEM_MALLOC(sizeof(int) * (size_t)(n_keys + 1));
    received_total = 0;
    deleted_seed = 0;
    deleted_other = 0;
    seed_received = 0;
    seed_deleted = 0;

    char title[128];
    snprintf(title, sizeof(title), "%s: setup", label);
    set_expected_results(
        title,
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_tranger(DATABASE3, TRUE);
    if(!tm || !create_topic(tm)) {
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    if(append_one(tm, SEED_KEY_ID, BASE_T)<0) {
        result += -1;
    }
    json_t *tf = startup_tranger(DATABASE3, FALSE);
    if(!tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    rt = tranger2_open_rt_disk(
        tf, TOPIC_NAME, "", NULL, my_record_callback, "rtALL", "", NULL
    );
    json_t *rt_seed = NULL;
    if(with_keyed_feed) {
        rt_seed = tranger2_open_rt_disk(
            tf, TOPIC_NAME, SEED_KEY, NULL, seed_record_callback, "rtSEED", "", NULL
        );
    }
    if(!rt || (with_keyed_feed && !rt_seed)) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    tranger2_set_rt_key_deleted_callback(rt, my_key_deleted_callback, NULL);
    if(rt_seed) {
        tranger2_set_rt_key_deleted_callback(rt_seed, seed_key_deleted_callback, NULL);
    }
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(append_one(tm, SEED_KEY_ID, BASE_T + 1)<0 || append_one(tm, 1, BASE_T + 1)<0) {
        result += -1;
    }
    result += drain(2);
    if(received[SEED_KEY_ID] != 1 || received[1] != 1 || (with_keyed_feed && seed_received != 1)) {
        printf("%sERROR%s --> before the overflow: whole-topic feed %d/%d, seed feed %d, expected 1/1/1\n",
            On_Red BWhite, Color_Off, received[SEED_KEY_ID], received[1], seed_received);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  The queue of rtALL overflowed with the loop stopped; the test reads
     *  some of it (room behind the overflow, which is still far: a turn of
     *  the loop drains the whole queue, the overflow included); then the
     *  seed key is deleted: its signal to rtALL is queued behind the
     *  overflow
     */
    snprintf(title, sizeof(title), "%s: the delete is told once", label);
    set_expected_results_unordered(
        title,
        written_again?
        json_pack("[{s:s},{s:s}]",  // the key is on disk at the overflow: nothing told there
            "msg", "inotify IN_Q_OVERFLOW: events lost, rescanning the watched tree",
            "msg", "watched tree rescanned after lost inotify events"
        ):
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "inotify IN_Q_OVERFLOW: events lost, rescanning the watched tree",
            "msg", "keys deleted while the inotify events were lost",
            "msg", "watched tree rescanned after lost inotify events"
        ),
        NULL, NULL, 1
    );
    result += overflow_whole_topic_feed(path_database);
    fs_event_t *fs_all = (fs_event_t *)(uintptr_t)json_integer_value(
        json_object_get(rt, "fs_event_client")
    );
    char room[4096];    // 128 events of the churn
    if(!fs_all || read(fs_all->fd, room, sizeof(room)) <= 0) {
        printf("%sERROR%s --> the test did not test: no room made behind the overflow: %s\n",
            On_Red BWhite, Color_Off, strerror(errno));
        result += -1;
    }
    if(tranger2_delete_key(tm, TOPIC_NAME, SEED_KEY)<0) {
        printf("%sERROR%s --> the seed key could not be deleted\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(written_again) {
        /*
         *  Written again before the follower reads anything: the keyed
         *  feed hears the delete first, the overflowed one owes it and
         *  finds the key on disk again at its overflow; its signal, behind
         *  the overflow, is that delete, not a new one (up to the fix the
         *  debt went at the overflow, and the signal was taken for a new
         *  delete, owed by the keyed feed, which had heard it)
         */
        if(append_one(tm, SEED_KEY_ID, BASE_T + 2)<0) {
            result += -1;
        }
    }
    result += drain(written_again? 1 : 0);

    if(deleted_seed != 1 || deleted_other != 0) {
        printf("%sERROR%s --> the overflowed feed heard the delete %d times (others %d), expected 1\n",
            On_Red BWhite, Color_Off, deleted_seed, deleted_other);
        result += -1;
    }
    if(written_again) {
        if(received[SEED_KEY_ID] != 2) {
            printf("%sERROR%s --> the overflowed feed got %d records of the seed key, expected 2 (one of each life)\n",
                On_Red BWhite, Color_Off, received[SEED_KEY_ID]);
            result += -1;
        }
        if(!json_object_get(json_object_get(tranger2_topic(tf, TOPIC_NAME), "cache"), SEED_KEY)) {
            printf("%sERROR%s --> the seed key written again is not in the follower's cache\n",
                On_Red BWhite, Color_Off);
            result += -1;
        }
    }
    if(with_keyed_feed && seed_deleted != 1) {
        printf("%sERROR%s --> the keyed feed heard the delete %d times, expected 1\n",
            On_Red BWhite, Color_Off, seed_deleted);
        result += -1;
    }
    result += expect_no_debts("rtALL", rt);
    if(rt_seed) {
        result += expect_no_debts("rtSEED", rt_seed);
    }
    result += test_json(NULL);

    snprintf(title, sizeof(title), "%s: shutdown", label);
    set_expected_results(title, NULL, NULL, NULL, 1);
    tranger2_close_rt_disk(tf, rt);
    rt = NULL;
    if(rt_seed) {
        tranger2_close_rt_disk(tf, rt_seed);
    }
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    tranger2_shutdown(tf);
    tranger2_shutdown(tm);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    GBMEM_FREE(received);
    received_total = 0;
    received_bad_key = 0;
    deleted_seed = 0;
    deleted_other = 0;
    seed_received = 0;
    seed_deleted = 0;
    rmrdir(path_database);
    return result;
}

/***************************************************************************
 *  A feed opened while a delete was in flight
 ***************************************************************************/
PRIVATE int do_test_feed_opened_in_flight(void)
{
    int result = 0;
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database),
        getenv("HOME"), "tests_yuneta", DATABASE4, NULL);
    rmrdir(path_database);

    queue_limit = max_queued_events();
    n_keys = 1;
    received = GBMEM_MALLOC(sizeof(int) * (size_t)(n_keys + 1));
    received_total = 0;
    deleted_seed = 0;
    deleted_other = 0;
    late_received = 0;
    late_deleted = 0;

    set_expected_results(
        "feed opened in flight: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_tranger(DATABASE4, TRUE);
    if(!tm || !create_topic(tm)) {
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    if(append_one(tm, SEED_KEY_ID, BASE_T)<0) {
        result += -1;
    }
    json_t *tf = startup_tranger(DATABASE4, FALSE);
    if(!tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    rt = tranger2_open_rt_disk(
        tf, TOPIC_NAME, "", NULL, my_record_callback, "rtALL", "", NULL
    );
    if(!rt) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    tranger2_set_rt_key_deleted_callback(rt, my_key_deleted_callback, NULL);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(append_one(tm, SEED_KEY_ID, BASE_T + 1)<0 || append_one(tm, 1, BASE_T + 1)<0) {
        result += -1;
    }
    result += drain(2);
    result += test_json(NULL);

    /*
     *  With the loop stopped: the seed key deleted (mirrored into
     *  disks/rtALL/ alone), THEN rtLATE opened
     */
    set_expected_results("feed opened in flight: the first delete", NULL, NULL, NULL, 1);
    if(tranger2_delete_key(tm, TOPIC_NAME, SEED_KEY)<0) {
        printf("%sERROR%s --> the seed key could not be deleted\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    json_t *rt_late = tranger2_open_rt_disk(
        tf, TOPIC_NAME, "", NULL, late_record_callback, "rtLATE", "", NULL
    );
    if(!rt_late) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    tranger2_set_rt_key_deleted_callback(rt_late, late_key_deleted_callback, NULL);
    result += drain(0);
    if(deleted_seed != 1 || late_deleted != 0) {
        printf("%sERROR%s --> the first delete: heard %d times by rtALL, %d by rtLATE, expected 1/0\n",
            On_Red BWhite, Color_Off, deleted_seed, late_deleted);
        result += -1;
    }
    result += expect_no_debts("rtLATE", rt_late);   // watched after the signal was queued
    result += test_json(NULL);

    /*
     *  The seed key born again: both feeds get its record, and rtLATE's
     *  stream shows the key alive after the debt was made
     */
    set_expected_results("feed opened in flight: the key born again", NULL, NULL, NULL, 1);
    memset(received, 0, sizeof(int) * (size_t)(n_keys + 1));
    received_total = 0;
    if(append_one(tm, SEED_KEY_ID, BASE_T + 2)<0) {
        result += -1;
    }
    result += drain(1);
    if(received[SEED_KEY_ID] != 1 || late_received != 1) {
        printf("%sERROR%s --> the key born again: rtALL %d records, rtLATE %d, expected 1/1\n",
            On_Red BWhite, Color_Off, received[SEED_KEY_ID], late_received);
        result += -1;
    }
    result += expect_no_debts("rtLATE", rt_late);
    result += test_json(NULL);

    /*
     *  rtALL overflowed with the loop stopped, the seed key deleted again:
     *  rtALL's signal is dropped, rtLATE's comes first
     */
    set_expected_results_unordered(
        "feed opened in flight: the second delete reaches the overflowed feed",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "inotify IN_Q_OVERFLOW: events lost, rescanning the watched tree",
            "msg", "keys deleted while the inotify events were lost",
            "msg", "watched tree rescanned after lost inotify events"
        ),
        NULL, NULL, 1
    );
    result += overflow_whole_topic_feed(path_database);
    if(tranger2_delete_key(tm, TOPIC_NAME, SEED_KEY)<0) {
        printf("%sERROR%s --> the seed key could not be deleted again\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    result += drain(0);
    if(deleted_seed != 2 || deleted_other != 0 || late_deleted != 1) {
        printf("%sERROR%s --> the second delete: heard %d times by rtALL in all (others %d), %d by rtLATE, expected 2/0/1\n",
            On_Red BWhite, Color_Off, deleted_seed, deleted_other, late_deleted);
        result += -1;
    }
    result += expect_no_debts("rtALL", rt);
    result += expect_no_debts("rtLATE", rt_late);
    result += test_json(NULL);

    set_expected_results("feed opened in flight: shutdown", NULL, NULL, NULL, 1);
    tranger2_close_rt_disk(tf, rt);
    rt = NULL;
    tranger2_close_rt_disk(tf, rt_late);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    tranger2_shutdown(tf);
    tranger2_shutdown(tm);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    GBMEM_FREE(received);
    received_total = 0;
    received_bad_key = 0;
    deleted_seed = 0;
    deleted_other = 0;
    late_received = 0;
    late_deleted = 0;
    rmrdir(path_database);
    return result;
}

/***************************************************************************
 *  An old delete heard after the key came back
 ***************************************************************************/
PRIVATE int do_test_old_delete_after_reborn(void)
{
    int result = 0;
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database),
        getenv("HOME"), "tests_yuneta", DATABASE5, NULL);
    rmrdir(path_database);

    n_keys = 1;
    received = GBMEM_MALLOC(sizeof(int) * (size_t)(n_keys + 1));
    received_total = 0;
    deleted_seed = 0;
    deleted_other = 0;
    seed_received = 0;
    seed_deleted = 0;

    set_expected_results(
        "old delete after reborn: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_tranger(DATABASE5, TRUE);
    if(!tm || !create_topic(tm)) {
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    if(append_one(tm, SEED_KEY_ID, BASE_T)<0) {
        result += -1;
    }
    json_t *tf = startup_tranger(DATABASE5, FALSE);
    if(!tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    rt = tranger2_open_rt_disk(
        tf, TOPIC_NAME, "", NULL, my_record_callback, "rtALL", "", NULL
    );
    json_t *rt_one = tranger2_open_rt_disk(      // the feed of another key
        tf, TOPIC_NAME, "0000000000000000001", NULL, seed_record_callback, "rtONE", "", NULL
    );
    if(!rt || !rt_one) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    tranger2_set_rt_key_deleted_callback(rt, my_key_deleted_callback, NULL);
    tranger2_set_rt_key_deleted_callback(rt_one, seed_key_deleted_callback, NULL);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(append_one(tm, SEED_KEY_ID, BASE_T + 1)<0 || append_one(tm, 1, BASE_T + 1)<0) {
        result += -1;
    }
    result += drain(2);
    result += test_json(NULL);

    /*
     *  With the loop stopped: the queue of rtONE made long (a directory
     *  created and removed in its key directory, 2000 times), then the seed
     *  key deleted and written again. rtALL reads its few events first:
     *  the delete, then the record of the key born again.
     */
    set_expected_results("old delete after reborn: the live key stays", NULL, NULL, NULL, 1);
    char churn[PATH_MAX];
    build_path(churn, sizeof(churn), path_database, TOPIC_NAME, "disks", "rtONE",
        "0000000000000000001", "c", NULL);
    for(int i = 0; i < 2000; i++) {
        if(mkdir(churn, 0700)<0 || rmdir(churn)<0) {
            printf("%sERROR%s --> cannot churn %s: %s\n",
                On_Red BWhite, Color_Off, churn, strerror(errno));
            result += -1;
            break;
        }
    }
    if(tranger2_delete_key(tm, TOPIC_NAME, SEED_KEY)<0) {
        printf("%sERROR%s --> the seed key could not be deleted\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    memset(received, 0, sizeof(int) * (size_t)(n_keys + 1));
    received_total = 0;
    if(append_one(tm, SEED_KEY_ID, BASE_T + 2)<0) {
        result += -1;
    }
    result += drain(1);

    json_t *cache = json_object_get(tranger2_topic(tf, TOPIC_NAME), "cache");
    if(deleted_seed != 1 || received[SEED_KEY_ID] != 1 || seed_deleted != 0) {
        printf("%sERROR%s --> rtALL heard the delete %d times and %d records of the key born again, rtONE %d deletes, expected 1/1/0\n",
            On_Red BWhite, Color_Off, deleted_seed, received[SEED_KEY_ID], seed_deleted);
        result += -1;
    }
    if(!json_object_get(cache, SEED_KEY)) {
        printf("%sERROR%s --> the key born again is not in the cache: an old delete took it out\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += expect_no_debts("rtALL", rt);
    result += expect_no_debts("rtONE", rt_one);

    /*
     *  And the key lives for the feed: its next record reaches rtALL
     */
    memset(received, 0, sizeof(int) * (size_t)(n_keys + 1));
    received_total = 0;
    if(append_one(tm, SEED_KEY_ID, BASE_T + 3)<0) {
        result += -1;
    }
    result += drain(1);
    if(received[SEED_KEY_ID] != 1) {
        printf("%sERROR%s --> the next record of the key born again: %d, expected 1\n",
            On_Red BWhite, Color_Off, received[SEED_KEY_ID]);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("old delete after reborn: shutdown", NULL, NULL, NULL, 1);
    tranger2_close_rt_disk(tf, rt);
    rt = NULL;
    tranger2_close_rt_disk(tf, rt_one);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    tranger2_shutdown(tf);
    tranger2_shutdown(tm);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    GBMEM_FREE(received);
    received_total = 0;
    received_bad_key = 0;
    deleted_seed = 0;
    deleted_other = 0;
    seed_received = 0;
    seed_deleted = 0;
    rmrdir(path_database);
    return result;
}

/***************************************************************************
 *  A master's own feed that overflows
 ***************************************************************************/
PRIVATE int do_test_master_feed_overflow(void)
{
    int result = 0;
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database),
        getenv("HOME"), "tests_yuneta", DATABASE6, NULL);
    rmrdir(path_database);

    queue_limit = max_queued_events();
    n_keys = 1;
    received = GBMEM_MALLOC(sizeof(int) * (size_t)(n_keys + 1));
    received_total = 0;
    deleted_seed = 0;
    deleted_other = 0;

    set_expected_results(
        "master feed overflow: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_tranger(DATABASE6, TRUE);
    if(!tm || !create_topic(tm)) {
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    rt = tranger2_open_rt_disk(
        tm, TOPIC_NAME, "", NULL, my_record_callback, "rtALL", "", NULL
    );
    if(!rt) {
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    tranger2_set_rt_key_deleted_callback(rt, my_key_deleted_callback, NULL);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(append_one(tm, SEED_KEY_ID, BASE_T)<0 || append_one(tm, 1, BASE_T)<0) {
        result += -1;
    }
    /*
     *  A master's own rt_disk feed is handed no record: the master's cache
     *  counts each one at its append, so the link it makes in the feed's
     *  directory is "nothing new" when heard (a master feeds its lists from
     *  memory). What this needs is the link's directory, where the overflow
     *  is made.
     */
    char key1_dir[PATH_MAX];
    build_path(key1_dir, sizeof(key1_dir), path_database, TOPIC_NAME, "disks", "rtALL",
        "0000000000000000001", NULL);
    result += drain(0);
    if(received_total != 0 || !is_directory(key1_dir)) {
        printf("%sERROR%s --> the master's own feed: %d records handed (expected 0), %s %s\n",
            On_Red BWhite, Color_Off, received_total, key1_dir,
            is_directory(key1_dir)? "made" : "NOT made");
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  With the loop stopped: the feed's queue overflowed, then the seed
     *  key deleted: the echo of the delete is dropped
     */
    set_expected_results_unordered(
        "master feed overflow: the delete is told",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "inotify IN_Q_OVERFLOW: events lost, rescanning the watched tree",
            "msg", "keys deleted while the inotify events were lost",
            "msg", "watched tree rescanned after lost inotify events"
        ),
        NULL, NULL, 1
    );
    result += overflow_whole_topic_feed(path_database);
    if(tranger2_delete_key(tm, TOPIC_NAME, SEED_KEY)<0) {
        result += -1;
    }
    result += drain(0);
    if(deleted_seed != 1 || deleted_other != 0) {
        printf("%sERROR%s --> the master's overflowed feed heard the delete %d times (others %d), expected 1\n",
            On_Red BWhite, Color_Off, deleted_seed, deleted_other);
        result += -1;
    }
    result += expect_no_debts("rtALL", rt);
    result += test_json(NULL);

    set_expected_results("master feed overflow: shutdown", NULL, NULL, NULL, 1);
    tranger2_close_rt_disk(tm, rt);
    rt = NULL;
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    tranger2_shutdown(tm);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    GBMEM_FREE(received);
    received_total = 0;
    received_bad_key = 0;
    deleted_seed = 0;
    deleted_other = 0;
    rmrdir(path_database);
    return result;
}

/***************************************************************************
 *  A key deleted and written again behind an overflow
 ***************************************************************************/
PRIVATE int do_test_reborn_behind_overflow(BOOL new_key)
{
    int result = 0;
    const char *label = new_key? "reborn behind overflow, a new key" : "reborn behind overflow";
    char title[128];
    json_int_t kid = new_key? 2 : SEED_KEY_ID;
    const char *kname = new_key? "0000000000000000002" : SEED_KEY;
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database),
        getenv("HOME"), "tests_yuneta", DATABASE8, NULL);
    rmrdir(path_database);

    queue_limit = max_queued_events();
    n_keys = 2;
    received = GBMEM_MALLOC(sizeof(int) * (size_t)(n_keys + 1));
    received_total = 0;
    deleted_seed = 0;
    deleted_other = 0;

    snprintf(title, sizeof(title), "%s: setup", label);
    set_expected_results(
        title,
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_tranger(DATABASE8, TRUE);
    if(!tm || !create_topic(tm)) {
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    if(append_one(tm, SEED_KEY_ID, BASE_T)<0) {
        result += -1;
    }
    json_t *tf = startup_tranger(DATABASE8, FALSE);
    if(!tf || !tranger2_open_topic(tf, TOPIC_NAME, TRUE)) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    rt = tranger2_open_rt_disk(
        tf, TOPIC_NAME, "", NULL, my_record_callback, "rtALL", "", NULL
    );
    if(!rt) {
        tranger2_shutdown(tf);
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    tranger2_set_rt_key_deleted_callback(rt, my_key_deleted_callback, NULL);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(append_one(tm, SEED_KEY_ID, BASE_T + 1)<0 || append_one(tm, SEED_KEY_ID, BASE_T + 2)<0 ||
            append_one(tm, 1, BASE_T + 1)<0) {
        result += -1;
    }
    result += drain(3);
    result += test_json(NULL);

    /*
     *  With the loop stopped: rtALL overflowed, some of its queue read
     *  (room behind the overflow), the seed key deleted and written again
     */
    snprintf(title, sizeof(title), "%s: the delete, then the new record", label);
    set_expected_results_unordered(
        title,
        json_pack("[{s:s},{s:s}]",
            "msg", "inotify IN_Q_OVERFLOW: events lost, rescanning the watched tree",
            "msg", "watched tree rescanned after lost inotify events"
        ),
        NULL, NULL, 1
    );
    result += overflow_whole_topic_feed(path_database);
    fs_event_t *fs_all = (fs_event_t *)(uintptr_t)json_integer_value(
        json_object_get(rt, "fs_event_client")
    );
    /*
     *  Room for almost the whole queue behind the overflow, filled again
     *  but for a margin: the stream behind the overflow is long, the pass
     *  that starts at the overflow runs its slices while it is read, and
     *  reaches the key's directory before the stream reaches the delete
     */
    size_t room_bytes = (size_t)(queue_limit - 1024) * 32;  // a churn event is 32 bytes
    size_t made = 0;
    char room[4096];
    while(fs_all && made < room_bytes) {
        ssize_t n = read(fs_all->fd, room, sizeof(room));
        if(n <= 0) {
            break;
        }
        made += (size_t)n;
    }
    if(made < room_bytes/2) {
        printf("%sERROR%s --> the test did not test: %lu bytes of room made behind the overflow\n",
            On_Red BWhite, Color_Off, (unsigned long)made);
        result += -1;
    }
    char churn[PATH_MAX];
    build_path(churn, sizeof(churn), path_database, TOPIC_NAME, "disks", "rtALL",
        "0000000000000000001", "c", NULL);
    for(size_t i = 0; i < made/32/2 - 512; i++) {
        if(mkdir(churn, 0700)<0 || rmdir(churn)<0) {
            result += -1;
            break;
        }
    }
    memset(received, 0, sizeof(int) * (size_t)(n_keys + 1));
    received_total = 0;
    if(new_key) {
        /*
         *  A key the follower never saw: written, deleted, written again
         */
        if(append_one(tm, kid, BASE_T + 3)<0 || append_one(tm, kid, BASE_T + 4)<0) {
            result += -1;
        }
    }
    if(tranger2_delete_key(tm, TOPIC_NAME, kname)<0 || append_one(tm, kid, BASE_T + 5)<0) {
        result += -1;
    }
    result += drain(1);
    json_t *cache = json_object_get(tranger2_topic(tf, TOPIC_NAME), "cache");
    int deletes = new_key? deleted_other : deleted_seed;
    if(deletes != 1 || received[kid] != 1 || !json_object_get(cache, kname)) {
        printf("%sERROR%s --> %s: %d deletes, %d records of the key born again, in the cache: %s; expected 1/1/yes\n",
            On_Red BWhite, Color_Off, label, deletes, received[kid],
            json_object_get(cache, kname)? "yes" : "NO");
        result += -1;
    }
    result += test_json(NULL);

    snprintf(title, sizeof(title), "%s: the next record", label);
    set_expected_results(title, NULL, NULL, NULL, 1);
    memset(received, 0, sizeof(int) * (size_t)(n_keys + 1));
    received_total = 0;
    if(append_one(tm, kid, BASE_T + 6)<0) {
        result += -1;
    }
    result += drain(1);
    if(received[kid] != 1) {
        printf("%sERROR%s --> %s: the next record handed %d times, expected 1\n",
            On_Red BWhite, Color_Off, label, received[kid]);
        result += -1;
    }
    result += test_json(NULL);

    snprintf(title, sizeof(title), "%s: shutdown", label);
    set_expected_results(title, NULL, NULL, NULL, 1);
    tranger2_close_rt_disk(tf, rt);
    rt = NULL;
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    tranger2_shutdown(tf);
    tranger2_shutdown(tm);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    GBMEM_FREE(received);
    received_total = 0;
    received_bad_key = 0;
    deleted_seed = 0;
    deleted_other = 0;
    rmrdir(path_database);
    return result;
}

/***************************************************************************
 *  In a master, an echo behind an overflow that found the key written again
 ***************************************************************************/
PRIVATE int do_test_master_echo_behind_overflow(void)
{
    int result = 0;
    char path_database[PATH_MAX];
    build_path(path_database, sizeof(path_database),
        getenv("HOME"), "tests_yuneta", DATABASE7, NULL);
    rmrdir(path_database);

    queue_limit = max_queued_events();
    n_keys = 1;
    received = GBMEM_MALLOC(sizeof(int) * (size_t)(n_keys + 1));
    received_total = 0;
    deleted_seed = 0;
    deleted_other = 0;
    seed_deleted = 0;

    set_expected_results(
        "master echo behind overflow: setup",
        json_pack("[{s:s},{s:s}]",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating topic"
        ),
        NULL, NULL, 1
    );
    json_t *tm = startup_tranger(DATABASE7, TRUE);
    if(!tm || !create_topic(tm)) {
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    rt = tranger2_open_rt_disk(     // overflowed
        tm, TOPIC_NAME, "", NULL, my_record_callback, "rtALL", "", NULL
    );
    json_t *rt_other = tranger2_open_rt_disk(
        tm, TOPIC_NAME, "", NULL, seed_record_callback, "rtOTHER", "", NULL
    );
    if(!rt || !rt_other) {
        tranger2_shutdown(tm);
        GBMEM_FREE(received);
        return -1;
    }
    tranger2_set_rt_key_deleted_callback(rt, my_key_deleted_callback, NULL);
    tranger2_set_rt_key_deleted_callback(rt_other, seed_key_deleted_callback, NULL);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(append_one(tm, SEED_KEY_ID, BASE_T)<0 || append_one(tm, 1, BASE_T)<0) {
        result += -1;
    }
    result += drain(0);     // a master's own feed is handed no record
    result += test_json(NULL);

    /*
     *  With the loop stopped: rtALL overflowed, some of its queue read
     *  (room behind the overflow), the seed key deleted (its echo to rtALL
     *  queued behind the overflow) and written again
     */
    set_expected_results_unordered(
        "master echo behind overflow: nobody is left owing",
        json_pack("[{s:s},{s:s}]",
            "msg", "inotify IN_Q_OVERFLOW: events lost, rescanning the watched tree",
            "msg", "watched tree rescanned after lost inotify events"
        ),
        NULL, NULL, 1
    );
    result += overflow_whole_topic_feed(path_database);
    fs_event_t *fs_all = (fs_event_t *)(uintptr_t)json_integer_value(
        json_object_get(rt, "fs_event_client")
    );
    char room[4096];
    if(!fs_all || read(fs_all->fd, room, sizeof(room)) <= 0) {
        printf("%sERROR%s --> the test did not test: no room made behind the overflow: %s\n",
            On_Red BWhite, Color_Off, strerror(errno));
        result += -1;
    }
    if(tranger2_delete_key(tm, TOPIC_NAME, SEED_KEY)<0 || append_one(tm, SEED_KEY_ID, BASE_T + 1)<0) {
        result += -1;
    }
    result += drain(0);
    if(deleted_seed != 1 || seed_deleted != 1) {
        printf("%sERROR%s --> the delete heard %d times by rtALL, %d by rtOTHER, expected 1/1\n",
            On_Red BWhite, Color_Off, deleted_seed, seed_deleted);
        result += -1;
    }
    result += expect_no_debts("rtALL", rt);
    result += expect_no_debts("rtOTHER", rt_other);
    result += test_json(NULL);

    set_expected_results("master echo behind overflow: shutdown", NULL, NULL, NULL, 1);
    tranger2_close_rt_disk(tm, rt);
    rt = NULL;
    tranger2_close_rt_disk(tm, rt_other);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    tranger2_shutdown(tm);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    GBMEM_FREE(received);
    received_total = 0;
    received_bad_key = 0;
    deleted_seed = 0;
    deleted_other = 0;
    seed_deleted = 0;
    rmrdir(path_database);
    return result;
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

    json_t *tm = startup_tranger(DATABASE, TRUE);
    if(!tm) {
        return -1;
    }
    json_t *topic = create_topic(tm);
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

    json_t *tf = startup_tranger(DATABASE, FALSE);
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
    result += drain(1);
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
    result += drain(n_keys);
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
    result += drain(2);
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

    int result = do_test_two_feeds();
    result += do_test_signal_behind_overflow(FALSE, FALSE);
    result += do_test_signal_behind_overflow(TRUE, FALSE);
    result += do_test_signal_behind_overflow(TRUE, TRUE);
    result += do_test_feed_opened_in_flight();
    result += do_test_old_delete_after_reborn();
    result += do_test_reborn_behind_overflow(FALSE);
    result += do_test_reborn_behind_overflow(TRUE);
    result += do_test_master_feed_overflow();
    result += do_test_master_echo_behind_overflow();
    result += do_test();

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
