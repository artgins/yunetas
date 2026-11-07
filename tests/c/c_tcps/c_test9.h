/****************************************************************************
 *          C_TEST9.H
 *
 *          A class to test C_TCP / C_TCP_S
 *          Test: a TLS client whose trusted certificate cannot be loaded
 *          fails its connection, not the yuno
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
GOBJ_DECLARE_GCLASS(C_TEST9);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test9(void);

extern int test9_reached_end;   // the test ran to its end (the yuno did not exit)
extern int test9_failed;        // checks that failed

#ifdef __cplusplus
}
#endif
