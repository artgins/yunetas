/****************************************************************************
 *          C_TEST29_TLS_ORIGIN.H
 *
 *          Driver GClass for test29_tls_origin: a BFF that ends TLS itself
 *          takes no Origin http://, and a logout with no URL to call.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST29_TLS_ORIGIN);

PUBLIC int register_c_test29_tls_origin(void);

#ifdef __cplusplus
}
#endif
