/****************************************************************************
 *          c_timer.h
 *
 *          GClass Timer
 *          High level, feed timers from periodic time of yuno
 *          ACCURACY IN SECONDS! although the parameter is in milliseconds (msec)
 *
 *          Don't use gobj_start()/gobj_stop(), USE set_timeout..(), clear_timeout()
 *
 *          These three are the whole contract: set_timeout..() arms,
 *          clear_timeout() disarms, and a one-shot is spent once it fires.
 *          Whether the gobj is running is INTERNAL -- the caller never has to
 *          know this gobj exists.
 *
 *          They are also an escape from the gclass interface (attributes,
 *          events, commands, local methods, stats), so they are SUGAR and
 *          nothing else: the behaviour lives in mt_writing() on the "msec"
 *          attribute, and writing that attribute leaves the timer exactly as
 *          these do.
 *
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

PUBLIC void set_timeout(hgobj gobj, json_int_t msec);   // it does the gobj_start() if not done
PUBLIC void set_timeout_periodic(hgobj gobj, json_int_t msec); // it does the gobj_start() if not done
PUBLIC void clear_timeout(hgobj gobj); // it does the gobj_stop() if not done

#ifdef __cplusplus
}
#endif
