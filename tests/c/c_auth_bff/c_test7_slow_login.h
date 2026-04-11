/****************************************************************************
 *          C_TEST7_SLOW_LOGIN.H
 *
 *          Driver GClass for test7_slow_login: same happy-path POST
 *          /auth/login as test1, but with the mock Keycloak configured
 *          to hold its response for a few hundred milliseconds via the
 *          latency_ms attr.  Validates that the BFF + c_task +
 *          c_prot_http_cl chain waits correctly for a slow upstream
 *          without timing out or losing the request.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST7_SLOW_LOGIN);

PUBLIC int register_c_test7_slow_login(void);

#ifdef __cplusplus
}
#endif
