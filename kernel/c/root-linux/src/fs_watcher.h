/****************************************************************************
 *              fs_watcher.h
 *
 *              Monitoring of directories and files with io_uring
 *
 *              Copyright (c) 2024 ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

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
} fs_type_t;

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct {
    uint8_t type;           // fs_type_t
    volatile char *directory;
    volatile char *filename;
    void *user_data;
} fs_event_t;

typedef int (*fs_callback_t)(
    volatile fs_event_t *event
);

typedef void *fs_handler_h;

/*********************************************************************
 *  Prototypes
 *********************************************************************/
PUBLIC fs_handler_h fs_open_watcher(
    const char *path,
    fs_type_t fs_type,
    fs_callback_t callback
);

PUBLIC void fs_close_watcher(
    fs_handler_h fs
);

#ifdef __cplusplus
}
#endif
