/****************************************************************************
 *          C_TEST12.H
 *
 *          A class to test C_TCP: a local stop of a TLS C_TCP says the
 *          TLS output it drops
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
GOBJ_DECLARE_GCLASS(C_TEST12);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_STOP_THE_CLIENT);     // posted at the client's EV_CONNECTED

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test12(void);


#ifdef __cplusplus
}
#endif
