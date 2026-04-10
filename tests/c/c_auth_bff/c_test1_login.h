/****************************************************************************
 *          C_TEST1_LOGIN.H
 *
 *          Driver GClass for test1_login: POSTs /auth/login to the BFF,
 *          validates the 200 response (Set-Cookie + body shape), cross-
 *          checks the BFF's gobj_stats counters, and terminates the yuno.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST1_LOGIN);

PUBLIC int register_c_test1_login(void);

#ifdef __cplusplus
}
#endif
