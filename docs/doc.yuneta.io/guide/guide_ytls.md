(ytls)=
# **TLS**

## **ytls.h**
The **ytls.h** header file defines the interface for the TLS (Transport Layer Security) functionality in the Yuneta framework. It provides function declarations and structures for handling secure communication using TLS. Key features include:

- **Initialization & Cleanup:** Functions to initialize and clean up TLS resources.
- **TLS Context Management:** Creation and management of TLS contexts for secure communication.
- **Certificate Handling:** Functions for setting up and managing SSL certificates and private keys.
- **Secure Data Transmission:** Functions for sending and receiving encrypted data over TLS connections.
- **Error Handling & Debugging:** Utilities for logging and debugging TLS-related issues.

## **Architecture**

The ytls module uses a **backend-agnostic** design. The public API (`ytls.h` / `ytls.c`) exposes a single `api_tls_t` dispatch table, while the actual crypto is provided by two interchangeable backends configured via Kconfig (one or both can be enabled):

- **OpenSSL** (`CONFIG_HAVE_OPENSSL`) — default, full-featured TLS backend.
- **mbed-TLS** (`CONFIG_HAVE_MBEDTLS`) — lightweight alternative that produces ~3x smaller static binaries.

Both backends can be enabled simultaneously. When both are present, OpenSSL is preferred as the default.

`ytls.h` is the **single source of truth** for the backend names:

- `TLS_LIBRARY_NAME` — preferred backend (`"openssl"` when both are enabled).
- `TLS_LIBRARIES_NAME` — every backend compiled in, joined with `+` (e.g. `"openssl+mbedtls"`).

At runtime, two matching **yuno global variables** are available — `root-linux`'s `yunetas_register_c_core()` publishes them into gobj's global-variable pool via [`gobj_add_global_variable()`](../api/gobj/info.md#gobj_add_global_variable), so `gobj-c` itself stays free of any `CONFIG_HAVE_OPENSSL` / `CONFIG_HAVE_MBEDTLS` checks:

- `__tls_library__` — preferred backend, used for `(^^__tls_library__^^)` substitution in kw configs.
- `__tls_libraries__` — all enabled backends, useful for diagnostics and logging.

### Source files

| File | Purpose |
|------|---------|
| `ytls.h` / `ytls.c` | Public API and dispatch table |
| `tls/openssl.c` / `openssl.h` | OpenSSL backend implementation |
| `tls/mbedtls.c` / `mbedtls.h` | mbed-TLS backend implementation |

### Backend implementations

Both backends implement the same functionality:

- **TLS Context Setup:** Creating, configuring, and destroying TLS contexts.
- **Certificate Loading:** Loading certificates, verifying them, and handling private keys.
- **Connection Handling:** Establishing, reading, writing, and closing TLS-secured connections.
- **Error Reporting:** Logging and handling backend-specific errors.
- **Secure Communication:** Encrypting and decrypting data sent over a TLS connection.
- **Password Hashing:** Both backends use PBKDF2-HMAC (RFC 2898) and produce bit-for-bit identical output, so user/password databases are fully portable between backends.

This module ensures that Yuneta applications can securely transmit data over the network using industry-standard encryption protocols.

## Hot-reloading certificates

`ytls` can swap the certificate material of a running TLS listener **without
dropping the sessions that are already established**. This is how Yuneta
keeps MQTT brokers, intake gates and control-plane channels online while
Let's Encrypt renews a certificate in the background.

### API

[`ytls_reload_certificates()`](../api/ytls/ytls.md#ytls_reload_certificates)
is the only entry point. It rebuilds the per-backend context from the cert
paths already stored in the `ytls` handle, validates it, and atomically
swaps it in. If any step fails, the previous context is kept intact and the
caller sees a negative return — traffic is never interrupted by a bad
reload.

[`ytls_get_cert_info()`](../api/ytls/ytls.md#ytls_get_cert_info) returns
`subject`, `issuer`, `not_before`, `not_after`, `serial` and
`days_remaining` for the currently-active context, so operators can observe
the live cert (not just the file on disk).

### How live sessions survive the swap

The invariant is: **live sessions hold their own reference on the old
context**, so the swap only drops the handle's reference. This is the
easiest thing to break when touching the reload path.

- **OpenSSL backend.** `SSL_new(ctx)` bumps the `SSL_CTX` refcount,
  `SSL_free(ssl)` decrements it. `ytls_reload_certificates()` builds
  `new_ctx`, swaps `ytls->ctx = new_ctx` and calls `SSL_CTX_free(old_ctx)`
  to release the handle's ref. In-flight `SSL *` objects keep `old_ctx`
  alive until the last session closes.
- **mbed-TLS backend.** `ytls` maintains an explicit `mbedtls_state_t`
  bundle (`mbedtls_ssl_config` + `mbedtls_x509_crt` + `mbedtls_pk_context`)
  with a refcount. Each `hsskt` takes a ref on creation and releases it on
  `ytls_free_secure_filter()`. The swap drops the handle's ref; live
  sessions keep the old bundle alive on their own.

### Callers

Yuneta ships three layers of defence that all drive
`ytls_reload_certificates()` through the same path — see the
[TLS certificate management guide](guide_cert_management.md) for the full
picture:

1. **Layer 1 — certbot deploy hook.** Pushes the `reload-certs` command
   down the yuno tree the moment certbot succeeds.
2. **Layer 2 — `c_agent` auto-sync timer.** Re-reads the cert files every
   `cert_sync_interval_sec` seconds (default 15 min) and broadcasts
   `reload-certs` when `size+mtime` changes.
3. **Layer 3 — `c_yuno` expiry monitor.** Periodic `view-cert` walk with
   warning / critical thresholds (`cert_warn_days`, `cert_critical_days`);
   alerts only — never reloads.

Each `C_TCP_S` / `C_UDP_S` listener also exposes `reload-certs` and
`view-cert` directly, so a single listener can be targeted from
`ycommand` without touching the rest of the yuno.

## Philosophy of ytls
The **ytls** module is built with the core philosophy of Yuneta in mind:

- **Security as a Priority:** Ensuring that all data transmitted over the network is encrypted and protected from eavesdropping or tampering.
- **Backend Agnosticism:** Abstracting the TLS backend behind a dispatch table so the rest of the codebase never depends on a specific crypto library. Switching between OpenSSL and mbed-TLS requires only a Kconfig change and rebuild.
- **Minimalism & Efficiency:** Providing a streamlined, efficient implementation that integrates seamlessly with the Yuneta framework. Choose mbed-TLS for smaller binaries on embedded/edge deployments.
- **Reliability & Stability:** Both OpenSSL and mbed-TLS are well-tested, industry-standard cryptographic libraries.
- **Flexibility:** Allowing customization of TLS parameters, certificates, and cipher suites to meet diverse application needs.
- **Ease of Use:** Abstracting complex TLS operations while giving developers a simple and consistent API for secure communication.

By following these principles, **ytls** ensures that Yuneta-based applications maintain strong security without unnecessary complexity.
