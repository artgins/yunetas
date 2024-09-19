/****************************************************************************
 *          C_IEVENT_CLI.H
 *          Ievent_cli GClass.
 *
 *          Inter-event (client side)
 *          Simulate a remote service like a local gobj.
 *
 *          Copyright (c) 2016-2023 Niyamaka, 2024 ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <gobj.h>

#pragma once

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_IEVENT_CLI);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DECLARE_STATE(ST_WAIT_IDENTITY_CARD_ACK);

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_ievent_cli(void);

#ifdef __cplusplus
}
#endif
