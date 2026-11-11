/****************************************************************************
 *          C_BROKER_SESSIONS.H
 *
 *          Driver of the test of what a session of the broker keeps across
 *          a take-over, and of the in-flight limit of a session with queues.
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
GOBJ_DECLARE_GCLASS(C_BROKER_SESSIONS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_broker_sessions(void);


#ifdef __cplusplus
}
#endif
