/****************************************************************************
 *          C_TEST6_INVALID_BODY.H
 *
 *          Driver GClass for test6_invalid_body: POSTs /auth/login with
 *          a body missing the required `password` field.  The BFF must
 *          reject with HTTP 400 BEFORE calling Keycloak (kc_calls=0).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST6_INVALID_BODY);

PUBLIC int register_c_test6_invalid_body(void);

#ifdef __cplusplus
}
#endif
