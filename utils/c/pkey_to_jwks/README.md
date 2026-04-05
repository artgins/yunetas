# pkey_to_jwks

Convert an RSA private key (base64-encoded DER) to a **JWKS** (JSON Web Key Set) document suitable for OAuth2 / JWT providers. The issuer string is used as the `kid`.

## Usage

```bash
pkey_to_jwks --key <base64-der> --issuer <issuer>
```

Output is a JWKS JSON on stdout.
