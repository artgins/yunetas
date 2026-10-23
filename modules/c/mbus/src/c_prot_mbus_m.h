/****************************************************************************
 *          C_PROT_MBUS_M.H
 *          Prot_mbus_m GClass.
 *
 *          M-Bus master (EN 13757-2/-3): reads the meters of a wired bus,
 *          through a serial M-Bus master (C_UART) or a transparent M-Bus
 *          to TCP gateway (C_TCP).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_PROT_MBUS_M);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_prot_mbus_m(void);

#ifdef __cplusplus
}
#endif
