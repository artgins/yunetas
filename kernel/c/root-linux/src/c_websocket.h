/****************************************************************************
 *          C_WEBSOCKET.H
 *          GClass of WEBSOCKET protocol.
 *
 *  Implementation of the WebSocket protocol.
 *  `WebSockets <http://dev.w3.org/html5/websockets/>`_ allow for bidirectional
 *  communication between the browser and server.
 *
 *  Warning:
 *  The WebSocket protocol was recently finalized as `RFC 6455
 *  <http://tools.ietf.org/html/rfc6455>`_ and is not yet supported in
 *  all browsers.
 *  Refer to http://caniuse.com/websockets for details on compatibility.
 *
 *  This module only supports the latest version 13 of the RFC 6455 protocol.
 *


    Input Events                                Output Events

                    ┌───────────────────────┐
        start   ━━━▷│●                      │
                    │-----------------------│
                    │                       │
                    │                       │====▷  EV_ON_OPEN
                    │                       │====▷  EV_ON_MESSAGE
                    │                       │====▷  EV_ON_CLOSE
                    │                       │
                    │-----------------------│
        stop    ━━━▷│■  ◁--(auto) in clisrv │====▷  EV_STOPPED
                    └───────────────────────┘


 *          Copyright (c) 2013-2014 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <gobj.h>

#pragma once

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_WEBSOCKET);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DECLARE_STATE(ST_WAITING_HANDSHAKE);
GOBJ_DECLARE_STATE(ST_WAITING_FRAME_HEADER);
GOBJ_DECLARE_STATE(ST_WAITING_PAYLOAD_DATA);

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_websocket(void);

#ifdef __cplusplus
}
#endif
