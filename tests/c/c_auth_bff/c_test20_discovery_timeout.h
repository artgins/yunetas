/****************************************************************************
 *          C_TEST20_DISCOVERY_TIMEOUT.H
 *
 *          Driver GClass for test20_discovery_timeout: configures the
 *          BFF with `issuer` so the OIDC discovery task fires at
 *          mt_start, makes the mock IdP accept the discovery GET and
 *          never answer it, and asserts that the discovery timeout
 *
 *            (a) is logged (an ERROR, not a silent end), and
 *            (b) counts in the BFF's `idp_timeouts` stat, like the
 *                timeout of a login, callback, refresh or logout.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST20_DISCOVERY_TIMEOUT);

PUBLIC int register_c_test20_discovery_timeout(void);

#ifdef __cplusplus
}
#endif
