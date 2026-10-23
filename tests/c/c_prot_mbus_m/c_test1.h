/****************************************************************************
 *          C_TEST1.H
 *
 *          A C_PROT_MBUS_M master against a simulated M-Bus behind a raw
 *          TCP server (a transparent gateway): the cycle, the FCB, retries,
 *          secondary addressing, and failures logged on transition
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
GOBJ_DECLARE_GCLASS(C_TEST1);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test1(void);


#ifdef __cplusplus
}
#endif
