/****************************************************************************
 *          C_FS.H
 *          Watch file system events yev-mixin.
 *
 *          Copyright (c) 2014 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
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
GOBJ_DECLARE_GCLASS(C_FS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_FS_RENAMED);
GOBJ_DECLARE_EVENT(EV_FS_CHANGED);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_fs(void);

#ifdef __cplusplus
}
#endif
