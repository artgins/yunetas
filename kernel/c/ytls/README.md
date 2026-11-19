# ytls

`ytls` is the TLS abstraction layer of Yuneta. It gives one `api_tls_t` dispatch table, so the rest of the code does not depend on a backend. OpenSSL or mbed-TLS supplies the crypto.

## Selecting a backend

You select the backend at compile time, in `.config` (Kconfig):

```
CONFIG_HAVE_OPENSSL=y     # default
CONFIG_HAVE_MBEDTLS=y     # alternative (≈3× smaller static binaries)
```

`ytls.h` is the single source of truth for the backend names:

- `TLS_LIBRARY_NAME`: the preferred backend (`"openssl"` when both are enabled).
- `TLS_LIBRARIES_NAME`: every backend compiled in, joined with `+` (for example `"openssl+mbedtls"`).

In C string literals, use `TLS_LIBRARY_NAME` and not a hard-coded value:

```c
'crypto': { 'library': '" TLS_LIBRARY_NAME "', ... }
```

`yunetas_register_c_core()` of `root-linux` publishes both names into the global-variable pool of gobj, with [`gobj_add_global_variable()`](../../docs/doc.yuneta.io/api/gobj/info.md#gobj_add_global_variable). Thus a kw configuration can substitute them at load time:

```json
"crypto": { "library": "(^^__tls_library__^^)" }
```

As a result, gobj-c has no `CONFIG_HAVE_OPENSSL` / `CONFIG_HAVE_MBEDTLS` checks. Only this module knows about TLS.

## Backend notes (mbed-TLS v4.0)

- You must call `psa_crypto_init()` before any crypto operation. It initializes the PSA RNG, so `mbedtls_ssl_conf_rng` is no longer necessary.
- `mbedtls_pk_parse_keyfile(ctx, path, password)` has 3 arguments in v4.0 (`f_rng` / `p_rng` removed).
- To get the handshake state, use `mbedtls_ssl_is_handshake_over(&ssl)`. Do not read the return value of `mbedtls_ssl_handshake`.
- `set_trace()` implementations must be silent. They must never emit `gobj_log_info` calls. Tests capture INFO-level logs, and any extra message breaks the comparison with the expected results.

## TLS client verification (verify-by-default)

Since 7.6.0, both backends refuse a TLS client that has no validation of the
server certificate (`VERIFY_NONE`: no CA configured, or effective authmode
`NONE`). They refuse it when they build the ctx or the state:
`build_ssl_ctx()` / `build_state()` return `NULL`. Thus `ytls_init()` fails
and the connection is refused. This failure is soft: the yuno stays up. This
closes the silent MITM hole, where an unverified client trusts any
certificate that the peer presents.

Configuration keys in the `crypto` block (read by `ytls_init`):

| key | effect |
|-----|--------|
| `ssl_trusted_certificate` | path to a CA bundle (PEM) used to verify the peer |
| `ssl_use_system_ca` | `true` → also trust the OS system CA store. Both backends. A portable probe across distributions finds the bundle (Debian/Ubuntu, RHEL/Rocky/Alma/Fedora, SUSE, Alpine). Thus it works also in fully-static binaries that do not inherit the host OPENSSLDIR / `SSL_CERT_FILE`. In those binaries, `set_default_verify_paths()` of OpenSSL alone loads an empty store. mbedTLS has no system store of its own, so it parses the bundle that the probe found. |
| `ssl_verify_mode` | `"none"` / `"optional"` / `"required"`. It overrides the calculated default. A server also takes `"request"`: it asks for the certificate of the client and accepts it as it comes. It does not verify or report its chain. The owner then pins it by `spki_sha256` (7.30.10+) |
| `ssl_allow_insecure_client` | `true` → accept an unverified client on purpose (self-signed / PSK / IoT bring-up). Default `false` |

Notes:

- A server with no CA accepts anonymous clients. This policy does not
  apply to servers.
- A client with a CA but with `ssl_verify_mode="none"` is also refused, in both
  backends. For parity, the mbedTLS gate uses the effective authmode. To
  run such a client, you must set `ssl_allow_insecure_client=true`.
- An insecure client that you enable on purpose (`ssl_allow_insecure_client=true`)
  is never silent, in either backend. At build time, it logs a warning
  (*"TLS client WITHOUT server-certificate validation (MITM surface)"*). The
  verify result is still calculated, and at the end of the handshake the client
  reports it (*"TLS peer certificate did NOT verify"*). OpenSSL records it
  natively, also under `VERIFY_NONE`. The mbedTLS backend gets the same result
  because it runs this client under `VERIFY_OPTIONAL` (verify and tolerate).
  The decision to accept the peer does not change.
- These attributes have the default
  `{"ssl_use_system_ca": true, "ssl_verify_mode": "required"}`: `crypto` of
  `C_AUTH_BFF`, `kc_crypto` of `c_authz`, `crypto` of `C_PROT_HTTP_CL`, and
  `crypto` of `c_smtp_session` in emailsender. Thus, by default, they verify a
  public IdP/Keycloak, HTTPS API or SMTPS server against the system store. The
  probe finds the trust store also in fully-static builds. For a
  private/self-signed CA, override the default with `ssl_trusted_certificate`.

## Mutual TLS: who the peer is

A server with a CA (`ssl_trusted_certificate`) asks the client for a
certificate. Mode `"optional"` is the default with a CA. In that mode, a
client without a certificate still connects. A client with a certificate that
does not verify also connects, with a WARNING. In mode `"required"`, the
handshake refuses such a client.

A client presents its own certificate when it gets `ssl_certificate` and
`ssl_certificate_key`, in both backends. The OpenSSL client loads them since
`ytls_get_peer_cert_info()` exists. If the files cannot be loaded, the client
is refused (`ytls_init()` answers `NULL`). It is never a fatal exit.

A server is never fatal either (since 7.30.18). These failures make
`ytls_init()` answer `NULL` with one ERROR that gives the cause: a
certificate, a key or a trusted certificate that cannot be loaded, or an
unknown `library`. The owner of the server decides what happens. `C_TCP_S`
never listens without a context. By default (`exitOnError`), it exits the yuno
with code 0. With `exitOnError` 0, it tries again until the files load, and
the yuno continues. Up to 7.30.17, ytls itself exited the process with code 0.
`tests/c/ytls/test_server_init_not_fatal`.

After the handshake, `ytls_get_peer_cert_info(ytls, sskt)` gives the
certificate of the other end: `subject`, `issuer`, `cn`, `dns_names`,
`not_before`, `not_after`, `serial` and `verified`. Take a name from the
certificate only when `verified` is true. Under `"optional"`, an unverified
certificate is also there. Under `"none"`, nothing was verified. `C_TCP`
puts the result in its `EV_CONNECTED` as `peer_cert`. `archive_keeper` is the
consumer that binds an identity to it.

```json
{
    "ssl_certificate": "/yuneta/store/certs/backup.example.com.crt",
    "ssl_certificate_key": "/yuneta/store/certs/private/backup.example.com.key",
    "ssl_trusted_certificate": "/yuneta/store/certs/nodes_ca.pem",
    "ssl_verify_mode": "required"
}
```

## Password hashing: cross-backend compatibility

`hash_password()` in `c_authz.c` uses PBKDF2-HMAC (RFC 2898 / PKCS#5). Both backends implement the same standard:

- OpenSSL → `PKCS5_PBKDF2_HMAC()`
- mbed-TLS → `mbedtls_pkcs5_pbkdf2_hmac_ext()`

With the same password, salt, iterations and digest, the two functions produce output that is identical bit for bit. As a result:

- You can move user/password databases between an OpenSSL build and a mbed-TLS build. When you change the backend, you do not have to hash the passwords again.
- One backend can verify a hash that the other backend stored. The stored `salt`, `hashIterations` and `algorithm` fields carry all the information to calculate the key again.

Only the salt generation (`gen_salt()`) is not deterministic. It uses `RAND_bytes()` (OpenSSL) or `psa_generate_random()` (mbed-TLS). Both produce cryptographically strong random bytes. The result of the algorithm does not depend on the RNG.

### Supported digests (PBKDF2)

Both backends support `sha1`, `sha256`, `sha384`, `sha512`. mbed-TLS also supports `sha3-256`, `sha3-384`, `sha3-512`. The default is `sha512` with 27500 iterations.

## License

Licensed under the [MIT License](http://www.opensource.org/licenses/mit-license). See `LICENSE.txt` for details.
