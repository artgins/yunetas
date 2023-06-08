/****************************************************************************
 *          yev_loop.h
 *
 *          Yuneta Event Loop
 *
 *          Copyright (c) 2024 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Structures
 ***************************************************************/
typedef void *yev_loop_h;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int yev_loop_create(hgobj gobj, yev_loop_h *yev_loop);
PUBLIC void yev_loop_destroy(yev_loop_h yev_loop);
PUBLIC int yev_loop_run(yev_loop_h yev_loop);
PUBLIC int yev_loop_stop(yev_loop_h yev_loop);


#ifdef __cplusplus
}
#endif
