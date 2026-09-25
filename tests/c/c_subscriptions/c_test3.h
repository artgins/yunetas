/****************************************************************************
 *          C_TEST3.H
 *
 *          A gclass to test the HARD subscriptions of gobj_subscribe_event()
 *          and gobj_unsubscribe_event()
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
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_TEST3);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_TEST_RUN);        // posted from mt_play: run the checks
GOBJ_DECLARE_EVENT(EV_TEST_RENAMED);    // EV_ON_MESSAGE as a renaming subscription delivers it

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test3(void);

#ifdef __cplusplus
}
#endif
