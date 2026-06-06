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

| # | Setup | Expectation |
|---|-------|-------------|
| 1 | RSA-key checker (RS256), verify an `HS256` token | rejected — `jwt_checker_error()` set |
| 2 | `setkey(HS256, RSA jwk)` | rejected — RSA key can't be an HMAC key |
| 3 | `setkey(RS256, RSA jwk)` | accepted — guard not over-rejecting a legit pairing |
| 4 | OCT-key checker, verify a genuinely HMAC-signed `HS256` token | accepted — HMAC path still works |

**Verify contract (footgun).** `jwt_checker_verify2()` *always* returns the
parsed claims (incref'd) — success/failure is signalled only by
`jwt_checker_error()`. Checking the return pointer alone would accept the
forgery. The test (and `c_authz`: `if(!jwt_checker->error) validated = TRUE;`)
key off the error flag.

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
