/****************************************************************************
 *          C_TEST23_COOKIE_NO_HOST.H
 *
 *          Driver GClass for test23_cookie_no_host: two logins pipelined
 *          on one connection to a BFF with `cookie_domain`, one with a Host
 *          of that domain (Domain written) and one with no Host (no
 *          Domain).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST23_COOKIE_NO_HOST);

PUBLIC int register_c_test23_cookie_no_host(void);

#ifdef __cplusplus
}
#endif
