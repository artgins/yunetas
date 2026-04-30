/****************************************************************************
 *          C_TEST_DRIVER.H
 *
 *          Shared driver GClass for the c_task_authenticate test suite.
 *
 *          Unlike the c_auth_bff suite (which uses one driver per test
 *          because each test verifies different stats/body fields), every
 *          c_task_authenticate test boils down to the same three steps:
 *
 *            1. wait briefly for the mock IdP listen socket to come up
 *            2. find the pre-created task-authenticate service, subscribe
 *               to EV_ON_TOKEN, and start it
 *            3. assert that the EV_ON_TOKEN payload's `result` field
 *               equals `expected_result` and die
 *
 *          Driver attrs:
 *            expected_result   int   Expected value of EV_ON_TOKEN's
 *                                    result field (0 = success, -1 =
 *                                    discovery/token failure).  Default 0.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST_DRIVER);

PUBLIC int register_c_test_driver(void);

#ifdef __cplusplus
}
#endif
