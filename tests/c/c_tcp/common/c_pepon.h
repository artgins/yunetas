/****************************************************************************
 *          C_PEPON.H
 *          Pepon GClass.
 *
 *          Pepon, yuno server de pruebas
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
GOBJ_DECLARE_GCLASS(C_PEPON);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_pepon(void);

#ifdef __cplusplus
}
#endif