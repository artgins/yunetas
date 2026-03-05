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
 *  Copyright (c) 2025, ArtGins.
 *  All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 *              Prototypes
 ****************************************************************************/
PUBLIC int register_c_auth_bff(void);

#ifdef __cplusplus
}
#endif
