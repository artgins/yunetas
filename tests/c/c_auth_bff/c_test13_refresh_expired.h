/****************************************************************************
 *          C_TEST13_REFRESH_EXPIRED.H
 *
 *          Driver GClass for test13_refresh_expired: POSTs /auth/refresh
 *          with a fake refresh_token Cookie while the mock Keycloak is
 *          scripted to return 401 invalid_grant.  The BFF must translate
 *          this into a browser-facing HTTP 401 with error_code ==
 *          "session_expired" (NOT "invalid_credentials", which is only
 *          used for the /auth/login path).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST13_REFRESH_EXPIRED);

PUBLIC int register_c_test13_refresh_expired(void);

#ifdef __cplusplus
}
#endif
