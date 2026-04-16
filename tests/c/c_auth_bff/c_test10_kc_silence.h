/****************************************************************************
 *          C_TEST10_KC_SILENCE.H
 *
 *          Driver GClass for test10_kc_silence: Keycloak accepts the
 *          TCP connection but never responds.  Validates the outbound
 *          watchdog added in the previous commit (idp_timeout_ms +
 *          ac_kc_watchdog in c_auth_bff.c).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST10_KC_SILENCE);

PUBLIC int register_c_test10_kc_silence(void);

#ifdef __cplusplus
}
#endif
