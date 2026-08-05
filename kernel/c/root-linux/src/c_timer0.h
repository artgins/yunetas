/****************************************************************************
 *          c_timer0.h
 *
 *          GClass Timer
 *          Low level using liburing
 *
 *          WARNING: don't abuse of this gclass, each class instance opens a file!
 *                  Use c_timer better !!!
 *
 *          set_timeout0..() arms and clear_timeout0() disarms: that is the
 *          whole contract. They are an escape from the gclass interface
 *          (attributes, events, commands, local methods, stats), so they are
 *          SUGAR and nothing else -- the behaviour lives in mt_writing() on
 *          the "msec" attribute, and writing that attribute leaves the timer
 *          exactly as they do.
 *
 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
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
