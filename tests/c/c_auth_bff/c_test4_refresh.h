/****************************************************************************
 *          C_TEST4_REFRESH.H
 *
 *          Driver GClass for test4_refresh: POSTs /auth/refresh with a
 *          Cookie header carrying a fake refresh_token.  The mock
 *          Keycloak returns its default 200 token envelope; the BFF
 *          should respond 200 with body.success=true and new cookies.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST4_REFRESH);

PUBLIC int register_c_test4_refresh(void);

#ifdef __cplusplus
}
#endif
