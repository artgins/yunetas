/****************************************************************************
 *          C_TEST_PLAY_PAUSE.H
 *
 *          GClass to test who may play and pause a yuno through a
 *          C_IEVENT_CLI
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
GOBJ_DECLARE_GCLASS(C_TEST_PLAY_PAUSE);

/*
 *  A wrong result found by the driver
 */
extern int test_play_pause_failed;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_play_pause(void);

#ifdef __cplusplus
}
#endif
