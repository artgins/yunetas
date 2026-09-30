/****************************************************************************
 *          C_FAKE_TRANSPORT.H
 *
 *          A transport with no socket, the bottom of an MQTT protocol gobj
 *          that a test drives by hand.
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
GOBJ_DECLARE_GCLASS(C_FAKE_TRANSPORT);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_fake_transport(void);


#ifdef __cplusplus
}
#endif
