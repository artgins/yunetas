/****************************************************************************
 *          C_TEST_TCP_S_BUSY_CHANNEL.H
 *
 *          GClass to test that an accept never takes a busy channel
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
GOBJ_DECLARE_GCLASS(C_TEST_TCP_S_BUSY_CHANNEL);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_tcp_s_busy_channel(void);

#ifdef __cplusplus
}
#endif
