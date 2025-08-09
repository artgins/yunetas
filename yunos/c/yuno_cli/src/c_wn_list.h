/****************************************************************************
 *          C_WN_LIST.H
 *          Wn_list GClass.
 *
 *          - It has "layout_type" and "scroll_size" attributes
 *          - Manages position, size and color.
 *          - It creates a new WINDOW and PANEL (sorry?)
 *          - It has a dl_list_t dl_lines (line has text,color)
 *
 *          It has the mt_play/mt_pause,  to disable or not the box,
 *              - changing the state ST_DISABLED,ST_IDLE
 *
 *          It has the local methods
 *              - clrscr
 *              - delete_line
 *              - add_line
 *              - setcolor
 *
 *          It supports the events:
 *              EV_SETTEXT
 *                  - get the "text" argument and convert in lines (add_line)
 *                  - delete lines if overflow scroll_size
 *                  - auto-send EV_PAINT
 *              EV_ON_MESSAGE
 *                  - same as EV_SETTEXT but with "gbuffer" argument.
 *              EV_PAINT
 *                  - clear the WINDOW
 *                  - write the lines of the current page
 *                  - update PANEL or WINDOW
 *
 *              EV_ON_OPEN  ? nothing
 *              EV_ON_CLOSE ? nothing
 *
 *              EV_SET_TOP_WINDOW
 *                  - call PANEL.top_panel()
 *
 *              EV_SCROLL_LINE_UP
 *              EV_SCROLL_LINE_DOWN
 *              EV_SCROLL_PAGE_UP
 *              EV_SCROLL_PAGE_DOWN
 *              EV_SCROLL_TOP
 *              EV_SCROLL_BOTTOM
 *              EV_CLRSCR
 *
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
GOBJ_DECLARE_GCLASS(C_WN_LIST);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_wn_list(void);

#ifdef __cplusplus
}
#endif
