/****************************************************************************
 *          C_TEST_CLI_PEER.H
 *
 *          GClass to test what C_IEVENT_CLI does with the faults of its peer
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
GOBJ_DECLARE_GCLASS(C_TEST_CLI_PEER);

/*
 *  A wrong result found by the driver
 */
extern int test_cli_peer_failed;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_cli_peer(void);

#ifdef __cplusplus
}
#endif
