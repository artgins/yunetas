/****************************************************************************
 *          C_TEST10.H
 *
 *          A class to test C_TCP / C_TCP_S
 *          Test: a TLS server with ssl_verify_mode "request" takes the
 *          client's certificate as it comes, and gives its spki_sha256
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
GOBJ_DECLARE_GCLASS(C_TEST10);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test10(void);

extern int test10_reached_end;   // the test ran to its end (the yuno did not exit)
extern int test10_failed;        // checks that failed

#ifdef __cplusplus
}
#endif
