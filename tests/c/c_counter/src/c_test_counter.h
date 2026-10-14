/****************************************************************************
 *          C_TEST_COUNTER.H
 *
 *          Driver of test_c_counter.
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
GOBJ_DECLARE_GCLASS(C_TEST_COUNTER);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_TEST_RUN);        // posted from mt_play: case A starts
GOBJ_DECLARE_EVENT(EV_TEST_CHECK);      // posted when a counter ended: check it, start the next case

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_counter(void);

#ifdef __cplusplus
}
#endif
