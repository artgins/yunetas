/****************************************************************************
 *          C_TEST_PEER_SUBS.H
 *
 *          GClasses to test what a remote peer may put in a subscription
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
GOBJ_DECLARE_GCLASS(C_TEST_PEER_SUBS);  // the driver, and every subscriber
GOBJ_DECLARE_GCLASS(C_TEST_PEER_PUB);   // the service subscribed to
GOBJ_DECLARE_GCLASS(C_TEST_PEER_SINK);  // a local subscriber

/*
 *  A wrong result found by the driver
 */
extern int test_peer_subs_failed;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_peer_subs(void);

#ifdef __cplusplus
}
#endif
