/****************************************************************************
 *          C_TEST1.H
 *
 *          A websocket client that is closing still receives: the data is discarded
 *
 *          Tasks
 *          - Play a raw server that answers the upgrade by hand
 *          - Open __output_side__, a websocket client
 *          - The server answers the upgrade and sends a frame header, and the rest of the frame after the client gave up on it
 *          - The client, closing, receives it: discarded, no FSM error
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
