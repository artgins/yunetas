/****************************************************************************
 *          C_TEST2_KC_401.H
 *
 *          Driver GClass for test2_kc_401: POSTs /auth/login to the BFF
 *          while the mock Keycloak is configured to return 401 on the
 *          token endpoint.  Expects the BFF to bubble up as HTTP 400 and
 *          bump the kc_errors + bff_errors counters.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST2_KC_401);

PUBLIC int register_c_test2_kc_401(void);

#ifdef __cplusplus
}
#endif
