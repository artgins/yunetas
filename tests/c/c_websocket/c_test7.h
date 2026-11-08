/****************************************************************************
 *          C_TEST7.H
 *
 *          What a websocket client closes for, and what it takes
 *
 *          Tasks
 *          - Play a raw server that answers each upgrade by hand
 *          - Open __output_side__, a websocket client that reconnects
 *          - The server answers half a 101, then frames the client must
 *            close for, then a right 101 with OWS around its values
 *          - The client closes each one, ONE warning, and takes the last
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
GOBJ_DECLARE_GCLASS(C_TEST7);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test7(void);


#ifdef __cplusplus
}
#endif
