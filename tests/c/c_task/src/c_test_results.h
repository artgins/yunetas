/****************************************************************************
 *          C_TEST_RESULTS.H
 *
 *          The gobj_results of the tasks of test_c_task: it says it is
 *          open, and answers, when the test tells it to.
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
GOBJ_DECLARE_GCLASS(C_TEST_RESULTS);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_TEST_OPEN);       // in: publish EV_ON_OPEN
GOBJ_DECLARE_EVENT(EV_TEST_ANSWER);     // in: publish EV_ON_MESSAGE, the result of a job

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_results(void);

#ifdef __cplusplus
}
#endif
