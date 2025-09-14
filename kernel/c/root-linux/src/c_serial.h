/****************************************************************************
 *          C_SERIAL.H
 *          Serial GClass.
 *
 *          Manage Serial Ports uv-mixin
 *
 *          Partial source code from https://github.com/vsergeev/c-periphery
 *
 *          Copyright (c) 2021 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
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
GOBJ_DECLARE_GCLASS(C_SERIAL);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_serial(void);

#ifdef __cplusplus
}
#endif
