/****************************************************************************
 *          C_TEST2.H
 *
 *          A class to test C_TCP / C_TCP_S
 *          Test: Use teston as client and test the tcp server in c_test2
 *          No interchange of messages, only connections
 *
 *          Tasks
 *          - Play teston to connect to us
 *          - Wait 2 seconds until play __input_side__ (pepon)
 *              - This will cause messages of "Disconnected To" in teston
 *          - Play __input_side__
 *          - When client connected, wait 1 second, to dropping the connection.
 *          - Teston will retry the connect (each 2 seconds)
 *          - On 3 disconnections, shutdown
 *
 *          Copyright (c) 2024, Artgins.
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
GOBJ_DECLARE_GCLASS(C_TEST2);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test2(void);


#ifdef __cplusplus
}
#endif
