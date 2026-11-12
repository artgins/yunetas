/****************************************************************************
 *          C_BROKER_TREEDB_SRC.H
 *
 *          Driver of the test of the treedb events of the broker: only
 *          the ones of its own treedb administer the users of authz.
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
GOBJ_DECLARE_GCLASS(C_BROKER_TREEDB_SRC);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_broker_treedb_src(void);


#ifdef __cplusplus
}
#endif
