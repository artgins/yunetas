/****************************************************************************
 *          C_TEST_POST.H
 *
 *          A gclass to test gobj_post_message()
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
GOBJ_DECLARE_GCLASS(C_TEST_POST);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DECLARE_STATE(ST_CHAIN);           // posting to itself without pause
GOBJ_DECLARE_STATE(ST_CEILING);         // draining what the ceiling accepted

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_TEST_A);          // phase 1, the three posted from mt_play
GOBJ_DECLARE_EVENT(EV_TEST_B);
GOBJ_DECLARE_EVENT(EV_TEST_C);
GOBJ_DECLARE_EVENT(EV_TEST_D);          // phase 1, posted from the first action
GOBJ_DECLARE_EVENT(EV_TEST_TICK);       // phase 2, one link of the chain
GOBJ_DECLARE_EVENT(EV_TEST_ARM);        // phase 3, tell the child to post
GOBJ_DECLARE_EVENT(EV_TEST_X);          // phase 3, what the child posts and never gets
GOBJ_DECLARE_EVENT(EV_TEST_NOP);        // phase 4, the messages of the ceiling

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_post(void);

#ifdef __cplusplus
}
#endif
