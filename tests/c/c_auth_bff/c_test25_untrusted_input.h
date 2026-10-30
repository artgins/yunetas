/****************************************************************************
 *          C_TEST25_UNTRUSTED_INPUT.H
 *
 *          Driver GClass for test25_untrusted_input: what the BFF takes
 *          from others -- credentials and cookies of the browser put in the
 *          form to the IdP, tokens and bodies of the IdP, the Origin and the
 *          size of a cookie, requests pipelined on one connection.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST25_UNTRUSTED_INPUT);

PUBLIC int register_c_test25_untrusted_input(void);

#ifdef __cplusplus
}
#endif
