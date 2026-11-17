/****************************************************************************
 *          C_TEST33_IDP_TLS_NAMES.H
 *
 *          Driver GClass for test33_idp_tls_names: each IdP of
 *          `issuers` is verified against its own name.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST33_IDP_TLS_NAMES);

PUBLIC int register_c_test33_idp_tls_names(void);

#ifdef __cplusplus
}
#endif
