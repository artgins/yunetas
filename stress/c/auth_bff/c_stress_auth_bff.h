/****************************************************************************
 *          C_STRESS_AUTH_BFF.H
 *
 *          Orchestrator GClass for stress_auth_bff.  Owns N parallel
 *          HTTP client slots that each loop
 *            connect → POST /auth/login → wait response → close
 *          on independent BFF channels for a configurable number of
 *          iterations, and at the end cross-checks the aggregated
 *          BFF and mock KC stats against what it thinks happened.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_STRESS_AUTH_BFF);

PUBLIC int register_c_stress_auth_bff(void);

#ifdef __cplusplus
}
#endif
