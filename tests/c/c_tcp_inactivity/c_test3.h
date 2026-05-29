/****************************************************************************
 *          C_TEST3.H
 *
 *          Test C_TCP reconnect-with-backoff after a FAILED connect
 *          (inactivity model). Regression for the stall fixed in c_tcp.c:
 *          a pure tcp client with timeout_inactivity > 0 whose connect fails
 *          (server down) must keep retrying at timeout_between_connections
 *          and connect once the server comes up — not stall on first failure.
 *
 *          Copyright (c) 2024-2026, ArtGins.
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
