/****************************************************************************
 *          C_PTY.H
 *          Pty GClass.
 *
 *          Pseudoterminal uv-mixin.
 *
 *          Copyright (c) 2021 Niyamaka.
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
GOBJ_DECLARE_GCLASS(C_PTY);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
// GOBJ_DECLARE_EVENT(EV_WRITE_TTY);
// GOBJ_DECLARE_EVENT(EV_TTY_DATA);
// GOBJ_DECLARE_EVENT(EV_TTY_OPEN);
// GOBJ_DECLARE_EVENT(EV_TTY_CLOSE);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_pty(void);

#ifdef __cplusplus
}
#endif
