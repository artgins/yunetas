/****************************************************************************
 *          C_TEST1.H
 *
 *          A class to test C_TCP / C_TCP_S
 *          Test: Use pepon as server and test the tcp client in c_test1
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
