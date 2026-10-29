/****************************************************************************
 *          C_TEST3.H
 *
 *          A C_PROT_MBUS_M master against a simulated M-Bus behind a raw
 *          TCP server (a transparent gateway): long answers that take
 *          longer on the wire than timeout_response
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
GOBJ_DECLARE_GCLASS(C_TEST3);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test3(void);


#ifdef __cplusplus
}
#endif
