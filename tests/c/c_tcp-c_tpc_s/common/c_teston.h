/****************************************************************************
 *          C_TESTON.H
 *
 *          Teston, yuno client de pruebas
 *
 *          Copyright (c) 2018 by Niyamaka.
 *          Copyright (c) 2024, Artgins.
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
GOBJ_DECLARE_GCLASS(C_TESTON);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_START_TRAFFIC);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_teston(void);

#ifdef __cplusplus
}
#endif
