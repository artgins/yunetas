/****************************************************************************
 *          C_YCOMMAND.H
 *          YCommand GClass.
 *
 *          Yuneta Command utility
 *
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
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
GOBJ_DECLARE_GCLASS(C_YCOMMAND);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_EDIT_CONFIG);
GOBJ_DECLARE_EVENT(EV_VIEW_CONFIG);
GOBJ_DECLARE_EVENT(EV_EDIT_YUNO_CONFIG);
GOBJ_DECLARE_EVENT(EV_VIEW_YUNO_CONFIG);
GOBJ_DECLARE_EVENT(EV_READ_JSON);
GOBJ_DECLARE_EVENT(EV_READ_FILE);
GOBJ_DECLARE_EVENT(EV_READ_BINARY_FILE);
GOBJ_DECLARE_EVENT(EV_TTY_OPEN);
GOBJ_DECLARE_EVENT(EV_TTY_CLOSE);
GOBJ_DECLARE_EVENT(EV_TTY_DATA);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_ycommand(void);

#ifdef __cplusplus
}
#endif
