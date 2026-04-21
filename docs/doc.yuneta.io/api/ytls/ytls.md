# TLS API (ytls)

**TLS abstraction layer** for Yuneta. Exposes a single `api_tls_t`
dispatch table so the rest of the codebase stays backend-agnostic — the
actual crypto can be provided by **OpenSSL**, **mbed-TLS**, or **both**.

## Selecting backends

One or both backends can be enabled at compile time via `.config` (Kconfig):

```
CONFIG_HAVE_OPENSSL=y     # default, full-featured
CONFIG_HAVE_MBEDTLS=y     # lightweight (≈3× smaller static binaries)
```

When both are enabled, the preferred default is OpenSSL. Each connection
can select its backend at runtime via the `"library"` key in the crypto
JSON config (e.g. `"library": "mbedtls"` to override the default).

`ytls.h` is the **single source of truth** for the backend names:

- `TLS_LIBRARY_NAME` — preferred backend (`"openssl"` when both are enabled).
- `TLS_LIBRARIES_NAME` — every backend compiled in, joined with `+` (e.g. `"openssl+mbedtls"`).

Use `TLS_LIBRARY_NAME` directly in C string literals instead of a hard-coded value:

```c
"'crypto': { 'library': '" TLS_LIBRARY_NAME "', ... }"
```

`root-linux`'s `yunetas_register_c_core()` publishes both names into gobj's
global-variable pool via [`gobj_add_global_variable()`](../gobj/info.md#gobj_add_global_variable),
so kw configs can substitute them at load time:

```json
"crypto": { "library": "(^^__tls_library__^^)" }
```

This is what keeps `gobj-c` free of any `CONFIG_HAVE_OPENSSL` /
`CONFIG_HAVE_MBEDTLS` checks — TLS knowledge lives only in this module.

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

(ytls_cleanup)=
## `ytls_cleanup()`

The `ytls_cleanup()` function releases resources associated with the given TLS context.

```C
void ytls_cleanup(
    hytls ytls
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ytls` | `hytls` | The TLS context to be cleaned up. |

**Returns**

This function does not return a value.

**Notes**

Ensure that [`ytls_cleanup()`](<#ytls_cleanup>) is called to free resources allocated by [`ytls_init()`](<#ytls_init>) to prevent memory leaks.

---

(ytls_reload_certificates)=
## `ytls_reload_certificates()`

Hot-reloads the TLS certificate material of an existing context **without
disrupting live connections**. Builds a fresh backend state (OpenSSL
`SSL_CTX` or mbed-TLS `mbedtls_ssl_config` bundle) from the new
`jn_config`, validates it, and atomically swaps it into the ytls handle.

```C
int ytls_reload_certificates(
    hytls    ytls,
    json_t  *jn_config   // not owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ytls` | `hytls` | TLS context previously returned by [`ytls_init()`](<#ytls_init>). |
| `jn_config` | `json_t *` | New crypto config (same keys as [`ytls_init()`](<#ytls_init>): `ssl_certificate`, `ssl_certificate_key`, `ssl_trusted_certificate`, `ssl_ciphers`, `ssl_verify_depth`, `trace_tls`, `rx_buffer_size`). Not owned by the function. |

**Returns**

`0` on success; `-1` on failure (for example: a file is missing, the
cert and key do not match, or the cert/key is unparseable). On failure
the previous context is kept intact — the caller can continue serving
traffic with the old material and retry the reload later.

**Notes**

The old backend state is kept alive, via refcount, for every secure
filter ([`hsskt`](<#ytls_new_secure_filter>)) created before the reload.
New filters created after the reload use the fresh state. Concretely:

- **OpenSSL**: `SSL_new()` already increments the `SSL_CTX` refcount;
  swapping `ytls->ctx` and calling `SSL_CTX_free()` just drops the
  ytls handle's ref. The `SSL_CTX` lives until the last
  [`SSL_free`](https://www.openssl.org/docs/man3.0/man3/SSL_free.html)
  on a per-session `SSL` releases it.
- **mbed-TLS**: this module maintains an explicit refcount on the
  `mbedtls_ssl_config` + `mbedtls_x509_crt` + `mbedtls_pk_context`
  bundle. Each [`hsskt`](<#ytls_new_secure_filter>) takes a ref on
  creation and releases it on
  [`ytls_free_secure_filter()`](<#ytls_free_secure_filter>).

This is how Yuneta integrates Let's Encrypt renewals without dropping
active TLS connections. See the
[cert management guide](../../guide/guide_cert_management.md) for the
full operational flow.

---

(ytls_decrypt_data)=
## `ytls_decrypt_data()`

`ytls_decrypt_data()` decrypts encrypted data from a secure connection and delivers the clear data via the `on_clear_data_cb` callback.

```C
int ytls_decrypt_data(
    hytls       ytls,
    hsskt       sskt,
    gbuffer_t  *gbuf  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ytls` | `hytls` | TLS context handle. |
| `sskt` | `hsskt` | Secure socket handle. |
| `gbuf` | `gbuffer_t *` | Encrypted data buffer, owned by the function. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

The decrypted data is provided through the `on_clear_data_cb` callback.

---

(ytls_do_handshake)=
## `ytls_do_handshake()`

`ytls_do_handshake()` initiates the TLS handshake process for a secure connection, returning the handshake status.

```C
int ytls_do_handshake(
    hytls  ytls,
    hsskt  sskt
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ytls` | `hytls` | TLS context handle. |
| `sskt` | `hsskt` | Secure socket handle. |

**Returns**

Returns `1` if the handshake is complete, `0` if it is in progress, and `-1` if it fails.

**Notes**

The callback `on_handshake_done_cb` will be invoked once upon success or multiple times in case of failure.

---

(ytls_encrypt_data)=
## `ytls_encrypt_data()`

The `ytls_encrypt_data()` function encrypts the given clear data in `gbuf` and triggers the `on_encrypted_data_cb` callback with the encrypted result.

```C
int ytls_encrypt_data(
    hytls       ytls,
    hsskt       sskt,
    gbuffer_t  *gbuf  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ytls` | `hytls` | TLS context handle. |
| `sskt` | `hsskt` | Secure socket handle. |
| `gbuf` | `gbuffer_t *` | Buffer containing the clear data to be encrypted. Ownership is transferred. |

**Returns**

Returns 0 on success, or a negative value on failure.

**Notes**

The encrypted data is delivered asynchronously via the `on_encrypted_data_cb` callback.

---

(ytls_flush)=
## `ytls_flush()`

`ytls_flush()` flushes both clear and encrypted data in the secure connection.

```C
int ytls_flush(
    hytls  ytls,
    hsskt  sskt
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ytls` | `hytls` | TLS context handle. |
| `sskt` | `hsskt` | Secure socket handle. |

**Returns**

Returns `0` on success, or a negative value on failure.

**Notes**

This function ensures that any pending clear or encrypted data is processed and sent.

---

(ytls_free_secure_filter)=
## `ytls_free_secure_filter()`

The `ytls_free_secure_filter()` function releases the resources associated with a secure filter created by [`ytls_new_secure_filter()`](<#ytls_new_secure_filter>).

```C
void ytls_free_secure_filter(
    hytls  ytls,
    hsskt  sskt
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ytls` | `hytls` | The TLS context managing the secure filter. |
| `sskt` | `hsskt` | The secure socket filter to be freed. |

**Returns**

This function does not return a value.

**Notes**

Ensure that [`ytls_shutdown()`](<#ytls_shutdown>) is called before freeing the secure filter to properly close the connection.

---

(ytls_get_cert_info)=
## `ytls_get_cert_info()`

Introspects the server certificate currently loaded in the TLS context.
Used by the `view-cert` command on TLS listeners and by the yuno-level
certificate-expiry monitor.

```C
json_t *ytls_get_cert_info(
    hytls  ytls
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ytls` | `hytls` | TLS context handle. |

**Returns**

A new `json_t` object owned by the caller, or `NULL` if no server
certificate is loaded (client-side ytls, or a backend that cannot
introspect). The caller must `json_decref()` the returned object.

The returned object contains these keys (same shape across the
OpenSSL and mbed-TLS backends):

| Key | Type | Description |
|---|---|---|
| `subject` | `string` | X.509 subject DN in one-line form (e.g. `/CN=api.example.com`). |
| `issuer` | `string` | X.509 issuer DN in one-line form. Equal to `subject` for self-signed certs. |
| `not_before` | `integer` | Unix timestamp of the certificate's `notBefore`. |
| `not_after` | `integer` | Unix timestamp of the certificate's `notAfter`. Use `(not_after - time(NULL)) / 86400` to get `days_remaining`; a negative value means already expired. |
| `serial` | `string` | Serial number as uppercase hex (no colons). |

**Notes**

Loading an expired certificate does NOT fail — the loader intentionally
does not enforce validity at load time (the TLS handshake does). This
is what lets the expiry monitor report a negative `days_remaining` for
a cert that needs urgent replacement, instead of the yuno refusing to
start at all.

---

(ytls_get_last_error)=
## `ytls_get_last_error()`

Retrieves the last error message associated with the given secure socket `sskt` in the TLS context `ytls`.

```C
const char *ytls_get_last_error(
    hytls  ytls,
    hsskt  sskt
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ytls` | `hytls` | The TLS context from which to retrieve the last error. |
| `sskt` | `hsskt` | The secure socket whose last error message is to be retrieved. |

**Returns**

A pointer to a string containing the last error message, or `NULL` if no error has occurred.

**Notes**

The returned error message is managed internally and should not be freed by the caller.

---

(ytls_init)=
## `ytls_init()`

`ytls_init()` initializes a TLS context using the specified configuration and mode (server or client).

```C
hytls ytls_init(
    hgobj     gobj,
    json_t   *jn_config,  // not owned
    BOOL      server
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj context in which the TLS instance will operate. |
| `jn_config` | `json_t *` | A JSON object containing TLS configuration parameters. This object is not owned by the function. |
| `server` | `BOOL` | A boolean flag indicating whether the TLS context should be initialized in server mode (`TRUE`) or client mode (`FALSE`). |

**Returns**

Returns a handle to the newly created TLS context (`hytls`) on success, or `NULL` on failure.

**Notes**

The `jn_config` parameter should include necessary TLS settings such as certificates, ciphers, and buffer sizes. See the structure documentation for valid configuration fields.

---

(ytls_new_secure_filter)=
## `ytls_new_secure_filter()`

`ytls_new_secure_filter()` creates a new secure filter for handling encrypted communication using a TLS context.

```C
hsskt ytls_new_secure_filter(
    hytls ytls,
    int (*on_handshake_done_cb)(void *user_data, int error),
    int (*on_clear_data_cb)(
        void *user_data,
        gbuffer_t *gbuf  // must be decref
    ),
    int (*on_encrypted_data_cb)(
        void *user_data,
        gbuffer_t *gbuf  // must be decref
    ),
    void *user_data
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ytls` | `hytls` | The TLS context used to create the secure filter. |
| `on_handshake_done_cb` | `int (*)(void *user_data, int error)` | Callback function invoked when the handshake process completes. The `error` parameter indicates success (0) or failure (non-zero). |
| `on_clear_data_cb` | `int (*)(void *user_data, gbuffer_t *gbuf)` | Callback function invoked when decrypted data is available. The `gbuf` parameter must be decremented (`decref`) after use. |
| `on_encrypted_data_cb` | `int (*)(void *user_data, gbuffer_t *gbuf)` | Callback function invoked when encrypted data is available. The `gbuf` parameter must be decremented (`decref`) after use. |
| `user_data` | `void *` | User-defined data passed to the callback functions. |

**Returns**

Returns an `hsskt` handle representing the newly created secure filter, or `NULL` on failure.

**Notes**

The secure filter manages encrypted communication and invokes the provided callbacks for handshake completion, clear data reception, and encrypted data transmission.

The filter is returned "cold": no handshake is started automatically. Call [`ytls_do_handshake()`](<#ytls_do_handshake>) right after this function to kick off the TLS handshake. On the client side this emits the ClientHello via `on_encrypted_data_cb`; on the server side it is a no-op that returns `0` (WANT_READ) until the peer speaks first.

---

(ytls_set_trace)=
## `ytls_set_trace()`

Enables or disables tracing for the given secure socket `sskt` in the TLS context `ytls`.

```C
void ytls_set_trace(
    hytls  ytls,
    hsskt  sskt,
    BOOL   set
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ytls` | `hytls` | The TLS context. |
| `sskt` | `hsskt` | The secure socket for which tracing is to be set. |
| `set` | `BOOL` | If `TRUE`, enables tracing; if `FALSE`, disables tracing. |

**Returns**

This function does not return a value.

**Notes**

Tracing provides verbose output for debugging purposes. It should be used with caution in production environments.

---

(ytls_shutdown)=
## `ytls_shutdown()`

The `ytls_shutdown()` function terminates a secure connection associated with the given `hsskt` session within the specified `hytls` context.

```C
void ytls_shutdown(
    hytls ytls,
    hsskt sskt
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ytls` | `hytls` | The TLS context managing the secure connection. |
| `sskt` | `hsskt` | The secure socket session to be shut down. |

**Returns**

This function does not return a value.

**Notes**

After calling `ytls_shutdown()`, the secure session `sskt` should no longer be used.

---

(ytls_version)=
## `ytls_version()`

Retrieves the version string of the TLS implementation used by `ytls_version()`.

```C
const char *ytls_version(
    hytls ytls
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ytls` | `hytls` | Handle to the TLS context. |

**Returns**

A string representing the version of the TLS implementation.

**Notes**

The returned string is managed internally and should not be freed by the caller.

---

(openssl_api_tls)=
## `openssl_api_tls()`

Returns the OpenSSL TLS backend dispatch table.

```C
api_tls_t *openssl_api_tls(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Pointer to the `api_tls_t` structure with OpenSSL-specific implementations.

**Notes**

- Defined in `kernel/c/ytls/src/tls/openssl.h`.
- Only available when `CONFIG_HAVE_OPENSSL=y`.

---

(mbed_api_tls)=
## `mbed_api_tls()`

Returns the mbed-TLS backend dispatch table.

```C
api_tls_t *mbed_api_tls(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Pointer to the `api_tls_t` structure with mbed-TLS-specific implementations.

**Notes**

- Defined in `kernel/c/ytls/src/tls/mbedtls.h`.
- Only available when `CONFIG_HAVE_MBEDTLS=y`.

---
