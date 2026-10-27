/****************************************************************************
 *          C_TEST_AGENT_TREEDB_OPEN.H
 *
 *          GClass to test that the agent ends (exit 0) when its treedb does not open
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
GOBJ_DECLARE_GCLASS(C_TEST_AGENT_TREEDB_OPEN);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_agent_treedb_open(void);

#ifdef __cplusplus
}
#endif
