(yuno-auth_bff)=
# `auth_bff`

OAuth 2 / OpenID Connect **Backend-For-Frontend**. It mediates between browser
SPAs and an identity provider (Keycloak, Google, …), running the
authorization-code flow with PKCE so the SPA never handles raw tokens: tokens
are kept server-side and the browser only gets an httpOnly cookie
(SEC-04/06/07/09 compliant).

## Architecture

```
C_AUTH_BFF_YUNO (default service)
    __bff_side__ : gate of per-connection C_AUTH_BFF instances
```

The yuno (`C_AUTH_BFF_YUNO`) hosts a `__bff_side__` gate; each client
connection gets its own `C_AUTH_BFF` gobj that drives the OIDC round-trip with
the IdP. The protocol/flow details live in the **Auth, permissions, TLS**
chapter.

## Commands

| Command | Description |
|---------|-------------|
| `view-bff-status` | Snapshot of every `C_AUTH_BFF` instance: queues, pending tasks, active IdP round-trip |
| `help` | Command help |

## Deep dive

See [Auth, permissions, TLS](../../../yunos/c/yuno_agent/YUNO_AUTH.md) for the
full OIDC / `auth_bff` flow, `C_AUTHZ`, the cookie model and cert-sync.
