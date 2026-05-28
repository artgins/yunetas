(yuno-auth_bff)=
# `auth_bff`

OAuth 2 / OpenID Connect Backend-For-Frontend yuno — mediates between browser
SPAs and an identity provider (Keycloak, Google, …) implementing
authorization-code flow with PKCE. Tokens are stored in httpOnly cookies
(SEC-04/06/07/09 compliant).
