/****************************************************************************
 *          C_TEST_IDENTITY_CARD.H
 *
 *          GClass to test the identity cards a peer sends to C_IEVENT_SRV
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
GOBJ_DECLARE_GCLASS(C_TEST_IDENTITY_CARD);

/*
 *  A wrong result found by the driver
 */
extern int test_identity_card_failed;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_identity_card(void);

#ifdef __cplusplus
}
#endif
