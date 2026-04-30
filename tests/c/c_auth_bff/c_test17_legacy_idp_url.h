/****************************************************************************
 *          C_TEST17_LEGACY_IDP_URL.H
 *
 *          Driver GClass for test17_legacy_idp_url: configures the BFF
 *          with the legacy idp_url + realm pair (instead of issuer),
 *          POSTs /auth/login, validates the 200 response, and asserts
 *          that the deprecation warning fires once at mt_create.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST17_LEGACY_IDP_URL);

PUBLIC int register_c_test17_legacy_idp_url(void);

#ifdef __cplusplus
}
#endif
