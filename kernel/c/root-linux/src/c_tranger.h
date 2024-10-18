/****************************************************************************
 *          C_TRANGER.H
 *          Resource GClass.
 *
 *          Resources with tranger
 *
 *          Copyright (c) 2020 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 *
 *          Wrapper over tranger
 *
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
GOBJ_DECLARE_GCLASS(C_TRANGER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_TRANGER_ADD_RECORD);
GOBJ_DECLARE_EVENT(EV_TRANGER_RECORD_ADDED);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_tranger(void);


#ifdef __cplusplus
}
#endif
