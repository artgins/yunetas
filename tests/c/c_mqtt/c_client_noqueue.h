/****************************************************************************
 *          C_CLIENT_NOQUEUE.H
 *
 *          Driver of the test of C_PROT_MQTT2 as a CLIENT with no queues, on a
 *          fake transport.
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
GOBJ_DECLARE_GCLASS(C_CLIENT_NOQUEUE);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_client_noqueue(void);


#ifdef __cplusplus
}
#endif
