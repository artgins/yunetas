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

## Philosophy of ytls
The **ytls** module is built with the core philosophy of Yuneta in mind:

- **Security as a Priority:** Ensuring that all data transmitted over the network is encrypted and protected from eavesdropping or tampering.
- **Backend Agnosticism:** Abstracting the TLS backend behind a dispatch table so the rest of the codebase never depends on a specific crypto library. Switching between OpenSSL and mbed-TLS requires only a Kconfig change and rebuild.
- **Minimalism & Efficiency:** Providing a streamlined, efficient implementation that integrates seamlessly with the Yuneta framework. Choose mbed-TLS for smaller binaries on embedded/edge deployments.
- **Reliability & Stability:** Both OpenSSL and mbed-TLS are well-tested, industry-standard cryptographic libraries.
- **Flexibility:** Allowing customization of TLS parameters, certificates, and cipher suites to meet diverse application needs.
- **Ease of Use:** Abstracting complex TLS operations while giving developers a simple and consistent API for secure communication.

By following these principles, **ytls** ensures that Yuneta-based applications maintain strong security without unnecessary complexity.
