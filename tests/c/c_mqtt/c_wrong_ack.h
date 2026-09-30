/****************************************************************************
 *          C_WRONG_ACK.H
 *
 *          Driver of the test of the acks of the wrong QoS, the CONNECT trace
 *          and the masked auth_data of the broker.
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
GOBJ_DECLARE_GCLASS(C_WRONG_ACK);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_wrong_ack(void);


#ifdef __cplusplus
}
#endif
