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
 *  And a root that cannot be watched (do_test_root_unwatchable: mode 000,
 *  SKIPPED as root): no watcher is created. Up to 7.25.20 a watcher was
 *  handed over all the same, watching nothing.
 *
 *  And FS_FLAG_DIR_FDS (do_test_dir_fds): a subdirectory created is told
 *  with `subdir_wd`, whose descriptor (fs_watcher_dir_fd()) is that very
 *  directory; the root has none (ENOTSUP). The subdirectory removed and
 *  made again before the watcher reads anything: its old watch answers
 *  ENOENT, the new one has a descriptor of its own on another inode, an
 *  openat() through the old descriptor does not reach the new directory,
 *  and once the events held by the old descriptor come the table holds the
 *  root and the new directory only (the descriptor held IN_DELETE_SELF and
 *  IN_IGNORED until it was closed). After the stop the process holds the
 *  descriptors it held before the watcher (/proc/self/fd): none leaks.
 *
 *  And the open-files limit (do_test_dir_fds_limit): with a small soft
 *  limit, the directories watched through descriptors are said once when
 *  they reach half of it; out of descriptors (EMFILE), the failure to open
 *  a directory is said ONCE however many fail (they are watched by path,
 *  counted), and when one opens again, how many went by path. The half is
 *  the process's (do_test_dir_fds_limit_process): two watchers of 40
 *  directories each, under a soft limit of 128, say it once. Up to 7.25.21
 *  each watcher counted its own, and neither said it.
 *
 *  And an end of the queued events said past the stream
 *  (do_test_padded_end): FIONREAD fails once (__wrap_ioctl), so
 *  fs_queued_events_end() counts a read whole, unseen. The watcher is quiet
 *  after it, and must still reach that end: an FS_BATCH_END at it, the
 *  stream there, and the next events after it. Up to 7.25.21 an owner
 *  waiting for that end waited for unrelated events, for ever on a quiet
 *  watcher.
 *
 *  The end said when FIONREAD fails is not short
 *  (do_test_padded_end_not_short): with far more than a read queued, every
 *  event queued when it was said ends at it or before. Up to 7.25.21 a
 *  read whole was all that was added, and the owner did too soon what it
 *  left for after them. And a FIONREAD that keeps failing
 *  (do_test_fionread_broken) ends the watcher, its owner told
 *  (FS_WATCHER_GONE): up to 7.25.21 the end was never closed.
 *
 *  A subdirectory whose watch cannot be made (ENOSPC at max_user_watches,
 *  __wrap_inotify_add_watch) is tried again at the end of each batch
 *  (do_test_unwatched_retry): once there are watches again, it is handed
 *  as created with its watch, and what is made in it is heard. Up to
 *  7.25.21 it was never watched: nothing made in it was ever heard.
 *
 *  And the ROOT deleted and created again while the queue is full
 *  (do_test_root_reborn, recursive and not): after the pass the new root
 *  is watched, a file created in it is heard. Up to 7.25.20 the pass
 *  watched again the directories it met, never the root it started from.
 *  And the entry of the old root, whose IN_IGNORED went with the rest, is
 *  taken out of the table of watches once the stream is past where it
 *  would have come: up to 7.25.21 it stayed for good.
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
#include <dirent.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <stdarg.h>

#include <gobj.h>
#include <kwid.h>
#include <helpers.h>
#include <yev_loop.h>
#include <fs_watcher.h>
#include <testing.h>

#define APP "test_fs_watcher_overflow"

/*
 *  FIONREAD fails `fionread_fails` times (linked with --wrap=ioctl)
 */
int __real_ioctl(int fd, unsigned long request, ...);
int __wrap_ioctl(int fd, unsigned long request, ...);
static int fionread_fails = 0;

int __wrap_ioctl(int fd, unsigned long request, ...)
{
    va_list ap;
    va_start(ap, request);
    void *arg = va_arg(ap, void *);
    va_end(ap);
    if(request == FIONREAD && fionread_fails > 0) {
        fionread_fails--;
        errno = EIO;
        return -1;
    }
    return __real_ioctl(fd, request, arg);
}

int __real_inotify_add_watch(int fd, const char *pathname, uint32_t mask);
int __wrap_inotify_add_watch(int fd, const char *pathname, uint32_t mask);
static const char *watch_enospc_path = NULL;   // its watch fails, ENOSPC

int __wrap_inotify_add_watch(int fd, const char *pathname, uint32_t mask)
{
    if(watch_enospc_path && strcmp(pathname, watch_enospc_path) == 0) {
        errno = ENOSPC;
        return -1;
    }
    return __real_inotify_add_watch(fd, pathname, mask);
}

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

PRIVATE int count_open_fds(void)    // -1: /proc/self/fd cannot be listed
{
    DIR *d = opendir("/proc/self/fd");
    if(!d) {
        return -1;
    }
    int n = 0;
    struct dirent *de;
    while((de = readdir(d)) != NULL) {
        if(de->d_name[0] != '.') {
            n++;
        }
    }
    closedir(d);
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
    if(fs_start_watcher_event(fs_event) < 0) {
        printf("%sERROR%s --> the watcher could not be started\n", On_Red BWhite, Color_Off);
        fs_stop_watcher_event(fs_event);
        return -1;
    }
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
 *  A root that cannot be watched
 ***************************************************************************/
PRIVATE int do_test_root_unwatchable(void)
{
    if(geteuid() == 0) {
        printf("     SKIPPED the unwatchable root, running as root: a mode of 000 is still read\n");
        return 0;
    }
    int result = 0;
    char root5[PATH_MAX];
    build_path(root5, sizeof(root5), getenv("HOME"), "tests_yuneta", "fs_watcher_unwatchable", NULL);
    rmrdir(root5);
    mkrdir(root5, 02770);
    chmod(root5, 0);

    set_expected_results(
        "fs_watcher unwatchable root: no watcher",
        json_pack("[{s:s}]",
            "msg", "inotify_add_watch() FAILED"
        ),
        NULL, NULL, 1
    );
    fs_event_t *fs_event = fs_create_watcher_event(
        yev_loop,
        root5,
        FS_FLAG_RECURSIVE_PATHS,
        fs_callback_end,
        0,
        NULL,
        NULL
    );
    if(fs_event) {
        printf("%sERROR%s --> a watcher of a root that cannot be watched was created\n", On_Red BWhite, Color_Off);
        result += -1;
        fs_stop_watcher_event(fs_event);
        for(int i = 0; i < 5; i++) {
            yev_loop_run_once(yev_loop);
        }
    }
    result += test_json(NULL);

    chmod(root5, 02770);
    rmrdir(root5);
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
    if(fs_start_watcher_event(fs_event) < 0) {
        printf("%sERROR%s --> the watcher could not be started\n", On_Red BWhite, Color_Off);
        fs_stop_watcher_event(fs_event);
        return -1;
    }
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
 *  An end said past the stream, on a quiet watcher
 ***************************************************************************/
PRIVATE int pad_files = 0;
PRIVATE int pad_batch_ends = 0;
PRIVATE uint64_t pad_last_end = 0;

PRIVATE uint64_t pad_last_event_end = 0;
PRIVATE int pad_gone = 0;

PRIVATE int fs_callback_pad(fs_event_t *fs_event)
{
    if(fs_event->fs_type == FS_FILE_CREATED_TYPE) {
        pad_files++;
        if(fs_event->offset_end > pad_last_event_end) {
            pad_last_event_end = fs_event->offset_end;
        }
    }
    if(fs_event->fs_type == FS_WATCHER_GONE_TYPE) {
        pad_gone++;
    }
    if(fs_event->fs_type == FS_BATCH_END_TYPE) {
        pad_batch_ends++;
        pad_last_end = fs_event->offset;
    }
    return 0;
}

PRIVATE int create_file_in(const char *dir, const char *name)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), dir, name, NULL);
    int fd = open(path, O_CREAT|O_WRONLY, 0600);
    if(fd < 0) {
        return -1;
    }
    close(fd);
    return 0;
}

PRIVATE int do_test_padded_end(void)
{
    int result = 0;
    char root5[PATH_MAX];
    build_path(root5, sizeof(root5), getenv("HOME"), "tests_yuneta", "fs_watcher_padded_end", NULL);
    rmrdir(root5);
    mkrdir(root5, 02770);
    pad_files = 0;

    set_expected_results(
        "fs_watcher padded end: an end said past the stream is reached on a quiet watcher",
        json_pack("[{s:s}]",
            "msg", "ioctl(FIONREAD) FAILED: the events queued by now are not counted"
        ),
        NULL, NULL, 1
    );
    fs_event_t *fs_event = fs_create_watcher_event(
        yev_loop,
        root5,
        FS_FLAG_BATCH_END,
        fs_callback_pad,
        0,
        NULL,
        NULL
    );
    if(!fs_event) {
        return -1;
    }
    if(fs_start_watcher_event(fs_event) < 0) {
        printf("%sERROR%s --> the watcher could not be started\n", On_Red BWhite, Color_Off);
        fs_stop_watcher_event(fs_event);
        return -1;
    }
    for(int i = 0; i < 5; i++) {
        yev_loop_run_once(yev_loop);
    }
    create_file_in(root5, "first");
    for(int i = 0; i < 50 && pad_files < 1; i++) {
        yev_loop_run_once(yev_loop);
    }

    fionread_fails = 1;
    uint64_t end = fs_queued_events_end(fs_event);
    fionread_fails = 0;
    if(end <= fs_event->offset) {
        printf("%sERROR%s --> FIONREAD failed and the end (%lu) is not past the stream (%lu)\n",
            On_Red BWhite, Color_Off, (unsigned long)end, (unsigned long)fs_event->offset);
        result += -1;
    }

    pad_batch_ends = 0;
    pad_last_end = 0;
    for(int i = 0; i < 50 && pad_batch_ends == 0; i++) {
        usleep(2000);                   // nothing happens in the tree; the turn
        yev_loop_run_once(yev_loop);    // of the loop does not wait: time passes here
    }
    if(pad_batch_ends == 0 || pad_last_end < end || fs_event->offset < end) {
        printf("%sERROR%s --> a quiet watcher did not reach the end said past it: %d FS_BATCH_END, at %lu, the stream at %lu, the end %lu\n",
            On_Red BWhite, Color_Off, pad_batch_ends, (unsigned long)pad_last_end,
            (unsigned long)fs_event->offset, (unsigned long)end);
        result += -1;
    }

    create_file_in(root5, "second");
    for(int i = 0; i < 50 && pad_files < 2; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(pad_files != 2 || fs_event->offset <= end) {
        printf("%sERROR%s --> the events after the end: %d files heard (expected 2), the stream at %lu (past %lu)\n",
            On_Red BWhite, Color_Off, pad_files, (unsigned long)fs_event->offset, (unsigned long)end);
        result += -1;
    }

    fs_stop_watcher_event(fs_event);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    rmrdir(root5);
    return result;
}

/***************************************************************************
 *  The end said when FIONREAD fails holds every event queued
 ***************************************************************************/
PRIVATE int do_test_padded_end_not_short(void)
{
    int result = 0;
    char root7[PATH_MAX];
    build_path(root7, sizeof(root7), getenv("HOME"), "tests_yuneta", "fs_watcher_pad_not_short", NULL);
    rmrdir(root7);
    mkrdir(root7, 02770);
    pad_files = 0;
    pad_last_event_end = 0;

    set_expected_results(
        "fs_watcher padded end: not short",
        json_pack("[{s:s}]",
            "msg", "ioctl(FIONREAD) FAILED: the events queued by now are not counted"
        ),
        NULL, NULL, 1
    );
    fs_event_t *fs_event = fs_create_watcher_event(
        yev_loop, root7, FS_FLAG_BATCH_END, fs_callback_pad, 0, NULL, NULL
    );
    if(!fs_event || fs_start_watcher_event(fs_event) < 0) {
        printf("%sERROR%s --> the watcher could not be started\n", On_Red BWhite, Color_Off);
        return -1;
    }
    for(int i = 0; i < 5; i++) {
        yev_loop_run_once(yev_loop);
    }

    /*
     *  1000 files: far more than a read (32 events of the largest size)
     */
    char name[32];
    for(int i = 0; i < 1000; i++) {
        snprintf(name, sizeof(name), "f%04d", i);
        create_file_in(root7, name);
    }
    fionread_fails = 1;
    uint64_t end = fs_queued_events_end(fs_event);
    fionread_fails = 0;

    for(int i = 0; i < 500 && pad_files < 1000; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(pad_files != 1000 || pad_last_event_end > end) {
        printf("%sERROR%s --> the end said with FIONREAD failed (%lu) is short: %d files heard, the last ends at %lu\n",
            On_Red BWhite, Color_Off, (unsigned long)end, pad_files, (unsigned long)pad_last_event_end);
        result += -1;
    }

    pad_batch_ends = 0;
    pad_last_end = 0;
    for(int i = 0; i < 50 && pad_last_end < end; i++) {
        usleep(2000);
        yev_loop_run_once(yev_loop);
    }
    if(fs_event->offset < end) {
        printf("%sERROR%s --> a quiet watcher did not reach the end said past it: the stream at %lu, the end %lu\n",
            On_Red BWhite, Color_Off, (unsigned long)fs_event->offset, (unsigned long)end);
        result += -1;
    }

    fs_stop_watcher_event(fs_event);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);
    rmrdir(root7);
    return result;
}

/***************************************************************************
 *  A FIONREAD that keeps failing ends the watcher, its owner told
 ***************************************************************************/
PRIVATE int do_test_fionread_broken(void)
{
    int result = 0;
    char root8[PATH_MAX];
    build_path(root8, sizeof(root8), getenv("HOME"), "tests_yuneta", "fs_watcher_fionread_broken", NULL);
    rmrdir(root8);
    mkrdir(root8, 02770);
    pad_gone = 0;

    set_expected_results(
        "fs_watcher FIONREAD broken: the watcher is gone, its owner told",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "ioctl(FIONREAD) FAILED: the events queued by now are not counted",
            "msg", "ioctl(FIONREAD) FAILED: the events queued by now are not counted",
            "msg", "The events queued by the kernel cannot be counted: an end said past the stream cannot be closed, the watcher is gone"
        ),
        NULL, NULL, 1
    );
    fs_event_t *fs_event = fs_create_watcher_event(
        yev_loop, root8, FS_FLAG_BATCH_END, fs_callback_pad, 0, NULL, NULL
    );
    if(!fs_event || fs_start_watcher_event(fs_event) < 0) {
        printf("%sERROR%s --> the watcher could not be started\n", On_Red BWhite, Color_Off);
        return -1;
    }
    for(int i = 0; i < 5; i++) {
        yev_loop_run_once(yev_loop);
    }

    fionread_fails = 1000;
    fs_queued_events_end(fs_event);
    for(int i = 0; i < 50 && pad_gone == 0; i++) {
        usleep(2000);
        yev_loop_run_once(yev_loop);
    }
    fionread_fails = 0;
    if(pad_gone != 1) {
        printf("%sERROR%s --> FIONREAD broken: the owner was told the watcher is gone %d times, expected 1\n",
            On_Red BWhite, Color_Off, pad_gone);
        result += -1;
        fs_stop_watcher_event(fs_event);
    }
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);
    rmrdir(root8);
    return result;
}

/***************************************************************************
 *  A subdirectory that could not be watched is watched later
 ***************************************************************************/
PRIVATE int unw_created_unwatched = 0;
PRIVATE int unw_created_watched = 0;
PRIVATE int unw_files_in_a = 0;

PRIVATE int fs_callback_unwatched(fs_event_t *fs_event)
{
    if(fs_event->fs_type == FS_SUBDIR_CREATED_TYPE &&
            strcmp((const char *)fs_event->filename, "a") == 0) {
        if(fs_event->subdir_wd < 0) {
            unw_created_unwatched++;
        } else {
            unw_created_watched++;
        }
    }
    if(fs_event->fs_type == FS_FILE_CREATED_TYPE) {
        const char *dir = (const char *)fs_event->directory;
        size_t n = strlen(dir);
        if(n >= 2 && strcmp(dir + n - 2, "/a") == 0) {
            unw_files_in_a++;
        }
    }
    return 0;
}

PRIVATE int do_test_unwatched_retry(void)
{
    int result = 0;
    char root9[PATH_MAX], dir_a[PATH_MAX], dir_b[PATH_MAX];
    build_path(root9, sizeof(root9), getenv("HOME"), "tests_yuneta", "fs_watcher_unwatched", NULL);
    build_path(dir_a, sizeof(dir_a), root9, "a", NULL);
    build_path(dir_b, sizeof(dir_b), root9, "b", NULL);
    rmrdir(root9);
    mkrdir(root9, 02770);
    unw_created_unwatched = unw_created_watched = unw_files_in_a = 0;

    set_expected_results(
        "fs_watcher: a directory that could not be watched is watched later",
        json_pack("[{s:s},{s:s}]",
            "msg", "Cannot watch a directory, out of inotify watches or memory: tried again at each batch (and the next ones that fail, counted)",
            "msg", "Directories watched again: every one that could not be is watched now"
        ),
        NULL, NULL, 1
    );
    fs_event_t *fs_event = fs_create_watcher_event(
        yev_loop, root9, FS_FLAG_RECURSIVE_PATHS, fs_callback_unwatched, 0, NULL, NULL
    );
    if(!fs_event || fs_start_watcher_event(fs_event) < 0) {
        printf("%sERROR%s --> the watcher could not be started\n", On_Red BWhite, Color_Off);
        return -1;
    }

    /*
     *  Out of watches: "a" is created, not watched, and a file in it is
     *  not heard
     */
    watch_enospc_path = dir_a;
    mkdir(dir_a, 02770);
    for(int i = 0; i < 50 && unw_created_unwatched == 0; i++) {
        yev_loop_run_once(yev_loop);
    }
    create_file_in(dir_a, "made_while_unwatched");

    /*
     *  Watches again: the next batch ("b") watches "a", hands it as created
     *  with its watch, and a file in it is heard
     */
    watch_enospc_path = NULL;
    mkdir(dir_b, 02770);
    for(int i = 0; i < 50 && unw_created_watched == 0; i++) {
        yev_loop_run_once(yev_loop);
    }
    create_file_in(dir_a, "made_once_watched");
    for(int i = 0; i < 50 && unw_files_in_a == 0; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(unw_created_unwatched != 1 || unw_created_watched != 1 || unw_files_in_a != 1 ||
            json_object_size(fs_event->jn_unwatched) != 0) {
        printf("%sERROR%s --> unwatched retry: created unwatched %d (1), then watched %d (1), files heard in it %d (1), left unwatched %d (0)\n",
            On_Red BWhite, Color_Off, unw_created_unwatched, unw_created_watched, unw_files_in_a,
            (int)json_object_size(fs_event->jn_unwatched));
        result += -1;
    }

    fs_stop_watcher_event(fs_event);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);
    rmrdir(root9);
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
    if(fs_start_watcher_event(fs_event) < 0) {
        printf("%sERROR%s --> the watcher could not be started\n", On_Red BWhite, Color_Off);
        fs_stop_watcher_event(fs_event);
        return -1;
    }
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
    if(fs_event->rescan_dirs) {
        printf("%sERROR%s --> the pass after the overflow did not end\n", On_Red BWhite, Color_Off);
        result += -1;
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
    /*
     *  The wd of the old root is gone, and its IN_IGNORED with it: once
     *  the stream is past the point where it would have come, its entry
     *  goes. Up to 7.25.21 it stayed for good.
     */
    if(json_object_size(fs_event->jn_tracked_paths) != 1) {
        printf("%sERROR%s --> the table of watches holds %d entries, expected 1 (the new root)\n",
            On_Red BWhite, Color_Off, (int)json_object_size(fs_event->jn_tracked_paths));
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
 *  FS_FLAG_DIR_FDS
 ***************************************************************************/
PRIVATE int dirfd_subdir_wd = -1;
PRIVATE int dirfd_created = 0;
PRIVATE int dirfd_deleted = 0;

PRIVATE int fs_callback_dirfd(fs_event_t *fs_event)
{
    if(fs_event->fs_type == FS_SUBDIR_CREATED_TYPE) {
        dirfd_created++;
        dirfd_subdir_wd = fs_event->subdir_wd;
    } else if(fs_event->fs_type == FS_SUBDIR_DELETED_TYPE) {
        dirfd_deleted++;
    }
    return 0;
}

PRIVATE int do_test_dir_fds(void)
{
    int result = 0;
    char root4[PATH_MAX], sub[PATH_MAX];
    build_path(root4, sizeof(root4), getenv("HOME"), "tests_yuneta", "fs_watcher_dir_fds", NULL);
    build_path(sub, sizeof(sub), root4, "k", NULL);
    rmrdir(root4);
    mkrdir(root4, 02770);

    int fds_before = count_open_fds();

    set_expected_results("fs_watcher dir fds", NULL, NULL, NULL, 1);
    fs_event_t *fs_event = fs_create_watcher_event(
        yev_loop, root4, FS_FLAG_RECURSIVE_PATHS|FS_FLAG_DIR_FDS, fs_callback_dirfd, 0, NULL, NULL
    );
    if(!fs_event || fs_start_watcher_event(fs_event) < 0) {
        printf("%sERROR%s --> dir fds: the watcher could not be started\n", On_Red BWhite, Color_Off);
        return -1;
    }
    for(int i = 0; i < 5; i++) {
        yev_loop_run_once(yev_loop);
    }

    errno = 0;
    int root_wd = -1;
    const char *s_wd; json_t *jn_path;
    json_object_foreach(fs_event->jn_tracked_paths, s_wd, jn_path) {
        root_wd = atoi(s_wd);
    }
    if(fs_watcher_dir_fd(fs_event, root_wd) != -1 || errno != ENOTSUP) {
        printf("%sERROR%s --> dir fds: the root must have no descriptor (ENOTSUP)\n", On_Red BWhite, Color_Off);
        result += -1;
    }

    mkdir(sub, 0700);
    for(int i = 0; i < 20 && dirfd_created == 0; i++) {
        yev_loop_run_once(yev_loop);
    }
    int old_wd = dirfd_subdir_wd;
    int old_fd = fs_watcher_dir_fd(fs_event, old_wd);
    struct stat st_old, st_sub;
    if(old_wd < 0 || old_fd < 0 || fstat(old_fd, &st_old) < 0 || stat(sub, &st_sub) < 0 ||
            st_old.st_ino != st_sub.st_ino) {
        printf("%sERROR%s --> dir fds: the subdirectory has no descriptor of its own inode (wd %d, fd %d)\n",
            On_Red BWhite, Color_Off, old_wd, old_fd);
        result += -1;
    }

    /*
     *  Removed and made again before anything is read
     */
    rmdir(sub);
    mkdir(sub, 0700);
    int held = old_fd >= 0? openat(old_fd, "x", O_CREAT|O_WRONLY|O_CLOEXEC, 0600) : -1;
    if(held >= 0) {
        close(held);
        printf("%sERROR%s --> dir fds: a file was made through the descriptor of a directory gone\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    dirfd_created = 0;
    for(int i = 0; i < 30 && dirfd_created == 0; i++) {
        yev_loop_run_once(yev_loop);
    }
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    errno = 0;
    if(fs_watcher_dir_fd(fs_event, old_wd) != -1 || errno != ENOENT) {
        printf("%sERROR%s --> dir fds: the watch of the directory gone must answer ENOENT\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    int new_fd = fs_watcher_dir_fd(fs_event, dirfd_subdir_wd);
    struct stat st_new;
    if(dirfd_deleted != 1 || dirfd_subdir_wd == old_wd || new_fd < 0 ||
            fstat(new_fd, &st_new) < 0 || stat(sub, &st_sub) < 0 || st_new.st_ino != st_sub.st_ino) {
        printf("%sERROR%s --> dir fds: the directory made again is not watched with a descriptor of its own (deleted %d, wd %d -> %d)\n",
            On_Red BWhite, Color_Off, dirfd_deleted, old_wd, dirfd_subdir_wd);
        result += -1;
    }
    if(json_object_size(fs_event->jn_tracked_paths) != 2 ||
            json_object_size(fs_event->jn_tracked_fds) != 1) {
        printf("%sERROR%s --> dir fds: the table holds %d watches and %d descriptors, expected 2 and 1\n",
            On_Red BWhite, Color_Off, (int)json_object_size(fs_event->jn_tracked_paths),
            (int)json_object_size(fs_event->jn_tracked_fds));
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("fs_watcher dir fds: stop", NULL, NULL, NULL, 1);
    fs_stop_watcher_event(fs_event);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);

    int fds_after = count_open_fds();
    if(fds_before < 0 || fds_after != fds_before) {
        printf("%sERROR%s --> dir fds: %d descriptors open after the stop, %d before the watcher\n",
            On_Red BWhite, Color_Off, fds_after, fds_before);
        result += -1;
    }
    rmrdir(root4);
    return result;
}

/***************************************************************************
 *  FS_FLAG_DIR_FDS and the open-files limit
 ***************************************************************************/
PRIVATE int fs_callback_count(fs_event_t *fs_event)
{
    if(fs_event->fs_type == FS_SUBDIR_CREATED_TYPE) {
        dirfd_created++;
    }
    return 0;
}

PRIVATE int do_test_dir_fds_limit(void)
{
    int result = 0;
    char root5[PATH_MAX];
    build_path(root5, sizeof(root5), getenv("HOME"), "tests_yuneta", "fs_watcher_dir_fds_limit", NULL);
    rmrdir(root5);
    mkrdir(root5, 02770);

    struct rlimit rl_saved;
    getrlimit(RLIMIT_NOFILE, &rl_saved);
    int base = count_open_fds();
    if(base < 0 || base + 72 > 128 || rl_saved.rlim_max < 128) {
        printf("     SKIPPED: dir fds limit, %d descriptors open, hard limit %lu\n",
            base, (unsigned long)rl_saved.rlim_max);
        rmrdir(root5);
        return 0;
    }

    set_expected_results("fs_watcher dir fds limit",
        json_pack("[{s:s},{s:s},{s:s}]",
            "msg", "Directories watched through descriptors: half of the open-files limit",
            "msg", "Cannot open a directory to watch it through its descriptor: watched by its path (and the next ones that fail, counted)",
            "msg", "Directories watched through their descriptor again"
        ),
        NULL, NULL, 1
    );
    fs_event_t *fs_event = fs_create_watcher_event(
        yev_loop, root5, FS_FLAG_RECURSIVE_PATHS|FS_FLAG_DIR_FDS, fs_callback_count, 0, NULL, NULL
    );
    if(!fs_event || fs_start_watcher_event(fs_event) < 0) {
        printf("%sERROR%s --> dir fds limit: the watcher could not be started\n", On_Red BWhite, Color_Off);
        return -1;
    }

    /*
     *  A soft limit of 128: 64 directories reach half of it
     */
    struct rlimit rl = rl_saved;
    rl.rlim_cur = 128;
    setrlimit(RLIMIT_NOFILE, &rl);
    char sub[PATH_MAX];
    char name[32];
    dirfd_created = 0;
    for(int i = 0; i < 64; i++) {
        snprintf(name, sizeof(name), "k%02d", i);
        build_path(sub, sizeof(sub), root5, name, NULL);
        mkdir(sub, 0700);
    }
    for(int i = 0; i < 100 && dirfd_created < 64; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(!fs_event->dir_fds_warned) {
        printf("%sERROR%s --> dir fds limit: half of the limit not said (%d dirs, %d fds)\n",
            On_Red BWhite, Color_Off, dirfd_created, (int)json_object_size(fs_event->jn_tracked_fds));
        result += -1;
    }

    /*
     *  Out of descriptors: three directories, one error, three by path
     */
    rl.rlim_cur = (rlim_t)(count_open_fds() - 1);  // its own count held one more
    setrlimit(RLIMIT_NOFILE, &rl);
    dirfd_created = 0;
    for(int i = 0; i < 3; i++) {
        snprintf(name, sizeof(name), "e%02d", i);
        build_path(sub, sizeof(sub), root5, name, NULL);
        mkdir(sub, 0700);
    }
    for(int i = 0; i < 50 && dirfd_created < 3; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(fs_event->dir_fds_by_path != 3) {
        printf("%sERROR%s --> dir fds limit: %ld directories by path, expected 3\n",
            On_Red BWhite, Color_Off, (long)fs_event->dir_fds_by_path);
        result += -1;
    }

    /*
     *  Room again: one opens, and the count is said
     */
    setrlimit(RLIMIT_NOFILE, &rl_saved);
    dirfd_created = 0;
    build_path(sub, sizeof(sub), root5, "again", NULL);
    mkdir(sub, 0700);
    for(int i = 0; i < 50 && dirfd_created < 1; i++) {
        yev_loop_run_once(yev_loop);
    }
    if(fs_event->dir_fds_by_path != 0) {
        printf("%sERROR%s --> dir fds limit: the count was not said when a descriptor opened again\n",
            On_Red BWhite, Color_Off);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("fs_watcher dir fds limit: stop", NULL, NULL, NULL, 1);
    fs_stop_watcher_event(fs_event);
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);
    setrlimit(RLIMIT_NOFILE, &rl_saved);
    rmrdir(root5);
    return result;
}

/***************************************************************************
 *  The half of the limit counts the descriptors of every watcher
 ***************************************************************************/
PRIVATE int do_test_dir_fds_limit_process(void)
{
    int result = 0;
    char root6[PATH_MAX];
    build_path(root6, sizeof(root6), getenv("HOME"), "tests_yuneta", "fs_watcher_dir_fds_process", NULL);
    rmrdir(root6);
    mkrdir(root6, 02770);

    struct rlimit rl_saved;
    getrlimit(RLIMIT_NOFILE, &rl_saved);
    int base = count_open_fds();
    if(base < 0 || base + 2 + 80 > 128 || rl_saved.rlim_max < 128) {
        printf("     SKIPPED: dir fds of the process, %d descriptors open, hard limit %lu\n",
            base, (unsigned long)rl_saved.rlim_max);
        rmrdir(root6);
        return 0;
    }

    set_expected_results("fs_watcher dir fds of the process",
        json_pack("[{s:s}]",
            "msg", "Directories watched through descriptors: half of the open-files limit"
        ),
        NULL, NULL, 1
    );

    struct rlimit rl = rl_saved;
    rl.rlim_cur = 128;
    setrlimit(RLIMIT_NOFILE, &rl);

    fs_event_t *fs_events[2] = {0};
    char sub[PATH_MAX];
    char name[32];
    for(int w = 0; w < 2; w++) {
        char wroot[PATH_MAX];
        snprintf(name, sizeof(name), "w%d", w);
        build_path(wroot, sizeof(wroot), root6, name, NULL);
        mkrdir(wroot, 02770);
        fs_events[w] = fs_create_watcher_event(
            yev_loop, wroot, FS_FLAG_RECURSIVE_PATHS|FS_FLAG_DIR_FDS, fs_callback_count, 0, NULL, NULL
        );
        if(!fs_events[w] || fs_start_watcher_event(fs_events[w]) < 0) {
            printf("%sERROR%s --> dir fds of the process: the watcher could not be started\n",
                On_Red BWhite, Color_Off);
            setrlimit(RLIMIT_NOFILE, &rl_saved);
            return -1;
        }
        dirfd_created = 0;
        for(int i = 0; i < 40; i++) {
            snprintf(name, sizeof(name), "k%02d", i);
            build_path(sub, sizeof(sub), wroot, name, NULL);
            mkdir(sub, 0700);
        }
        for(int i = 0; i < 100 && dirfd_created < 40; i++) {
            yev_loop_run_once(yev_loop);
        }
    }
    if(fs_events[0]->dir_fds_warned || !fs_events[1]->dir_fds_warned) {
        printf("%sERROR%s --> dir fds of the process: the half not said by the second watcher (%d, %d)\n",
            On_Red BWhite, Color_Off, fs_events[0]->dir_fds_warned, fs_events[1]->dir_fds_warned);
        result += -1;
    }
    result += test_json(NULL);

    set_expected_results("fs_watcher dir fds of the process: stop", NULL, NULL, NULL, 1);
    for(int w = 0; w < 2; w++) {
        fs_stop_watcher_event(fs_events[w]);
    }
    for(int i = 0; i < 10; i++) {
        yev_loop_run_once(yev_loop);
    }
    result += test_json(NULL);
    setrlimit(RLIMIT_NOFILE, &rl_saved);
    rmrdir(root6);
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
    if(fs_start_watcher_event(fs_event) < 0) {
        printf("%sERROR%s --> the watcher could not be started\n", On_Red BWhite, Color_Off);
        fs_stop_watcher_event(fs_event);
        return -1;
    }
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
    while(quiet < 50) {  // by time: a slice per turn
        if(time_in_milliseconds_monotonic() - t0 >= 5*60*1000) {
            printf("%sERROR%s --> the pass did not end in 5 minutes (~30 s here)\n", On_Red BWhite, Color_Off);
            result += -1;
            break;
        }
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
    result += do_test_padded_end();
    result += do_test_padded_end_not_short();
    result += do_test_fionread_broken();
    result += do_test_unwatched_retry();
    result += do_test_root_unwatchable();
    result += do_test_dir_fds();
    result += do_test_dir_fds_limit();
    result += do_test_dir_fds_limit_process();
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
