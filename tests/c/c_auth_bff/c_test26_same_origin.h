/****************************************************************************
 *          C_TEST26_SAME_ORIGIN.H
 *
 *          Driver GClass for test26_same_origin: a BFF with no
 *          allowed_origin serves its own origin only, and the redirect_uri
 *          a callback may have under an allowed_redirect_uri.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST26_SAME_ORIGIN);

PUBLIC int register_c_test26_same_origin(void);

#ifdef __cplusplus
}
#endif
