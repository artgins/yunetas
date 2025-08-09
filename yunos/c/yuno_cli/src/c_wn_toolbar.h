/****************************************************************************
 *          C_WN_TOOLBAR.H
 *          Wn_toolbar GClass.
 *
 *          - It has layout vertical and horizontal (sure?)
 *          - Manages layout, position, size, color and selected color.
 *          - It creates a new WINDOW and PANE (sorry?)
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
 *              EV_PAINT
 *                  - clear the WINDOW
 *                  - it it has color set the color in WINDOW (back/fore)???
 *                  - update PANE or WINDOW
 *              EV_MOVE
 *                  - update PANE or WINDOW
 *              EV_SIZE
 *                  - update PANE or WINDOW
 *                  - auto-send EV_PAINT (ncurses not does refresh)
 *                  - fix_child_sizes()
 *
 *              EV_SET_SELECTED_BUTTON
 *                  - change the color of selected
 *
 *              EV_GET_PREV_SELECTED_BUTTON
 *                  - set as "selected" the previous child (? no change of color)
 *
 *              EV_GET_NEXT_SELECTED_BUTTON
 *                  - set as "selected" the next child (? no change of color)
 *
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
GOBJ_DECLARE_GCLASS(C_WN_TOOLBAR);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_wn_toolbar(void);

#ifdef __cplusplus
}
#endif
