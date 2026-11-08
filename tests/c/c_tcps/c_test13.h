/****************************************************************************
 *          C_TEST13.H
 *
 *          A class to test C_TCP / C_TCP_S
 *          Test: timeout_stop_tx bounds the stop of a client whose peer
 *          does not take the data in flight
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
GOBJ_DECLARE_GCLASS(C_TEST13);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test13(void);

#ifdef __cplusplus
}
#endif
