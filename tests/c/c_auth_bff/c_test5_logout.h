/****************************************************************************
 *          C_TEST5_LOGOUT.H
 *
 *          Driver GClass for test5_logout: POSTs /auth/logout with a
 *          Cookie header carrying a fake refresh_token.  The mock
 *          Keycloak returns 204 No Content on /logout (spec-compliant
 *          OIDC RP-Initiated Logout).  The BFF should respond 200 to
 *          the browser with clear-cookie Set-Cookie headers.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST5_LOGOUT);

PUBLIC int register_c_test5_logout(void);

#ifdef __cplusplus
}
#endif
