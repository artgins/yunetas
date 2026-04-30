/****************************************************************************
 *          C_TEST18_DISCOVERY_FAILURE.H
 *
 *          Driver GClass for test18_discovery_failure: configures the
 *          BFF with `issuer` so the OIDC discovery task fires at
 *          mt_start, points the mock IdP at an `override_discovery_body`
 *          that omits the required `token_endpoint`, and asserts that
 *
 *            (a) `save_oidc_discovery` rejects the malformed body and
 *                emits the corresponding gobj_log_error, and
 *            (b) the BFF's discovery_done counter stays 0 — so any
 *                pending /auth/login request would remain queued
 *                rather than being routed to a half-resolved IdP.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST18_DISCOVERY_FAILURE);

PUBLIC int register_c_test18_discovery_failure(void);

#ifdef __cplusplus
}
#endif
