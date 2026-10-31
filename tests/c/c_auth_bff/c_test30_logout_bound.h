/****************************************************************************
 *          C_TEST30_LOGOUT_BOUND.H
 *
 *          Driver GClass for test30_logout_bound: a logout answers within
 *          idp_logout_timeout_ms whatever the IdP does.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/
GOBJ_DECLARE_GCLASS(C_TEST30_LOGOUT_BOUND);

PUBLIC int register_c_test30_logout_bound(void);

#ifdef __cplusplus
}
#endif
