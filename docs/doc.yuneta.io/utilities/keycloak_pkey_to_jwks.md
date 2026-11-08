(util-keycloak_pkey_to_jwks)=
# `keycloak_pkey_to_jwks`

Convert an RSA **public** key (base64-encoded DER, the "Public key" that
Keycloak shows for a realm key) into ONE JWK, an entry of `C_AUTHZ`'s
[`jwks`](#authz-jwks). The issuer string becomes the key's `kid`: give the
issuer (`iss`) of the tokens, never `*`. The JWK is printed to stdout.
(Source dir: `utils/c/pkey_to_jwks`.)

## Usage

```bash
keycloak_pkey_to_jwks --iss <issuer> <base64-der-public-key>
```

| Option | Purpose |
|--------|---------|
| `--iss` | Issuer of the tokens (the realm's URL), used as the JWK `kid`. Required. |

Example:

```bash
keycloak_pkey_to_jwks --iss https://auth.example.com/realms/example MIIBIjANBgkq...
```

```json
{
    "kid": "https://auth.example.com/realms/example",
    "description": "",
    "use": "sig",
    "kty": "RSA",
    "alg": "RS256",
    "n": "<base64url modulus>",
    "e": "AQAB"
}
```

## See also

- [Auth, permissions, TLS](../../../yunos/c/yuno_agent/YUNO_AUTH.md) — how Yuneta
  uses JWKS for JWT verification.
- [`utils/c/pkey_to_jwks/README.md`](https://github.com/artgins/yunetas/blob/7.30.17/utils/c/pkey_to_jwks/README.md).
