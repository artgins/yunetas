/****************************************************************************
 *          C_UDP.H
 *          Udp GClass: a UDP client, it sends to and reads from its url.
 *
 *          Not registered by the SDK (yunetas_register.c) nor included by
 *          yunetas.h: a yuno that uses it includes this header and calls
 *          register_c_udp(). Up to 7.29.3 this header declared C_PROT_UDP,
 *          a name nothing defines, instead of C_UDP.
 *
 *          Copyright (c) 2020 Niyamaka.
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_UDP);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_udp(void);

#ifdef __cplusplus
}
#endif
