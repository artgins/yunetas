/****************************************************************************
 *          C_TEST32_IDP_FAILOVER.H
 *
 *          Driver GClass for test32_idp_failover: a login passes to the
 *          next IdP of `issuers`, a refresh stays with its own.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST32_IDP_FAILOVER);

PUBLIC int register_c_test32_idp_failover(void);

#ifdef __cplusplus
}
#endif
