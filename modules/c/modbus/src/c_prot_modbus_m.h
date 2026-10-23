/****************************************************************************
 *          C_PROT_MODBUS_M.H
 *          Prot_modbus_master GClass.
 *
 *          Modbus protocol (master side)
 *
 *          Copyright (c) 2021-2023 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
/*
 *  A cycle publishes only the variables it read: a block that timed out
 *  or answered an exception is left out (after SDK 7.27.7; up to it every
 *  variable was published, the ones never read as 0). A consumer that
 *  relies on it refuses to build without it:
 *      #ifndef C_PROT_MODBUS_M_PUBLISHES_WHAT_IT_READ
 *      #error "..."
 *      #endif
 */
#define C_PROT_MODBUS_M_PUBLISHES_WHAT_IT_READ  1

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_PROT_MODBUS_M);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_prot_modbus_m(void);

#ifdef __cplusplus
}
#endif
