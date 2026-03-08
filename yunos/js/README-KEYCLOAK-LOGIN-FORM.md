## Keycloak Login Form vs Custom Login Form

### Why use Keycloak's built-in login form

**Security advantages:**
- Credentials never touch your application — they go directly to Keycloak. This eliminates an entire class of vulnerabilities (your app can't leak what it never sees).
- Keycloak handles brute-force protection, account lockout, CAPTCHA, and rate limiting out of the box.
- Security patches (CSRF, XSS, session fixation) are maintained by the Keycloak team and applied with a simple upgrade.
- The login flow follows the **OAuth 2.0 / OpenID Connect** redirect pattern, which is battle-tested and audited.

**Functionality for free:**
- MFA/2FA (TOTP, WebAuthn/FIDO2, SMS) — no custom code needed.
- Social login (Google, GitHub, SAML IdPs) — just configuration.
- Password policies, password reset, email verification, account linking.
- Session management, SSO across multiple applications, single logout.
- Consent screens, terms acceptance, required actions.

**Compliance:**
- Easier to pass security audits — auditors trust a well-known IdP handling credentials over custom code.
- Clear separation of concerns: your app does business logic, Keycloak does identity.

### Why you might want a custom login form

- **Full UI control** — Keycloak themes can be customized, but it's more work than owning the HTML/CSS directly.
- **Single-page experience** — no redirect to a different domain/page (though Keycloak supports embedded login via the Resource Owner Password Credentials grant, this is **deprecated in OAuth 2.1** and discouraged).
- **Simpler deployment** — no Keycloak server to operate.

### The key risk of a custom form

With a custom form, your application **receives the raw username and password**, then forwards them to Keycloak (or validates them itself). This means:
1. Your app becomes a credential-handling surface — any vulnerability (logging, XSS, MITM on internal calls) can expose passwords.
2. You lose the redirect-based OAuth flow, making it harder to add MFA, social login, or SSO later.
3. You must implement CSRF protection, rate limiting, and brute-force detection yourself.

### Recommendation

**Use Keycloak's login form** (the standard redirect flow) unless you have a hard UX requirement that can't be solved with Keycloak theme customization. The security and maintenance benefits far outweigh the minor UX trade-off of a redirect.

If the redirect feels jarring, Keycloak supports **custom themes** (FreeMarker templates) that can match your application's look and feel exactly — you get full visual control while keeping the security model intact.


## Switching IdPs: Keycloak vs Ory vs Others

### If you use Keycloak's redirect-based login flow (OpenID Connect)

Switching to another IdP later is **straightforward** because your application only knows:
1. An authorization endpoint (redirect the user there)
2. A token endpoint (exchange the code for tokens)
3. A JWKS endpoint (validate JWT signatures)

These are all **standardized by OpenID Connect**. To swap IdPs, you change configuration (URLs, client ID/secret), not code.

### Ory as an alternative

Ory is a strong option. It comes in two flavors:

| | **Ory Kratos** (self-hosted) | **Ory Network** (cloud) |
|---|---|---|
| Identity management | Yes | Yes |
| Login/registration flows | Yes (API + browser) | Yes |
| MFA | TOTP, WebAuthn, Lookup Secrets | Same + SMS |
| Social login | Yes | Yes |
| OpenID Connect provider | Via **Ory Hydra** (separate component) | Built-in |
| License | Apache 2.0 | SaaS |

**Key difference from Keycloak:** Ory is modular — Kratos handles identity, Hydra handles OAuth2/OIDC, Oathkeeper handles API gateway/auth. Keycloak bundles everything in one server.

### Ory advantages over Keycloak

- **Headless/API-first** — Kratos exposes login/registration as JSON APIs, making custom UIs natural (no theme templating).
- **Cloud-native** — designed for containers, lightweight, no JVM.
- **Smaller footprint** — Keycloak needs a JVM + database; Kratos is a single Go binary + PostgreSQL.
- **Better developer experience** for custom login UIs — this is Ory's explicit design goal.

### Ory disadvantages vs Keycloak

- **More assembly required** — you need Kratos + Hydra + Oathkeeper for what Keycloak gives you in one package.
- **Smaller community** — less documentation, fewer Stack Overflow answers.
- **Admin UI** — Keycloak has a full admin console; Ory's self-hosted admin UI is more limited (Ory Network has a good dashboard).

### What this means for Yuneta

If your app talks **standard OIDC** (redirect flow, JWT validation), the IdP is a pluggable configuration choice:

```
App → redirect to IdP → user logs in → redirect back with code → exchange for JWT → validate JWT
```

This flow is identical whether the IdP is Keycloak, Ory Hydra, Auth0, Azure AD, or anything else OIDC-compliant.

### Recommendation

1. **Design your app against OIDC standards**, not Keycloak-specific APIs — this keeps IdP-switching trivial.
2. If you want **full UI control without theming hassles** → Ory Kratos + Hydra is a better fit.
3. If you want **everything in one box with minimal setup** → Keycloak is simpler to get running.
4. Either way, **don't build a custom login form that handles passwords directly** — that's the decision that locks you in and creates risk.

## Keycloak Login as a Popup

Yes, there are a few approaches:

### 1. Popup Window (most common)

Open Keycloak's authorization URL in a `window.open()` popup instead of a full redirect:

```javascript
const popup = window.open(
  keycloakAuthUrl,
  'keycloak-login',
  'width=500,height=600,menubar=no,toolbar=no'
);
```

After login, Keycloak redirects back to your `redirect_uri` — that page runs inside the popup, grabs the authorization code/token, sends it to the parent window via `postMessage`, and closes itself:

```javascript
// In the popup's redirect_uri page:
window.opener.postMessage({ code: authCode }, origin);
window.close();
```

**Pros:** Standard OIDC flow, no security compromise, works today.
**Cons:** Popup blockers can interfere; mobile experience is poor.

### 2. Iframe (not recommended)

Embedding Keycloak in an `<iframe>` is **blocked by default** — Keycloak sets `X-Frame-Options: SAMEORIGIN`. You can change this in Keycloak's config, but:

- It breaks clickjacking protection (security risk).
- Browsers increasingly block third-party cookies in iframes, which breaks session management.
- **Don't do this.**

### 3. Modal with Custom UI + Ory-style API (different IdP)

If you really want a modal/overlay login without leaving the page, that's exactly what **Ory Kratos** is designed for — it exposes login as a JSON API, so you build the form yourself inside a modal and POST credentials directly to Kratos's API.

This is **not possible with standard Keycloak** without using the deprecated Resource Owner Password Credentials (ROPC) grant.

### Recommendation

**Popup window** is the practical choice with Keycloak — it keeps the full OIDC security model intact while avoiding a full-page redirect. Just handle popup blockers gracefully (fall back to redirect if the popup is blocked).

If the popup UX isn't acceptable and you want a true in-page modal login, that's a point in favor of **Ory Kratos** over Keycloak.


# Changing to a own login form

A simple login form is cleaner UX. The browser's password manager handles credentials anyway.

The tradeoff is that with a direct login form, the app sends credentials to Keycloak's Resource Owner Password Grant (direct access grant), which means:

    The app handles the raw password (briefly, to exchange it for tokens)
    It must be enabled in the Keycloak client settings ("Direct Access Grants Enabled")
    No MFA/social login support (those require the browser redirect flow)

But if your setup doesn't need MFA or social login, a direct form is simpler and better UX.

Yes, it's still secure. The key security properties are preserved:

- **Tokens never touch JavaScript** — the BFF exchanges credentials server-side and stores tokens in `httpOnly; Secure; SameSite=Strict` cookies. JS can't read them.
- **Password only transits once** — browser sends it over HTTPS to the BFF, BFF forwards to Keycloak, done. It's not stored anywhere.
- **Browser password manager works** — which is actually a security win over the popup, since users are more likely to use strong unique passwords.
- **CORS + cookie domain** restrictions still apply.
- **Token refresh/logout** still go through the BFF with httpOnly cookies.

The only security tradeoff vs PKCE is that the app briefly handles the raw password (to forward it to the BFF). With PKCE, the app never saw the password at all — Keycloak's own form handled it. But since the BFF is your own server on the same infrastructure, the password is only exposed over your HTTPS connection, same as any traditional login form.

If you ever need MFA or social login in the future, you can re-enable the PKCE flow alongside the direct login — the BFF still has the `/auth/callback` endpoint for that.
