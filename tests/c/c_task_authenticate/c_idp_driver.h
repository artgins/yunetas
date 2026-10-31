/****************************************************************************
 *          C_IDP_DRIVER.H
 *
 *          Driver GClass of test18_idp_keycloak.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_IDP_DRIVER);
GOBJ_DECLARE_GCLASS(C_IDP_CHANNEL);

PUBLIC int register_c_idp_driver(void);

#ifdef __cplusplus
}
#endif
