/****************************************************************************
 *          C_TEST11_CANCEL_RETRY.H
 *
 *          Driver GClass for test11_cancel_retry: POST /auth/login,
 *          cancel mid-round-trip, then immediately POST /auth/login
 *          again on a fresh connection and verify the second round
 *          completes cleanly.
 *
 *          Follows up on test9_browser_cancel: test9 proved the BFF
 *          drops the stale KC reply; test11 proves the BFF state
 *          (queue, priv->gobj_http, processing flag, browser_alive)
 *          is correctly re-initialised for the second request.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST11_CANCEL_RETRY);

PUBLIC int register_c_test11_cancel_retry(void);

#ifdef __cplusplus
}
#endif
