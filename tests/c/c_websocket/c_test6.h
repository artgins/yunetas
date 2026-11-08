/****************************************************************************
 *          C_TEST6.H
 *
 *          A C_WEBSOCKET server closes a handshake left half done
 *
 *          Tasks
 *          - Play a C_WEBSOCKET server of 2 channels
 *          - Two raw clients send one byte each: both closed at
 *            timeout_handshake
 *          - A third one sends a whole upgrade: its 101
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
GOBJ_DECLARE_GCLASS(C_TEST6);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test6(void);


#ifdef __cplusplus
}
#endif
