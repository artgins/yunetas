/****************************************************************************
 *          C_GATE_FLOOD.H
 *
 *          Driver of the test of a peer that asks for more answers than it
 *          reads: dropped at max_unwritten_bytes, its memory bounded.
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
GOBJ_DECLARE_GCLASS(C_GATE_FLOOD);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_gate_flood(void);


#ifdef __cplusplus
}
#endif
