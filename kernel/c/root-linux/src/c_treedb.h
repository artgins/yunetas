/****************************************************************************
 *          C_TREEDB.H
 *          Treedb GClass.
 *
 *          Management of treedb's
 *          - Create a __system__ timeranger
 *          - Create a treedb_system_schema (C_NODE) over the __system__ timeranger
 *
 *          - With commands (by events not yet ready) you can open/close services of treedb
 *
 *          "open-treedb"   -> create a timeranger and a treedb (C_NODE) with the schema passed
 *          "close-treedb"
 *          "delete-treedb"
 *          "create-topic"
 *          "delete-topic"
 *
 *          Copyright (c) 2021 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
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
GOBJ_DECLARE_GCLASS(C_TREEDB);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_treedb(void);


#ifdef __cplusplus
}
#endif
