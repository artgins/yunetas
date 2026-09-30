/****************************************************************************
 *          C_CLIENT_PUBREC.H
 *
 *          Driver of the test of the PUBRECs that C_PROT_MQTT2 receives as
 *          a client, on a fake transport.
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
GOBJ_DECLARE_GCLASS(C_CLIENT_PUBREC);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_client_pubrec(void);


#ifdef __cplusplus
}
#endif
