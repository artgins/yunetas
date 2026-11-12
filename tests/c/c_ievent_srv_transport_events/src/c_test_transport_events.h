/****************************************************************************
 *          C_TEST_TRANSPORT_EVENTS.H
 *
 *          GClass to test that a peer of a C_IEVENT_SRV cannot send the
 *          events of the transport itself
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
GOBJ_DECLARE_GCLASS(C_TEST_TRANSPORT_EVENTS);

/*
 *  A wrong result found by the driver
 */
extern int test_transport_events_failed;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_transport_events(void);

#ifdef __cplusplus
}
#endif
