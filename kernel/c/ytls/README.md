# ytls

TLS abstraction layer for Yuneta. Exposes a single `api_tls_t` dispatch table so the rest of the codebase stays backend-agnostic — the actual crypto can be provided by **OpenSSL** or **mbed-TLS**.

## Selecting a backend

The backend is chosen at compile time via `.config` (Kconfig):

```
CONFIG_HAVE_OPENSSL=y     # default
CONFIG_HAVE_MBEDTLS=y     # alternative (≈3× smaller static binaries)
```

`ytls.h` is the single source of truth for the backend names:

- `TLS_LIBRARY_NAME` — preferred backend (`"openssl"` when both are enabled).
- `TLS_LIBRARIES_NAME` — every backend compiled in, joined with `+` (e.g. `"openssl+mbedtls"`).

Use `TLS_LIBRARY_NAME` directly in C string literals instead of a hard-coded value:

```c
'crypto': { 'library': '" TLS_LIBRARY_NAME "', ... }
```

`root-linux`'s `yunetas_register_c_core()` publishes both names into gobj's global-variable pool via [`gobj_add_global_variable()`](../../docs/doc.yuneta.io/api/gobj/info.md#gobj_add_global_variable), so kw configs can substitute them at load time:

```json
"crypto": { "library": "(^^__tls_library__^^)" }
```

This is what keeps gobj-c free of any `CONFIG_HAVE_OPENSSL` / `CONFIG_HAVE_MBEDTLS` checks — TLS knowledge lives only in this module.

## Backend notes (mbed-TLS v4.0)

- `psa_crypto_init()` must be called before any crypto operation; it initialises the PSA RNG, so `mbedtls_ssl_conf_rng` is no longer needed.
- `mbedtls_pk_parse_keyfile(ctx, path, password)` — 3 arguments in v4.0 (`f_rng` / `p_rng` removed).
- Use `mbedtls_ssl_is_handshake_over(&ssl)` to check handshake state instead of inspecting the return value of `mbedtls_ssl_handshake`.
- `set_trace()` implementations must be **silent** — they must never emit `gobj_log_info` calls. Tests capture INFO-level logs and any extra message breaks the expected-results comparison.

## TLS client verification (verify-by-default)

Since 7.6.0, a TLS **client** that would run with no server-certificate
validation (`VERIFY_NONE` — no CA configured / effective authmode `NONE`) is
**refused at ctx/state build time** in both backends: `build_ssl_ctx()` /
`build_state()` return `NULL`, so `ytls_init()` fails and the connection is
refused (fail-soft — the yuno stays up). This closes the silent MITM hole where
an unverified client trusts whatever certificate the peer presents.

Config keys in the `crypto` block (read by `ytls_init`):

| key | effect |
|-----|--------|
| `ssl_trusted_certificate` | path to a CA bundle (PEM) used to verify the peer |
| `ssl_use_system_ca` | `true` → also trust the OS store (**OpenSSL only**; mbedTLS has no system store) |
| `ssl_verify_mode` | `"none"` / `"optional"` / `"required"` — overrides the computed default |
| `ssl_allow_insecure_client` | `true` → explicitly accept an unverified **client** (self-signed / PSK / IoT bring-up). Default `false` |

Notes:

- **Servers** with no CA legitimately accept anonymous clients and are
  unaffected by this policy.
- A client with a CA but `ssl_verify_mode="none"` is **also** refused (both
  backends — the mbedTLS gate keys off the effective authmode for parity). To
  run such a client you must set `ssl_allow_insecure_client=true`.
- An **opted-in insecure client** (`ssl_allow_insecure_client=true`) is never
  silent, in either backend: a warning (*"TLS client WITHOUT server-certificate
  validation (MITM surface)"*) is logged at build time, and the verify result is
  still computed and surfaced at handshake end (*"TLS peer certificate did NOT
  verify"*). OpenSSL records it natively even under `VERIFY_NONE`; the mbedTLS
  backend achieves the same by running the opted-in client under
  `VERIFY_OPTIONAL` (verify-and-tolerate — the accept decision is unchanged).
- The `C_AUTH_BFF` `crypto` and `c_authz` `kc_crypto` IdP-client attributes
  default to `{"ssl_use_system_ca": true, "ssl_verify_mode": "required"}` — a
  public IdP/Keycloak is verified against the system store out of the box; for a
  private/self-signed IdP CA, override with `ssl_trusted_certificate`.

## Password hashing — cross-backend compatibility

`hash_password()` in `c_authz.c` uses **PBKDF2-HMAC** (RFC 2898 / PKCS#5). Both backends implement the identical standard:

- OpenSSL → `PKCS5_PBKDF2_HMAC()`
- mbed-TLS → `mbedtls_pkcs5_pbkdf2_hmac_ext()`

**Given the same password, salt, iterations and digest, the two functions produce bit-for-bit identical output.** This means:

- User/password databases are fully portable between an OpenSSL build and a mbed-TLS build — no re-hashing is required when switching backends.
- A hash stored by one backend can be verified by the other, because the stored `salt`, `hashIterations` and `algorithm` fields carry all information needed to recompute the key.

The only non-deterministic part is salt generation (`gen_salt()`), which uses `RAND_bytes()` (OpenSSL) or `psa_generate_random()` (mbed-TLS). Both produce cryptographically strong random bytes; the algorithm result is independent of which RNG was used.

### Supported digests (PBKDF2)

Both backends support `sha1`, `sha256`, `sha384`, `sha512`. mbed-TLS also supports `sha3-256`, `sha3-384`, `sha3-512`. The default is `sha512` with 27500 iterations.

## License

Licensed under the [MIT License](http://www.opensource.org/licenses/mit-license). See `LICENSE.txt` for details.
