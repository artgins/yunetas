/****************************************************************************
 *          C_TEST_QIOGATE_STATS.H
 *
 *          GClass to test the queue gauges of C_QIOGATE, read through
 *          the stats of a C_MQIOGATE
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
GOBJ_DECLARE_GCLASS(C_TEST_QIOGATE_STATS);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_qiogate_stats(void);

extern int test_qiogate_stats_failed;   // checks that failed

#ifdef __cplusplus
}
#endif
