/****************************************************************************
 *          C_TEST_UDP_REFUSED.H
 *
 *          Driver of the C_UDP test of a peer that refuses
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>
#include <c_udp.h>      // not in yunetas.h: C_UDP is not registered by the SDK

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_TEST_UDP_REFUSED);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_udp_refused(void);


#ifdef __cplusplus
}
#endif
