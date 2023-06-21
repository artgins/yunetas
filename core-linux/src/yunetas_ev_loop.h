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
    hgobj gobj,
    yev_event_t *event,
    // void *data is:
    //      (gbuffer *gbuf) in READ/WRITE events that must be owned or reused
    //      (int *sock_conn_fd) in ACCEPT event
    void *data,
    BOOL stopped    // True if the event has stopped
);

struct yev_event_s {
    yev_loop_t *yev_loop;
    uint8_t type;               // yev_type_t
    uint8_t flag;               // yev_flag_t
    int fd;
    union {
        uint64_t timer_bf;
        gbuffer *gbuf;
    } bf;
    hgobj gobj;
    yev_callback_t callback;
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

PUBLIC yev_event_t *yev_create_timer_event(yev_loop_t *loop, yev_callback_t callback, hgobj gobj);
PUBLIC void yev_timer_set(
    yev_event_t *yev_event,
    time_t timeout_ms,
    BOOL periodic
);

PUBLIC int yev_start_event( // Don't use with timer event: use yev_timer_set
    yev_event_t *yev_event,
    gbuffer *gbuf           // Used with yev_create_read_event(), yev_create_write_event()
);
PUBLIC int yev_stop_event(yev_event_t *yev_event);
PUBLIC void yev_destroy_event(yev_event_t *yev_event);

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
PUBLIC yev_event_t *yev_create_connect_event(
    yev_loop_t *loop,
    yev_callback_t callback,
    hgobj gobj
);

PUBLIC yev_event_t *yev_create_accept_event(
    yev_loop_t *loop,
    yev_callback_t callback,
    hgobj gobj
);

PUBLIC int yev_setup_connect_event(
    yev_event_t *yev_event,
    const char *dst_url,
    const char *src_url     /* only host:port */
);

PUBLIC int yev_setup_accept_event(
    yev_event_t *yev_event,
    const char *listen_url,
    BOOL shared,
    BOOL exitOnError
);


#ifdef __cplusplus
}
#endif
