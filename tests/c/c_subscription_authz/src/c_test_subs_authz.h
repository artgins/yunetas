/****************************************************************************
 *          C_TEST_SUBS_AUTHZ.H
 *
 *          GClasses to test the authorization of an external subscription
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
GOBJ_DECLARE_GCLASS(C_TEST_SUBS_AUTHZ);     // the driver, and the remote subscriber
GOBJ_DECLARE_GCLASS(C_TEST_PUB_AUTHZ);      // the service subscribed to

/*
 *  What the authz checker of main.c was asked, to check the permission name
 */
extern int authz_asked_read;
extern int authz_asked_other;

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_test_subs_authz(void);

#ifdef __cplusplus
}
#endif
