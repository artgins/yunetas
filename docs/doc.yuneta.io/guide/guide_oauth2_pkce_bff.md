# OAuth2 / PKCE / BFF Pattern in Yunetas

## Overview

Yunetas authenticates browser SPAs through a **Backend-For-Frontend (BFF)**:
the `auth_bff` yuno, which runs the [`C_AUTH_BFF`](#gclass-c-auth-bff) gclass.
The browser talks to the BFF only. The BFF is the only component that talks
OAuth2 to the identity provider (IdP). The key security property: **the tokens
live in `HttpOnly` cookies, and JavaScript never reads them** (the one opt-in
exception is [`/auth/token`](#bff-auth-token-exception)).

The BFF gives the browser two ways in:

| Endpoint | OAuth2 grant | Who uses it |
|---|---|---|
| `POST /auth/login` | Resource Owner Password Credentials (`grant_type=password`) | The in-repo SPAs `gui_agent` and `gui_treedb`: their own login form posts `{username, password}` |
| `POST /auth/callback` | Authorization Code + **PKCE** (`grant_type=authorization_code`) | An SPA that sends the user to the IdP's own login page. No in-repo SPA uses it today |

After the login the two are the same: cookies, refresh, logout, and the
cookie on the WebSocket upgrade.

The system is **identity-provider agnostic**. Keycloak is the IdP of every
deployment so far, but the BFF code names everything as *IdP* (`idp_calls`,
`idp_timeouts`, `idp_timeout_ms`). Any OIDC-compliant provider works. The full
server-side contract is [`YUNO_AUTH.md` §2](../../../yunos/c/yuno_agent/YUNO_AUTH.md).

---

## Components Involved

| Layer | Component | File | Role |
|-------|-----------|------|------|
| **Frontend (JS)** | `C_AGENT_LOGIN`, `C_TREEDB_LOGIN` | [`yunos/js/gui_agent/src/c_agent_login.js`](https://github.com/artgins/yunos-js/blob/main/gui_agent/src/c_agent_login.js), [`yunos/js/gui_treedb/src/c_login.js`](https://github.com/artgins/yunos-js/blob/main/gui_treedb/src/c_login.js) | Post the login form to the BFF, restore the session on load, arm the token refresh, log out |
| **Config (JS)** | `deploy_info()` | `yunos/js/<spa>/src/conf/deploy.js` | Derives the BFF URL from the serving host name (`https://<host>:<bff port>`). No per-host table |
| **BFF Server (C)** | [`C_AUTH_BFF`](#gclass-c-auth-bff) in the `auth_bff` yuno | [`kernel/c/root-linux/src/c_auth_bff.c`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/c_auth_bff.c) | HTTPS server on the port of its configuration. Exchanges credentials or codes for tokens, sets the cookies |
| **WebSocket Bridge** | [`C_WEBSOCKET`](#gclass-c-websocket) | [`kernel/c/root-linux/src/c_websocket.c`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/c_websocket.c) | Captures `Cookie` header from HTTP Upgrade request |
| **Gatekeeper** | [`C_IEVENT_SRV`](#gclass-c-ievent-srv) | [`kernel/c/root-linux/src/c_ievent_srv.c`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/c_ievent_srv.c) | Extracts `access_token` from the cookie (or the identity card's `jwt`), injects it into the IDENTITY_CARD |
| **Auth Manager** | [`C_AUTHZ`](#gclass-c-authz) | [`kernel/c/root-linux/src/c_authz.c`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/c_authz.c) | Validates JWT signature (JWKS), checks expiry/issuer/claims, manages roles |
| **JWT Library** | `libjwt` | `kernel/c/libjwt/src/` | Cryptographic JWT verification (RS256, ES256, EdDSA and more.) |

The BFF port is not fixed: it is the `url` of the `auth_bff`'s configuration
(`auth_bff.<port>.json`). The in-repo SPAs expect it on their own host:

| SPA | BFF |
|---|---|
| `gui_agent` | `https://<host>:1806` (agents plane), `https://<host>:1807` (agent22 plane) |
| `gui_treedb` | `https://<host>:1808` |

On `localhost` the `gui_treedb` dev server proxies `/auth/*` to
`https://localhost:1808` (`gui_treedb/vite.config.js`), so the BFF base is
same-origin there and the local BFF must listen on 1808. The localhost example
of the yuno (`yunos/c/auth_bff/batches/localhost/auth_bff.1801.json`) listens
on 1801. To use it with the dev server, set its `__bff_secure_url__` to
`https://0.0.0.0:1808` before you load the batch:

```json
"__bff_secure_url__": "https://0.0.0.0:1808"
```

---

## The Three Key Concepts

### 1. BFF (Backend-For-Frontend)

The BFF is a thin server-side component that acts as a secure intermediary
between the browser and the IdP. The browser never talks to the IdP's token
endpoint. The BFF:

- Exchanges the credentials (or the authorization code) for tokens, server to
  server
- Stores the tokens in `HttpOnly` cookies (invisible to JavaScript)
- Handles token refresh and logout

### 2. The password grant (`/auth/login`)

The SPA shows its own login form and posts the username and password to the
BFF, over HTTPS, and nowhere else. The BFF sends them to the IdP's token
endpoint with `grant_type=password`. The IdP client must allow it ("Direct
Access Grants" in Keycloak), or every login is refused. The SPA never keeps
the password, and never receives a token.

### 3. PKCE (Proof Key for Code Exchange) — [RFC 7636](https://datatracker.ietf.org/doc/html/rfc7636)

For the authorization-code flow (`/auth/callback`), PKCE prevents
authorization code interception attacks. Before redirecting to the IdP, the
browser:

1. Generates a random `code_verifier` (32 bytes, base64url-encoded)
2. Computes `code_challenge = base64url(SHA-256(code_verifier))`
3. Sends the `code_challenge` to the IdP with the authorization request
4. Later, the BFF sends the original `code_verifier` when exchanging the code
5. The IdP verifies that `SHA-256(code_verifier) == code_challenge`

An attacker who intercepts the authorization code cannot use it because they
do not have the `code_verifier`. The `state` and `nonce` are the SPA's job:
the BFF does not generate them.

---

## Login Flow (`/auth/login`)

This is the flow of `gui_agent` and `gui_treedb`.

```
 ┌─────────────────────────────────────────────────────────────────────────┐
 │  STEP 1: The user submits the SPA's login form                         │
 │                                                                        │
 │  Browser (C_AGENT_LOGIN / C_TREEDB_LOGIN)                              │
 │  └── POST https://app.example.com:1808/auth/login                      │
 │      Content-Type: application/json                                    │
 │      credentials: "include"                                            │
 │      { "username": "john", "password": "..." }                         │
 └─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
 ┌─────────────────────────────────────────────────────────────────────────┐
 │  STEP 2: The BFF asks the IdP for tokens (c_auth_bff.c)                │
 │                                                                        │
 │    POST https://auth.example.com/realms/{realm}/protocol/              │
 │         openid-connect/token                                           │
 │    Content-Type: application/x-www-form-urlencoded                     │
 │                                                                        │
 │    grant_type=password                                                 │
 │    &username=john                                                      │
 │    &password=...                                                       │
 │    &client_id={client_id}                                              │
 │    &client_secret=...  (only if the client is confidential)            │
 │                                                                        │
 │  The IdP responds with:                                                │
 │    { access_token, refresh_token, expires_in,                          │
 │      refresh_expires_in, token_type: "Bearer" }                        │
 │  Some IdPs add an id_token (it depends on the IdP and the scope);      │
 │  the BFF does not use it.                                              │
 └─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
 ┌─────────────────────────────────────────────────────────────────────────┐
 │  STEP 3: The BFF sets HttpOnly cookies and responds                    │
 │                                                                        │
 │  ├── Reads username and email from the token claims                    │
 │  ├── Sets the cookies on the HTTP response:                            │
 │  │                                                                     │
 │  │   Set-Cookie: access_token=<JWT>; Max-Age=<expires_in>;             │
 │  │     Path=/; HttpOnly; Secure; SameSite=Strict[; Domain=<domain>]    │
 │  │   Set-Cookie: refresh_token=<JWT>; Max-Age=<refresh_expires_in>;    │
 │  │     Path=/; HttpOnly; Secure; SameSite=Strict[; Domain=<domain>]    │
 │  │                                                                     │
 │  └── Returns to the browser (NO tokens in the body):                   │
 │      {                                                                 │
 │        "success": true,                                                │
 │        "username": "john",                                             │
 │        "email": "john@example.com",                                    │
 │        "expires_in": 300,                                              │
 │        "refresh_expires_in": 1800                                      │
 │      }                                                                 │
 └─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
 ┌─────────────────────────────────────────────────────────────────────────┐
 │  STEP 4: The browser opens the WebSocket (authenticated)               │
 │                                                                        │
 │  A cookie is not scoped by port, so the browser attaches it to the     │
 │  Upgrade request to any port of the same host:                         │
 │                                                                        │
 │  GET wss://app.example.com:1996/  HTTP/1.1                             │
 │  Upgrade: websocket                                                    │
 │  Cookie: access_token=<JWT>; refresh_token=<JWT>                       │
 └─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
 ┌─────────────────────────────────────────────────────────────────────────┐
 │  STEP 5: Server-side JWT validation chain                              │
 │                                                                        │
 │  C_WEBSOCKET (c_websocket.c)                                           │
 │  └── Captures the Cookie header of the HTTP Upgrade request            │
 │      └── Passes it via EV_ON_OPEN { http_cookie: "..." }               │
 │                                                                        │
 │  C_IEVENT_SRV (c_ievent_srv.c) — the "gatekeeper"                      │
 │  └── Parses "access_token=<JWT>" from the cookie string                │
 │      └── Injects the JWT into the IDENTITY_CARD                        │
 │          └── Sends EV_IDENTITY_CARD to C_AUTHZ                         │
 │                                                                        │
 │  C_AUTHZ (c_authz.c) — authentication & authorization manager          │
 │  └── verify_token():                                                   │
 │      ├── Verify JWT signature using JWKS public keys (libjwt)          │
 │      ├── Check token expiry (exp claim)                                │
 │      ├── Validate issuer (iss claim)                                   │
 │      ├── Check email_verified claim                                    │
 │      ├── Look up user in treedb                                        │
 │      └── Assign roles and permissions                                  │
 │                                                                        │
 │  ✓ Connection authenticated → service GClasses can process events      │
 └─────────────────────────────────────────────────────────────────────────┘
```

A failed login answers with a stable `error_code` (for example
`invalid_credentials`, `account_disabled`, `auth_service_unavailable`), which
the SPA uses as its i18n key.

---

## PKCE Authentication Flow

The BFF also serves the authorization-code flow, for an SPA that sends the
user to the IdP's own login page (or to a social login through it). Only the
start differs from the flow above:

```{figure} ../_static/auth_flow.svg
:alt: OIDC/PKCE auth sequence across Browser, auth_bff, Keycloak and the backend yuno — login phase (PKCE code exchange, HttpOnly cookie) and per-request phase (WS + cookie, JWKS verify, the per-command authz gate that runs only when enable_command_authz is set).
:width: 100%

The PKCE login and the per-request phase.
```

1. The SPA generates `code_verifier`, `code_challenge` (S256) and a `state`
   nonce, keeps the verifier and the state in `sessionStorage`, and redirects
   to the IdP:

   ```
   https://auth.example.com/realms/{realm}/protocol/openid-connect/auth
       ?response_type=code
       &client_id={client_id}
       &redirect_uri=https://app.example.com/
       &scope=openid profile email
       &code_challenge={challenge}
       &code_challenge_method=S256
       &state={state}
   ```

2. The user authenticates at the IdP, which redirects back:
   `302 → https://app.example.com/?code=AUTH_CODE&state=STATE_NONCE`.
3. The SPA checks the `state`, removes `?code=` from the address bar
   (`history.replaceState()`), and posts to the BFF:

   ```
   POST https://app.example.com:1808/auth/callback
   Content-Type: application/json
   credentials: "include"
   {
     "code": "AUTH_CODE",
     "code_verifier": "ORIGINAL_VERIFIER",
     "redirect_uri": "https://app.example.com/"
   }
   ```

4. The BFF checks that `redirect_uri` starts with `allowed_redirect_uri`, and
   calls the token endpoint with `grant_type=authorization_code`, the `code`
   and the `code_verifier`. The IdP checks
   `SHA-256(code_verifier) == code_challenge`.
5. From here on it is step 3 of the login flow: cookies, and the same body.

With Keycloak, a social login (Google, GitHub and more) is the
`kc_idp_hint` parameter of the authorization URL: Keycloak skips its own page
and redirects to that provider. The BFF does not know which provider
authenticated the user.

---

## Session Restore and Token Refresh

On load, the SPA calls `POST /auth/refresh`. The tokens are `HttpOnly`, so
the answer of the refresh is the only way a reloaded SPA learns that it has a
session, and who it is (`username`, `email`).

The access token has a short lifetime (typically 5 minutes). The SPA arms the
next refresh before it expires:

```
save_session_info()
├── timeout = max(0.75 * expires_in, expires_in - 30) seconds
│     (expires_in 300 → refresh after 270 s)
│
├── When the timer fires:
│     POST https://app.example.com:1808/auth/refresh
│     credentials: "include"  (sends the HttpOnly cookies)
│
├── C_AUTH_BFF:
│     ├── Reads refresh_token from the cookie
│     ├── Calls the token endpoint:
│     │     grant_type=refresh_token
│     │     &refresh_token=<from_cookie>
│     │     &client_id={client_id}
│     ├── Receives a new access_token + refresh_token
│     └── Sets new HttpOnly cookies (overwriting the old ones)
│
└── Browser receives { success, username, email, expires_in, refresh_expires_in }
    └── Arms the next refresh
```

**A refresh that cannot be made is not a logout.** A network failure, a
timeout (`gui_treedb` gives each BFF call 15 s) or a 5xx answer such as
`502 auth_service_unavailable` (the IdP is unreachable, for example on a node
just after a reboot) says nothing about the user's credentials. The SPA keeps
the session and retries with backoff (5 s, doubling up to 60 s). Only a 4xx
answer (`session_expired`, `missing_refresh_token`) ends the session.

---

## Logout Flow

```
POST https://app.example.com:1808/auth/logout
credentials: "include"
│
├── C_AUTH_BFF:
│   ├── Reads refresh_token from the cookie
│   ├── Calls the IdP end_session_endpoint to revoke it:
│   │     refresh_token=<from_cookie>
│   │     &client_id={client_id}
│   ├── Clears the cookies with Max-Age=0:
│   │     Set-Cookie: access_token=; Max-Age=0; HttpOnly; Secure; ...
│   │     Set-Cookie: refresh_token=; Max-Age=0; HttpOnly; Secure; ...
│   └── Returns { "success": true }
│
└── Browser: closes the WebSocket, shows the login screen
```

---

(bff-auth-token-exception)=
## The Exception: Backends on Other Hosts

A cookie does not travel to another host. `gui_treedb` opens treedb backends
on hosts that the user configures, and the Monitor workspace of `gui_agent`
talks to a node's agent directly. For these, the SPA asks the BFF for the
access token:

```
POST https://app.example.com:1808/auth/token
credentials: "include"
→ { "success": true, "access_token": "<JWT>" }
```

and puts it in the `jwt` of each `C_IEVENT_CLI` identity card. The remote
`C_IEVENT_SRV` takes that `jwt` (there is no cookie), and `C_AUTHZ` validates
it against the issuer's JWKS like a cookie token: each remote backend needs
the issuer's JWKS in `Authz.jwks` and a role for the target service.

`/auth/token` is **off by default** and answers `404 unknown_endpoint`. Turn
it on only on the BFF of an SPA that needs it, with both settings:

```json
"expose_access_token": true,
"allowed_origin":      "https://app.example.com"
```

The BFF gives the token only to a request whose `Origin` is exactly
`allowed_origin`, else `403 origin_not_allowed`. An XSS on that one origin
can read the token, so keep the access-token lifetime short and the CSP of
that SPA strict.

---

## Security Properties

| Property | How it is achieved |
|----------|-------------------|
| **JWTs never in JavaScript** | Tokens stored as `HttpOnly` cookies — `document.cookie` cannot read them. Exception: `/auth/token`, opt-in and origin-pinned |
| **The password goes only to the BFF** | `/auth/login` over HTTPS; the SPA does not keep it, the BFF does not log it (traces redact it) |
| **Code interception protection** (`/auth/callback`) | PKCE S256 — intercepted authorization codes are useless without `code_verifier` |
| **CSRF protection** | `SameSite=Strict` cookies; for `/auth/callback`, the `state` nonce checked by the SPA |
| **Token theft protection** | `Secure` flag (HTTPS only). `SameSite=Strict` (no cross-site requests) |
| **CORS** | The BFF answers only `allowed_origin`; it never reflects an arbitrary `Origin` and never answers `*` |
| **Server-side JWT validation** | `C_AUTHZ` verifies signature via JWKS, checks expiry, issuer, claims |
| **Token refresh** | Proactive refresh via the BFF before the access token expires |

---

## Cookie Attributes Explained

Each cookie set by the BFF carries these attributes:

```
Set-Cookie: access_token=<JWT>;
  Max-Age=<expires_in>; ← refresh_token: refresh_expires_in
  Path=/;               ← Sent with all requests to this host
  HttpOnly;             ← JavaScript cannot access (no document.cookie)
  Secure;               ← Only sent over HTTPS
  SameSite=Strict;      ← Never sent on cross-site requests (CSRF protection)
  Domain=<domain>       ← Only when cookie_domain is set and matches the Host
```

A cookie is not scoped by port (RFC 6265), so the cookie that the BFF sets
(for example on port 1808) travels with the WebSocket Upgrade request to any
other port of the same host, with or without `Domain`. The `Domain` attribute
extends the cookie to the subdomains of `cookie_domain`. The BFF writes it
only when `cookie_domain` is set and the request's `Host` (without its port)
is that domain or one of its subdomains. When the `Host` does not match, the
BFF omits `Domain`, logs a WARNING (*"cookie_domain does not match request
Host, omitting Domain attribute"*), and the cookie binds to the exact host of
the request.

---

## Configuration

### BFF Server (C_AUTH_BFF attributes)

```json
{
  "issuer":               "https://auth.example.com/realms/myrealm/",
  "client_id":            "app.example.com",
  "client_secret":        "",
  "cookie_domain":        "app.example.com",
  "allowed_origin":       "https://app.example.com",
  "allowed_redirect_uri": "https://app.example.com/",
  "expose_access_token":  false,
  "crypto":               {"ssl_use_system_ca": true, "ssl_verify_mode": "required"},
  "pending_queue_size":   16,
  "idp_timeout_ms":       30000,
  "idp_connect_timeout_ms": 30000
}
```

| Attr | Default | Purpose |
|------|---------|---------|
| `issuer` | — (required) | OIDC issuer URL (for example `https://auth.example.com/realms/foo/`). Triggers discovery via `/.well-known/openid-configuration`. Use `token_endpoint` + `end_session_endpoint` instead to bypass discovery. |
| `client_id`, `client_secret` | — | OAuth2 client credentials (leave `client_secret` empty for a public client). |
| `cookie_domain` | `""` | `Domain=` attribute on the cookies. Written only when the request `Host` is this domain or a subdomain of it; otherwise omitted, with a WARNING. Empty: never written (host-only cookies). |
| `allowed_origin` | `""` | The CORS origin. Empty: no cross-origin access. Also the origin `/auth/token` is pinned to. |
| `allowed_redirect_uri` | `""` | The prefix a `/auth/callback` `redirect_uri` must start with. Empty: not checked, so set it whenever the BFF serves `/auth/callback`. |
| `expose_access_token` | `false` | Enables `/auth/token`. Needs `allowed_origin`. |
| `crypto` | `{"ssl_use_system_ca": true, "ssl_verify_mode": "required"}` | TLS crypto for the outbound IdP HTTP client. **Verifying-by-default** since 7.6.0: a public IdP is validated against the system CA store by default. For a private/self-signed IdP CA, override with `{"ssl_trusted_certificate": "/path/ca.pem"}`. The attribute **replaces** the default wholesale (no merge) — an override must repeat the verifying keys it still wants. mbedTLS has no system store: set `ssl_trusted_certificate` there. (The `c_idp_keycloak` `kc_crypto` attribute mirrors this for its Keycloak admin calls.) |
| `pending_queue_size` | `16` (clamped to `[1, 1024]`) | Depth of the per-channel pending IdP request queue. Raised on front-line BFFs under burst. |
| `idp_timeout_ms` | `30000` (0 disables) | Bounds the IdP round-trip once connected. On expiry the browser gets `504`. |
| `idp_connect_timeout_ms` | `30000` (0 disables) | Bounds the wait to connect to the IdP. See [`YUNO_AUTH.md` §2.6](../../../yunos/c/yuno_agent/YUNO_AUTH.md). |

### BFF internals (operator knobs)

A few details that are not visible in the request/response flow but matter
in production:

- **One BFF per channel, one pending queue each.** The `auth_bff` yuno builds
  a `C_AUTH_BFF` under each `C_CHANNEL` of its server. Each one keeps a FIFO of
  pending IdP requests (depth `pending_queue_size`), with one job in flight at
  a time. Overflows bump the `q_full_drops` stat and the browser gets
  `503 server_busy`.
- **Flush-on-disconnect.** When a browser closes mid-round-trip the BFF
  flushes its queue for that channel and, if a reply from the IdP later
  arrives for a disconnected client, drops it (`responses_dropped`
  counter). No cross-user token leak can occur: each task carries a
  per-browser generation which the reply path re-checks.
- **Two IdP watchdogs.** The round-trip once connected is `C_TASK`'s
  `exec_timeout` (`idp_timeout_ms`, answered `504 auth_timeout`). The wait to
  connect is a `C_TIMER` child of the BFF (`idp_connect_timeout_ms`), armed
  only while the outbound is disconnected; when discovery never completed the
  answer is `502 auth_service_unavailable`. `idp_timeouts` counts every IdP
  task that ends by one of the two watchdogs: a login, callback, refresh or
  logout (answered `504 auth_timeout`) and the discovery (each request queued
  behind it is answered `502`). A
  failed discovery is retried at the next request, not by a timer.
- **Stable error_code for GUI i18n.** Every error response carries a
  stable `error_code` (snake_case, for example `session_expired`,
  `auth_service_unavailable`, `server_busy`; the catalog is in
  `c_auth_bff.h`). The human `error` message is
  localisable in the GUI. `error_code` is the i18n translation key and
  never changes between releases.
- **Log hygiene.** An answer to the browser with a 4xx status (a wrong
  password, an expired session) is logged as a WARNING (*"👤BFF request
  rejected"*), not an ERROR — a wrong password is not a server error. A 5xx
  answer is an ERROR (*"👤BFF server error"*). When the IdP's answer maps to
  one of the generic codes, one more ERROR line carries what the IdP said
  (*"👤BFF IdP rejected the request"*). All secrets (cookies, passwords,
  authorization headers, client_secret) are redacted by
  [`redact_for_trace()`](https://github.com/artgins/yunetas/blob/7.29.3/kernel/c/root-linux/src/c_auth_bff.c#L1023) with case-insensitive key matching.
- **Stats.** `mt_stats` exposes:
  `requests_total`, `q_count`, `q_max_seen`, `q_full_drops`,
  `idp_calls`, `idp_ok`, `idp_errors`, `idp_timeouts`,
  `responses_dropped`, `bff_errors`. Any 2xx IdP reply counts as
  `idp_ok` (Keycloak `/logout` returns spec-compliant 204, which
  previously poisoned the health signal). The filter accepts
  `"<stat>"` or `"<prefix>_"` and is case-insensitive.
- **Trace levels.** `messages` and `traffic` trace levels dump the full
  BFF↔IdP and BFF↔browser exchanges, with 👤 BFF prefix and ⏩/⏪
  direction arrows. Both honour the redaction rules above.

### Frontend (`conf/deploy.js`)

The SPA has no table of hosts: one build serves every deployment, and the BFF
URL is derived from the host that serves the SPA.

This is `gui_treedb/src/conf/deploy.js`:

```javascript
const BFF_PORT = 1808;   /* auth_bff for the treedb GUI plane */

function deploy_info()
{
    let host = (typeof window !== "undefined" && window.location && window.location.hostname)
        ? window.location.hostname : "localhost";

    let tenant = (host.split(".")[0]) || "local";
    let is_local = (host === "localhost" || host === "127.0.0.1");

    return {
        host:    host,
        tenant:  tenant,
        bff_url: is_local ? "" : `https://${host}:${BFF_PORT}`
    };
}
```

### IdP Client Settings (Keycloak)

One client per BFF, in the realm of the project.

| Setting | Value |
|---------|-------|
| Direct Access Grants | **On** — `/auth/login` uses `grant_type=password`; without it every login is refused |
| Standard Flow | On only if the BFF must also serve `/auth/callback` |
| Client Authentication | Off (public), or On (confidential) with `client_secret` in the BFF config |
| PKCE Method | S256 (only for `/auth/callback`) |
| Valid Redirect URIs | `https://app.example.com/*` (only for `/auth/callback`) |
| Web Origins | `https://app.example.com` |

---

## Why BFF Instead of Direct Token Exchange?

Without a BFF, the browser calls the IdP's token endpoint directly and receives
the JWT in a JavaScript-accessible response. This exposes tokens to:

- XSS attacks (malicious scripts can steal them)
- Browser extensions
- `localStorage`/`sessionStorage` persistence (survives page reload, accessible to any JS)

With the BFF pattern, the token exchange happens server-to-server. The browser
only receives metadata (`username`, `expires_in`). The actual JWT travels only
in `HttpOnly` cookies that JavaScript cannot read, and the browser attaches
them to the later requests by itself.
