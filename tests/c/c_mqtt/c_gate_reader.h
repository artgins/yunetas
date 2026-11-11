/****************************************************************************
 *          C_GATE_READER.H
 *
 *          Driver of the test of the memory a gate holds for peers that
 *          pipeline their packets and READ their answers.
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
GOBJ_DECLARE_GCLASS(C_GATE_READER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_gate_reader(void);


#ifdef __cplusplus
}
#endif
