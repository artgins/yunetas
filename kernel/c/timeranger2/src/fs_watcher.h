/****************************************************************************
 *              fs_watcher.h
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
#pragma once

#include <yev_loop.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
typedef enum  {
    FS_SUBDIR_CREATED_TYPE  = 1,    // use directory / filename
    FS_SUBDIR_DELETED_TYPE,         // use directory / filename
    FS_FILE_CREATED_TYPE,           // use directory / filename
    FS_FILE_DELETED_TYPE,           // use directory / filename
    FS_FILE_MODIFIED_TYPE,          // use directory / filename, see WARNING
    FS_FILE_RENAME_TYPE,            // use directory / filename TODO to check,copied from libuv
    FS_OVERFLOW_TYPE,               // directory: the watched path. The kernel's queue
                                    // overflowed: an unknown set of events was LOST
                                    // (they will not come). A pass over the tree starts:
                                    // do here what is global and cheap.
    FS_RESCAN_DIR_TYPE,             // directory: one directory of the watched tree (the
                                    // root included), during the pass that follows an
                                    // overflow: read it again, what it holds may never
                                    // have been told. Delivered a slice per loop turn;
                                    // a directory born in the overflow is watched first.
    FS_WATCHER_GONE_TYPE,           // directory: the watched path. The read of the watcher
                                    // FAILED, could not be armed again, or was canceled
                                    // from outside fs_watcher (an order broken, see
                                    // fs_watcher.md), logged: it is destroyed when this
                                    // call returns, and nothing else comes. Drop every
                                    // pointer to it. Not told when its owner stopped it.
    FS_BATCH_END_TYPE,              // Only with FS_FLAG_BATCH_END. A batch read from inotify
                                    // (or a slice of the pass after an overflow) was handed
                                    // over whole: `offset` is where the stream is.
                                    // What an owner left for "when the stream is past
                                    // here" can be done now; no event of the batch comes
                                    // after this.

    // There are more fs events available with io_uring, but this code only manages these events.
} fs_type_t;

typedef enum  {
    FS_FLAG_RECURSIVE_PATHS     = 0x0001,     // add path and all his subdirectories
    FS_FLAG_MODIFIED_FILES      = 0x0002,     // Add FS_FILE_MODIFIED_TYPE, WARNING about using it.
    FS_FLAG_BATCH_END           = 0x0004,     // Add FS_BATCH_END_TYPE after each batch
    FS_FLAG_DIR_FDS             = 0x0008,     // Hold a descriptor of each SUBDIRECTORY watched,
                                              // the very inode its watch is on: fs_watcher_dir_fd()
    FS_FLAG_MOVED_AS_DELETED    = 0x0010,     // A subdirectory moved away (renamed) is told as
                                              // FS_SUBDIR_DELETED_TYPE, by its old name. For a watch
                                              // that is NOT recursive (a moved subdirectory's own
                                              // watch would keep its old path)
} fs_flag_t;


/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct fs_event_s fs_event_t;

typedef int (*fs_callback_t)(
    fs_event_t *fs_event
);

struct fs_event_s {
    yev_loop_h yev_loop;
    yev_event_h yev_event;
    const char *path;
    fs_flag_t fs_flag;
    fs_type_t fs_type;          // Output
    volatile char *directory;   // Output
    volatile char *filename;    // Output
    hgobj gobj;
    void *user_data;
    void *user_data2;
    fs_callback_t callback;
    int fd;
    json_t *jn_tracked_paths;
    BOOL in_callback;           // Internal: yev_callback is walking the events
    BOOL stop_requested;        // Internal: stopped from inside its own callback
    yev_event_h yev_rescan;     // Internal: timer that runs the pass after an overflow, a slice per turn
    json_t *rescan_dirs;        // Internal: directories the pass has still to visit
    json_t *rescan_watched;     // Internal: paths watched, indexed by path, for the pass
    BOOL rescan_again;          // Internal: an overflow came during the pass: another pass after it
    uint64_t rescan_t0;         // Internal: start of the pass, ms monotonic
    json_int_t rescan_visited;  // Internal: directories visited by the pass
    json_int_t rescan_slices;   // Internal: slices of the pass
    uint64_t rescan_us_owner;   // Internal: us spent in the owner's callback
    uint64_t rescan_us_slices;  // Internal: us spent inside slices (the owner's included)
    uint64_t rescan_us_slice_end; // Internal: end of the last slice, us monotonic
    uint64_t rescan_us_max_gap; // Internal: the longest wait of the loop between two slices
    uint64_t offset;            // Output: where the event handed over starts in the watcher's
                                // stream of events (the bytes read from inotify before it)
    uint64_t offset_end;        // Output: where it ends (the offset of the next one)
    uint64_t batch_end;         // Internal: offset of the end of the batch being walked
    BOOL in_batch;              // Internal: yev_callback is walking a batch read from inotify
    BOOL stopping;              // Internal: its owner stopped it (fs_stop_watcher_event)
    json_t *rescan_seen;        // Internal: directories the pass visited, by path
    json_t *stale_wds;          // Internal: wds stopped by a pass, whose IN_IGNORED may never come
    uint64_t stale_mark;        // Internal: where the stream holds their IN_IGNORED, if it comes
    json_t *jn_tracked_fds;     // Internal: FS_FLAG_DIR_FDS, wd -> fd (-1: its directory is gone)
    json_t *jn_paths_wd;        // Internal: FS_FLAG_DIR_FDS, path -> the last wd watched there
    int event_wd;               // Output: the watch of `directory` (the directory where the event
                                // happened; the one visited, in FS_RESCAN_DIR_TYPE); -1 if none
    int subdir_wd;              // Output: FS_SUBDIR_CREATED_TYPE, the watch just set on the
                                // directory created; -1 if it is gone or is not watched
    json_int_t dir_fds_by_path; // Internal: FS_FLAG_DIR_FDS, subdirectories watched by their path
                                // because their descriptor could not be opened, since that was said
    BOOL dir_fds_warned;        // Internal: FS_FLAG_DIR_FDS, half the open-files limit was said
    uint64_t pad_end;           // Internal: an end of the queued events said past the stream
                                // (fs_queued_events_end() could not see a read), to close
    yev_event_h yev_pad;        // Internal: one-shot turn of the loop that closes pad_end
} ;



/*************************************************************************
 *  WARNING with FS_FILE_MODIFIED_TYPE:
 *      Be careful with IN_MODIFY in intense writing/reading,
 *      will cause IN_Q_OVERFLOW and event lost.
 *************************************************************************/
PUBLIC fs_event_t *fs_create_watcher_event(
    yev_loop_h yev_loop,
    const char *path,
    fs_flag_t fs_flag,
    fs_callback_t callback,
    hgobj gobj,
    void *user_data,
    void *user_data2
);

PUBLIC int fs_start_watcher_event(
    fs_event_t *fs_event
);
PUBLIC int fs_stop_watcher_event( // When the event is stopped the fs_event will be destroyed
    fs_event_t *fs_event
);

/*
 *  Where the events the kernel holds for this watcher BY NOW end, in its
 *  stream of events: an event handed over later with an `offset` below it was
 *  already queued when this was asked -- what it says may be what the owner
 *  has just read from the disk. Exact, from the owner's own callback and of
 *  another watcher alike: a read the kernel completed and the loop has not
 *  delivered is counted (its completion is looked for in the ring). Only
 *  when completions overflowed the ring (or FIONREAD failed) is a read
 *  counted whole, unseen: the answer may then be past the end, never short.
 *  Such an end is closed by the watcher itself: once everything queued has
 *  been delivered, the stream jumps to it with an FS_BATCH_END, so an owner
 *  waiting for it does not wait for unrelated events (up to 7.25.21 a quiet
 *  watcher never got there).
 */
PUBLIC uint64_t fs_queued_events_end(
    fs_event_t *fs_event
);

/*
 *  With FS_FLAG_DIR_FDS: the descriptor of the directory watched under `wd`
 *  (`event_wd`, `subdir_wd`): the very inode of that watch, opened before it
 *  was set, so a directory removed and made again under the same path is
 *  never taken for it -- openat()/unlinkat() through it cannot reach another
 *  directory. Owned by the watcher: do not close it; it may be closed at the
 *  next event, take it again then. -1 and errno:
 *      ENOENT   the directory is gone (or `wd` is not watched)
 *      ENOTSUP  watched without a descriptor (no FS_FLAG_DIR_FDS, the root,
 *               or the descriptor could not be opened, logged): use the path
 */
PUBLIC int fs_watcher_dir_fd(
    fs_event_t *fs_event,
    int wd
);


#ifdef __cplusplus
}
#endif
