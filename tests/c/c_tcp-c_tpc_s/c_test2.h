/****************************************************************************
 *          C_TEST2.H
 *
 *          A class to test C_TCP / C_TCP_S
 *          Test: Use teston as client and test the tcp server in c_test2
 *          No interchange of messages, only connections
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