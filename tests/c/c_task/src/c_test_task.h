/****************************************************************************
 *          C_TEST_TASK.H
 *
 *          Driver of test_c_task: the gobj_jobs of its tasks.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_TEST_TASK);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_TEST_RUN);        // posted from mt_play: case A starts
GOBJ_DECLARE_EVENT(EV_TEST_CHECK);      // posted when a task ended: check it, start the next case

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_task(void);

#ifdef __cplusplus
}
#endif
