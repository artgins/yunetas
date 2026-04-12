/****************************************************************************
 *          C_TEST15_MISSING_BODY.H
 *
 *          Driver GClass for test15_missing_body: sends POST /auth/callback
 *          without a JSON body.  The BFF must reject with HTTP 400 and
 *          error_code="missing_body" before calling Keycloak.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST15_MISSING_BODY);

PUBLIC int register_c_test15_missing_body(void);

#ifdef __cplusplus
}
#endif
