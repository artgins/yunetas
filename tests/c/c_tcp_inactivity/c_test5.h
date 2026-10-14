/****************************************************************************
 *          C_TEST5.H
 *
 *          Test C_TCP reconnect after a FAILED connect with
 *          timeout_between_connections 0: it retries at a floor, with one
 *          warning, instead of never reconnecting.
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
GOBJ_DECLARE_GCLASS(C_TEST5);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test5(void);


#ifdef __cplusplus
}
#endif
