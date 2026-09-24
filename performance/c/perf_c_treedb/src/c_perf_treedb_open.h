/****************************************************************************
 *          c_perf_treedb_open.h
 *          Perf_treedb_open GClass.
 *
 *          Benchmark of the open of a dynamic-schema treedb by C_TREEDB,
 *          in a store of many treedbs.
 *
 *          Copyright (c) 2026, ArtGins.
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
GOBJ_DECLARE_GCLASS(C_PERF_TREEDB_OPEN);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_perf_treedb_open(void);

#ifdef __cplusplus
}
#endif
