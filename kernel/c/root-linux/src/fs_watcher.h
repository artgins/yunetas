/****************************************************************************
 *              fs_watcher.h
 *
 *              Monitoring of directories and files with io_uring
 *
 *              Copyright (c) 2024 ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
typedef enum  {
    FS_SUBDIR_CREATED_TYPE        = 1,
    FS_SUBDIR_DELETED_TYPE,
    FS_FILE_CREATED_TYPE,
    FS_FILE_DELETED_TYPE,
    FS_FILE_MODIFIED_TYPE,      // Don't use if the files are hardly modified. Overflow and event loss.
    // There are more fs events available with io_uring, but this code only manages this events.
} fs_type_t;

typedef enum  { // WARNING 8 bits only, strings in yev_flag_s[]
    FS_FLAG_RECURSIVE   = 0x01,
} fs_flag_t;


/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct fs_event_s fs_event_t;

typedef int (*fs_callback_t)(
    volatile fs_event_t *event
);

struct fs_event_s {
    yev_loop_t *yev_loop;
    yev_event_t *yev_event;
    uint8_t type;           // fs_type_t
    uint8_t flag;           // fs_flag_t
    const char *path;
    volatile char *directory;
    volatile char *filename;
    hgobj gobj;             // If yev_loop→yuno is null, it can be used as a generic user data pointer
    fs_callback_t callback;
    int fd;
    json_t *jn_tracked_paths;
} ;



/*********************************************************************
 *  Prototypes
 *********************************************************************/
PUBLIC fs_event_t *fs_open_watcher(
    yev_loop_t *yev_loop,
    const char *path,
    fs_type_t fs_type,
    fs_flag_t fs_flag,
    fs_callback_t callback,
    hgobj gobj   // If yev_loop→yuno is null, it can be used as a generic user data pointer
);

PUBLIC void fs_close_watcher(
    fs_event_t *fs_event
);

#ifdef __cplusplus
}
#endif
