# auth_bff

**OAuth2 / OpenID Connect Backend-for-Frontend** yuno. Sits between a browser-based single-page app and an upstream identity provider (Keycloak, Google, …), implementing the authorization-code flow **with PKCE** so the SPA never handles client secrets or tokens directly.

## What it does

- Initiates the OAuth2 authorization-code flow with PKCE
- Exchanges the code for tokens server-side
- Stores tokens in **httpOnly, SameSite cookies** (never exposed to JavaScript)
- Proxies API calls to downstream services, injecting the access token
- Refreshes expired tokens transparently
- Handles logout (local and RP-initiated)

## Security hardening

Implements the SEC-04 / SEC-06 / SEC-07 / SEC-09 recommendations — strict `cookie_domain`-vs-`Host` matching (mismatches are rejected before cookies are set), anti-CSRF state/nonce, and XSS-safe session handling.

## Configuration

Main config lives in the yuno's `kw` section. Key attributes: upstream provider URLs, client id, redirect URI, cookie domain, scope set. See the source in `src/` for the full attribute schema.
