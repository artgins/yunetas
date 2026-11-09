/****************************************************************************
 *          C_LEGACY_SESSIONS.H
 *
 *          Driver of the test of the client records of C_PROT_MQTT
 *          (deprecated) as a server: the sessions it keeps, the
 *          subscriptions of a client, on fake transports.
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
GOBJ_DECLARE_GCLASS(C_LEGACY_SESSIONS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_legacy_sessions(void);


#ifdef __cplusplus
}
#endif
