/****************************************************************************
 *          C_DBA_POSTGRES.H
 *          Dba_postgres GClass.
 *
 *          DBA Dba_postgres
 *
 *          Copyright (c) 2021 by Niyamaka.
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
GOBJ_DECLARE_GCLASS(C_DBA_POSTGRES);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_dba_postgres(void);

#ifdef __cplusplus
}
#endif
