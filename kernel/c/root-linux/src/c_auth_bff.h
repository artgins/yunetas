/****************************************************************************
 *          c_auth_bff.h
 *          Auth_bff GClass — Backend For Frontend OAuth2 helper.
 *
 *  SEC-06: The BFF runs as a separate HTTP listener (default port 1801).
 *  It provides three endpoints consumed by the browser SPA:
 *
 *    POST /auth/callback  — exchange PKCE authorization code for tokens
 *    POST /auth/refresh   — refresh access_token using the httpOnly cookie
 *    POST /auth/logout    — revoke refresh_token and clear cookies
 *
 *  Tokens are NEVER returned to JavaScript.  They are written as
 *  httpOnly; Secure; SameSite=Strict cookies so the browser forwards
 *  them automatically with every WebSocket HTTP Upgrade to port 1800.
 *
 *  IdP configuration (priority order):
 *
 *    1. Explicit endpoints — set both `token_endpoint` and
 *       `end_session_endpoint` to the full URLs.  Skips discovery.
 *
 *    2. OIDC discovery — set `issuer` to the IdP issuer URL
 *       (e.g. https://auth.example.com/realms/foo/).  At mt_start the
 *       BFF GETs <issuer>/.well-known/openid-configuration and caches
 *       the resolved endpoints.  Standard OIDC, IdP-agnostic.
 *
 *    3. Legacy Keycloak path scheme — `idp_url` + `realm` (DEPRECATED).
 *       Builds <idp_url>/realms/<realm>/protocol/openid-connect/token
 *       and .../logout.  Emits a warning at startup; will be removed
 *       in a future release.  Migrate to (1) or (2).
 *
 *  The `iss` claim of every JWT validated by c_authz must match the
 *  `issuer` value returned by the discovery document (or the issuer of
 *  the IdP backing the explicit endpoints).
 *
 *  Copyright (c) 2026, ArtGins.
 *  All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************
 *      GClass name
 ***************************************************************/
GOBJ_DECLARE_GCLASS(C_AUTH_BFF);

/****************************************************************************
 *  BFF error response contract (browser-facing)
 *
 *  Every error response to the browser carries a JSON body of the shape:
 *
 *    { "success": false,
 *      "error_code": "<stable_snake_case_code>",
 *      "error":      "<english fallback, NOT for display>" }
 *
 *  The `error_code` is the stable contract with the GUI — use it as the
 *  i18n translation key.  The `error` field is a developer-facing English
 *  fallback, mirrored into the server logs; the GUI must NOT display it
 *  as-is (end users never want to read "IdP token exchange failed").
 *
 *  ---------------------------------------------------------------------
 *  Codes emitted by the login/token path (after talking to IdP)
 *  ---------------------------------------------------------------------
 *  invalid_credentials       401  Wrong username or password (LOGIN only)
 *  session_expired           401  Refresh_token or auth code no longer
 *                                 valid — emitted for REFRESH and
 *                                 CALLBACK actions instead of
 *                                 invalid_credentials, because the user
 *                                 did not type anything wrong; their
 *                                 session simply timed out.
 *  account_disabled          403  Account disabled / not fully set up
 *  auth_rate_limited         429  Too many login attempts
 *  auth_service_unavailable  502  IdP unreachable or 5xx
 *  auth_config_error         500  BFF misconfigured (bad client_id, ...)
 *  auth_unexpected_error     502  IdP returned something unexpected
 *  auth_empty_response       502  IdP reply had no JSON body
 *  auth_timeout              504  Watchdog fired, no reply in time
 *
 *  ---------------------------------------------------------------------
 *  Codes emitted by request validation (before talking to IdP)
 *  ---------------------------------------------------------------------
 *  method_not_allowed        405  Request was not POST
 *  missing_body              400  POST without JSON body
 *  missing_params            400  Required body fields absent
 *  redirect_uri_not_allowed  400  redirect_uri fails allowlist check
 *  missing_refresh_token     401  /auth/refresh called without RT cookie
 *  unknown_endpoint          404  URL not one of /auth/{callback,login,refresh,logout}
 *  server_busy               503  Pending queue full
 *
 *  New codes MUST be added here first, then implemented in c_auth_bff.c,
 *  then added to the GUI i18n catalogue.  Never introduce an error_code
 *  that is not listed in this header.
 ****************************************************************************/

/****************************************************************************
 *              Prototypes
 ****************************************************************************/
PUBLIC int register_c_auth_bff(void);

#ifdef __cplusplus
}
#endif
