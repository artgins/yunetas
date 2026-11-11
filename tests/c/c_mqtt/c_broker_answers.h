/****************************************************************************
 *          C_BROKER_ANSWERS.H
 *
 *          Driver of the test of the bound on the answers against clients
 *          of the broker that read: none is ever dropped.
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
GOBJ_DECLARE_GCLASS(C_BROKER_ANSWERS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_broker_answers(void);


#ifdef __cplusplus
}
#endif
