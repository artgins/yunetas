/****************************************************************************
 *          C_WN_BOX.H
 *          Wn_box GClass.
 *
 *          UI Box. Manage position, dimension and colors, it seems a virtual window.
 *          Well, it manages a WINDOW and PANE ncurses objects.
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
