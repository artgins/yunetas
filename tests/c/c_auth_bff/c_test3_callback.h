/****************************************************************************
 *          C_TEST3_CALLBACK.H
 *
 *          Driver GClass for test3_callback: POSTs /auth/callback with a
 *          PKCE {code, code_verifier, redirect_uri} body.  The mock
 *          Keycloak returns its default 200 token envelope; the BFF
 *          should respond 200 with the mockuser claims.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST3_CALLBACK);

PUBLIC int register_c_test3_callback(void);

#ifdef __cplusplus
}
#endif
