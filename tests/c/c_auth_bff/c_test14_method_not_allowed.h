/****************************************************************************
 *          C_TEST14_METHOD_NOT_ALLOWED.H
 *
 *          Driver GClass for test14_method_not_allowed: sends GET /auth/login
 *          instead of POST.  The BFF must reject with HTTP 405 and
 *          error_code="method_not_allowed" before calling Keycloak.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST14_METHOD_NOT_ALLOWED);

PUBLIC int register_c_test14_method_not_allowed(void);

#ifdef __cplusplus
}
#endif
