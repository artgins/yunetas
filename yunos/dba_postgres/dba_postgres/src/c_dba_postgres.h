/****************************************************************************
 *          C_DBA_POSTGRES.H
 *          Dba_postgres GClass.
 *
 *          DBA Dba_postgres
 *
 *          Copyright (c) 2021 by Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define GCLASS_DBA_POSTGRES_NAME "Dba_postgres"
#define GCLASS_DBA_POSTGRES gclass_dba_postgres()

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC GCLASS *gclass_dba_postgres(void);

#ifdef __cplusplus
}
#endif
