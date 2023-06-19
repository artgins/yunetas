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
typedef int (*yev_callback_t)(hgobj gobj, yev_event_h event, gbuffer *gbuf);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int yev_loop_create(hgobj yuno, unsigned entries, yev_loop_h *yev_loop);
PUBLIC void yev_loop_destroy(yev_loop_h yev_loop);
PUBLIC int yev_loop_run(yev_loop_h yev_loop);

PUBLIC yev_event_h yev_create_timer(yev_loop_h loop, yev_callback_t callback, hgobj gobj);
PUBLIC void yev_timer_set(
    yev_event_h yev_event,
    time_t timeout_ms,
    BOOL periodic
);

PUBLIC void yev_destroy_event(yev_event_h yev_event);

#ifdef __cplusplus
}
#endif
