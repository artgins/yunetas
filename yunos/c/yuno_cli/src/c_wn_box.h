/****************************************************************************
 *          C_WN_BOX.H
 *          Wn_box GClass.
 *
 *          UI Box. Manage position, dimension and colors, it seems a virtual window.
 *          Well, it manages a WINDOW and PANE ncurses objects.
 *
 *          It has the mt_child_added:
 *              - when a child is added it sends a EV_SIZE message.
 *          It has the mt_play/mt_pause,  to disable or not the box,
 *              - changing the state ST_DISABLED,ST_IDLE
 *          It supports the events:
 *              EV_PAINT
 *                  - clear the WINDOW
 *                  - it it has color set the color in WINDOW (back/fore)???
 *                  - update PANE or WINDOW
 *              EV_MOVE
 *                  - update PANE or WINDOW and send EV_MOVE to children.
 *              EV_SIZE
 *                  - update PANE or WINDOW and send EV_PAINT (ncurses not does) and EV_SIZE to children.
 *
 *          HACK: the PANE always exists, it's not an option.
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
