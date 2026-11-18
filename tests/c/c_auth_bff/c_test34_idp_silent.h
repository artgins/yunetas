/****************************************************************************
 *          C_TEST34_IDP_SILENT.H
 *
 *          Driver GClass for test34_idp_silent: a login passes to the
 *          next IdP of `issuers` when its IdP is silent on an open
 *          connection.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST34_IDP_SILENT);

PUBLIC int register_c_test34_idp_silent(void);

#ifdef __cplusplus
}
#endif
