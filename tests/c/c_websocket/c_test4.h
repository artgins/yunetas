/****************************************************************************
 *          C_TEST4.H
 *
 *          A C_WEBSOCKET server answers one subprotocol, or none
 *
 *          Tasks
 *          - Play a C_WEBSOCKET server
 *          - Open raw clients, one per upgrade request
 *          - Check the Sec-WebSocket-Protocol of each 101
 *          - Shutdown
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
GOBJ_DECLARE_GCLASS(C_TEST4);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test4(void);


#ifdef __cplusplus
}
#endif
