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

**An IdP success that cannot be read** is answered `502` with a WARNING
*"👤BFF IdP answer refused"*, not an ERROR: it is bad data of a peer. A token
that is not a string, no access_token, or a token with a byte a cookie value
cannot carry (outside RFC 6265 `cookie-octet`: a `;`, a space, a CR LF...,
which in a `Set-Cookie` would add an attribute or a header):
`auth_unexpected_error`. A body that is not a JSON object (empty, a proxy's
HTML page, `[1, 2]`, the JSON string `""`): `auth_empty_response`, whose
`error` says which:

```
{"success": false, "error_code": "auth_empty_response",
 "error": "Authentication service answered success with an empty body"}
```

The BFF reads at most 1 MB of the body of an IdP answer (a token answer is a
few KB). A longer one is not read to the end: the HTTP client logs one
WARNING (*"HTTP body larger than its limit, message refused"*), drops the
connection to the IdP, and the request is answered `502
auth_unexpected_error` (*"Authentication service answered a body longer than
1048576 bytes"*).

**A `429` of the IdP** is answered `429 auth_rate_limited`, with the
`Retry-After` of the IdP when it is delay-seconds of 1 to 10 digits (`120`)
or a real HTTP-date in any of its three forms, passed on as IMF-fixdate
(`Wednesday, 21-Oct-26 07:28:00 GMT` and `Wed Oct 21 07:28:00 2026` become
`Wed, 21 Oct 2026 07:28:00 GMT`); the blanks around it are removed. A date
whose day is not in its month, whose time is past 23:59:60 or whose day-name
is not the one of the date, 11 digits or more, a list (`120, 60`): not passed
on, with a WARNING. The CORS headers of every answer carry
`Access-Control-Expose-Headers: Retry-After`, so a SPA on another origin can
read it:

```
HTTP/1.1 429 Too Many Requests
Access-Control-Allow-Origin: https://<host>
Access-Control-Allow-Credentials: true
Access-Control-Expose-Headers: Retry-After
Retry-After: 120

{"success": false, "error_code": "auth_rate_limited", "error": "Too many login attempts, please try again later"}
```

**A request is sent to the IdP once.** When the IdP closes the connection
before it answers, the request is not sent again on the next connection (a
second password grant, a `refresh_token` used twice): `502
auth_service_unavailable`, *"Authentication service closed the connection
before answering, the request is not sent again"*.

**Answers leave in the order of their requests.** A client may pipeline
requests on one connection (RFC 9112 §9.3.2); an answer the BFF knows at
once (a `400`, a `404`) waits behind a request still at the IdP. More than
`pending_queue_size` such answers waiting drop the connection, with a
WARNING.

**A lifetime of 0** (`"refresh_expires_in": 0`, the offline token of
Keycloak) is a token with no expiry: its cookie has no `Max-Age`, a cookie of
the browser session, and the answer says 0.

**Logout.** `POST /auth/logout` ends the session at the IdP with the
`refresh_token` cookie, then clears both cookies with `Max-Age=0` and
answers `200 {"success": true}`. With no `refresh_token` cookie (a session
opened without one), it does not call the IdP and only clears the cookies,
with the same answer -- if the request carries the `access_token` cookie.
A request with no cookie (no session, or a cross-site form) gets `200
{"success": true}` with no `Set-Cookie`: there is nothing to end.

**The Origin of a request.** A login, a callback, a refresh, a logout with a
cookie and a `/auth/token` are served only to the `Origin` of the SPA:
exactly `allowed_origin` when it is set; with `allowed_origin` empty, the
BFF's own origin (`http://` or `https://` and the `Host` of the request; a
proxy in front must pass the `Host` on), and no `/auth/token`. Another
origin, a sibling subdomain (it sends the `SameSite=Strict` cookies too), no
`Origin`, `null`, or the allowed one with more after it: `403
origin_not_allowed`, with no call to the IdP and no `Set-Cookie`. The
`error` names the `Origin` received and the one allowed.

**In development behind a proxy** (the vite dev server of a SPA, which
rewrites only the `Host`), the `Origin` is the dev server's: give the dev
BFF that origin, as `batches/localhost/auth_bff.1801.json` does
(`"allowed_origin": "http://localhost:5173"`), or make the proxy send the
allowed one (`headers: {origin: "https://<host>"}` in its `/auth` entry of
`vite.config.js`). With the production `allowed_origin` every request of
the dev server is `403`, a logout included, and a refused logout leaves the
`HttpOnly` cookies: a reload restores the session.

**Token cookies the BFF did not write.** A browser keeps a cookie whose name
and value take at most 4096 bytes, and the BFF writes no bigger one (an
access_token of at most 4084 bytes, a refresh_token of at most 4083). A
`refresh_token` cookie (`/auth/refresh`, `/auth/logout`) or an
`access_token` cookie (`/auth/token`) longer than that is none the BFF
wrote: `400 token_too_large`, *"refresh_token cookie of 4084 bytes: the BFF
writes none longer than 4083 (a cookie name and value take at most 4096
bytes)"*. A logout that clears the cookies answers:

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
the cookies never carry `Domain`. `allowed_redirect_uri` is the
`redirect_uri` a `/auth/callback` may have: exactly it, or, when it ends
with `/` after a scheme and a host (`https://<host>/`,
`https://<host>/app/`), one that starts with it and whose rest is a path
and query in it (visible ASCII, no `\`, no `#`, no `.` or `..` segment,
nor with `%2e`): `https://<host>.evil.com/` and `https://<host>@evil/`
never pass `https://<host>/`. Empty: not checked. The full attribute list
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
