/****************************************************************************
 *          C_MOCK_KEYCLOAK.H
 *
 *          Test-only GClass that mimics a Keycloak token endpoint.
 *
 *          Sits behind a C_PROT_HTTP_SR in the test yuno.  On every
 *          EV_ON_MESSAGE it inspects the request URL, builds a response
 *          according to its current attrs, and sends it back via
 *          EV_SEND_MESSAGE.  The generated access_token is a real HS256
 *          JWT signed with a fixed test JWK so the BFF's base64url decode
 *          of the payload segment sees a well-formed token carrying the
 *          configured preferred_username / email claims.
 *
 *          Attrs (all SDF_RD so tests configure them at create time):
 *            return_status            (int)  HTTP status to return (default 200)
 *            username                 (str)  preferred_username claim
 *            email                    (str)  email claim
 *            access_token_ttl         (int)  seconds — exp - iat (default 300)
 *            refresh_token_ttl        (int)  seconds (default 1800)
 *            override_body            (json) if set, sent verbatim instead of
 *                                             the synthesized token envelope
 *            latency_ms               (int)  if > 0, defer the response by
 *                                             this many ms via an internal
 *                                             C_TIMER child (single pending
 *                                             slot — not re-entrant).
 *
 *          Stats (custom mt_stats, counters in PRIVATE_DATA):
 *            requests_received        token endpoint hits
 *            responses_sent           responses successfully emitted
 *            token_requests           POSTs on the /token path
 *            logout_requests          POSTs on the /logout path
 *            unknown_requests         anything else
 *            deferred_responses       requests held by latency_ms
 *            pending_cancelled        pending responses aborted by EV_ON_CLOSE
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************
 *              FSM
 ***************************************************************/
GOBJ_DECLARE_GCLASS(C_MOCK_KEYCLOAK);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_mock_keycloak(void);

/*
 *  One-time init for the process-wide HS256 test key used to sign
 *  mock access/refresh tokens.  Safe to call multiple times.
 *  Returns 0 on success, -1 on failure (error logged).
 */
PUBLIC int mock_keycloak_init_jwk(void);

/*
 *  Tear down the process-wide HS256 key.  Call from the test's
 *  cleaning() hook.
 */
PUBLIC void mock_keycloak_end_jwk(void);

#ifdef __cplusplus
}
#endif
