/****************************************************************************
 *          yev_loop.h
 *
 *          Yunetas Event Loop
 *
 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <netdb.h>  // need it by struct addrinfo

#include <testing.h>
#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/*

                                    Callback:
           FLAG_IN_RING              RESPONSE
                ▲                       │
                │                       ▼
                ┌───────────────────────┐
                │                       │
                │                       │
    ────────────┘                       └───────────────────────┼────
                ▲                       │                       ▲
                │                       ▼                       │
              start                  reset FLAG_IN_RING        stop
                                        │                       │
                                        ▼                       ▼
                                   publish EV_???11          publish EV_STOPPED

   States:
                │                       │
   ───IDLE──────┼─RUNNING───────────────┴─IDLE────────────────────────────
   ───STOPPED───┘



                                                                Callback:
           FLAG_IN_RING    -1     FLAG_CANCELING  -1    -1       CANCELED
                ▲           ▲           ▲          ▲     ▲          │
                │           │           │          │     │          ▼
                ┌───────────┼───────────┼==========┼=====┼==========┐
                │                                                   │
                │                                                   │                   -1
    ────────────┘                                                   └───────────────────┼────
                ▲           ▲           ▲          ▲     ▲          │                   ▲
                │           │           │          │     │          │                   │
              start       start       stop       stop  start        ▼                  stop
                                                                 reset FLAG_IN_RING
                                                                 reset FLAG_CANCELING
                                                                    │
                                                                    ▼
                                                             publish EV_STOPPED

   States:
                │                       │                           │
   ───IDLE──────┼─RUNNING───────────────┴─WAIT_STOPPED──────────────┴─STOPPED──
───STOPPED──────┘


    States:     YEV_ST_STOPPED
                YEV_ST_RUNNING,         // IN_RING (active, not cancelling)
                YEV_ST_CANCELING,    // IN_RING CANCELING

 */


/***************************************************************
 *              Constants
 ***************************************************************/
#define DEFAULT_ENTRIES 2400    /* 2400 is recommended for manage 1000 connections */

typedef enum  { // WARNING 8 bits only
    YEV_CONNECT_TYPE    = 0x01,
    YEV_ACCEPT_TYPE     = 0x02,
    YEV_READ_TYPE       = 0x04,
    YEV_WRITE_TYPE      = 0x08,
    YEV_TIMER_TYPE      = 0x10,
} yev_type_t;

typedef enum  { // WARNING 8 bits only, strings in yev_flag_s[]
    YEV_FLAG_TIMER_PERIODIC     = 0x01,
    YEV_FLAG_USE_TLS            = 0x02,
    YEV_FLAG_CONNECTED          = 0x04,     // user
} yev_flag_t;

typedef enum  {
    YEV_ST_IDLE = 0,        // not in ring, ready for more operations
    YEV_ST_RUNNING,         // IN_RING (active, not cancelling)
    YEV_ST_CANCELING,       // IN_RING CANCELING
    YEV_ST_STOPPED,         // not inn ring, ready for die
} yev_state_t;

/***************************************************************
 *              Structures
 ***************************************************************/
typedef void *yev_event_h;
typedef void *yev_loop_h;

typedef int (*yev_callback_t)(
    yev_event_h event
);


typedef int (*yev_protocol_fill_hints_fn_t)( // fill hints according the schema
    const char *schema,
    struct addrinfo *hints,
    int *secure // fill true if needs TLS
);

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int yev_loop_create(
    hgobj yuno,
    unsigned entries,
    int keep_alive,
    yev_callback_t callback, // if return -1 the loop in yev_loop_run will break;
                             // if the callback in another function is NULL then this callback will be used.
    yev_loop_h *yev_loop
);
PUBLIC void yev_loop_destroy(yev_loop_h yev_loop);

PUBLIC int yev_loop_run(yev_loop_h yev_loop, int timeout_in_seconds);
PUBLIC int yev_loop_run_once(yev_loop_h yev_loop);
PUBLIC int yev_loop_stop(yev_loop_h yev_loop);
PUBLIC void yev_loop_reset_running(yev_loop_h yev_loop);
PUBLIC BOOL yev_event_is_stopping(yev_event_h yev_event);
PUBLIC BOOL yev_event_is_stopped(yev_event_h yev_event);
PUBLIC BOOL yev_event_is_running(yev_event_h yev_event);
PUBLIC int yev_protocol_set_protocol_fill_hints_fn( // Set your own table of protocols
    yev_protocol_fill_hints_fn_t yev_protocol_fill_hints_fn
);


/*
 *  To start a timer event, don't use this yev_start_event(), use yev_start_timer_event().
 */

PUBLIC int yev_set_gbuffer( // only for yev_create_read_event() and yev_create_write_event()
                            // you can set the same gbuffer without warning.
                            // you get a warning if overwritten the current gbuf,
                            // but it's set and the old removed.
    yev_event_h yev_event,
    gbuffer_t *gbuf // WARNING if there is previous gbuffer it will be free
                    // if NULL reset the current gbuf
);

PUBLIC gbuffer_t *yev_get_gbuf(yev_event_h yev_event);
PUBLIC int yev_get_fd(yev_event_h yev_event);
PUBLIC void yev_set_fd( // only for yev_create_read_event() and yev_create_write_event()
    yev_event_h yev_event,
    int fd
);
PUBLIC void yev_set_flag(
    yev_event_h yev_event,
    yev_flag_t flag,
    BOOL set
);
PUBLIC yev_type_t yev_get_type(yev_event_h yev_event);
PUBLIC yev_callback_t yev_get_callback(yev_event_h yev_event);
PUBLIC yev_loop_h yev_get_loop(yev_event_h yev_event);
PUBLIC yev_flag_t yev_get_flag(yev_event_h yev_event);
PUBLIC yev_state_t yev_get_state(yev_event_h yev_event);
PUBLIC int yev_get_result(yev_event_h yev_event);
PUBLIC int yev_get_dup_idx(yev_event_h yev_event);
PUBLIC hgobj yev_get_gobj(yev_event_h yev_event);
PUBLIC hgobj yev_get_yuno(yev_loop_h yev_loop);
PUBLIC int yev_set_user_data(
    yev_event_h yev_event,
    void *user_data
);
PUBLIC void * yev_get_user_data(yev_event_h yev_event);
PUBLIC const char *yev_get_state_name(yev_event_h yev_event);

PUBLIC int yev_start_event(
    yev_event_h yev_event
);
PUBLIC int yev_start_timer_event( // Create the handler fd for timer if not exist
    yev_event_h yev_event,
    time_t timeout_ms,  // timeout_ms <= 0 is equivalent to use yev_stop_event()
    BOOL periodic
);

/*
 *  The timer (once) if it's in idle can be reused, if it's stopped, you must create one new.
 */
PUBLIC int yev_stop_event(yev_event_h yev_event); // IDEMPOTENT close fd (timer,accept,connect)


/*
 *  In `connect`, `timer` and `accept` events, the socket will be closed.
 */
PUBLIC void yev_destroy_event(yev_event_h yev_event);

PUBLIC yev_event_h yev_create_timer_event(
    yev_loop_h yev_loop,
    yev_callback_t callback, // if return -1 the loop in yev_loop_run will break;
    hgobj gobj
);

PUBLIC yev_event_h yev_create_inotify_event(
    yev_loop_h yev_loop,
    yev_callback_t callback, // if return -1 the loop in yev_loop_run will break;
    hgobj gobj,
    int fd,
    gbuffer_t *gbuf
);

PUBLIC yev_event_h yev_create_connect_event( // create the socket to connect in yev_event->fd
    yev_loop_h yev_loop,
    yev_callback_t callback, // if return -1 the loop in yev_loop_run will break;
    const char *dst_url,
    const char *src_url,    /* local bind, only host:port */
    int ai_family,          /* default: AF_UNSPEC, Allow IPv4 or IPv6  (AF_INET AF_INET6) */
    int ai_flags,           /* default: AI_V4MAPPED | AI_ADDRCONFIG */
    hgobj gobj
);

PUBLIC int yev_rearm_connect_event( // re-create the socket to connect in yev_event->fd
                                    // If fd already set, let it and return
                                    // To recreate fd, previously close it and set -1
    yev_event_h yev_event,
    const char *dst_url,
    const char *src_url,    /* local bind, only host:port */
    int ai_family,          /* default: AF_UNSPEC, Allow IPv4 or IPv6  (AF_INET AF_INET6) */
    int ai_flags            /* default: AI_V4MAPPED | AI_ADDRCONFIG */
);

PUBLIC yev_event_h yev_create_accept_event( // create the socket listening in yev_event->fd
    yev_loop_h yev_loop,
    yev_callback_t callback, // if return -1 the loop in yev_loop_run will break;
    const char *listen_url,
    int backlog,            /* queue of pending connections for socket listening */
    BOOL shared,            /* open socket as shared */
    int ai_family,          /* default: AF_UNSPEC, Allow IPv4 or IPv6  (AF_INET AF_INET6) */
    int ai_flags,           /* default: AI_V4MAPPED | AI_ADDRCONFIG */
    hgobj gobj
);

PUBLIC yev_event_h yev_dup_accept_event(
    yev_event_h yev_server_accept,
    int dup_idx,
    hgobj gobj
);

PUBLIC yev_event_h yev_create_read_event(
    yev_loop_h yev_loop,
    yev_callback_t callback, // if return -1 the loop in yev_loop_run will break;
    hgobj gobj,
    int fd,
    gbuffer_t *gbuf
);
PUBLIC yev_event_h yev_create_write_event(
    yev_loop_h yev_loop,
    yev_callback_t callback, // if return -1 the loop in yev_loop_run will break;
    hgobj gobj,
    int fd,
    gbuffer_t *gbuf
);

PUBLIC const char *yev_event_type_name(yev_event_h yev_event);

/*
 *  Set TCP_NODELAY, SO_KEEPALIVE and SO_LINGER options to socket
 */
PUBLIC int set_tcp_socket_options(int fd, int delay); // Set internally in tcp sockets (client and clisrv)

PUBLIC BOOL is_tcp_socket(int fd);
PUBLIC BOOL is_udp_socket(int fd);
PUBLIC int get_peername(char *bf, size_t bfsize, int fd);
PUBLIC int get_sockname(char *bf, size_t bfsize, int fd);
PUBLIC const char **yev_flag_strings(void);
PUBLIC int set_nonblocking(int fd);
PUBLIC void set_measure_times(int types); // Set the measure of times of types (-1 all)
PUBLIC int get_measure_times(void);

#ifdef __cplusplus
}
#endif
