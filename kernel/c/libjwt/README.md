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
**2026-06-05** and re-run against **v3.4.0** on **2026-06-15**.

- **Vendored base:** upstream commit `375e539`
  (`v3.2.1`+2, 2025-06-04 — "jwt: Use long long for json integers").
- **Analyzed up to:** upstream commit `6ea3b6b` (**v3.4.0**, 2026-06-15).
- **Gap:** v3.4.0 is a large feature release (full JWE, `crit` header, `jti`
  callbacks, PEM→JWK public API). Almost none of it touches Yuneta's compiled
  subset: JWE, `jwks-curl.c`, the `gnutls`/`json-c` backends are all excluded
  here. After filtering, only the **#281–#285 hardening batch** plus two
  conformance features (`crit`, `jti`) land on files we build; of those, two
  reachable hardenings were backported (below) and the rest are N/A.

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
- **`cfd8902` (low-severity remainder):** key-material scrub before free
  (portable `jwt_scrub_and_free` in `jwt-private.h` for the HMAC key in
  `jwks.c`; `OPENSSL_cleanse` for the PEM in `openssl/jwk-parse.c`),
  `secure_getenv` for crypto-provider selection (`jwt-crypto-ops.c`),
  builder/checker OOM-path JSON leak (`jwt-builder.c`/`jwt-checker.c`), and the
  missing-alg-header error message (`jwt-verify.c`).
- **Exact-alg pin (same-family downgrade), local addition in
  `__verify_config_post` (`jwt-verify.c`).** GHSA-q843 blocks *cross-family*
  confusion (HMAC vs RSA/EC) via the `kty` backstop, but that backstop is only
  family-granular (`jwt_alg_required_kty` maps every RS/PS alg to RSA), and on
  the common pinned path (`config->alg == config->key->alg`, both set) the
  alg-vs-alg chain never compares the token alg. So an **RS512 token verified
  against an RS256-pinned key** still slipped through. The added check requires
  `jwt->alg` to equal whichever alg is pinned (`config->alg` / `config->key->alg`).
  Purely additive — no legitimate token is newly rejected.
- **Forgery regression test:** [`tests/c/libjwt/test_jwt_alg_confusion.c`](../../../tests/c/libjwt/test_jwt_alg_confusion.c)
  pins the GHSA-q843 fix shut (RSA/EC/OKP confusion + `alg:none` + positive
  controls), wired into ctest. A focused equivalent of upstream's
  `jwt_security.c`, not a verbatim port. (Same-family downgrade — the exact-alg
  pin above — is not yet covered here; see the "Broaden the forgery test" TODO.)

#### v3.4.0 re-review (2026-06-15)

- **`18133e4` (L17) — reject duplicate JSON members on the token parse
  (`jwt-verify.c`).** RFC 8725 §2.4: the inbound header/payload is now parsed
  with `JSON_REJECT_DUPLICATES` (Jansson otherwise keeps the last occurrence),
  so a peer that selects a different occurrence of a duplicated claim/header
  cannot be made to disagree with us. Reachable on every `jwt_parse`.
- **`d180cc7` — strict base64url on decode (`jwt.c`, `jwt_base64uri_decode`).**
  The decoder accepted the standard-base64 `+`/`/` and silently *truncated* on
  an embedded `=`; both let a segment be re-spelled (or cut short) yet decode to
  the same/truncated bytes. Now rejects anything outside `[A-Za-z0-9_-]`.
  Reachable on every token segment and JWK member decode. Additive — the
  positive controls in `test_jwt_alg_confusion` still pass.

### N/A in this tree (v3.4.0 batch, after reachability analysis)

- **`5fada81` — mbedTLS RSA short-signature OOB heap read (the batch's only
  CVE-class bug).** *Not present here.* The vendored mbedTLS backend is a full
  **v4.0/PSA rewrite** that verifies via the length-aware high-level
  `mbedtls_pk_verify_ext(…, sig, sig_len)`; upstream's bug is in the low-level
  `mbedtls_rsa_*_verify` path this copy never calls. Immune by construction.
- **`2ef9500` (L4) mbedTLS ECDSA exact-size** — same reason (PK API is
  length-checked).
- **`60902e1` — jwks clone-OOM free** — already correct here (`jwks.c` frees the
  owned `item`, not the borrowed `jwk`).
- **`18133e4` L6 (aud-as-array) / L7 (jti-after-signature)** — `C_AUTHZ` enables
  only `JWT_CLAIM_ISS` (validates `iss` in C) and registers no `jti` callback.
- **`18133e4` L8/L9, all `jwe:` commits** — JWE is not compiled.
- **`927e8d4` length-math hardening** — the `kid` `strcpy`→`memcpy` is cosmetic
  (config-sourced JWKS); the `INT_MAX` encode guards are producer-side and
  `C_AUTHZ` only verifies.
- **`fe8840a` `crit` header (RFC 7515 §4.1.11)** — genuine conformance gap (the
  parser silently ignores `crit` instead of rejecting unknown critical params),
  but not exploitable: an attacker adding `crit` only triggers stricter
  rejection, and IdP access tokens don't carry it. Deferred (see TODO).

### Could we link upstream directly instead of vendoring? — No

v3.4.0's new public API (`jwks_create_fromkey`, PEM→JWK, JWE) does not close the
structural divergence: the `gbmem` allocator wiring (`jwt-memory.c`), the
`#include <gobj.h>` + `gobj_trace_json` coupling (`jwt-verify.c`), the mbedTLS
v4.0/PSA backend rewrite, the `jwt-common.c`→`jwt-builder.c`/`jwt-checker.c`
split, and the ArtGins-only exports (`jwt_checker_verify2` returning the payload
as `json_t *` — the basis of `verify_token()` — plus `jwk_process_one` /
`jwks_item_add` / `jwks_item_free2`, none public upstream). Re-vendor remains the
model.

### Worth contributing upstream

- **Exact-alg pin (same-family downgrade).** Upstream v3.4.0 *still* lacks it:
  `__verify_config_post` only checks config↔key consistency on the both-set path
  and relies on the family-granular `kty` backstop, so an RS512 token still
  verifies against an RS256-pinned key. Our local pin (above) fixes it.
- **mbedTLS v4.0/PSA backend** — forward-compatible and immune-by-construction to
  `5fada81`; offerable as portability (divergent design, not a clean patch).

### Still to do — see the repo [`TODO.md`](../../../TODO.md)

- **Broaden the forgery test** toward the rest of upstream's `jwt_security.c`
  (malformed JWKs, more `none`/`null` variants).
- **N/A here:** the JWKS-curl `Content-Length`/`atol` fixes (`jwks-curl.c` is
  disabled in CMakeLists; JWKS is loaded from a config attr, not libjwt's curl
  path). Re-classify if the curl backend is ever enabled.
- **Periodic re-vendor from upstream** rather than growing the backport list —
  procedure below. Full analysis lives in the security-review workspace
  `UPSTREAM-DRIFT.md`.

### Re-vendor procedure

The upstream clone lives at `/yuneta/development/projects/libjwt.original`
(OpenSSL+Jansson subset only — this tree does not compile gnutls/mbedtls or
`jwks-curl.c`). To re-check drift and pull fixes:

```bash
# 1. Refresh upstream and re-derive the gap from the recorded base.
cd /yuneta/development/projects/libjwt.original && git pull
git log --oneline 375e539..HEAD        # base is upstream 375e539 (v3.2.1+2)

# 2. For each new commit, classify: SEC / BUG / N-A (file not in our subset).
#    Map hunks carefully — the vendored copy SPLIT jwt-common.c into
#    jwt-builder.c + jwt-checker.c, so every jwt-common.c hunk lands in BOTH.

# 3. Backport SEC/reachable-BUG hunks by hand into kernel/c/libjwt/src/**,
#    matching local style (tabs; builder/checker use 4-space). Keep the
#    backend-agnostic headers free of an OpenSSL dependency.

# 4. Rebuild + run the regression test.
cd /yuneta/development/yunetas/kernel/c/libjwt/build && make install
cd /yuneta/development/yunetas/tests/c/libjwt/build && cmake .. && make \
    && ./test_jwt_alg_confusion

# 5. Update the recorded base commit + the lists above, and relink the JWT
#    consumers (auth_bff + the agent's OAuth2 path) per TODO.md.
```

When the backport list outgrows hand-porting, the cleaner move is a full
re-vendor: drop in upstream's OpenSSL+Jansson sources at a new tagged release,
re-apply the local split + the `// ArtGins` additions (e.g. `jwt_checker_verify2`,
`jwk_process_one`/`jwks_item_add` exports), and re-run the test.

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
