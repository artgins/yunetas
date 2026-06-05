/****************************************************************************
 *          C_TEST19_LOGOUT_NO_COOKIE.H
 *
 *          Driver GClass for test19_logout_no_cookie: sends POST /auth/logout
 *          with no refresh_token cookie.  The BFF must reject with HTTP 401
 *          and error_code="missing_refresh_token" BEFORE reading the local
 *          rt[] buffer (regression for f002 — uninitialized-stack read).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST19_LOGOUT_NO_COOKIE);

PUBLIC int register_c_test19_logout_no_cookie(void);

#ifdef __cplusplus
}
#endif
