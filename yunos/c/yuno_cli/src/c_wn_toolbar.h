/****************************************************************************
 *          C_WN_TOOLBAR.H
 *          Wn_toolbar GClass.
 *
 *          - It has layout vertical and horizontal (sure?)
 *          - Manages dimension (position and size), color and selected color.
 *          - It creates a new WINDOW and PANE (sorry?)
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
