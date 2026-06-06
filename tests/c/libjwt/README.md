# libjwt — algorithm-confusion regression test

`test_jwt_alg_confusion` is the in-tree regression test for
**GHSA-q843-6q5f-w55g**, the JWT algorithm-confusion forgery: an RSA/EC public
key (published in a JWKS) accepted to verify an `HS*` token, whose HMAC the
buggy path then validates against the public-key bytes — an authentication
bypass. The fix (commits `30db0ab77` + `f85c26031`) binds the JWA algorithm to
the JWK's actual key type at three layers; this test pins it shut.

It mirrors the live reachable path in `kernel/c/root-linux/src/c_authz.c`:
`jwks_create` → `jwk_process_one` → `jwks_item_add` → `jwt_checker_setkey` →
`jwt_checker_verify2`.

## Cases

Asymmetric confusion is checked for all three key families (RSA, EC, OKP/EdDSA),
each with the same three sub-checks:

| Setup | Expectation |
|-------|-------------|
| asym-key checker (native alg), verify an `HS256` token | rejected — `jwt_checker_error()` set |
| `setkey(HS256, asym jwk)` | rejected — an asymmetric key can't be an HMAC key |
| `setkey(native alg, asym jwk)` | accepted — guard not over-rejecting a legit pairing |

Plus:

| Setup | Expectation |
|-------|-------------|
| real-key checker, verify an `alg:none` (unsigned) token | rejected — "Expected a signature, but JWT has none" |
| OCT-key checker, verify a genuinely HMAC-signed `HS256` token | accepted — HMAC path still works |
| (above) verified payload | carries the expected `sub` claim |

**Verify contract (footgun).** `jwt_checker_verify2()` *always* returns the
parsed claims (incref'd) — success/failure is signalled only by
`jwt_checker_error()`. Checking the return pointer alone would accept the
forgery. The test (and `c_authz`: `if(!jwt_checker->error) validated = TRUE;`)
key off the error flag.

### Robustness cases (ported from upstream `tests/jwt_security.c`)

Beyond the forgery proper, the test sweeps malformed inputs to confirm the
parser degrades gracefully rather than crashing or silently accepting:

| Group | Cases | Expectation |
|-------|-------|-------------|
| Malformed JWKs | non-string `alg` (int/null/bool); missing `kty`/unknown `kty`; missing RSA `n`/`e`; missing EC `x`/`crv`; missing OKP `x`; non-string `n`/`e`; deeply-nested `n`; bad-base64 `oct` `k`; empty string | handled — rejected via `jwks_item_error()`, or (non-string `alg`) handled without the `alg_str` NULL-deref |
| Malformed tokens | `NULL`/empty; no/one/many dots; empty header; header-not-JSON; header missing `alg`; invalid `alg`; `alg` as integer; `alg:none` + bad-base64 payload | rejected by a real-key checker, no crash |
| `jwt_checker_*` NULL-safety | `verify`/`error`/`error_msg`/`setkey` on a NULL checker | no crash, reports failure |

Scope note: upstream also hardens the `jwks_*` keyring API against `NULL`
(`jwks_item_get(NULL)` / `jwks_free(NULL)`); the vendored v3.2.1+2 copy has
**not** backported those guards (`jwks_item_get` derefs `jwk_set->head` —
`jwks.c:201`), so they are deliberately not asserted. Not reachable from
`c_authz` (the keyring is always valid there) — tracked as a drift item.

## Fixtures

The JWKs and tokens in the `.c` are deterministically generated (RSA-2048
public key as a JWK; a forged `HS256` token; an octet key + a token correctly
HMAC-signed with it). The test is standalone — no gobj framework — so libjwt and
jansson both default to libc `malloc`, with no allocator-timing dependency.

## Run

```bash
cd tests/c/libjwt/build && cmake .. && make && ctest --output-on-failure
# or, from the top-level build:
ctest -R alg_confusion --output-on-failure --test-dir build
```
