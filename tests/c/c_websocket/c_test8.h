/****************************************************************************
 *          C_TEST8.H
 *
 *          A C_WEBSOCKET server does not carry a message from one
 *          connection to the next one of the same channel
 *
 *          Tasks
 *          - Play a C_WEBSOCKET server of ONE channel
 *          - A raw client sends a text frame not final, and leaves
 *          - Another one sends a final continuation on the same channel:
 *            closed, and the bytes of the first one are not delivered
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
GOBJ_DECLARE_GCLASS(C_TEST8);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test8(void);


#ifdef __cplusplus
}
#endif
