/****************************************************************************
 *          C_CHANNEL.H
 *          Channel GClass.
 *          Copyright (c) 2016 Niyamaka, 2024- ArtGins.
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
GOBJ_DECLARE_GCLASS(C_CHANNEL);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_channel(void);

#ifdef __cplusplus
}
#endif