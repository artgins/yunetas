/****************************************************************************
 *          yev_loop.h
 *
 *          Yuneta Event Loop
 *
 *          Copyright (c) 2024 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <time.h>
#include <liburing.h>
#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
typedef enum  {
    YEV_TIMER_EV        = 1,
    YEV_LISTEN_EV       = 2,
    YEV_READ_EV         = 3,
    YEV_READ_DATA_EV    = 4,
    YEV_WRITE_DATA_EV   = 5,
    YEV_WRITE_EV        = 6,
    YEV_TIMER_ONCE_EV   = 7,
    YEV_READ_CB_EV      = 8,
    YEV_READV_EV        = 9,

//    SOCKET_READ,
//    SOCKET_WRITE,
//    LISTEN_SOCKET_ACCEPT,
//    SOCKET_CONNECT,
} yev_type_t;

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct yev_event_s yev_event_t;
typedef struct yev_loop_s yev_loop_t;
typedef int (*yev_callback_t)(hgobj gobj, yev_event_t *event, gbuffer *gbuf);

struct yev_event_s {
    yev_loop_t *yev_loop;
    yev_type_t type;
    int fd;
    uint64_t buf;
    hgobj gobj;
    yev_callback_t callback;
//    mr_timer_cb *tcb;
//    mr_accept_cb *acb;
//    mr_read_cb *rcb;
//    mr_write_cb *wcb;
//    mr_done_cb *dcb;
//    void *user_data;
//    struct iovec iov;
};

struct yev_loop_s {
    struct io_uring ring;
    // TODO fuera yev_event_t *timer;
    hgobj yuno;
};



/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int yev_loop_create(hgobj yuno, yev_loop_t **yev_loop);
PUBLIC void yev_loop_destroy(yev_loop_t *yev_loop);
PUBLIC int yev_loop_run(yev_loop_t *yev_loop);

PUBLIC yev_event_t *yev_create_timer(yev_loop_t *loop, yev_callback_t callback, hgobj gobj);
PUBLIC void yev_timer_set(
    yev_event_t *yev_event,
    time_t timeout_ms,
    BOOL periodic
);
PUBLIC void yev_destroy_event(yev_event_t *yev_event);

#ifdef __cplusplus
}
#endif
