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
 *              Structures
 ***************************************************************/
typedef void * yev_loop_h;
typedef void * yev_event_h;
typedef int (*yev_callback_t)(hgobj gobj, yev_event_h event, gbuffer *gbuf, BOOL stopped);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int yev_loop_create(hgobj yuno, unsigned entries, yev_loop_h *yev_loop);
PUBLIC void yev_loop_destroy(yev_loop_h yev_loop);
PUBLIC int yev_loop_run(yev_loop_h yev_loop);

PUBLIC yev_event_h yev_create_timer_event(yev_loop_h loop, yev_callback_t callback, hgobj gobj);
PUBLIC void yev_timer_set(
    yev_event_h yev_event,
    time_t timeout_ms,
    BOOL periodic
);

PUBLIC int yev_start_event(yev_event_h yev_event); // Don't use with timer event: use yev_timer_set
PUBLIC int yev_stop_event(yev_event_h yev_event);
PUBLIC void yev_destroy_event(yev_event_h yev_event);

PUBLIC yev_event_h yev_create_accept_event(yev_loop_h loop, yev_callback_t callback, hgobj gobj, int fd);
PUBLIC yev_event_h yev_create_read_event(yev_loop_h loop, yev_callback_t callback, hgobj gobj, int fd);
PUBLIC yev_event_h yev_create_write_event(yev_loop_h loop, yev_callback_t callback, hgobj gobj, int fd);

#ifdef __cplusplus
}
#endif
