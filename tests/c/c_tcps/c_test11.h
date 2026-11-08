/****************************************************************************
 *          C_TEST11.H
 *
 *          A class to test C_TCP / C_TCP_S
 *          Test: a TLS handshake that does not end is dropped at
 *          timeout_tls_handshake, by a server and by a client
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
GOBJ_DECLARE_GCLASS(C_TEST11);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test11(void);

#ifdef __cplusplus
}
#endif
