/****************************************************************************
 *          C_TEST_DISPATCH.H
 *
 *          Driver of test_gobj_dispatch.
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
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_TEST_DISPATCH);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_TEST_RUN);        // posted from mt_play: the phases run outside of it

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_dispatch(void);

#ifdef __cplusplus
}
#endif
