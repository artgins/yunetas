/****************************************************************************
 *          C_MOCK_IDP.H
 *
 *          Test-only mock of an OIDC IdP token endpoint, used by the
 *          c_task_authenticate test suite.
 *
 *          Routes by URL substring (the BFF/task posts to URLs that end
 *          in /.well-known/openid-configuration, /token or /logout).
 *
 *          Configuration attrs (all optional, sane defaults):
 *
 *            return_status_token   HTTP status for /token responses
 *                                    (default: 200; tests can force 4xx/5xx)
 *            override_token_body   if non-empty, sent verbatim instead of
 *                                  the synthesised token envelope (lets a
 *                                  test return a malformed body)
 *            override_discovery_body
 *                                  if non-empty, sent verbatim instead of
 *                                  the synthesised discovery JSON (lets a
 *                                  test return a body missing required
 *                                  endpoints — exercises the
 *                                  result_save_discovery failure path)
 *            return_status_discovery
 *                                  HTTP status for /.well-known requests
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

GOBJ_DECLARE_GCLASS(C_MOCK_IDP);

PUBLIC int register_c_mock_idp(void);

#ifdef __cplusplus
}
#endif
