/****************************************************************************
 *          C_BROKER_SESSION_SUBS.H
 *
 *          Driver of the test of what the subscriptions of a persistent
 *          session write, and of their limit per session.
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
GOBJ_DECLARE_GCLASS(C_BROKER_SESSION_SUBS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_broker_session_subs(void);


#ifdef __cplusplus
}
#endif
