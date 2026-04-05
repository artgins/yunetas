# TLS API (ytls)

**TLS abstraction layer** for Yuneta. Exposes a single `api_tls_t`
dispatch table so the rest of the codebase stays backend-agnostic — the
actual crypto can be provided by **OpenSSL** or **mbed-TLS**.

## Selecting a backend

The backend is chosen at compile time via `.config` (Kconfig):

```
CONFIG_HAVE_OPENSSL=y     # default
CONFIG_HAVE_MBEDTLS=y     # alternative (≈3× smaller static binaries)
```

The macro `TLS_LIBRARY_NAME` (defined in `ytls.h`) expands to `"openssl"`
or `"mbedtls"`. **Always use this macro in configuration strings** instead
of a hard-coded literal:

```c
"'crypto': { 'library': '" TLS_LIBRARY_NAME "', ... }"
```

## Backend notes — mbed-TLS v4.0

- `psa_crypto_init()` must be called before any crypto operation; it
  initialises the PSA RNG, so `mbedtls_ssl_conf_rng` is no longer needed.
- `mbedtls_pk_parse_keyfile(ctx, path, password)` — 3 arguments in v4.0
  (`f_rng` / `p_rng` removed).
- Use `mbedtls_ssl_is_handshake_over(&ssl)` to check handshake state
  instead of inspecting the return value of `mbedtls_ssl_handshake`.
- `set_trace()` implementations must be **silent** — they must never
  emit `gobj_log_info` calls. Tests capture INFO-level logs and any
  extra message breaks the expected-results comparison.

## Password hashing — cross-backend compatibility

`hash_password()` in `c_authz.c` uses **PBKDF2-HMAC** (RFC 2898 /
PKCS#5). Both backends implement the identical standard:

- OpenSSL → `PKCS5_PBKDF2_HMAC()`
- mbed-TLS → `mbedtls_pkcs5_pbkdf2_hmac_ext()`

:::{important}
Given the same password, salt, iterations and digest, the two functions
produce **bit-for-bit identical output**. This means user/password
databases are fully portable between an OpenSSL build and a mbed-TLS
build — no re-hashing is required when switching backends. A hash
stored by one backend can be verified by the other, because the stored
`salt`, `hashIterations` and `algorithm` fields carry all the
information needed to recompute the key.
:::

The only non-deterministic part is salt generation (`gen_salt()`), which
uses `RAND_bytes()` (OpenSSL) or `psa_generate_random()` (mbed-TLS).
Both produce cryptographically strong random bytes; the algorithm
result is independent of which RNG was used.

### Supported digests (PBKDF2)

Both backends support `sha1`, `sha256`, `sha384`, `sha512`. mbed-TLS
additionally supports `sha3-256`, `sha3-384`, `sha3-512`. The default
is `sha512` with **27500 iterations**.

## Source code

- [`ytls.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/ytls/src/ytls.c) /
  [`ytls.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/ytls/src/ytls.h)
- [`openssl.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/ytls/src/tls/openssl.c) /
  [`openssl.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/ytls/src/tls/openssl.h)
- [`mbedtls.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/ytls/src/tls/mbedtls.c) /
  [`mbedtls.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/ytls/src/tls/mbedtls.h)

## Function reference

The individual function reference pages are listed in the left-hand
sidebar under **TLS API**.
