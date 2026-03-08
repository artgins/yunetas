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
