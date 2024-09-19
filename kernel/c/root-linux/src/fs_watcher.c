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
PRIVATE int yev_callback(
    yev_event_t *event
);

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
        yev_callback,
        gobj,
        fd,
        fs_event->gbuf
    );
    if(!fs_event->yev_event) {
        fs_close_watcher(fs_event);
        return NULL;
    }
    yev_set_user_data(fs_event->yev_event, fs_event);

    yev_start_event(fs_event->yev_event);

    return fs_event;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void fs_close_watcher(
    fs_event_t *fs_event
)
{
    yev_set_fd(fs_event->yev_event, -1);
    yev_stop_event(fs_event->yev_event);
    EXEC_AND_RESET(yev_destroy_event, fs_event->yev_event)
    if(fs_event->fd != -1) {
        close(fs_event->fd);
        fs_event->fd = -1;
    }
    GBUFFER_DECREF(fs_event->gbuf)
    GBMEM_FREE(fs_event->path)
    GBMEM_FREE(fs_event)
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_callback(
    yev_event_t *event
)
{
    fs_event_t *fs_event = event->user_data;

    hgobj gobj = event->yev_loop->yuno?yev_event->gobj:0;

    if(gobj_trace_level(gobj) & TRACE_UV) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_event->flag);
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev callback",
            "msg2",         "%s", "💥 yev callback",
            "event type",   "%s", yev_event_type_name(yev_event),
            "result",       "%d", yev_event->result,
            "sres",         "%s", (yev_event->result<0)? strerror(-yev_event->result):"",
            "flag",         "%j", jn_flags,
            "p",            "%p", yev_event,
            NULL
        );
        json_decref(jn_flags);
    }
    switch(yev_event->type) {
        case YEV_READ_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Disconnected
                     */
                    if(gobj_trace_level(gobj) & TRACE_UV) {
                        if(yev_event->result != -ECANCELED) {
                            gobj_log_info(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                                "msg",          "%s", "read FAILED",
                                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                                "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                                "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                                "errno",        "%d", -yev_event->result,
                                "strerror",     "%s", strerror(-yev_event->result),
                                "p",            "%p", yev_event,
                                NULL
                            );
                        }
                    }
                    set_disconnected(gobj, strerror(-yev_event->result));

                } else {
                    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
                        gobj_trace_dump_gbuf(gobj, yev_event->gbuf, "%s: %s%s%s",
                            gobj_short_name(gobj),
                            gobj_read_str_attr(gobj, "sockname"),
                            " <- ",
                            gobj_read_str_attr(gobj, "peername")
                        );
                    }

                    INCR_ATTR_INTEGER(rxMsgs)
                    INCR_ATTR_INTEGER2(rxBytes, gbuffer_leftbytes(yev_event->gbuf))

                    GBUFFER_INCREF(yev_event->gbuf)
                    json_t *kw = json_pack("{s:I}",
                        "gbuffer", (json_int_t)(size_t)yev_event->gbuf
                    );
                    if(gobj_is_pure_child(gobj)) {
                        gobj_send_event(gobj_parent(gobj), EV_RX_DATA, kw, gobj);
                    } else {
                        gobj_publish_event(gobj, EV_RX_DATA, kw);
                    }

                    /*
                     *  Clear buffer
                     *  Re-arm read
                     */
                    if(yev_event->gbuf) {
                        gbuffer_clear(yev_event->gbuf);
                        yev_start_event(yev_event);
                    }
                }
            }
            break;

        case YEV_WRITE_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Disconnected
                     */
                    if(gobj_trace_level(gobj) & TRACE_UV) {
                        if(yev_event->result != -ECANCELED) {
                            gobj_log_info(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                                "msg",          "%s", "write FAILED",
                                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                                "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                                "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                                "errno",        "%d", -yev_event->result,
                                "strerror",     "%s", strerror(-yev_event->result),
                                "p",            "%p", yev_event,
                                NULL
                            );
                        }
                    }
                    set_disconnected(gobj, strerror(-yev_event->result));

                } else {
                    json_int_t mark = (json_int_t)gbuffer_getmark(yev_event->gbuf);
                    if(yev_event->flag & YEV_FLAG_WANT_TX_READY) {
                        json_t *kw_tx_ready = json_object();
                        json_object_set_new(kw_tx_ready, "gbuffer_mark", json_integer(mark));
                        if(gobj_is_pure_child(gobj)) {
                            gobj_send_event(gobj_parent(gobj), EV_TX_READY, kw_tx_ready, gobj);
                        } else {
                            gobj_publish_event(gobj, EV_TX_READY, kw_tx_ready);
                        }
                    }
                }

                yev_destroy_event(yev_event);
            }
            break;

        case YEV_CONNECT_TYPE:
            {
                if(yev_event->result < 0) {
                    /*
                     *  Error on connection
                     */
                    if(gobj_trace_level(gobj) & TRACE_UV) {
                        if(yev_event->result != -ECANCELED) {
                            gobj_log_error(gobj, 0,
                                "function",     "%s", __FUNCTION__,
                                "msgset",       "%s", MSGSET_LIBUV_ERROR,
                                "msg",          "%s", "connect FAILED",
                                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                                "errno",        "%d", -yev_event->result,
                                "strerror",     "%s", strerror(-yev_event->result),
                                "p",            "%p", yev_event,
                                NULL
                            );
                        }
                    }
                    set_disconnected(gobj, strerror(-yev_event->result));
                } else {
                    set_connected(gobj, yev_event->fd);
                }
            }
            break;
        default:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "event type NOT IMPLEMENTED",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                "event_type",   "%s", yev_event_type_name(yev_event),
                "p",            "%p", yev_event,
                NULL
            );
            break;
    }

    return 0;
}
