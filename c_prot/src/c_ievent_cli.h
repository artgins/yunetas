/****************************************************************************
 *          C_IEVENT_CLI.H
 *          Ievent_cli GClass.
 *
 *          Inter-event client protocol
 *
 *          Copyright (c) 2016-2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

// TODO check these includes
//#include "yuneta_version.h"
//#include "msglog_yuneta.h"
//#include "c_iogate.h"
//#include "c_timer.h"


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
