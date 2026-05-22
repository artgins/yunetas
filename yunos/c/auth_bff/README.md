# auth_bff — Authentication Backend-For-Frontend

**OAuth2 / OpenID Connect Backend-for-Frontend** yuno. Sits between a browser-based single-page app and an upstream identity provider (Keycloak, Google, …), implementing the authorization-code flow **with PKCE** so the SPA never handles client secrets or tokens directly.

A Yuneta yuno that deploys the kernel `C_AUTH_BFF` GClass as a standalone HTTPS service. It mediates OAuth2 flows between browser SPAs and Keycloak, storing tokens as httpOnly cookies so they never reach JavaScript.

## What it does

- Initiates the OAuth2 authorization-code flow with PKCE
- Exchanges the code for tokens server-side
- Stores tokens in **httpOnly, SameSite cookies** (never exposed to JavaScript)
- Proxies API calls to downstream services, injecting the access token
- Refreshes expired tokens transparently
- Handles logout (local and RP-initiated)

## Architecture

```
Browser SPA ──POST /auth/*──> auth_bff (default port 1802) ──> Keycloak
                                  │
                                  ├── Sets httpOnly cookies
                                  │   (access_token, refresh_token)
                                  │
Browser SPA ──WSS upgrade──> yuno backend (configurable port,
                                  │          typically 1600, 1800, …)
                                  │
                                  └── Reads JWT from Cookie header
```

Cookies are scoped by Domain (no port), so a cookie set by the BFF is automatically sent with the WebSocket upgrade to any port on the same hostname.

## Endpoints

| Endpoint | Purpose |
|----------|---------|
| `POST /auth/login`    | Username/password → Direct Access Grant |
| `POST /auth/callback` | PKCE code exchange (authorization_code grant) |
| `POST /auth/refresh`  | Refresh tokens via httpOnly cookie |
| `POST /auth/logout`   | Revoke tokens, clear cookies |

## Security hardening

Implements the SEC-04 / SEC-06 / SEC-07 / SEC-09 recommendations — strict `cookie_domain`-vs-`Host` matching (mismatches are rejected before cookies are set), anti-CSRF state/nonce, and XSS-safe session handling.

## Configuration

Main config lives in the yuno's `kw` section. Key attributes:
upstream provider URLs, client id, redirect URI, cookie domain,
scope set.

See `c_auth_bff.h` in `kernel/c/root-linux/src/` for the full
attribute list, or the source in `src/` for the yuno-level schema.

**OIDC config — current vs deprecated shape.** As of the 2026-04-30
migration, configure the IdP via `issuer` (or explicit endpoints):

```json
"issuer":               "https://auth.example.com/realms/<realm>/",
"client_id":            "<client_id>",
"client_secret":        "",
"cookie_domain":        "<host>",
"allowed_redirect_uri": "https://<host>/auth/callback"
```

The pair `idp_url` + `realm` is `SDF_DEPRECATED` and only kept as a
legacy fallback (`c_auth_bff.c:181-192, 358`). New deployments must
not use it.

## Deployment

One `auth_bff` instance per realm. Per-host runtime configuration
lives in `batches/<host>/auth_bff.<port>.json` (e.g.
`batches/localhost/auth_bff.1801.json`). See
[`yunos/c/yuno_agent/AUTH.md`](../yuno_agent/AUTH.md) §7 for the
per-project Keycloak realm convention and §9.1 for the full
bootstrap recipe.

## Known issues

Tracked at `~/.claude/.../memory/project_auth_bff_pending_bugs`:

- **HTTP_CL chain leak under rapid disconnect** — the outbound
  `C_PROT_HTTP_CL` chain to the IdP isn't always reclaimed cleanly
  when the browser disconnects mid-`/token`. Watch process FD count
  under unusual load.
- **No real-IdP smoke tests** — `tests/c/c_auth_bff/` runs against
  `c_mock_keycloak.c` only. Live Keycloak regressions go uncaught
  in CI; manual staging smoke test is mandatory before each release.

See [`AUTH.md`](../yuno_agent/AUTH.md) for the full authn / authz
walkthrough, including the **critical caveat** that the per-command
authz check (`gobj_user_has_authz` for commands) is currently
**commented out** in `kernel/c/gobj-c/src/command_parser.c:73-113`.
