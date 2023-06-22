/****************************************************************************
 *          yunetas_event_loop.h
 *
 *          Yunetas Event Loop
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <time.h>
#include <liburing.h>
#include <gobj.h>
#include <parse_url.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define DEFAULT_ENTRIES 2024

typedef enum  {
    YEV_TIMER_TYPE        = 1,
    YEV_READ_TYPE,
    YEV_WRITE_TYPE,
    YEV_CONNECT_TYPE,
    YEV_ACCEPT_TYPE,
} yev_type_t;

typedef enum  { // WARNING 8 bits only
    YEV_STOPPING_FLAG           = 0x01,
    YEV_STOPPED_FLAG            = 0x02,
    YEV_TIMER_PERIODIC_FLAG     = 0x04,
    YEV_USE_SSL                 = 0x08,
    YEV_IS_TCP                  = 0x10,
} yev_flag_t;

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct yev_event_s yev_event_t;
typedef struct yev_loop_s yev_loop_t;

typedef int (*yev_callback_t)(
    yev_event_t *event
);

struct yev_event_s {
    yev_loop_t *yev_loop;
    uint8_t type;               // yev_type_t
    uint8_t flag;               // yev_flag_t
    int fd;
    uint64_t timer_bf;
    gbuffer *gbuf;
    hgobj gobj;
    yev_callback_t callback;
    int result;
    struct sockaddr *dst_addr;
    socklen_t dst_addrlen;
    struct sockaddr *src_addr;
    socklen_t src_addrlen;
};

struct yev_loop_s {
    struct io_uring ring;
    unsigned entries;
    hgobj yuno;
    volatile int running;
};


/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int yev_loop_create(hgobj yuno, unsigned entries, yev_loop_t **yev_loop);
PUBLIC void yev_loop_destroy(yev_loop_t *yev_loop);

PUBLIC int yev_loop_run(yev_loop_t *yev_loop);
PUBLIC int yev_loop_stop(yev_loop_t *yev_loop);

/*
 *  Parameter `gbuf`: to use only with yev_create_read_event() and yev_create_write_event().
 *  To start a timer event, don't use this yev_start_event(), use yev_start_timer_event().
 *  Before start `connects` and `accepts` events, you need to configure them with
 *      yev_setup_connect_event() and yev_setup_accept_event().
 *      These functions will create and configure a socket to listen or to connect
 */
PUBLIC int yev_start_event(
    yev_event_t *yev_event,
    gbuffer *gbuf // only for yev_create_read_event() and yev_create_write_event()
);
PUBLIC int yev_start_timer_event(
    yev_event_t *yev_event,
    time_t timeout_ms,  // timeout_ms <= 0 is equivalent to use yev_stop_event()
    BOOL periodic
);

/*
 *  In `connects` and `accepts` events, the socket will be closed.
 */
PUBLIC int yev_stop_event(yev_event_t *yev_event);

PUBLIC void yev_destroy_event(yev_event_t *yev_event);

PUBLIC yev_event_t *yev_create_timer_event(
    yev_loop_t *loop,
    yev_callback_t callback,
    hgobj gobj
);

PUBLIC yev_event_t *yev_create_connect_event(
    yev_loop_t *loop,
    yev_callback_t callback,
    hgobj gobj
);
PUBLIC int yev_setup_connect_event(
    yev_event_t *yev_event,
    const char *dst_url,
    const char *src_url     /* only host:port */
);

PUBLIC yev_event_t *yev_create_accept_event(
    yev_loop_t *loop,
    yev_callback_t callback,
    hgobj gobj
);
PUBLIC int yev_setup_accept_event(
    yev_event_t *yev_event,
    const char *listen_url,
    int backlog,
    BOOL shared
);

PUBLIC yev_event_t *yev_create_read_event(
    yev_loop_t *loop,
    yev_callback_t callback,
    hgobj gobj,
    int fd
);
PUBLIC yev_event_t *yev_create_write_event(
    yev_loop_t *loop,
    yev_callback_t callback,
    hgobj gobj,
    int fd
);


#ifdef __cplusplus
}
#endif
