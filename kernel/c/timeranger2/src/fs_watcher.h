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

    // There are more fs events available with io_uring, but this code only manages these events.
} fs_type_t;

typedef enum  {
    FS_FLAG_RECURSIVE_PATHS     = 0x0001,     // add path and all his subdirectories
    FS_FLAG_MODIFIED_FILES      = 0x0002,     // Add FS_FILE_MODIFIED_TYPE, WARNING about using it.
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


#ifdef __cplusplus
}
#endif
