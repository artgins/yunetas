/****************************************************************************
 *          YUNO_DBA_POSTGRES.H
 *          Dba_postgres yuno.
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
#define GCLASS_YUNO_DBA_POSTGRES_NAME "YDba_postgres"
#define ROLE_DBA_POSTGRES "dba_postgres"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void register_yuno_dba_postgres(void);

#ifdef __cplusplus
}
#endif
