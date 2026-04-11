/****************************************************************************
 *          C_TEST8_QUEUE_FULL.H
 *
 *          Driver GClass for test8_queue_full: pipelines 4 POST
 *          /auth/login requests on a single TCP connection against a
 *          BFF configured with pending_queue_size=2 and a mock KC
 *          slow enough to hold processing busy while the queue fills.
 *          Validates:
 *            - q_full_drops increments for the overflow (4th POST
 *              lands on a full queue and is rejected 503)
 *            - q_max_seen reaches the real observed peak
 *            - requests_total counts successful enqueues only
 *            - the queue drains in order, producing 3× 200 responses
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_TEST8_QUEUE_FULL);

PUBLIC int register_c_test8_queue_full(void);

#ifdef __cplusplus
}
#endif
