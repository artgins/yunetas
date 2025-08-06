/****************************************************************************
 *          c_timer0.h
 *
 *          GClass Timer
 *          Low level using liburing
 *
 *          WARNING: don't abuse of this gclass, each class instance opens a file!
 *                  Use c_timer better !!!
 *
 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
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
GOBJ_DECLARE_GCLASS(C_TIMER0);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/


/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_timer0(void);

PUBLIC void set_timeout0(hgobj gobj, json_int_t msec);
PUBLIC void set_timeout_periodic0(hgobj gobj, json_int_t msec);
PUBLIC void clear_timeout0(hgobj gobj);

#ifdef __cplusplus
}
#endif
