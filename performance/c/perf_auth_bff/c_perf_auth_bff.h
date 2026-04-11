/****************************************************************************
 *          C_PERF_AUTH_BFF.H
 *
 *          Orchestrator GClass for perf_auth_bff.  Same N-slot
 *          concurrent model as stress_auth_bff but tuned for
 *          throughput measurement: more iterations, latency_ms=0
 *          on the mock KC (instant replies), and MT_PRINT_TIME at
 *          the end.
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

/*
 *  Exposed to main_perf_auth_bff.c so register_yuno_and_more can
 *  start the timer just before the benchmark launches, and
 *  cleaning() can print the final throughput.
 */
extern time_measure_t perf_time_measure;

PUBLIC int register_c_perf_auth_bff(void);

#ifdef __cplusplus
}
#endif
