/****************************************************************************
 *          c_timer.h
 *
 *          GClass Timer
 *          Low level linux
 *
 *          Copyright (c) 2023 Niyamaka, 2024 ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>
#include <kwid.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_TIMER);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
// HACK: Systems events: defined in gobj.h
//GOBJ_DECLARE_EVENT(EV_TIMEOUT);
//GOBJ_DECLARE_EVENT(EV_TIMEOUT_PERIODIC);


/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_timer(void);

PUBLIC void set_timeout(hgobj gobj, json_int_t msec);
PUBLIC void set_timeout_periodic(hgobj gobj, json_int_t msec);
PUBLIC void clear_timeout(hgobj gobj);

#ifdef __cplusplus
}
#endif
