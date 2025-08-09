/****************************************************************************
 *          C_WN_STSLINE.H
 *          Wn_stsline GClass.
 *
 *          - It has "text" attribute
 *          - Manages position, size and color.
 *          - It creates a new WINDOW and PANEL (sorry?)
 *
 *          It has the mt_play/mt_pause,  to disable or not the box,
 *              - changing the state ST_DISABLED,ST_IDLE
 *
 *          It has the mt_child_added, mt_child_removed:
 *              - mt_child_added:
 *                  - fix_child_sizes()
 *                  - send EV_PAINT to children
 *              - mt_child_removed:
 *                  - fix_child_sizes()
 *                  - send EV_PAINT to children
 *
 *          It supports the events:
 *              EV_SETTEXT
 *                  - write "text" attribute
 *                  - auto-send EV_PAINT
 *              EV_PAINT
 *                  - clear the WINDOW
 *                  - it it has color set the color in WINDOW (back/fore)???
 *                  - write "text" value in WINDOW!
 *                  - update PANEL or WINDOW
 *              EV_MOVE
 *                  - update PANEL or WINDOW
 *              EV_SIZE
 *                  - update PANEL or WINDOW
 *                  - auto-send EV_PAINT
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
GOBJ_DECLARE_GCLASS(C_WN_STSLINE);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_wn_stsline(void);

#ifdef __cplusplus
}
#endif
