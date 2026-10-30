# auth_bff — Authentication Backend-For-Frontend

**OAuth2 / OpenID Connect Backend-for-Frontend** yuno. It sits between a
browser single-page app and an identity provider (IdP: Keycloak, or any OIDC
provider), so the SPA never handles a client secret or a token.

It runs the kernel [`C_AUTH_BFF`](../../../kernel/c/root-linux/src/c_auth_bff.c)
gclass as an HTTPS service, one `C_AUTH_BFF` under each `C_CHANNEL` of its
server. It talks OAuth2 to the IdP and keeps the tokens in `HttpOnly` cookies,
which JavaScript cannot read.

## What it does

- Exchanges a username and password for tokens (`/auth/login`,
  `grant_type=password`): the flow of the in-repo SPAs `gui_agent` and
  `gui_treedb`.
- Exchanges a PKCE authorization code for tokens (`/auth/callback`,
  `grant_type=authorization_code`), for an SPA that uses the IdP's own login
  page.
- Stores the tokens in `HttpOnly; Secure; SameSite=Strict` cookies. They
  carry `Domain` only when `cookie_domain` is set and matches the request's
  host (see *Configuration*).
- Refreshes the tokens, and answers who the user is (`/auth/refresh`).
- Logs out at the IdP and clears the cookies (`/auth/logout`).
- Optionally gives the access token to one pinned origin (`/auth/token`), for
  an SPA that must forward it to backends on other hosts.

## Architecture

```
Browser SPA ──POST /auth/*──> auth_bff (https://<host>:<port>) ──> IdP
                                  │
                                  └── Sets HttpOnly cookies
                                      (access_token, refresh_token)

Browser SPA ──WSS upgrade──> backend yuno (any port of the same host)
                                  │
                                  └── C_IEVENT_SRV reads the JWT from the
                                      Cookie header, C_AUTHZ verifies it
```

A cookie is not scoped by port, so a cookie set by the BFF travels with the
WebSocket upgrade to any port of the same host name.

The port is the yuno's configuration. The in-repo SPAs expect their BFF on
their own host: `gui_agent` on 1806 (agents plane) and 1807 (agent22 plane),
`gui_treedb` on 1808. The localhost example
(`batches/localhost/auth_bff.1801.json`) listens on 1801.

## Endpoints

| Endpoint | Purpose |
|----------|---------|
| `POST /auth/login`    | `{username, password}` → password grant at the IdP; sets the cookies |
| `POST /auth/callback` | `{code, code_verifier, redirect_uri}` → PKCE code exchange; sets the cookies |
| `POST /auth/refresh`  | Reads the `refresh_token` cookie, gets new tokens, answers `username` and `email` too |
| `POST /auth/logout`   | Ends the session at the IdP, clears the cookies (`Max-Age=0`); with no `refresh_token` cookie it only clears them |
| `POST /auth/token`    | **Opt-in** (`expose_access_token`): returns the access token to the origin `allowed_origin`, else `403 origin_not_allowed`; `404 unknown_endpoint` when off |
| `OPTIONS *`           | CORS preflight, only for `allowed_origin` |

A successful login, callback or refresh answers
`{"success": true, "username": "...", "email": "...", "expires_in": N,
"refresh_expires_in": N}`, with no token in the body. Every error carries a
stable `error_code` (the catalog is in `c_auth_bff.h`), which the SPA uses as
its i18n key.

**When the IdP answers with no `refresh_token`.** A login or callback opens
a session that cannot be refreshed: the BFF clears the `refresh_token`
cookie, answers with no `refresh_expires_in`, and logs one WARNING per
process. The session ends with its access token. A refresh answered with no
new `refresh_token` keeps the browser's cookie (no `Set-Cookie` for it) and
also answers with no `refresh_expires_in`:

```
HTTP/1.1 200 OK
Set-Cookie: access_token=<jwt>; Max-Age=300; Path=/; HttpOnly; Secure; SameSite=Strict
Set-Cookie: refresh_token=; Max-Age=0; Path=/; HttpOnly; Secure; SameSite=Strict   (login only)

{"success": true, "username": "john", "email": "john@example.com", "expires_in": 300}
```

**A lifetime of 0** (`"refresh_expires_in": 0`, the offline token of
Keycloak) is a token with no expiry: its cookie has no `Max-Age`, a cookie of
the browser session, and the answer says 0.

**Logout.** `POST /auth/logout` ends the session at the IdP with the
`refresh_token` cookie, then clears both cookies with `Max-Age=0` and
answers `200 {"success": true}`. With no `refresh_token` cookie (a session
opened without one), it does not call the IdP and only clears the cookies,
with the same answer:

```
HTTP/1.1 200 OK
Set-Cookie: access_token=; Max-Age=0; Path=/; HttpOnly; Secure; SameSite=Strict
Set-Cookie: refresh_token=; Max-Age=0; Path=/; HttpOnly; Secure; SameSite=Strict

{"success": true}
```

## Configuration

The IdP is configured with `issuer` (discovery through
`/.well-known/openid-configuration`), or with the explicit `token_endpoint` +
`end_session_endpoint`:

```json
"issuer":               "https://auth.example.com/realms/<realm>/",
"client_id":            "<client_id>",
"client_secret":        "",
"cookie_domain":        "<host>",
"allowed_origin":       "https://<host>",
"allowed_redirect_uri": "https://<host>/"
```

`cookie_domain` is checked against the request's `Host` header. When the host
is that domain or one of its subdomains, the cookies carry `Domain=<cookie_domain>`
and reach the subdomains too. When it does not match, the cookies are set
without `Domain` (bound to the exact host) and the BFF logs a WARNING. Empty:
the cookies never carry `Domain`. `allowed_redirect_uri` is the prefix
a `/auth/callback` `redirect_uri` must start with (empty: not checked). The full attribute list
(`crypto`, `pending_queue_size`, `idp_timeout_ms`, `idp_connect_timeout_ms`,
`expose_access_token`) is the `attrs_table` of `c_auth_bff.c`.

The IdP client must allow the password grant ("Direct Access Grants" in
Keycloak) for `/auth/login`, and the standard flow with PKCE S256 for
`/auth/callback`.

## Deployment

One `auth_bff` per SPA plane. Its runtime configuration lives in
`batches/<host>/auth_bff.<port>.json`.

## Known issues

- **HTTP_CL chain leak under rapid disconnect**: the outbound
  `C_PROT_HTTP_CL` chain to the IdP is not always released cleanly when the
  browser disconnects during a call to the IdP. Watch the process FD count
  under unusual load.
- **No real-IdP test**: `tests/c/c_auth_bff/` runs against
  `c_mock_keycloak.c` only, so a regression against a real Keycloak is found
  by hand, on a staging node, before each release.

## See also

- [`YUNO_AUTH.md`](../yuno_agent/YUNO_AUTH.md) §2 — the full contract of the
  BFF, and §4.5 for the per-command authz check (gated by
  `enable_command_authz`, off by default).
- [OAuth2 / PKCE / BFF Pattern](../../../docs/doc.yuneta.io/guide/guide_oauth2_pkce_bff.md)
  — the two login flows step by step, the refresh and the logout.
