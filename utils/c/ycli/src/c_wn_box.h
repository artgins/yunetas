/****************************************************************************
 *          C_WN_BOX.H
 *          Wn_box GClass.
 *
 *          UI Box. Manage position, size and colors, it seems a virtual window.
 *          Well, it manages a WINDOW and PANEL ncurses objects.
 *
 *          It has the mt_play/mt_pause,  to disable or not the box,
 *              - changing the state ST_DISABLED,ST_IDLE
 *
 *          It has the mt_child_added, mt_child_removed:
 *              - mt_child_added: when a child is added it sends:
 *                  - a EV_MOVE message with the position (x,y) of the box.
 *                  - a EV_SIZE message with the size (cx, cy) of the box.
 *              - mt_child_removed: nothing
 *
 *          It supports the events:
 *              EV_PAINT
 *                  - clear the WINDOW
 *                  - it it has color set the color in WINDOW (back/fore)???
 *                  - update PANEL or WINDOW
 *              EV_MOVE
 *                  - update PANEL or WINDOW
                    - send EV_MOVE to children
 *              EV_SIZE
 *                  - update PANEL or WINDOW
 *                  - auto-send EV_PAINT (ncurses not does refresh)
 *                  - send EV_SIZE to children.
 *
 *          HACK: the PANEL exists in Toolbar and Stsline, in WorkArea and Editline not.
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
GOBJ_DECLARE_GCLASS(C_WN_BOX);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_wn_box(void);

#ifdef __cplusplus
}
#endif
