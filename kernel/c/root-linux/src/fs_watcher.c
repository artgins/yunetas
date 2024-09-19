/****************************************************************************
 *              fs_watcher.c
 *
 *              Monitoring of directories and files with io_uring
 *
 *              Copyright (c) 2024 ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <helpers.h>

#include "yunetas_ev_loop.h"
#include "fs_watcher.h"

/***************************************************************************
 *  Prototypes
 ***************************************************************************/

/***************************************************************************
 *  Data
 ***************************************************************************/

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC fs_handler_h fs_open_watcher(
    yev_loop_t *yev_loop,
    const char *directory,
    fs_type_t fs_type,
    fs_flag_t fs_flag,
    fs_callback_t callback,
    void *user_data
)
{
    fs_handler_h fs_handler = 0;

    return fs_handler;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void fs_close_watcher(
    fs_handler_h fs
)
{

}
