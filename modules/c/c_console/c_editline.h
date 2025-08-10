/****************************************************************************
 *          C_EDITLINE.H
 *          Editline GClass.
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
GOBJ_DECLARE_GCLASS(C_EDITLINE);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_editline(void);

PUBLIC int tty_init(void); /* Create and return a 'stdin' fd, to read input keyboard, without echo, then you can feed the editline with EV_KEYCHAR event */

#ifdef __cplusplus
}
#endif
