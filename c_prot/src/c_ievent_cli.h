/****************************************************************************
 *          C_IEVENT_CLI.H
 *          Ievent_cli GClass.
 *
 *          Inter-event (client side)
 *          Simulate a remote service like a local gobj.
 *
 *          Copyright (c) 2016-2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <gobj.h>
#include "msg_ievent.h"

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
GOBJ_DECLARE_EVENT(EV_IDENTITY_CARD_ACK);
GOBJ_DECLARE_EVENT(EV_PLAY_YUNO);
GOBJ_DECLARE_EVENT(EV_PAUSE_YUNO);
GOBJ_DECLARE_EVENT(EV_MT_STATS);
GOBJ_DECLARE_EVENT(EV_MT_COMMAND);
GOBJ_DECLARE_EVENT(EV_SEND_COMMAND_ANSWER);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_ievent_cli(void);

#ifdef __cplusplus
}
#endif
