/****************************************************************************
 *          C_TEST24_NO_REFRESH_TOKEN.H
 *
 *          Driver GClass for test24_no_refresh_token: logins and refreshes
 *          answered by the IdP with no refresh_token, and successes whose
 *          tokens or lifetimes have another type.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST24_NO_REFRESH_TOKEN);

PUBLIC int register_c_test24_no_refresh_token(void);

#ifdef __cplusplus
}
#endif
