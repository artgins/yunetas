/****************************************************************************
 *          C_GATE_LIMITS.H
 *
 *          Driver of the test of what a peer can do to C_PROT_MQTT2 as the
 *          input of a gate, on a fake transport.
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
GOBJ_DECLARE_GCLASS(C_GATE_LIMITS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_gate_limits(void);


#ifdef __cplusplus
}
#endif
