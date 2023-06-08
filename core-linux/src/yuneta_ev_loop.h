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
typedef void *yev_loop_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int yev_loop_init(hgobj gobj, yev_loop_t *yev_loop);
PUBLIC void yev_loop_close(yev_loop_t yev_loop);
PUBLIC int yev_loop_run(yev_loop_t yev_loop);


#ifdef __cplusplus
}
#endif
