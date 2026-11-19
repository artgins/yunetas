# auth_bff: Authentication Backend-For-Frontend

auth_bff is an OAuth2 / OpenID Connect Backend-for-Frontend (BFF) yuno. It
sits between a single-page app (SPA) in the browser and an identity provider
(IdP: Keycloak, or any OIDC provider). Thus the SPA never handles a client
secret or a token.

It runs the kernel [`C_AUTH_BFF`](../../../kernel/c/root-linux/src/c_auth_bff.c)
gclass as an HTTPS service, with one `C_AUTH_BFF` under each `C_CHANNEL` of its
server. It talks OAuth2 to the IdP and keeps the tokens in `HttpOnly` cookies,
which JavaScript cannot read.

To use it, create a client in the IdP and write the configuration of the yuno.
*Configuration* and *Deployment* below give the details.

## What it does

- Exchanges a username and password for tokens (`/auth/login`,
  `grant_type=password`): the flow of the in-repo SPAs `gui_agent` and
  `gui_treedb`.
- Exchanges a PKCE authorization code for tokens (`/auth/callback`,
  `grant_type=authorization_code`), for an SPA that uses the login page of
  the IdP itself.
- Stores the tokens in `HttpOnly; Secure; SameSite=Strict` cookies. They
  carry `Domain` only when `cookie_domain` is set and matches the host of the
  request (see *Configuration*).
- Refreshes the tokens, and answers who the user is (`/auth/refresh`).
- Logs out at the IdP and clears the cookies (`/auth/logout`).
- As an option, gives the access token to one pinned origin (`/auth/token`),
  for an SPA that must forward it to backends on other hosts.

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

The configuration of the yuno sets the port. The in-repo SPAs expect their BFF on
their own host: `gui_agent` on 1806 (agents plane) and 1807 (agent22 plane),
`gui_treedb` on 1808. The localhost example
(`batches/localhost/auth_bff.1801.json`) listens on 1801.

## Endpoints

| Endpoint | Purpose |
|----------|---------|
| `POST /auth/login`    | `{username, password}` → password grant at the IdP. Sets the cookies |
| `POST /auth/callback` | `{code, code_verifier, redirect_uri}` → PKCE code exchange. Sets the cookies |
| `POST /auth/refresh`  | Reads the `refresh_token` cookie, gets new tokens, answers `username` and `email` too |
| `POST /auth/logout`   | Ends the session at the IdP and clears the cookies (`Max-Age=0`). With no `refresh_token` cookie, it only clears them |
| `POST /auth/token`    | Opt-in (`expose_access_token`). Returns the access token to the origin `allowed_origin`, else `403 origin_not_allowed`. When off, `404 unknown_endpoint` |
| `OPTIONS *`           | CORS preflight, only for `allowed_origin` |

A successful login, callback or refresh answers
`{"success": true, "username": "...", "email": "...", "expires_in": N,
"refresh_expires_in": N}`, with no token in the body. Every error carries a
stable `error_code` (the catalog is in `c_auth_bff.h`). The SPA uses it as
its i18n key.

If the IdP answers with no `refresh_token`, a login or callback opens a
session that cannot be refreshed. The BFF clears the `refresh_token` cookie,
answers with no `refresh_expires_in`, and logs one WARNING per process. The
session ends with its access token. If a refresh gets no new
`refresh_token`, the BFF keeps the cookie of the browser (no `Set-Cookie` for
it). It also answers with no `refresh_expires_in`:

```
HTTP/1.1 200 OK
Set-Cookie: access_token=<jwt>; Max-Age=300; Path=/; HttpOnly; Secure; SameSite=Strict
Set-Cookie: refresh_token=; Max-Age=0; Path=/; HttpOnly; Secure; SameSite=Strict   (login only)

{"success": true, "username": "john", "email": "john@example.com", "expires_in": 300}
```

If the IdP answers success but the BFF cannot read the answer, the BFF answers
`502` and logs a WARNING *"👤BFF IdP answer refused"*. It is not an ERROR,
because it is bad data of a peer. The `error_code` tells the fault:

- `auth_unexpected_error`: a token that is not a string, no access_token, or
  a token with a byte that a cookie value cannot carry. These bytes are
  outside RFC 6265 `cookie-octet`: a `;`, a space, a CR LF... In a
  `Set-Cookie`, such a byte can add an attribute or a header.
- `auth_empty_response`: a body that is not a JSON object (empty, the HTML
  page of a proxy, `[1, 2]`, the JSON string `""`). Its `error` says which:

```
{"success": false, "error_code": "auth_empty_response",
 "error": "Authentication service answered success with an empty body"}
```

The BFF reads at most 1 MB of the body of an IdP answer. A token answer is a
few KB. The BFF does not read a longer body to the end. The HTTP client logs
one WARNING (*"HTTP body larger than its limit, message refused"*) and drops
the connection to the IdP. The BFF answers the request with `502
auth_unexpected_error` (*"Authentication service answered a body longer than
1048576 bytes"*). The same occurs for headers over the 64 KB of the HTTP
client, and for an answer that breaks the HTTP protocol after it began. The
`error` names each case. If such an answer comes to a logout, the BFF still
clears the cookies, and logs a WARNING.

If the IdP answers `429`, the BFF answers `429 auth_rate_limited`. It
forwards the `Retry-After` of the IdP when that value is delay-seconds of 1
to 10 digits (`120`), or a real HTTP-date in any of its three forms. The BFF
forwards a date as IMF-fixdate: `Wednesday, 21-Oct-26 07:28:00 GMT` and
`Wed Oct 21 07:28:00 2026` become `Wed, 21 Oct 2026 07:28:00 GMT`. It removes
the blanks around the value. The BFF does not forward these values, and logs
a WARNING: a date whose day is not in its month, a time past 23:59:60, a
day-name that is not the one of the date, 11 digits or more, or a list
(`120, 60`). The CORS headers of every answer carry
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

The BFF sends a request to the IdP once. If the IdP closes the connection
before it answers, the BFF does not send the request again on the next
connection. A second send can be a second password grant, or a
`refresh_token` used twice. The BFF answers `502
auth_service_unavailable`, *"Authentication service closed the connection
before answering, the request is not sent again"*.

Answers leave in the order of their requests. A client can pipeline
requests on one connection (RFC 9112 §9.3.2). An answer that the BFF knows at
once (a `400`, a `404`) waits behind a request that is still at the IdP. If
more than `pending_queue_size` such answers wait, the BFF drops the
connection and logs a WARNING.

A lifetime of 0 (`"refresh_expires_in": 0`, the offline token of
Keycloak) is a token with no expiry. Its cookie has no `Max-Age`, so it is a
cookie of the browser session. The answer says 0.

`POST /auth/logout` ends the session at the IdP with the `refresh_token`
cookie. Then it clears both cookies with `Max-Age=0` and answers
`200 {"success": true}`. A request can carry the `access_token` cookie but no
`refresh_token` cookie (a session opened without one). For such a request,
the BFF does not call the IdP. It only clears the cookies, with the same
answer. A request with no cookie (no session, or a cross-site form) gets `200
{"success": true}` with no `Set-Cookie`, because there is nothing to end.

The BFF serves a login, a callback, a refresh, a logout with a cookie, and a
`/auth/token` only to the `Origin` of the SPA:

- If `allowed_origin` is set, the `Origin` must be exactly `allowed_origin`.
- If `allowed_origin` is empty, the `Origin` must be the origin of the BFF
  itself: `http://` or `https://` and the `Host` of the request. A proxy in
  front must forward the `Host`. In this case, the BFF serves no
  `/auth/token`.

These origins get `403
origin_not_allowed`, with no call to the IdP and no `Set-Cookie`: another
origin, a sibling subdomain (it sends the `SameSite=Strict` cookies too), no
`Origin`, `null`, or the allowed one with more after it. The `error` names
the `Origin` received and the one allowed.

In development, a proxy can be in front of the BFF, for example the vite dev
server of a SPA, which rewrites only the `Host`. Then the `Origin` is the
origin of the dev server. Give the dev BFF that origin, as
`batches/localhost/auth_bff.1801.json` does
(`"allowed_origin": "http://localhost:5173"`). Or make the proxy send the
allowed origin (`headers: {origin: "https://<host>"}` in its `/auth` entry of
`vite.config.js`). With the production `allowed_origin`, every request of the
dev server gets `403`, a logout too. A refused logout leaves the `HttpOnly`
cookies, so a reload restores the session.

The BFF refuses token cookies that it did not write. A browser keeps a cookie
whose name and value take at most 4096 bytes, and the BFF writes no bigger
cookie. Its access_token is at most 4084 bytes, and its refresh_token at most
4083. A longer `refresh_token` cookie (`/auth/refresh`, `/auth/logout`) or
`access_token` cookie (`/auth/token`) is not one that the BFF wrote. The BFF
answers `400 token_too_large`, *"refresh_token cookie of 4084 bytes: the BFF
writes none longer than 4083 (a cookie name and value take at most 4096
bytes)"*. A logout that clears the cookies answers:

```
HTTP/1.1 200 OK
Set-Cookie: access_token=; Max-Age=0; Path=/; HttpOnly; Secure; SameSite=Strict
Set-Cookie: refresh_token=; Max-Age=0; Path=/; HttpOnly; Secure; SameSite=Strict

{"success": true}
```

## Configuration

Configure the IdP with `issuer` (discovery through
`/.well-known/openid-configuration`), or with the explicit `token_endpoint`
and `end_session_endpoint`:

```json
"issuer":               "https://auth.example.com/realms/<realm>/",
"client_id":            "<client_id>",
"client_secret":        "",
"cookie_domain":        "<host>",
"allowed_origin":       "https://<host>",
"allowed_redirect_uri": "https://<host>/"
```

The BFF compares `cookie_domain` with the `Host` header of the request. If the
host is that domain or one of its subdomains, the cookies carry
`Domain=<cookie_domain>` and reach the subdomains too. If the host does not
match, the cookies are set without `Domain` (bound to the exact host), and
the BFF logs a WARNING. If `cookie_domain` is empty, the cookies never carry
`Domain`.

`allowed_redirect_uri` sets the `redirect_uri` that a `/auth/callback` can
have. The `redirect_uri` must be exactly `allowed_redirect_uri`, with one
exception. If `allowed_redirect_uri` ends with `/` after a scheme and a host
(`https://<host>/`, `https://<host>/app/`), the `redirect_uri` can start with
it. The rest must then be a path and query in it: visible ASCII, no `\`, no
`#`, no `.` or `..` segment, nor with `%2e`. Thus `https://<host>.evil.com/`
and `https://<host>@evil/` never pass `https://<host>/`. If
`allowed_redirect_uri` is empty, the BFF does not examine the `redirect_uri`.

The `attrs_table` of `c_auth_bff.c` gives the full list of attributes
(`crypto`, `pending_queue_size`, `idp_timeout_ms`, `idp_connect_timeout_ms`,
`expose_access_token`, `idp_failover_connect_ms`, `idp_failover_response_ms`,
`idp_down_retry_ms`).

If two or more IdPs serve the realm (the realm imported with its keys), name
them all in `issuers`, in order of preference. `issuers` replaces `issuer`:

```json
"issuers": [
    "https://auth.example.com/realms/<realm>/",
    "https://auth2.example.com/realms/<realm>/"
],
```

A login goes to the first IdP that answers. A refresh and a logout go to the
IdP of their session (the `iss` of the refresh token), because the other IdP
refuses that token. For more, read "Failover between IdPs" in the
`C_AUTH_BFF` reference, and `YUNO_AUTH.md` §2.7.

The IdP client must allow the password grant ("Direct Access Grants" in
Keycloak) for `/auth/login`, and the standard flow with PKCE S256 for
`/auth/callback`.

## Deployment

Run one `auth_bff` for each SPA plane. Its runtime configuration is in
`batches/<host>/auth_bff.<port>.json`.

## Known issues

- HTTP_CL chain leak under rapid disconnect. If the browser disconnects
  during a call to the IdP, the outbound `C_PROT_HTTP_CL` chain to the IdP is
  not always released cleanly. Under unusual load, watch the FD count of the
  process.
- No real-IdP test. `tests/c/c_auth_bff/` runs only against
  `c_mock_keycloak.c`. Thus a regression against a real Keycloak is found by
  hand, on a staging node, before each release.

## See also

- [`YUNO_AUTH.md`](../yuno_agent/YUNO_AUTH.md) §2: the full contract of the
  BFF, and §4.5 for the per-command authz check (gated by
  `enable_command_authz`, off by default).
- [OAuth2 / PKCE / BFF Pattern](../../../docs/doc.yuneta.io/guide/guide_oauth2_pkce_bff.md):
  the two login flows step by step, the refresh and the logout.
