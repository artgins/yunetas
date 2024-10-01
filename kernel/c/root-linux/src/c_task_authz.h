/****************************************************************************
 *          C_TASK_AUTHENTICATE.H
 *          Task_authenticate GClass.
 *
 *          Task to authenticate with OAuth2
 *
 *          Copyright (c) 2021 Niyamaka.
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
GOBJ_DECLARE_GCLASS(C_TASK_AUTHENTICATE);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_ON_TOKEN);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_task_authenticate(void);

#ifdef __cplusplus
}
#endif
