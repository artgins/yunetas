/****************************************************************************
 *          C_TEST1.H
 *
 *          Test C_TCP timeout_inactivity (clear traffic, no TLS)
 *
 *          A pure tcp client with timeout_inactivity > 0 must:
 *            1) connect the first time,
 *            2) close the connection after timeout_inactivity ms of silence,
 *            3) reconnect on demand when new tx data arrives, flushing it.
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
