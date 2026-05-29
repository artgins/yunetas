/****************************************************************************
 *          C_TEST2.H
 *
 *          Test C_TCP timeout_inactivity (secure traffic, TLS)
 *
 *          Same flow as C_TEST1 but the client connects with tcps:// and a
 *          'crypto' config, exercising the inactivity close/reconnect path
 *          across the TLS handshake.
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
