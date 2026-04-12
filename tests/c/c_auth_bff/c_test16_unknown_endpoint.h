/****************************************************************************
 *          C_TEST16_UNKNOWN_ENDPOINT.H
 *
 *          Driver GClass for test16_unknown_endpoint: sends POST to
 *          /auth/nonexistent.  The BFF must reject with HTTP 404 and
 *          error_code="unknown_endpoint" before calling Keycloak.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST16_UNKNOWN_ENDPOINT);

PUBLIC int register_c_test16_unknown_endpoint(void);

#ifdef __cplusplus
}
#endif
