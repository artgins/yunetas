/****************************************************************************
 *          C_BROKER_STOP_DROPPED.H
 *
 *          Driver of the test of a broker that stops while the transports
 *          of connections it dropped are still stopping.
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
GOBJ_DECLARE_GCLASS(C_BROKER_STOP_DROPPED);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_broker_stop_dropped(void);


#ifdef __cplusplus
}
#endif
