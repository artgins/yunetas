/****************************************************************************
 *          C_GATE_ANSWERS.H
 *
 *          Driver of the test of the bound on the answers a gate holds for
 *          peers that do not read them: many connections, and big SUBACKs.
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
GOBJ_DECLARE_GCLASS(C_GATE_ANSWERS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_gate_answers(void);


#ifdef __cplusplus
}
#endif
