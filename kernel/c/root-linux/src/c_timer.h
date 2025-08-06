/****************************************************************************
 *          c_timer.h
 *
 *          GClass Timer
 *          High level, feed timers from periodic time of yuno
 *          ACCURACY IN SECONDS! although the parameter is in milliseconds (msec)
 *
 *          Don't use gobj_start()/gobj_stop(), USE set_timeout..(), clear_timeout()
 *
 *          Copyright (c) 2024-2025, ArtGins.
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
