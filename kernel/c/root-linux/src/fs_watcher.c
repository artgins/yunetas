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
#include <sys/inotify.h>

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
PUBLIC fs_event_t *fs_open_watcher(
    yev_loop_t *yev_loop,
    const char *path,
    fs_type_t fs_type,
    fs_flag_t fs_flag,
    fs_callback_t callback,
    hgobj gobj
)
{
    int fd = inotify_init1(IN_NONBLOCK|IN_CLOEXEC);
    if(fd < 0) {
        gobj_log_critical(yev_loop->yuno?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "inotify_init1() FAILED",
            "serror",       "%s", strerror(errno),
            NULL
        );
        return NULL;
    }

    fs_event_t *fs_event = GBMEM_MALLOC(sizeof(fs_event_t));
    if(!fs_event) {
        gobj_log_critical(yev_loop->yuno?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "No memory for yev event",    // use the same string
            NULL
        );
        close(fd);
        return NULL;
    }

    fs_event->yev_loop = yev_loop;
    fs_event->type = fs_type;
    fs_event->flag = fs_flag;
    fs_event->path = GBMEM_STRDUP(path);
    fs_event->directory = NULL;
    fs_event->filename = NULL;
    fs_event->gobj = gobj;
    fs_event->callback = callback;
    fs_event->fd = fd;

    /*
     *  Alloc buffer to read
     */
    size_t len = sizeof(struct inotify_event) + NAME_MAX + 1;
    fs_event->gbuf = gbuffer_create(len, len);
    if(!fs_event->gbuf) {
        gobj_log_critical(yev_loop->yuno?gobj:0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "No memory for fs_event gbuf",
            NULL
        );
        fs_close_watcher(fs_event);
        return NULL;

    }

    fs_event->yev_event = yev_create_read_event(
        yev_loop,
        NULL, // TODO callback,
        gobj,
        fd,
        fs_event->gbuf
    );
    if(!fs_event->yev_event) {
        fs_close_watcher(fs_event);
        return NULL;
    }

    return fs_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void fs_close_watcher(
    fs_event_t *fs_event
)
{
    EXEC_AND_RESET(yev_destroy_event, fs_event->yev_event)
    if(fs_event->fd != -1) {
        close(fs_event->fd);
        fs_event->fd = -1;
    }
    GBUFFER_DECREF(fs_event->gbuf)
    GBMEM_FREE(fs_event->path)
    GBMEM_FREE(fs_event)
}
