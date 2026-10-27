/****************************************************************************
 *          C_TEST3.H
 *
 *          The Close a websocket peer sends is logged with its code and reason
 *
 *          Tasks
 *          - Play a raw server that answers the upgrade by hand
 *          - Open __output_side__, a websocket client
 *          - The server sends a Close frame: code 4002, reason "peer fault"
 *          - The client logs "Websocket Close received" with both
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
