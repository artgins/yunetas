/****************************************************************************
 *          C_LISTEN.H
 *
 *          A class to test C_TCP / C_TCP_S
 *          Test: Use pepon as server and receive messages
 *
 *          Tasks
 *          - Play pepon as server with echo
 *          - Open __out_side__ (teston)
 *
 *          Copyright (c) 2025, Artgins.
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
GOBJ_DECLARE_GCLASS(C_LISTEN);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_listen(void);


#ifdef __cplusplus
}
#endif
