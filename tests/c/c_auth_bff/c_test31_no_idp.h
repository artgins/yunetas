/****************************************************************************
 *          C_TEST31_NO_IDP.H
 *
 *          Driver GClass for test31_no_idp: a BFF with no IdP configured
 *          answers at once (see c_test31_no_idp.c).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST31_NO_IDP);

PUBLIC int register_c_test31_no_idp(void);

#ifdef __cplusplus
}
#endif
