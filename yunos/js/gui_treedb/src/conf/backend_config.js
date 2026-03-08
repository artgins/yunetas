/***********************************************************************
 *          Configuration of supported urls, BFF endpoints,
 *          Keycloak configs, and OAuth2 login providers.
 *
 *  IMPORTANT — keep the three sections below in sync:
 *
 *  1. backend_urls   — WebSocket endpoint per hostname
 *  2. bff_urls       — BFF (Backend For Frontend) HTTP base URL per hostname
 *  3. keycloak_configs — Keycloak realm/client per hostname
 *
 *  Every origin added to bff_urls must also appear in the CSP
 *  connect-src directive in index.html, and vice-versa.
 *
 *  BFF URL notes:
 *  - The BFF runs on port 1801 (HTTP/REST) alongside the WebSocket
 *    on port 1800.
 *  - httpOnly cookies set by the BFF are scoped to the hostname
 *    (no port), so they are automatically sent with WebSocket upgrades
 *    to wss://<hostname>:1800 as well.
 *  - All BFF endpoints require HTTPS in production.
 *
 *  Copyright (c) 2025, ArtGins.
 *  All Rights Reserved.
 ***********************************************************************/

/*
 *  WebSocket backend endpoints.
 */
const backend_urls = {
    "localhost":            "wss://localhost:1800",
    "treedb.yunetas.com":   "wss://treedb.yunetas.com:1800"
};

/*
 *  BFF (Backend For Frontend) base URLs.
 *  Endpoints provided:
 *    POST /auth/login      — Direct Access Grant (username/password → tokens)
 *    POST /auth/callback   — exchange PKCE code for tokens (sets httpOnly cookies)
 *    POST /auth/refresh    — refresh tokens via httpOnly cookie
 *    POST /auth/logout     — revoke tokens and clear cookies
 *
 *  SEC-06: the BFF handles all token operations server-side so that
 *  raw JWTs never appear in JavaScript.
 */
const bff_urls = {
    "localhost":            "",    // same-origin; Vite proxies /auth/* to https://localhost:1801
    "treedb.yunetas.com":   "https://treedb.yunetas.com:1801"
};

/*
 *  Keycloak realm / client configurations.
 *  Used by c_login.js to build the PKCE Authorization Code URL.
 *
 *  Required Keycloak client settings:
 *  - Direct Access Grants (ROPC):         ENABLED
 *  - Valid Redirect URIs:                 the app URL + "/*"
 *  - Web Origins:                         the app origin
 *
 *  To enable social logins, configure Identity Providers in the
 *  Keycloak admin console (Identity Providers → Add provider → Google /
 *  GitHub / …) and use the corresponding kc_idp_hint values below.
 */
const keycloak_configs = {
    "localhost": {
        "realm": "estadodelaire.com",
        "auth-server-url": "https://auth.artgins.com",
        "ssl-required": "external",
        "resource": "gui_treedb",
        "public-client": true,
        "confidential-port": 0
    },
    "treedb.yunetas.com": {
        "realm": "estadodelaire.com",
        "auth-server-url": "https://auth.artgins.com",
        "ssl-required": "external",
        "resource": "gui_treedb",
        "public-client": true,
        "confidential-port": 0
    }
};

/*
 *  Login provider buttons shown in the UI.
 *
 *  id:           unique identifier
 *  label:        button text
 *  kc_idp_hint:  null  → Keycloak-hosted login form (local accounts)
 *                str   → direct IDP redirect (must be configured in Keycloak)
 *
 *  To add Google login:
 *    1. In Keycloak admin: Identity Providers → Add → Google
 *    2. Set Alias to "google"
 *    3. Add { id:"google", label:"Sign in with Google", kc_idp_hint:"google" }
 *
 *  To add GitHub login:
 *    1. In Keycloak admin: Identity Providers → Add → GitHub
 *    2. Set Alias to "github"
 *    3. Add { id:"github", label:"Sign in with GitHub", kc_idp_hint:"github" }
 */
const oauth_providers = [
    {
        id:             "local",
        label:          "Sign in",
        kc_idp_hint:    null        // Shows Keycloak login form
    }
    // Uncomment and configure after setting up Keycloak IDPs:
    // {
    //     id:             "google",
    //     label:          "Sign in with Google",
    //     kc_idp_hint:    "google"
    // },
    // {
    //     id:             "github",
    //     label:          "Sign in with GitHub",
    //     kc_idp_hint:    "github"
    // }
];

export {
    backend_urls,
    bff_urls,
    keycloak_configs,
    oauth_providers,
};
