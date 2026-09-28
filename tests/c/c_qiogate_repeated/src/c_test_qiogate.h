/****************************************************************************
 *          C_TEST_QIOGATE.H
 *
 *          Test of C_QIOGATE ignoring repeated messages.
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
 *              Constants
 ***************************************************************/
GOBJ_DECLARE_GCLASS(C_TEST_QIOGATE);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_TEST_RUN);        // run the phases, on the first cycle of the loop

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_qiogate(void);

#ifdef __cplusplus
}
#endif
