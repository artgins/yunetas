/****************************************************************************
 *          C_TEST_RUNNER.H
 *
 *          Generic JSON-driven test runner GClass.
 *
 *          Reads a test script from a JSON file and executes it step by step
 *          against the configured services (MQTT, TCP, etc.).
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
GOBJ_DECLARE_GCLASS(C_TEST_RUNNER);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_runner(void);

#ifdef __cplusplus
}
#endif
