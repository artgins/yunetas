/****************************************************************************
 *          test_fs_watcher_overflow.c
 *
 *  fs_watcher when its inotify queue OVERFLOWS, whoever owns it.
 *
 *  After an IN_Q_OVERFLOW the watcher walks the tree and hands every
 *  directory to its owner (FS_RESCAN_DIR_TYPE), a slice per loop turn. The
 *  owner reads what each directory holds, and that can be slow: a
 *  timeranger2 follower of 50000 keys on the busy disk of a central took
 *  minutes, and while one pass held the loop the yuno answered nothing (its
 *  agent, its commands, its timers). The pass gives the loop back every
 *  slice.
 *
 *  The overflow is real: with the loop NOT running, more directories than
 *  the queue holds are created in the watched root (one IN_CREATE each).
 *  The owner here is slow on purpose (OWNER_US per directory, a pass of
 *  seconds), and a periodic timer probes the loop. Then:
 *
 *      - the overflow is announced, and a pass follows;
 *      - every directory is told to the owner, heard or rescanned;
 *      - the loop is never deaf for more than MAX_DEAF_MS, while the pass
 *        takes seconds (in one piece it would be deaf for all of them);
 *      - the watcher's own cost per directory (the pass less the owner's
 *        time) stays under MAX_OWN_US: a cost that grows with the tree, as
 *        an index rebuilt per slice did, makes a pass of minutes;
 *      - a directory born in the overflow is watched after it: a file
 *        created in it is heard;
 *      - a directory watched BEFORE the overflow, deleted and created again
 *        while the queue was full (its IN_DELETE_SELF and IN_IGNORED lost
 *        with the rest), is watched again: a file created in it is heard.
 *        Up to 7.25.20 the pass found its path in the table of watches,
 *        under the wd of the directory that went, and never watched the
 *        new one.
 *
 *  And an owner that STOPS the watcher from its FS_OVERFLOW_TYPE callback
 *  (do_test_stop_on_overflow): no pass is started for a watcher being
 *  destroyed. Up to 7.25.20 the watcher built the index of the pass and
 *  created and armed its timer, all of it thrown away when the batch ended.
 *  The timer is what shows: yev_loop says each timer it creates under the
 *  global trace `liburing`, and the test counts those lines.
 *
 *  And fs_queued_events_end() of a watcher whose read has COMPLETED and
 *  not been handed over (do_test_queued_events_end): the loop stopped, the
 *  kernel reads the first events at the next system call and keeps them
 *  in the completion ring, where FIONREAD no longer counts them. The
 *  answer must still be where the events end, that read included.
 *
 *  And the ROOT deleted and created again while the queue is full
 *  (do_test_root_reborn, recursive and not): after the pass the new root
 *  is watched, a file created in it is heard. Up to 7.25.20 the pass
 *  watched again the directories it met, never the root it started from.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/inotify.h>

#include <gobj.h>
#include <kwid.h>
#include <helpers.h>
#include <yev_loop.h>
#include <fs_watcher.h>
#include <testing.h>

#define APP "test_fs_watcher_overflow"

#define EXTRA_DIRS  4096        // beyond the queue's limit
#define OWNER_US    100         // what the owner spends on each directory of the pass
#define PROBE_MS    50
#define MAX_DEAF_MS 1000
#define MAX_OWN_US  200         // the watcher's own cost per directory of the pass
#define REBORN_DIR  "reborn"    // watched before the flood, deleted and created again in it

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE char root[PATH_MAX];
PRIVATE int n_dirs = 0;

PRIVATE json_t *told = NULL;        // directory name -> times told (created or rescanned)
PRIVATE int overflows = 0;
PRIVATE int rescan_dirs = 0;
PRIVATE int files_created = 0;

PRIVATE uint64_t owner_us = 0;     // time spent by the owner in the pass

PRIVATE uint64_t probe_last = 0;
PRIVATE uint64_t probe_max_gap = 0;

PRIVATE int stop_overflows = 0;     // FS_OVERFLOW_TYPE told to the owner that stops
PRIVATE int stop_rescanned = 0;     // FS_RESCAN_DIR_TYPE told to it
PRIVATE int timers_created = 0;     // "yev_create_timer_event" lines, under the trace

PRIVATE int end_files = 0;          // FS_FILE_CREATED_TYPE told to the owner of the queued-end test
PRIVATE int root_overflows = 0;     // FS_OVERFLOW_TYPE told to the owner of a reborn root
PRIVATE int root_files = 0;         // FS_FILE_CREATED_TYPE told to it

/***************************************************************
 *              Callbacks
 ***************************************************************/
PRIVATE uint64_t now_us(void)   // a measure, not a timeout: the helpers count ms
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

PRIVATE void tell(const char *directory, const char *filename)
{
    /*
     *  A directory of the root is told either by its IN_CREATE (directory
     *  = the root, filename = its name) or by the pass (directory = it)
     */
    const char *name = NULL;
    if(strcmp(directory, root)==0) {
        name = filename;
    } else if(strncmp(directory, root, strlen(root))==0 && directory[strlen(root)]=='/') {
        name = directory + strlen(root) + 1;
    }
    if(!empty_string(name)) {
        json_object_set_new(told, name,
            json_integer(json_integer_value(json_object_get(told, name)) + 1)
        );
    }
}

PRIVATE int fs_callback(fs_event_t *fs_event)
{
    const char *directory = (const char *)fs_event->directory;
    const char *filename = (const char *)fs_event->filename;

    switch(fs_event->fs_type) {
        case FS_SUBDIR_CREATED_TYPE:
            tell(directory, filename);
            break;
        case FS_OVERFLOW_TYPE:
            overflows++;
            break;
        case FS_RESCAN_DIR_TYPE:
            {
                uint64_t t = now_us();
                rescan_dirs++;
                tell(directory, "");
                usleep(OWNER_US);   // a slow owner: it reads what the directory holds
                owner_us += now_us() - t;
            }
            break;
        case FS_FILE_CREATED_TYPE:
            files_created++;
            break;
        default:
            break;
    }
    return 0;
}

PRIVATE int fs_callback_stop(fs_event_t *fs_event)
{
    switch(fs_event->fs_type) {
        case FS_OVERFLOW_TYPE:
            stop_overflows++;
            fs_stop_watcher_event(fs_event);
            gobj_set_global_trace2(TRACE_URING, TRUE);  // from here each timer created is said
            break;
        case FS_RESCAN_DIR_TYPE:
            stop_rescanned++;
            break;
        default:
            break;
    }
    return 0;
}

PRIVATE int fs_callback_root(fs_event_t *fs_event)
{
    switch(fs_event->fs_type) {
        case FS_OVERFLOW_TYPE:
            root_overflows++;
            break;
        case FS_FILE_CREATED_TYPE:
            root_files++;
            break;
        default:
            break;
    }
    return 0;
}

PRIVATE int fs_callback_end(fs_event_t *fs_event)
{
    if(fs_event->fs_type == FS_FILE_CREATED_TYPE) {
        end_files++;
    }
    return 0;
}

PRIVATE int count_timers_write(void *v, int priority, const char *bf, size_t len)
{
    if(strstr(bf, "\"yev_create_timer_event\"")) {
        timers_created++;
    }
    return 0;
}

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

PRIVATE int count_told(void)
{
    int n = 0;
    const char *name; json_t *v;
    json_object_foreach(told, name, v) {
        if(json_integer_value(v) > 0) {
            n++;
        }
    }
    return n;
}

/***************************************************************************
 *  An owner that stops the watcher when it is told of the overflow
 ***************************************************************************/
PRIVATE int do_test_stop_on_overflow(void)
{
    int result = 0;
    char root2[PATH_MAX];
    build_path(root2, sizeof(root2), getenv("HOME"), "tests_yuneta", "fs_watcher_overflow_stop", NULL);
    rmrdir(root2);
    mkrdir(root2, 02770);

    set_expected_results("fs_watcher stopped on overflow: watch", NULL, NULL, NULL, 1);
    fs_event_t *fs_event = fs_create_watcher_event(
        yev_loop,
        root2,
        0,
        fs_callback_stop,
        0,
        NULL,
        NULL
    );
    if(!fs_event) {
        return -1;
    }
    fs_start_watcher_event(fs_event);
    for(int i = 0; i < 5; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    /*
     *  The queue filled, with the loop stopped, by one directory created
     *  and removed over and over (two events each, no disk left behind)
     */
    set_expected_results(
        "fs_watcher stopped on overflow: no pass",
        json_pack("[{s:s}]",
            "msg", "inotify IN_Q_OVERFLOW: events lost, rescanning the watched tree"
        ),
        NULL, NULL, 1
    );
    gobj_log_register_handler("count_timers", 0, count_timers_write, 0);
    gobj_log_add_handler("count_timers", "count_timers", LOG_OPT_ALL, 0);

    char churn[PATH_MAX];
    build_path(churn, sizeof(churn), root2, "c", NULL);
    for(int i = 0; i < max_queued_events()/2 + 1024; i++) {
        if(mkdir(churn, 0700) < 0 || rmdir(churn) < 0) {
            printf("%sERROR%s --> cannot churn %s: %s\n", On_Red BWhite, Color_Off, churn, strerror(errno));
            result += -1;
            break;
        }
    }
    for(int i = 0; i < 200000 && stop_overflows == 0; i++) {
        yev_loop_run_once(yev_loop);
    }
    gobj_set_global_trace2(TRACE_URING, FALSE);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    gobj_log_del_handler("count_timers");

    if(stop_overflows != 1) {
        printf("%sERROR%s --> the overflow told %d times, expected 1: the test did not test\n",
            On_Red BWhite, Color_Off, stop_overflows);
        result += -1;
    }
    if(timers_created != 0 || stop_rescanned != 0) {
        printf("%sERROR%s --> a watcher stopped on overflow: %d rescan timer(s) created, %d directories rescanned, expected 0/0\n",
            On_Red BWhite, Color_Off, timers_created, stop_rescanned);
        result += -1;
    }
    result += test_json(NULL);

    rmrdir(root2);
    return result;
}

/***************************************************************************
 *  Where the queued events end, with a read completed and not handed over
 ***************************************************************************/
#define END_FILES   20      // "f00".."f19": 16 bytes of header and 16 of name each
PRIVATE int do_test_queued_events_end(void)
{
    int result = 0;
    char root4[PATH_MAX];
    build_path(root4, sizeof(root4), getenv("HOME"), "tests_yuneta", "fs_watcher_queued_end", NULL);
    rmrdir(root4);
    mkrdir(root4, 02770);
    end_files = 0;

    set_expected_results("fs_watcher queued end: a read completed and not handed over", NULL, NULL, NULL, 1);
    fs_event_t *fs_event = fs_create_watcher_event(
        yev_loop,
        root4,
        0,
        fs_callback_end,
        0,
        NULL,
        NULL
    );
    if(!fs_event) {
        return -1;
    }
    fs_start_watcher_event(fs_event);
    for(int i = 0; i < 5; i++) {
        yev_loop_run_once(yev_loop);    // the read is in the kernel, waiting
    }

    /*
     *  With the loop stopped: each file created is an IN_CREATE of 32
     *  bytes; the first one wakes the read, done at the next system call
     */
    size_t expected = 0;
    for(int i = 0; i < END_FILES; i++) {
        char name[16], path[PATH_MAX];
        snprintf(name, sizeof(name), "f%02d", i);
        build_path(path, sizeof(path), root4, name, NULL);
        int fd = open(path, O_CREAT|O_WRONLY, 0600);
        if(fd >= 0) {
            close(fd);
        }
        expected += sizeof(struct inotify_event) + 16;
    }
    uint64_t end = fs_queued_events_end(fs_event);

    for(int i = 0; i < 50 && end_files < END_FILES; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(end_files != END_FILES) {
        printf("%sERROR%s --> %d files heard, expected %d\n", On_Red BWhite, Color_Off, end_files, END_FILES);
        result += -1;
    }
    if(fs_event->offset != (uint64_t)expected || end != (uint64_t)expected) {
        printf("%sERROR%s --> the queued events end at %lu (the stream at %lu once handed over), expected %lu\n",
            On_Red BWhite, Color_Off, (unsigned long)end, (unsigned long)fs_event->offset,
            (unsigned long)expected);
        result += -1;
    }
    fs_stop_watcher_event(fs_event);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    rmrdir(root4);
    return result;
}

/***************************************************************************
 *  The root deleted and created again during an overflow
 ***************************************************************************/
PRIVATE int do_test_root_reborn(BOOL recursive)
{
    int result = 0;
    const char *label = recursive? "fs_watcher root reborn, recursive" : "fs_watcher root reborn";
    char root3[PATH_MAX];
    build_path(root3, sizeof(root3), getenv("HOME"), "tests_yuneta", "fs_watcher_overflow_root", NULL);
    rmrdir(root3);
    mkrdir(root3, 02770);
    root_overflows = 0;
    root_files = 0;

    char title[128];
    snprintf(title, sizeof(title), "%s: watch", label);
    set_expected_results(title, NULL, NULL, NULL, 1);
    fs_event_t *fs_event = fs_create_watcher_event(
        yev_loop,
        root3,
        recursive? FS_FLAG_RECURSIVE_PATHS : 0,
        fs_callback_root,
        0,
        NULL,
        NULL
    );
    if(!fs_event) {
        return -1;
    }
    fs_start_watcher_event(fs_event);
    for(int i = 0; i < 5; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    /*
     *  With the loop stopped: the queue filled by one directory created and
     *  removed over and over, then the root removed and made again (its
     *  IN_DELETE_SELF and IN_IGNORED dropped with the rest)
     */
    snprintf(title, sizeof(title), "%s: the new root is watched", label);
    set_expected_results_unordered(
        title,
        json_pack("[{s:s},{s:s}]",
            "msg", "inotify IN_Q_OVERFLOW: events lost, rescanning the watched tree",
            "msg", "watched tree rescanned after lost inotify events"
        ),
        NULL, NULL, 1
    );
    char churn[PATH_MAX];
    build_path(churn, sizeof(churn), root3, "c", NULL);
    for(int i = 0; i < max_queued_events()/2 + 1024; i++) {
        if(mkdir(churn, 0700) < 0 || rmdir(churn) < 0) {
            printf("%sERROR%s --> cannot churn %s: %s\n", On_Red BWhite, Color_Off, churn, strerror(errno));
            result += -1;
            break;
        }
    }
    if(rmdir(root3) < 0 || mkdir(root3, 02770) < 0) {
        printf("%sERROR%s --> delete and create again %s: %s\n",
            On_Red BWhite, Color_Off, root3, strerror(errno));
        result += -1;
    }
    for(int i = 0; i < 200000 && (root_overflows == 0 || fs_event->rescan_dirs); i++) {
        yev_loop_run_once(yev_loop);
    }
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(root_overflows != 1) {
        printf("%sERROR%s --> the overflow told %d times, expected 1: the test did not test\n",
            On_Red BWhite, Color_Off, root_overflows);
        result += -1;
    }

    char path[PATH_MAX];
    build_path(path, sizeof(path), root3, "file.txt", NULL);
    int fd = open(path, O_CREAT|O_WRONLY, 0600);
    if(fd >= 0) {
        close(fd);
    }
    for(int i = 0; i < 50 && root_files == 0; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(root_files != 1) {
        printf("%sERROR%s --> a file in the root deleted and created again in the overflow: heard %d times, expected 1\n",
            On_Red BWhite, Color_Off, root_files);
        result += -1;
    }
    result += test_json(NULL);

    snprintf(title, sizeof(title), "%s: stop", label);
    set_expected_results(title, NULL, NULL, NULL, 1);
    fs_stop_watcher_event(fs_event);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    rmrdir(root3);
    return result;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;
    build_path(root, sizeof(root), getenv("HOME"), "tests_yuneta", "fs_watcher_overflow", NULL);
    rmrdir(root);
    mkrdir(root, 02770);

    n_dirs = max_queued_events() + EXTRA_DIRS;
    told = json_object();

    set_expected_results("fs_watcher overflow: watch", NULL, NULL, NULL, 1);
    fs_event_t *fs_event = fs_create_watcher_event(
        yev_loop,
        root,
        FS_FLAG_RECURSIVE_PATHS,
        fs_callback,
        0,
        NULL,
        NULL
    );
    if(!fs_event) {
        return -1;
    }
    fs_start_watcher_event(fs_event);
    for(int i = 0; i < 5; i++) {
        yev_loop_run_once(yev_loop);
    }
    char reborn[PATH_MAX];
    build_path(reborn, sizeof(reborn), root, REBORN_DIR, NULL);
    if(mkdir(reborn, 02770) < 0) {
        printf("%sERROR%s --> mkdir %s: %s\n", On_Red BWhite, Color_Off, reborn, strerror(errno));
        result += -1;
    }
    for(int i = 0; i < 5; i++) {
        yev_loop_run_once(yev_loop);    // its IN_CREATE: watched
    }
    if(json_integer_value(json_object_get(told, REBORN_DIR)) != 1) {
        printf("%sERROR%s --> %s not heard before the flood\n", On_Red BWhite, Color_Off, REBORN_DIR);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  The flood, with the loop stopped: one IN_CREATE per directory, more
     *  than the queue holds
     */
    set_expected_results_unordered(
        "fs_watcher overflow: the pass tells every directory, the loop stays awake",
        json_pack("[{s:s},{s:s}]",
            "msg", "inotify IN_Q_OVERFLOW: events lost, rescanning the watched tree",
            "msg", "watched tree rescanned after lost inotify events"
        ),
        NULL, NULL, 1
    );
    for(int i = 1; i <= n_dirs; i++) {
        char name[32], path[PATH_MAX];
        snprintf(name, sizeof(name), "d%06d", i);
        build_path(path, sizeof(path), root, name, NULL);
        if(mkdir(path, 02770) < 0) {
            printf("%sERROR%s --> mkdir %s: %s\n", On_Red BWhite, Color_Off, path, strerror(errno));
            result += -1;
            break;
        }
    }
    if(rmdir(reborn) < 0 || mkdir(reborn, 02770) < 0) {
        printf("%sERROR%s --> delete and create again %s: %s\n",
            On_Red BWhite, Color_Off, reborn, strerror(errno));
        result += -1;
    }

    yev_event_h yev_probe = yev_create_timer_event(yev_loop, probe_callback, 0);
    probe_last = time_in_milliseconds_monotonic();  // a loop deaf from the start counts
    yev_start_timer_event(yev_probe, PROBE_MS, TRUE);

    uint64_t t0 = time_in_milliseconds_monotonic();
    int quiet = 0;
    int last = -1;
    while(quiet < 50 && time_in_milliseconds_monotonic() - t0 < 10*60*1000) {  // by time: a slice per turn
        yev_loop_run_once(yev_loop);
        int n = count_told() + rescan_dirs;
        if(n == last && !fs_event->rescan_dirs) {
            quiet++;
        } else {
            quiet = 0;
        }
        last = n;
    }
    uint64_t t1 = time_in_milliseconds_monotonic();
    yev_stop_event(yev_probe);
    for(int i = 0; i < 5; i++) {
        yev_loop_run_once(yev_loop);
    }
    yev_destroy_event(yev_probe);

    int n_told = count_told();
    uint64_t pass_us = (t1 - t0) * 1000;
    uint64_t own_us = pass_us > owner_us? (pass_us - owner_us) / (rescan_dirs? rescan_dirs: 1): 0;
    printf("     %d directories: %d told, %d overflow(s), %d rescanned, %lu ms (%lu in the owner, %lu us per directory in the watcher), the loop deaf at most %lu ms\n",
        n_dirs, n_told, overflows, rescan_dirs, (unsigned long)(t1 - t0),
        (unsigned long)(owner_us/1000), (unsigned long)own_us, (unsigned long)probe_max_gap);
    if(overflows < 1) {
        printf("%sERROR%s --> no overflow: the test did not test\n", On_Red BWhite, Color_Off);
        result += -1;
    }
    if(n_told != n_dirs + 1) {
        printf("%sERROR%s --> %d of %d directories told to the owner\n",
            On_Red BWhite, Color_Off, n_told, n_dirs + 1);
        result += -1;
    }
    if(own_us > MAX_OWN_US) {
        /*
         *  7.25.10 indexed the watched paths once per SLICE: 254 us per
         *  directory at 69632 (and growing with the tree), 73 once per pass
         */
        printf("%sERROR%s --> the watcher spent %lu us per directory (at most %d)\n",
            On_Red BWhite, Color_Off, (unsigned long)own_us, MAX_OWN_US);
        result += -1;
    }
    if(probe_max_gap > MAX_DEAF_MS) {
        printf("%sERROR%s --> the loop was deaf for %lu ms (at most %d)\n",
            On_Red BWhite, Color_Off, (unsigned long)probe_max_gap, MAX_DEAF_MS);
        result += -1;
    }
    result += test_json(NULL);

    /*
     *  The last directory was born while its IN_CREATE was being dropped:
     *  after the pass it is watched, and a file created in it is heard.
     *  So is one in the directory deleted and created again.
     */
    set_expected_results("fs_watcher overflow: a directory born in the overflow is watched", NULL, NULL, NULL, 1);
    char path[PATH_MAX], name[32];
    snprintf(name, sizeof(name), "d%06d", n_dirs);
    build_path(path, sizeof(path), root, name, "file.txt", NULL);
    files_created = 0;
    int fd = open(path, O_CREAT|O_WRONLY, 0600);
    if(fd >= 0) {
        close(fd);
    }
    for(int i = 0; i < 50 && files_created == 0; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(files_created != 1) {
        printf("%sERROR%s --> a file in a directory born in the overflow: heard %d times, expected 1\n",
            On_Red BWhite, Color_Off, files_created);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("fs_watcher overflow: a directory reborn in the overflow is watched", NULL, NULL, NULL, 1);
    build_path(path, sizeof(path), reborn, "file.txt", NULL);
    files_created = 0;
    fd = open(path, O_CREAT|O_WRONLY, 0600);
    if(fd >= 0) {
        close(fd);
    }
    for(int i = 0; i < 50 && files_created == 0; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(files_created != 1) {
        printf("%sERROR%s --> a file in a directory deleted and created again in the overflow: heard %d times, expected 1\n",
            On_Red BWhite, Color_Off, files_created);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("fs_watcher overflow: stop", NULL, NULL, NULL, 1);
    fs_stop_watcher_event(fs_event);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    JSON_DECREF(told)
    rmrdir(root);
    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    gbmem_setup(
        0,                          // mem_max_block, the default
        (size_t)512*1024*1024,      // mem_max_system_memory: ~70000 watched paths
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

    int result = do_test_stop_on_overflow();
    result += do_test_queued_events_end();
    result += do_test_root_reborn(FALSE);
    result += do_test_root_reborn(TRUE);
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
