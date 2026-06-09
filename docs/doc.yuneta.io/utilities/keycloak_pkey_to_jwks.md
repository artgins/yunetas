(util-keycloak_pkey_to_jwks)=
# `keycloak_pkey_to_jwks`

Convert an RSA private key (base64-encoded DER) into a **JWKS** (JSON Web Key
Set) document suitable for OAuth 2 / JWT providers. The issuer string becomes the
key's `kid`. The JWKS is printed to stdout. (Source dir: `utils/c/pkey_to_jwks`.)

## Usage

```bash
keycloak_pkey_to_jwks --key <base64-der> --issuer <issuer> > jwks.json
```

| Option | Purpose |
|--------|---------|
| `--key` | RSA private key, base64-encoded DER |
| `--issuer` | Issuer string, used as the JWK `kid` |

## See also

- [Auth, permissions, TLS](../../../yunos/c/yuno_agent/YUNO_AUTH.md) — how Yuneta
  uses JWKS for JWT verification.
- [`utils/c/pkey_to_jwks/README.md`](https://github.com/artgins/yunetas/blob/7.5.7/utils/c/pkey_to_jwks/README.md).
