# libjwt (vendored)

In-tree fork of [libjwt](https://github.com/benmcollins/libjwt)
adapted for Yuneta.

## What it does

JSON Web Token (JWT) creation, parsing, and signature verification.
Used by the auth path to validate access tokens issued by an upstream
IdP (Keycloak / Auth0 / Azure AD) on every incoming WebSocket
connection — see [`AUTH.md`](../../../yunos/c/yuno_agent/AUTH.md) §3.

Crypto backends:
- `src/openssl/` — OpenSSL implementations.
- `src/mbedtls/` — mbedTLS implementations.
- `src/gnutls/` — GnuTLS implementations (kept for portability).

Backend selection follows the same `ytls` runtime choice used
everywhere else in the kernel (see
[`kernel/c/ytls/README.md`](../ytls/README.md)).

## Why vendored

Upstream libjwt has been working well at version **1.16.0**. The
**2.x / 3.x branches introduced API changes incompatible with
Yuneta's existing usage**. Rather than chase upstream and patch
divergence, libjwt was forked into this directory and the required
modifications applied in-tree. New upstream features can be ported
on a per-commit basis when they're worth the integration cost.

## Public API

The shipped headers expose:

```c
#include <jwt.h>
```

Key entry points (declared in `src/jwt.h`):

| Function                  | Purpose                                                   |
|---------------------------|-----------------------------------------------------------|
| `jwt_new` / `jwt_free`    | Construct / destroy a JWT object                          |
| `jwt_add_grant*`          | Add a claim                                               |
| `jwt_set_alg`             | Choose the signing algorithm (HS256, RS256, ES256, …)     |
| `jwt_encode`              | Sign and serialise to compact form                        |
| `jwt_decode` / `jwt_parse`| Parse + verify signature                                  |
| `jwks_*`                  | Load/iterate JWKS (`jwks.c`, `jwks-curl.c`)               |

The runtime entry called from Yuneta's `C_AUTHZ` is `jwt_parse()` at
`src/jwt-verify.c:83`.

## Build

Built as part of the standard `yunetas build` flow. Produces
`outputs/lib/libjwt.a` linked statically into the kernel.

## License

LGPL-2.1+ from upstream libjwt, plus the Yuneta-specific
modifications. See the source headers for per-file attribution.
