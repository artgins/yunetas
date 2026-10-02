/****************************************************************************
 *              fs_watcher.c
 *
 *              Monitoring of directories and files with io_uring
 *
 *              We only monitor:
 *                  - create/delete of sub-directories (recursive optionally)
 *                  - create/delete of files in these directories,
 *              and optionally modification of files.
 *
 *              Copyright (c) 2024-2026, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/inotify.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>

#include <testing.h>
#include <gobj.h>
#include "fs_watcher.h"

/***************************************************************************
 *  Constants
 ***************************************************************************/

#define DEFAULT_MASK (IN_DELETE_SELF|IN_MOVE_SELF|IN_CREATE|IN_DELETE | IN_DONT_FOLLOW|IN_EXCL_UNLINK)
#define RESCAN_SLICE_MS 20      // the pass after an overflow gives the loop back after this
/*
 *  One read takes up to this much of the queue (at least one event of the
 *  longest name). It was one such event: a backlog of 65536 events was
 *  read in ~8000 reads, a batch each, and an owner that asks where the
 *  queue ends at the end of a batch (FIONREAD walks the whole queue) paid
 *  it 8000 times -- a flood of new keys of a timeranger2 follower took
 *  twice as long.
 */
#define READ_SIZE       (32 * (sizeof(struct inotify_event) + NAME_MAX + 1))

/***************************************************************************
 *  Prototypes
 ***************************************************************************/
PRIVATE void fs_destroy_watcher_event(
    fs_event_t *fs_event
);
PRIVATE int yev_callback(
    yev_event_h yev_event
);
PRIVATE void handle_inotify_event(fs_event_t *fs_event, struct inotify_event *event);
PRIVATE int add_watch(fs_event_t *fs_event, const char *path, BOOL may_vanish);
PRIVATE int remove_watch(fs_event_t *fs_event, const char *path, int wd);
PRIVATE void note_stale_wd(fs_event_t *fs_event, int wd);
PRIVATE void drop_tracked(fs_event_t *fs_event, int wd);
PRIVATE void close_dir_fd_of_gone(fs_event_t *fs_event, const char *path);

PRIVATE BOOL proc_fd_watch_failed_said = FALSE;   // once per process
PRIVATE void note_gone_directories(fs_event_t *fs_event);
PRIVATE void forget_stale_wds(fs_event_t *fs_event);
PRIVATE const char *get_path(fs_event_t *fs_event, int wd);
PRIVATE int add_watch_recursive(fs_event_t *fs_event, const char *path);
PRIVATE void start_rescan_pass(fs_event_t *fs_event);
PRIVATE void stop_rescan_pass(fs_event_t *fs_event);
PRIVATE int rescan_slice_callback(yev_event_h yev_event);
PRIVATE uint64_t monotonic_us(void);
PRIVATE uint32_t fs_type_2_inotify_mask(fs_event_t *fs_event);
PRIVATE int queued_in_kernel(fs_event_t *fs_event, uint64_t *queued);
PRIVATE uint64_t note_padded_end(fs_event_t *fs_event, uint64_t end);
PRIVATE uint64_t kernel_queue_bound(fs_event_t *fs_event);
PRIVATE void arm_pad_check(fs_event_t *fs_event);
PRIVATE int pad_check_callback(yev_event_h yev_event);
PRIVATE void tell_owner_watcher_gone(fs_event_t *fs_event);
PRIVATE void close_tracked_dir_fd(int dfd);
PRIVATE void watch_unwatched_again(fs_event_t *fs_event);

/***************************************************************************
 *  Data
 ***************************************************************************/
PRIVATE size_t dir_fds_held = 0;        // FS_FLAG_DIR_FDS: by every watcher of the process
PRIVATE BOOL dir_fds_half_said = FALSE; // see warn_dir_fds_near_the_limit()

typedef struct {
    uint32_t bit;
    char *name;
    char *description;
} bits_table_t;

PRIVATE bits_table_t bits_table[] = {
{IN_ACCESS,         "IN_ACCESS",        "File was accessed"},
{IN_MODIFY,         "IN_MODIFY",        "File was modified"},
{IN_ATTRIB,         "IN_ATTRIB",        "Metadata changed"},
{IN_CLOSE_WRITE,    "IN_CLOSE_WRITE",   "Writable file was closed"},
{IN_CLOSE_NOWRITE,  "IN_CLOSE_NOWRITE", "Unwritable file closed"},
{IN_OPEN,           "IN_OPEN",          "File was opened"},
{IN_MOVED_FROM,     "IN_MOVED_FROM",    "File was moved from X"},
{IN_MOVED_TO,       "IN_MOVED_TO",      "File was moved to Y"},
{IN_CREATE,         "IN_CREATE",        "Subfile was created"},
{IN_DELETE,         "IN_DELETE",        "Subfile was deleted"},
{IN_DELETE_SELF,    "IN_DELETE_SELF",   "Self was deleted"},
{IN_MOVE_SELF,      "IN_MOVE_SELF",     "Self was moved"},

//" Events sent by the kernel"
{IN_UNMOUNT,        "IN_UNMOUNT",       "Backing fs was unmounted"},
{IN_Q_OVERFLOW,     "IN_Q_OVERFLOW",    "Event queued overflowed"},
{IN_IGNORED,        "IN_IGNORED",       "File was ignored"},

// " Special flags"
{IN_ONLYDIR,        "IN_ONLYDIR",       "Only watch the path if it is a directory"},
{IN_DONT_FOLLOW,    "IN_DONT_FOLLOW",   "Do not follow a sym link"},
{IN_EXCL_UNLINK,    "IN_EXCL_UNLINK",   "Exclude events on unlinked objects"},
{IN_MASK_CREATE,    "IN_MASK_CREATE",   "Only create watches"},
{IN_MASK_ADD,       "IN_MASK_ADD",      "Add to the mask of an already existing watch"},
{IN_ISDIR,          "IN_ISDIR",         "Event occurred against dir"},
{IN_ONESHOT,        "IN_ONESHOT",       "Only send event once"},
{0,0,0}
};

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC fs_event_t *fs_create_watcher_event(
    yev_loop_h yev_loop,
    const char *path,
    fs_flag_t fs_flag,
    fs_callback_t callback,
    hgobj gobj,
    void *user_data,
    void *user_data2
)
{
    if(!is_directory(path)) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "No is a directory",
            "path",         "%s", path?path:"",
            NULL
        );
        return NULL;

    }
    int fd = inotify_init1(IN_NONBLOCK|IN_CLOEXEC);
    if(fd < 0) {
        const char *serr = "";
        int err = errno;
        if(err == EMFILE) {
            serr = "The user limit on the total number of INOTIFY INSTANCES has been reached";
        } else if(err == ENFILE) {
            serr = "The system limit on the total number of FILE DESCRIPTORS has been reached";
        }
        gobj_log_critical(yev_get_yuno(yev_loop)?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "inotify_init1() FAILED",
            "msg2",         "%s", serr,
            "errno",        "%d", err,
            "serror",       "%s", strerror(errno),
            NULL
        );
        return NULL;
    }

    fs_event_t *fs_event = GBMEM_MALLOC(sizeof(fs_event_t));
    if(!fs_event) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "No memory for yev event",    // use the same string
            NULL
        );
        close(fd);
        return NULL;
    }

    fs_event->yev_loop = yev_loop;
    fs_event->path = gbmem_strdup(path);
    fs_event->fs_flag = fs_flag;
    fs_event->fs_type = 0;
    fs_event->directory = NULL;
    fs_event->filename = NULL;
    fs_event->gobj = gobj;
    fs_event->user_data = user_data;
    fs_event->user_data2 = user_data2;
    fs_event->callback = callback;
    fs_event->fd = fd;
    fs_event->jn_tracked_paths = json_object();
    fs_event->in_callback = FALSE;
    fs_event->stop_requested = FALSE;
    fs_event->rescan_seen = NULL;
    fs_event->stale_wds = NULL;
    fs_event->stale_mark = 0;
    fs_event->jn_tracked_fds = json_object();
    fs_event->jn_paths_wd = json_object();
    fs_event->jn_unwatched = json_object();
    fs_event->event_wd = -1;
    fs_event->subdir_wd = -1;

    uint32_t trace_level = gobj_global_trace_level();

    if(trace_level & (TRACE_URING|TRACE_CREATE_DELETE|TRACE_CREATE_DELETE2|TRACE_FS)) {
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "fs_create_watcher_event",
            "msg2",         "%s", "💾🟦 fs_create_watcher_event",
            "path",         "%s", path,
            "fd",           "%d", fd,
            "p",            "%p", fs_event,
            NULL
        );
    }

    /*
     *  Alloc buffer to read
     */
    size_t len = READ_SIZE;
    gbuffer_t *gbuf = gbuffer_create(len, len);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "No memory for fs_event gbuf",
            NULL
        );
        fs_destroy_watcher_event(fs_event);
        return NULL;
    }

    fs_event->yev_event = yev_create_read_event(
        yev_loop,
        yev_callback,
        gobj,
        fd,
        gbuf
    );
    if(!fs_event->yev_event) {
        // yev_create_read_event only fails before it takes ownership of gbuf,
        // so the buffer is still ours to free here.
        GBUFFER_DECREF(gbuf)
        fs_destroy_watcher_event(fs_event);
        return NULL;
    }
    yev_set_user_data(fs_event->yev_event, fs_event);

    /*
     *  A root that cannot be watched (ENOSPC at fs.inotify.max_user_watches,
     *  say) is no watcher: up to 7.25.20 it was handed over all the same,
     *  running and watching nothing, its owner deaf without knowing.
     */
    int wd_root;
    if(fs_flag & FS_FLAG_RECURSIVE_PATHS) {
        wd_root = add_watch_recursive(fs_event, path);
    } else {
        wd_root = add_watch(fs_event, path, FALSE);
    }
    if(wd_root < 0) {
        // Error already logged
        fs_destroy_watcher_event(fs_event);
        return NULL;
    }

    return fs_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int fs_start_watcher_event(
    fs_event_t *fs_event
)
{
    if(!fs_event) {
        return -1;
    }
    return yev_start_event(fs_event->yev_event);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int fs_stop_watcher_event(
    fs_event_t *fs_event
)
{
    if(!fs_event) {
        return -1;
    }
    fs_event->stopping = TRUE;
    stop_rescan_pass(fs_event);
    if(fs_event->yev_pad && yev_event_is_running(fs_event->yev_pad)) {
        yev_stop_event(fs_event->yev_pad);
    }
    if(fs_event->in_callback) {
        /*
         *  Stopped by a consumer reacting to one of our own events (a feed
         *  closed from its key_deleted callback). yev_callback is still
         *  walking the batch with this fs_event: it stops there and destroys
         *  it once the walk is over.
         */
        fs_event->stop_requested = TRUE;
        return 0;
    }
    if(yev_event_is_running(fs_event->yev_event)) {
        return yev_stop_event(fs_event->yev_event);
    } else {
        fs_destroy_watcher_event(fs_event);
    }
    return 0;
}

/***************************************************************************
 *  The kernel says how many bytes of events it holds for the fd (FIONREAD).
 *  Before them come the rest of the batch being walked, if one is, or else
 *  a read the kernel has completed and the loop not delivered: its bytes
 *  left the kernel's queue and are in the buffer, the completion in the
 *  ring. Between two batches a read completes at any return to user space
 *  (an interrupt's too), so the ring is looked at before and after asking
 *  the kernel: the same answer both times, and nothing moved in between --
 *  there is one read at a time, and once completed it waits for the loop.
 ***************************************************************************/
PRIVATE int queued_in_kernel(fs_event_t *fs_event, uint64_t *queued)
{
    int n = 0;
    if(ioctl(fs_event->fd, FIONREAD, &n) < 0) {
        gobj_log_error(fs_event->gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "ioctl(FIONREAD) FAILED: the events queued by now are not counted",
            "path",         "%s", fs_event->path,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    *queued = (uint64_t)(n > 0? n : 0);
    return 0;
}

PUBLIC uint64_t fs_queued_events_end(
    fs_event_t *fs_event
)
{
    if(!fs_event) {
        return 0;
    }
    /*
     *  The kernel's queue not counted (FIONREAD failed), the end is said
     *  past all it can hold: an end said short is reached before the
     *  events queued now, and an owner does too soon what it left for
     *  after them (up to 7.25.21 a read whole was all that was added).
     *  The pad check closes it when nothing is left, or ends the watcher
     *  if the kernel cannot be asked again.
     */
    uint64_t queued = 0;
    if(fs_event->in_batch) {
        if(queued_in_kernel(fs_event, &queued) < 0) {
            return note_padded_end( // Error already logged
                fs_event, fs_event->batch_end + kernel_queue_bound(fs_event)
            );
        }
        return fs_event->batch_end + queued;
    }

    for(int tries = 0; tries < 3; tries++) {
        int res1 = 0, res2 = 0;
        int waiting1 = yev_get_waiting_completion(fs_event->yev_event, &res1);
        if(queued_in_kernel(fs_event, &queued) < 0) {
            return note_padded_end( // Error already logged
                fs_event, fs_event->offset + READ_SIZE + kernel_queue_bound(fs_event)
            );
        }
        int waiting2 = yev_get_waiting_completion(fs_event->yev_event, &res2);
        if(waiting1 < 0 || waiting2 < 0) {
            /*
             *  Completions overflowed the ring: whether one of this read
             *  waits cannot be seen. A read holds READ_SIZE at most.
             */
            return note_padded_end(fs_event, fs_event->offset + READ_SIZE + queued);
        }
        if(waiting1 == waiting2 && res1 == res2) {
            return fs_event->offset + ((waiting2 && res2 > 0)? (uint64_t)res2 : 0) + queued;
        }
    }

    gobj_log_error(fs_event->gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "the completions of the watcher's read kept moving: counted a read whole",
        "path",         "%s", fs_event->path,
        NULL
    );
    return note_padded_end(fs_event, fs_event->offset + READ_SIZE + queued);
}

/***************************************************************************
 *  An end said past the stream (a read counted whole, unseen) is noted, and
 *  closed by the watcher (pad_check_callback()): an owner waiting for the
 *  stream to reach it would otherwise wait for READ_SIZE bytes of unrelated
 *  events, for ever on a quiet watcher.
 ***************************************************************************/
PRIVATE uint64_t note_padded_end(fs_event_t *fs_event, uint64_t end)
{
    if(end > fs_event->pad_end) {
        fs_event->pad_end = end;
    }
    arm_pad_check(fs_event);
    return end;
}

/***************************************************************************
 *  The most the kernel can hold queued for an inotify fd: max_queued_events
 *  events of the largest size (an overflow event past them)
 ***************************************************************************/
PRIVATE uint64_t kernel_queue_bound(fs_event_t *fs_event)
{
    uint64_t max_events = 16384;    // the kernel's default
    FILE *file = fopen("/proc/sys/fs/inotify/max_queued_events", "r");
    if(file) {
        unsigned long long n = 0;
        if(fscanf(file, "%llu", &n) == 1 && n > 0) {
            max_events = (uint64_t)n;
        }
        fclose(file);
    } else {
        gobj_log_warning(fs_event->gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Cannot read max_queued_events of inotify: the default 16384 taken",
            "path",         "%s", fs_event->path,
            "serrno",       "%s", strerror(errno),
            NULL
        );
    }
    return (max_events + 1) * (sizeof(struct inotify_event) + NAME_MAX + 1);
}

PRIVATE void arm_pad_check(fs_event_t *fs_event)
{
    if(fs_event->stopping) {
        return;
    }
    if(!fs_event->yev_pad) {
        fs_event->yev_pad = yev_create_timer_event(
            fs_event->yev_loop,
            pad_check_callback,
            fs_event->gobj
        );
        if(!fs_event->yev_pad) {
            gobj_log_error(fs_event->gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Cannot create the timer that closes an end said past the stream: it is reached by the next events",
                "path",         "%s", fs_event->path,
                NULL
            );
            return;
        }
        yev_set_user_data(fs_event->yev_pad, fs_event);
    }
    if(!yev_event_is_running(fs_event->yev_pad)) {
        yev_start_timer_event(fs_event->yev_pad, 1, FALSE);   // the next turn of the loop
    }
}

/***************************************************************************
 *  The next turn of the loop after an end was said past the stream, or
 *  after a batch while one is open. When nothing is left to deliver -- no
 *  completion of the read waiting in the ring, none in the kernel's queue,
 *  the same answer before and after asking it -- everything queued when the
 *  end was said has been handed over: the stream jumps to that end, and the
 *  owner is told with an FS_BATCH_END. A read still to come closes it at its
 *  batch's end; a ring still overflowed is looked at again next turn.
 ***************************************************************************/
PRIVATE int pad_check_callback(yev_event_h yev_event)
{
    fs_event_t *fs_event = yev_get_user_data(yev_event);
    if(!fs_event || yev_get_state(yev_event) != YEV_ST_IDLE || fs_event->stopping) {
        return 0;   // the stop of the timer, or of the watcher
    }
    if(fs_event->pad_end <= fs_event->offset) {
        fs_event->pad_end = 0;
        return 0;   // reached by the events themselves
    }

    int res1 = 0, res2 = 0;
    uint64_t queued = 0;
    int waiting1 = yev_get_waiting_completion(fs_event->yev_event, &res1);
    if(queued_in_kernel(fs_event, &queued) < 0) {
        /*
         *  Error already logged. The end cannot be closed without the
         *  kernel's count, and on a quiet watcher nothing else closes it:
         *  the watcher is over, and its owner is told. Up to 7.25.21 the
         *  end waited for the next batch, for ever on a quiet watcher.
         */
        gobj_log_error(fs_event->gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "The events queued by the kernel cannot be counted: an end said past the stream cannot be closed, the watcher is gone",
            "path",         "%s", fs_event->path,
            NULL
        );
        tell_owner_watcher_gone(fs_event);
        fs_stop_watcher_event(fs_event);
        return 0;
    }
    int waiting2 = yev_get_waiting_completion(fs_event->yev_event, &res2);
    if(waiting1 < 0 || waiting2 < 0) {
        yev_start_timer_event(yev_event, 1, FALSE);     // the loop flushes the ring
        return 0;
    }
    if(waiting1 || waiting2 || queued) {
        return 0;   // a read comes: its batch's end looks again
    }

    fs_event->offset = fs_event->pad_end;
    fs_event->pad_end = 0;
    if(fs_event->fs_flag & FS_FLAG_BATCH_END) {
        fs_event->fs_type = FS_BATCH_END_TYPE;
        fs_event->event_wd = -1;
        fs_event->subdir_wd = -1;
        fs_event->directory = (volatile char *)fs_event->path;
        fs_event->filename = "";
        fs_event->offset_end = fs_event->offset;
        fs_event->in_callback = TRUE;
        fs_event->callback(fs_event);
        fs_event->in_callback = FALSE;
        if(fs_event->stop_requested) {
            /*
             *  The owner stopped the watcher from its callback: stop it now
             *  that nobody is walking with it
             */
            fs_event->stop_requested = FALSE;
            fs_stop_watcher_event(fs_event);
        }
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void fs_destroy_watcher_event(
    fs_event_t *fs_event
)
{
    if(!fs_event) {
        return;
    }
    uint32_t trace_level = gobj_global_trace_level();

    if(trace_level & (TRACE_URING|TRACE_CREATE_DELETE|TRACE_CREATE_DELETE2|TRACE_FS)) {
        gobj_log_debug(fs_event->gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "fs_destroy_watcher_event",
            "msg2",         "%s", "💾🟥🟥 fs_destroy_watcher_event",
            "path",         "%s", fs_event->path,
            "fd",           "%d", fs_event->fd,
            "p",            "%p", fs_event,
            NULL
        );
    }

    if(fs_event->fd != -1) {
        if(trace_level & (TRACE_URING|TRACE_CREATE_DELETE|TRACE_CREATE_DELETE2|TRACE_FS)) {
            gobj_log_debug(fs_event->gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_YEV_LOOP,
                "msg",          "%s", "close socket",
                "msg2",         "%s", "💾🟥 close socket",
                "fd",           "%d", fs_event->fd ,
                "p",            "%p", fs_event,
                NULL
            );
        }
        close(fs_event->fd);
        fs_event->fd = -1;
    }
    yev_set_fd(fs_event->yev_event, -1);
    EXEC_AND_RESET(yev_destroy_event, fs_event->yev_event)
    stop_rescan_pass(fs_event);
    EXEC_AND_RESET(yev_destroy_event, fs_event->yev_rescan) // no callback after this, see yev_loop
    if(fs_event->yev_pad && yev_event_is_running(fs_event->yev_pad)) {
        yev_stop_event(fs_event->yev_pad);
    }
    EXEC_AND_RESET(yev_destroy_event, fs_event->yev_pad)
    GBMEM_FREE(fs_event->path)
    JSON_DECREF(fs_event->jn_tracked_paths)
    JSON_DECREF(fs_event->stale_wds)
    {
        const char *s_wd; json_t *jn_fd;
        json_object_foreach(fs_event->jn_tracked_fds, s_wd, jn_fd) {
            int dfd = (int)json_integer_value(jn_fd);
            if(dfd >= 0) {
                close_tracked_dir_fd(dfd);
            }
        }
    }
    JSON_DECREF(fs_event->jn_tracked_fds)
    JSON_DECREF(fs_event->jn_paths_wd)
    JSON_DECREF(fs_event->jn_unwatched)
    GBMEM_FREE(fs_event)
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE uint32_t fs_type_2_inotify_mask(fs_event_t *fs_event)
{
    uint32_t inotify_mask = DEFAULT_MASK;
    if(fs_event->fs_flag & FS_FLAG_MODIFIED_FILES) {
        inotify_mask |= IN_MODIFY;
    }
    if(fs_event->fs_flag & FS_FLAG_MOVED_AS_DELETED) {
        inotify_mask |= IN_MOVED_FROM;
    }

    return inotify_mask;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_callback(
    yev_event_h yev_event
)
{
    fs_event_t *fs_event = yev_get_user_data(yev_event);
    hgobj gobj = fs_event->gobj;

    uint32_t trace_level = gobj_global_trace_level();

    if(trace_level & (TRACE_URING|TRACE_FS)) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_get_flag(yev_event));
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev callback",
            "msg2",         "%s", "💾💥 yev callback",
            "event type",   "%s", yev_event_type_name(yev_event),
            "state",        "%s", yev_get_state_name(yev_event),
            "result",       "%d", yev_get_result(yev_event),
            "sres",         "%s", (yev_get_result(yev_event)<0)? strerror(-yev_get_result(yev_event)):"",
            "flag",         "%j", jn_flags,
            "fd",           "%d", yev_get_fd(yev_event),
            "gbuffer",      "%p", yev_get_gbuf(yev_event),
            "p",            "%p", yev_event,
            NULL
        );
        json_decref(jn_flags);
    }

    switch(yev_get_type(yev_event)) {
        case YEV_READ_TYPE:
            {
                if(yev_get_result(yev_event) < 0) {
                    /*
                     *  The read is over. After a stop of its owner that is
                     *  the stop itself, whatever the result says (a cancel,
                     *  or a cancel that found the read done). Otherwise it
                     *  FAILED, or somebody else canceled it -- no shutdown
                     *  does (yev_loop_stop() comes after every owner stopped
                     *  its watcher, and a loop run after it delivers nothing
                     *  behind its completion), so that is an order broken.
                     *  Either way the watcher is over, and its owner is told
                     *  before it goes. Up to
                     *  7.25.20 it went silently, and the owner kept a pointer
                     *  to freed memory (a timeranger2 feed stopped it again
                     *  when closed).
                     */
                    if(!fs_event->stopping) {
                        BOOL canceled = (yev_get_result(yev_event) == -ECANCELED)? TRUE: FALSE;
                        gobj_log_error(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", canceled? MSGSET_INTERNAL : MSGSET_SYSTEM,
                            "msg",          "%s", canceled?
                                "inotify read canceled, and not by its owner: the watcher is gone" :
                                "inotify read FAILED: the watcher is gone",
                            "path",         "%s", fs_event->path,
                            "errno",        "%d", -yev_get_result(yev_event),
                            "serrno",       "%s", strerror(-yev_get_result(yev_event)),
                            NULL
                        );
                        tell_owner_watcher_gone(fs_event);
                    }
                    fs_destroy_watcher_event(fs_event);

                } else {
                    gbuffer_t *gbuf = yev_get_gbuf(yev_event);
                    size_t len = gbuffer_leftbytes(gbuf);
                    char *buffer = gbuffer_cur_rd_pointer(gbuf);
                    char *ptr = buffer;
                    uint64_t batch_start = fs_event->offset;
                    fs_event->batch_end = batch_start + len;
                    fs_event->in_batch = TRUE;
                    fs_event->in_callback = TRUE;
                    while (ptr < buffer + len && !fs_event->stop_requested) {
                        /*
                         *  Bound the parse: the fixed header must fit, and so
                         *  must the variable-length name, before we dereference
                         *  event->len / event->name. A truncated tail (short
                         *  read) would otherwise over-read past the buffer.
                         */
                        if(ptr + sizeof(struct inotify_event) > buffer + len) {
                            break;
                        }
                        struct inotify_event *event = (struct inotify_event *) ptr;
                        if(ptr + sizeof(struct inotify_event) + event->len > buffer + len) {
                            break;
                        }

                        // Handle the file modification event
                        fs_event->offset = batch_start + (uint64_t)(ptr - buffer);
                        fs_event->offset_end = fs_event->offset +
                            sizeof(struct inotify_event) + event->len;
                        handle_inotify_event(fs_event, event);

                        ptr += sizeof(struct inotify_event) + event->len;
                    }
                    if(!fs_event->stop_requested && json_object_size(fs_event->jn_unwatched) > 0) {
                        fs_event->offset = fs_event->batch_end;
                        fs_event->offset_end = fs_event->batch_end;
                        watch_unwatched_again(fs_event);
                    }
                    if(!fs_event->stop_requested && (fs_event->fs_flag & FS_FLAG_BATCH_END)) {
                        fs_event->fs_type = FS_BATCH_END_TYPE;
                        fs_event->event_wd = -1;
                        fs_event->subdir_wd = -1;
                        fs_event->directory = (volatile char *)fs_event->path;
                        fs_event->filename = "";
                        fs_event->offset = fs_event->batch_end;
                        fs_event->offset_end = fs_event->batch_end;
                        fs_event->callback(fs_event);
                    }
                    fs_event->in_callback = FALSE;
                    fs_event->in_batch = FALSE;
                    fs_event->offset = fs_event->batch_end;
                    if(!fs_event->stop_requested) {
                        forget_stale_wds(fs_event);
                        if(fs_event->pad_end > fs_event->offset) {
                            arm_pad_check(fs_event);
                        }
                    }

                    if(fs_event->stop_requested) {
                        /*
                         *  A consumer stopped us from inside a callback: the
                         *  rest of the batch is for a watcher nobody wants.
                         */
                        fs_destroy_watcher_event(fs_event);
                        break;
                    }

                    /*
                     *  Clear buffer
                     *  Re-arm read. A read that cannot be armed again is the
                     *  end of the watcher, as one that fails: up to 7.25.20
                     *  it was left dead, its owner never told.
                     */
                    gbuf = yev_get_gbuf(yev_event);
                    if(gbuf) {
                        gbuffer_clear(gbuf);
                    }
                    if(!gbuf || yev_start_event(yev_event) < 0) {
                        gobj_log_error(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_INTERNAL,
                            "msg",          "%s", "inotify read cannot be armed again: the watcher is gone",
                            "path",         "%s", fs_event->path,
                            "gbuffer",      "%p", gbuf,
                            NULL
                        );
                        if(!fs_event->stopping) {
                            tell_owner_watcher_gone(fs_event);
                        }
                        fs_destroy_watcher_event(fs_event);
                    }
                }
            }
            break;

        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "event type NOT IMPLEMENTED",
                NULL
            );
            break;
    }

    return 0;
}

/***************************************************************************
 *  The watcher is over: its owner is told once, and must drop it (it is
 *  destroyed when this returns)
 ***************************************************************************/
PRIVATE void tell_owner_watcher_gone(fs_event_t *fs_event)
{
    fs_event->fs_type = FS_WATCHER_GONE_TYPE;
    fs_event->event_wd = -1;
    fs_event->subdir_wd = -1;
    fs_event->directory = (volatile char *)fs_event->path;
    fs_event->filename = "";
    fs_event->in_callback = TRUE;
    fs_event->callback(fs_event);
    fs_event->in_callback = FALSE;
}

/***************************************************************************
 *  The end of a batch: the subdirectories whose watch could not be made
 *  (ENOSPC, ENOMEM; add_watch()) are tried again, some per batch. One
 *  watched now is handed as created (FS_SUBDIR_CREATED_TYPE, `subdir_wd`
 *  its watch, at the batch's end): its owner reads what was made in it
 *  while it was not watched. One gone is forgotten (its parent said it).
 *  Out of watches still, the rest wait for the next batch. Not on a timer:
 *  a watcher whose only activity is in a directory it cannot watch hears
 *  nothing to retry on, which the ERROR of the first failure says.
 ***************************************************************************/
#define UNWATCHED_RETRIES_PER_BATCH 64

PRIVATE void watch_unwatched_again(fs_event_t *fs_event)
{
    json_t *paths = json_array();
    const char *path_; json_t *v;
    json_object_foreach(fs_event->jn_unwatched, path_, v) {
        if(json_array_size(paths) >= UNWATCHED_RETRIES_PER_BATCH) {
            break;
        }
        json_array_append_new(paths, json_string(path_));
    }

    int idx; json_t *jn_path;
    json_array_foreach(paths, idx, jn_path) {
        if(fs_event->stop_requested) {
            break;  // its owner stopped it from a callback
        }
        const char *path = json_string_value(jn_path);
        if(!is_directory(path)) {
            json_object_del(fs_event->jn_unwatched, path);  // gone: its parent said it
            continue;
        }
        int wd = add_watch(fs_event, path, TRUE);
        if(wd < 0) {
            if(errno == ENOSPC || errno == ENOMEM) {
                break;  // still out of watches: the next batch
            }
            json_object_del(fs_event->jn_unwatched, path);  // Error already logged
            continue;
        }

        char parent[PATH_MAX];
        snprintf(parent, sizeof(parent), "%s", path);
        char *slash = strrchr(parent, '/');
        if(!slash) {
            continue;   // a subdirectory always has a parent
        }
        *slash = 0;
        int parent_wd = -1;
        const char *s_wd; json_t *jn_p;
        json_object_foreach(fs_event->jn_tracked_paths, s_wd, jn_p) {
            if(strcmp(json_string_value(jn_p)? json_string_value(jn_p) : "", parent) == 0) {
                parent_wd = atoi(s_wd);
                break;
            }
        }
        fs_event->fs_type = FS_SUBDIR_CREATED_TYPE;
        fs_event->event_wd = parent_wd;
        fs_event->subdir_wd = wd;
        fs_event->directory = (volatile char *)parent;
        fs_event->filename = slash + 1;
        fs_event->callback(fs_event);
        fs_event->subdir_wd = -1;
    }
    JSON_DECREF(paths)
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void handle_inotify_event(fs_event_t *fs_event, struct inotify_event *event)
{
    hgobj gobj = fs_event->gobj;
    const char *path;
    char full_path[PATH_MAX];

    fs_event->event_wd = event->wd;
    fs_event->subdir_wd = -1;

    uint32_t trace_level = gobj_global_trace_level();

    if(trace_level & TRACE_FS) {
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "inotify_event",
            "msg2",         "%s", "💾💾💥 inotify_event",
            "event->wd",    "%d", event->wd,
            "event->name",  "%s", event->len? event->name:"",
            "p",            "%p", fs_event,
            NULL
        );

        for(int i=0; i< sizeof(bits_table)/sizeof(bits_table[0]); i++) {
            bits_table_t entry = bits_table[i];
            if(entry.bit & event->mask) {
                gobj_trace_msg(gobj, "  - %s (%s)", entry.name, entry.description);
            }
        }
    }

    /*
     *  being:
     *      "tr_topic_pkey_integer/topic_pkey_integer/keys": 7,
     *      "tr_topic_pkey_integer/topic_pkey_integer/keys/0000000000000000002": 8,
     *
     *  TRICK when a watched directory .../keys/0000000000000000002 is deleted, the events are:
     *
     *      - IN_DELETE_SELF 8  ""  with the wd you can know what directory is. !!!HACK use this!!!
     *      - IN_IGNORED     8  ""  the wd of deleted directory has been removed of watchers
     *
     *      - IN_DELETE      7  "0000000000000000002"
     *                          comes with the wd of the directory parent (keys),
     *                          informing that his child has been deleted (0000000000000000002).
     *                          but in a tree, in the final first subdirectories deleting
     *                          this event is not arriving.
     *
     *      HACK don't use IN_MODIFY in intense writing, cause IN_Q_OVERFLOW and lost events.
     */

    if(event->mask & (IN_Q_OVERFLOW)) {
        /*
         *  The kernel dropped an unknown set of events (event->wd == -1).
         *  What they said is still on the filesystem: the watches are set
         *  again on every directory the tree has now, and the owner is told
         *  to rebuild its view from it. Up to 7.25.8 this aborted the yuno
         *  to reload clean: under a sustained burst the reload met the next
         *  overflow, and a timeranger2 reader lived in a crash loop that did
         *  less work than the lost events would have cost.
         */
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "inotify IN_Q_OVERFLOW: events lost, rescanning the watched tree",
            "path",         "%s", fs_event->path,
            NULL
        );

        fs_event->fs_type = FS_OVERFLOW_TYPE;
        fs_event->event_wd = -1;
        fs_event->directory = (volatile char *)fs_event->path;
        fs_event->filename = "";
        fs_event->callback(fs_event);

        if(fs_event->stop_requested) {
            return; // the owner stopped us: the watcher goes when the batch ends
        }
        if(fs_event->rescan_dirs) {
            /*
             *  A pass is running: the directories it visited already may
             *  have lost events too. Another whole pass after this one --
             *  starting again now would starve the end of the tree under
             *  overflows that keep coming.
             */
            fs_event->rescan_again = TRUE;
        } else {
            start_rescan_pass(fs_event);
        }
        return;
    }

    if(event->mask & (IN_DELETE_SELF)) {
        // The directory is removed or moved
        path=get_path(fs_event, event->wd);
        if(path != NULL) {
            /*
             *  In a recursive watch a SUBDIRECTORY's deletion is reported by
             *  its parent too (IN_DELETE|IN_ISDIR), which comes after this
             *  one -- and comes even when the directory went before its
             *  watch was set. So only the ROOT is reported from here: every
             *  subdirectory used to reach the consumer twice.
             */
            BOOL is_root = (strcmp(path, fs_event->path)==0)? TRUE: FALSE;
            if(is_root || !(fs_event->fs_flag & FS_FLAG_RECURSIVE_PATHS)) {
                char path_[PATH_MAX];
                snprintf(path_, sizeof(path_), "%s", path);
                char *filename = pop_last_segment(path_);

                fs_event->fs_type = FS_SUBDIR_DELETED_TYPE;
                fs_event->directory = path_;
                fs_event->filename = filename;

                fs_event->callback(fs_event);
            }
            remove_watch(fs_event, path, event->wd);
        }
        return;
    }

    if(event->mask & (IN_IGNORED)) {
        /*
         *  The kernel removed the watch. Mostly after an IN_DELETE_SELF that
         *  already took the wd out of the table; when that event was lost
         *  (an overflow, a watch removed for another reason) this is the
         *  last word on the wd: its entry would name a directory nobody
         *  watches, and the pass after an overflow would take it as
         *  watched.
         */
        drop_tracked(fs_event, event->wd);
        return;
    }

    path = get_path(fs_event, event->wd);
    if(path == NULL) {
        // Error already logged by get_path (a concurrent wd-removal race):
        // the event is stale, there is nothing to resolve it against.
        return;
    }
    char *filename = event->len? event->name:"";

    if(event->mask & (IN_ISDIR)) {
        /*
         *  Directory
         */
        if (event->mask & (IN_CREATE)) {
            if(fs_event->fs_flag & FS_FLAG_RECURSIVE_PATHS) {
                snprintf(full_path, sizeof(full_path), "%s/%s", path, filename);
                /*
                 *  A directory created and removed at once (timeranger2's
                 *  key-delete signal) is gone when its IN_CREATE is read:
                 *  there is nothing to watch, and its parent reports the
                 *  removal.
                 */
                if(is_directory(full_path)) {
                    fs_event->subdir_wd = add_watch(fs_event, full_path, TRUE);
                }
            }
            fs_event->fs_type = FS_SUBDIR_CREATED_TYPE;
            fs_event->directory = (volatile char *)path;
            fs_event->filename = filename;

            fs_event->callback(fs_event);
        }

        if((event->mask & IN_DELETE) ||
                ((event->mask & IN_MOVED_FROM) && (fs_event->fs_flag & FS_FLAG_MOVED_AS_DELETED))) {
            if(path != NULL) {
                /*
                 *  Gone from this directory: removed, or (FS_FLAG_MOVED_AS_DELETED)
                 *  renamed away. The descriptor held on it delays its IN_DELETE_SELF and
                 *  IN_IGNORED to its close: closed now, they come, and the
                 *  watch goes as without one
                 */
                snprintf(full_path, sizeof(full_path), "%s/%s", path, filename);
                close_dir_fd_of_gone(fs_event, full_path);

                fs_event->fs_type = FS_SUBDIR_DELETED_TYPE;
                fs_event->directory = (volatile char *)path;
                fs_event->filename = filename;

                fs_event->callback(fs_event);
            }
        }

    } else {
        /*
         *  File
         */
        if (event->mask & (IN_CREATE)) {
            fs_event->fs_type = FS_FILE_CREATED_TYPE;
            fs_event->directory = (volatile char *)path;
            fs_event->filename = filename;

            fs_event->callback(fs_event);
        }

        if (event->mask & (IN_DELETE)) {
            fs_event->fs_type = FS_FILE_DELETED_TYPE;
            fs_event->directory = (volatile char *)path;
            fs_event->filename = filename;

            fs_event->callback(fs_event);
        }

        if (event->mask & (IN_MODIFY)) {
            // TODO check, libuv uses in UV_CHANGE -> (IN_ATTRIB|IN_MODIFY)
            fs_event->fs_type = FS_FILE_MODIFIED_TYPE;
            fs_event->directory = (volatile char *)path;
            fs_event->filename = filename;

            fs_event->callback(fs_event);
        }
    }
}

/***************************************************************************
 *  FS_FLAG_DIR_FDS holds a descriptor per subdirectory watched, for the
 *  life of the watch: a follower's feed, one per key directory. Said when
 *  the ones held by all the watchers of the process reach half of the soft
 *  open-files limit, before the limit is what says it (every read failing
 *  with EMFILE), and said again only after they fell under the half. The
 *  limit is the process's: up to 7.25.21 each watcher counted its own, and
 *  four followers of 400 directories each, under 1024, never said it.
 ***************************************************************************/
PRIVATE void close_tracked_dir_fd(int dfd)
{
    close(dfd);
    if(dir_fds_held > 0) {
        dir_fds_held--;
    }
}

PRIVATE void warn_dir_fds_near_the_limit(fs_event_t *fs_event)
{
    size_t n = dir_fds_held;
    struct rlimit rl;
    if(getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        gobj_log_error(fs_event->gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "getrlimit() FAILED",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return;
    }
    if(rl.rlim_cur == RLIM_INFINITY || n < rl.rlim_cur / 2) {
        dir_fds_half_said = FALSE;
        return;
    }
    if(dir_fds_half_said) {
        return;
    }
    dir_fds_half_said = TRUE;
    fs_event->dir_fds_warned = TRUE;
    gobj_log_warning(fs_event->gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_SYSTEM,
        "msg",          "%s", "Directories watched through descriptors: half of the open-files limit",
        "path",         "%s", fs_event->path,
        "dir_fds",      "%lu", (unsigned long)n,
        "dir_fds_of_this_watcher", "%lu", (unsigned long)json_object_size(fs_event->jn_tracked_fds),
        "soft_limit",   "%lu", (unsigned long)rl.rlim_cur,
        NULL
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int add_watch(
    fs_event_t *fs_event,
    const char *path,
    BOOL may_vanish     // a subdirectory met by the watch, not the root it was given
)
{
    hgobj gobj = fs_event->gobj;

    /*
     *  FS_FLAG_DIR_FDS: the subdirectory is opened FIRST, and watched through
     *  its descriptor (/proc/self/fd/N, followed: it is the inode itself), so
     *  the watch and the descriptor are one inode whatever happens to the
     *  path in between. The root is watched by its path, as always: nothing
     *  above it would close its descriptor when it goes.
     */
    int dir_fd = -1;
    char watched_path[PATH_MAX];
    snprintf(watched_path, sizeof(watched_path), "%s", path);
    uint32_t mask = fs_type_2_inotify_mask(fs_event);
    if((fs_event->fs_flag & FS_FLAG_DIR_FDS) && strcmp(path, fs_event->path) != 0) {
        dir_fd = open(path, O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if(dir_fd < 0) {
            if(errno != ENOENT || !may_vanish) {
                /*
                 *  Said at the transition, not per directory: out of
                 *  descriptors (EMFILE), every new key directory of a
                 *  follower fails the same way. The ones after it are
                 *  counted, and said when a descriptor opens again.
                 */
                if(fs_event->dir_fds_by_path == 0) {
                    gobj_log_error(fs_event->gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_SYSTEM,
                        "msg",          "%s", "Cannot open a directory to watch it through its descriptor: watched by its path (and the next ones that fail, counted)",
                        "path" ,        "%s", path,
                        "errno",        "%d", errno,
                        "serrno" ,      "%s", strerror(errno),
                        NULL
                    );
                }
                fs_event->dir_fds_by_path++;
            }
            // ENOENT: inotify_add_watch() below says it the usual way
        } else {
            if(fs_event->dir_fds_by_path > 0) {
                gobj_log_warning(fs_event->gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "Directories watched through their descriptor again",
                    "path" ,        "%s", fs_event->path,
                    "watched_by_path", "%ld", (long)fs_event->dir_fds_by_path,
                    NULL
                );
                fs_event->dir_fds_by_path = 0;
            }
            snprintf(watched_path, sizeof(watched_path), "/proc/self/fd/%d", dir_fd);
            mask &= ~(uint32_t)IN_DONT_FOLLOW;
        }
    }

    int wd = inotify_add_watch(fs_event->fd, watched_path, mask);
    if (wd == -1 && dir_fd >= 0) {
        int err = errno;
        close(dir_fd);
        dir_fd = -1;
        if(err == ENOSPC || err == ENOMEM) {
            /*
             *  No watch can be made (max_user_watches, memory), by its path
             *  neither: said below as what it is, not as /proc
             */
            errno = err;
        } else {
            /*
             *  Not through the descriptor (no /proc?): by its path, as
             *  without FS_FLAG_DIR_FDS, said once
             */
            if(!proc_fd_watch_failed_said) {
                proc_fd_watch_failed_said = TRUE;
                gobj_log_error(fs_event->gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "Cannot watch a directory through its descriptor (/proc/self/fd): watched by its path",
                    "path" ,        "%s", path,
                    "errno",        "%d", err,
                    "serrno" ,      "%s", strerror(err),
                    NULL
                );
            }
            wd = inotify_add_watch(fs_event->fd, path, fs_type_2_inotify_mask(fs_event));
        }
    }
    if (wd == -1) {
        if(errno == ENOENT && may_vanish) {
            /*
             *  Seen created, gone before it could be watched. A master
             *  signals a deleted key to its followers by creating and
             *  removing the key's directory (appear-and-vanish), so in a
             *  recursive watch this is that signal arriving late, not a
             *  fault: the parent's IN_DELETE follows. Only for what the
             *  watch meets on its way: a ROOT that does not exist is a
             *  misconfigured path, and stays an error.
             */
            gobj_log_warning(fs_event->gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Directory gone before it could be watched",
                "path" ,        "%s", path,
                NULL
            );
            return -1;
        }
        if((errno == ENOSPC || errno == ENOMEM) && may_vanish) {
            /*
             *  Out of watches (fs.inotify.max_user_watches) or memory: a
             *  subdirectory is not watched, and nothing made in it is heard.
             *  It is kept, and tried again at the end of each batch
             *  (watch_unwatched_again()); when its watch is made it is
             *  handed as created then, so its owner reads what was made in
             *  it meanwhile. Said at the transition, the ones after it
             *  counted. Up to 7.25.21 it was an ERROR per directory, and the
             *  directory was never watched (a timeranger2 follower took a
             *  key directory for gone, and lost its records).
             */
            int err = errno;
            if(json_object_size(fs_event->jn_unwatched) == 0) {
                gobj_log_error(fs_event->gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_SYSTEM,
                    "msg",          "%s", "Cannot watch a directory, out of inotify watches or memory: tried again at each batch (and the next ones that fail, counted)",
                    "path" ,        "%s", path,
                    "errno",        "%d", err,
                    "serrno" ,      "%s", strerror(err),
                    NULL
                );
            }
            json_object_set_new(fs_event->jn_unwatched, path, json_true());
            errno = err;
            return -1;
        }
        gobj_log_error(fs_event->gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "inotify_add_watch() FAILED",
            "path" ,        "%s", path,
            "serrno" ,      "%s", strerror(errno),
            NULL
        );
        return -1;
    }

    if(json_object_get(fs_event->jn_unwatched, path)) {
        json_object_del(fs_event->jn_unwatched, path);  // watched at last (here, or by a pass)
        if(json_object_size(fs_event->jn_unwatched) == 0) {
            gobj_log_warning(fs_event->gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "Directories watched again: every one that could not be is watched now",
                "path" ,        "%s", fs_event->path,
                NULL
            );
        }
    }

    char s_wd[64];
    snprintf(s_wd, sizeof(s_wd), "%d", wd);
    json_object_set_new(fs_event->jn_tracked_paths, s_wd, json_string(path));

    if(fs_event->fs_flag & FS_FLAG_DIR_FDS) {
        json_t *jn_old_fd = json_object_get(fs_event->jn_tracked_fds, s_wd);
        if(dir_fd >= 0 && jn_old_fd && json_integer_value(jn_old_fd) >= 0) {
            close(dir_fd);  // the same inode, already held
        } else if(dir_fd >= 0) {
            json_object_set_new(fs_event->jn_tracked_fds, s_wd, json_integer(dir_fd));
            dir_fds_held++;
            warn_dir_fds_near_the_limit(fs_event);
        }
        /*
         *  Another directory watched at this path before, and gone (its
         *  parent's IN_DELETE not read yet, or lost): its descriptor goes,
         *  so its IN_DELETE_SELF and IN_IGNORED come
         */
        json_t *jn_prev = json_object_get(fs_event->jn_paths_wd, path);
        if(jn_prev && json_integer_value(jn_prev) != wd) {
            close_dir_fd_of_gone(fs_event, path);
        }
        json_object_set_new(fs_event->jn_paths_wd, path, json_integer(wd));
    }

    uint32_t trace_level = gobj_global_trace_level();
    if(trace_level & TRACE_FS) {
        gobj_log_debug(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_YEV_LOOP,
            "msg",              "%s", "add watch",
            "msg2",             "%s", "💾🔷 add watch",
            "path",             "%s", path,
            "wd",               "%d", wd,
            "fs_flag",          "%d", fs_event->fs_flag,
            "recursive",        "%d", fs_event->fs_flag & FS_FLAG_RECURSIVE_PATHS,
            "p",                "%p", fs_event,
            NULL
        );
    }

    return wd;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int remove_watch(fs_event_t *fs_event, const char *path, int wd)
{
    hgobj gobj = fs_event->gobj;

    /*
     *  path may alias the entry we are about to delete from jn_tracked_paths
     *  (the IN_DELETE_SELF caller passes get_path()'s borrowed string), so copy
     *  it first — the trace and the inotify_rm_watch error log below must not
     *  read it after json_object_del() frees the backing string.
     */
    char path_[PATH_MAX];
    path_[0] = 0;
    if(path) {
        snprintf(path_, sizeof(path_), "%s", path);
    }

    char s_wd[64];
    snprintf(s_wd, sizeof(s_wd), "%d", wd);
    if(!json_object_get(fs_event->jn_tracked_paths, s_wd)) {
        gobj_log_error(fs_event->gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "wd not found",
            "wd" ,          "%d", wd,
            NULL
        );
    }
    drop_tracked(fs_event, wd);

    uint32_t trace_level = gobj_global_trace_level();
    if(trace_level & TRACE_FS) {
        gobj_log_debug(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_YEV_LOOP,
            "msg",              "%s", "remove watch",
            "msg2",             "%s", "💾🔶 remove watch",
            "path",             "%s", path_,
            "wd",               "%d", wd,
            "fs_flag",          "%d", fs_event->fs_flag,
            "recursive",        "%d", fs_event->fs_flag & FS_FLAG_RECURSIVE_PATHS,
            "p",                "%p", fs_event,
            NULL
        );
    }

    if(inotify_rm_watch(fs_event->fd, wd)<0) {
        if(errno != EINVAL) { // Han borrado el directorio
            gobj_log_error(fs_event->gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "inotify_rm_watch() FAILED",
                "path" ,        "%s", path_,
                "serrno" ,      "%s", strerror(errno),
                NULL
            );
        }
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  A watch forgotten: its entry, its descriptor (FS_FLAG_DIR_FDS) and its
 *  place in the index by path
 ***************************************************************************/
PRIVATE void drop_tracked(fs_event_t *fs_event, int wd)
{
    char s_wd[64];
    snprintf(s_wd, sizeof(s_wd), "%d", wd);

    json_t *jn_fd = json_object_get(fs_event->jn_tracked_fds, s_wd);
    if(jn_fd) {
        int dfd = (int)json_integer_value(jn_fd);
        if(dfd >= 0) {
            close_tracked_dir_fd(dfd);
        }
        json_object_del(fs_event->jn_tracked_fds, s_wd);
    }
    const char *path = json_string_value(json_object_get(fs_event->jn_tracked_paths, s_wd));
    if(path) {
        json_t *jn_wd = json_object_get(fs_event->jn_paths_wd, path);
        if(jn_wd && json_integer_value(jn_wd) == wd) {
            json_object_del(fs_event->jn_paths_wd, path);
        }
    }
    json_object_del(fs_event->jn_tracked_paths, s_wd);
}

/***************************************************************************
 *  FS_FLAG_DIR_FDS: the directory last watched at `path` is gone (no link
 *  left, st_nlink 0): its descriptor is closed. Kept, the kernel kept its
 *  inode, and with it its IN_DELETE_SELF and IN_IGNORED, until the close.
 *  Its entry stays until they come; its descriptor reads -1, "gone".
 ***************************************************************************/
PRIVATE void close_dir_fd_of_gone(fs_event_t *fs_event, const char *path)
{
    json_t *jn_wd = json_object_get(fs_event->jn_paths_wd, path);
    if(!jn_wd) {
        return;
    }
    char s_wd[64];
    snprintf(s_wd, sizeof(s_wd), "%d", (int)json_integer_value(jn_wd));
    json_t *jn_fd = json_object_get(fs_event->jn_tracked_fds, s_wd);
    int dfd = jn_fd? (int)json_integer_value(jn_fd) : -1;
    if(dfd < 0) {
        return;
    }
    struct stat st;
    if(fstat(dfd, &st) == 0 && st.st_nlink > 0) {
        return;     // there: another one of the same name is the one gone
    }
    close_tracked_dir_fd(dfd);
    json_object_set_new(fs_event->jn_tracked_fds, s_wd, json_integer(-1));
}

/***************************************************************************
 *  See fs_watcher.h
 ***************************************************************************/
PUBLIC int fs_watcher_dir_fd(
    fs_event_t *fs_event,
    int wd
)
{
    char s_wd[64];
    snprintf(s_wd, sizeof(s_wd), "%d", wd);
    if(!fs_event || wd < 0 || !json_object_get(fs_event->jn_tracked_paths, s_wd)) {
        errno = ENOENT;
        return -1;
    }
    json_t *jn_fd = json_object_get(fs_event->jn_tracked_fds, s_wd);
    if(!jn_fd) {
        errno = ENOTSUP;
        return -1;
    }
    int dfd = (int)json_integer_value(jn_fd);
    if(dfd < 0) {
        errno = ENOENT;
        return -1;
    }
    return dfd;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *get_path(fs_event_t *fs_event, int wd)
{
    char s_wd[64];
    snprintf(s_wd, sizeof(s_wd), "%d", wd);
    const char *path = json_string_value(json_object_get(fs_event->jn_tracked_paths, s_wd));
    if(!empty_string(path)) {
        return path;
    }

    gobj_log_error(fs_event->gobj, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "wd not found",
        "wd" ,          "%d", wd,
        NULL
    );
    return NULL;
}

/***************************************************************************
 *  Recursively add inotify watches to all subdirectories
 ***************************************************************************/
PRIVATE BOOL search_by_paths_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // dname[255]
    int level,              // level of tree where file found
    wd_option opt           // option parameter
)
{
    fs_event_t *fs_event = user_data;
    add_watch(fs_event, fullpath, TRUE);
    return TRUE; // to continue
}

PRIVATE int add_watch_recursive(fs_event_t *fs_event, const char *path)
{
    int wd = add_watch(fs_event, path, FALSE);
    if(wd < 0) {
        return -1;  // Error already logged
    }
    walk_dir_tree(
        0,
        path,
        0,
        WD_RECURSIVE|WD_MATCH_DIRECTORY,
        search_by_paths_cb,
        fs_event
    );
    return wd;
}

/***************************************************************************
 *  The pass after an overflow: every directory of the tree, the root
 *  first, is handed to the owner as FS_RESCAN_DIR_TYPE, and a directory
 *  born while its IN_CREATE was dropped is watched before (or nothing
 *  inside it would ever be heard). One walk does both.
 *
 *  It runs a slice of RESCAN_SLICE_MS per loop turn: an owner reads what
 *  every directory holds (a timeranger2 follower, the pending records of
 *  each key), and a tree of 50000 keys on a busy disk took minutes -- a
 *  yuno deaf to its agent, its commands and its timers meanwhile. The next
 *  slice comes from a timer: this is not a gobj, there is no event to post
 *  to itself, and what has to happen between two slices is precisely that
 *  the loop runs.
 *
 *  The table of watches may still hold directories that went while the
 *  events were dropped (their IN_IGNORED lost too), and a directory
 *  deleted and created again in that time is ANOTHER inode under the same
 *  path: finding its path in the table says nothing. So every directory of
 *  the pass, the ROOT included, is watched again -- inotify_add_watch() on
 *  an inode already watched returns its wd and changes nothing -- and when
 *  the wd differs from the table's, the old one is stopped (its entry goes
 *  with its IN_IGNORED: events of it may still be queued). Up to 7.25.20
 *  the root was left out: deleted and created again during an overflow, it
 *  was never heard again. The directories gone for good are stopped at the
 *  end of the pass. The entry of a wd stopped goes with its IN_IGNORED, and
 *  when that was lost with the overflow, once the stream is past where it
 *  would have come (forget_stale_wds()). Up to 7.25.21 such an entry
 *  stayed for good.
 ***************************************************************************/
PRIVATE void start_rescan_pass(fs_event_t *fs_event)
{
    if(!fs_event->yev_rescan) {
        fs_event->yev_rescan = yev_create_timer_event(
            fs_event->yev_loop,
            rescan_slice_callback,
            fs_event->gobj
        );
        if(!fs_event->yev_rescan) {
            gobj_log_error(fs_event->gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Cannot create the timer of the rescan: the lost events stay lost",
                "path",         "%s", fs_event->path,
                NULL
            );
            return;
        }
        yev_set_user_data(fs_event->yev_rescan, fs_event);
    }

    JSON_DECREF(fs_event->rescan_dirs)
    fs_event->rescan_dirs = json_array();
    json_array_append_new(fs_event->rescan_dirs, json_string(fs_event->path));

    /*
     *  The watch table is indexed by wd; the pass asks by path. Built once
     *  per pass: per slice it cost more than the slice (50000 paths)
     */
    JSON_DECREF(fs_event->rescan_watched)
    if(fs_event->fs_flag & FS_FLAG_RECURSIVE_PATHS) {
        fs_event->rescan_watched = json_object();
        const char *s_wd; json_t *jn_path;
        json_object_foreach(fs_event->jn_tracked_paths, s_wd, jn_path) {
            json_object_set_new(fs_event->rescan_watched, json_string_value(jn_path),
                json_integer(atoi(s_wd))
            );
        }
    }
    JSON_DECREF(fs_event->rescan_seen)
    fs_event->rescan_seen = json_object();
    fs_event->rescan_again = FALSE;
    fs_event->rescan_visited = 0;
    fs_event->rescan_t0 = time_in_milliseconds_monotonic();
    fs_event->rescan_slices = 0;
    fs_event->rescan_us_owner = 0;
    fs_event->rescan_us_slices = 0;
    fs_event->rescan_us_slice_end = monotonic_us();
    fs_event->rescan_us_max_gap = 0;

    yev_start_timer_event(fs_event->yev_rescan, 1, FALSE);
}

PRIVATE void stop_rescan_pass(fs_event_t *fs_event)
{
    if(fs_event->yev_rescan && yev_event_is_running(fs_event->yev_rescan)) {
        yev_stop_event(fs_event->yev_rescan);
    }
    JSON_DECREF(fs_event->rescan_dirs)
    JSON_DECREF(fs_event->rescan_watched)
    JSON_DECREF(fs_event->rescan_seen)
    fs_event->rescan_again = FALSE;
}

/*
 *  Where the time of a pass goes (said by its closing INFO): a measure, not
 *  a timeout, and an owner's call is well under a millisecond
 */
PRIVATE uint64_t monotonic_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

/*
 *  Watch `path` again, and stop a stale wd under its path (its entry goes
 *  with its IN_IGNORED). `watched` (path -> wd) is the index of the pass;
 *  without it (a watch that does not recurse holds its root alone) the
 *  table is searched.
 */
PRIVATE void watch_again(fs_event_t *fs_event, const char *path, json_t *watched)
{
    int old_wd = -1;
    if(watched) {
        json_t *jn_wd = json_object_get(watched, path);
        if(jn_wd) {
            old_wd = (int)json_integer_value(jn_wd);
        }
    } else {
        const char *s_wd; json_t *jn_path;
        json_object_foreach(fs_event->jn_tracked_paths, s_wd, jn_path) {
            if(strcmp(json_string_value(jn_path), path)==0) {
                old_wd = atoi(s_wd);
                break;
            }
        }
    }
    int wd = add_watch(fs_event, path, TRUE);
    if(wd < 0) {
        return; // Error already logged (or gone meanwhile, a warning)
    }
    if(old_wd >= 0 && old_wd != wd) {
        /*
         *  The old wd is stopped (its directory went, or was moved away),
         *  but its entry stays until its IN_IGNORED: the pass runs ahead of
         *  the stream, and events of that wd may still be queued (the
         *  removal of the directory replaced) -- taken out here they were
         *  "wd not found" errors
         */
        if(inotify_rm_watch(fs_event->fd, old_wd) < 0 && errno != EINVAL) {
            gobj_log_error(fs_event->gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "inotify_rm_watch() FAILED",
                "path" ,        "%s", path,
                "serrno" ,      "%s", strerror(errno),
                NULL
            );
        }
        note_stale_wd(fs_event, old_wd);    // its IN_IGNORED may have gone with the overflow
    }
    if(watched) {
        json_object_set_new(watched, path, json_integer(wd));
    }
}

/*
 *  Push the subdirectories of `path`, watching each one (again)
 */
PRIVATE void push_subdirectories(fs_event_t *fs_event, const char *path, json_t *watched)
{
    DIR *dir = opendir(path);
    if(!dir) {
        if(errno != ENOENT) {
            gobj_log_error(fs_event->gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "opendir() FAILED",
                "path",         "%s", path,
                "serrno",       "%s", strerror(errno),
                NULL
            );
        }
        return; // ENOENT: gone meanwhile, nothing below it to visit
    }
    struct dirent *de;
    while((de = readdir(dir)) != NULL) {
        if(strcmp(de->d_name, ".")==0 || strcmp(de->d_name, "..")==0) {
            continue;
        }
        char child[PATH_MAX];
        if(!build_path(child, sizeof(child), path, de->d_name, NULL)) {
            continue;   // Error already logged
        }
        BOOL is_dir = (de->d_type == DT_DIR)? TRUE: FALSE;
        if(de->d_type == DT_UNKNOWN) {
            is_dir = is_directory(child);
        }
        if(!is_dir) {
            continue;
        }
        watch_again(fs_event, child, watched);
        json_array_append_new(fs_event->rescan_dirs, json_string(child));
    }
    closedir(dir);
}

PRIVATE int rescan_slice_callback(yev_event_h yev_event)
{
    fs_event_t *fs_event = yev_get_user_data(yev_event);
    if(!fs_event || yev_get_state(yev_event) != YEV_ST_IDLE || !fs_event->rescan_dirs) {
        return 0;   // the stop of the timer, or a pass already dropped
    }

    json_t *watched = fs_event->rescan_watched;   // NULL if not recursive

    uint64_t us_start = monotonic_us();
    uint64_t gap = us_start - fs_event->rescan_us_slice_end;
    if(gap > fs_event->rescan_us_max_gap) {
        fs_event->rescan_us_max_gap = gap;
    }
    fs_event->rescan_slices++;

    uint64_t t0 = time_in_milliseconds_monotonic();
    fs_event->in_callback = TRUE;
    while(json_array_size(fs_event->rescan_dirs) > 0 && !fs_event->stop_requested) {
        size_t last = json_array_size(fs_event->rescan_dirs) - 1;
        char dir[PATH_MAX];
        snprintf(dir, sizeof(dir), "%s", json_string_value(json_array_get(fs_event->rescan_dirs, last)));
        json_array_remove(fs_event->rescan_dirs, last);

        if(!is_directory(dir)) {
            continue;   // gone meanwhile: its delete is the owner's to find
        }
        if(strcmp(dir, fs_event->path)==0) {
            watch_again(fs_event, dir, watched);    // the root too may be another inode now
        }
        if(watched) {
            push_subdirectories(fs_event, dir, watched);
        }

        fs_event->rescan_visited++;
        json_object_set_new(fs_event->rescan_seen, dir, json_true());
        fs_event->fs_type = FS_RESCAN_DIR_TYPE;
        json_t *jn_dir_wd = watched? json_object_get(watched, dir) : NULL;
        fs_event->event_wd = jn_dir_wd? (int)json_integer_value(jn_dir_wd) : -1;
        fs_event->subdir_wd = -1;
        fs_event->directory = dir;
        fs_event->filename = "";
        uint64_t us_owner = monotonic_us();
        fs_event->callback(fs_event);
        fs_event->rescan_us_owner += monotonic_us() - us_owner;

        if(time_in_milliseconds_monotonic() - t0 >= RESCAN_SLICE_MS) {
            break;
        }
    }
    if(!fs_event->stop_requested && (fs_event->fs_flag & FS_FLAG_BATCH_END)) {
        /*
         *  A slice of the pass is a batch too: what the owner noted in it
         *  is placed now (offset: where the stream is)
         */
        fs_event->fs_type = FS_BATCH_END_TYPE;
        fs_event->event_wd = -1;
        fs_event->subdir_wd = -1;
        fs_event->directory = (volatile char *)fs_event->path;
        fs_event->filename = "";
        fs_event->offset_end = fs_event->offset;
        fs_event->callback(fs_event);
    }
    fs_event->in_callback = FALSE;
    fs_event->rescan_us_slice_end = monotonic_us();
    fs_event->rescan_us_slices += fs_event->rescan_us_slice_end - us_start;

    if(fs_event->stop_requested) {
        /*
         *  The owner stopped the watcher from its callback: stop it now
         *  that nobody is walking with it
         */
        fs_event->stop_requested = FALSE;
        fs_stop_watcher_event(fs_event);
        return 0;
    }
    if(!fs_event->rescan_dirs) {
        return 0;
    }

    if(json_array_size(fs_event->rescan_dirs) > 0) {
        yev_start_timer_event(fs_event->yev_rescan, 1, FALSE);
        return 0;
    }

    gobj_log_info(fs_event->gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_SYSTEM,
        "msg",          "%s", "watched tree rescanned after lost inotify events",
        "path",         "%s", fs_event->path,
        "directories",  "%ld", (long)fs_event->rescan_visited,
        "ms",           "%ld", (long)(time_in_milliseconds_monotonic() - fs_event->rescan_t0),
        "slices",       "%ld", (long)fs_event->rescan_slices,
        "ms_owner",     "%ld", (long)(fs_event->rescan_us_owner/1000),
        "ms_watcher",   "%ld", (long)((fs_event->rescan_us_slices - fs_event->rescan_us_owner)/1000),
        "ms_loop",      "%ld", (long)((time_in_milliseconds_monotonic() - fs_event->rescan_t0) - fs_event->rescan_us_slices/1000),
        "max_loop_ms",  "%ld", (long)(fs_event->rescan_us_max_gap/1000),
        NULL
    );
    note_gone_directories(fs_event);
    forget_stale_wds(fs_event);     // at once, if nothing of them can still come
    if(fs_event->rescan_again) {
        start_rescan_pass(fs_event);
    } else {
        JSON_DECREF(fs_event->rescan_dirs)
        JSON_DECREF(fs_event->rescan_watched)
        JSON_DECREF(fs_event->rescan_seen)
    }
    return 0;
}

/***************************************************************************
 *  A wd stopped by a pass: its entry stays until its IN_IGNORED, events of
 *  it may still be queued. That IN_IGNORED may have been dropped with the
 *  overflow too: the stream as it ends now holds it if it comes at all.
 ***************************************************************************/
PRIVATE void note_stale_wd(fs_event_t *fs_event, int wd)
{
    if(!fs_event->stale_wds) {
        fs_event->stale_wds = json_array();
    }
    json_array_append_new(fs_event->stale_wds, json_integer(wd));
    uint64_t mark = fs_queued_events_end(fs_event);
    if(mark > fs_event->stale_mark) {
        fs_event->stale_mark = mark;
    }
}

/***************************************************************************
 *  At the end of a pass: the entries whose directory is no longer there
 *  (gone while the events were dropped, their IN_DELETE_SELF lost) are
 *  stopped. The watcher does not follow moves: a directory that is not
 *  there is gone for it. Only what the pass did not visit is asked to the
 *  filesystem: a lstat() per entry, in one piece, made the loop deaf for
 *  as long on a tree of 50000 keys.
 ***************************************************************************/
PRIVATE void note_gone_directories(fs_event_t *fs_event)
{
    const char *s_wd; json_t *jn_path;
    json_object_foreach(fs_event->jn_tracked_paths, s_wd, jn_path) {
        const char *path = json_string_value(jn_path);
        if(!path || json_object_get(fs_event->rescan_seen, path)) {
            continue;
        }
        struct stat st;
        if(lstat(path, &st) == 0 || errno != ENOENT) {
            continue;
        }
        int wd = atoi(s_wd);
        if(inotify_rm_watch(fs_event->fd, wd) < 0 && errno != EINVAL) {
            gobj_log_error(fs_event->gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "inotify_rm_watch() FAILED",
                "path" ,        "%s", path,
                "serrno" ,      "%s", strerror(errno),
                NULL
            );
        }
        note_stale_wd(fs_event, wd);
    }
}

/***************************************************************************
 *  Once the stream is past the mark, every IN_IGNORED of the stopped wds
 *  that was to come has come and taken its entry out: an entry still there
 *  lost it. It goes now.
 ***************************************************************************/
PRIVATE void forget_stale_wds(fs_event_t *fs_event)
{
    if(!fs_event->stale_wds || fs_event->offset < fs_event->stale_mark) {
        return;
    }

    uint32_t trace_level = gobj_global_trace_level();
    size_t idx; json_t *jn_wd;
    json_array_foreach(fs_event->stale_wds, idx, jn_wd) {
        char s_wd[64];
        snprintf(s_wd, sizeof(s_wd), "%d", (int)json_integer_value(jn_wd));
        json_t *jn_path = json_object_get(fs_event->jn_tracked_paths, s_wd);
        if(!jn_path) {
            continue;   // its IN_IGNORED came
        }
        if(trace_level & TRACE_FS) {
            gobj_log_debug(fs_event->gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_YEV_LOOP,
                "msg",          "%s", "forget a watch whose IN_IGNORED was lost",
                "msg2",         "%s", "💾🔶 forget a watch whose IN_IGNORED was lost",
                "path",         "%s", json_string_value(jn_path),
                "wd",           "%s", s_wd,
                NULL
            );
        }
        drop_tracked(fs_event, (int)json_integer_value(jn_wd));
    }
    JSON_DECREF(fs_event->stale_wds)
    fs_event->stale_mark = 0;
}
