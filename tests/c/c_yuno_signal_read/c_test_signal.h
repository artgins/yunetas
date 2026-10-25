/****************************************************************************
 *          C_TEST_SIGNAL.H
 *
 *          Closes the yuno's signalfd under its pending read, and then asks
 *          the yuno to stop with SIGQUIT: the yuno must still hear it
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
GOBJ_DECLARE_GCLASS(C_TEST_SIGNAL);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_signal(void);

#ifdef __cplusplus
}
#endif
