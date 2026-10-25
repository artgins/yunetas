/****************************************************************************
 *          C_TEST_STATS.H
 *
 *          Reads the memory and disk stats of its yuno, with a store of its
 *          own on another filesystem (/dev/shm), and checks them
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
GOBJ_DECLARE_GCLASS(C_TEST_STATS);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_stats(void);

#ifdef __cplusplus
}
#endif
