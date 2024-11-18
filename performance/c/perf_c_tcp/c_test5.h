/****************************************************************************
 *          C_TEST5.H
 *
 *          A class to test C_TCP / C_TCP_S
 *          Test: Use pepon as server and test interchange of messages
 *
 *          Tasks
 *          - Play pepon as server with echo
 *          - Open __out_side__ (teston)
 *          - On open (pure cli connected to pepon), send a Hola message
 *          - On receiving the message re-send again
 *          - On 3 received messages shutdown
 *
 *          Copyright (c) 2024, Artgins.
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
