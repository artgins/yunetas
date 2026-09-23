/****************************************************************************
 *          C_TEST_LITERAL_WINS.H
 *
 *          GClass to test what an open of a dynamic-schema treedb does
 *          when its schema from C is newer than the schema file in use
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
GOBJ_DECLARE_GCLASS(C_TEST_LITERAL_WINS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_literal_wins(void);

#ifdef __cplusplus
}
#endif
