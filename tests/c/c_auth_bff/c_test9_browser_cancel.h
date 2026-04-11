/****************************************************************************
 *          C_TEST9_BROWSER_CANCEL.H
 *
 *          Driver GClass for test9_browser_cancel: reproduces the main
 *          sporadic failure pattern that motivated the auth_bff test
 *          suite — a browser that aborts its HTTP connection while the
 *          BFF is still waiting for a Keycloak reply.
 *
 *          Uses the mock Keycloak's latency_ms feature to open a
 *          deterministic window between "BFF forwards to KC" and
 *          "KC replies", then closes the driver's http_cl mid-flight
 *          and verifies that the BFF drops the reply cleanly (via
 *          the browser_alive gate added in the previous commit).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST9_BROWSER_CANCEL);

PUBLIC int register_c_test9_browser_cancel(void);

#ifdef __cplusplus
}
#endif
