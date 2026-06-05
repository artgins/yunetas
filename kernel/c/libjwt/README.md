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

## Security review & upstream sync status

> **Heads-up on the version claim above:** the in-tree source was
> fingerprinted against upstream history and actually derives from
> **v3.2.1 + 2 commits** (the "1.16.0" line is stale), so it is on the
> 3.x branch, not 1.x.

A security review of this vendored copy against upstream was done on
**2026-06-05**.

- **Vendored base:** upstream commit `375e539`
  (`v3.2.1`+2, 2025-06-04 — "jwt: Use long long for json integers").
- **Analyzed up to:** upstream commit `602118d` (**v3.3.3**, 2026-05-04).
- **Gap:** 26 commits behind, including two security commits.

### Done (backported to this tree)

- **`49c730a` — GHSA-q843-6q5f-w55g, algorithm-confusion JWT forgery
  (CRITICAL).** An RSA/EC JWK was accepted as the HMAC verification key,
  so any token signed `HMAC("", header.payload)` verified against the
  public JWKS → forgery. This was **reachable** from `C_AUTHZ`'s verify
  loop (RS256 checker + an attacker HS256 token). Backported all four
  layers: `jwt_alg_required_kty()` (`jwt-private.h`), the `__setkey_check`
  kty check (`jwt-builder.c` + `jwt-checker.c` — the vendored copy split
  upstream's `jwt-common.c` into both), `__verify_config_post`
  (`jwt-verify.c`), and the `__check_hmac` backstop (`jwt.c`).
- **`cfd8902` (reachable subset):** JWK octet/RSA-PSS NULL+bounds guards
  (`openssl/jwk-parse.c`), `strncpy` in `jwt_copy_error` (`jwt-private.h`),
  and a `volatile` constant-time compare (`jwt-memory.c`).

### Still to do — see the repo [`TODO.md`](../../../TODO.md)

- **Port upstream's `jwt_security.c` test suite** (76 cases incl. the
  forgery PoC). This tree has **no jwt-level test harness**, so the
  forgery rejection is not covered by a deterministic test here — the
  backport is a faithful port of the official fix, build- and
  no-regression-verified (`c_auth_bff` RS256 flow) only.
- **Remaining `cfd8902` items (lower severity):** key-material scrub
  before free (`OPENSSL_cleanse`), `secure_getenv` for crypto-provider
  selection, builder/checker OOM-path JSON leak, missing-alg-header error
  message. N/A here: the JWKS-curl `Content-Length`/`atol` fixes
  (`jwks-curl.c` is disabled in CMakeLists; JWKS is loaded from a config
  attr, not libjwt's curl path).
- **Consider a periodic re-vendor from upstream** rather than carrying a
  growing backport list. Full analysis lives in the security-review
  workspace `UPSTREAM-DRIFT.md`.

When re-checking the gap: the upstream clone at
`/yuneta/development/projects/libjwt.original` is `git pull`ed on demand,
so re-derive `375e539..<new HEAD>` if it has moved.

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
