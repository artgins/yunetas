/****************************************************************************
 *          C_TEST12.H
 *
 *          A class to test C_TCP / C_TCP_S
 *          Test: the inactivity timer of a client does not fire into
 *          ST_WAIT_STOPPED after a local drop
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
GOBJ_DECLARE_GCLASS(C_TEST12);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test12(void);

#ifdef __cplusplus
}
#endif
