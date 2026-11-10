/****************************************************************************
 *          C_GATE_CUT.H
 *
 *          Driver of the test of the cut of a gate's MQTT 3.1.1 connection:
 *          the answers written before the drop go out, then the close.
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
GOBJ_DECLARE_GCLASS(C_GATE_CUT);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_gate_cut(void);


#ifdef __cplusplus
}
#endif
