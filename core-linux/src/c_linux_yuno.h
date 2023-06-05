/****************************************************************************
 *          c_linux_yuno.h
 *
 *          GClass __yuno__
 *          Low level
 *
 *          Copyright (c) 2023 Niyamaka.
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
GOBJ_DECLARE_GCLASS(C_YUNO);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_linux_yuno(void);

// PUBLIC int gobj_post_event(
//     hgobj dst,
//     gobj_event_t event,
//     json_t *kw,  // owned
//     hgobj src
// );

#ifdef __cplusplus
}
#endif
