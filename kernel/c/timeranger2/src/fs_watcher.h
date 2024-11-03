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
 *              Copyright (c) 2024, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>
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

    // There are more fs events available with io_uring, but this code only manages these events.
} fs_type_t;

typedef enum  {
    FS_FLAG_RECURSIVE_PATHS     = 0x0001,     // add path and all his subdirectories
    FS_FLAG_MODIFIED_FILES      = 0x0002,     // Add FS_FILE_MODIFIED_TYPE, WARNING about using it.
    FS_FLAG_DEBUG               = 0x8000,     // Add FS_FILE_MODIFIED_TYPE, WARNING about using it.
} fs_flag_t;


/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct fs_event_s fs_event_t;

typedef int (*fs_callback_t)(
    fs_event_t *fs_event
);

struct fs_event_s {
    yev_loop_t *yev_loop;
    yev_event_t *yev_event;
    const char *path;
    fs_flag_t fs_flag;
    fs_type_t fs_type;          // Output
    volatile char *directory;   // Output
    volatile char *filename;    // Output
    hgobj gobj;
    void *user_data;
    fs_callback_t callback;
    int fd;
    json_t *jn_tracked_paths;
} ;



/*************************************************************************
 *  WARNING with FS_FILE_MODIFIED_TYPE:
 *      Be careful with IN_MODIFY in intense writing/reading,
 *      will cause IN_Q_OVERFLOW and event lost.
 *************************************************************************/
PUBLIC fs_event_t *fs_create_watcher_event(
    yev_loop_t *yev_loop,
    const char *path,
    fs_flag_t fs_flag,
    fs_callback_t callback,
    hgobj gobj,
    void *user_data
);

PUBLIC int fs_start_watcher_event(
    fs_event_t *fs_event
);
PUBLIC int fs_stop_watcher_event(
    fs_event_t *fs_event
);
PUBLIC void fs_destroy_watcher_event(
    fs_event_t *fs_event
);


#ifdef __cplusplus
}
#endif
