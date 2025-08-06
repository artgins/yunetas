/****************************************************************************
 *          C_WN_EDITLINE.H
 *          Wn_editline GClass.
 *
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_WN_EDITLINE);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_KEYCHAR);
GOBJ_DECLARE_EVENT(EV_EDITLINE_MOVE_START);
GOBJ_DECLARE_EVENT(EV_EDITLINE_MOVE_LEFT);
GOBJ_DECLARE_EVENT(EV_EDITLINE_DEL_CHAR);
GOBJ_DECLARE_EVENT(EV_EDITLINE_MOVE_END);
GOBJ_DECLARE_EVENT(EV_EDITLINE_MOVE_RIGHT);
GOBJ_DECLARE_EVENT(EV_EDITLINE_BACKSPACE);
GOBJ_DECLARE_EVENT(EV_EDITLINE_COMPLETE_LINE);
GOBJ_DECLARE_EVENT(EV_EDITLINE_DEL_EOL);
GOBJ_DECLARE_EVENT(EV_EDITLINE_ENTER);
GOBJ_DECLARE_EVENT(EV_EDITLINE_PREV_HIST);
GOBJ_DECLARE_EVENT(EV_EDITLINE_NEXT_HIST);
GOBJ_DECLARE_EVENT(EV_EDITLINE_SWAP_CHAR);
GOBJ_DECLARE_EVENT(EV_EDITLINE_DEL_LINE);
GOBJ_DECLARE_EVENT(EV_EDITLINE_DEL_PREV_WORD);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_wn_editline(void);

#ifdef __cplusplus
}
#endif
