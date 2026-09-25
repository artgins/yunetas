/****************************************************************************
 *          C_CLIENT_QUEUES.H
 *
 *          Driver of the test of the persistent queues of an MQTT CLIENT,
 *          against a raw broker
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
GOBJ_DECLARE_GCLASS(C_CLIENT_QUEUES);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_client_queues(void);


#ifdef __cplusplus
}
#endif
