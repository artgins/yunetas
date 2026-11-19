# pkey_to_jwks

`keycloak_pkey_to_jwks` converts an RSA public key to one JWK, an entry of `C_AUTHZ`'s `jwks`. The key is base64-encoded DER, the "Public key" that Keycloak shows for a realm key.

## Usage

```bash
keycloak_pkey_to_jwks --iss <issuer> <base64-der-public-key>
```

The tool writes the JWK (JSON) to stdout.

The tool uses the issuer string as the `kid`. Give the issuer (`iss`) of the tokens. `C_AUTHZ` refuses a `kid` of `*`.
