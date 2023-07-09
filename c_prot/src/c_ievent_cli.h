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
