(ytls)=
# **TLS**

## **ytls.h**
The **ytls.h** header file defines the interface for the [TLS](https://en.wikipedia.org/wiki/Transport_Layer_Security) (Transport Layer Security) functionality in the Yuneta framework. It provides function declarations and structures for handling secure communication using TLS. Key features include:

- **Initialization & Cleanup:** Functions to initialize and clean up TLS resources.
- **TLS Context Management:** Creation and management of TLS contexts for secure communication.
- **Certificate Handling:** Functions for setting up and managing SSL certificates and private keys.
- **Secure Data Transmission:** Functions for sending and receiving encrypted data over TLS connections.
- **Error Handling & Debugging:** Utilities for logging and debugging TLS-related issues.

## **Architecture**

The ytls module uses a **backend-agnostic** design. The public API ([`ytls.h`](https://github.com/artgins/yunetas/blob/7.8.2/kernel/c/ytls/src/ytls.h) / [`ytls.c`](https://github.com/artgins/yunetas/blob/7.8.2/kernel/c/ytls/src/ytls.c)) exposes a single `api_tls_t` dispatch table, while the actual crypto is provided by two interchangeable backends configured via Kconfig (one or both can be enabled):

- **OpenSSL** (`CONFIG_HAVE_OPENSSL`) — default, full-featured TLS backend.
- **mbed-TLS** (`CONFIG_HAVE_MBEDTLS`) — lightweight alternative that produces ~3x smaller static binaries.

Both backends can be enabled simultaneously. When both are present, OpenSSL is preferred as the default.

[`ytls.h`](https://github.com/artgins/yunetas/blob/7.8.2/kernel/c/ytls/src/ytls.h) is the **single source of truth** for the backend names:

- `TLS_LIBRARY_NAME` — preferred backend (`"openssl"` when both are enabled).
- `TLS_LIBRARIES_NAME` — every backend compiled in, joined with `+` (e.g. `"openssl+mbedtls"`).

At runtime, two matching **yuno global variables** are available — `root-linux`'s [`yunetas_register_c_core()`](#yunetas_register_c_core) publishes them into gobj's global-variable pool via [`gobj_add_global_variable()`](../api/gobj/info.md#gobj_add_global_variable), so `gobj-c` itself stays free of any `CONFIG_HAVE_OPENSSL` / `CONFIG_HAVE_MBEDTLS` checks:

- `__tls_library__` — preferred backend, used for `(^^__tls_library__^^)` substitution in kw configs.
- `__tls_libraries__` — all enabled backends, useful for diagnostics and logging.

### Source files

| File | Purpose |
|------|---------|
| [`ytls.h`](https://github.com/artgins/yunetas/blob/7.8.2/kernel/c/ytls/src/ytls.h) / [`ytls.c`](https://github.com/artgins/yunetas/blob/7.8.2/kernel/c/ytls/src/ytls.c) | Public API and dispatch table |
| [`tls/openssl.c`](https://github.com/artgins/yunetas/blob/7.8.2/kernel/c/ytls/src/tls/openssl.c) / [`openssl.h`](https://github.com/artgins/yunetas/blob/7.8.2/kernel/c/ytls/src/tls/openssl.h) | OpenSSL backend implementation |
| [`tls/mbedtls.c`](https://github.com/artgins/yunetas/blob/7.8.2/kernel/c/ytls/src/tls/mbedtls.c) / [`mbedtls.h`](https://github.com/artgins/yunetas/blob/7.8.2/kernel/c/ytls/src/tls/mbedtls.h) | mbed-TLS backend implementation |

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

```{figure} ../_static/cert_hotswap.svg
:alt: The ytls handle is repointed from old_ctx to new_ctx and frees its own reference to old_ctx; live SSL sessions still hold their own refs, so old_ctx survives until the last in-flight session closes, while new sessions attach to new_ctx.
:width: 100%

The handle drops its ref on `old_ctx`; live sessions keep theirs, so the old
context outlives the swap.
```

The invariant is: **live sessions hold their own reference on the old
context**, so the swap only drops the handle's reference. This is the
easiest thing to break when touching the reload path.

- **OpenSSL backend.** `SSL_new(ctx)` bumps the `SSL_CTX` refcount,
  `SSL_free(ssl)` decrements it. [`ytls_reload_certificates()`](#ytls_reload_certificates) builds
  `new_ctx`, swaps `ytls->ctx = new_ctx` and calls `SSL_CTX_free(old_ctx)`
  to release the handle's ref. In-flight `SSL *` objects keep `old_ctx`
  alive until the last session closes.
- **mbed-TLS backend.** `ytls` maintains an explicit `mbedtls_state_t`
  bundle (`mbedtls_ssl_config` + `mbedtls_x509_crt` + `mbedtls_pk_context`)
  with a refcount. Each `hsskt` takes a ref on creation and releases it on
  [`ytls_free_secure_filter()`](#ytls_free_secure_filter). The swap drops the handle's ref; live
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

Each [`C_TCP_S`](#gclass-c-tcp-s) / [`C_UDP_S`](#gclass-c-udp-s) listener also exposes `reload-certs` and
`view-cert` directly, so a single listener can be targeted from
[`ycommand`](#util-ycommand) without touching the rest of the yuno.

## TLS security posture (hardening)

`ytls` is **secure-by-default**: a gate that sets no TLS knobs gets the hardened
behaviour, and every deliberate relaxation is an explicit, **logged** downgrade —
the yuneta *"no silent errors"* axiom applied to crypto. The knobs below live in
the gate's `crypto` config; the same key means the same thing on both backends.

### Protocol floor — `ssl_min_version`

Both backends floor at **TLS 1.2** when `ssl_min_version` is unset.

- **OpenSSL** accepts `SSLv3` / `TLS1.0` / `TLS1.1` / `TLS1.2` / `TLS1.3`. A floor
  **below TLS 1.2** is the IoT/legacy escape hatch: it must be paired with
  `ssl_ciphers "@SECLEVEL=0"` for OpenSSL to actually negotiate the old suite, and
  it logs a warning at context build (*"legacy floor below TLS1.2 / IoT-compat
  downgrade"*) so downgraded gates stay enumerable in the logs.
- **mbed-TLS** accepts only `TLS1.2` / `TLS1.3` — it can raise the floor, never
  lower it; legacy peers must use the OpenSSL backend.
- A peer offering a version below the floor is rejected, and the rejected
  handshake is logged **by default** (not only under `trace_tls`) in
  [`ytls_do_handshake()`](#ytls_do_handshake).

### Renegotiation — `ssl_disable_renegotiation`

Defaults to **`true`** (renegotiation disabled), closing the renegotiation-based
DoS/abuse surface; TLS 1.3 has no renegotiation at all. Set it to `false` only on
a gate that genuinely needs it — that re-enable is logged as an auditable
downgrade. OpenSSL backend only.

### Peer verification — `ssl_verify_mode`

When unset, the mode is **computed** from the gate's role and whether a CA is
configured:

| Situation | Computed default |
|---|---|
| **server**, no CA configured | `none` — preserves historical IoT / PSK / self-signed behaviour (anonymous clients accepted) |
| **client**, no CA configured | **refused** — see verify-by-default below |
| **server** with a CA | `optional` — request + verify the client cert if presented |
| **client** with a CA | `required` — validate the server cert + hostname |

`ssl_verify_mode` (`required` / `optional` / `none`) overrides the computed
default; `ssl_use_system_ca` adds the OS trust store; `ssl_verify_depth` bounds
the chain. A verify failure is **never silent**: under `optional` a non-verifying
peer is accepted but logged post-handshake (OpenSSL `SSL_get_verify_result()`,
mbed-TLS `mbedtls_ssl_get_verify_result()`); under `required` the verify callback
logs the chain error and aborts the handshake. The mbed-TLS verify *default* is
deliberately left IoT-tolerant (`optional`, surfaced) — raise it per gate with
`ssl_verify_mode=required`.

#### Verify-by-default for clients (since 7.6.0)

A TLS **client** with no way to validate the server certificate (effective
authmode `none` — no CA configured, or an explicit `ssl_verify_mode=none`) is a
standing MITM hole, so it is now **refused at ctx/state build time** in both
backends: `ytls_init()` fails and the connection is not made (fail-soft — the
yuno stays up). This is a **breaking change** for clients that previously ran
unverified. To keep running such a client (self-signed / PSK / IoT bring-up) set
`ssl_allow_insecure_client: true` in its `crypto` block to accept the MITM risk
explicitly; the choice is then logged, never silent (the mbed-TLS backend runs
the opted-in client under `optional` so the tolerated verify failure is still
surfaced — OpenSSL parity). **Servers** with no CA legitimately accept anonymous
clients and are unaffected.

### Knob summary

| Knob | Default | Effect |
|---|---|---|
| `ssl_min_version` | TLS 1.2 floor | minimum negotiated protocol version |
| `ssl_disable_renegotiation` | `true` | OpenSSL renegotiation off |
| `ssl_verify_mode` | computed (table above) | `required` / `optional` / `none` |
| `ssl_trusted_certificate` | unset | path to a CA bundle (PEM) used to verify the peer |
| `ssl_use_system_ca` | `false` | also trust the OS CA store (OpenSSL only) |
| `ssl_verify_depth` | `2` | max certificate chain depth (leaf → intermediate → root) |
| `ssl_allow_insecure_client` | `false` | `true` → run an unverified **client** anyway (accept the MITM risk) |
| `ssl_ciphers` | backend default | cipher list (`@SECLEVEL=0` to reach legacy suites) |

Regression coverage:
[`test_tls_floor_openssl.c`](https://github.com/artgins/yunetas/blob/7.8.2/tests/c/ytls/test_tls_floor_openssl.c)
asserts that an explicit sub-TLS1.2 floor is logged and that a real TLS1.0
ClientHello is rejected by the default floor;
[`test_tls_verify_openssl.c`](https://github.com/artgins/yunetas/blob/7.8.2/tests/c/ytls/test_tls_verify_openssl.c)
drives a real client/server handshake and asserts that a trusted cert with a
matching host connects, while a hostname mismatch or an unknown CA is rejected.

### Deployment — gate profiles & rollout

Secure-by-default means **the upgrade itself breaks nothing**: modern peers keep
negotiating on the new TLS 1.2 floor, renegotiation-off is transparent, and peer
verification stays off on any gate until you give it a CA. Hardening a gate is
then just a few keys in its `crypto` config. Two profiles cover almost every
gate.

#### Profile A — high-security gate

For SPA / BFF front ends, host-to-host links and the control plane — gates that
talk to modern peers. Turn verification on; the floor, reneg-off and the
computed `required`/`optional` mode are already the defaults, so the CA is the
only thing you add.

A **client** that dials out (validates the server it connects to):

```json
"crypto": {
    "library": "openssl",
    "ssl_trusted_certificate": "/yuneta/store/.../trusted_ca.pem",
    "ssl_server_name": "api.example.com"
}
```

- `ssl_server_name` is **mandatory** here: without it the chain is verified but
  the hostname is not (the gate logs a warning). It doubles as the SNI sent in
  the ClientHello.
- For a public endpoint, drop `ssl_trusted_certificate` and set
  `"ssl_use_system_ca": true` to trust the OS store instead.

The bundled CLI tools (`ycommand`, `ystats`, `ybatch`, `ytests`, `ycli`,
`mqtt_tui`, `emu_device`) already pass `"ssl_use_system_ca": true` on their
outbound TLS, so a `wss://` / `https://` endpoint with a public CA (e.g. Let's
Encrypt) works out of the box — including `ycommand`'s OIDC `task-authenticate`
to the issuer. `ycommand` exposes `--ssl-use-system-ca` (default on),
`--ssl-trusted-certificate` (private CA) and `--ssl-allow-insecure-client`
(bypass) to override per call. Note that verification checks the **hostname**:
dial the name the server's certificate is issued for, not just any DNS alias
that resolves to the same host.

A **server** doing mutual-TLS (validates the client certificate):

```json
"crypto": {
    "library": "openssl",
    "ssl_certificate": "/path/server.pem",
    "ssl_certificate_key": "/path/server.key",
    "ssl_trusted_certificate": "/path/client_ca.pem",
    "ssl_verify_mode": "required"
}
```

A CA-configured server defaults to `optional` (request + verify the client cert
if presented, tolerate absent); set `required` to reject clients without a valid
certificate.

#### Profile B — IoT / legacy-compat gate

For old devices that cannot do TLS 1.2, or that use PSK / self-signed certs.
Relax **explicitly** — each relaxation is logged and stays greppable.

```json
"crypto": {
    "library": "openssl",
    "ssl_min_version": "TLS1.0",
    "ssl_ciphers": "HIGH:@SECLEVEL=0",
    "ssl_verify_mode": "none"
}
```

- Legacy floors are **OpenSSL only** — mbed-TLS cannot negotiate below TLS 1.2,
  so legacy gates must use the OpenSSL backend.
- `@SECLEVEL=0` is required for OpenSSL to actually offer the old cipher suites;
  `ssl_min_version` alone is not enough.
- Keep this profile on the narrowest possible set of gates; everything else
  stays on Profile A / the defaults.

#### Rollout procedure

1. **Upgrade.** Nothing breaks; the reduced-security gates simply start
   announcing themselves in the log.
2. **Enumerate from the logs.** Every gap is designed to be greppable:

   | Log message | Meaning | Action |
   |---|---|---|
   | `TLS client WITHOUT server-certificate validation` | a client is not validating the server it dials | add a CA → Profile A |
   | `TLS handshake rejected` | a peer was refused (often a legacy device below the floor) | if legitimate, move that gate to Profile B |
   | `legacy floor below TLS1.2` | a gate is on a relaxed floor | confirm it is an intended IoT gate |
   | `renegotiation explicitly enabled` | a gate re-enabled renegotiation | confirm it is needed |
   | `peer certificate did NOT verify` | an `optional` gate accepted an invalid cert | fix the chain, or raise to `required` |

3. **Harden the high-level gates** one at a time: apply Profile A, restart,
   confirm the warning is gone and traffic still flows.
4. **Pin the legacy gates** that showed `TLS handshake rejected` to Profile B.
5. **Validate in staging, then production.** Roll the config to a staging
   environment first and watch the same logs; only then promote to production.

Goal state: no `WITHOUT server-certificate validation` lines outside known IoT
gates, and every `legacy floor` / `renegotiation enabled` line traceable to a
deliberate Profile-B gate.

## Philosophy of ytls
The **ytls** module is built with the core philosophy of Yuneta in mind:

- **Security as a Priority:** Ensuring that all data transmitted over the network is encrypted and protected from eavesdropping or tampering.
- **Backend Agnosticism:** Abstracting the TLS backend behind a dispatch table so the rest of the codebase never depends on a specific crypto library. Switching between OpenSSL and mbed-TLS requires only a Kconfig change and rebuild.
- **Minimalism & Efficiency:** Providing a streamlined, efficient implementation that integrates seamlessly with the Yuneta framework. Choose mbed-TLS for smaller binaries on embedded/edge deployments.
- **Reliability & Stability:** Both OpenSSL and mbed-TLS are well-tested, industry-standard cryptographic libraries.
- **Flexibility:** Allowing customization of TLS parameters, certificates, and cipher suites to meet diverse application needs.
- **Ease of Use:** Abstracting complex TLS operations while giving developers a simple and consistent API for secure communication.

By following these principles, **ytls** ensures that Yuneta-based applications maintain strong security without unnecessary complexity.
