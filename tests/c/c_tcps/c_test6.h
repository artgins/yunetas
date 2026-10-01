/****************************************************************************
 *          C_TEST6.H
 *
 *          A class to test C_TCP_S: a TLS server stopped and started
 *          again with a connection alive, and its certificates reloaded
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
GOBJ_DECLARE_GCLASS(C_TEST6);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test6(void);

extern int test6_reloads;   // main_test6.c: the "TLS certificates reloaded" said


#ifdef __cplusplus
}
#endif
