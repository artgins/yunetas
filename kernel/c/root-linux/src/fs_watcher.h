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
    // There are more fs events available with io_uring, but this code only manages this events.
} fs_type_t;

typedef enum  { // WARNING 8 bits only, strings in yev_flag_s[]
    FS_FLAG_RECURSIVE   = 0x01,
} fs_flag_t;


/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct {
    uint8_t type;           // fs_type_t
    uint8_t flag;           // fs_flag_t
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
    yev_loop_t *yev_loop,
    const char *directory,
    fs_type_t fs_type,
    fs_flag_t fs_flag,
    fs_callback_t callback,
    void *user_data
);

PUBLIC void fs_close_watcher(
    fs_handler_h fs
);

#ifdef __cplusplus
}
#endif
