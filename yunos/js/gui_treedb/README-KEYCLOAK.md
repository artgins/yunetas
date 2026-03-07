# Deep Review: OAuth2 Authorization Code + PKCE with BFF Pattern

Now I have a thorough understanding of the entire OAuth2 Authorization Code + PKCE + BFF flow. Let me compile my deep review.

## Architecture Overview

The implementation spans four layers forming a complete BFF pattern:

| Layer | Component | File |
|-------|-----------|------|
| **Frontend (JS)** | `C_LOGIN` GClass | `yunos/js/gui_treedb/src/c_login.js` |
| **Config** | Per-hostname BFF/Keycloak URLs | `yunos/js/gui_treedb/src/conf/backend_config.js` |
| **BFF Server (C)** | `C_AUTH_BFF` GClass | `kernel/c/root-linux/src/c_auth_bff.c` |
| **WebSocket Auth (C)** | `C_IEVENT_SRV` + `C_AUTHZ` | `kernel/c/root-linux/src/c_ievent_srv.c`, `c_authz.c` |
| **WebSocket Transport** | `C_WEBSOCKET` | `kernel/c/root-linux/src/c_websocket.c` |

The token flow:

```
Browser ──PKCE redirect──> Keycloak ──?code=&state=──> Browser
Browser ──POST /auth/callback──> C_AUTH_BFF ──token exchange──> Keycloak
C_AUTH_BFF ──Set-Cookie: httpOnly──> Browser (tokens never in JS)
Browser ──WebSocket Upgrade (Cookie auto-sent)──> C_WEBSOCKET
C_WEBSOCKET ──Cookie header──> C_IEVENT_SRV ──jwt extracted──> C_AUTHZ ──verify──> access granted
```

---

## What's Done Well

### 1. PKCE Implementation (Frontend) — Solid
- `c_login.js:118-145` — `code_verifier` generated with `crypto.getRandomValues(32)` + base64url encoding (RFC 7636 compliant)
- `code_challenge` computed via `crypto.subtle.digest("SHA-256", ...)` — correct S256 method
- `state` nonce generated with 16 bytes of randomness — good CSRF protection
- `sessionStorage` used for PKCE state (tab-scoped, not cross-origin, not persistent) — correct choice
- PKCE state immediately deleted after consumption (`c_login.js:345`) — one-time use enforced

### 2. BFF Token Isolation — Core Goal Achieved
- `c_auth_bff.c` exchanges the authorization code server-side (`c_auth_bff.c:554-569`)
- Tokens written exclusively as `HttpOnly; Secure; SameSite=Strict` cookies (`c_auth_bff.c:194`)
- BFF response to browser contains only `{success, username, email, expires_in, refresh_expires_in}` — no raw JWT (`c_auth_bff.c:498-514`)
- `c_yuneta_gui.js:714-718` confirms JWT is NOT forwarded from JavaScript to the WebSocket

### 3. Cookie Security Attributes — Correct
- `HttpOnly` — JavaScript cannot read via `document.cookie`
- `Secure` — cookies only sent over HTTPS
- `SameSite=Strict` — strongest CSRF protection (no cross-site requests carry the cookie)
- `Domain` attribute configurable for sharing between BFF port 1801 and WebSocket port 1800
- Logout properly clears cookies with `Max-Age=0` (`c_auth_bff.c:208-211`)

### 4. WebSocket Cookie Bridge — Well Integrated
- `c_websocket.c:1676-1689` — captures the `Cookie` header from the HTTP Upgrade request and passes it up via `EV_ON_OPEN`
- `c_ievent_srv.c:541-551` — stores the cookie string in `http_cookie` volatile attribute
- `c_ievent_srv.c:760-787` — if no JWT in IDENTITY_CARD, extracts `access_token=` from the cookie and injects it into the kw for authentication
- `c_authz.c:846-921` — JWT is validated via `verify_token()` which uses JWKS-based `jwt_checker_verify2()` with signature verification, expiry checks, and issuer validation

### 5. URL Cleanup — Good Practice
- `c_login.js:338-339` — `window.history.replaceState()` immediately removes `?code=&state=` from the URL bar after callback, preventing leakage via browser history, referrer headers, or extensions

### 6. CORS Handling — Reasonable
- `c_auth_bff.c:277-295` — `Access-Control-Allow-Credentials: true` + configurable `allowed_origin` + `Vary: Origin`
- Preflight (OPTIONS) returns appropriate `Allow-Methods`, `Allow-Headers`, `Max-Age`

### 7. Token Refresh — Correct Pattern
- `c_login.js:476-498` — refresh timer set to `refresh_expires_in - 10` seconds, proactively refreshing before expiry
- BFF `/auth/refresh` reads the `refresh_token` httpOnly cookie server-side (`c_auth_bff.c:889-900`)
- Fresh cookies are set on refresh response

---

## Issues Found

### CRITICAL

#### C1. CSP `connect-src` Missing BFF URLs
**File:** `yunos/js/gui_treedb/index.html:17-20`

The CSP `connect-src` directive includes WebSocket and Keycloak URLs but **NOT** the BFF endpoints (`https://localhost:1801`, `https://treedb.yunetas.com:1801`). The `fetch()` calls to `/auth/callback`, `/auth/refresh`, and `/auth/logout` will be **blocked by the browser's CSP enforcement** unless `'self'` covers them (it won't — different port = different origin).

```html
connect-src 'self'
  wss://localhost:1800
  wss://treedb.yunetas.com:1800
  https://auth.artgins.com;   <!-- BFF URLs missing! -->
```

**Fix:** Add `https://localhost:1801 https://treedb.yunetas.com:1801` to `connect-src`.

#### C2. Cookie Value Not URL-Encoded — Injection Risk
**File:** `c_auth_bff.c:189-203`

The `make_set_cookie()` function writes token values directly into the `Set-Cookie` header with `snprintf(... "%s=%s; ...")`. JWT tokens contain base64url characters that are safe for cookies, but the **value is not sanitized or validated**. If Keycloak ever returns a token containing `;`, `\r\n`, or other header-injection characters, this could lead to **HTTP response header injection**.

While JWTs from Keycloak are base64url-encoded (safe characters), a defense-in-depth approach would validate or encode the value.

**Fix:** Either validate that the token only contains `[A-Za-z0-9._-]` before writing it, or URL-encode the cookie value.

### HIGH

#### H1. `browser_src` Stored as Raw Pointer — Use-After-Free Risk
**File:** `c_auth_bff.c:97-105, 733-734`

The `PENDING_AUTH.browser_src` field stores a raw `hgobj` pointer to the `c_prot_http_sr` connection. This pointer is passed through the task system as a `json_int_t` cast (`c_auth_bff.c:733`). If the browser client disconnects while a Keycloak request is in-flight, the `browser_src` pointer becomes dangling. When the Keycloak response arrives, `send_json_response(browser_src, ...)` will dereference a freed gobj.

The `ac_on_close` handler (`c_auth_bff.c:816-820`) does nothing to invalidate queued or in-progress references to that connection.

**Fix:** On `EV_ON_CLOSE`, scan the queue and the current in-progress task for matching `browser_src` and NULL them out. In `result_token_response` / `result_kc_logout`, check if `browser_src` is still a valid gobj before sending.

#### H2. JWT Payload Decoded Without Signature Verification (BFF Side)
**File:** `c_auth_bff.c:447-484`

The BFF decodes the JWT payload (middle base64url segment) to extract `username` and `email` for the response. This is done by raw base64url decoding **without any signature verification**. While the token was just received from Keycloak (trusted), this is still a risky pattern — if the code is ever reused or the flow changes, unsigned claims could be trusted.

The real validation happens later in `c_authz.c` via `verify_token()` with JWKS. But the BFF is returning `username` and `email` to the browser from an unverified decode.

**Mitigation:** This is acceptable since the token comes directly from Keycloak over TLS, but add a comment explaining this trust boundary. Consider using `libjwt` for proper decode if available.

#### H3. Refresh Token in Fixed 4KB Buffer
**File:** `c_auth_bff.c:103, 891`

Refresh tokens can vary in size. Keycloak's offline tokens can be >4KB. The `refresh_token[4096]` buffer in `PENDING_AUTH` and the `rt[4096]` local in `ac_on_message` could silently truncate long tokens, causing the Keycloak refresh/logout to fail with an opaque error.

**Fix:** Use dynamic allocation for the token, or at minimum log a warning when truncation occurs.

### MEDIUM

#### M1. No `nonce` Parameter in OIDC Authorization Request
**File:** `c_login.js:304-312`

The authorization URL includes `scope=openid` (requesting OIDC), but does not include a `nonce` parameter. The OpenID Connect Core spec (§3.1.2.1) **requires** `nonce` for the Implicit flow and **recommends** it for the Authorization Code flow. Without `nonce`, there's no protection against ID token replay attacks at the token endpoint.

Since you're using the Authorization Code flow (not Implicit), this is a recommendation rather than a strict requirement, but it would strengthen security.

**Fix:** Generate a random nonce, include it in the auth URL, and verify it in the `id_token` claims after token exchange.

#### M2. Hardcoded Cookie Name in `c_ievent_srv.c`
**File:** `c_ievent_srv.c:773`

The cookie extraction uses a hardcoded `"access_token="` string rather than using the same `COOKIE_NAME_AT` constant defined in `c_auth_bff.c:72`. If the cookie name is ever changed in one place but not the other, authentication breaks silently.

**Fix:** Share the constant (e.g., in a common header) or at minimum add a cross-reference comment.

#### M3. Queue Overflow Returns Error But Connection Not Dropped
**File:** `c_auth_bff.c:300-315, 915-920`

When the BFF queue is full (16 entries), `enqueue()` returns -1 and a 503 error is sent. However, the connection remains open. A slow-loris style attack could keep connections open, filling the queue and DoSing other clients. With `MAX_PENDING_QUEUE=16`, this is a small window.

**Fix:** Consider dropping the connection after a 503, or adding a per-connection rate limit.

#### M4. CORS Wildcard Fallback
**File:** `c_auth_bff.c:281-283`

If `allowed_origin` is not configured and the `Origin` header is missing, the code falls back to `"*"`. With `Access-Control-Allow-Credentials: true`, browsers will **reject** `Access-Control-Allow-Origin: *` (they're mutually exclusive per the CORS spec). This means the fallback doesn't actually work for credentialed requests, but it also means a misconfiguration silently fails rather than loudly.

**Fix:** Never fall back to `*` when credentials are enabled. If origin is unknown, return 403.

#### M5. Token Refresh Timer Uses `refresh_expires_in`, Not `expires_in`
**File:** `c_login.js:491-497`

The refresh timer is set to `refresh_expires_in - 10` seconds. This means the access token could expire **long before** the refresh is triggered (access tokens typically expire in 5 minutes, refresh tokens in 30 minutes). While expired access tokens will cause the WebSocket server to reject requests (which triggers a reconnect), proactively refreshing before the *access token* expires would provide a smoother UX.

**Fix:** Use `expires_in` (access token lifetime) as the refresh trigger, not `refresh_expires_in`.

### LOW

#### L1. No `offline_access` Scope in PKCE Flow
**File:** `c_login.js:309`

The scope is `openid profile email`. There's no `offline_access`, which means the refresh token will be a standard (session-bound) token. This is probably intentional for browser SPAs (offline tokens are dangerous in browsers), but it should be documented as a conscious decision.

#### L2. `client_secret` Support in Public Client BFF
**Files:** `c_auth_bff.c:122, 566-569`, `backend_config.js:69`

The BFF supports an optional `client_secret`, but `backend_config.js` configures `"public-client": true`. This is fine and flexible, but the BFF should validate at startup that if the client is configured as public, no `client_secret` is set (and vice versa), to catch configuration mismatches early.

#### L3. `c_yuneta_gui.js:221` Still Writes JWT Attribute
The line `gobj_write_attr(__yuno__.__remote_service__, "jwt", jwt)` is still called in `do_connect()`, but the `jwt` parameter is `null` (set at line 197). This dead code path should be cleaned up to avoid confusion.

---

## Security Summary

| Property | Status | Notes |
|----------|--------|-------|
| JWTs never visible to JS | **PASS** | httpOnly cookies, BFF response has no tokens |
| PKCE S256 challenge | **PASS** | RFC 7636 compliant |
| CSRF protection (state nonce) | **PASS** | Random state verified on callback |
| Cookie attributes (HttpOnly/Secure/SameSite) | **PASS** | All three set correctly |
| Server-side JWT validation (JWKS) | **PASS** | `c_authz.c` uses `jwt_checker_verify2()` |
| URL cleanup after redirect | **PASS** | `history.replaceState()` |
| Token refresh before expiry | **PARTIAL** | Uses refresh_token expiry, not access_token expiry |
| CSP enforcement | **FAIL** | BFF URLs missing from `connect-src` |
| OIDC nonce replay protection | **MISSING** | No `nonce` parameter |
| Connection lifecycle safety | **RISK** | Use-after-free if client disconnects during Keycloak call |

---

## Overall Assessment

The implementation is **well-architected** and achieves its primary security goal: raw JWTs are never exposed to JavaScript. The PKCE flow is correctly implemented on both frontend and backend, the BFF properly mediates all token operations, and the WebSocket cookie bridge elegantly reuses the httpOnly cookies for authenticated connections.

The most critical issue is **C1 (CSP missing BFF URLs)** which will actually prevent the flow from working in browsers that enforce CSP. The **H1 (use-after-free)** issue is the most dangerous from a stability perspective and should be addressed before production. The **M5 (refresh timer)** issue will cause unnecessary session disruptions in practice.

The background research agent has completed and confirms all the findings in my review above.

To summarize the most actionable items:

1. **Fix immediately (C1):** Add BFF URLs to CSP `connect-src` in `index.html` — the flow is likely broken in production without this
2. **Fix before production (H1):** Guard against use-after-free when browser disconnects during Keycloak exchange
3. **Fix for UX (M5):** Refresh timer should use `expires_in` (access token), not `refresh_expires_in` (refresh token)

All 5 security fixes have been committed and pushed. Here's a summary:

| # | Issue | File | Fix |
|---|-------|------|-----|
| 1 | **CORS origin reflection** | `c_auth_bff.c` | No longer reflects arbitrary `Origin` headers; omits CORS headers entirely when `allowed_origin` is unconfigured |
| 2 | **Cookie name substring match** | `c_ievent_srv.c` | Verifies `access_token=` is at string start or preceded by `; `/` `, preventing matches on e.g. `xaccess_token=` |
| 3 | **Error responses missing CORS** | `c_auth_bff.c` | `send_error_response()` now forwards CORS headers so browsers can read error JSON cross-origin |
| 4 | **Refresh timer wrong expiry** | `c_login.js` | Now refreshes based on `expires_in` (access token, ~5 min) instead of `refresh_expires_in` (~30 min), keeping the WebSocket JWT valid |
| 5 | **Unvalidated redirect_uri** | `c_auth_bff.c` | New `allowed_redirect_uri` attribute rejects callback requests with non-matching redirect URIs |

**Note:** Fix #5 adds a new config attribute `allowed_redirect_uri`. You'll need to set it in your gateway/yuno configuration (e.g. `"allowed_redirect_uri": "https://treedb.yunetas.com/"`) for it to take effect. If left empty, redirect_uri validation is skipped (backward compatible).


# how must be the fields of keycloak: Root URL,Home URL, Valid redirect URIs, Valid post logout # redirect URIs,Web origins in this case of gui_treedb?

Based on the PKCE flow we implemented, here's exactly what each field should contain.

In our `initiate_pkce_login()` the `redirect_uri` is always:
```javascript
window.location.origin + window.location.pathname
```

So the values depend on where the app is served from.

---

## Production (`treedb.yunetas.com`)

| Field | Value |
|-------|-------|
| **Root URL** | `https://treedb.yunetas.com` |
| **Home URL** | `https://treedb.yunetas.com` |
| **Valid redirect URIs** | `https://treedb.yunetas.com/*` |
| **Valid post logout redirect URIs** | `https://treedb.yunetas.com/*` |
| **Web origins** | `https://treedb.yunetas.com` |

---

## Development (`localhost`)

Add these **in addition** to the production ones (same client, multiple entries):

| Field | Value |
|-------|-------|
| **Valid redirect URIs** | `https://localhost/*` |
| **Valid post logout redirect URIs** | `https://localhost/*` |
| **Web origins** | `https://localhost` |

If you use Vite's dev server (`http://localhost:5173`), also add:
- Valid redirect URIs: `http://localhost:5173/*`
- Web origins: `http://localhost:5173`

---

## Why each field matters

**Root URL** — Keycloak uses this as the base when building relative redirect URIs. Set it to the production origin.

**Home URL** — Where Keycloak links back to the app from the admin console. Same as Root URL.

**Valid redirect URIs** — Keycloak checks that the `redirect_uri` in the PKCE authorization request exactly matches one of these patterns. Our code sends `https://treedb.yunetas.com/` (origin + pathname), so the wildcard `/*` covers any sub-path.

**Valid post logout redirect URIs** — Not used by our BFF logout flow (the BFF calls Keycloak's logout endpoint server-side with no `post_logout_redirect_uri`), but Keycloak requires at least one entry for the UI. Set it the same as redirect URIs.

**Web origins** — Controls `Access-Control-Allow-Origin` for Keycloak's token endpoint. With our BFF the token exchange is server-to-server (no CORS needed for that), but the authorization endpoint response and `.well-known` metadata calls still benefit from correct CORS. Set it to the app's exact origin **without** a trailing slash and **without** a wildcard — Keycloak Web Origins doesn't support wildcards.

---

## Required client switches (reminder)

| Setting | Value |
|---------|-------|
| **Standard flow** | ✅ Enabled |
| **Direct access grants** (ROPC) | ❌ Disabled |
| **Client authentication** | Off (public client) — unless you add a `client_secret` to C_AUTH_BFF |


Great questions — these are important Keycloak client settings that directly affect the security of your BFF flow. Let me explain each:

## Client Authentication (Confidential vs Public)

This is the toggle between **confidential** and **public** client types.

| | Public Client | Confidential Client |
|---|---|---|
| **client_secret** | None | Required (BFF sends it to Keycloak) |
| **Who can exchange codes** | Anyone with the code + PKCE verifier | Only someone who also knows the secret |
| **Your current setup** | `"public-client": true` in keycloak_configs | Would need `client_secret` attr in c_auth_bff |

**Recommendation for your BFF architecture: Confidential.**

Since your BFF (`c_auth_bff.c`) runs server-side, it *can* safely hold a secret. This adds a second layer — an attacker who somehow intercepts the authorization code still can't exchange it without the secret. Your code already supports it:

```c
// c_auth_bff.c:566-569 — already handles client_secret
if(!empty_string(client_secret)) {
    json_object_set_new(jn_data, "client_secret",
        json_string(client_secret));
}
```

You just need to:
1. In Keycloak admin: Client → Settings → Client Authentication = **ON**
2. Copy the secret from Client → Credentials tab
3. Set `client_secret` in your BFF gobj config
4. Change `"public-client": false` in `keycloak_configs`

## PKCE Method (S256 vs plain vs none)

PKCE (Proof Key for Code Exchange) prevents authorization code interception attacks. The flow:

1. Browser generates a random `code_verifier` (your `generate_code_verifier()` in c_login.js)
2. Browser computes `code_challenge = SHA256(code_verifier)` and sends it to Keycloak in the auth request
3. BFF sends the original `code_verifier` in the token exchange
4. Keycloak verifies `SHA256(code_verifier) == code_challenge`

| Keycloak Setting | Meaning |
|---|---|
| **S256** (recommended) | Requires SHA-256 challenge — secure |
| **plain** | Challenge = verifier in plain text — defeats the purpose |
| **(empty/none)** | PKCE is optional — clients may or may not send it |

**Recommendation: S256.**

Your c_login.js already does this correctly:

```js
// c_login.js — already uses S256
code_challenge = base64url(SHA256(code_verifier))
// sent as: code_challenge_method=S256
```

Setting the Keycloak field to **S256** means Keycloak will *reject* any authorization request that doesn't include a valid S256 challenge — closing the door on downgrade attacks.

## Require DPoP Bound Tokens

DPoP (Demonstrating Proof of Possession) binds tokens to a cryptographic key pair so stolen tokens can't be replayed from a different client.

The flow would be:
1. Client generates an ephemeral key pair
2. Every token request includes a DPoP proof (a signed JWT proving possession of the private key)
3. The resulting access_token is bound to that key — presenting it from another machine fails

**Recommendation: Don't enable it (yet).**

Reasons:
- Your BFF stores tokens as httpOnly cookies — they're already protected from JS theft
- DPoP requires every HTTP request (including WebSocket upgrades) to carry a DPoP proof header, which would need significant changes to `c_ievent_srv.c`, `c_websocket.c`, and the JS client
- It's designed for scenarios where tokens traverse untrusted intermediaries (mobile apps, public APIs) — your BFF-to-Keycloak channel is already server-to-server over TLS

## Summary for your setup

| Setting | Recommended Value | Why |
|---|---|---|
| Standard Flow | ON | Already set correctly |
| Direct Access Grants | OFF | Already set correctly — no password grants |
| **Client Authentication** | **ON (Confidential)** | BFF is server-side, can hold a secret safely |
| **PKCE Method** | **S256** | Already implemented in c_login.js, enforce it server-side |
| Require DPoP | OFF | Overkill for httpOnly cookie architecture |

The biggest win here is switching to **confidential client + S256 enforced** — it gives you defense-in-depth (PKCE *and* client secret) with zero code changes since `c_auth_bff.c` already supports `client_secret`.
