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

(bff-dev-proxy)=
### Development behind a proxy

The vite proxy (`changeOrigin: true`) rewrites only the `Host` of the
request. The `Origin` stays the one of the dev server
(`http://localhost:5173`), and the BFF serves only the `Origin` of its SPA
(see [Logout Flow](#bff-logout-flow)): a BFF with the production
`allowed_origin` answers every login, refresh and logout from the dev server
`403 origin_not_allowed`, and its `error` names both origins:

```
{"success": false, "error_code": "origin_not_allowed",
 "error": "Origin 'http://localhost:5173' not allowed for /auth/logout: allowed_origin is 'https://app.example.com'"}
```

Do one of the two:

- Give the dev BFF the origin of the dev server (the localhost batch
  `auth_bff.1801.json` does):

  ```json
  "allowed_origin": "http://localhost:5173"
  ```

- Or make the proxy send the `Origin` the BFF allows, in the
  `vite.config.js` of the SPA:

  ```javascript
  proxy: {
      "/auth": {
          target: "https://localhost:1808",
          changeOrigin: true,
          secure: false,
          headers: {origin: "https://app.example.com"}   // the allowed_origin of the BFF
      }
  }
  ```

A logout refused this way leaves the session: the `HttpOnly` cookies stay,
and a reload of the SPA restores it. So does a logout that does not reach
the BFF. A logout the BFF serves clears the cookies whatever happens at the
IdP (see [Logout Flow](#bff-logout-flow)).

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

4. The BFF checks that `allowed_redirect_uri` allows the `redirect_uri` (see
   the table of [the attributes](#bff-config-attrs)), and
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

**The IdP may answer a refresh with no new refresh_token** (RFC 6749 §6: the
server MAY issue one; when it does not, the client keeps the one it has). The
BFF then sends no `Set-Cookie` for `refresh_token`: the browser keeps its
cookie, and the next refresh uses it. Only the access_token cookie is
replaced, and the answer carries no `refresh_expires_in` (no cookie was
written for it to describe):

```
HTTP/1.1 200 OK
Set-Cookie: access_token=<new JWT>; Max-Age=300; Path=/; HttpOnly; Secure; SameSite=Strict
                                         ← no Set-Cookie: refresh_token=...

{"success": true, "username": "john", "email": "john@example.com", "expires_in": 300}
```

**A login (or a callback) answered with no refresh_token opens a session that
cannot be refreshed.** There is no refresh_token to keep, so the BFF clears the
cookie (`Set-Cookie: refresh_token=; Max-Age=0; ...`: no refresh_token of an
earlier session stays in the browser), answers with no `refresh_expires_in`,
and logs a WARNING once per process (*"👤BFF IdP answered a login with no
refresh_token: its session cannot be refreshed, it ends with its access_token
(said once)"*). The session ends when its access_token expires; the next
refresh answers `401 missing_refresh_token`. The logout of such a session
works: see [Logout Flow](#bff-logout-flow). To get refreshable sessions,
configure the IdP client to issue refresh tokens.

**A lifetime of 0 is a token with no expiry.** Keycloak answers
`"refresh_expires_in": 0` for an offline token (scope `offline_access`). A
`Max-Age=0` would tell the browser to delete the cookie on arrival, so the BFF
writes that cookie with no `Max-Age`: a cookie of the browser session. A
browser drops it when its session ends, but a browser that restores its
session (Chrome and Firefox with "continue where you left off") keeps it, so
the cookie of an offline token can live as long as the token. The same holds
for `"expires_in": 0`. The answer says the 0 as it came:

```
IdP:      {"access_token": "...", "expires_in": 300,
           "refresh_token": "...", "refresh_expires_in": 0}

BFF:      Set-Cookie: access_token=<JWT>; Max-Age=300; Path=/; HttpOnly; Secure; SameSite=Strict
          Set-Cookie: refresh_token=<JWT>; Path=/; HttpOnly; Secure; SameSite=Strict
          {"success": true, ..., "expires_in": 300, "refresh_expires_in": 0}
```

The SPAs read `"expires_in": 0` as their default of 300 s for the next
refresh (`expires_in || 300`).

**The answer of the IdP is data of a peer, and the BFF reads it that way.** A
token that is not a string (`"access_token": 123`, `"refresh_token": {}`) is
refused: `502 auth_unexpected_error`, *"Authentication service answered an
access_token that is not a string"*. Two WARNINGs with no stack say it: one
names the token and its type (*"👤BFF IdP answered a token that is not a
string"*, `"type": "integer"`), and one is the answer of the 502 (*"👤BFF IdP
answer refused"*, `"error_code": "auth_unexpected_error"`). A success with no
access_token is refused the same way, with that second WARNING only. A token
goes into a `Set-Cookie` as it is, so a token with a byte that a cookie value
cannot carry (outside RFC 6265 `cookie-octet`: a control byte, a space, `"`,
`,`, `;`, `\`) is refused too, whole: `502 auth_unexpected_error`
(*"Authentication service answered a refresh_token with a byte a cookie
cannot carry"*), with a WARNING that names the token, the position and the
byte (*"👤BFF IdP answered a token that is not a cookie value"*), never the
token. Without it, an `access_token` with `; Domain=evil.com` would add an
attribute, and a `refresh_token` with a CR LF would begin another header. So
is a success whose body is not a JSON object, answered `502 auth_empty_response`:
an empty body, text that is not JSON (a proxy's HTML page), or another JSON
type (`[1, 2]`, the JSON string `""`). Its `error` says which
(*"Authentication service answered success with an empty body"*, *"... with
a body that is a JSON string, not an object"*), and a body labelled
`application/json` that does not parse also gets the WARNING of the HTTP
parser (*"data of a peer is not json"*). The BFF reads at most 1 MB of the
body of an IdP answer: a longer one is not read to the end (one WARNING of
the HTTP client, *"HTTP body larger than its limit, message refused"*, and
the connection to the IdP dropped), and the request is answered `502
auth_unexpected_error` (*"Authentication service answered a body longer than
1048576 bytes"*). So is an answer whose headers pass the 64 KB the HTTP
client takes (*"... answered headers longer than 65536 bytes"*), or that
breaks the HTTP protocol once begun (*"... answered an answer that breaks the
HTTP protocol (...)"*): the WARNING of the HTTP client, then the WARNING of
the 502. Such an answer to a logout still clears the cookies, with a
WARNING (*"👤BFF IdP answer to a logout refused, the cookies are
cleared"*). Every
other 5xx of the BFF is an ERROR (*"👤BFF server error"*). A lifetime
(`expires_in`, `refresh_expires_in`) is a JSON integer or the text of one:
`"expires_in": "300"` is read as 300, as some IdPs send it. Anything else
(`"expires_in": "soon"`, `300.5`, `false`, a negative number) gets its default
(300 for `expires_in`, 1800 for `refresh_expires_in`) and a WARNING that names
the key and the value (*"👤BFF IdP answered a lifetime that is not a number of
seconds, its default is used"*, `"key": "expires_in"`, `"value": "300.5"`).

**A `429` of the IdP passes on its `Retry-After`** (RFC 9110 §10.2.3). The
BFF answers `429 auth_rate_limited` with the `Retry-After` of the IdP when it
is one of these, with the blanks around it removed:

| The IdP sends | The browser gets |
|---|---|
| delay-seconds of 1 to 10 digits: `120` | `Retry-After: 120` |
| IMF-fixdate: `Wed, 21 Oct 2026 07:28:00 GMT` | `Retry-After: Wed, 21 Oct 2026 07:28:00 GMT` |
| RFC 850: `Wednesday, 21-Oct-26 07:28:00 GMT` | `Retry-After: Wed, 21 Oct 2026 07:28:00 GMT` |
| asctime: `Wed Oct 21 07:28:00 2026` | `Retry-After: Wed, 21 Oct 2026 07:28:00 GMT` |

A date must be a real one: its day exists in its month, the time is
00:00:00 to 23:59:60, and the day-name is the one of that date. An RFC 850
year more than 50 years ahead is the same year a century before. The names
are case-sensitive and the zone is `GMT`. Anything else is not passed on:
11 digits or more, `Wed, 99 Oct 2026 ...`, `Thu, 21 Oct 2026 ...` (a
Wednesday), a list (`120, 60`), `soon`, `-5`. It gets a WARNING
(*"👤BFF IdP answered a Retry-After that is not delay-seconds (1 to 10
digits) nor an HTTP-date, not passed on"*), so a value of the IdP never
writes more than that one header. The CORS headers of every answer carry
`Access-Control-Expose-Headers: Retry-After`, so a SPA on another origin
than its BFF can read it, and waits what the IdP asks.

**A request is sent to the IdP once.** When the IdP closes the connection
before it answers (it restarts, or the HTTP client cannot parse its answer),
the BFF does not send the request again on the next connection: a second
password grant, or a `refresh_token` used twice, which an IdP that rotates
them takes for a stolen token. The browser gets `502
auth_service_unavailable` (*"Authentication service closed the connection
before answering, the request is not sent again"*), with the ERROR of the
502.

**A refresh that cannot be made is not a logout.** A network failure, a
timeout (`gui_treedb` gives each BFF call 15 s) or a 5xx answer such as
`502 auth_service_unavailable` (the IdP is unreachable, for example on a node
just after a reboot) says nothing about the user's credentials. The SPA keeps
the session and retries with backoff (5 s, doubling up to 60 s). A 4xx answer
of the BFF (`session_expired`, `missing_refresh_token`) ends the session,
except a 429, which is retried after its `Retry-After`. So does a code a retry
would only give again (`token_too_large`, `auth_unexpected_error`,
`auth_config_error`, `internal_error`).

---

(bff-logout-flow)=
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

**A logout ends in the browser whatever fails at the IdP.** Every answer to a
logout the BFF serves clears both cookies, its errors too: the user is leaving,
maybe on a shared machine, and the session must not stay in that browser. The
error tells the SPA that the session may still be open at the IdP (it ends
there by its own timeout):

```
HTTP/1.1 502 Bad Gateway
Set-Cookie: access_token=; Max-Age=0; Path=/; HttpOnly; Secure; SameSite=Strict
Set-Cookie: refresh_token=; Max-Age=0; Path=/; HttpOnly; Secure; SameSite=Strict

{"success": false, "error_code": "auth_service_unavailable",
 "error": "Authentication service closed the connection before answering the logout, the cookies are cleared"}
```

**The end-session is the one request that goes twice.** Every other request
to the IdP is sent once: a refresh_token used twice can end the session, a
password grant must not be repeated by a client. Sending an end-session twice
means the same as sending it once, and not sending it leaves the session open
at the IdP. So when the IdP closes the connection before answering (the
commonest cut is a connection kept open that it closed as the request left),
the logout goes once more on the next connection, with a WARNING (*"👤BFF IdP
closed the connection before answering a logout, it is sent again"*). A
second cut is the `502` above.

**A logout with no refresh_token cookie still ends the session in the
browser.** A session opened by a login answered with no refresh_token has no
such cookie. The BFF then does not call the IdP (it has no refresh_token to
end the IdP session with; that session ends on the IdP by its own timeout),
clears both cookies with `Max-Age=0` at once, and answers
`200 {"success": true}`. It clears them only for a request that carries the
`access_token` cookie: a cross-site form carries no cookie, and cannot log
the user out.

**Every request that reads or changes the session needs the `Origin` of the
SPA**: `/auth/login`, `/auth/callback`, `/auth/refresh`, a `/auth/logout`
with a cookie (`refresh_token` or `access_token`) and `/auth/token`.
`SameSite=Strict` says same SITE, not same origin: a sibling subdomain sends
the cookies too, and a `POST` with no body (a refresh, a logout) needs no
CORS preflight, so it reaches the BFF.

- With `allowed_origin` set, the `Origin` must be exactly it: its scheme, host
  and port. **`allowed_origin` is what pins the scheme.**
- With `allowed_origin` empty, the BFF serves its own origin only: the
  `Origin` must be `https://` and the `Host` of the request
  (`Origin: https://app.example.com` with `Host: app.example.com`), or
  `http://` and the `Host` on a connection that is not TLS. A BFF that ends TLS
  itself (the deployment of the `auth_bff` batches, `https://0.0.0.0:1808`)
  knows the scheme, and refuses `Origin: http://...`. A plain connection tells
  nothing: behind a proxy that ends TLS the BFF sees http while the browser
  says `https://`, so it takes both. Set `allowed_origin` to pin it there.

  A proxy in front of the BFF must pass the `Host` on **with its port**. In
  nginx that is `$http_host`, not `$host`, which drops the port and turns every
  request on a non-standard port into a `403`:

  ```nginx
  location /auth/ {
      proxy_pass http://127.0.0.1:1801;
      proxy_set_header Host $http_host;
  }
  ```

  `/auth/token` is refused: the token is given only to an `allowed_origin` that
  is set.

Another origin, no `Origin`, `Origin: null`, a sibling subdomain, the
allowed origin with more after it (`https://app.example.com.evil.com`,
`https://app.example.com:8443`) or a part of it, a `Host` that is a list (two
`Host` lines, which RFC 9110 §7.2 makes invalid): `403 origin_not_allowed`,
no IdP call, no `Set-Cookie`. A browser sends `Origin` with every `POST`,
same-origin included; a client that is not a browser must send it too. The
`error` names the `Origin` received and the one allowed.

| Request to `/auth/logout` | Answer |
|---|---|
| `refresh_token` cookie, the SPA's `Origin` | IdP end-session call, both cookies cleared, `200` (also when the IdP answers an error, or an answer the HTTP client refuses, with a WARNING) |
| The same, the IdP cuts the connection once | Sent again on the next connection, both cookies cleared, `200` |
| The same, the IdP cuts it twice, or the discovery fails | Both cookies cleared, `502 auth_service_unavailable` |
| The same, the IdP stays silent | Both cookies cleared, `504 auth_timeout` |
| The same, the pending queue is full | Both cookies cleared, `503 server_busy`, no IdP call |
| The same, `end_session_endpoint` is not a URL | Both cookies cleared, `500 auth_config_error` |
| `access_token` cookie only, the SPA's `Origin` | No IdP call, both cookies cleared, `200` |
| A cookie, another `Origin` or none | `403 origin_not_allowed`, no IdP call, no `Set-Cookie` |
| No cookie (no session, or a cross-site request) | `200 {"success": true}`, no `Set-Cookie`: nothing to end |
| `refresh_token` cookie longer than the BFF writes (name and value over 4096 bytes) | `400 token_too_large`, no `Set-Cookie`: it is not one of its cookies, and no session it opened |

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
| **CSRF protection** | `SameSite=Strict` cookies, and the `Origin` of the SPA on every request that reads or changes the session (a sibling subdomain is same-site); for `/auth/callback`, the `state` nonce checked by the SPA |
| **No open redirect of the code** | `allowed_redirect_uri`: exactly it, or a path under it that no server reads out of it (no dot-segment, also before a `;`, no `\`, no `#`, no `%XX` of a dot, a slash or a byte out of visible ASCII); its host cannot be extended |
| **A request is sent to the IdP once** | Not sent again when the IdP closes the connection before it answers (`502`) |
| **Token theft protection** | `Secure` flag (HTTPS only). `SameSite=Strict` (no cross-site requests) |
| **CORS** | The BFF answers only `allowed_origin`; it never reflects an arbitrary `Origin` and never answers `*` |
| **Server-side JWT validation** | `C_AUTHZ` verifies signature via JWKS, checks expiry, issuer, claims |
| **Token refresh** | Proactive refresh via the BFF before the access token expires |
| **The form to the IdP** | Each key and value form-urlencoded (`+`, `%`, `&`, `=` of a password or a cookie are bytes of the value): a user cannot add parameters to a request the BFF signs with its `client_secret` |
| **Pipelined requests** | Answered in their order (RFC 9112 §9.3.2): an answer the BFF knows at once waits behind a request still at the IdP |

---

## Cookie Attributes Explained

Each cookie set by the BFF carries these attributes:

```
Set-Cookie: access_token=<JWT>;
  Max-Age=<expires_in>; ← refresh_token: refresh_expires_in; none for a 0
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
the request. A request with no `Host` gets no `Domain` either, with a WARNING
(*"request without Host, omitting Domain attribute"*). So does a request whose
`Host` name is longer than a host name can be (253 bytes), with a WARNING
(*"request Host longer than a host name, omitting Domain attribute"*) whose
`host_len` is the length of the name in the request (`"host_len": 300` for a
name of 300 bytes), measured before the BFF keeps 255 bytes of the `Host`.

A token travels in a cookie and comes back in one. A browser keeps a cookie
only when its name and value take at most 4096 bytes (RFC 6265bis, in the
algorithm that parses a `Set-Cookie`; Chrome and Firefox drop a bigger one with
nothing said, and the session then opens with no token). RFC 6265 §6.1 is not
that rule: it counts the name, the value and the attributes, and it is a
minimum a browser must keep. So the BFF takes an access_token of at most 4084 bytes (4096
less `access_token`) and a refresh_token of at most 4083 (4096 less
`refresh_token`). A bigger token is refused by its size, before anything reads
it: `500 token_too_large`, and its `error` says the sizes (*"Token too large
for a browser cookie: access_token 5120 bytes (limit 4084), refresh_token 652
bytes (limit 4083); a cookie name and value take at most 4096 bytes"*). An
IdP that answers a success with no access_token is refused too: `502
auth_unexpected_error`, *"Authentication service answered success with no
access_token"*.

The same rule reads the cookies that come back. A `refresh_token` cookie
(`/auth/refresh`, `/auth/logout`) of more than 4083 bytes, or an
`access_token` cookie (`/auth/token`) of more than 4084 (name and value over
4096 bytes), is none the BFF wrote: `400 token_too_large`, *"refresh_token
cookie of 4084 bytes: the BFF writes none longer than 4083 (a cookie name
and value take at most 4096 bytes)"*, and it goes nowhere. A cookie that
fits is taken whole.

---

## Configuration

(bff-config-attrs)=
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
| `allowed_origin` | `""` | The origin of the SPA: the CORS origin, the one `Origin` served for login, callback, refresh, logout and token (else `403`), and the origin `/auth/token` is pinned to. It is what pins the scheme. Empty: no cross-origin access, the `Origin` must be the BFF's own (`https://` and the `Host`, or `http://` and it on a connection that is not TLS), and `/auth/token` is refused. |
| `allowed_redirect_uri` | `""` | The `redirect_uri` a `/auth/callback` may have. Without a `/` at its end, exactly it: `"https://app.example.com/auth/callback"`. Ending with `/` after a scheme and a host, one that starts with it and whose rest is a path and query in it: `"https://app.example.com/app/"` allows `https://app.example.com/app/cb?x=1`, `.../app/a%41;v=1` and any query, not `https://app.example.com/app/../admin`, `.../app/%2e%2e/x`, `.../app/..\x`, `.../app/x#f`, a space or a byte over 0x7E. A prefix is a strict rule: what RFC 3986 allows and some server normalizes out of the prefix is refused too, so a `.` or `..` segment before a `;` (`.../app/..;/x`, which Tomcat reads as `..`), and in the path a `%XX` of `.`, `/`, `\`, `;`, `?`, `#`, `%` or a byte out of visible ASCII (`.../app/..%2Fx`, `.../app/%5c..%5cx`, `.../app/%252e%252e/x`, `.../app/%c0%ae/x`), or one badly written (`.../app/x%zz`). Its host cannot be extended: `https://app.example.com.evil.com/` and `https://app.example.com@evil/` never pass `"https://app.example.com/"`. Empty: not checked, so set it whenever the BFF serves `/auth/callback`. |
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
  logout (answered `504 auth_timeout`, a logout with both cookies cleared) and
  the discovery (each request queued behind it is answered `502`). A timeout logs one ERROR for its cause, when
  its task ends -- *"OIDC discovery: IdP did not answer in time"* (with the
  number `queued`) or *"BFF IdP outbound watchdog fired"* -- with its
  `cause`, `connected` and `tcp_state` (what the outbound to the IdP was
  then). Each answer it causes adds the ERROR of that answer (*"BFF server
  error"*): one for the `504`, or one for each request queued behind the
  discovery, after an INFO with how many. A
  failed discovery is retried at the next request, not by a timer. A
  discovery goes twice at most when the IdP cuts its connection, or an IdP
  that always cuts would re-arm its timeout for ever. What a discovery
  answers is data of a peer: a broken answer, no endpoints, or an endpoint
  that is not a URL or is longer than a path are a WARNING (*"👤BFF OIDC
  discovery: an endpoint is not a URL"*), and refused there, so a bad endpoint
  is never kept. A status other than 200 is the IdP saying no, an ERROR.
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
  (*"👤BFF IdP rejected the request"*). The status on the wire is the one of
  the catalog (`502`, `429`, ...), and every error answered carries a code of
  the catalog: one without it is a bug of the BFF, logged with a stack and
  answered `internal_error`. All secrets (cookies, passwords,
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
