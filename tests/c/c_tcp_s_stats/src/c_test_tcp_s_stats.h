/****************************************************************************
 *          C_TEST_TCP_S_STATS.H
 *
 *          GClass to test the connection stats of C_TCP_S
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
GOBJ_DECLARE_GCLASS(C_TEST_TCP_S_STATS);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_tcp_s_stats(void);

#ifdef __cplusplus
}
#endif
