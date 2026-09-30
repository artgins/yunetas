/****************************************************************************
 *          c_test_schedule_slot.h
 *
 *          Drives C_WEBSTATS through two runs of the same day and checks
 *          the report each run publishes (EV_REPORT_READY).
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
GOBJ_DECLARE_GCLASS(C_TEST_SCHEDULE_SLOT);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_schedule_slot(void);

#ifdef __cplusplus
}
#endif
