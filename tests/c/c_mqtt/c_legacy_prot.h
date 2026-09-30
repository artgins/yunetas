/****************************************************************************
 *          C_LEGACY_PROT.H
 *
 *          Driver of the test of C_PROT_MQTT (deprecated) as a server, on a
 *          fake transport.
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
GOBJ_DECLARE_GCLASS(C_LEGACY_PROT);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_legacy_prot(void);


#ifdef __cplusplus
}
#endif
