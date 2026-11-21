# libjwt — algorithm-confusion regression test

`test_jwt_alg_confusion` is the in-tree regression test for
GHSA-q843-6q5f-w55g, the JWT algorithm-confusion forgery: an RSA/EC public
key (published in a JWKS) accepted to verify an `HS*` token, whose HMAC the
buggy path then validates against the public-key bytes. This is an authentication
bypass. The fix (commits `30db0ab77` + `f85c26031`) binds the JWA algorithm to
the JWK's actual key type at three layers. This test pins it shut.

It mirrors the live reachable path in `kernel/c/root-linux/src/c_authz.c`:
`jwks_create` → `jwk_process_one` → `jwks_item_add` → `jwt_checker_setkey` →
`jwt_checker_verify2`.

## Cases

Asymmetric confusion is checked for all three key families (RSA, EC, OKP/EdDSA),
each with the same three sub-checks:

| Setup | Expectation |
|-------|-------------|
| asym-key checker (native alg), verify an `HS256` token | rejected, with `jwt_checker_error()` set |
| `setkey(HS256, asym jwk)` | rejected: an asymmetric key cannot be an HMAC key |
| `setkey(native alg, asym jwk)` | accepted: the guard does not over-reject a legit pairing |

Plus:

| Setup | Expectation |
|-------|-------------|
| real-key checker, verify an `alg:none` (unsigned) token | rejected: "Unsigned tokens are not allowed" (it was "Expected a signature, but JWT has none" up to 7.31.28) |
| OCT-key checker, verify a genuinely HMAC-signed `HS256` token | accepted: the HMAC path still works |
| (above) verified payload | carries the expected `sub` claim |

**Verify contract.** `jwt_checker_verify2()` fails closed (since 7.6.0): it
returns the claims only for a token that passed every check, and `NULL` on any
failure, with the cause in `jwt_checker_error_msg()`. Up to 7.5.x it returned
the claims of a failed token too, so a caller that looked at the pointer
accepted the forgery. `test_verify2_fail_closed` pins the new contract.

### Robustness cases (ported from upstream `tests/jwt_security.c`)

Beyond the forgery proper, the test sweeps malformed inputs to confirm the
parser degrades gracefully rather than crashing or silently accepting:

| Group | Cases | Expectation |
|-------|-------|-------------|
| Malformed JWKs | non-string `alg` (int/null/bool), missing `kty`/unknown `kty`, missing RSA `n`/`e`, missing EC `x`/`crv`, missing OKP `x`, non-string `n`/`e`, deeply-nested `n`, bad-base64 `oct` `k`, empty string | handled: rejected via `jwks_item_error()`, or (non-string `alg`) handled without the `alg_str` NULL-deref |
| Malformed tokens | `NULL`/empty, no/one/many dots, empty header, header-not-JSON, header missing `alg`, invalid `alg`, `alg` as integer, `alg:none` + bad-base64 payload | rejected by a real-key checker, no crash |
| `jwt_checker_*` NULL-safety | `verify`/`error`/`error_msg`/`setkey` on a NULL checker | no crash, reports failure |
| Key in error (upstream GHSA-4jp4, `cd9f46d`) | an RSA JWK with no `n` (failed to load), and an `oct` JWK with good `k` and `"alg": 256` (loaded, then flagged) | `jwt_checker_setkey()` and `jwt_builder_setkey()` refuse both with *"Key is in error and cannot be used"*. The same octets with a string `alg` still verify. Up to 7.31.3 the flagged key was accepted and verified tokens |
| `alg:none` with no key (upstream `c1ced0e`, v3.7.0) | `NONE_TOKEN` against a checker with no key, on `verify` and `verify2`, and against a keyed checker | refused with *"Unsigned tokens are not allowed"*. Up to 7.31.28 a checker with no key accepted it |
| Maximum token size (upstream `1760016`, v3.8.0) | 1 MiB + 1 byte, exactly 1 MiB, `setmaxsize` of 0, `INT_MAX`, 16 and 2 MiB, and of a NULL checker | over the limit: *"Token too large"* on `verify` and `verify2`. At the limit: the size check passes. A refused `setmaxsize` keeps the old limit |
| EC private key consistency (upstream `ccd7c6b`, v3.7.0) | a P-256 private JWK with its own `d`, then the same `(x, y)` with another key's `d`, `d = 1`, `d = 0` and `d = n` | the good key loads and a builder takes it. Each other key is in error (*"EC private key does not match its public key"*) and a builder refuses it. Skipped on a backend that loads no EC JWK (MbedTLS) |
| `jwks_*` NULL-safety | `item_get`/`item_free`/`item_free_all`/`error`/`error_any`/`error_msg`/`error_clear`/`free`/`item_count`/`find_bykid`/`item_add`/`item_free_bad` on a NULL set, `find_bykid` with a NULL kid, `item_add` of a NULL item | no crash. Getters answer NULL or 0, `error`/`error_any` answer 1, `item_add` fails |

The `jwks_*` keyring API is asserted NULL-safe too (`jwks_item_get`,
`jwks_item_free`, `jwks_item_free_all`, `jwks_error`, `jwks_error_any`,
`jwks_error_msg`, `jwks_error_clear`, `jwks_free` on a NULL set). Up to 7.25.4
the vendored copy lacked upstream's guards and `jwks_item_get(NULL, 0)`
crashed. The backport is recorded in `kernel/c/libjwt/README.md`. So are
`jwks_item_count`, `jwks_find_bykid` (also with a NULL kid), `jwks_item_add`
and `jwks_item_free_bad`, which upstream does not guard either: the first
backport said the whole keyring API was safe, and these four still
dereferenced the set.

## Fixtures

The JWKs and tokens in the `.c` are deterministically generated (an RSA-2048
public key as a JWK, a forged `HS256` token, and an octet key + a token correctly
HMAC-signed with it). `openssl ecparam -name prime256v1 -genkey` made the EC
P-256 private keys. The 1 MiB token is a static buffer, so the test allocates
nothing for it. The test is standalone (no gobj framework), so libjwt and
jansson both default to libc `malloc`, with no allocator-timing dependency.

## Run

```bash
cd tests/c/libjwt/build && cmake .. && make && ctest --output-on-failure
# or, from the top-level build:
ctest -R alg_confusion --output-on-failure --test-dir build
```
