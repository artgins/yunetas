/****************************************************************************
 *          C_PERF_AUTH_BFF.H
 *
 *          Orchestrator GClass for perf_auth_bff.  Drives NUM_SLOTS
 *          persistent HTTP/1.1 keep-alive clients against an equal
 *          number of BFF channels for `run_seconds` seconds and prints
 *          live Msg/sec + Bytes/sec in the style of perf_yev_ping_pong.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_PERF_AUTH_BFF);

PUBLIC int register_c_perf_auth_bff(void);

#ifdef __cplusplus
}
#endif
