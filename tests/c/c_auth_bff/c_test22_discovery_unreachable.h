/****************************************************************************
 *          C_TEST22_DISCOVERY_UNREACHABLE.H
 *
 *          Driver GClass for test22_discovery_unreachable: the BFF's OIDC
 *          discovery cannot connect to the IdP, its connect watchdog ends
 *          the task, and a login queued behind it is answered 502. The
 *          failure must be ONE ERROR, plus the ERROR of the 502.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST22_DISCOVERY_UNREACHABLE);

PUBLIC int register_c_test22_discovery_unreachable(void);

#ifdef __cplusplus
}
#endif
