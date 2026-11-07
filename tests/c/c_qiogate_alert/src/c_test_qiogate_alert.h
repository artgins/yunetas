/****************************************************************************
 *          C_TEST_QIOGATE_ALERT.H
 *
 *          GClass to test the queue-size alerts of C_QIOGATE
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
GOBJ_DECLARE_GCLASS(C_TEST_QIOGATE_ALERT);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_qiogate_alert(void);

extern int test_qiogate_alert_failed;   // checks that failed

#ifdef __cplusplus
}
#endif
