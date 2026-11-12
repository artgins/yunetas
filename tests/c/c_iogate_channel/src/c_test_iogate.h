/****************************************************************************
 *          C_TEST_IOGATE.H
 *
 *          A gclass to test what C_IOGATE writes in a message: the
 *          channel it came in by, and the brake of "No channel FOUND"
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
GOBJ_DECLARE_GCLASS(C_TEST_IOGATE);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_TEST_RUN);        // posted from mt_play: the whole test

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_iogate(void);

#ifdef __cplusplus
}
#endif
