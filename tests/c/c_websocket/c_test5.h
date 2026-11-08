/****************************************************************************
 *          C_TEST5.H
 *
 *          A websocket client refuses a handshake that does not prove the
 *          server read its key, or that names a subprotocol it did not offer
 *
 *          Tasks
 *          - Play a raw server that answers each upgrade by hand
 *          - Open __output_side__, a websocket client that reconnects
 *          - The server answers no accept, a wrong one, a subprotocol, no
 *            Upgrade, Upgrade: h2c, no Connection, an extension, and a right
 *            handshake with a frame behind it in the same write
 *          - The client refuses the first seven, opens with the last one and
 *            delivers the frame
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
