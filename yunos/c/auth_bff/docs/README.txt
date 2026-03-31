auth_bff — Authentication Backend-For-Frontend
===============================================

A Yuneta yuno that deploys the kernel C_AUTH_BFF GClass as a standalone
HTTPS service.  It mediates OAuth2 flows between browser SPAs and
Keycloak, storing tokens as httpOnly cookies so they never reach JavaScript.

Architecture
------------

    Browser SPA ──POST /auth/*──> auth_bff (port 1801) ──> Keycloak
                                      │
                                      ├── Sets httpOnly cookies
                                      │   (access_token, refresh_token)
                                      │
    Browser SPA ──WSS upgrade──> yuno backend (port 1800)
                                      │
                                      └── Reads JWT from Cookie header

Endpoints
---------

    POST /auth/login      Username/password → Direct Access Grant
    POST /auth/callback   PKCE code exchange (authorization_code grant)
    POST /auth/refresh    Refresh tokens via httpOnly cookie
    POST /auth/logout     Revoke tokens, clear cookies

Deployment
----------

One auth_bff instance per realm.  Managed by yuneta_agent like any
other yuno:

    yuno_agent deploy auth_bff realm=demo.estadodelaire.com

Configuration is provided via the yuno's JSON config at deployment
time (keycloak_url, realm, client_id, cookie_domain, etc.).

See c_auth_bff.h in kernel/c/root-linux/src/ for the full attribute list.
