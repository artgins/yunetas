/****************************************************************************
 *          C_TEST_NODE.H
 *
 *          A gobj that publishes, destroys itself, or says goodbye, on
 *          request of test_gobj_dispatch.
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
GOBJ_DECLARE_GCLASS(C_TEST_NODE);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_TEST_FIRE);       // in: publish EV_TEST_PING
GOBJ_DECLARE_EVENT(EV_TEST_PING);       // out: the publication under test; in: a witness counts it
GOBJ_DECLARE_EVENT(EV_TEST_DIE);        // in: destroy yourself
GOBJ_DECLARE_EVENT(EV_TEST_SAY_BYE);    // in: publish EV_TEST_BYE, then destroy yourself
GOBJ_DECLARE_EVENT(EV_TEST_BYE);        // out: the last publication of a gobj that destroys itself
GOBJ_DECLARE_EVENT(EV_TEST_STOP_ME);    // in: publish EV_STOPPED, the farewell of the stop contract
GOBJ_DECLARE_EVENT(EV_TEST_MOVE);       // in: go to ST_TEST_MOVED, set by the FSM before the action
GOBJ_DECLARE_EVENT(EV_TEST_RELAY);      // in: send EV_TEST_BOUNCE to the "peer"
GOBJ_DECLARE_EVENT(EV_TEST_BOUNCE);     // in: send EV_TEST_DIE back to whoever sent it

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DECLARE_STATE(ST_TEST_MOVED);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_node(void);

#ifdef __cplusplus
}
#endif
