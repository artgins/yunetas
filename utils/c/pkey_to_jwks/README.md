# pkey_to_jwks

Convert an RSA public key (base64-encoded DER, the "Public key" Keycloak shows for a realm key) to ONE JWK, an entry of `C_AUTHZ`'s `jwks`. The issuer string is used as the `kid`: give the issuer (`iss`) of the tokens; `C_AUTHZ` refuses a `kid` of `*`.

## Usage

```bash
keycloak_pkey_to_jwks --iss <issuer> <base64-der-public-key>
```

Output is the JWK (JSON) on stdout.
